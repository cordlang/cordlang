#include "adapters/outbound/backends/static_html/static_html.h"
#include "domain/interp.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

static void sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
  if (!sb->buf) sb_oom();
}

static void sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) sb_oom();
    sb->buf = nbuf;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

static void sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) sb_append(sb, "  ");
}

static void html_escape_append(StrBuf *sb, const char *s) {
  if (!s) return;
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '&': sb_append(sb, "&amp;"); break;
      case '<': sb_append(sb, "&lt;"); break;
      case '>': sb_append(sb, "&gt;"); break;
      case '"': sb_append(sb, "&quot;"); break;
      case '\'': sb_append(sb, "&#39;"); break;
      default: {
        char c[2] = {*p, 0};
        sb_append(sb, c);
        break;
      }
    }
  }
}

#define SH_MAX_BIND 96
static char *g_bind_names[SH_MAX_BIND];
static char *g_bind_vals[SH_MAX_BIND];
static int g_bind_count;
static unsigned g_spa_mask;

static void bind_reset(void) {
  for (int i = 0; i < g_bind_count; i++) {
    free(g_bind_names[i]);
    free(g_bind_vals[i]);
    g_bind_names[i] = NULL;
    g_bind_vals[i] = NULL;
  }
  g_bind_count = 0;
  g_spa_mask = 0;
}

static const char *bind_get(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < g_bind_count; i++) {
    if (g_bind_names[i] && strcmp(g_bind_names[i], name) == 0)
      return g_bind_vals[i];
  }
  return NULL;
}

static void bind_add(const char *name, const char *val) {
  if (!name || !*name || g_bind_count >= SH_MAX_BIND) return;
  if (bind_get(name)) return;
  g_bind_names[g_bind_count] = strdup(name);
  g_bind_vals[g_bind_count] = strdup(val ? val : "");
  g_bind_count++;
}

static void strip_quotes(char *s) {
  if (!s || !*s) return;
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    memmove(s, s + 1, n - 2);
    s[n - 2] = '\0';
  }
}

static void collect_binds(IrNode *node) {
  if (!node) return;
  if (node->kind == IR_STATE) {
    g_spa_mask |= 1u;
    if (node->name && strcmp(node->name, "__states__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *st = node->kids[i];
        if (st && st->kind == IR_STATE && st->name) {
          char *v = strdup(st->value ? st->value : "0");
          strip_quotes(v);
          bind_add(st->name, v);
          free(v);
        }
      }
    } else if (node->name) {
      char *v = strdup(node->value ? node->value : "0");
      strip_quotes(v);
      bind_add(node->name, v);
      free(v);
    }
  } else if (node->kind == IR_PROP) {
    if (node->name && strcmp(node->name, "__props__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *p = node->kids[i];
        if (p && p->kind == IR_PROP && p->name) {
          char *v = strdup(p->value ? p->value : "");
          strip_quotes(v);
          bind_add(p->name, v);
          free(v);
        }
      }
    } else if (node->name) {
      char *v = strdup(node->value ? node->value : "");
      strip_quotes(v);
      bind_add(node->name, v);
      free(v);
    }
  } else if (node->kind == IR_EVENT) {
    g_spa_mask |= 2u;
  } else if (node->kind == IR_ROUTE) {
    g_spa_mask |= 4u;
  }
  for (size_t i = 0; i < node->n_kids; i++) collect_binds(node->kids[i]);
}

unsigned static_html_scan_spa_hooks(IrProgram *ir) {
  if (!ir || !ir->root) return 0;
  bind_reset();
  collect_binds(ir->root);
  unsigned m = g_spa_mask;
  bind_reset();
  return m;
}

void static_html_warn_spa_hooks(IrProgram *ir, const char *backend_name) {
  unsigned m = static_html_scan_spa_hooks(ir);
  if (!m) return;
  fprintf(stderr, "note: backend '%s' emits a static HTML snapshot; ",
          backend_name ? backend_name : "static");
  int first = 1;
  if (m & 1u) {
    fputs("state", stderr);
    first = 0;
  }
  if (m & 2u) {
    if (!first) fputs("/", stderr);
    fputs("@click/events", stderr);
    first = 0;
  }
  if (m & 4u) {
    if (!first) fputs("/", stderr);
    fputs("route", stderr);
  }
  fputs(" are not interactive (see docs/EMAIL.md / docs/PDF.md)\n", stderr);
}

