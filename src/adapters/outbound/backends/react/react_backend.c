#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/backends/theme_css.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

static void sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
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
    while (sb->len + (size_t)n + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
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

static int is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static int looks_like_number(const char *s) {
  if (!s || !*s) return 0;
  const char *p = s;
  if (*p == '-' || *p == '+') p++;
  int digits = 0;
  while (*p) {
    if (isdigit((unsigned char)*p)) digits++;
    else if (*p != '.') return 0;
    p++;
  }
  return digits > 0;
}

static int looks_like_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

/* product.price / user.name — not free text like "Search..." */
static int looks_like_js_expr(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) return 0;
  int after_dot = 0;
  int saw_dot = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '.') {
      if (after_dot) return 0; /* ".." */
      after_dot = 1;
      saw_dot = 1;
      continue;
    }
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
    after_dot = 0;
  }
  if (after_dot) return 0; /* trailing '.' */
  /* single ident without dots is also a js expr (count, label) */
  (void)saw_dot;
  return 1;
}

static int needs_js_arrow(const char *handler) {
  if (!handler || !*handler) return 0;
  /* call / expression → wrap in () => ... */
  if (strchr(handler, '(') || strchr(handler, '+') || strchr(handler, '-') ||
      strchr(handler, '*') || strchr(handler, '/') || strchr(handler, ' ') ||
      strchr(handler, '?') || strchr(handler, ':') || strchr(handler, '='))
    return 1;
  return 0;
}

/* ── style maps ─────────────────────────────────────────── */

typedef struct {
  const char *style_key;
  const char *tw_prefix;
  int is_value_appended;
} StyleMap;

static StyleMap style_mappings[] = {
  {"bg", "bg-", 1},
  {"text", "text-", 1},
  {"p", "p-", 1},
  {"px", "px-", 1},
  {"py", "py-", 1},
  {"m", "m-", 1},
  {"mx", "mx-", 1},
  {"my", "my-", 1},
  {"gap", "gap-", 1},
  {"rounded", "rounded-", 1},
  {"shadow", "shadow-", 1},
  {"size", "text-", 1},
  {"w", "w-", 1},
  {"h", "h-", 1},
  {"max-w", "max-w-", 1},
  {"z", "z-", 1},
  {"op", "opacity-", 1},
  {"leading", "leading-", 1},
  {"tracking", "tracking-", 1},
  {NULL, NULL, 0},
};

static void gen_style_classes(StrBuf *sb, Node *style_map) {
  for (size_t i = 0; i < style_map->children_len; i++) {
    Node *entry = style_map->children[i];
    if (entry->type != NODE_STYLE_ENTRY) continue;

    const char *key = entry->value ? entry->value : "";
    const char *val = entry->value2 ? entry->value2 : "";

    int mapped = 0;
    for (int s = 0; style_mappings[s].style_key; s++) {
      if (strcmp(key, style_mappings[s].style_key) == 0) {
        sb_append(sb, " ");
        sb_append(sb, style_mappings[s].tw_prefix);
        if (style_mappings[s].is_value_appended) sb_append(sb, val);
        mapped = 1;
        break;
      }
    }

    if (!mapped) {
      if (strcmp(key, "between") == 0) sb_append(sb, " justify-between");
      else if (strcmp(key, "center") == 0) sb_append(sb, " items-center justify-center");
      else if (strcmp(key, "around") == 0) sb_append(sb, " justify-around");
      else if (strcmp(key, "evenly") == 0) sb_append(sb, " justify-evenly");
      else if (strcmp(key, "sticky") == 0) sb_append(sb, " sticky top-0");
      else if (strcmp(key, "bold") == 0) sb_append(sb, " font-bold");
      else if (strcmp(key, "muted") == 0) sb_append(sb, " text-muted");
      else if (strcmp(key, "overflow") == 0) {
        sb_append(sb, " overflow-");
        sb_append(sb, val);
      }
    }
  }
}

static const char *tag_to_div_plus_class(const char *tag) {
  if (strcmp(tag, "col") == 0) return "flex flex-col";
  if (strcmp(tag, "row") == 0) return "flex flex-row";
  if (strcmp(tag, "stack") == 0) return "flex flex-col";
  if (strcmp(tag, "grid") == 0) return "grid";
  if (strcmp(tag, "page") == 0) return "min-h-screen";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

static const char *html_tag_for(const char *tag) {
  if (strcmp(tag, "col") == 0) return "div";
  if (strcmp(tag, "row") == 0) return "div";
  if (strcmp(tag, "stack") == 0) return "div";
  if (strcmp(tag, "page") == 0) return "div";
  if (strcmp(tag, "btn") == 0) return "button";
  if (strcmp(tag, "card") == 0) return "div";
  if (strcmp(tag, "grid") == 0) return "div";
  if (strcmp(tag, "group") == 0) return "div";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "link") == 0) return "a"; /* may become Link */
  if (strcmp(tag, "img") == 0) return "img";
  if (strcmp(tag, "input") == 0) return "input";
  if (strcmp(tag, "textarea") == 0) return "textarea";
  if (strcmp(tag, "select") == 0) return "select";
  if (strcmp(tag, "checkbox") == 0) return "input";
  if (strcmp(tag, "radio") == 0) return "input";
  if (strcmp(tag, "icon") == 0) return "span";
  if (strcmp(tag, "nav") == 0) return "nav";
  if (strcmp(tag, "header") == 0) return "header";
  if (strcmp(tag, "footer") == 0) return "footer";
  if (strcmp(tag, "main") == 0) return "main";
  if (strcmp(tag, "section") == 0) return "section";
  if (strcmp(tag, "article") == 0) return "article";
  if (strcmp(tag, "aside") == 0) return "aside";
  if (strcmp(tag, "span") == 0) return "span";
  if (strcmp(tag, "h1") == 0) return "h1";
  if (strcmp(tag, "h2") == 0) return "h2";
  if (strcmp(tag, "h3") == 0) return "h3";
  if (strcmp(tag, "p") == 0) return "p";
  return tag;
}

static int is_style_attr_name(const char *name) {
  return strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
         strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
         strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
         strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
         strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
         strcmp(name, "overflow") == 0 || strcmp(name, "fit") == 0 ||
         strcmp(name, "aspect") == 0 || strcmp(name, "lines") == 0 ||
         strcmp(name, "w") == 0 || strcmp(name, "h") == 0 ||
         strcmp(name, "mx") == 0 || strcmp(name, "my") == 0 ||
         strcmp(name, "px") == 0 || strcmp(name, "py") == 0 ||
         strcmp(name, "m") == 0 || strcmp(name, "op") == 0 ||
         strcmp(name, "z") == 0;
}

/* Generation context — tracks React hook imports needed */
typedef struct {
  int use_router;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;     /* useActionState */
  int use_lazy;       /* lazy */
  int use_suspense;   /* Suspense */
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;     /* createPortal */
  int use_error_boundary;
  int use_children;   /* children prop / slot */
  int in_component;
  int is_layout;      /* slot → Outlet vs children */
  /* D4: action pending for form aria-busy / btn disabled */
  char action_pending[64];
  char action_fn[64];     /* e.g. submitForm */
  char action_handler[64]; /* e.g. formAction */
  int in_form_with_action;
} GenCtx;

static void gen_node(StrBuf *sb, Node *node, int depth, GenCtx *ctx);
static void gen_children(StrBuf *sb, Node *node, int depth, GenCtx *ctx);
static void gen_interpolation(StrBuf *sb, Node *node, int depth);

