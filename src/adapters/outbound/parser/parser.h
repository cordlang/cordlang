#ifndef CORDLANG_PARSER_H
#define CORDLANG_PARSER_H

#include "domain/ast.h"
#include "adapters/outbound/lexer/lexer.h"

typedef struct {
  Lexer *lexer;
  Token current;
  AST *ast;
  int had_error;
  const char *error_msg;
} Parser;

Parser *parser_create(Lexer *lexer);
void parser_destroy(Parser *parser);
AST *parser_parse(Parser *parser);

#endif
