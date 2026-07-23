#ifndef CORDLANG_DOMAIN_DIAG_H
#define CORDLANG_DOMAIN_DIAG_H

#include <stddef.h>

typedef enum { DIAG_ERROR, DIAG_WARN, DIAG_INFO } DiagLevel;

typedef struct {
  DiagLevel level;
  char *file;
  int line, col;
  char *message;
} Diagnostic;

typedef struct {
  Diagnostic *items;
  size_t len, cap;
} DiagList;

void diag_list_init(DiagList *d);
void diag_list_free(DiagList *d);
void diag_emit(DiagList *d, DiagLevel level, const char *file, int line, int col,
               const char *fmt, ...);
void diag_print_all(const DiagList *d); /* file:line:col: error: msg */
int diag_error_count(const DiagList *d);

#endif
