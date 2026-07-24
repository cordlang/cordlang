cmake_minimum_required(VERSION 3.10)
project(cordlang C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
  src/main.c
  src/domain/ast.c
  src/domain/diag.c
  src/domain/interp.c
  src/domain/ir.c
  src/domain/ir_pass.c
  src/domain/expr.c
  src/application/init_service.c
  src/application/add_service.c
  src/application/preset_service.c
  src/application/compile_service.c
  src/application/check_service.c
  src/application/analyze_service.c
  src/application/symbols_service.c
  src/application/fmt_service.c
  src/application/run_service.c
  src/application/watch_service.c
  src/application/preview_service.c
  src/application/lsp_service.c
  src/adapters/inbound/cli.c
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
  src/adapters/outbound/backends/theme_css.c
  src/adapters/outbound/backends/react/react_backend.c
  src/adapters/outbound/backends/react/react_ir.c
  src/adapters/outbound/backends/react/react_scaffold.c
  src/adapters/outbound/backends/svelte/svelte_backend.c
  src/adapters/outbound/backends/svelte/svelte_scaffold.c
  src/adapters/outbound/backends/vue/vue_backend.c
  src/adapters/outbound/backends/vue/vue_ir.c
  src/adapters/outbound/backends/vue/vue_scaffold.c
  src/adapters/outbound/backends/solid/solid_backend.c
  src/adapters/outbound/backends/solid/solid_ir.c
  src/adapters/outbound/backends/solid/solid_scaffold.c
  src/adapters/outbound/backends/static_html/static_html.c
  src/adapters/outbound/backends/email/email_backend.c
  src/adapters/outbound/backends/pdf/pdf_backend.c
  src/adapters/outbound/backends/next/next_backend.c
  src/adapters/outbound/backends/sveltekit/sveltekit_backend.c
  src/adapters/outbound/backends/html/html_backend.c
  src/adapters/outbound/runtime/preview_server.c
)

add_executable(cordlang ${SOURCES})
target_include_directories(cordlang PRIVATE src)
# strdup / POSIX APIs under -std=c17 on glibc
target_compile_definitions(cordlang PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(cordlang PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation)

if(WIN32)
  target_link_libraries(cordlang PRIVATE ws2_32)
endif()

option(CORDLANG_ASAN "Build with AddressSanitizer + UBSan" OFF)
if(CORDLANG_ASAN)
  target_compile_options(cordlang PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(cordlang PRIVATE -fsanitize=address,undefined)
endif()
