#ifndef CORDLANG_DOMAIN_EXPR_H
#define CORDLANG_DOMAIN_EXPR_H

#include <stddef.h>

/*
 * Lightweight expression mini-parser (Phase C2).
 * Accepts simple JS-like expressions and normalizes them for backends.
 *
 * Supported: identifiers, numbers, strings, a.b, a+b, f(x), !a, (a), a && b, a || b
 */

typedef struct {
  int ok;
  char *error; /* heap string; free when non-NULL */
  int line;
} ExprResult;

/* Returns a normalized JS-like string, or NULL on parse failure. Caller frees. */
char *expr_normalize(const char *src);

/* Returns 1 if valid, 0 if not. Writes a short message into err when invalid. */
int expr_validate(const char *src, char *err, size_t errlen);

#endif
