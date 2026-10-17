#!/usr/bin/env bash
# Build Cordlang WASM for the browser playground (Docker + emscripten/emsdk).
# Usage (repo root):
#   bash playground/build_wasm.sh
#   make wasm
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT_DIR="$ROOT/playground"
mkdir -p "$OUT_DIR"

SRCS=(
  src/adapters/inbound/wasm_api.c
  src/adapters/inbound/wasm_project.c
  src/adapters/inbound/wasm_result.c
  src/domain/ast.c
  src/domain/diag.c
  src/domain/interp.c
  src/domain/interp_js.c
  src/domain/interp_html.c
  src/domain/ir.c
  src/domain/ir_convert.c
  src/domain/ir_convert_declarations.c
  src/domain/ir_node.c
  src/domain/ir_walk.c
  src/domain/ir_dump.c
  src/domain/ir_pass.c
  src/domain/expr.c
  src/domain/expr_lexer.c
  src/domain/expr_parser.c
  src/application/compile_service.c
  src/application/check_service.c
  src/application/check_collect.c
  src/application/check_diagnostics.c
  src/adapters/outbound/fs/fs.c
  src/adapters/outbound/fs/fs_memory.c
  src/adapters/outbound/fs/fs_path.c
  src/adapters/outbound/fs/fs_copy.c
  src/adapters/outbound/fs/fs_walk.c
  src/adapters/outbound/process/process_spawn.c
  src/adapters/outbound/json/json_mini.c
  src/adapters/outbound/html_escape.c
  src/adapters/outbound/lexer/lexer.c
  src/adapters/outbound/lexer/lexer_tokens.c
  src/adapters/outbound/parser/parser.c
  src/adapters/outbound/parser/parser_attrs.c
  src/adapters/outbound/parser/parser_body.c
  src/adapters/outbound/parser/parser_content.c
  src/adapters/outbound/parser/parser_control.c
  src/adapters/outbound/parser/parser_data.c
  src/adapters/outbound/parser/parser_declarations.c
  src/adapters/outbound/parser/parser_events.c
  src/adapters/outbound/parser/parser_modules.c
  src/adapters/outbound/parser/parser_react.c
  src/adapters/outbound/parser/parser_react_boundaries.c
  src/adapters/outbound/parser/parser_react_declarations.c
  src/adapters/outbound/parser/parser_react_effects.c
  src/adapters/outbound/parser/parser_react_resources.c
  src/adapters/outbound/parser/parser_statement.c
  src/adapters/outbound/parser/parser_style_attrs.c
  src/adapters/outbound/compiler/compiler.c
  src/adapters/outbound/compiler/compiler_paths.c
  src/adapters/outbound/compiler/compiler_modules.c
  src/adapters/outbound/compiler/compiler_project.c
  src/adapters/outbound/backends/registry.c
  src/adapters/outbound/backends/preset_registry.c
  src/adapters/outbound/backends/preset_packages.c
  src/adapters/outbound/backends/preset_bridges.c
  src/adapters/outbound/backends/source_attr.c
  src/adapters/outbound/backends/source_attr_file.c
  src/adapters/outbound/backends/cord_class.c
  src/adapters/outbound/backends/cord_class_emit.c
  src/adapters/outbound/backends/theme_css.c
  src/adapters/outbound/backends/theme_css_core.c
  src/adapters/outbound/backends/theme_css_shared.c
  src/adapters/outbound/backends/theme_css_ir.c
  src/adapters/outbound/fonts/font_cache.c
  src/adapters/outbound/fonts/font_download.c
  src/adapters/outbound/backends/react/react_backend.c
  src/adapters/outbound/backends/react/react_ir.c
  src/adapters/outbound/backends/react/react_ir_support.c
  src/adapters/outbound/backends/react/react_ir_project_scan.c
  src/adapters/outbound/backends/react/react_ir_project_units.c
  src/adapters/outbound/backends/react/react_ir_element_attrs.c
  src/adapters/outbound/backends/react/react_ir_element.c
  src/adapters/outbound/backends/react/react_ir_nodes.c
  src/adapters/outbound/backends/react/react_ir_component.c
  src/adapters/outbound/backends/react/react_ir_component_derived.c
  src/adapters/outbound/backends/react/react_ir_module.c
  src/adapters/outbound/backends/react/react_ir_app.c
  src/adapters/outbound/backends/svelte/svelte_backend.c
  src/adapters/outbound/backends/svelte/svelte_backend_support.c
  src/adapters/outbound/backends/svelte/svelte_backend_directives.c
  src/adapters/outbound/backends/svelte/svelte_backend_classes.c
  src/adapters/outbound/backends/svelte/svelte_backend_project_scan.c
  src/adapters/outbound/backends/svelte/svelte_backend_project_units.c
  src/adapters/outbound/backends/svelte/svelte_backend_element.c
  src/adapters/outbound/backends/svelte/svelte_backend_children.c
  src/adapters/outbound/backends/svelte/svelte_backend_nodes.c
  src/adapters/outbound/backends/svelte/svelte_backend_component_helpers.c
  src/adapters/outbound/backends/svelte/svelte_backend_module_imports.c
  src/adapters/outbound/backends/svelte/svelte_backend_module_props.c
  src/adapters/outbound/backends/svelte/svelte_backend_module_state.c
  src/adapters/outbound/backends/svelte/svelte_backend_module_context.c
  src/adapters/outbound/backends/svelte/svelte_backend_app.c
  src/adapters/outbound/backends/vue/vue_backend.c
  src/adapters/outbound/backends/vue/vue_ir.c
  src/adapters/outbound/backends/vue/vue_ir_support.c
  src/adapters/outbound/backends/vue/vue_ir_directives.c
  src/adapters/outbound/backends/vue/vue_ir_classes.c
  src/adapters/outbound/backends/vue/vue_ir_project_scan.c
  src/adapters/outbound/backends/vue/vue_ir_project_units.c
  src/adapters/outbound/backends/vue/vue_ir_element.c
  src/adapters/outbound/backends/vue/vue_ir_children.c
  src/adapters/outbound/backends/vue/vue_ir_nodes.c
  src/adapters/outbound/backends/vue/vue_ir_component_helpers.c
  src/adapters/outbound/backends/vue/vue_ir_module_imports.c
  src/adapters/outbound/backends/vue/vue_ir_module_props.c
  src/adapters/outbound/backends/vue/vue_ir_module_state.c
  src/adapters/outbound/backends/vue/vue_ir_module_context.c
  src/adapters/outbound/backends/vue/vue_ir_app.c
  src/adapters/outbound/backends/static_html/static_html.c
  src/adapters/outbound/backends/static_html/static_html_buffer.c
  src/adapters/outbound/backends/static_html/static_html_bindings.c
  src/adapters/outbound/backends/static_html/static_html_warnings.c
  src/adapters/outbound/backends/static_html/static_html_attrs.c
  src/adapters/outbound/backends/static_html/static_html_elements.c
  src/adapters/outbound/backends/static_html/static_html_render.c
  src/adapters/outbound/backends/email/email_backend.c
  src/adapters/outbound/backends/html/html_backend.c
  src/adapters/outbound/backends/html/html_backend_buffer.c
  src/adapters/outbound/backends/html/html_backend_styles.c
  src/adapters/outbound/backends/html/html_backend_state.c
  src/adapters/outbound/backends/html/html_backend_live_text.c
  src/adapters/outbound/backends/html/html_backend_ast_classes.c
  src/adapters/outbound/backends/html/html_backend_ast_element.c
  src/adapters/outbound/backends/html/html_backend_ast_render.c
  src/adapters/outbound/backends/html/html_backend_ir_classes.c
  src/adapters/outbound/backends/html/html_backend_ir_element.c
  src/adapters/outbound/backends/html/html_backend_ir_render.c
  src/adapters/outbound/backends/html/html_backend_scaffold.c
  src/adapters/outbound/backends/html/html_backend_runtime.c
  src/adapters/outbound/backends/esm/esm_ir.c
  src/adapters/outbound/backends/esm/esm_ir_support.c
  src/adapters/outbound/backends/esm/esm_ir_imports.c
  src/adapters/outbound/backends/esm/esm_ir_project.c
  src/adapters/outbound/backends/esm/esm_ir_props.c
  src/adapters/outbound/backends/esm/esm_ir_element.c
  src/adapters/outbound/backends/esm/esm_ir_nodes.c
  src/adapters/outbound/backends/esm/esm_ir_component.c
  src/adapters/outbound/backends/esm/esm_ir_routes.c
  src/adapters/outbound/backends/esm/esm_ir_generate.c
)

