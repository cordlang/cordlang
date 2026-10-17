#include "domain/expr_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void expr_buf_init(Buf *b) {
  b->cap = 64;
  b->len = 0;
  b->data = malloc(b->cap);
  if (b->data) b->data[0] = '\0';
}
static int buf_reserve(Buf *b, size_t need) {
  if (!b->data) return 0;
  if (b->len + need + 1 <= b->cap) return 1;
  size_t ncap = b->cap;
  while (b->len + need + 1 > ncap) ncap *= 2;
  char *nd = realloc(b->data, ncap);
  if (!nd) return 0;
  b->data = nd;
  b->cap = ncap;
  return 1;
}
void expr_buf_putc(Buf *b, char c) {
  if (!buf_reserve(b, 1)) return;
  b->data[b->len++] = c;
  b->data[b->len] = '\0';
}
void expr_buf_puts(Buf *b, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (!buf_reserve(b, n)) return;
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}
void expr_buf_free(Buf *b) {
  free(b->data);
  b->data = NULL;
  b->len = b->cap = 0;
}
char *expr_normalize(const char *src) {
  if (!src) return NULL;
  /* empty / whitespace-only → empty string (valid) */
  const char *p = src;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!*p) return strdup("");

  EParser P;
  memset(&P, 0, sizeof(P));
  expr_lexer_init(&P.lex, src);
  expr_buf_init(&P.out);
  if (!P.out.data) return NULL;

  expr_lexer_next(&P.lex);
  if (P.lex.failed) {
    expr_buf_free(&P.out);
    return NULL;
  }

  expr_parse(&P);
  if (P.failed || P.lex.failed) {
    expr_buf_free(&P.out);
    return NULL;
  }
  if (P.lex.type != ET_EOF) {
    expr_buf_free(&P.out);
    return NULL;
  }

  char *result = P.out.data;
  P.out.data = NULL; /* transfer ownership */
  expr_buf_free(&P.out);
  return result;
}
int expr_validate(const char *src, char *err, size_t errlen) {
  if (err && errlen) err[0] = '\0';
  if (!src) {
    if (err && errlen) snprintf(err, errlen, "null expression");
    return 0;
  }

  const char *p = src;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!*p) return 1; /* empty ok */

  EParser P;
  memset(&P, 0, sizeof(P));
  expr_lexer_init(&P.lex, src);
  expr_buf_init(&P.out);

  expr_lexer_next(&P.lex);
  if (P.lex.failed) {
    if (err && errlen) snprintf(err, errlen, "%s", P.lex.err);
    expr_buf_free(&P.out);
    return 0;
  }

  expr_parse(&P);
  if (P.failed || P.lex.failed) {
    if (err && errlen)
      snprintf(err, errlen, "%s", P.failed ? P.err : P.lex.err);
    expr_buf_free(&P.out);
    return 0;
  }
  if (P.lex.type != ET_EOF) {
    if (err && errlen) snprintf(err, errlen, "trailing tokens in expression");
    expr_buf_free(&P.out);
    return 0;
  }

  expr_buf_free(&P.out);
  return 1;
}