static int ir_attr_is_true(const char *v) {
  return v && strcmp(v, "true") == 0;
}

static int is_decl_only(const IrNode *n) {
  if (!n) return 1;
  switch (n->kind) {
    case IR_STATE:
    case IR_PROP:
    case IR_COMPUTED:
    case IR_EFFECT:
    case IR_FETCH:
    case IR_MODULE_USE:
    case IR_STORE:
    case IR_SNIPPET:
    case IR_RENDER:
    case IR_AWAIT:
    case IR_ROUTE:
    case IR_ATTR:
    case IR_EVENT:
      return 1;
    case IR_HOOK:
      return 1;
    default:
      return 0;
  }
}

static int tag_forbidden(const char *tag) {
  static const char *bad[] = {
      "script", "iframe", "object", "embed", "applet", "frame",
      "frameset", "meta", "base", "link", "style", "html", "head", "body",
      "template", NULL};
  if (!tag) return 1;
  for (int i = 0; bad[i]; i++) {
    if (strcasecmp(tag, bad[i]) == 0) return 1;
  }
  return 0;
}

static int tag_safe_name(const char *tag) {
  if (!tag || !*tag) return 0;
  if (!isalpha((unsigned char)tag[0])) return 0;
  for (const char *p = tag + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '-')) return 0;
  }
  return 1;
}

/* Map Cord tag → HTML. Layout tags become table wrappers. */
typedef enum { LAY_NONE, LAY_COL, LAY_ROW } LayKind;

static LayKind lay_kind(const char *tag) {
  if (!tag) return LAY_NONE;
  if (strcmp(tag, "col") == 0 || strcmp(tag, "stack") == 0 ||
      strcmp(tag, "page") == 0)
    return LAY_COL;
  if (strcmp(tag, "row") == 0) return LAY_ROW;
  return LAY_NONE;
}

static const char *html_tag_for(const char *tag) {
  if (!tag) return "div";
  if (lay_kind(tag) != LAY_NONE) return NULL; /* table layout */
  if (strcmp(tag, "btn") == 0) return "a";    /* email: link-like CTA */
  if (strcmp(tag, "card") == 0) return "table";
  if (strcmp(tag, "grid") == 0 || strcmp(tag, "group") == 0) return "table";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "img") == 0) return "img";
  if (strcmp(tag, "input") == 0) return "span"; /* static snapshot */
  if (strcmp(tag, "textarea") == 0) return "p";
  if (strcmp(tag, "select") == 0) return "span";
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) return "span";
  if (strcmp(tag, "icon") == 0) return "span";
  if (strcmp(tag, "nav") == 0) return "table";
  if (strcmp(tag, "header") == 0 || strcmp(tag, "footer") == 0 ||
      strcmp(tag, "main") == 0 || strcmp(tag, "section") == 0 ||
      strcmp(tag, "article") == 0 || strcmp(tag, "aside") == 0)
    return "table";
  if (strcmp(tag, "span") == 0) return "span";
  if (strcmp(tag, "h1") == 0) return "h1";
  if (strcmp(tag, "h2") == 0) return "h2";
  if (strcmp(tag, "h3") == 0) return "h3";
  if (strcmp(tag, "p") == 0) return "p";
  if (tag_forbidden(tag) || !tag_safe_name(tag)) return "div";
  return tag;
}

static int px_from_attr(const char *v, int fallback) {
  if (!v || !*v) return fallback;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (end == v) return fallback;
  if (n < 0) n = 0;
  if (n > 4096) n = 4096;
  return (int)n;
}

