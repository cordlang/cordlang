#include "adapters/inbound/wasm_internal.h"
#include "application/compile_service.h"
#include "application/check_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include "domain/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define CORD_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define CORD_WASM_EXPORT
#endif

CORD_WASM_EXPORT const char *cordlang_version(void) { return CORDLANG_VERSION; }
CORD_WASM_EXPORT void cordlang_free(char *p) { free(p); }
CORD_WASM_EXPORT char *cordlang_compile(const char *source, const char *backend) {
  if (!source) source = "";
  if (!backend || !*backend) backend = "react";

  backend_register_all();

  size_t source_len = strlen(source);
  CompileResult parsed = compiler_parse_source(source, source_len);
  if (!parsed.ok || !parsed.ast) {
    char *json = wasm_build_result_json(0, backend, NULL, NULL,
                                   parsed.error ? parsed.error : "parse error",
                                   parsed.error_line, parsed.error_col);
    compiler_result_free(&parsed);
    return json;
  }

  DiagList diags;
  diag_list_init(&diags);
  check_service_run_source("<playground>", source, source_len, &diags);
  int has_errors = diag_error_count(&diags) > 0;

  char *code = NULL;
  if (!has_errors) {
    code = compile_service_source(source, source_len, backend, "<playground>");
    if (!code) {
      diag_emit(&diags, DIAG_ERROR, "<playground>", 1, 1,
                "codegen failed for backend '%s'", backend);
      has_errors = 1;
    }
  }

  char *json = wasm_build_result_json(!has_errors && code != NULL, backend, code,
                                 &diags, NULL, 0, 0);
  free(code);
  diag_list_free(&diags);
  compiler_result_free(&parsed);
  return json;
}
CORD_WASM_EXPORT char *cordlang_compile_project(const char *entry_path,
                                                const char *files_json,
                                                const char *backend) {
  if (!backend || !*backend) backend = "react";
  if (!entry_path || entry_path[0] != '/')
    return wasm_build_result_json(0, backend, NULL, NULL,
                             "entry path must be an absolute virtual path", 0,
                             0);

  WasmProjectFiles project = {0};
  const char *manifest_error = NULL;
  if (!wasm_parse_project_manifest(files_json, &project, &manifest_error)) {
    wasm_project_files_free(&project);
    return wasm_build_result_json(0, backend, NULL, NULL,
                             manifest_error ? manifest_error
                                            : "invalid project manifest",
                             0, 0);
  }
  if (fs_memory_mount(project.items, project.len, "/") != 0) {
    wasm_project_files_free(&project);
    return wasm_build_result_json(0, backend, NULL, NULL,
                             "cannot mount virtual project filesystem", 0,
                             0);
  }
  wasm_project_files_free(&project);

  backend_register_all();
  DiagList diags;
  diag_list_init(&diags);
  int has_errors = check_service_run(entry_path, &diags) != 0;
  char *code = NULL;
  if (!has_errors) {
    code = compile_service_file_with_passes(entry_path, backend, 0, NULL, NULL,
                                            0);
    if (!code) {
      diag_emit(&diags, DIAG_ERROR, entry_path, 1, 1,
                "codegen failed for backend '%s'", backend);
      has_errors = 1;
    }
  }

  char *json = wasm_build_result_json(!has_errors && code != NULL, backend, code,
                                 &diags, NULL, 0, 0);
  free(code);
  diag_list_free(&diags);
  fs_memory_unmount();
  return json;
}