static void collect_classes(char *classes, size_t classes_sz, Node *node,
                            const char *base_class) {
  classes[0] = '\0';
  if (base_class) strncat(classes, base_class, classes_sz - 1);

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_STYLE_MAP && child->value &&
        strcmp(child->value, "style") == 0) {
      StrBuf style_sb = {0};
      style_sb.buf = malloc(1024);
      style_sb.cap = 1024;
      style_sb.len = 0;
      style_sb.buf[0] = '\0';
      gen_style_classes(&style_sb, child);
      if (style_sb.len > 0) {
        size_t room = classes_sz - strlen(classes) - 1;
        strncat(classes, style_sb.buf, room);
      }
      free(style_sb.buf);
    }
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    char vbuf[96];
    if (child->type == NODE_BOOL_ATTR && child->value) {
      if (strcmp(child->value, "between") == 0)
        strncat(classes, " justify-between", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "center") == 0)
        strncat(classes, " items-center justify-center",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "around") == 0)
        strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "evenly") == 0)
        strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "bold") == 0)
        strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "muted") == 0)
        strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "sticky") == 0)
        strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "primary") == 0)
        strncat(classes, " btn-primary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "outline") == 0)
        strncat(classes, " btn-outline", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "ghost") == 0)
        strncat(classes, " btn-ghost", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "secondary") == 0)
        strncat(classes, " btn-secondary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "xs") == 0)
        strncat(classes, " text-xs", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "sm") == 0)
        strncat(classes, " text-sm", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "lg") == 0)
        strncat(classes, " text-lg", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "xl") == 0)
        strncat(classes, " text-xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "2xl") == 0)
        strncat(classes, " text-2xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "3xl") == 0)
        strncat(classes, " text-3xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "4xl") == 0)
        strncat(classes, " text-4xl", classes_sz - strlen(classes) - 1);
    }

    if (child->type == NODE_ATTR && child->value && child->value2) {
      if (strcmp(child->value, "variant") == 0) {
        snprintf(vbuf, sizeof(vbuf), " btn-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "size") == 0) {
        snprintf(vbuf, sizeof(vbuf), " text-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "color") == 0) {
        /* color=primary | color=$primary → CSS var from theme.css */
        if (theme_is_color_token(child->value2)) {
          char tok[64];
          if (theme_token_name(child->value2, tok, sizeof(tok))) {
            snprintf(vbuf, sizeof(vbuf),
                     " text-[var(--color-%s)]", tok);
            strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
          }
        } else {
          snprintf(vbuf, sizeof(vbuf), " text-%s", child->value2);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else if (strcmp(child->value, "gap") == 0) {
        snprintf(vbuf, sizeof(vbuf), " gap-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "cols") == 0) {
        snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "p") == 0) {
        snprintf(vbuf, sizeof(vbuf), " p-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "bg") == 0) {
        if (theme_is_color_token(child->value2)) {
          char tok[64];
          if (theme_token_name(child->value2, tok, sizeof(tok))) {
            snprintf(vbuf, sizeof(vbuf), " bg-[var(--color-%s)]", tok);
            strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
          }
        } else {
          snprintf(vbuf, sizeof(vbuf), " bg-%s", child->value2);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else if (strcmp(child->value, "shadow") == 0) {
        snprintf(vbuf, sizeof(vbuf), " shadow-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "rounded") == 0) {
        snprintf(vbuf, sizeof(vbuf), " rounded-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "max-w") == 0) {
        snprintf(vbuf, sizeof(vbuf), " max-w-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "w") == 0) {
        snprintf(vbuf, sizeof(vbuf), " w-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "h") == 0) {
        snprintf(vbuf, sizeof(vbuf), " h-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    }
  }
}

/* Emit a JS/JSX attribute value: string literal, number, bool, or expression */
static void emit_jsx_value(StrBuf *sb, const char *val, int force_expr) {
  if (!val) {
    sb_append(sb, "{undefined}");
    return;
  }
  if (looks_like_number(val) || looks_like_bool(val) || force_expr) {
    sb_appendf(sb, "{%s}", val);
    return;
  }
  /* Identifier / expression with dots → JS expression */
  if ((isalpha((unsigned char)val[0]) || val[0] == '_') &&
      strchr(val, ' ') == NULL && strchr(val, '"') == NULL) {
    /* Could be a string-like path starting with letter, or identifier.
       Paths usually start with /. Bare words that look like props use {}. */
    int has_dot = strchr(val, '.') != NULL;
    int all_ident = 1;
    for (const char *p = val; *p; p++) {
      if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')) {
        all_ident = 0;
        break;
      }
    }
    if (all_ident && (has_dot || islower((unsigned char)val[0]) || val[0] == '_')) {
      sb_appendf(sb, "{%s}", val);
      return;
    }
  }
  /* Default: string literal */
  sb_appendf(sb, "\"%s\"", val);
}

static void gen_element(StrBuf *sb, Node *node, int depth, GenCtx *ctx) {
  const char *tag = node->value ? node->value : "div";

  /* Custom component: PascalCase */
  if (is_pascal_case(tag)) {
    sb_indent(sb, depth);
    sb_appendf(sb, "<%s", tag);

    for (size_t i = 0; i < node->children_len; i++) {
      Node *child = node->children[i];
      if (child->type == NODE_ATTR && child->value) {
        if (is_style_attr_name(child->value)) continue;
        sb_appendf(sb, " %s=", child->value);
        emit_jsx_value(sb, child->value2, 0);
      } else if (child->type == NODE_BOOL_ATTR && child->value) {
        /* skip style bools on components? allow as boolean props */
        if (strcmp(child->value, "bold") == 0 || strcmp(child->value, "muted") == 0 ||
            strcmp(child->value, "center") == 0 || strcmp(child->value, "between") == 0 ||
            strcmp(child->value, "sticky") == 0 || strcmp(child->value, "primary") == 0 ||
            strcmp(child->value, "outline") == 0 || strcmp(child->value, "ghost") == 0)
          continue;
        sb_appendf(sb, " %s", child->value);
      } else if (child->type == NODE_EVENT && child->value && child->value2) {
        char react_event[64];
        react_event[0] = 'o';
        react_event[1] = 'n';
        react_event[2] = (char)toupper((unsigned char)child->value[0]);
        strncpy(react_event + 3, child->value + 1, sizeof(react_event) - 4);
        react_event[sizeof(react_event) - 1] = '\0';
        if (needs_js_arrow(child->value2))
          sb_appendf(sb, " %s={() => %s}", react_event, child->value2);
        else
          sb_appendf(sb, " %s={%s}", react_event, child->value2);
      }
    }

    /* children of custom component? */
    int has_kids = 0;
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_ELEMENT || c->type == NODE_TEXT || c->type == NODE_FOR ||
          c->type == NODE_IF || c->type == NODE_SLOT ||
          (c->type == NODE_ELEMENT) /* component */) {
        if (c->type == NODE_ATTR || c->type == NODE_EVENT || c->type == NODE_BOOL_ATTR ||
            c->type == NODE_STYLE_MAP)
          continue;
        if (c->type == NODE_ELEMENT || c->type == NODE_TEXT || c->type == NODE_FOR ||
            c->type == NODE_IF || c->type == NODE_SLOT)
          has_kids = 1;
      }
    }
    /* recompute has_kids cleanly */
    has_kids = 0;
    for (size_t i = 0; i < node->children_len; i++) {
      NodeType t = node->children[i]->type;
      if (t == NODE_ELEMENT || t == NODE_TEXT || t == NODE_FOR || t == NODE_IF ||
          t == NODE_SLOT || t == NODE_STRING)
        has_kids = 1;
    }

    if (!has_kids) {
      sb_append(sb, " />\n");
      return;
    }
    sb_append(sb, ">\n");
    gen_children(sb, node, depth + 1, ctx);
    sb_indent(sb, depth);
    sb_appendf(sb, "</%s>\n", tag);
    return;
  }

  /* provide Theme value=x → <Theme.Provider value={x}> */
  if (strcmp(tag, "provide") == 0) {
    const char *ctx_name = "Context";
    const char *val = NULL;
    for (size_t i = 0; i < node->children_len; i++) {
      Node *ch = node->children[i];
      if (ch->type == NODE_ATTR && ch->value) {
        if (strcmp(ch->value, "context") == 0 && ch->value2) ctx_name = ch->value2;
        if (strcmp(ch->value, "value") == 0) val = ch->value2;
      }
    }
    sb_indent(sb, depth);
    sb_appendf(sb, "<%s.Provider", ctx_name);
    if (val) {
      sb_append(sb, " value=");
      emit_jsx_value(sb, val, 1);
    }
    sb_append(sb, ">\n");
    gen_children(sb, node, depth + 1, ctx);
    sb_indent(sb, depth);
    sb_appendf(sb, "</%s.Provider>\n", ctx_name);
    if (ctx) ctx->use_context = 1;
    return;
  }

  /* slot → Outlet (layout) or {children} (component composition) */
  if (strcmp(tag, "slot") == 0 || node->type == NODE_SLOT) {
    sb_indent(sb, depth);
    if (ctx && ctx->is_layout) {
      sb_append(sb, "<Outlet />\n");
      ctx->use_router = 1;
    } else {
      sb_append(sb, "{children}\n");
      if (ctx) ctx->use_children = 1;
    }
    return;
  }

  const char *html_tag = html_tag_for(tag);
  const char *base_class = tag_to_div_plus_class(tag);

  if (!html_tag) {
    gen_children(sb, node, depth, ctx);
    return;
  }

  /* link + router → <Link to=...> */
  int use_link = 0;
  const char *to_path = NULL;
  if (strcmp(tag, "link") == 0) {
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_ATTR && c->value &&
          (strcmp(c->value, "to") == 0 || strcmp(c->value, "href") == 0)) {
        to_path = c->value2;
        break;
      }
    }
    if (ctx && ctx->use_router) {
      use_link = 1;
      html_tag = "Link";
    }
  }

  sb_indent(sb, depth);

  int self_closing =
      (strcmp(html_tag, "img") == 0 || strcmp(html_tag, "input") == 0);

  sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  collect_classes(classes, sizeof(classes), node, base_class);
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) sb_appendf(sb, " className=\"%s\"", cls);
  }

  /* Regular attributes */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type != NODE_ATTR || !child->value) continue;
    if (is_style_attr_name(child->value)) continue;

    if (use_link && (strcmp(child->value, "to") == 0 || strcmp(child->value, "href") == 0)) {
      sb_append(sb, " to=");
      emit_jsx_value(sb, child->value2 ? child->value2 : "/", 0);
      continue;
    }

    if (strcmp(child->value, "to") == 0) {
      sb_appendf(sb, " href=\"%s\"", child->value2 ? child->value2 : "/");
      continue;
    }

    if (strcmp(child->value, "action") == 0 && child->value2) {
      /* React 19 <form action={fn}> */
      sb_appendf(sb, " action={%s}", child->value2);
      if (ctx && ctx->action_pending[0] &&
          (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0)) {
        /* Match *Action handler or explicit action_handler name */
        if (ctx->action_handler[0] &&
            strcmp(child->value2, ctx->action_handler) == 0)
          ctx->in_form_with_action = 1;
        else {
          size_t al = strlen(child->value2);
          if (al > 6 && strcmp(child->value2 + al - 6, "Action") == 0)
            ctx->in_form_with_action = 1;
        }
      }
    } else if (strcmp(child->value, "src") == 0 || strcmp(child->value, "alt") == 0 ||
        strcmp(child->value, "href") == 0 || strcmp(child->value, "placeholder") == 0 ||
        strcmp(child->value, "type") == 0 || strcmp(child->value, "rows") == 0 ||
        strcmp(child->value, "name") == 0 || strcmp(child->value, "value") == 0 ||
        strcmp(child->value, "id") == 0 || strcmp(child->value, "key") == 0) {
      const char *an = child->value;
      /* "Hi #{user}" → placeholder={`Hi ${user}`} */
      if (child->value2 && interp_has(child->value2)) {
        char *body = interp_to_js_template_body(child->value2);
        sb_appendf(sb, " %s={`%s`}", an, body ? body : "");
        free(body);
      } else if (child->value2 && looks_like_js_expr(child->value2) &&
          strchr(child->value2, '.')) {
        sb_appendf(sb, " %s={%s}", an, child->value2);
      } else if (child->value2 && looks_like_number(child->value2)) {
        sb_appendf(sb, " %s={%s}", an, child->value2);
      } else {
        sb_appendf(sb, " %s=\"%s\"", an, child->value2 ? child->value2 : "");
      }
    } else if (strcmp(child->value, "bind") == 0 && child->value2) {
      /* bind=email → controlled input with setEmail; pairs with form action/FormData */
      const char *bn = child->value2;
      char setter[128];
      int has_name = 0;
      for (size_t j = 0; j < node->children_len; j++) {
        Node *a = node->children[j];
        if (a->type == NODE_ATTR && a->value && strcmp(a->value, "name") == 0)
          has_name = 1;
      }
      /* form.email → not auto-setter; simple ident only */
      if (strchr(bn, '.')) {
        sb_appendf(sb, " value={%s}", bn);
        sb_appendf(sb, " onChange={(e) => { /* bind %s */ }}", bn);
      } else {
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)bn[0]), bn + 1);
        sb_appendf(sb, " value={%s}", bn);
        sb_appendf(sb, " onChange={(e) => %s(e.target.value)}", setter);
        /* name= for FormData when using form action=… with bind */
        if (!has_name) sb_appendf(sb, " name=\"%s\"", bn);
      }
    } else if (strcmp(child->value, "ref") == 0 && child->value2) {
      sb_appendf(sb, " ref={%s}", child->value2);
    } else {
      /* pass-through as prop-like attr for advanced use */
      sb_appendf(sb, " %s=", child->value);
      emit_jsx_value(sb, child->value2, 0);
    }
  }

  if (strcmp(tag, "checkbox") == 0) sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_ATTR && c->value && strcmp(c->value, "type") == 0) has_type = 1;
      if (c->type == NODE_BOOL_ATTR && c->value) {
        if (strcmp(c->value, "text") == 0 || strcmp(c->value, "email") == 0 ||
            strcmp(c->value, "password") == 0 || strcmp(c->value, "search") == 0 ||
            strcmp(c->value, "number") == 0) {
          if (!has_type) {
            sb_appendf(sb, " type=\"%s\"", c->value);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_BOOL_ATTR && child->value) {
      if (strcmp(child->value, "required") == 0 || strcmp(child->value, "disabled") == 0 ||
          strcmp(child->value, "readonly") == 0 || strcmp(child->value, "checked") == 0) {
        sb_appendf(sb, " %s", child->value);
      }
    }
  }

  /* D4: form with action → aria-busy={pending}; submit button → disabled={pending} */
  if (ctx && ctx->action_pending[0]) {
    if ((strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0) &&
        ctx->in_form_with_action) {
      int has_busy = 0;
      for (size_t i = 0; i < node->children_len; i++) {
        Node *a = node->children[i];
        if (a->type == NODE_ATTR && a->value &&
            (strcmp(a->value, "aria-busy") == 0 ||
             strcmp(a->value, "ariaBusy") == 0))
          has_busy = 1;
      }
      if (!has_busy)
        sb_appendf(sb, " aria-busy={%s}", ctx->action_pending);
    }
    if ((strcmp(tag, "btn") == 0 || strcmp(html_tag, "button") == 0) &&
        ctx->in_form_with_action) {
      int has_dis = 0;
      for (size_t i = 0; i < node->children_len; i++) {
        Node *a = node->children[i];
        if ((a->type == NODE_ATTR || a->type == NODE_BOOL_ATTR) && a->value &&
            strcmp(a->value, "disabled") == 0)
          has_dis = 1;
      }
      if (!has_dis)
        sb_appendf(sb, " disabled={%s}", ctx->action_pending);
    }
  }

  /* Events — @submit always preventDefault (client handlers; form action= uses action prop) */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_EVENT && child->value && child->value2) {
      const char *event = child->value;
      const char *handler = child->value2;
      char react_event[64];
      react_event[0] = 'o';
      react_event[1] = 'n';
      react_event[2] = (char)toupper((unsigned char)event[0]);
      strncpy(react_event + 3, event + 1, sizeof(react_event) - 4);
      react_event[sizeof(react_event) - 1] = '\0';

      if (strcmp(event, "submit") == 0) {
        if (needs_js_arrow(handler))
          sb_appendf(sb, " %s={(e) => { e.preventDefault(); %s; }}", react_event,
                     handler);
        else
          sb_appendf(sb, " %s={(e) => { e.preventDefault(); %s(e); }}", react_event,
                     handler);
      } else if (needs_js_arrow(handler)) {
        sb_appendf(sb, " %s={() => %s}", react_event, handler);
      } else {
        sb_appendf(sb, " %s={%s}", react_event, handler);
      }
    }
  }

  (void)to_path;

  if (self_closing) {
    sb_append(sb, " />\n");
    return;
  }

  sb_append(sb, ">\n");

  /* Implicit children: bare identifiers used as content (span count, h3 product.name) */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type != NODE_BOOL_ATTR || !child->value) continue;
    const char *v = child->value;
    if (strcmp(v, "between") == 0 || strcmp(v, "center") == 0 ||
        strcmp(v, "around") == 0 || strcmp(v, "evenly") == 0 ||
        strcmp(v, "bold") == 0 || strcmp(v, "muted") == 0 ||
        strcmp(v, "sticky") == 0 || strcmp(v, "primary") == 0 ||
        strcmp(v, "outline") == 0 || strcmp(v, "ghost") == 0 ||
        strcmp(v, "secondary") == 0 || strcmp(v, "required") == 0 ||
        strcmp(v, "disabled") == 0 || strcmp(v, "readonly") == 0 ||
        strcmp(v, "checked") == 0 || strcmp(v, "text") == 0 ||
        strcmp(v, "email") == 0 || strcmp(v, "password") == 0 ||
        strcmp(v, "search") == 0 || strcmp(v, "number") == 0 ||
        strcmp(v, "xs") == 0 || strcmp(v, "sm") == 0 || strcmp(v, "lg") == 0 ||
        strcmp(v, "xl") == 0 || strcmp(v, "2xl") == 0 || strcmp(v, "3xl") == 0 ||
        strcmp(v, "4xl") == 0)
      continue;
    /* treat as JS expression child */
    sb_indent(sb, depth + 1);
    sb_appendf(sb, "{%s}\n", v);
  }

  gen_children(sb, node, depth + 1, ctx);
  if (ctx && (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0))
    ctx->in_form_with_action = 0;
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html_tag);
}

