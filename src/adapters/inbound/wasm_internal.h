#ifndef CORDLANG_WASM_INTERNAL_H
#define CORDLANG_WASM_INTERNAL_H

#include "application/ports/fs_port.h"
#include "domain/diag.h"
#include <stddef.h>

typedef struct {
  FsMemoryFile *items;
  size_t len;
  size_t cap;
} WasmProjectFiles;

#define WASM_PROJECT_MAX_FILES 128

char *wasm_build_result_json(int ok, const char *backend, const char *code,
                             const DiagList *diags, const char *parse_error,
                             int parse_line, int parse_col);
void wasm_project_files_free(WasmProjectFiles *project);
int wasm_parse_project_manifest(const char *manifest, WasmProjectFiles *project,
                                const char **error);

#endif