static void collect_inline_style(StrBuf *style, IrNode *node, LayKind lay) {
  if (lay == LAY_COL)
    sb_append(style, "width:100%;border-collapse:collapse;");
  else if (lay == LAY_ROW)
    sb_append(style, "width:100%;border-collapse:collapse;");

  int center = 0, bold = 0, muted = 0;
  const char *variant = NULL;
  const char *size = NULL;
  const char *bg = NULL;
  int gap = -1, pad = -1;

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (ir_attr_is_true(c->value)) {
      if (strcmp(c->name, "center") == 0) center = 1;
      else if (strcmp(c->name, "bold") == 0) bold = 1;
      else if (strcmp(c->name, "muted") == 0) muted = 1;
      else if (strcmp(c->name, "primary") == 0) variant = "primary";
      else if (strcmp(c->name, "outline") == 0) variant = "outline";
      else if (strcmp(c->name, "ghost") == 0) variant = "ghost";
      continue;
    }
    if (!c->value) continue;
    if (strcmp(c->name, "gap") == 0) gap = px_from_attr(c->value, 8);
    else if (strcmp(c->name, "p") == 0) pad = px_from_attr(c->value, 16);
    else if (strcmp(c->name, "variant") == 0) variant = c->value;
    else if (strcmp(c->name, "size") == 0) size = c->value;
    else if (strcmp(c->name, "bg") == 0) bg = c->value;
  }

  if (pad >= 0) sb_appendf(style, "padding:%dpx;", pad);
  if (gap >= 0 && lay != LAY_NONE) {
    /* gap applied on cells later via data — store as padding hint on table */
    (void)gap;
  }
  if (center) sb_append(style, "text-align:center;");
  if (bold) sb_append(style, "font-weight:700;");
  if (muted) sb_append(style, "color:#6b7280;");
  if (bg) {
    if (strcmp(bg, "white") == 0) sb_append(style, "background:#ffffff;");
    else if (strcmp(bg, "gray-50") == 0) sb_append(style, "background:#f9fafb;");
    else sb_appendf(style, "background:%s;", bg);
  }
  if (size) {
    if (strcmp(size, "xs") == 0) sb_append(style, "font-size:12px;");
    else if (strcmp(size, "sm") == 0) sb_append(style, "font-size:14px;");
    else if (strcmp(size, "lg") == 0) sb_append(style, "font-size:18px;");
    else if (strcmp(size, "xl") == 0) sb_append(style, "font-size:20px;");
    else if (strcmp(size, "2xl") == 0) sb_append(style, "font-size:24px;");
    else if (strcmp(size, "3xl") == 0) sb_append(style, "font-size:30px;");
    else if (strcmp(size, "4xl") == 0) sb_append(style, "font-size:36px;");
  }
  if (variant) {
    if (strcmp(variant, "primary") == 0)
      sb_append(style,
                "background:#2563eb;color:#ffffff;padding:10px 16px;"
                "text-decoration:none;border-radius:6px;display:inline-block;");
    else if (strcmp(variant, "outline") == 0)
      sb_append(style,
                "background:#ffffff;color:#111827;border:1px solid #d1d5db;"
                "padding:10px 16px;text-decoration:none;border-radius:6px;"
                "display:inline-block;");
    else if (strcmp(variant, "ghost") == 0)
      sb_append(style,
                "background:transparent;color:#374151;padding:10px 16px;"
                "text-decoration:none;display:inline-block;");
  }
  if (node->name && strcmp(node->name, "card") == 0) {
    sb_append(style,
              "width:100%;max-width:560px;border-collapse:collapse;"
              "background:#ffffff;border:1px solid #e5e7eb;border-radius:12px;");
  }
}

static int attr_gap(IrNode *node) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "gap") == 0)
      return px_from_attr(c->value, 8);
  }
  return 8;
}

