#include "adapters/outbound/lexer/lexer_internal.h"

Token lexer_make_token(Lexer *lexer, TokenType type, const char *start, size_t len) {
  Token t = { type, start, len, lexer->line, lexer->col };
  return t;
}
Token lexer_emit_token(Lexer *lexer, TokenType type, const char *start, size_t len) {
  if (lexer->tokens_len >= lexer->tokens_cap) {
    lexer->tokens_cap *= 2;
    lexer->tokens = realloc(lexer->tokens, lexer->tokens_cap * sizeof(Token));
  }
  Token t = lexer_make_token(lexer, type, start, len);
  lexer->tokens[lexer->tokens_len++] = t;
  return t;
}
void lexer_fail(Lexer *lexer, const char *msg, int line, int col) {
  if (!lexer || lexer->had_error) return;
  lexer->had_error = 1;
  lexer->error_msg = msg ? msg : "lex error";
  lexer->error_line = line > 0 ? line : 1;
  lexer->error_col = col > 0 ? col : 1;
}
Token lexer_read_string(Lexer *lexer, int open_line, int open_col) {
  const char *start = lexer->source + lexer->pos;
  size_t len = 0;
  while (lexer_peek_char(lexer) && lexer_peek_char(lexer) != '"') {
    if (lexer_peek_char(lexer) == '\n') {
      lexer_fail(lexer, "unterminated string (missing closing quote)",
                 open_line, open_col);
      return lexer_make_token(lexer, TOKEN_ERROR, start, len);
    }
    if (lexer_peek_char(lexer) == '\\') {
      lexer_advance_char(lexer); /* backslash */
      len++;
      if (lexer_peek_char(lexer) && lexer_peek_char(lexer) != '\n') {
        lexer_advance_char(lexer); /* escaped char */
        len++;
      }
    } else {
      lexer_advance_char(lexer);
      len++;
    }
  }
  if (lexer_peek_char(lexer) != '"') {
    lexer_fail(lexer, "unterminated string (missing closing quote)", open_line,
               open_col);
    return lexer_make_token(lexer, TOKEN_ERROR, start, len);
  }
  lexer_advance_char(lexer);
  return lexer_make_token(lexer, TOKEN_STRING, start, len);
}
Token lexer_read_number_or_id(Lexer *lexer) {
  const char *start = lexer->source + lexer->pos - 1;
  size_t len = 1;
  while (isdigit(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '.') {
    lexer_advance_char(lexer);
    len++;
  }
  if (isalpha(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '-') {
    while (isalnum(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '_' || lexer_peek_char(lexer) == '-') {
      lexer_advance_char(lexer);
      len++;
    }
    return lexer_make_token(lexer, TOKEN_IDENTIFIER, start, len);
  }
  return lexer_make_token(lexer, TOKEN_NUMBER, start, len);
}
Token lexer_read_identifier(Lexer *lexer) {
  const char *start = lexer->source + lexer->pos - 1;
  size_t len = 1;
  while (isalnum(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '_' || lexer_peek_char(lexer) == '-' || lexer_peek_char(lexer) == '.') {
    if (lexer_peek_char(lexer) == '.') {
      lexer_advance_char(lexer);
      len++;
      if (isalpha(lexer_peek_char(lexer))) {
        while (isalnum(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '_') {
          lexer_advance_char(lexer);
          len++;
        }
      }
    } else {
      lexer_advance_char(lexer);
      len++;
    }
  }
  return lexer_make_token(lexer, TOKEN_IDENTIFIER, start, len);
}
