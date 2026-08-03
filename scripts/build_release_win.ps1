# Release build for Windows (MinGW). Embeds VERSION into cordlang --version.
# Usage:
#   powershell -File scripts\build_release_win.ps1 -Version 0.0.013-alpha.1

param(
  [Parameter(Mandatory = $true)]
  [string]$Version
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$srcs = @(
  "src/main.c",
  "src/domain/ast.c",
  "src/domain/diag.c",
  "src/domain/interp.c",
  "src/domain/ir.c",
  "src/domain/ir_pass.c",
  "src/domain/expr.c",
  "src/application/init_service.c",
  "src/application/add_service.c",
  "src/application/preset_service.c",
  "src/application/compile_service.c",
  "src/application/check_service.c",
  "src/application/analyze_service.c",
  "src/application/symbols_service.c",
  "src/application/fmt_service.c",
  "src/application/run_service.c",
  "src/application/watch_service.c",
  "src/application/preview_service.c",
  "src/application/lsp_service.c",
  "src/adapters/inbound/cli.c",
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
  "src/adapters/outbound/backends/react/react_scaffold.c",
  "src/adapters/outbound/backends/svelte/svelte_backend.c",
  "src/adapters/outbound/backends/svelte/svelte_scaffold.c",
  "src/adapters/outbound/backends/vue/vue_backend.c",
  "src/adapters/outbound/backends/vue/vue_ir.c",
  "src/adapters/outbound/backends/vue/vue_scaffold.c",
  "src/adapters/outbound/backends/solid/solid_backend.c",
  "src/adapters/outbound/backends/solid/solid_ir.c",
  "src/adapters/outbound/backends/solid/solid_scaffold.c",
  "src/adapters/outbound/backends/static_html/static_html.c",
  "src/adapters/outbound/backends/email/email_backend.c",
  "src/adapters/outbound/backends/pdf/pdf_backend.c",
  "src/adapters/outbound/backends/next/next_backend.c",
  "src/adapters/outbound/backends/sveltekit/sveltekit_backend.c",
  "src/adapters/outbound/backends/html/html_backend.c",
  "src/adapters/outbound/backends/esm/esm_ir.c",
  "src/adapters/outbound/backends/esm/esm_runtime.c",
  "src/adapters/outbound/backends/esm/esm_css.c",
  "src/adapters/outbound/runtime/preview_server.c",
  "src/adapters/outbound/runtime/dev_server.c",
  "src/adapters/outbound/term/term_log.c"
)

$verMacro = '\"' + $Version + '\"'
$gccArgs = @(
  "-Wall", "-Wextra", "-Werror",
  "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-format-truncation",
  "-O2", "-std=c17",
  "-D_POSIX_C_SOURCE=200809L",
  "-DCORDLANG_VERSION=$verMacro",
  "-Isrc",
  "-o", "cordlang.exe"
) + $srcs + @("-lws2_32")

Write-Host "Building cordlang.exe VERSION=$Version"
if (Test-Path "cordlang.exe") {
  try {
    Rename-Item "cordlang.exe" "cordlang.exe.prev" -Force -ErrorAction Stop
  } catch {
    Write-Host "Note: could not rename existing cordlang.exe (may be locked): $_"
  }
}
& gcc @gccArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& .\cordlang.exe --version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item "cordlang.exe.prev" -Force -ErrorAction SilentlyContinue
Write-Host "OK: cordlang.exe"
exit 0