static void emit_snapshot_text(StrBuf *sb, const char *expr_or_text) {
  if (!expr_or_text) return;
  /* Resolve #{name} against bind table; leave unknown as [name]. */
  const char *p = expr_or_text;
  while (*p) {
    if (p[0] == '#' && p[1] == '{') {
      const char *end = strchr(p + 2, '}');
      if (!end) {
        html_escape_append(sb, p);
        return;
      }
      char name[128];
      size_t nlen = (size_t)(end - (p + 2));
      if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
      memcpy(name, p + 2, nlen);
      name[nlen] = '\0';
      /* trim */
      char *s = name;
      while (*s == ' ') s++;
      char *e = s + strlen(s);
      while (e > s && e[-1] == ' ') *--e = '\0';
      const char *val = bind_get(s);
      if (val)
        html_escape_append(sb, val);
      else {
        sb_append(sb, "[");
        html_escape_append(sb, s);
        sb_append(sb, "]");
      }
      p = end + 1;
    } else if (p[0] == '\\' && p[1] == '#' && p[2] == '{') {
      sb_append(sb, "#{");
      p += 3;
    } else {
      char c[2] = {*p, 0};
      if (*p == '&' || *p == '<' || *p == '>' || *p == '"' || *p == '\'')
        html_escape_append(sb, c);
      else
        sb_append(sb, c);
      p++;
    }
  }
}

static void gen_node(StrBuf *sb, IrNode *node, int depth);
static void gen_children(StrBuf *sb, IrNode *node, int depth);

static void emit_html_attrs(StrBuf *sb, IrNode *node, const char *tag) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name || ir_attr_is_true(c->value))
      continue;
    if (strcmp(c->name, "gap") == 0 || strcmp(c->name, "p") == 0 ||
        strcmp(c->name, "px") == 0 || strcmp(c->name, "py") == 0 ||
        strcmp(c->name, "variant") == 0 || strcmp(c->name, "size") == 0 ||
        strcmp(c->name, "bg") == 0 || strcmp(c->name, "color") == 0 ||
        strcmp(c->name, "cols") == 0 || strcmp(c->name, "style") == 0 ||
        strcmp(c->name, "__file__") == 0)
      continue;
    const char *an = c->name;
    if (strcmp(an, "to") == 0) an = "href";
    if (strcmp(an, "src") == 0 || strcmp(an, "alt") == 0 ||
        strcmp(an, "href") == 0 || strcmp(an, "width") == 0 ||
        strcmp(an, "height") == 0 || strcmp(an, "title") == 0) {
      sb_appendf(sb, " %s=\"", an);
      html_escape_append(sb, c->value ? c->value : "");
      sb_append(sb, "\"");
    }
  }
  /* btn without href → # */
  if (tag && strcmp(tag, "btn") == 0) {
    int has_href = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c && c->kind == IR_ATTR && c->name &&
          (strcmp(c->name, "href") == 0 || strcmp(c->name, "to") == 0))
        has_href = 1;
    }
    if (!has_href) sb_append(sb, " href=\"#\"");
  }
  (void)tag;
}

static void gen_layout_table(StrBuf *sb, IrNode *node, int depth, LayKind lay) {
  int gap = attr_gap(node);
  StrBuf style;
  sb_init(&style);
  collect_inline_style(&style, node, lay);
  sb_indent(sb, depth);
  sb_append(sb, "<table role=\"presentation\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\"");
  if (style.len > 0) {
    sb_append(sb, " style=\"");
    sb_append(sb, style.buf);
    sb_append(sb, "\"");
  }
  sb_append(sb, ">\n");
  free(style.buf);

  if (lay == LAY_ROW) {
    sb_indent(sb, depth + 1);
    sb_append(sb, "<tr>\n");
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || is_decl_only(c)) continue;
      sb_indent(sb, depth + 2);
      sb_appendf(sb, "<td style=\"padding:%dpx;vertical-align:middle;\">\n",
                 gap / 2);
      gen_node(sb, c, depth + 3);
      sb_indent(sb, depth + 2);
      sb_append(sb, "</td>\n");
    }
    sb_indent(sb, depth + 1);
    sb_append(sb, "</tr>\n");
  } else {
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || is_decl_only(c)) continue;
      sb_indent(sb, depth + 1);
      sb_append(sb, "<tr>\n");
      sb_indent(sb, depth + 2);
      sb_appendf(sb, "<td style=\"padding:%dpx 0;\">\n", gap / 2);
      gen_node(sb, c, depth + 3);
      sb_indent(sb, depth + 2);
      sb_append(sb, "</td>\n");
      sb_indent(sb, depth + 1);
      sb_append(sb, "</tr>\n");
    }
  }
  sb_indent(sb, depth);
  sb_append(sb, "</table>\n");
}

