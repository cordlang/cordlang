#include "adapters/outbound/lexer/lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

Lexer *lexer_create(const char *source, size_t source_len) {
  Lexer *lexer = calloc(1, sizeof(Lexer));
  lexer->source = source;
  lexer->source_len = source_len;
  lexer->pos = 0;
  lexer->line = 1;
  lexer->col = 1;
  lexer->token_pos = 0;
  lexer->tokens_cap = 4096;
  lexer->tokens = calloc(lexer->tokens_cap, sizeof(Token));
  lexer->indent_cap = 64;
  lexer->indent_stack = calloc(lexer->indent_cap, sizeof(int));
  lexer->indent_stack[lexer->indent_len++] = 0;
  return lexer;
}

void lexer_destroy(Lexer *lexer) {
  if (lexer) {
    free(lexer->tokens);
    free(lexer->indent_stack);
    free(lexer);
  }
}

static char peek(Lexer *lexer) {
  if (lexer->pos >= lexer->source_len) return '\0';
  return lexer->source[lexer->pos];
}

static char advance(Lexer *lexer) {
  char c = lexer->source[lexer->pos++];
  lexer->col++;
  if (c == '\n') {
    lexer->line++;
    lexer->col = 1;
  }
  return c;
}

static void skip_whitespace(Lexer *lexer) {
  while (peek(lexer) == ' ' || peek(lexer) == '\t' || peek(lexer) == '\r') {
    advance(lexer);
  }
}

static int current_indent(Lexer *lexer) {
  int indent = 0;
  size_t saved_pos = lexer->pos;
  int saved_line = lexer->line;
  int saved_col = lexer->col;

  while (peek(lexer) == ' ' || peek(lexer) == '\t') {
    if (peek(lexer) == ' ') indent++;
    else indent += 4;
    advance(lexer);
  }

  if (peek(lexer) == '\n' || peek(lexer) == '\r' || peek(lexer) == '\0' || peek(lexer) == '#') {
    lexer->pos = saved_pos;
    lexer->line = saved_line;
    lexer->col = saved_col;
    return -1;
  }

  lexer->pos = saved_pos;
  lexer->line = saved_line;
  lexer->col = saved_col;
  return indent;
}

static Token make_token(Lexer *lexer, TokenType type, const char *start, size_t len) {
  Token t = { type, start, len, lexer->line, lexer->col };
  return t;
}

static Token emit_token(Lexer *lexer, TokenType type, const char *start, size_t len) {
  if (lexer->tokens_len >= lexer->tokens_cap) {
    lexer->tokens_cap *= 2;
    lexer->tokens = realloc(lexer->tokens, lexer->tokens_cap * sizeof(Token));
  }
  Token t = make_token(lexer, type, start, len);
  lexer->tokens[lexer->tokens_len++] = t;
  return t;
}

static void lexer_fail(Lexer *lexer, const char *msg, int line, int col) {
  if (!lexer || lexer->had_error) return;
  lexer->had_error = 1;
  lexer->error_msg = msg ? msg : "lex error";
  lexer->error_line = line > 0 ? line : 1;
  lexer->error_col = col > 0 ? col : 1;
  fprintf(stderr, "Error at line %d: %s\n", lexer->error_line,
          lexer->error_msg);
}

static Token read_string(Lexer *lexer, int open_line, int open_col) {
  const char *start = lexer->source + lexer->pos;
  size_t len = 0;
  while (peek(lexer) && peek(lexer) != '"') {
    if (peek(lexer) == '\n') {
      lexer_fail(lexer, "unterminated string (missing closing quote)",
                 open_line, open_col);
      return make_token(lexer, TOKEN_ERROR, start, len);
    }
    if (peek(lexer) == '\\') {
      advance(lexer); /* backslash */
      len++;
      if (peek(lexer) && peek(lexer) != '\n') {
        advance(lexer); /* escaped char */
        len++;
      }
    } else {
      advance(lexer);
      len++;
    }
  }
  if (peek(lexer) != '"') {
    lexer_fail(lexer, "unterminated string (missing closing quote)", open_line,
               open_col);
    return make_token(lexer, TOKEN_ERROR, start, len);
  }
  advance(lexer);
  return make_token(lexer, TOKEN_STRING, start, len);
}

