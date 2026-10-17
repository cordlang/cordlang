#include "adapters/outbound/backends/solid/solid_ir_internal.h"

void solid_ir_sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

void solid_ir_sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

void solid_ir_sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) solid_ir_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) solid_ir_sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

void solid_ir_sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) solid_ir_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) solid_ir_sb_oom();
    sb->buf = nbuf;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

void solid_ir_sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) solid_ir_sb_append(sb, "  ");
}

int solid_ir_is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

int solid_ir_looks_like_number(const char *s) {
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

int solid_ir_looks_like_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

int solid_ir_is_dom_a11y_or_data_attr(const char *name) {
  if (!name || !*name) return 0;
  if (strcmp(name, "role") == 0) return 1;
  if (strncmp(name, "aria-", 5) == 0) return 1;
  if (strncmp(name, "data-", 5) == 0) return 1;
  return 0;
}

int solid_ir_looks_like_js_expr(const char *s) {
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

int solid_ir_needs_js_arrow(const char *handler) {
  if (!handler || !*handler) return 0;
  if (strchr(handler, '(') || strchr(handler, '+') || strchr(handler, '-') ||
      strchr(handler, '*') || strchr(handler, '/') || strchr(handler, ' ') ||
      strchr(handler, '?') || strchr(handler, ':') || strchr(handler, '='))
    return 1;
  return 0;
}

int solid_ir_attr_is_true(const char *v) {
  return !v || !*v || strcmp(v, "true") == 0;
}

int solid_ir_is_style_attr_name(const char *name) {
  return name &&
         (strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
          strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
          strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
          strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
          strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
          strcmp(name, "overflow") == 0 || strcmp(name, "fit") == 0 ||
          strcmp(name, "aspect") == 0 || strcmp(name, "lines") == 0 ||
          strcmp(name, "w") == 0 || strcmp(name, "h") == 0 ||
          strcmp(name, "min-h") == 0 || strcmp(name, "border") == 0 ||
          strcmp(name, "mx") == 0 || strcmp(name, "my") == 0 ||
          strcmp(name, "px") == 0 || strcmp(name, "py") == 0 ||
          strcmp(name, "m") == 0 || strcmp(name, "op") == 0 ||
          strcmp(name, "z") == 0);
}

int solid_ir_is_style_bool_name(const char *name) {
  return name &&
         (strcmp(name, "between") == 0 || strcmp(name, "center") == 0 ||
          strcmp(name, "around") == 0 || strcmp(name, "evenly") == 0 ||
          strcmp(name, "bold") == 0 || strcmp(name, "muted") == 0 ||
          strcmp(name, "sticky") == 0 || strcmp(name, "primary") == 0 ||
          strcmp(name, "outline") == 0 || strcmp(name, "ghost") == 0 ||
          strcmp(name, "secondary") == 0 || strcmp(name, "xs") == 0 ||
          strcmp(name, "sm") == 0 || strcmp(name, "lg") == 0 ||
          strcmp(name, "xl") == 0 || strcmp(name, "2xl") == 0 ||
          strcmp(name, "3xl") == 0 || strcmp(name, "4xl") == 0 ||
          strcmp(name, "flex-1") == 0 || strcmp(name, "font-mono") == 0 ||
          strcmp(name, "border") == 0);
}

const char *solid_ir_tag_to_div_plus_class(const char *tag) {
  if (!tag) return NULL;
  if (strcmp(tag, "col") == 0) return "flex flex-col";
  if (strcmp(tag, "row") == 0) return "flex flex-row";
  if (strcmp(tag, "stack") == 0) return "flex flex-col";
  if (strcmp(tag, "grid") == 0) return "grid";
  if (strcmp(tag, "page") == 0) return "min-h-screen";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

const char *solid_ir_html_tag_for(const char *tag) {
  if (!tag) return "div";
  if (strcmp(tag, "col") == 0 || strcmp(tag, "row") == 0 ||
      strcmp(tag, "stack") == 0 || strcmp(tag, "page") == 0 ||
      strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
      strcmp(tag, "group") == 0)
    return "div";
  if (strcmp(tag, "btn") == 0 || strcmp(tag, "button") == 0) return "button";
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) return "input";
  if (strcmp(tag, "icon") == 0) return "CordIcon";
  if (strcmp(tag, "motion") == 0 || strcmp(tag, "Motion") == 0) return "CordMotion";
  if (strcmp(tag, "chart") == 0 || strcmp(tag, "Chart") == 0) return "CordChart";
  return tag;
}

void solid_ir_solid_track_signal(GenCtx *ctx, const char *name) {
  if (!ctx || !name || !*name || ctx->n_signals >= 32) return;
  for (int i = 0; i < ctx->n_signals; i++)
    if (strcmp(ctx->signals[i], name) == 0) return;
  snprintf(ctx->signals[ctx->n_signals], sizeof(ctx->signals[0]), "%s", name);
  ctx->n_signals++;
}



void solid_ir_emit_jsx_value(StrBuf *sb, const char *val, int force_expr) {
  if (!val) {
    solid_ir_sb_append(sb, "{undefined}");
    return;
  }
  if (solid_ir_looks_like_number(val) || solid_ir_looks_like_bool(val) || force_expr) {
    solid_ir_sb_appendf(sb, "{%s}", val);
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
      solid_ir_sb_appendf(sb, "{%s}", val);
      return;
    }
  }
  char *esc = js_escape_dq_dup(val);
  solid_ir_sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}

void solid_ir_emit_js_literal(StrBuf *sb, const char *val) {
  if (!val) {
    solid_ir_sb_append(sb, "null");
    return;
  }
  if (solid_ir_looks_like_number(val) || solid_ir_looks_like_bool(val)) {
    solid_ir_sb_append(sb, val);
    return;
  }
  char *esc = js_escape_dq_dup(val);
  solid_ir_sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}
