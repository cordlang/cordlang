#include "adapters/outbound/compiler/compiler_internal.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CompileResult compiler_parse_source(const char *source, size_t source_len) {
  CompileResult result = {0};
  Lexer *lexer = lexer_create(source, source_len);
  Parser *parser = parser_create(lexer);
  AST *ast = parser_parse(parser);

  if (parser->had_error) {
    result.ok = 0;
    result.error = parser->error_msg ? parser->error_msg : "parse error";
    result.error_line = parser->error_line;
    result.error_col = parser->error_col;
    ast_free(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return result;
  }

  result.ok = 1;
  result.ast = ast;
  parser_destroy(parser);
  lexer_destroy(lexer);
  return result;
}

CompileResult compiler_parse_file(const char *path) {
  CompileResult result = {0};
  size_t len = 0;
  char *source = fs_read_file(path, &len);
  if (!source) {
    result.ok = 0;
    result.error = "cannot read file";
    return result;
  }
  result = compiler_parse_source(source, len);
  if (result.ok && result.ast) {
    result.ast->source = source;
  } else {
    free(source);
  }
  return result;
}

void compiler_result_free(CompileResult *result) {
  if (!result) return;
  if (result->ast) {
    ast_free(result->ast);
    result->ast = NULL;
  }
}
