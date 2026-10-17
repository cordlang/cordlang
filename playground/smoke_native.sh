#!/usr/bin/env bash
# Native smoke for wasm_api without Emscripten.
# Builds the same CORDLANG_WASM source set used by the browser bridge.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SRCS=(
  playground/smoke_native.c
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

OUT="$ROOT/playground/.smoke_native"
trap 'rm -f "$OUT"' EXIT

if ! command -v gcc >/dev/null 2>&1; then
  echo "FAIL: gcc not found" >&2
  exit 1
fi

echo "Building playground native smoke..."
gcc -O2 -std=c17 -Wall -Wno-unused-parameter -Wno-unused-function \
  -D_DEFAULT_SOURCE -DCORDLANG_WASM=1 -Isrc \
  "${SRCS[@]}" -o "$OUT"

"$OUT"

# UI contract: the browser playground must call the multi-file project API.
html="$ROOT/playground/index.html"
for needle in cordlang_compile_project /playground/src/app.cord; do
  if ! grep -F -q -- "$needle" "$html"; then
    echo "FAIL: playground/index.html missing $needle" >&2
    exit 1
  fi
done
echo "PASS: playground UI wires project API"
