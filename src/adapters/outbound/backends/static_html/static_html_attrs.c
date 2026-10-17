#include "static_html_internal.h"

int statich_ir_attr_is_true(const char *v) {
  return v && strcmp(v, "true") == 0;
}

int statich_is_decl_only(const IrNode *n) {
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

int statich_tag_forbidden(const char *tag) {
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

int statich_tag_safe_name(const char *tag) {
  if (!tag || !*tag) return 0;
  if (!isalpha((unsigned char)tag[0])) return 0;
  for (const char *p = tag + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '-')) return 0;
  }
  return 1;
}

LayKind statich_lay_kind(const char *tag) {
  if (!tag) return LAY_NONE;
  if (strcmp(tag, "col") == 0 || strcmp(tag, "stack") == 0 ||
      strcmp(tag, "page") == 0)
    return LAY_COL;
  if (strcmp(tag, "row") == 0) return LAY_ROW;
  return LAY_NONE;
}

const char *statich_html_tag_for(const char *tag) {
  if (!tag) return "div";
  if (statich_lay_kind(tag) != LAY_NONE) return NULL; /* table layout */
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
  if (statich_tag_forbidden(tag) || !statich_tag_safe_name(tag)) return "div";
  return tag;
}

int statich_px_from_attr(const char *v, int fallback) {
  if (!v || !*v) return fallback;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (end == v) return fallback;
  if (n < 0) n = 0;
  if (n > 4096) n = 4096;
  return (int)n;
}

void statich_collect_inline_style(StrBuf *style, IrNode *node, LayKind lay) {
  if (lay == LAY_COL)
    statich_sb_append(style, "width:100%;border-collapse:collapse;");
  else if (lay == LAY_ROW)
    statich_sb_append(style, "width:100%;border-collapse:collapse;");

  int center = 0, bold = 0, muted = 0;
  const char *variant = NULL;
  const char *size = NULL;
  const char *bg = NULL;
  int gap = -1, pad = -1;

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (statich_ir_attr_is_true(c->value)) {
      if (strcmp(c->name, "center") == 0) center = 1;
      else if (strcmp(c->name, "bold") == 0) bold = 1;
      else if (strcmp(c->name, "muted") == 0) muted = 1;
      else if (strcmp(c->name, "primary") == 0) variant = "primary";
      else if (strcmp(c->name, "outline") == 0) variant = "outline";
      else if (strcmp(c->name, "ghost") == 0) variant = "ghost";
      continue;
    }
    if (!c->value) continue;
    if (strcmp(c->name, "gap") == 0) gap = statich_px_from_attr(c->value, 8);
    else if (strcmp(c->name, "p") == 0) pad = statich_px_from_attr(c->value, 16);
    else if (strcmp(c->name, "variant") == 0) variant = c->value;
    else if (strcmp(c->name, "size") == 0) size = c->value;
    else if (strcmp(c->name, "bg") == 0) bg = c->value;
  }

  if (pad >= 0) statich_sb_appendf(style, "padding:%dpx;", pad);
  if (gap >= 0 && lay != LAY_NONE) {
    /* gap applied on cells later via data — store as padding hint on table */
    (void)gap;
  }
  if (center) statich_sb_append(style, "text-align:center;");
  if (bold) statich_sb_append(style, "font-weight:700;");
  if (muted) statich_sb_append(style, "color:#6b7280;");
  if (bg) {
    if (strcmp(bg, "white") == 0) statich_sb_append(style, "background:#ffffff;");
    else if (strcmp(bg, "gray-50") == 0) statich_sb_append(style, "background:#f9fafb;");
    else statich_sb_appendf(style, "background:%s;", bg);
  }
  if (size) {
    if (strcmp(size, "xs") == 0) statich_sb_append(style, "font-size:12px;");
    else if (strcmp(size, "sm") == 0) statich_sb_append(style, "font-size:14px;");
    else if (strcmp(size, "lg") == 0) statich_sb_append(style, "font-size:18px;");
    else if (strcmp(size, "xl") == 0) statich_sb_append(style, "font-size:20px;");
    else if (strcmp(size, "2xl") == 0) statich_sb_append(style, "font-size:24px;");
    else if (strcmp(size, "3xl") == 0) statich_sb_append(style, "font-size:30px;");
    else if (strcmp(size, "4xl") == 0) statich_sb_append(style, "font-size:36px;");
  }
  if (variant) {
    if (strcmp(variant, "primary") == 0)
      statich_sb_append(style,
                "background:#2563eb;color:#ffffff;padding:10px 16px;"
                "text-decoration:none;border-radius:6px;display:inline-block;");
    else if (strcmp(variant, "outline") == 0)
      statich_sb_append(style,
                "background:#ffffff;color:#111827;border:1px solid #d1d5db;"
                "padding:10px 16px;text-decoration:none;border-radius:6px;"
                "display:inline-block;");
    else if (strcmp(variant, "ghost") == 0)
      statich_sb_append(style,
                "background:transparent;color:#374151;padding:10px 16px;"
                "text-decoration:none;display:inline-block;");
  }
  if (node->name && strcmp(node->name, "card") == 0) {
    statich_sb_append(style,
              "width:100%;max-width:560px;border-collapse:collapse;"
              "background:#ffffff;border:1px solid #e5e7eb;border-radius:12px;");
  }
}

int statich_attr_gap(IrNode *node) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "gap") == 0)
      return statich_px_from_attr(c->value, 8);
  }
  return 8;
}

void statich_emit_snapshot_text(StrBuf *sb, const char *expr_or_text) {
  if (!expr_or_text) return;
  /* Resolve #{name} against bind table; leave unknown as [name]. */
  const char *p = expr_or_text;
  while (*p) {
    if (p[0] == '#' && p[1] == '{') {
      const char *end = strchr(p + 2, '}');
      if (!end) {
        statich_html_escape_append(sb, p);
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
      const char *val = statich_bind_get(s);
      if (val)
        statich_html_escape_append(sb, val);
      else {
        statich_sb_append(sb, "[");
        statich_html_escape_append(sb, s);
        statich_sb_append(sb, "]");
      }
      p = end + 1;
    } else if (p[0] == '\\' && p[1] == '#' && p[2] == '{') {
      statich_sb_append(sb, "#{");
      p += 3;
    } else {
      char c[2] = {*p, 0};
      if (*p == '&' || *p == '<' || *p == '>' || *p == '"' || *p == '\'')
        statich_html_escape_append(sb, c);
      else
        statich_sb_append(sb, c);
      p++;
    }
  }
}

void statich_emit_html_attrs(StrBuf *sb, IrNode *node, const char *tag) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name || statich_ir_attr_is_true(c->value))
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
    if (strcmp(an, "purpose") == 0) an = "data-purpose";
    if (strcmp(an, "importance") == 0) an = "data-importance";
    if (strcmp(an, "src") == 0 || strcmp(an, "alt") == 0 ||
        strcmp(an, "href") == 0 || strcmp(an, "width") == 0 ||
        strcmp(an, "height") == 0 || strcmp(an, "title") == 0 ||
        strcmp(an, "data-purpose") == 0 ||
        strcmp(an, "data-importance") == 0) {
      statich_sb_appendf(sb, " %s=\"", an);
      statich_html_escape_append(sb, c->value ? c->value : "");
      statich_sb_append(sb, "\"");
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
    if (!has_href) statich_sb_append(sb, " href=\"#\"");
  }
  (void)tag;
}