static void gen_children(StrBuf *sb, Node *node, int depth, GenCtx *ctx) {
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_FOR) {
      const char *var = child->value ? child->value : "item";
      const char *list = child->value2 ? child->value2 : "items";
      const char *key_expr = "idx";
      for (size_t j = 0; j < child->children_len; j++) {
        Node *ch = child->children[j];
        if (ch->type == NODE_ATTR && ch->value && strcmp(ch->value, "key") == 0 &&
            ch->value2) {
          key_expr = ch->value2;
          break;
        }
      }
      sb_indent(sb, depth);
      sb_appendf(sb, "{(%s || []).map((%s, idx) => (\n", list, var);
      sb_indent(sb, depth + 1);
      sb_appendf(sb, "<Fragment key={%s}>\n", key_expr);
      for (size_t j = 0; j < child->children_len; j++) {
        Node *ch = child->children[j];
        if (ch->type == NODE_ATTR || ch->type == NODE_EVENT ||
            ch->type == NODE_BOOL_ATTR)
          continue;
        gen_node(sb, ch, depth + 2, ctx);
      }
      sb_indent(sb, depth + 1);
      sb_append(sb, "</Fragment>\n");
      sb_indent(sb, depth);
      sb_append(sb, "))}\n");
    } else if (child->type == NODE_IF) {
      const char *cond = child->value ? child->value : "true";
      int has_else = 0;
      size_t else_idx = 0;
      for (size_t j = i + 1; j < node->children_len; j++) {
        if (node->children[j]->type == NODE_TEXT && node->children[j]->value &&
            strcmp(node->children[j]->value, "__else__") == 0) {
          has_else = 1;
          else_idx = j;
          break;
        }
      }

      sb_indent(sb, depth);
      if (has_else)
        sb_appendf(sb, "{%s ? (\n", cond);
      else
        sb_appendf(sb, "{%s && (\n", cond);

      /* wrap multiple children */
      sb_indent(sb, depth + 1);
      sb_append(sb, "<>\n");
      for (size_t j = 0; j < child->children_len; j++) {
        gen_node(sb, child->children[j], depth + 2, ctx);
      }
      sb_indent(sb, depth + 1);
      sb_append(sb, "</>\n");

      if (has_else) {
        sb_indent(sb, depth);
        sb_append(sb, ") : (\n");
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        Node *else_node = node->children[else_idx];
        for (size_t j = 0; j < else_node->children_len; j++) {
          gen_node(sb, else_node->children[j], depth + 2, ctx);
        }
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>\n");
        sb_indent(sb, depth);
        sb_append(sb, ")}\n");
        i = else_idx; /* skip else marker */
      } else {
        sb_indent(sb, depth);
        sb_append(sb, ")}\n");
      }
    } else if (child->type == NODE_SLOT) {
      sb_indent(sb, depth);
      sb_append(sb, "<Outlet />\n");
      if (ctx) ctx->use_router = 1;
    } else if (child->type == NODE_INTERPOLATION) {
      gen_interpolation(sb, child, depth);
    } else if (child->type == NODE_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      sb_indent(sb, depth);
      /* Defensive: expand leftover #{ } in plain text */
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        sb_appendf(sb, "{`%s`}\n", body ? body : "");
        free(body);
      } else if (child->value && looks_like_js_expr(child->value) &&
                 strchr(child->value, '.')) {
        sb_appendf(sb, "{%s}\n", child->value);
      } else {
        sb_append(sb, "{'");
        if (child->value) {
          for (const char *p = child->value; *p; p++) {
            if (*p == '\'' || *p == '\\') sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          }
        }
        sb_append(sb, "'}\n");
      }
    } else if (child->type == NODE_STRING) {
      sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        sb_appendf(sb, "{`%s`}\n", body ? body : "");
        free(body);
      } else {
        sb_appendf(sb, "{'%s'}\n", child->value ? child->value : "");
      }
    } else if (child->type != NODE_ATTR && child->type != NODE_EVENT &&
               child->type != NODE_BOOL_ATTR && child->type != NODE_STYLE_MAP &&
               child->type != NODE_PROPS_DECL && child->type != NODE_STATE_DECL &&
               child->type != NODE_COMPUTED_DECL &&
               child->type != NODE_COMPONENT_DEF && child->type != NODE_ROUTE &&
               child->type != NODE_THEME) {
      gen_node(sb, child, depth, ctx);
    }
  }
}

