#include "adapters/outbound/backends/react/react_ir_internal.h"

void react_ir_sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

void react_ir_sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

void react_ir_sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) react_ir_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) react_ir_sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

void react_ir_sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) react_ir_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) react_ir_sb_oom();
    sb->buf = nbuf;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

void react_ir_sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) react_ir_sb_append(sb, "  ");
}

int react_ir_is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

int react_ir_looks_like_number(const char *s) { return cord_looks_like_number(s); }

int react_ir_looks_like_bool(const char *s) { return cord_looks_like_bool(s); }

int react_ir_is_dom_a11y_or_data_attr(const char *name) {
  return cord_is_dom_a11y_or_data_attr(name);
}

int react_ir_looks_like_js_expr(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) return 0;
  int after_dot = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '.') {
      if (after_dot) return 0;
      after_dot = 1;
      continue;
    }
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
    after_dot = 0;
  }
  if (after_dot) return 0;
  return 1;
}

int react_ir_is_stringish_prop_name(const char *name) {
  return name &&
         (strcmp(name, "title") == 0 || strcmp(name, "text") == 0 ||
          strcmp(name, "label") == 0 || strcmp(name, "placeholder") == 0 ||
          strcmp(name, "alt") == 0 || strcmp(name, "name") == 0);
}

int react_ir_needs_js_arrow(const char *handler) {
  if (!handler || !*handler) return 0;
  if (strchr(handler, '(') || strchr(handler, '+') || strchr(handler, '-') ||
      strchr(handler, '*') || strchr(handler, '/') || strchr(handler, ' ') ||
      strchr(handler, '?') || strchr(handler, ':') || strchr(handler, '='))
    return 1;
  return 0;
}

int react_ir_attr_is_true(const char *v) { return cord_attr_is_true(v); }

int react_ir_is_style_attr_name(const char *name) {
  return cord_is_style_attr_name(name);
}

int react_ir_is_style_bool_name(const char *name) {
  return cord_is_style_bool_name(name);
}

const char *react_ir_tag_to_div_plus_class(const char *tag) {
  return cord_tag_base_class(tag);
}

const char *react_ir_html_tag_for(const char *tag) {
  return cord_html_tag_for(tag);
}

void react_ir_emit_jsx_value(StrBuf *sb, const char *val, int force_expr) {
  if (!val) {
    react_ir_sb_append(sb, "{undefined}");
    return;
  }
  if (react_ir_looks_like_number(val) || react_ir_looks_like_bool(val) || force_expr) {
    react_ir_sb_appendf(sb, "{%s}", val);
    return;
  }
  if ((isalpha((unsigned char)val[0]) || val[0] == '_') &&
      strchr(val, ' ') == NULL && strchr(val, '"') == NULL) {
    int has_dot = strchr(val, '.') != NULL;
    int all_ident = 1;
    for (const char *p = val; *p; p++) {
      if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')) {
        all_ident = 0;
        break;
      }
    }
    if (all_ident &&
        (has_dot || islower((unsigned char)val[0]) || val[0] == '_')) {
      react_ir_sb_appendf(sb, "{%s}", val);
      return;
    }
  }
  char *esc = js_escape_dq_dup(val);
  react_ir_sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}

void react_ir_emit_js_literal(StrBuf *sb, const char *val) {
  if (!val) {
    react_ir_sb_append(sb, "null");
    return;
  }
  if (react_ir_looks_like_number(val) || react_ir_looks_like_bool(val)) {
    react_ir_sb_append(sb, val);
    return;
  }
  char *esc = js_escape_dq_dup(val);
  react_ir_sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}

void react_ir_collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class) {
  cord_collect_classes(classes, classes_sz, node, base_class);
}
