#!/usr/bin/env bash
# Native smoke for wasm_api without Emscripten.
# Builds the same CORDLANG_WASM source set used by the browser bridge.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SRCS=(
  playground/smoke_native.c
  src/adapters/inbound/wasm_api.c
  src/domain/ast.c
  src/domain/diag.c
  src/domain/interp.c
  src/domain/ir.c
  src/domain/ir_pass.c
  src/domain/expr.c
  src/application/compile_service.c
  src/application/check_service.c
  src/adapters/outbound/fs/fs.c
  src/adapters/outbound/process/process_spawn.c
  src/adapters/outbound/json/json_mini.c
  src/adapters/outbound/html_escape.c
  src/adapters/outbound/lexer/lexer.c
  src/adapters/outbound/parser/parser.c
  src/adapters/outbound/compiler/compiler.c
  src/adapters/outbound/backends/registry.c
  src/adapters/outbound/backends/preset_registry.c
  src/adapters/outbound/backends/source_attr.c
  src/adapters/outbound/backends/cord_class.c
  src/adapters/outbound/backends/theme_css.c
  src/adapters/outbound/fonts/font_cache.c
  src/adapters/outbound/backends/react/react_backend.c
  src/adapters/outbound/backends/react/react_ir.c
  src/adapters/outbound/backends/svelte/svelte_backend.c
  src/adapters/outbound/backends/vue/vue_backend.c
  src/adapters/outbound/backends/vue/vue_ir.c
  src/adapters/outbound/backends/static_html/static_html.c
  src/adapters/outbound/backends/email/email_backend.c
  src/adapters/outbound/backends/html/html_backend.c
  src/adapters/outbound/backends/esm/esm_ir.c
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