/* #{expr} leaf or template container → React JSX */
static void gen_interpolation(StrBuf *sb, Node *node, int depth) {
  if (!node) return;

  /* Leaf: NODE_INTERPOLATION with value = expression */
  if (node->value && node->children_len == 0) {
    sb_indent(sb, depth);
    sb_appendf(sb, "{%s}\n", node->value);
    return;
  }

  /* Container with mixed TEXT + INTERP children → one template literal */
  if (node->children_len > 0) {
    sb_indent(sb, depth);
    sb_append(sb, "{`");
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_TEXT && c->value) {
        for (const char *p = c->value; *p; p++) {
          if (*p == '`' || *p == '\\') {
            sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          } else if (*p == '$' && p[1] == '{') {
            sb_append(sb, "\\$");
          } else {
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          }
        }
      } else if (c->type == NODE_INTERPOLATION && c->value) {
        sb_appendf(sb, "${%s}", c->value);
      }
    }
    sb_append(sb, "`}\n");
    return;
  }

  /* Empty container — nothing */
}

static void gen_node(StrBuf *sb, Node *node, int depth, GenCtx *ctx) {
  if (!node) return;

  switch (node->type) {
    case NODE_ROOT:
      gen_children(sb, node, depth, ctx);
      break;
    case NODE_ELEMENT:
      gen_element(sb, node, depth, ctx);
      break;
    case NODE_SLOT:
      sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        sb_append(sb, "<Outlet />\n");
        ctx->use_router = 1;
      } else {
        sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
      break;
    case NODE_SUSPENSE:
    case NODE_LOADING: {
      if (ctx) ctx->use_suspense = 1;
      const char *fb_text = NULL;
      Node *fb_node = NULL;
      for (size_t i = 0; i < node->children_len; i++) {
        Node *c = node->children[i];
        if (c->type == NODE_ATTR && c->value && strcmp(c->value, "fallback") == 0)
          fb_text = c->value2;
        if (c->type == NODE_ELEMENT && c->value &&
            strcmp(c->value, "__fallback__") == 0)
          fb_node = c;
      }
      sb_indent(sb, depth);
      sb_append(sb, "<Suspense fallback={");
      if (fb_node) {
        sb_append(sb, "(\n");
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        for (size_t i = 0; i < fb_node->children_len; i++)
          gen_node(sb, fb_node->children[i], depth + 2, ctx);
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>\n");
        sb_indent(sb, depth);
        sb_append(sb, ")");
      } else if (fb_text) {
        sb_appendf(sb, "<div>%s</div>", fb_text);
      } else {
        sb_append(sb, "<div>Loading...</div>");
      }
      sb_append(sb, "}>\n");
      for (size_t i = 0; i < node->children_len; i++) {
        Node *c = node->children[i];
        if (c->type == NODE_ATTR) continue;
        if (c->type == NODE_ELEMENT && c->value &&
            strcmp(c->value, "__fallback__") == 0)
          continue;
        gen_node(sb, c, depth + 1, ctx);
      }
      sb_indent(sb, depth);
      sb_append(sb, "</Suspense>\n");
      break;
    }
    case NODE_EMPTY: {
      const char *cond = node->value ? node->value : "true";
      sb_indent(sb, depth);
      sb_appendf(sb, "{(%s) ? (\n", cond);
      sb_indent(sb, depth + 1);
      sb_append(sb, "<>\n");
      for (size_t i = 0; i < node->children_len; i++)
        gen_node(sb, node->children[i], depth + 2, ctx);
      sb_indent(sb, depth + 1);
      sb_append(sb, "</>\n");
      sb_indent(sb, depth);
      sb_append(sb, ") : null}\n");
      break;
    }
    case NODE_HEAD:
      /* emitted as useEffect(document.title) in component setup */
      break;
    case NODE_PORTAL: {
      if (ctx) ctx->use_portal = 1;
      sb_indent(sb, depth);
      sb_append(sb, "{createPortal(\n");
      sb_indent(sb, depth + 1);
      sb_append(sb, "<>\n");
      for (size_t i = 0; i < node->children_len; i++)
        gen_node(sb, node->children[i], depth + 2, ctx);
      sb_indent(sb, depth + 1);
      sb_append(sb, "</>,\n");
      sb_indent(sb, depth + 1);
      sb_appendf(sb, "%s\n", node->value ? node->value : "document.body");
      sb_indent(sb, depth);
      sb_append(sb, ")}\n");
      break;
    }
    case NODE_ERROR_BOUNDARY: {
      if (ctx) ctx->use_error_boundary = 1;
      Node *fb_node = NULL;
      for (size_t i = 0; i < node->children_len; i++) {
        Node *c = node->children[i];
        if (c->type == NODE_ELEMENT && c->value &&
            strcmp(c->value, "__fallback__") == 0)
          fb_node = c;
      }
      sb_indent(sb, depth);
      sb_append(sb, "<ErrorBoundary fallback={");
      if (fb_node) {
        sb_append(sb, "(\n");
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        for (size_t i = 0; i < fb_node->children_len; i++)
          gen_node(sb, fb_node->children[i], depth + 2, ctx);
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>\n");
        sb_indent(sb, depth);
        sb_append(sb, ")");
      } else {
        sb_append(sb, "<div>Something went wrong.</div>");
      }
      sb_append(sb, "}>\n");
      for (size_t i = 0; i < node->children_len; i++) {
        Node *c = node->children[i];
        if (c->type == NODE_ELEMENT && c->value &&
            strcmp(c->value, "__fallback__") == 0)
          continue;
        gen_node(sb, c, depth + 1, ctx);
      }
      sb_indent(sb, depth);
      sb_append(sb, "</ErrorBoundary>\n");
      break;
    }
    case NODE_INTERPOLATION:
      gen_interpolation(sb, node, depth);
      break;
    case NODE_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        sb_indent(sb, depth);
        if (interp_has(node->value)) {
          char *body = interp_to_js_template_body(node->value);
          sb_appendf(sb, "{`%s`}\n", body ? body : "");
          free(body);
        } else {
          sb_appendf(sb, "{'%s'}\n", node->value);
        }
      }
      break;
    default:
      gen_children(sb, node, depth, ctx);
      break;
  }
}

/* ── multi-file project generation ─────────────────────── */

typedef enum { RK_PAGE = 0, RK_COMPONENT = 1, RK_LAYOUT = 2 } ReactKind;

#define REACT_MAX_UNITS 64
#define REACT_MAX_USED 24
#define REACT_MAX_ROUTES 64

typedef struct {
  char name[96];      /* export name: HomePage, Counter, DefaultLayout */
  char dir[16];       /* pages | components | layouts */
  char rel[128];      /* pages/HomePage.jsx */
  char source_path[256]; /* optional: src/pages/HomePage.cord */
  Node *def;
  ReactKind kind;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;
  int use_lazy;
  int use_suspense;
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;
  int use_error_boundary;
  int use_link;
  int use_outlet;
  int use_children;
  int use_fragment;
  int needs_contexts_import;
  /* D4: action stub names (unique identifiers) */
  char action_stubs[8][64];
  int n_action_stubs;
  char used[REACT_MAX_USED][96]; /* PascalCase deps */
  int n_used;
  char ctx_names[16][64];
  int n_ctx;
  /* lazy decls: name → import path */
  char lazy_names[16][64];
  char lazy_paths[16][128];
  int n_lazy;
} ReactUnit;

typedef struct {
  ReactUnit units[REACT_MAX_UNITS];
  int n_units;
  Node *routes[REACT_MAX_ROUTES];
  int n_routes;
  Node *page_nodes[64];
  int n_page_nodes;
  Node *contexts[32]; /* NODE_CONTEXT_DECL at root */
  int n_contexts;
  int has_router;
  /* root lazy decl names (route code-splitting) */
  char lazy_route_names[16][64];
  int n_lazy_routes;
} ReactProject;

/* route has boolean attr `lazy` */
static int route_has_lazy_attr(Node *route) {
  if (!route) return 0;
  for (size_t i = 0; i < route->children_len; i++) {
    Node *c = route->children[i];
    if (c && c->type == NODE_BOOL_ATTR && c->value &&
        strcmp(c->value, "lazy") == 0)
      return 1;
  }
  return 0;
}

static int project_is_lazy_route_name(ReactProject *proj, const char *name) {
  if (!proj || !name) return 0;
  for (int i = 0; i < proj->n_lazy_routes; i++) {
    if (strcmp(proj->lazy_route_names[i], name) == 0) return 1;
  }
  return 0;
}

static int route_is_lazy(ReactProject *proj, Node *route) {
  if (!route) return 0;
  if (route_has_lazy_attr(route)) return 1;
  if (route->value2 && project_is_lazy_route_name(proj, route->value2)) return 1;
  return 0;
}

static void emit_js_literal(StrBuf *sb, const char *val) {
  if (!val) {
    sb_append(sb, "null");
    return;
  }
  if (looks_like_number(val) || looks_like_bool(val)) {
    sb_append(sb, val);
    return;
  }
  sb_appendf(sb, "\"%s\"", val);
}

