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
  -sEXPORT_NAME=createCordlang
  -sEXPORTED_FUNCTIONS=_cordlang_compile,_cordlang_free,_cordlang_version,_malloc,_free
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue
  -sALLOW_MEMORY_GROWTH=1
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
  docker run --rm \
    -v "$ROOT:/src" \
    -w /src \
    emscripten/emsdk:3.1.74 \
    emcc "${EMCC_FLAGS[@]}" "${SRCS[@]}" -o playground/cordlang.js
else
  echo "FAIL: need emcc on PATH or Docker (emscripten/emsdk)."
  exit 1
fi

ls -la "$OUT_DIR/cordlang.js" "$OUT_DIR/cordlang.wasm"
echo "OK: playground WASM ready. Open playground/index.html via a local HTTP server."
