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

static void json_escape_f(FILE *f, const char *s) {
  if (!s) {
    fputs("", f);
    return;
  }
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    switch (*p) {
      case '"':
        fputs("\\\"", f);
        break;
      case '\\':
        fputs("\\\\", f);
        break;
      case '\n':
        fputs("\\n", f);
        break;
      case '\r':
        fputs("\\r", f);
        break;
      case '\t':
        fputs("\\t", f);
        break;
      default:
        if (*p < 0x20)
          fprintf(f, "\\u%04x", *p);
        else
          fputc(*p, f);
        break;
    }
  }
}

void diag_print_json(const DiagList *d) {
  fputc('[', stdout);
  if (!d) {
    fputs("]", stdout);
    return;
  }
  for (size_t i = 0; i < d->len; i++) {
    const Diagnostic *it = &d->items[i];
    if (i) fputc(',', stdout);
    fputs("{\"level\":\"", stdout);
    fputs(level_str(it->level), stdout);
    fputs("\",\"file\":\"", stdout);
    json_escape_f(stdout, it->file && it->file[0] ? it->file : "<input>");
    fprintf(stdout, "\",\"line\":%d,\"col\":%d,\"message\":\"", it->line,
            it->col);
    json_escape_f(stdout, it->message ? it->message : "");
    fputc('"', stdout);
    if (it->code && it->code[0]) {
      fputs(",\"code\":\"", stdout);
      json_escape_f(stdout, it->code);
      fputc('"', stdout);
    }
    if (it->hint && it->hint[0]) {
      fputs(",\"hint\":\"", stdout);
      json_escape_f(stdout, it->hint);
      fputc('"', stdout);
    }
    fputc('}', stdout);
  }
  fputs("]", stdout);
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