static void layout_export_name(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (is_pascal_case(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

static int name_ends_with(const char *s, const char *suffix) {
  size_t ls = s ? strlen(s) : 0, lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

static int is_route_target(const char *name, Node **routes, int n_routes) {
  if (!name) return 0;
  for (int i = 0; i < n_routes; i++) {
    if (routes[i]->value2 && strcmp(routes[i]->value2, name) == 0) return 1;
  }
  return 0;
}

static void unit_add_used(ReactUnit *u, const char *name) {
  if (!name || !is_pascal_case(name)) return;
  if (strcmp(name, u->name) == 0) return;
  for (int i = 0; i < u->n_used; i++) {
    if (strcmp(u->used[i], name) == 0) return;
  }
  if (u->n_used < REACT_MAX_USED) {
    snprintf(u->used[u->n_used], sizeof(u->used[0]), "%s", name);
    u->n_used++;
  }
}

static void scan_tree_deps(Node *n, ReactUnit *u) {
  if (!n) return;
  if (n->type == NODE_SLOT) {
    if (u->kind == RK_LAYOUT) u->use_outlet = 1;
    else u->use_children = 1;
  }
  if (n->type == NODE_ELEMENT && n->value) {
    if (is_pascal_case(n->value)) unit_add_used(u, n->value);
    if (strcmp(n->value, "link") == 0) u->use_link = 1;
    if (strcmp(n->value, "provide") == 0) u->use_context = 1;
  }
  if (n->type == NODE_STATE_DECL) u->use_state = 1;
  if (n->type == NODE_COMPUTED_DECL) u->use_memo = 1;
  if (n->type == NODE_EFFECT_DECL) u->use_effect = 1;
  if (n->type == NODE_REF_DECL) u->use_ref = 1;
  if (n->type == NODE_CONTEXT_USE) u->use_context = 1;
  if (n->type == NODE_CONTEXT_DECL) {
    u->use_context = 1;
    if (n->value && u->n_ctx < 16) {
      snprintf(u->ctx_names[u->n_ctx], sizeof(u->ctx_names[0]), "%s", n->value);
      u->n_ctx++;
    }
  }
  if (n->type == NODE_REDUCER_DECL) u->use_reducer = 1;
  if (n->type == NODE_PARAMS_DECL) u->use_params = 1;
  if (n->type == NODE_NAVIGATE_DECL) u->use_navigate = 1;
  if (n->type == NODE_CALLBACK_DECL) u->use_callback = 1;
  if (n->type == NODE_ID_DECL) u->use_id = 1;
  if (n->type == NODE_TRANSITION_DECL) u->use_transition = 1;
  if (n->type == NODE_DEFERRED_DECL) u->use_deferred = 1;
  if (n->type == NODE_ACTION_DECL) u->use_action = 1;
  if (n->type == NODE_FETCH_DECL) {
    u->use_state = 1;
    u->use_effect = 1;
  }
  if (n->type == NODE_LAZY_DECL) {
    u->use_lazy = 1;
    if (n->value && n->value2 && u->n_lazy < 16) {
      snprintf(u->lazy_names[u->n_lazy], sizeof(u->lazy_names[0]), "%s", n->value);
      snprintf(u->lazy_paths[u->n_lazy], sizeof(u->lazy_paths[0]), "%s", n->value2);
      u->n_lazy++;
    }
  }
  if (n->type == NODE_LAYOUT_EFFECT) u->use_layout_effect = 1;
  if (n->type == NODE_INSERTION_EFFECT) u->use_insertion_effect = 1;
  if (n->type == NODE_EFFECT_EVENT) u->use_effect_event = 1;
  if (n->type == NODE_EXTERNAL_STORE) u->use_sync_external_store = 1;
  if (n->type == NODE_IMPERATIVE_HANDLE) u->use_imperative_handle = 1;
  if (n->type == NODE_BOOL_ATTR && n->value && strcmp(n->value, "forwardRef") == 0)
    u->use_forward_ref = 1;
  if (n->type == NODE_ACTION_DECL && n->value2 && n->value2[0]) {
    /* collect simple identifier stubs for useActionState targets */
    int simple = 1;
    for (const char *p = n->value2; *p; p++) {
      if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
        simple = 0;
        break;
      }
    }
    if (simple && u->n_action_stubs < 8) {
      int found = 0;
      for (int i = 0; i < u->n_action_stubs; i++) {
        if (strcmp(u->action_stubs[i], n->value2) == 0) {
          found = 1;
          break;
        }
      }
      if (!found) {
        snprintf(u->action_stubs[u->n_action_stubs],
                 sizeof(u->action_stubs[0]), "%s", n->value2);
        u->n_action_stubs++;
      }
    }
  }
  if (n->type == NODE_SUSPENSE || n->type == NODE_LOADING) u->use_suspense = 1;
  if (n->type == NODE_HEAD) u->use_effect = 1;
  if (n->type == NODE_PORTAL) u->use_portal = 1;
  if (n->type == NODE_ERROR_BOUNDARY) u->use_error_boundary = 1;
  if (n->type == NODE_FOR) u->use_fragment = 1;
  for (size_t i = 0; i < n->children_len; i++) {
    scan_tree_deps(n->children[i], u);
  }
}

static ReactUnit *project_find_unit(ReactProject *p, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < p->n_units; i++) {
    if (strcmp(p->units[i].name, name) == 0) return &p->units[i];
  }
  return NULL;
}

static void project_partition(ReactProject *p, Node *root) {
  memset(p, 0, sizeof(*p));
  if (!root) return;

  Node *raw_comps[REACT_MAX_UNITS];
  Node *raw_layouts[16];
  int n_comp = 0, n_layout = 0;

  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c->type == NODE_COMPONENT_DEF) {
      if (c->value2 && strcmp(c->value2, "__layout__") == 0) {
        if (n_layout < 16) raw_layouts[n_layout++] = c;
      } else if (n_comp < REACT_MAX_UNITS) {
        raw_comps[n_comp++] = c;
      }
    } else if (c->type == NODE_ROUTE) {
      if (p->n_routes < REACT_MAX_ROUTES) p->routes[p->n_routes++] = c;
    } else if (c->type == NODE_LAZY_DECL && c->value) {
      if (p->n_lazy_routes < 16) {
        snprintf(p->lazy_route_names[p->n_lazy_routes],
                 sizeof(p->lazy_route_names[0]), "%s", c->value);
        p->n_lazy_routes++;
      }
    } else if (c->type == NODE_CONTEXT_DECL) {
      if (p->n_contexts < 32) p->contexts[p->n_contexts++] = c;
    } else if (c->type != NODE_THEME) {
      if (p->n_page_nodes < 64) p->page_nodes[p->n_page_nodes++] = c;
    }
  }
  /* routes marked lazy= also count as lazy route names */
  for (int r = 0; r < p->n_routes; r++) {
    if (!route_has_lazy_attr(p->routes[r]) || !p->routes[r]->value2) continue;
    if (project_is_lazy_route_name(p, p->routes[r]->value2)) continue;
    if (p->n_lazy_routes < 16) {
      snprintf(p->lazy_route_names[p->n_lazy_routes],
               sizeof(p->lazy_route_names[0]), "%s", p->routes[r]->value2);
      p->n_lazy_routes++;
    }
  }

  p->has_router = (p->n_routes > 0 || n_layout > 0);

  for (int i = 0; i < n_comp; i++) {
    if (p->n_units >= REACT_MAX_UNITS) break;
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s",
             raw_comps[i]->value ? raw_comps[i]->value : "Component");
    u->def = raw_comps[i];
    if (is_route_target(u->name, p->routes, p->n_routes) ||
        name_ends_with(u->name, "Page")) {
      u->kind = RK_PAGE;
      snprintf(u->dir, sizeof(u->dir), "%s", "pages");
    } else {
      u->kind = RK_COMPONENT;
      snprintf(u->dir, sizeof(u->dir), "%s", "components");
    }
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "%s/%s.jsx", u->dir, u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    scan_tree_deps(u->def, u);
  }

  for (int i = 0; i < n_layout; i++) {
    if (p->n_units >= REACT_MAX_UNITS) break;
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    layout_export_name(raw_layouts[i]->value, u->name, sizeof(u->name));
    u->def = raw_layouts[i];
    u->kind = RK_LAYOUT;
    snprintf(u->dir, sizeof(u->dir), "%s", "layouts");
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "layouts/%s.jsx", u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    u->use_outlet = 1;
    p->has_router = 1;
    scan_tree_deps(u->def, u);
  }
}

static void import_path_between(const ReactUnit *from, const ReactUnit *to,
                                char *out, size_t n) {
  if (strcmp(from->dir, to->dir) == 0) {
    snprintf(out, n, "./%s", to->name);
  } else {
    snprintf(out, n, "../%s/%s", to->dir, to->name);
  }
}

static int is_hook_or_decl(NodeType t) {
  return t == NODE_PROPS_DECL || t == NODE_STATE_DECL || t == NODE_COMPUTED_DECL ||
         t == NODE_EFFECT_DECL || t == NODE_LAYOUT_EFFECT || t == NODE_REF_DECL ||
         t == NODE_CONTEXT_DECL || t == NODE_CONTEXT_USE || t == NODE_REDUCER_DECL ||
         t == NODE_PARAMS_DECL || t == NODE_NAVIGATE_DECL || t == NODE_CALLBACK_DECL ||
         t == NODE_ID_DECL || t == NODE_TRANSITION_DECL || t == NODE_DEFERRED_DECL ||
         t == NODE_ACTION_DECL || t == NODE_FETCH_DECL || t == NODE_LAZY_DECL ||
         t == NODE_INSERTION_EFFECT || t == NODE_EFFECT_EVENT ||
         t == NODE_EXTERNAL_STORE || t == NODE_IMPERATIVE_HANDLE ||
         t == NODE_HEAD || t == NODE_BOOL_ATTR;
}

/* Collect simple bind=name targets under a tree (no dots). */
static void collect_bind_names(Node *n, char names[][64], int *count, int max) {
  if (!n || *count >= max) return;
  if (n->type == NODE_ATTR && n->value && strcmp(n->value, "bind") == 0 &&
      n->value2 && !strchr(n->value2, '.')) {
    int found = 0;
    for (int i = 0; i < *count; i++) {
      if (strcmp(names[i], n->value2) == 0) {
        found = 1;
        break;
      }
    }
    if (!found && *count < max) {
      snprintf(names[*count], sizeof(names[0]), "%s", n->value2);
      (*count)++;
    }
  }
  for (size_t i = 0; i < n->children_len; i++)
    collect_bind_names(n->children[i], names, count, max);
}

static int state_name_exists(Node *def, const char *name) {
  if (!def || !name) return 0;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_STATE_DECL && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->type == NODE_STATE_DECL && !c->value) {
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->value && strcmp(c->children[j]->value, name) == 0)
          return 1;
      }
    }
    if (c->type == NODE_ACTION_DECL && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->type == NODE_FETCH_DECL && c->value && strcmp(c->value, name) == 0)
      return 1;
  }
  return 0;
}