static Token read_number_or_id(Lexer *lexer) {
  const char *start = lexer->source + lexer->pos - 1;
  size_t len = 1;
  while (isdigit(peek(lexer)) || peek(lexer) == '.') {
    advance(lexer);
    len++;
  }
  if (isalpha(peek(lexer)) || peek(lexer) == '-') {
    while (isalnum(peek(lexer)) || peek(lexer) == '_' || peek(lexer) == '-') {
      advance(lexer);
      len++;
    }
    return make_token(lexer, TOKEN_IDENTIFIER, start, len);
  }
  return make_token(lexer, TOKEN_NUMBER, start, len);
}

static Token read_identifier(Lexer *lexer) {
  const char *start = lexer->source + lexer->pos - 1;
  size_t len = 1;
  while (isalnum(peek(lexer)) || peek(lexer) == '_' || peek(lexer) == '-' || peek(lexer) == '.') {
    if (peek(lexer) == '.') {
      advance(lexer);
      len++;
      if (isalpha(peek(lexer))) {
        while (isalnum(peek(lexer)) || peek(lexer) == '_') {
          advance(lexer);
          len++;
        }
      }
    } else {
      advance(lexer);
      len++;
    }
  }
  return make_token(lexer, TOKEN_IDENTIFIER, start, len);
}

