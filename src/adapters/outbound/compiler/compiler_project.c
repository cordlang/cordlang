#include "adapters/outbound/compiler/compiler_internal.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CompileResult compiler_parse_project(const char *entry_path) {
  CompileResult result = {0};
  static char errbuf[512];
  errbuf[0] = '\0';

  if (!entry_path) {
    result.ok = 0;
    result.error = "no entry path";
    return result;
  }

  result = compiler_parse_file(entry_path);
  if (!result.ok || !result.ast) return result;

  char *project_root = find_project_root(entry_path);
  VisitedSet visited = {0};
  visited_add(&visited, entry_path);

  if (resolve_uses_in_ast(result.ast->root, entry_path, &visited, errbuf,
                          sizeof(errbuf), project_root) != 0) {
    compiler_result_free(&result);
    result.ok = 0;
    result.ast = NULL;
    result.error = errbuf[0] ? errbuf : "module resolution failed";
    visited_free(&visited);
    free(project_root);
    return result;
  }

  /* Tag any component defs that live in the entry file itself */
  tag_components_in_root(result.ast->root, entry_path);

  visited_free(&visited);
  free(project_root);
  return result;
}

char *compiler_project_root(const char *entry_path) {
  if (!entry_path) return NULL;
  return find_project_root(entry_path);
}

char *compiler_resolve_module(const char *from_file, const char *mod_path,
                              const char *project_root) {
  if (!from_file || !mod_path) return NULL;
  return resolve_module_file(from_file, mod_path, project_root);
}

char *compiler_module_export_name(const char *mod_path, const char *alias) {
  if (!mod_path && !(alias && *alias)) return NULL;
  return module_export_name(mod_path ? mod_path : "", alias);
}

int compiler_is_module_ref(const char *s) { return path_is_module_ref(s); }

void compiler_wrap_module_body(Node *root, const char *export_name,
                               const char *file_path) {
  ensure_component_wrapper(root, export_name, file_path);
  tag_components_in_root(root, file_path);
}
