#include "domain/diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void diag_list_init(DiagList *d) {
  if (!d) return;
  d->items = NULL;
  d->len = 0;
  d->cap = 0;
}

void diag_list_free(DiagList *d) {
  if (!d) return;
  for (size_t i = 0; i < d->len; i++) {
    free(d->items[i].file);
    free(d->items[i].message);
    free(d->items[i].code);
    free(d->items[i].hint);
  }
  free(d->items);
  d->items = NULL;
  d->len = 0;
  d->cap = 0;
}

static int diag_grow(DiagList *d) {
  if (d->len < d->cap) return 1;
  size_t ncap = d->cap ? d->cap * 2 : 16;
  Diagnostic *ni = realloc(d->items, ncap * sizeof(Diagnostic));
  if (!ni) return 0;
  d->items = ni;
  d->cap = ncap;
  return 1;
}

static void diag_emit_va(DiagList *d, DiagLevel level, const char *file,
                         int line, int col, const char *code, const char *hint,
                         const char *fmt, va_list ap) {
  if (!d || !fmt) return;
  if (!diag_grow(d)) return;

  Diagnostic *item = &d->items[d->len];
  item->level = level;
  item->file = file ? strdup(file) : NULL;
  item->line = line;
  item->col = col;
  item->code = code && code[0] ? strdup(code) : NULL;
  item->hint = hint && hint[0] ? strdup(hint) : NULL;

  char buf[1024];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  item->message = strdup(buf);

  d->len++;
}

void diag_emit(DiagList *d, DiagLevel level, const char *file, int line, int col,
               const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  diag_emit_va(d, level, file, line, col, NULL, NULL, fmt, ap);
  va_end(ap);
}

void diag_emit_ex(DiagList *d, DiagLevel level, const char *file, int line,
                  int col, const char *code, const char *hint, const char *fmt,
                  ...) {
  va_list ap;
  va_start(ap, fmt);
  diag_emit_va(d, level, file, line, col, code, hint, fmt, ap);
  va_end(ap);
}

static const char *level_str(DiagLevel level) {
  switch (level) {
    case DIAG_ERROR:
      return "error";
    case DIAG_WARN:
      return "warning";
    case DIAG_INFO:
      return "info";
    default:
      return "note";
  }
}

void diag_print_all(const DiagList *d) {
  if (!d) return;
  for (size_t i = 0; i < d->len; i++) {
    const Diagnostic *it = &d->items[i];
    const char *file = it->file && it->file[0] ? it->file : "<input>";
    fprintf(stderr, "%s:%d:%d: %s: %s\n", file, it->line, it->col,
            level_str(it->level), it->message ? it->message : "");
    if (it->hint && it->hint[0])
      fprintf(stderr, "hint: %s\n", it->hint);
  }
}

typedef struct {
  char *buf;
  size_t len, cap;
} DiagSb;

static int diagsb_grow(DiagSb *sb, size_t extra) {
  if (sb->len + extra + 1 <= sb->cap) return 1;
  size_t ncap = sb->cap ? sb->cap * 2 : 256;
  while (ncap < sb->len + extra + 1) ncap *= 2;
  char *n = realloc(sb->buf, ncap);
  if (!n) return 0;
  sb->buf = n;
  sb->cap = ncap;
  return 1;
}

static void diagsb_add(DiagSb *sb, const char *s) {
  if (!sb || !s) return;
  size_t n = strlen(s);
  if (!diagsb_grow(sb, n)) return;
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static void diagsb_addc(DiagSb *sb, char c) {
  if (!sb || !diagsb_grow(sb, 1)) return;
  sb->buf[sb->len++] = c;
  sb->buf[sb->len] = '\0';
}

static void diagsb_addf(DiagSb *sb, const char *fmt, ...) {
  if (!sb || !fmt) return;
  va_list ap;
  va_start(ap, fmt);
  char tmp[512];
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n < sizeof(tmp)) {
    diagsb_add(sb, tmp);
    return;
  }
  char *big = malloc((size_t)n + 1);
  if (!big) return;
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  diagsb_add(sb, big);
  free(big);
}

static void json_escape_sb(DiagSb *sb, const char *s) {
  if (!s) return;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    switch (*p) {
      case '"':
        diagsb_add(sb, "\\\"");
        break;
      case '\\':
        diagsb_add(sb, "\\\\");
        break;
      case '\n':
        diagsb_add(sb, "\\n");
        break;
      case '\r':
        diagsb_add(sb, "\\r");
        break;
      case '\t':
        diagsb_add(sb, "\\t");
        break;
      default:
        if (*p < 0x20)
          diagsb_addf(sb, "\\u%04x", *p);
        else
          diagsb_addc(sb, (char)*p);
        break;
    }
  }
}

static void diag_append_json_array(DiagSb *sb, const DiagList *d) {
  diagsb_addc(sb, '[');
  if (!d) {
    diagsb_addc(sb, ']');
    return;
  }
  for (size_t i = 0; i < d->len; i++) {
    const Diagnostic *it = &d->items[i];
    if (i) diagsb_addc(sb, ',');
    diagsb_add(sb, "{\"level\":\"");
    diagsb_add(sb, level_str(it->level));
    diagsb_add(sb, "\",\"file\":\"");
    json_escape_sb(sb, it->file && it->file[0] ? it->file : "<input>");
    diagsb_addf(sb, "\",\"line\":%d,\"col\":%d,\"message\":\"", it->line,
                it->col);
    json_escape_sb(sb, it->message ? it->message : "");
    diagsb_addc(sb, '"');
    if (it->code && it->code[0]) {
      diagsb_add(sb, ",\"code\":\"");
      json_escape_sb(sb, it->code);
      diagsb_addc(sb, '"');
    }
    if (it->hint && it->hint[0]) {
      diagsb_add(sb, ",\"hint\":\"");
      json_escape_sb(sb, it->hint);
      diagsb_addc(sb, '"');
    }
    diagsb_addc(sb, '}');
  }
  diagsb_addc(sb, ']');
}

char *diag_format_json(const DiagList *d) {
  DiagSb sb = {0};
  if (!diagsb_grow(&sb, 64)) return strdup("[]");
  sb.buf[0] = '\0';
  diag_append_json_array(&sb, d);
  if (!sb.buf) return strdup("[]");
  return sb.buf;
}

void diag_print_json(const DiagList *d) {
  char *json = diag_format_json(d);
  if (json) {
    fputs(json, stdout);
    free(json);
  } else {
    fputs("[]", stdout);
  }
}

int diag_error_count(const DiagList *d) {
  return diag_count_level(d, DIAG_ERROR);
}

int diag_count_level(const DiagList *d, DiagLevel level) {
  if (!d) return 0;
  int n = 0;
  for (size_t i = 0; i < d->len; i++) {
    if (d->items[i].level == level) n++;
  }
  return n;
}