void lexer_tokenize(Lexer *lexer) {
  int at_line_start = 1;

  while (lexer->pos < lexer->source_len) {
    char c = peek(lexer);

    if (at_line_start) {
      int indent = current_indent(lexer);
      if (indent >= 0) {
        int current = lexer->indent_stack[lexer->indent_len - 1];
        if (indent > current) {
          lexer->indent_stack[lexer->indent_len++] = indent;
          emit_token(lexer, TOKEN_INDENT, NULL, 0);
        } else if (indent < current) {
          while (lexer->indent_len > 1 && indent < lexer->indent_stack[lexer->indent_len - 1]) {
            lexer->indent_len--;
            emit_token(lexer, TOKEN_DEDENT, NULL, 0);
          }
        }
        at_line_start = 0;
        continue;
      }
    }

    c = peek(lexer);

    if (c == '\n') {
      advance(lexer);
      emit_token(lexer, TOKEN_NEWLINE, NULL, 0);
      at_line_start = 1;
      continue;
    }

    if (c == ' ' || c == '\t' || c == '\r') {
      skip_whitespace(lexer);
      continue;
    }

    /* Comments: # ... or // ... */
    if (c == '#') {
      while (peek(lexer) && peek(lexer) != '\n') advance(lexer);
      continue;
    }
    if (c == '/' && lexer->pos + 1 < lexer->source_len &&
        lexer->source[lexer->pos + 1] == '/') {
      while (peek(lexer) && peek(lexer) != '\n') advance(lexer);
      continue;
    }

    at_line_start = 0;

    if (c == '"') {
      advance(lexer);
      Token t = read_string(lexer);
      emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (c == '@') {
      advance(lexer);
      emit_token(lexer, TOKEN_AT, NULL, 0);
      continue;
    }

    if (c == '(') {
      advance(lexer);
      emit_token(lexer, TOKEN_LPAREN, NULL, 0);
      continue;
    }

    if (c == ')') {
      advance(lexer);
      emit_token(lexer, TOKEN_RPAREN, NULL, 0);
      continue;
    }

    if (c == '=') {
      advance(lexer);
      if (peek(lexer) == '>') {
        advance(lexer);
        emit_token(lexer, TOKEN_ARROW, NULL, 0);
      } else {
        emit_token(lexer, TOKEN_EQUALS, NULL, 0);
      }
      continue;
    }

    if (c == '-') {
      advance(lexer);
      if (peek(lexer) == '>') {
        advance(lexer);
        emit_token(lexer, TOKEN_MINUS, NULL, 0);
      } else {
        size_t pos = lexer->pos - 1;
        emit_token(lexer, TOKEN_IDENTIFIER, lexer->source + pos, 1);
      }
      continue;
    }

    if (c == ',') { advance(lexer); emit_token(lexer, TOKEN_COMMA, NULL, 0); continue; }
    if (c == ':') { advance(lexer); emit_token(lexer, TOKEN_COLON, NULL, 0); continue; }
    if (c == '.') { advance(lexer); emit_token(lexer, TOKEN_DOT, NULL, 0); continue; }
    if (c == '|') { advance(lexer); if (peek(lexer) == '>') { advance(lexer); } emit_token(lexer, TOKEN_PIPE, NULL, 0); continue; }

    /* URL/path tokens: /products/:id  and /guia#forma (used by route and link to=).
     * '#' starts comments elsewhere, but inside a path token it is a URL fragment. */
    if (c == '/') {
      const char *start = lexer->source + lexer->pos;
      size_t len = 0;
      while (peek(lexer) &&
             (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '/' ||
              peek(lexer) == ':' || peek(lexer) == '-' || peek(lexer) == '_' ||
              peek(lexer) == '.' || peek(lexer) == '*' || peek(lexer) == '?' ||
              peek(lexer) == '#' || peek(lexer) == '%' || peek(lexer) == '=' ||
              peek(lexer) == '&')) {
        advance(lexer);
        len++;
      }
      emit_token(lexer, TOKEN_STRING, start, len);
      continue;
    }

    /* Binary / unary operators kept as identifiers for expression capture */
    if (c == '+' || c == '*' || c == '%' || c == '!' || c == '<' || c == '>' ||
        c == '&' || c == '?' || c == ';' || c == '[' || c == ']') {
      const char *start = lexer->source + lexer->pos;
      advance(lexer);
      /* multi-char: && || == != <= >= */
      if ((c == '&' || c == '|' || c == '=' || c == '!' || c == '<' || c == '>') &&
          peek(lexer) == c) {
        advance(lexer);
        emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else if ((c == '<' || c == '>') && peek(lexer) == '=') {
        advance(lexer);
        emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else if (c == '!' && peek(lexer) == '=') {
        advance(lexer);
        emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else {
        emit_token(lexer, TOKEN_IDENTIFIER, start, 1);
      }
      continue;
    }

    if (isdigit(c)) {
      advance(lexer);
      Token t = read_number_or_id(lexer);
      emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (isalpha(c) || c == '_') {
      advance(lexer);
      Token t = read_identifier(lexer);
      emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (c == '{') { advance(lexer); emit_token(lexer, TOKEN_HASH, NULL, 0); continue; }
    if (c == '}') { advance(lexer); emit_token(lexer, TOKEN_HASH, NULL, 0); continue; }
    if (c == '$') { advance(lexer); emit_token(lexer, TOKEN_DOLLAR, NULL, 0); continue; }

    advance(lexer);
  }

  while (lexer->indent_len > 1) {
    lexer->indent_len--;
    emit_token(lexer, TOKEN_DEDENT, NULL, 0);
  }

  emit_token(lexer, TOKEN_EOF, NULL, 0);
  lexer->token_pos = 0;
}

Token lexer_next(Lexer *lexer) {
  if (lexer->token_pos >= lexer->tokens_len) {
    Token t = { TOKEN_EOF, NULL, 0, 0, 0 };
    return t;
  }
  return lexer->tokens[lexer->token_pos++];
}

Token lexer_peek(Lexer *lexer) {
  if (lexer->token_pos >= lexer->tokens_len) {
    Token t = { TOKEN_EOF, NULL, 0, 0, 0 };
    return t;
  }
  return lexer->tokens[lexer->token_pos];
}
