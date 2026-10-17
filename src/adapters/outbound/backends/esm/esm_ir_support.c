#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_sb_init(Sb *sb) {
  sb->cap = 16384;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
  if (!sb->buf) {
    fprintf(stderr, "fatal: out of memory (esm Sb)\n");
    exit(1);
  }
}

void esm_ir_sb_grow(Sb *sb, size_t extra) {
  if (sb->len + extra + 1 < sb->cap) return;
  while (sb->len + extra + 1 >= sb->cap) {
    if (sb->cap > (size_t)-1 / 2) {
      fprintf(stderr, "fatal: out of memory (esm Sb)\n");
      exit(1);
    }
    sb->cap *= 2;
  }
  char *nb = realloc(sb->buf, sb->cap);
  if (!nb) {
    fprintf(stderr, "fatal: out of memory (esm Sb)\n");
    exit(1);
  }
  sb->buf = nb;
}

void esm_ir_sb_add(Sb *sb, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  esm_ir_sb_grow(sb, n);
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

void esm_ir_sb_addf(Sb *sb, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  esm_ir_sb_grow(sb, (size_t)n);
  va_start(ap, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
  va_end(ap);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

void esm_ir_sb_pad(Sb *sb, int depth) {
  for (int i = 0; i < depth; i++) esm_ir_sb_add(sb, "  ");
}

void esm_ir_scope_add(EsmCtx *c, const char *name) {
  if (!name || !*name || strlen(name) >= 64) return;
  if (c->n_scope >= ESM_MAX_SCOPE) return;
  for (int i = 0; i < c->n_scope; i++)
    if (strcmp(c->scope[i], name) == 0) return;
  snprintf(c->scope[c->n_scope], 64, "%s", name);
  c->n_scope++;
}

int esm_ir_scope_has(EsmCtx *c, const char *expr) {
  if (!c || !expr || !*expr) return 0;
  char root[64];
  size_t i = 0;
  for (; expr[i] && expr[i] != '.' && i < sizeof(root) - 1; i++) root[i] = expr[i];
  root[i] = '\0';
  for (int j = 0; j < c->n_scope; j++)
    if (strcmp(c->scope[j], root) == 0) return 1;
  return 0;
}

void esm_ir_scope_add_list(EsmCtx *c, const char *list) {
  if (!list) return;
  const char *p = list;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == '\t') p++;
    if (!*p) break;
    char buf[64];
    size_t n = 0;
    while (*p && *p != ' ' && *p != ',' && *p != '\t' && n < sizeof(buf) - 1)
      buf[n++] = *p++;
    buf[n] = '\0';
    esm_ir_scope_add(c, buf);
  }
}

int esm_ir_is_pascal(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

int esm_ir_is_safe_js_ident(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) return 0;
  for (const char *p = s; *p; p++)
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
  return 1;
}

int esm_ir_is_safe_js_path(const char *s) {
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

void esm_ir_emit_sq_string(Sb *sb, const char *s) {
  char *esc = js_escape_sq_dup(s ? s : "");
  esm_ir_sb_addf(sb, "'%s'", esc ? esc : "");
  free(esc);
}

void esm_ir_emit_literal(Sb *sb, const char *val) {
  if (!val) {
    esm_ir_sb_add(sb, "null");
    return;
  }
  if (cord_looks_like_number(val) || cord_looks_like_bool(val)) {
    esm_ir_sb_add(sb, val);
    return;
  }
  esm_ir_emit_sq_string(sb, val);
}

void esm_ir_emit_value(Sb *sb, const char *val, int prefer_string, EsmCtx *c) {
  if (!val) {
    esm_ir_sb_add(sb, "null");
    return;
  }
  if (interp_has(val)) {
    char *body = interp_to_js_template_body(val);
    esm_ir_sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  if (!prefer_string) {
    if (cord_looks_like_number(val) || cord_looks_like_bool(val)) {
      esm_ir_sb_add(sb, val);
      return;
    }
    if (esm_ir_is_safe_js_path(val) && esm_ir_scope_has(c, val)) {
      esm_ir_sb_add(sb, val);
      return;
    }
  }
  char *plain = interp_plain_text(val);
  esm_ir_emit_sq_string(sb, plain ? plain : val);
  free(plain);
}

int esm_ir_handler_needs_arrow(const char *h) {
  if (!h || !*h) return 0;
  return strpbrk(h, "()+-*/ ?:=") != NULL;
}

void esm_ir_emit_handler(Sb *sb, const char *event, const char *handler) {
  if (!handler) {
    esm_ir_sb_add(sb, "undefined");
    return;
  }
  int is_submit = event && strcmp(event, "submit") == 0;
  if (is_submit) {
    if (esm_ir_handler_needs_arrow(handler))
      esm_ir_sb_addf(sb, "(e) => { e.preventDefault(); %s; }", handler);
    else
      esm_ir_sb_addf(sb, "(e) => { e.preventDefault(); %s(e); }", handler);
    return;
  }
  if (esm_ir_handler_needs_arrow(handler))
    esm_ir_sb_addf(sb, "(e) => { %s; }", handler);
  else
    esm_ir_sb_add(sb, handler);
}

void esm_ir_event_prop_name(const char *event, char *out, size_t n) {
  if (!event || !*event) {
    snprintf(out, n, "onEvent");
    return;
  }
  snprintf(out, n, "on%c%s", (char)toupper((unsigned char)event[0]), event + 1);
}

void esm_ir_path_basename_noext(const char *mod_path, char *out, size_t n) {
  const char *base = mod_path;
  for (const char *p = mod_path; *p; p++)
    if (*p == '/' || *p == '\\') base = p + 1;
  snprintf(out, n, "%s", base);
  size_t l = strlen(out);
  if (l > 5 && strcmp(out + l - 5, ".cord") == 0) out[l - 5] = '\0';
}
