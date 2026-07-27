#ifndef CORDLANG_DOMAIN_DIAG_H
#define CORDLANG_DOMAIN_DIAG_H

#include <stddef.h>

typedef enum { DIAG_ERROR, DIAG_WARN, DIAG_INFO } DiagLevel;

typedef struct {
  DiagLevel level;
  char *file;
  int line, col;
  char *message;
  char *code; /* optional machine code, e.g. "jsx-attr" */
  char *hint; /* optional fix hint for agents / humans */
} Diagnostic;

typedef struct {
  Diagnostic *items;
  size_t len, cap;
} DiagList;

void diag_list_init(DiagList *d);
void diag_list_free(DiagList *d);
void diag_emit(DiagList *d, DiagLevel level, const char *file, int line, int col,
               const char *fmt, ...);
void diag_emit_ex(DiagList *d, DiagLevel level, const char *file, int line,
                  int col, const char *code, const char *hint, const char *fmt,
                  ...);
void diag_print_all(const DiagList *d); /* file:line:col: error: msg (+ hint) */
void diag_print_json(const DiagList *d); /* stdout JSON array */
/* Heap JSON array matching diag_print_json. Caller frees. Never NULL. */
char *diag_format_json(const DiagList *d);
int diag_error_count(const DiagList *d);
int diag_count_level(const DiagList *d, DiagLevel level);

#endif
