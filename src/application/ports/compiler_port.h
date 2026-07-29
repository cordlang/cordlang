#ifndef CORDLANG_COMPILER_PORT_H
#define CORDLANG_COMPILER_PORT_H

#include "domain/ast.h"

/* Outbound port: source → AST */
typedef struct {
  AST *ast;
  int ok;
  const char *error;
  int error_line; /* 1-based when ok==0 and known; else 0 */
  int error_col;  /* 1-based when ok==0 and known; else 0 */
} CompileResult;

CompileResult compiler_parse_source(const char *source, size_t source_len);
CompileResult compiler_parse_file(const char *path);
/*
 * Parse entry .cord and resolve all `use` / route module paths into one AST
 * (components, pages, layouts merged at root).
 */
CompileResult compiler_parse_project(const char *entry_path);
void compiler_result_free(CompileResult *result);

/* ── single-module resolution (native ESM dev server) ─────
 *
 * compiler_parse_project() inlines every dependency into one AST. The ESM dev
 * server needs the opposite: parse ONE file and keep its `use` / route module
 * refs as edges, so each .cord maps to its own ES module URL. These expose the
 * resolver that compiler_parse_project uses internally.
 */

/* Directory holding cordlang.json, walking up from entry. Caller frees. */
char *compiler_project_root(const char *entry_path);

/*
 * Resolve a Cord module ref (`pages/HomePage`, `./Counter`, `layouts/docs`)
 * against from_file. Appends .cord when absent. Returns NULL when missing or
 * outside project_root (path jail). Caller frees.
 */
char *compiler_resolve_module(const char *from_file, const char *mod_path,
                              const char *project_root);

/* Exported component name for a module path (basename, PascalCase,
 * default → DefaultLayout). alias wins when non-empty. Caller frees. */
char *compiler_module_export_name(const char *mod_path, const char *alias);

/* True if the string looks like a module ref rather than a bare name. */
int compiler_is_module_ref(const char *s);

/*
 * Wrap a body-only module (no `def`) into a component named export_name, so a
 * standalone parse of pages/HomePage.cord yields a COMPONENT_DEF like the
 * project-wide parse does. No-op when the def already exists.
 */
void compiler_wrap_module_body(Node *root, const char *export_name,
                               const char *file_path);

#endif