static void gen_element(StrBuf *sb, IrNode *node, int depth) {
  const char *tag = node->name ? node->name : "div";
  LayKind lay = lay_kind(tag);
  if (lay != LAY_NONE) {
    gen_layout_table(sb, node, depth, lay);
    return;
  }
  /* card / section-like table wrappers: one cell column */
  if (tag && (strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
              strcmp(tag, "group") == 0 || strcmp(tag, "nav") == 0 ||
              strcmp(tag, "header") == 0 || strcmp(tag, "footer") == 0 ||
              strcmp(tag, "main") == 0 || strcmp(tag, "section") == 0 ||
              strcmp(tag, "article") == 0 || strcmp(tag, "aside") == 0)) {
    StrBuf style;
    sb_init(&style);
    collect_inline_style(&style, node, LAY_COL);
    sb_indent(sb, depth);
    sb_append(sb, "<table role=\"presentation\" cellpadding=\"0\" "
                  "cellspacing=\"0\" border=\"0\"");
    if (style.len > 0) {
      sb_append(sb, " style=\"");
      sb_append(sb, style.buf);
      sb_append(sb, "\"");
    }
    sb_append(sb, ">\n");
    free(style.buf);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || is_decl_only(c)) continue;
      sb_indent(sb, depth + 1);
      sb_append(sb, "<tr>\n");
      sb_indent(sb, depth + 2);
      sb_append(sb, "<td style=\"padding:8px 0;\">\n");
      gen_node(sb, c, depth + 3);
      sb_indent(sb, depth + 2);
      sb_append(sb, "</td>\n");
      sb_indent(sb, depth + 1);
      sb_append(sb, "</tr>\n");
    }
    sb_indent(sb, depth);
    sb_append(sb, "</table>\n");
    return;
  }
  if (strcmp(tag, "fragment") == 0) {
    gen_children(sb, node, depth);
    return;
  }

  const char *html_tag = html_tag_for(tag);
  if (!html_tag) {
    gen_children(sb, node, depth);
    return;
  }

  int self_closing = (strcmp(html_tag, "img") == 0);
  StrBuf style;
  sb_init(&style);
  collect_inline_style(&style, node, LAY_NONE);

  sb_indent(sb, depth);
  sb_appendf(sb, "<%s", html_tag);
  emit_html_attrs(sb, node, tag);
  if (style.len > 0) {
    sb_append(sb, " style=\"");
    sb_append(sb, style.buf);
    sb_append(sb, "\"");
  }
  free(style.buf);

  if (self_closing) {
    sb_append(sb, " />\n");
    return;
  }
  sb_append(sb, ">\n");
  gen_children(sb, node, depth + 1);
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html_tag);
}

static void gen_children(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || is_decl_only(c)) continue;
    if (c->kind == IR_IF) {
      sb_indent(sb, depth);
      sb_appendf(sb, "<!-- if %s (static: true branch) -->\n",
                 c->value ? c->value : "true");
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *kc = c->kids[j];
        if (kc && kc->kind == IR_TEXT && kc->value &&
            strcmp(kc->value, "__else__") == 0)
          break;
        gen_node(sb, kc, depth);
      }
      if (i + 1 < node->n_kids) {
        IrNode *next = node->kids[i + 1];
        if (next && next->kind == IR_TEXT && next->value &&
            strcmp(next->value, "__else__") == 0)
          i++;
      }
    } else if (c->kind == IR_FOR) {
      sb_indent(sb, depth);
      sb_appendf(sb, "<!-- for %s in %s (static: 1 sample) -->\n",
                 c->name ? c->name : "item", c->value ? c->value : "items");
      for (size_t j = 0; j < c->n_kids; j++) gen_node(sb, c->kids[j], depth);
    } else {
      gen_node(sb, c, depth);
    }
  }
}