EMCC_FLAGS=(
  -O2
  -std=c17
  -Wall
  -Wno-unused-parameter
  -Wno-unused-function
  -D_DEFAULT_SOURCE
  -DCORDLANG_WASM=1
  -Isrc
  -sWASM=1
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createCordlang
  -sEXPORTED_FUNCTIONS=_cordlang_compile,_cordlang_compile_project,_cordlang_free,_cordlang_version,_malloc,_free
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=262144
  -sENVIRONMENT=web,worker
  -sERROR_ON_UNDEFINED_SYMBOLS=1
)

run_emcc() {
  local emcc="$1"
  shift
  echo "Building playground WASM with: $emcc"
  "$emcc" "${EMCC_FLAGS[@]}" "${SRCS[@]}" -o "$OUT_DIR/cordlang.js"
}

if command -v emcc >/dev/null 2>&1; then
  run_emcc emcc
elif command -v docker >/dev/null 2>&1; then
  echo "emcc not on PATH — using Docker image emscripten/emsdk"
  # Mount repo and run emcc inside the container
  # Link in the container filesystem: llvm-objcopy cannot reliably rewrite a
  # WASM file on every Windows Docker bind mount. Copy finished assets back.
  docker run --rm \
    -v "$ROOT:/src" \
    -w /src \
    emscripten/emsdk:3.1.74 \
    sh -lc 'emcc "$@" -o /tmp/cordlang.js && cp /tmp/cordlang.js /tmp/cordlang.wasm playground/' \
    -- "${EMCC_FLAGS[@]}" "${SRCS[@]}"
else
  echo "FAIL: need emcc on PATH or Docker (emscripten/emsdk)."
  exit 1
fi

ls -la "$OUT_DIR/cordlang.js" "$OUT_DIR/cordlang.wasm"
echo "OK: playground WASM ready. Open playground/index.html via a local HTTP server."
