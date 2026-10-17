#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

void svelte_sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

void svelte_sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) svelte_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) svelte_sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

void svelte_sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) svelte_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) svelte_sb_oom();
    sb->buf = nbuf;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

void svelte_sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) svelte_sb_append(sb, "  ");
}

int svelte_looks_like_js_expr(const char *s) {
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
  return !after_dot;
}

void svelte_emit_svelte_prop_value(StrBuf *sb, const char *val) {
  if (!val) {
    svelte_sb_append(sb, "{undefined}");
    return;
  }
  if (irw_looks_number(val) || irw_looks_bool(val)) {
    svelte_sb_appendf(sb, "{%s}", val);
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
      svelte_sb_appendf(sb, "{%s}", val);
      return;
    }
  }
  svelte_sb_appendf(sb, "\"%s\"", val);
}

int svelte_is_style_attr(const char *name) {
  return irw_is_style_attr(name);
}

int svelte_attr_is_true(const IrNode *a) {
  return a && a->value &&
         (a->value[0] == '\0' || strcmp(a->value, "true") == 0);
}

void svelte_emit_js_lit(StrBuf *sb, const char *val) {
  if (!val) {
    svelte_sb_append(sb, "null");
    return;
  }
  if (irw_looks_number(val) || irw_looks_bool(val)) {
    svelte_sb_append(sb, val);
    return;
  }
  char *esc = js_escape_sq_dup(val);
  svelte_sb_appendf(sb, "'%s'", esc ? esc : "");
  free(esc);
}
