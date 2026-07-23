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
  }
  free(d->items);
  d->items = NULL;
  d->len = 0;
  d->cap = 0;
}

void diag_emit(DiagList *d, DiagLevel level, const char *file, int line, int col,
               const char *fmt, ...) {
  if (!d || !fmt) return;

  if (d->len >= d->cap) {
    size_t ncap = d->cap ? d->cap * 2 : 16;
    Diagnostic *ni = realloc(d->items, ncap * sizeof(Diagnostic));
    if (!ni) return;
    d->items = ni;
    d->cap = ncap;
  }

  Diagnostic *item = &d->items[d->len];
  item->level = level;
  item->file = file ? strdup(file) : NULL;
  item->line = line;
  item->col = col;

  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  item->message = strdup(buf);

  d->len++;
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
  }
}

int diag_error_count(const DiagList *d) {
  if (!d) return 0;
  int n = 0;
  for (size_t i = 0; i < d->len; i++) {
    if (d->items[i].level == DIAG_ERROR) n++;
  }
  return n;
}
