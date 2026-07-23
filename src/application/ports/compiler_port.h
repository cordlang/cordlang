#ifndef CORDLANG_COMPILER_PORT_H
#define CORDLANG_COMPILER_PORT_H

#include "domain/ast.h"

/* Outbound port: source → AST */
typedef struct {
  AST *ast;
  int ok;
  const char *error;
} CompileResult;

CompileResult compiler_parse_source(const char *source, size_t source_len);
CompileResult compiler_parse_file(const char *path);
/*
 * Parse entry .cord and resolve all `use` / route module paths into one AST
 * (components, pages, layouts merged at root).
 */
CompileResult compiler_parse_project(const char *entry_path);
void compiler_result_free(CompileResult *result);

#endif
