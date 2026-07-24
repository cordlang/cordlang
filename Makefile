CC = gcc
# c17 + POSIX (strdup, etc. on glibc). On Windows MinGW, extra define is harmless.
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function \
	-Wno-format-truncation -g -std=c17 \
	-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Isrc
TARGET = cordlang

SRC = \
  src/main.c \
  src/domain/ast.c \
  src/domain/diag.c \
  src/domain/interp.c \
  src/domain/ir.c \
  src/domain/expr.c \
  src/application/init_service.c \
  src/application/compile_service.c \
  src/application/check_service.c \
  src/application/analyze_service.c \
  src/application/symbols_service.c \
  src/application/fmt_service.c \
  src/application/run_service.c \
  src/application/watch_service.c \
  src/application/preview_service.c \
  src/application/lsp_service.c \
  src/adapters/inbound/cli.c \
  src/adapters/outbound/fs/fs.c \
  src/adapters/outbound/process/process_spawn.c \
  src/adapters/outbound/json/json_mini.c \
  src/adapters/outbound/html_escape.c \
  src/adapters/outbound/lexer/lexer.c \
  src/adapters/outbound/parser/parser.c \
  src/adapters/outbound/compiler/compiler.c \
  src/adapters/outbound/backends/registry.c \
  src/adapters/outbound/backends/source_attr.c \
  src/adapters/outbound/backends/theme_css.c \
  src/adapters/outbound/backends/react/react_backend.c \
  src/adapters/outbound/backends/react/react_ir.c \
  src/adapters/outbound/backends/react/react_scaffold.c \
  src/adapters/outbound/backends/svelte/svelte_backend.c \
  src/adapters/outbound/backends/svelte/svelte_scaffold.c \
  src/adapters/outbound/backends/html/html_backend.c \
  src/adapters/outbound/runtime/preview_server.c

# Optional sanitizers: make ASAN=1
ifeq ($(ASAN),1)
  CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif

# Windows (MinGW) needs Winsock
ifeq ($(OS),Windows_NT)
  LDFLAGS = -lws2_32
else
  LDFLAGS =
endif

.PHONY: all clean test goldens bench

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET).exe

# Golden snapshot tests (react + svelte codegen)
# On Windows: powershell -File tests/run_tests.ps1
# Update goldens: make goldens  OR  tests/run_tests.bat -UpdateGoldens
test: $(TARGET)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_tests.ps1
else
	@bash tests/run_tests.sh
endif

# G4: my-app react+svelte vite build (requires Node.js/npm)
.PHONY: test-myapp
test-myapp: $(TARGET)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_myapp_check.ps1
else
	@bash tests/run_myapp_check.sh
endif

goldens: $(TARGET)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_tests.ps1 -UpdateGoldens
else
	@bash tests/run_tests.sh --update
endif

# Compile-time benches + Cord vs JSX/Svelte size compare (not on default CI)
.PHONY: bench
bench: $(TARGET)
	@chmod +x bench/run_bench.sh bench/compare/run_compare.sh
	@bash bench/run_bench.sh
	@bash bench/compare/run_compare.sh
