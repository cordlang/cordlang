#ifndef CORDLANG_DOMAIN_EXPR_INTERNAL_H
#define CORDLANG_DOMAIN_EXPR_INTERNAL_H

#include "domain/expr.h"
#include <stddef.h>

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} Buf;

typedef enum {
  ET_EOF,
  ET_IDENT,
  ET_NUMBER,
  ET_STRING,
  ET_DOT,
  ET_LPAREN,
  ET_RPAREN,
  ET_COMMA,
  ET_PLUS,
  ET_MINUS,
  ET_STAR,
  ET_SLASH,
  ET_PERCENT,
  ET_BANG,
  ET_EQEQ,
  ET_NEQ,
  ET_LT,
  ET_GT,
  ET_LE,
  ET_GE,
  ET_ANDAND,
  ET_OROR,
  ET_EQ,
  ET_QUESTION,
  ET_COLON,
  ET_LBRACKET,
  ET_RBRACKET,
  ET_ERROR
} ETok;

typedef struct {
  const char *src;
  size_t pos;
  size_t len;
  ETok type;
  const char *start;
  size_t tlen;
  char err[128];
  int failed;
} ELex;

typedef struct {
  ELex lex;
  Buf out;
  char err[128];
  int failed;
} EParser;

void expr_buf_init(Buf *b);
void expr_buf_putc(Buf *b, char c);
void expr_buf_puts(Buf *b, const char *s);
void expr_buf_free(Buf *b);
void expr_lexer_init(ELex *lexer, const char *src);
void expr_lexer_next(ELex *lexer);
void expr_parse(EParser *parser);

#endif
