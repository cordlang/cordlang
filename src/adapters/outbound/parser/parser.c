#include "adapters/outbound/parser/parser_internal.h"

Parser *parser_create(Lexer *lexer) {
  Parser *p = calloc(1, sizeof(Parser));
  p->lexer = lexer;
  p->ast = ast_create();
  return p;
}

void parser_destroy(Parser *parser) {
  if (parser) {
    if (parser->ast) ast_free(parser->ast);
    free(parser);
  }
}

Token parser_peek(Parser *p) {
  return lexer_peek(p->lexer);
}

Token parser_advance(Parser *p) {
  Token t = lexer_next(p->lexer);
  p->current = t;
  return t;
}

int parser_match(Parser *p, TokenType type) {
  if (parser_peek(p).type == type) {
    parser_advance(p);
    return 1;
  }
  return 0;
}

void parser_fail(Parser *p, const char *msg, int line, int col) {
  if (!p || p->had_error) return;
  p->had_error = 1;
  p->error_msg = msg;
  p->error_line = line > 0 ? line : 1;
  p->error_col = col > 0 ? col : 1;
}

int parser_consume(Parser *p, TokenType type, const char *msg) {
  if (parser_peek(p).type == type) {
    parser_advance(p);
    return 1;
  }
  Token t = parser_peek(p);
  parser_fail(p, msg, t.line, t.col);
  return 0;
}

char *parser_token_str(Token t) {
  /* Allow empty strings (len == 0) e.g. props label="" */
  if (!t.start) return NULL;
  char *s = malloc(t.len + 1);
  if (!s) return NULL;
  if (t.type == TOKEN_STRING) {
    /* Unescape \" \\ \n \t \r inside string token span.
     * Keep \#{…} escaped so interp can emit literal "#{…}". */
    size_t j = 0;
    for (size_t i = 0; i < t.len; i++) {
      if (t.start[i] == '\\' && i + 1 < t.len) {
        i++;
        char e = t.start[i];
        if (e == 'n')
          s[j++] = '\n';
        else if (e == 't')
          s[j++] = '\t';
        else if (e == 'r')
          s[j++] = '\r';
        else if (e == '#' && i + 1 < t.len && t.start[i + 1] == '{') {
          s[j++] = '\\';
          s[j++] = '#';
        } else
          s[j++] = e;
      } else {
        s[j++] = t.start[i];
      }
    }
    s[j] = '\0';
    return s;
  }
  if (t.len > 0) memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

AST *parser_parse(Parser *p) {
  lexer_tokenize(p->lexer);

  if (p->lexer && p->lexer->had_error) {
    parser_fail(p,
                p->lexer->error_msg ? p->lexer->error_msg : "lex error",
                p->lexer->error_line, p->lexer->error_col);
    AST *result = p->ast;
    p->ast = NULL;
    return result;
  }

  while (parser_peek(p).type != TOKEN_EOF) {
    if (parser_peek(p).type == TOKEN_ERROR) {
      Token t = parser_peek(p);
      parser_fail(p, "invalid token", t.line, t.col);
      break;
    }
    Node *stmt = parser_parse_stmt(p);
    if (stmt) {
      node_add_child(p->ast->root, stmt);
    } else {
      if (parser_peek(p).type != TOKEN_EOF) parser_advance(p);
    }
  }

  AST *result = p->ast;
  p->ast = NULL;
  return result;
}