static void gen_node(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (is_decl_only(node)) return;

  switch (node->kind) {
    case IR_PROJECT:
    case IR_COMPONENT:
    case IR_LAYOUT:
      gen_children(sb, node, depth);
      break;
    case IR_ELEMENT:
      gen_element(sb, node, depth);
      break;
    case IR_TEXT: {
      if (node->value && strcmp(node->value, "__else__") == 0) break;
      sb_indent(sb, depth);
      char *plain = interp_plain_text(node->value);
      emit_snapshot_text(sb, plain ? plain : "");
      free(plain);
      sb_append(sb, "\n");
      break;
    }
    case IR_INTERP:
      sb_indent(sb, depth);
      if (node->value && node->n_kids == 0) {
        if (node->value[0] == '#' || strchr(node->value, '{')) {
          emit_snapshot_text(sb, node->value);
        } else {
          const char *v = bind_get(node->value);
          if (v)
            html_escape_append(sb, v);
          else {
            sb_append(sb, "[");
            html_escape_append(sb, node->value);
            sb_append(sb, "]");
          }
        }
      } else {
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *c = node->kids[j];
          if (!c) continue;
          if (c->kind == IR_TEXT && c->value) {
            char *plain = interp_plain_text(c->value);
            emit_snapshot_text(sb, plain ? plain : "");
            free(plain);
          } else if (c->kind == IR_INTERP && c->value) {
            const char *v = bind_get(c->value);
            if (v)
              html_escape_append(sb, v);
            else {
              sb_append(sb, "[");
              html_escape_append(sb, c->value);
              sb_append(sb, "]");
            }
          }
        }
      }
      sb_append(sb, "\n");
      break;
    case IR_SLOT:
      sb_indent(sb, depth);
      sb_append(sb, "<!-- slot");
      if (node->name) sb_appendf(sb, " %s", node->name);
      sb_append(sb, " -->\n");
      gen_children(sb, node, depth);
      break;
    case IR_IF:
    case IR_FOR: {
      IrNode fake;
      memset(&fake, 0, sizeof(fake));
      IrNode *kids[1] = {node};
      fake.kids = kids;
      fake.n_kids = 1;
      gen_children(sb, &fake, depth);
      break;
    }
    default:
      gen_children(sb, node, depth);
      break;
  }
}

char *static_html_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root)
    return strdup("<!DOCTYPE html><html><body></body></html>\n");

  bind_reset();
  collect_binds(ir->root);

  StrBuf body;
  sb_init(&body);
  gen_node(&body, ir->root, 2);

  StrBuf doc;
  sb_init(&doc);
  sb_append(&doc,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\" />\n"
            "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\" />\n"
            "  <meta http-equiv=\"Content-Type\" content=\"text/html; "
            "charset=UTF-8\" />\n"
            "  <title>Cordlang Static</title>\n"
            "</head>\n"
            "<body style=\"margin:0;padding:0;background:#f3f4f6;"
            "font-family:Arial,Helvetica,sans-serif;color:#111827;\">\n"
            "  <table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
            "cellspacing=\"0\" border=\"0\" style=\"background:#f3f4f6;"
            "padding:24px 0;\">\n"
            "    <tr>\n"
            "      <td align=\"center\">\n"
            "        <table role=\"presentation\" width=\"600\" "
            "cellpadding=\"0\" cellspacing=\"0\" border=\"0\" "
            "style=\"width:100%;max-width:600px;background:#ffffff;"
            "border-collapse:collapse;\">\n"
            "          <tr>\n"
            "            <td style=\"padding:24px;\">\n");
  sb_append(&doc, body.buf ? body.buf : "");
  sb_append(&doc,
            "            </td>\n"
            "          </tr>\n"
            "        </table>\n"
            "      </td>\n"
            "    </tr>\n"
            "  </table>\n"
            "  <!-- Cordlang static HTML: no reactive JS -->\n"
            "</body>\n"
            "</html>\n");

  free(body.buf);
  bind_reset();
  return doc.buf;
}
