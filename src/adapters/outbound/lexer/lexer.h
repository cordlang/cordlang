#ifndef CORDLANG_LEXER_H
#define CORDLANG_LEXER_H

#include <stddef.h>

typedef enum {
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,
  TOKEN_AT,
  TOKEN_EQUALS,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_PIPE,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_DOT,
  TOKEN_ARROW,
  TOKEN_MINUS,
  TOKEN_INDENT,
  TOKEN_DEDENT,
  TOKEN_NEWLINE,
  TOKEN_EOF,
  TOKEN_ERROR,
  TOKEN_HASH,
  TOKEN_DOLLAR,
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  size_t len;
  int line;
  int col;
} Token;

typedef struct {
  const char *source;
  size_t source_len;
  size_t pos;
  int line;
  int col;
  Token *tokens;
  size_t tokens_cap;
  size_t tokens_len;
  size_t token_pos;
  int *indent_stack;
  size_t indent_cap;
  size_t indent_len;
  int had_error;
  const char *error_msg; /* literal; not freed */
  int error_line;
  int error_col;
} Lexer;

Lexer *lexer_create(const char *source, size_t source_len);
void lexer_destroy(Lexer *lexer);
Token lexer_next(Lexer *lexer);
Token lexer_peek(Lexer *lexer);
void lexer_tokenize(Lexer *lexer);

#endif