/* Emit component function body (no imports / no export keyword) */
static void gen_component_fn(StrBuf *sb, Node *def, const char *name, GenCtx *ctx) {
  Node *props_decl = NULL;
  int has_children_slot = 0;

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_PROPS_DECL && !props_decl) props_decl = c;
    if (c->type == NODE_SLOT && !ctx->is_layout) has_children_slot = 1;
    if (c->type == NODE_BOOL_ATTR && c->value &&
        strcmp(c->value, "forwardRef") == 0)
      ctx->use_forward_ref = 1;
  }
  if (has_children_slot) ctx->use_children = 1;

  sb_appendf(sb, "function %s(", name);
  sb_append(sb, "{ ");
  int prop_n = 0;
  if (props_decl) {
    for (size_t i = 0; i < props_decl->children_len; i++) {
      Node *pr = props_decl->children[i];
      if (!pr->value) continue;
      if (prop_n++) sb_append(sb, ", ");
      sb_append(sb, pr->value);
      if (pr->value2) {
        sb_append(sb, " = ");
        emit_js_literal(sb, pr->value2);
      }
    }
  }
  if (ctx->use_children || has_children_slot) {
    if (prop_n++) sb_append(sb, ", ");
    sb_append(sb, "children");
  }
  sb_append(sb, " }");
  if (ctx->use_forward_ref) sb_append(sb, ", ref");
  sb_append(sb, ") {\n");

  /* ---- hooks (order: state, reducer, context, ref, memo, callback, effect, …) ---- */

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_STATE_DECL && !c->value) {
      for (size_t j = 0; j < c->children_len; j++) {
        Node *st = c->children[j];
        if (!st->value) continue;
        char setter[128];
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)st->value[0]), st->value + 1);
        sb_appendf(sb, "  const [%s, %s] = useState(", st->value, setter);
        emit_js_literal(sb, st->value2 ? st->value2 : "null");
        sb_append(sb, ");\n");
        ctx->use_state = 1;
      }
    } else if (c->type == NODE_STATE_DECL && c->value) {
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)c->value[0]), c->value + 1);
      sb_appendf(sb, "  const [%s, %s] = useState(", c->value, setter);
      emit_js_literal(sb, c->value2 ? c->value2 : "null");
      sb_append(sb, ");\n");
      ctx->use_state = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_REDUCER_DECL && c->value) {
      const char *init = "undefined";
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type == NODE_ATTR && c->children[j]->value &&
            strcmp(c->children[j]->value, "init") == 0 && c->children[j]->value2)
          init = c->children[j]->value2;
      }
      char dispatch[128];
      snprintf(dispatch, sizeof(dispatch), "dispatch%c%s",
               (char)toupper((unsigned char)c->value[0]), c->value + 1);
      /* common: const [cart, dispatch] = useReducer(...) */
      sb_appendf(sb, "  const [%s, dispatch] = useReducer(%s, %s);\n", c->value,
                 c->value2 ? c->value2 : "reducer", init);
      ctx->use_reducer = 1;
      (void)dispatch;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_CONTEXT_USE && c->value && c->value2) {
      sb_appendf(sb, "  const %s = useContext(%s);\n", c->value, c->value2);
      ctx->use_context = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_REF_DECL && c->value) {
      sb_appendf(sb, "  const %s = useRef(", c->value);
      if (c->value2)
        emit_js_literal(sb, c->value2);
      else
        sb_append(sb, "null");
      sb_append(sb, ");\n");
      ctx->use_ref = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_PARAMS_DECL) {
      sb_append(sb, "  const { ");
      sb_append(sb, c->value ? c->value : "id");
      sb_append(sb, " } = useParams();\n");
      ctx->use_params = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_NAVIGATE_DECL) {
      sb_appendf(sb, "  const %s = useNavigate();\n",
                 c->value ? c->value : "navigate");
      ctx->use_navigate = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_ID_DECL && c->value) {
      sb_appendf(sb, "  const %s = useId();\n", c->value);
      ctx->use_id = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_TRANSITION_DECL) {
      sb_appendf(sb, "  const [%s, %s] = useTransition();\n",
                 c->value ? c->value : "isPending",
                 c->value2 ? c->value2 : "startTransition");
      ctx->use_transition = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_DEFERRED_DECL && c->value) {
      sb_appendf(sb, "  const %s = useDeferredValue(%s);\n", c->value,
                 c->value2 ? c->value2 : c->value);
      ctx->use_deferred = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_COMPUTED_DECL && c->value) {
      sb_appendf(sb, "  const %s = useMemo(() => (%s), []);\n", c->value,
                 c->value2 ? c->value2 : "null");
      ctx->use_memo = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_CALLBACK_DECL && c->value) {
      const char *deps = "";
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type == NODE_ATTR && c->children[j]->value &&
            strcmp(c->children[j]->value, "deps") == 0 && c->children[j]->value2)
          deps = c->children[j]->value2;
      }
      /* wrap bare arrows if missing */
      const char *fn = c->value2 ? c->value2 : "() => {}";
      sb_appendf(sb, "  const %s = useCallback(%s, [%s]);\n", c->value, fn,
                 deps);
      ctx->use_callback = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_EFFECT_DECL || c->type == NODE_LAYOUT_EFFECT ||
        c->type == NODE_INSERTION_EFFECT) {
      const char *deps = c->value ? c->value : "";
      const char *body = c->value2 ? c->value2 : "";
      const char *cleanup = NULL;
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type == NODE_ATTR && c->children[j]->value &&
            strcmp(c->children[j]->value, "cleanup") == 0)
          cleanup = c->children[j]->value2;
      }
      const char *hook = "useEffect";
      if (c->type == NODE_LAYOUT_EFFECT)
        hook = "useLayoutEffect";
      else if (c->type == NODE_INSERTION_EFFECT)
        hook = "useInsertionEffect";
      sb_appendf(sb, "  %s(() => {\n", hook);
      if (body && body[0]) sb_appendf(sb, "    %s;\n", body);
      if (cleanup && cleanup[0]) {
        sb_append(sb, "    return () => {\n");
        sb_appendf(sb, "      %s;\n", cleanup);
        sb_append(sb, "    };\n");
      }
      sb_appendf(sb, "  }, [%s]);\n", deps);
      if (c->type == NODE_LAYOUT_EFFECT)
        ctx->use_layout_effect = 1;
      else if (c->type == NODE_INSERTION_EFFECT)
        ctx->use_insertion_effect = 1;
      else
        ctx->use_effect = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_EFFECT_EVENT && c->value) {
      const char *fn = c->value2 ? c->value2 : "() => {}";
      /* If body looks like a bare expression/ident (not arrow/function), wrap */
      int is_fn = (fn[0] == '(' || strncmp(fn, "function", 8) == 0 ||
                   strstr(fn, "=>") != NULL);
      if (is_fn) {
        sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
      } else {
        /* simple name or statement body */
        int simple_id = 1;
        for (const char *p = fn; *p; p++) {
          if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
            simple_id = 0;
            break;
          }
        }
        if (simple_id)
          sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
        else
          sb_appendf(sb, "  const %s = useEffectEvent((..._args) => { %s; });\n",
                     c->value, fn);
      }
      ctx->use_effect_event = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_EXTERNAL_STORE && c->value) {
      const char *sub = c->value2 ? c->value2 : "() => () => {}";
      const char *get_snap = "() => null";
      const char *get_server = NULL;
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type != NODE_ATTR || !c->children[j]->value) continue;
        if (strcmp(c->children[j]->value, "getSnapshot") == 0 &&
            c->children[j]->value2)
          get_snap = c->children[j]->value2;
        if (strcmp(c->children[j]->value, "getServerSnapshot") == 0 &&
            c->children[j]->value2)
          get_server = c->children[j]->value2;
      }
      if (get_server && get_server[0] && strcmp(get_server, "null") != 0)
        sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s, %s);\n",
                   c->value, sub, get_snap, get_server);
      else if (get_server && strcmp(get_server, "null") == 0)
        sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s);\n", c->value,
                   sub, get_snap);
      else
        sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s);\n", c->value,
                   sub, get_snap);
      ctx->use_sync_external_store = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_IMPERATIVE_HANDLE) {
      const char *refn = c->value ? c->value : "ref";
      const char *body = c->value2 ? c->value2 : "";
      const char *deps = "";
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type == NODE_ATTR && c->children[j]->value &&
            strcmp(c->children[j]->value, "deps") == 0 && c->children[j]->value2)
          deps = c->children[j]->value2;
      }
      sb_appendf(sb, "  useImperativeHandle(%s, () => ({\n", refn);
      if (body && body[0]) sb_appendf(sb, "    %s\n", body);
      sb_appendf(sb, "  }), [%s]);\n", deps);
      ctx->use_imperative_handle = 1;
    }
  }

  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type == NODE_ACTION_DECL && c->value) {
      const char *init = "null";
      const char *pending = NULL;
      for (size_t j = 0; j < c->children_len; j++) {
        if (c->children[j]->type != NODE_ATTR || !c->children[j]->value) continue;
        if (strcmp(c->children[j]->value, "init") == 0 && c->children[j]->value2)
          init = c->children[j]->value2;
        if (strcmp(c->children[j]->value, "pending") == 0 && c->children[j]->value2)
          pending = c->children[j]->value2;
      }
      char action_name[128], pending_name[128];
      snprintf(action_name, sizeof(action_name), "%sAction", c->value);
      if (pending)
        snprintf(pending_name, sizeof(pending_name), "%s", pending);
      else
        snprintf(pending_name, sizeof(pending_name), "%sPending", c->value);
      sb_appendf(sb, "  const [%s, %s, %s] = useActionState(%s, %s);\n",
                 c->value, action_name, pending_name,
                 c->value2 ? c->value2 : "async () => null", init);
      ctx->use_action = 1;
      /* remember for form/button polish (first action wins for auto attrs) */
      if (!ctx->action_pending[0]) {
        snprintf(ctx->action_pending, sizeof(ctx->action_pending), "%s",
                 pending_name);
        snprintf(ctx->action_handler, sizeof(ctx->action_handler), "%s",
                 action_name);
        if (c->value2)
          snprintf(ctx->action_fn, sizeof(ctx->action_fn), "%s", c->value2);
      }
    }
  }

  /* fetch products = "/api/products.json" → useState + useEffect */
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type != NODE_FETCH_DECL || !c->value) continue;
    const char *nm = c->value;
    const char *url = c->value2 ? c->value2 : "/";
    char set_n[128], set_err[128], set_load[128];
    snprintf(set_n, sizeof(set_n), "set%c%s",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_err, sizeof(set_err), "set%c%sError",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_load, sizeof(set_load), "set%c%sLoading",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    sb_appendf(sb, "  const [%s, %s] = useState(null);\n", nm, set_n);
    sb_appendf(sb, "  const [%sError, %s] = useState(null);\n", nm, set_err);
    sb_appendf(sb, "  const [%sLoading, %s] = useState(true);\n", nm, set_load);
    sb_append(sb, "  useEffect(() => {\n");
    sb_append(sb, "    let cancelled = false;\n");
    sb_appendf(sb, "    %s(true);\n", set_load);
    sb_appendf(sb, "    %s(null);\n", set_err);
    sb_appendf(sb, "    fetch(\"%s\")\n", url);
    sb_append(sb, "      .then((r) => {\n");
    sb_append(sb, "        if (!r.ok) throw new Error(String(r.status));\n");
    sb_append(sb, "        return r.json();\n");
    sb_append(sb, "      })\n");
    sb_appendf(sb, "      .then((data) => { if (!cancelled) %s(data); })\n",
               set_n);
    sb_appendf(sb, "      .catch((err) => { if (!cancelled) %s(err); })\n",
               set_err);
    sb_appendf(sb,
               "      .finally(() => { if (!cancelled) %s(false); });\n",
               set_load);
    sb_append(sb, "    return () => { cancelled = true; };\n");
    sb_append(sb, "  }, []);\n");
    ctx->use_state = 1;
    ctx->use_effect = 1;
  }

  /* Auto-declare useState for bind= targets not already declared */
  {
    char binds[32][64];
    int n_binds = 0;
    collect_bind_names(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (state_name_exists(def, binds[bi])) continue;
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)binds[bi][0]), binds[bi] + 1);
      sb_appendf(sb, "  const [%s, %s] = useState(\"\");\n", binds[bi], setter);
      ctx->use_state = 1;
    }
  }

  /* title / head → document.title via useEffect */
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c->type != NODE_HEAD || !c->value) continue;
    sb_append(sb, "  useEffect(() => {\n");
    sb_appendf(sb, "    document.title = \"%s\";\n", c->value);
    sb_append(sb, "  }, []);\n");
    ctx->use_effect = 1;
  }

  sb_append(sb, "  return (\n");
  sb_append(sb, "    <>\n");
  int body_count = 0;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (is_hook_or_decl(c->type)) continue;
    gen_node(sb, c, 3, ctx);
    body_count++;
  }
  if (body_count == 0) sb_append(sb, "      null\n");
  sb_append(sb, "    </>\n");
  sb_append(sb, "  );\n");
  sb_append(sb, "}\n");
}

