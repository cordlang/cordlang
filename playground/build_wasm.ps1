# Build Cordlang WASM for the browser playground (Docker + emscripten/emsdk).
# Usage (repo root):
#   powershell -ExecutionPolicy Bypass -File playground\build_wasm.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Root) { $Root = (Get-Location).Path }
Set-Location $Root

$OutDir = Join-Path $Root "playground"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Srcs = @(
  "src/adapters/inbound/wasm_api.c",
  "src/domain/ast.c",
  "src/domain/diag.c",
  "src/domain/interp.c",
  "src/domain/ir.c",
  "src/domain/ir_pass.c",
  "src/domain/expr.c",
  "src/application/compile_service.c",
  "src/application/check_service.c",
  "src/adapters/outbound/fs/fs.c",
  "src/adapters/outbound/process/process_spawn.c",
  "src/adapters/outbound/json/json_mini.c",
  "src/adapters/outbound/html_escape.c",
  "src/adapters/outbound/lexer/lexer.c",
  "src/adapters/outbound/parser/parser.c",
  "src/adapters/outbound/compiler/compiler.c",
  "src/adapters/outbound/backends/registry.c",
  "src/adapters/outbound/backends/preset_registry.c",
  "src/adapters/outbound/backends/source_attr.c",
  "src/adapters/outbound/backends/cord_class.c",
  "src/adapters/outbound/backends/theme_css.c",
  "src/adapters/outbound/fonts/font_cache.c",
  "src/adapters/outbound/backends/react/react_backend.c",
  "src/adapters/outbound/backends/react/react_ir.c",
  "src/adapters/outbound/backends/svelte/svelte_backend.c",
  "src/adapters/outbound/backends/vue/vue_backend.c",
  "src/adapters/outbound/backends/vue/vue_ir.c",
  "src/adapters/outbound/backends/static_html/static_html.c",
  "src/adapters/outbound/backends/email/email_backend.c",
  "src/adapters/outbound/backends/html/html_backend.c",
  "src/adapters/outbound/backends/esm/esm_ir.c"
)

$Flags = @(
  "-O2", "-std=c17", "-Wall", "-Wno-unused-parameter", "-Wno-unused-function",
  "-D_DEFAULT_SOURCE", "-DCORDLANG_WASM=1", "-Isrc",
  "-sWASM=1", "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sEXPORT_NAME=createCordlang",
  "-sEXPORTED_FUNCTIONS=_cordlang_compile,_cordlang_compile_project,_cordlang_free,_cordlang_version,_malloc,_free",
  "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue",
  "-sALLOW_MEMORY_GROWTH=1", "-sSTACK_SIZE=262144", "-sENVIRONMENT=web,worker",
  "-sERROR_ON_UNDEFINED_SYMBOLS=1"
)

$emcc = Get-Command emcc -ErrorAction SilentlyContinue
if ($emcc) {
  Write-Host "Building playground WASM with local emcc"
  & emcc @Flags @Srcs -o (Join-Path $OutDir "cordlang.js")
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
  $docker = Get-Command docker -ErrorAction SilentlyContinue
  if (-not $docker) {
    Write-Host "FAIL: need emcc on PATH or Docker (emscripten/emsdk)." -ForegroundColor Red
    exit 1
  }
  Write-Host "emcc not on PATH - using Docker image emscripten/emsdk"
  $mount = ($Root -replace '\\', '/')
  if ($mount -match '^[A-Za-z]:') {
    $drive = $mount.Substring(0, 1).ToLower()
    $mount = "/$drive" + $mount.Substring(2)
  }
  # Link in the container filesystem: llvm-objcopy cannot reliably rewrite a
  # WASM file on every Windows Docker bind mount. Copy finished assets back.
  docker run --rm -v "${mount}:/src" -w /src emscripten/emsdk:3.1.74 `
    sh -lc 'emcc "$@" -o /tmp/cordlang.js && cp /tmp/cordlang.js /tmp/cordlang.wasm playground/' `
    -- @Flags @Srcs
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Get-ChildItem (Join-Path $OutDir "cordlang.js"), (Join-Path $OutDir "cordlang.wasm") |
  Format-Table Name, Length
Write-Host "OK: playground WASM ready. Serve the playground folder over HTTP."
exit 0
