# Native smoke for wasm_api (no Docker / emcc required).
# Usage: powershell -ExecutionPolicy Bypass -File playground\smoke_native.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$Srcs = @(
  "playground/smoke_native.c",
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

$Out = Join-Path $Root "playground\smoke_native.exe"
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
  Write-Host "FAIL: gcc not found" -ForegroundColor Red
  exit 1
}

Write-Host "Building playground native smoke..."
& gcc -O2 -std=c17 -Wall -Wno-unused-parameter -Wno-unused-function `
  -D_DEFAULT_SOURCE -DCORDLANG_WASM=1 -Isrc `
  @Srcs -o $Out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $Out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# UI contract: the browser playground must call the multi-file project API.
$html = Get-Content (Join-Path $Root "playground\index.html") -Raw
foreach ($needle in @("cordlang_compile_project", "/playground/src/app.cord")) {
  if ($html.IndexOf($needle) -lt 0) {
    Write-Host "FAIL: playground/index.html missing $needle" -ForegroundColor Red
    exit 1
  }
}
Write-Host "PASS: playground UI wires project API"
exit 0