static char *gen_unit_module(ReactProject *proj, ReactUnit *u) {
  GenCtx ctx = {0};
  ctx.use_router = proj->has_router || u->use_link || u->use_outlet;
  ctx.in_component = 1;
  ctx.is_layout = (u->kind == RK_LAYOUT);
  ctx.use_children = u->use_children;

  StrBuf body;
  sb_init(&body);
  gen_component_fn(&body, u->def, u->name, &ctx);

  if (ctx.use_state) u->use_state = 1;
  if (ctx.use_memo) u->use_memo = 1;
  if (ctx.use_effect) u->use_effect = 1;
  if (ctx.use_ref) u->use_ref = 1;
  if (ctx.use_context) u->use_context = 1;
  if (ctx.use_reducer) u->use_reducer = 1;
  if (ctx.use_params) u->use_params = 1;
  if (ctx.use_navigate) u->use_navigate = 1;
  if (ctx.use_callback) u->use_callback = 1;
  if (ctx.use_id) u->use_id = 1;
  if (ctx.use_transition) u->use_transition = 1;
  if (ctx.use_deferred) u->use_deferred = 1;
  if (ctx.use_action) u->use_action = 1;
  if (ctx.use_layout_effect) u->use_layout_effect = 1;
  if (ctx.use_insertion_effect) u->use_insertion_effect = 1;
  if (ctx.use_effect_event) u->use_effect_event = 1;
  if (ctx.use_sync_external_store) u->use_sync_external_store = 1;
  if (ctx.use_imperative_handle) u->use_imperative_handle = 1;
  if (ctx.use_forward_ref) u->use_forward_ref = 1;
  if (ctx.use_suspense) u->use_suspense = 1;
  if (ctx.use_portal) u->use_portal = 1;
  if (ctx.use_error_boundary) u->use_error_boundary = 1;
  if (ctx.use_children) u->use_children = 1;
  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate)
    ctx.use_router = 1;

  StrBuf out;
  sb_init(&out);

  /* C5: source attribution */
  if (u->source_path[0])
    sb_appendf(&out, "/* cordlang: source=%s */\n", u->source_path);

  /* React hooks / symbols import */
  {
    const char *hooks[32];
    int nh = 0;
    if (u->use_state || ctx.use_state) hooks[nh++] = "useState";
    if (u->use_memo || ctx.use_memo) hooks[nh++] = "useMemo";
    if (u->use_effect || ctx.use_effect) hooks[nh++] = "useEffect";
    if (u->use_layout_effect || ctx.use_layout_effect) hooks[nh++] = "useLayoutEffect";
    if (u->use_insertion_effect || ctx.use_insertion_effect)
      hooks[nh++] = "useInsertionEffect";
    if (u->use_effect_event || ctx.use_effect_event) hooks[nh++] = "useEffectEvent";
    if (u->use_sync_external_store || ctx.use_sync_external_store)
      hooks[nh++] = "useSyncExternalStore";
    if (u->use_ref || ctx.use_ref) hooks[nh++] = "useRef";
    if (u->use_imperative_handle || ctx.use_imperative_handle)
      hooks[nh++] = "useImperativeHandle";
    if (u->use_forward_ref || ctx.use_forward_ref) hooks[nh++] = "forwardRef";
    if (u->use_context || ctx.use_context) hooks[nh++] = "useContext";
    if (u->use_reducer || ctx.use_reducer) hooks[nh++] = "useReducer";
    if (u->use_callback || ctx.use_callback) hooks[nh++] = "useCallback";
    if (u->use_id || ctx.use_id) hooks[nh++] = "useId";
    if (u->use_transition || ctx.use_transition) hooks[nh++] = "useTransition";
    if (u->use_deferred || ctx.use_deferred) hooks[nh++] = "useDeferredValue";
    if (u->use_action || ctx.use_action) hooks[nh++] = "useActionState";
    if (u->use_lazy || u->n_lazy > 0) hooks[nh++] = "lazy";
    if (u->use_suspense || ctx.use_suspense) hooks[nh++] = "Suspense";
    if (u->use_portal || ctx.use_portal) hooks[nh++] = "createPortal";
    if (u->use_fragment) hooks[nh++] = "Fragment";
    if (nh > 0) {
      sb_append(&out, "import { ");
      for (int i = 0; i < nh; i++) {
        if (i) sb_append(&out, ", ");
        sb_append(&out, hooks[i]);
      }
      sb_append(&out, " } from 'react';\n");
    }
  }

  if (u->use_error_boundary || ctx.use_error_boundary) {
    if (u->kind == RK_LAYOUT)
      sb_append(&out, "import ErrorBoundary from '../ErrorBoundary';\n");
    else
      sb_append(&out, "import ErrorBoundary from '../ErrorBoundary';\n");
  }

  /* lazy(() => import(...)) declarations */
  for (int i = 0; i < u->n_lazy; i++) {
    char ipath[256];
    /* Map components/X → ../components/X or ./X depending on kind */
    const char *path = u->lazy_paths[i];
    if (strncmp(path, "pages/", 6) == 0)
      snprintf(ipath, sizeof(ipath), "../pages/%s", path + 6);
    else if (strncmp(path, "components/", 11) == 0)
      snprintf(ipath, sizeof(ipath), "../components/%s", path + 11);
    else if (strncmp(path, "layouts/", 8) == 0)
      snprintf(ipath, sizeof(ipath), "../layouts/%s", path + 8);
    else
      snprintf(ipath, sizeof(ipath), "./%s", path);
    /* strip .cord if present */
    size_t pl = strlen(ipath);
    if (pl > 5 && strcmp(ipath + pl - 5, ".cord") == 0) ipath[pl - 5] = '\0';
    sb_appendf(&out, "const %s = lazy(() => import('%s'));\n", u->lazy_names[i],
               ipath);
  }

  /* Router imports */
  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate) {
    sb_append(&out, "import { ");
    int first = 1;
    if (u->use_link) {
      sb_append(&out, "Link");
      first = 0;
    }
    if (u->use_outlet) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "Outlet");
      first = 0;
    }
    if (ctx.use_params || u->use_params) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "useParams");
      first = 0;
    }
    if (ctx.use_navigate || u->use_navigate) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "useNavigate");
    }
    sb_append(&out, " } from 'react-router-dom';\n");
  }

  /* Shared contexts */
  if (proj->n_contexts > 0 && (u->use_context || ctx.use_context)) {
    sb_append(&out, "import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) sb_append(&out, ", ");
      sb_append(&out, proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    if (u->kind == RK_PAGE || u->kind == RK_COMPONENT)
      sb_append(&out, " } from '../contexts';\n");
    else
      sb_append(&out, " } from '../contexts';\n");
  }

  /* Local component imports */
  for (int i = 0; i < u->n_used; i++) {
    ReactUnit *dep = project_find_unit(proj, u->used[i]);
    if (!dep) continue;
    char ipath[256];
    import_path_between(u, dep, ipath, sizeof(ipath));
    sb_appendf(&out, "import %s from '%s';\n", dep->name, ipath);
  }

  if (out.len > 0) sb_append(&out, "\n");

  /* D4: default async action stubs when useActionState refs a bare identifier */
  for (int si = 0; si < u->n_action_stubs; si++) {
    sb_appendf(&out,
               "/* cordlang: default action stub - replace with your server "
               "action */\n"
               "async function %s(prev, formData) {\n"
               "  return null;\n"
               "}\n\n",
               u->action_stubs[si]);
  }

  if (u->use_forward_ref || ctx.use_forward_ref) {
    sb_append(&out, "export default forwardRef(");
    if (body.buf) {
      size_t bl = strlen(body.buf);
      while (bl > 0 &&
             (body.buf[bl - 1] == '\n' || body.buf[bl - 1] == '\r')) {
        body.buf[--bl] = '\0';
      }
      sb_append(&out, body.buf);
    } else {
      sb_append(&out, "function Empty(props, ref){ return null; }");
    }
    sb_append(&out, ");\n");
  } else {
    sb_append(&out, "export default ");
    sb_append(&out, body.buf ? body.buf : "function Empty(){ return null; }\n");
  }

  free(body.buf);
  return out.buf;
}

static char *gen_contexts_module(ReactProject *proj) {
  if (proj->n_contexts == 0) return NULL;
  StrBuf out;
  sb_init(&out);
  sb_append(&out, "import { createContext } from 'react';\n\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    Node *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    sb_appendf(&out, "export const %s = createContext(", name);
    if (c->value2)
      emit_js_literal(&out, c->value2);
    else
      sb_append(&out, "null");
    sb_append(&out, ");\n");
  }
  return out.buf;
}

/* layout= attr on NODE_ROUTE, or NULL if unset */
static const char *route_layout_attr(Node *route) {
  if (!route) return NULL;
  for (size_t i = 0; i < route->children_len; i++) {
    Node *c = route->children[i];
    if (c && c->type == NODE_ATTR && c->value &&
        strcmp(c->value, "layout") == 0 && c->value2 && c->value2[0])
      return c->value2;
  }
  return NULL;
}

/* Resolve layout=name (or default) to a layout unit. */
static ReactUnit *find_layout_for_route(ReactProject *proj, Node *route) {
  const char *attr = route_layout_attr(route);
  char export_name[96];

  if (attr) {
    layout_export_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != RK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0) return &proj->units[i];
      if (proj->units[i].def && proj->units[i].def->value &&
          strcmp(proj->units[i].def->value, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  /* No layout= → default layout (named default / DefaultLayout), else first */
  ReactUnit *first = NULL;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind != RK_LAYOUT) continue;
    if (!first) first = &proj->units[i];
    if (strcmp(proj->units[i].name, "DefaultLayout") == 0) return &proj->units[i];
    if (proj->units[i].def && proj->units[i].def->value &&
        strcmp(proj->units[i].def->value, "default") == 0)
      return &proj->units[i];
  }
  return first;
}

static char *gen_app_module_clean(ReactProject *proj) {
  StrBuf out;
  sb_init(&out);

  /* C5: App shell typically comes from entry src/app.cord */
  sb_append(&out, "/* cordlang: source=src/app.cord */\n");

  int imported[256];
  memset(imported, 0, sizeof(imported));

  int any_lazy_route = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (route_is_lazy(proj, proj->routes[r])) {
      any_lazy_route = 1;
      break;
    }
  }

  if (proj->n_routes > 0 || proj->has_router) {
    sb_append(&out,
              "import { BrowserRouter, Routes, Route } from 'react-router-dom';\n");
    if (any_lazy_route)
      sb_append(&out, "import { lazy, Suspense } from 'react';\n");

    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_LAYOUT) {
        sb_appendf(&out, "import %s from './layouts/%s';\n",
                   proj->units[i].name, proj->units[i].name);
        imported[i] = 1;
      }
    }

    /* static imports for non-lazy route targets */
    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value2;
      if (!comp) continue;
      if (route_is_lazy(proj, proj->routes[r])) continue;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i]) continue;
        if (strcmp(proj->units[i].name, comp) == 0) {
          sb_appendf(&out, "import %s from './%s/%s';\n", proj->units[i].name,
                     proj->units[i].dir, proj->units[i].name);
          imported[i] = 1;
          break;
        }
      }
    }

    /* lazy(() => import(...)) for lazy route targets */
    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value2;
      if (!comp || !route_is_lazy(proj, proj->routes[r])) continue;
      int already = 0;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i] && strcmp(proj->units[i].name, comp) == 0) already = 1;
      }
      /* also skip duplicate lazy decls */
      for (int prev = 0; prev < r; prev++) {
        if (proj->routes[prev]->value2 &&
            strcmp(proj->routes[prev]->value2, comp) == 0 &&
            route_is_lazy(proj, proj->routes[prev]))
          already = 1;
      }
      if (already) continue;
      const char *dir = "pages";
      for (int i = 0; i < proj->n_units; i++) {
        if (strcmp(proj->units[i].name, comp) == 0) {
          dir = proj->units[i].dir;
          imported[i] = 1;
          break;
        }
      }
      sb_appendf(&out, "const %s = lazy(() => import('./%s/%s'));\n", comp, dir,
                 comp);
    }
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    sb_appendf(&out, "import %s from './%s/%s';\n", root->name, root->dir,
               root->name);
  }

  sb_append(&out, "\nexport default function App() {\n");

  if (proj->n_routes > 0) {
    sb_append(&out, "  return (\n");
    sb_append(&out, "    <BrowserRouter>\n");
    if (any_lazy_route)
      sb_append(&out,
                "      <Suspense fallback={<div>Loading...</div>}>\n");
    sb_append(&out, "      <Routes>\n");

    /* Group routes by layout unit (nested <Route element={<Layout />}>). */
    int route_done[REACT_MAX_ROUTES];
    memset(route_done, 0, sizeof(route_done));

    for (int r = 0; r < proj->n_routes; r++) {
      if (route_done[r]) continue;
      ReactUnit *layout = find_layout_for_route(proj, proj->routes[r]);
      if (layout) {
        sb_appendf(&out, "        <Route element={<%s />}>\n", layout->name);
        for (int r2 = 0; r2 < proj->n_routes; r2++) {
          if (route_done[r2]) continue;
          if (find_layout_for_route(proj, proj->routes[r2]) != layout) continue;
          const char *path =
              proj->routes[r2]->value ? proj->routes[r2]->value : "/";
          const char *comp =
              proj->routes[r2]->value2 ? proj->routes[r2]->value2 : "Home";
          sb_appendf(&out, "          <Route path=\"%s\" element={<%s />} />\n",
                     path, comp);
          route_done[r2] = 1;
        }
        sb_append(&out, "        </Route>\n");
      } else {
        const char *path =
            proj->routes[r]->value ? proj->routes[r]->value : "/";
        const char *comp =
            proj->routes[r]->value2 ? proj->routes[r]->value2 : "Home";
        sb_appendf(&out, "        <Route path=\"%s\" element={<%s />} />\n",
                   path, comp);
        route_done[r] = 1;
      }
    }

    sb_append(&out, "      </Routes>\n");
    if (any_lazy_route) sb_append(&out, "      </Suspense>\n");
    sb_append(&out, "    </BrowserRouter>\n");
    sb_append(&out, "  );\n");
  } else if (proj->n_page_nodes > 0 && proj->n_units == 0) {
    GenCtx ctx = {0};
    sb_append(&out, "  return (\n");
    if (proj->n_page_nodes > 1) sb_append(&out, "    <>\n");
    for (int i = 0; i < proj->n_page_nodes; i++) {
      gen_node(&out, proj->page_nodes[i], proj->n_page_nodes > 1 ? 3 : 2, &ctx);
    }
    if (proj->n_page_nodes > 1) sb_append(&out, "    </>\n");
    sb_append(&out, "  );\n");
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    sb_appendf(&out, "  return <%s />;\n", root->name);
  } else {
    sb_append(&out, "  return null;\n");
  }

  sb_append(&out, "}\n");
  return out.buf;
}

