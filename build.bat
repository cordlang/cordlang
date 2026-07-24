@echo off
echo Building Cordlang (hexagonal + native runtime)...
gcc -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation -g -std=c17 -D_POSIX_C_SOURCE=200809L -Isrc -o cordlang ^
  src/main.c ^
  src/domain/ast.c ^
  src/domain/diag.c ^
  src/domain/interp.c ^
  src/domain/ir.c ^
  src/domain/expr.c ^
  src/application/init_service.c ^
  src/application/add_service.c ^
  src/application/compile_service.c ^
  src/application/check_service.c ^
  src/application/analyze_service.c ^
  src/application/symbols_service.c ^
  src/application/fmt_service.c ^
  src/application/run_service.c ^
  src/application/watch_service.c ^
  src/application/preview_service.c ^
  src/application/lsp_service.c ^
  src/adapters/inbound/cli.c ^
  src/adapters/outbound/fs/fs.c ^
  src/adapters/outbound/process/process_spawn.c ^
  src/adapters/outbound/json/json_mini.c ^
  src/adapters/outbound/html_escape.c ^
  src/adapters/outbound/lexer/lexer.c ^
  src/adapters/outbound/parser/parser.c ^
  src/adapters/outbound/compiler/compiler.c ^
  src/adapters/outbound/backends/registry.c ^
  src/adapters/outbound/backends/source_attr.c ^
  src/adapters/outbound/backends/theme_css.c ^
  src/adapters/outbound/backends/react/react_backend.c ^
  src/adapters/outbound/backends/react/react_ir.c ^
  src/adapters/outbound/backends/react/react_scaffold.c ^
  src/adapters/outbound/backends/svelte/svelte_backend.c ^
  src/adapters/outbound/backends/svelte/svelte_scaffold.c ^
  src/adapters/outbound/backends/vue/vue_backend.c ^
  src/adapters/outbound/backends/vue/vue_ir.c ^
  src/adapters/outbound/backends/vue/vue_scaffold.c ^
  src/adapters/outbound/backends/solid/solid_backend.c ^
  src/adapters/outbound/backends/solid/solid_ir.c ^
  src/adapters/outbound/backends/solid/solid_scaffold.c ^
  src/adapters/outbound/backends/static_html/static_html.c ^
  src/adapters/outbound/backends/email/email_backend.c ^
  src/adapters/outbound/backends/pdf/pdf_backend.c ^
  src/adapters/outbound/backends/next/next_backend.c ^
  src/adapters/outbound/backends/sveltekit/sveltekit_backend.c ^
  src/adapters/outbound/backends/html/html_backend.c ^
  src/adapters/outbound/runtime/preview_server.c ^
  -lws2_32
if %ERRORLEVEL% EQU 0 (
  echo Build successful: cordlang.exe
) else (
  echo Build failed.
  exit /b 1
)
