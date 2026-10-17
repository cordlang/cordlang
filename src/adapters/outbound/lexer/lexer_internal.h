#ifndef CORDLANG_LEXER_INTERNAL_H
#define CORDLANG_LEXER_INTERNAL_H
#include <ctype.h>

#include "adapters/outbound/lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char lexer_peek_char(Lexer *lexer);
char lexer_advance_char(Lexer *lexer);
Token lexer_make_token(Lexer *lexer, TokenType type, const char *start, size_t len);
Token lexer_emit_token(Lexer *lexer, TokenType type, const char *start, size_t len);
void lexer_fail(Lexer *lexer, const char *msg, int line, int col);
Token lexer_read_string(Lexer *lexer, int open_line, int open_col);
Token lexer_read_number_or_id(Lexer *lexer);
Token lexer_read_identifier(Lexer *lexer);

#endif