/* Attach root-level lazy decls to units that reference them (or first unit).
 * Route-only lazy targets are handled in App.jsx — skip them here. */
static void attach_root_lazies(ReactProject *proj, Node *root) {
  if (!root) return;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c->type != NODE_LAZY_DECL || !c->value || !c->value2) continue;
    /* Route code-split pages: App owns the lazy() import */
    if (is_route_target(c->value, proj->routes, proj->n_routes)) continue;
    int attached = 0;
    for (int u = 0; u < proj->n_units; u++) {
      int uses = 0;
      for (int k = 0; k < proj->units[u].n_used; k++) {
        if (strcmp(proj->units[u].used[k], c->value) == 0) uses = 1;
      }
      if (uses && proj->units[u].n_lazy < 16) {
        snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
        snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
        proj->units[u].n_lazy++;
        proj->units[u].use_lazy = 1;
        attached = 1;
      }
    }
    if (!attached && proj->n_units > 0 && proj->units[0].n_lazy < 16) {
      int u = 0;
      for (int j = 0; j < proj->n_units; j++) {
        if (proj->units[j].kind != RK_LAYOUT) {
          u = j;
          break;
        }
      }
      snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
      snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
      proj->units[u].n_lazy++;
      proj->units[u].use_lazy = 1;
    }
  }
}

int react_emit_modules(Node *root, ReactWriteFn write_fn, void *userdata) {
  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return -1;
  project_partition(proj, root);
  attach_root_lazies(proj, root);

  /* Also harvest context decls nested inside component defs */
  for (int i = 0; i < proj->n_units; i++) {
    Node *def = proj->units[i].def;
    if (!def) continue;
    for (size_t j = 0; j < def->children_len; j++) {
      Node *c = def->children[j];
      if (c->type == NODE_CONTEXT_DECL && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module(proj);
    if (ctxm) {
      int rc = write_fn("contexts.jsx", ctxm, userdata);
      free(ctxm);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_unit_module(proj, &proj->units[i]);
    if (!mod) {
      free(proj);
      return -1;
    }
    int rc = write_fn(proj->units[i].rel, mod, userdata);
    free(mod);
    if (rc != 0) {
      free(proj);
      return rc;
    }
  }

  char *app = gen_app_module_clean(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.jsx", app, userdata);
  free(app);
  free(proj);
  return rc;
}

/* Internal AST emit — only called via IR origin (IR-first pipeline). */
static char *react_generate_impl(Node *root) {
  /* Joined multi-file view for `cordlang compile` (readable sections) */
  if (!root) return strdup("export default function App(){return null}\n");

  StrBuf sb;
  sb_init(&sb);

  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return strdup("export default function App(){return null}\n");
  project_partition(proj, root);
  attach_root_lazies(proj, root);

  for (int i = 0; i < proj->n_units; i++) {
    Node *def = proj->units[i].def;
    if (!def) continue;
    for (size_t j = 0; j < def->children_len; j++) {
      Node *c = def->children[j];
      if (c->type == NODE_CONTEXT_DECL && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module(proj);
    sb_append(&sb, "// ===== src/contexts.jsx =====\n");
    sb_append(&sb, ctxm ? ctxm : "");
    sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_unit_module(proj, &proj->units[i]);
    sb_appendf(&sb, "// ===== src/%s =====\n", proj->units[i].rel);
    sb_append(&sb, mod ? mod : "");
    sb_append(&sb, "\n");
    free(mod);
  }
  char *app = gen_app_module_clean(proj);
  sb_append(&sb, "// ===== src/App.jsx =====\n");
  sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

/* react_generate_from_ir is implemented in react_ir.c (pure IrNode walkers). */

/* Legacy AST API: always lower through IR so the pipeline is uniform. */
char *react_generate(Node *root) {
  if (!root) return strdup("export default function App(){return null}\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return react_generate_impl(root);
  char *out = react_generate_from_ir(ir);
  ir_free(ir);
  return out;
}

