#include "adapters/outbound/lexer/lexer_internal.h"

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
static char lexer_peek_char(Lexer *lexer) {
  if (lexer->pos >= lexer->source_len) return '\0';
  return lexer->source[lexer->pos];
}
static char lexer_advance_char(Lexer *lexer) {
  char c = lexer->source[lexer->pos++];
  lexer->col++;
  if (c == '\n') {
    lexer->line++;
    lexer->col = 1;
  }
  return c;
}
static void skip_whitespace(Lexer *lexer) {
  while (lexer_peek_char(lexer) == ' ' || lexer_peek_char(lexer) == '\t' || lexer_peek_char(lexer) == '\r') {
    lexer_advance_char(lexer);
  }
}
static int current_indent(Lexer *lexer) {
  int indent = 0;
  size_t saved_pos = lexer->pos;
  int saved_line = lexer->line;
  int saved_col = lexer->col;

  while (lexer_peek_char(lexer) == ' ' || lexer_peek_char(lexer) == '\t') {
    if (lexer_peek_char(lexer) == ' ') indent++;
    else indent += 4;
    lexer_advance_char(lexer);
  }

  if (lexer_peek_char(lexer) == '\n' || lexer_peek_char(lexer) == '\r' || lexer_peek_char(lexer) == '\0' || lexer_peek_char(lexer) == '#') {
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
void lexer_tokenize(Lexer *lexer) {
  int at_line_start = 1;

  while (lexer->pos < lexer->source_len) {
    char c = lexer_peek_char(lexer);

    if (at_line_start) {
      int indent = current_indent(lexer);
      if (indent >= 0) {
        int current = lexer->indent_stack[lexer->indent_len - 1];
        if (indent > current) {
          lexer->indent_stack[lexer->indent_len++] = indent;
          lexer_emit_token(lexer, TOKEN_INDENT, NULL, 0);
        } else if (indent < current) {
          while (lexer->indent_len > 1 && indent < lexer->indent_stack[lexer->indent_len - 1]) {
            lexer->indent_len--;
            lexer_emit_token(lexer, TOKEN_DEDENT, NULL, 0);
          }
        }
        at_line_start = 0;
        continue;
      }
    }

    c = lexer_peek_char(lexer);

    if (c == '\n') {
      lexer_advance_char(lexer);
      lexer_emit_token(lexer, TOKEN_NEWLINE, NULL, 0);
      at_line_start = 1;
      continue;
    }

    if (c == ' ' || c == '\t' || c == '\r') {
      skip_whitespace(lexer);
      continue;
    }

    /* Comments: # ... or // ... */
    if (c == '#') {
      while (lexer_peek_char(lexer) && lexer_peek_char(lexer) != '\n') lexer_advance_char(lexer);
      continue;
    }
    if (c == '/' && lexer->pos + 1 < lexer->source_len &&
        lexer->source[lexer->pos + 1] == '/') {
      while (lexer_peek_char(lexer) && lexer_peek_char(lexer) != '\n') lexer_advance_char(lexer);
      continue;
    }

    at_line_start = 0;

    if (c == '"') {
      int open_line = lexer->line;
      int open_col = lexer->col;
      lexer_advance_char(lexer);
      Token t = lexer_read_string(lexer, open_line, open_col);
      lexer_emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (c == '@') {
      lexer_advance_char(lexer);
      lexer_emit_token(lexer, TOKEN_AT, NULL, 0);
      continue;
    }

    if (c == '(') {
      lexer_advance_char(lexer);
      lexer_emit_token(lexer, TOKEN_LPAREN, NULL, 0);
      continue;
    }

    if (c == ')') {
      lexer_advance_char(lexer);
      lexer_emit_token(lexer, TOKEN_RPAREN, NULL, 0);
      continue;
    }

    if (c == '=') {
      lexer_advance_char(lexer);
      if (lexer_peek_char(lexer) == '>') {
        lexer_advance_char(lexer);
        lexer_emit_token(lexer, TOKEN_ARROW, NULL, 0);
      } else {
        lexer_emit_token(lexer, TOKEN_EQUALS, NULL, 0);
      }
      continue;
    }

    if (c == '-') {
      lexer_advance_char(lexer);
      if (lexer_peek_char(lexer) == '>') {
        lexer_advance_char(lexer);
        lexer_emit_token(lexer, TOKEN_MINUS, NULL, 0);
      } else {
        size_t pos = lexer->pos - 1;
        lexer_emit_token(lexer, TOKEN_IDENTIFIER, lexer->source + pos, 1);
      }
      continue;
    }

    if (c == ',') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_COMMA, NULL, 0); continue; }
    if (c == ':') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_COLON, NULL, 0); continue; }
    if (c == '.') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_DOT, NULL, 0); continue; }
    if (c == '|') { lexer_advance_char(lexer); if (lexer_peek_char(lexer) == '>') { lexer_advance_char(lexer); } lexer_emit_token(lexer, TOKEN_PIPE, NULL, 0); continue; }

    /* URL/path tokens: /products/:id  and /guia#forma (used by route and link to=).
     * '#' starts comments elsewhere, but inside a path token it is a URL fragment. */
    if (c == '/') {
      const char *start = lexer->source + lexer->pos;
      size_t len = 0;
      while (lexer_peek_char(lexer) &&
             (isalnum((unsigned char)lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '/' ||
              lexer_peek_char(lexer) == ':' || lexer_peek_char(lexer) == '-' || lexer_peek_char(lexer) == '_' ||
              lexer_peek_char(lexer) == '.' || lexer_peek_char(lexer) == '*' || lexer_peek_char(lexer) == '?' ||
              lexer_peek_char(lexer) == '#' || lexer_peek_char(lexer) == '%' || lexer_peek_char(lexer) == '=' ||
              lexer_peek_char(lexer) == '&')) {
        lexer_advance_char(lexer);
        len++;
      }
      lexer_emit_token(lexer, TOKEN_STRING, start, len);
      continue;
    }

    /* Binary / unary operators kept as identifiers for expression capture */
    if (c == '+' || c == '*' || c == '%' || c == '!' || c == '<' || c == '>' ||
        c == '&' || c == '?' || c == ';' || c == '[' || c == ']') {
      const char *start = lexer->source + lexer->pos;
      lexer_advance_char(lexer);
      /* multi-char: && || == != <= >= */
      if ((c == '&' || c == '|' || c == '=' || c == '!' || c == '<' || c == '>') &&
          lexer_peek_char(lexer) == c) {
        lexer_advance_char(lexer);
        lexer_emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else if ((c == '<' || c == '>') && lexer_peek_char(lexer) == '=') {
        lexer_advance_char(lexer);
        lexer_emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else if (c == '!' && lexer_peek_char(lexer) == '=') {
        lexer_advance_char(lexer);
        lexer_emit_token(lexer, TOKEN_IDENTIFIER, start, 2);
      } else {
        lexer_emit_token(lexer, TOKEN_IDENTIFIER, start, 1);
      }
      continue;
    }

    if (isdigit(c)) {
      lexer_advance_char(lexer);
      Token t = lexer_read_number_or_id(lexer);
      lexer_emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (isalpha(c) || c == '_') {
      lexer_advance_char(lexer);
      Token t = lexer_read_identifier(lexer);
      lexer_emit_token(lexer, t.type, t.start, t.len);
      continue;
    }

    if (c == '{') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_HASH, NULL, 0); continue; }
    if (c == '}') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_HASH, NULL, 0); continue; }
    if (c == '$') { lexer_advance_char(lexer); lexer_emit_token(lexer, TOKEN_DOLLAR, NULL, 0); continue; }

    lexer_advance_char(lexer);
  }

  while (lexer->indent_len > 1) {
    lexer->indent_len--;
    lexer_emit_token(lexer, TOKEN_DEDENT, NULL, 0);
  }

  lexer_emit_token(lexer, TOKEN_EOF, NULL, 0);
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
