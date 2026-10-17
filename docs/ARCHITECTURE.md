# Cordlang — Internal architecture

Contributor-facing map of the C compiler: layers, pipeline, ownership, and how to extend it.

For the **IR contract** (kinds, backend port), see [`IR.md`](./IR.md).  
For **normative syntax**, see [`SPEC.md`](./SPEC.md) (when present).  
For **design notes**, see [`LANGUAGE.md`](./LANGUAGE.md).

---

## Hexagonal layers

```
adapters/inbound/cli.c + cli_{usage,compile,check,fmt,ai,build}.c
        │
        ▼
application/*_service.c     (use cases)
        │
   ports/*.h                (interfaces)
        │
        ├── domain/         (AST, IR, expr, diag — no I/O)
        └── adapters/outbound/
              ├── lexer / parser / compiler
              ├── fs / process / json
              └── backends/{react,svelte,vue,solid,html,esm}
```

| Layer | Path | Responsibility |
|-------|------|----------------|
| Inbound | `src/adapters/inbound/` | CLI argv → service calls |
| Application | `src/application/` | `compile`, `check`, `run`, `fmt`, `lsp`, … |
| Ports | `src/application/ports/` | `BackendPort`, `compiler_port`, `fs_port`, … |
| Domain | `src/domain/` | Pure data + transforms: AST, IR, expr, diagnostics |
| Outbound | `src/adapters/outbound/` | Lexer, parser, FS, spawn, codegen backends |

**Rule:** no filesystem, network, or process I/O inside `src/domain/`.

## Micro-modules

Every hand-written C translation unit under `src/` must stay at or below **300
physical lines**. Split larger files along cohesive responsibilities or use
cases; do not shrink them by compressing formatting or moving implementation
into oversized include files. Keep shared contracts narrow, put cross-module
private declarations in an internal header, and preserve the layer direction
shown above. A module should have one reason to change, while related parsing,
IR, rendering, and I/O work should remain grouped by the responsibility it
serves.

---

## Compile pipeline

```
.cord source
    │
    ▼
Lexer          src/adapters/outbound/lexer/lexer.{c,h}
    │  tokens
    ▼
Parser         src/adapters/outbound/parser/parser*.c + parser.h
    │  Node* tree
    ▼
Compiler       src/adapters/outbound/compiler/compiler.c
               compiler_parse_source / _file / _project
    │  Ast / CompileResult
    ▼
ir_from_ast    src/domain/ir.{c,h}   (+ expr_normalize)
    │  IrProgram*
    ▼
(IR passes)    Phase H3 — opt-in transforms on IrProgram
    │
    ├── ir_dump()                 →  compile --ir
    ├── generate_from_ir()        →  React / Svelte / Vue / HTML / ESM body
    └── scaffold_from_ir()        →  dist/react | dist/svelte | dist/vue | preview
```

Orchestration lives in `src/application/compile_service.c` and `run_service.c`:

1. `compiler_parse_project(entry)` → AST (resolves `use` / route modules).
2. `ir_from_ast(root, entry)` → `IrProgram`.
3. Optional IR passes (H3).
4. `backend->generate_from_ir(ir)` or `scaffold_from_ir`.
5. `ir_free(ir)` then `compiler_result_free(&result)`.

`check` / `analyze` / `fmt` are **sibling** services: they parse (and sometimes walk AST) but are not mandatory steps between AST and IR on the happy `compile` path.

---

## File map by stage

| Stage | Files |
|-------|--------|
| CLI entry | `src/main.c` → `cli_run` dispatcher in `adapters/inbound/cli.c`; command handlers in `adapters/inbound/cli_*.c` |
| WASM bridge | `adapters/inbound/wasm_api.c` exports, `wasm_project.c` virtual project manifest, `wasm_result.c` JSON result serialization |
| LSP | `application/lsp_service.c` protocol loop; document state, JSON-RPC, diagnostics and feature handlers live in `lsp_*.c` modules |
| Preview service | `application/preview_service.c` lifecycle, `preview_context.c` cache/context, `preview_response.c` assets, `preview_module.c` Cord compilation, `preview_routes.c` routing, `preview_smoke.c` smoke path, `preview_build_esm.c` static export |
| Symbols service | `application/symbols_service.c` list/goto, `symbols_text.c` identifier and path parsing, `symbols_locations.c` AST location collection, `symbols_project.c` project scan |
| Filesystem port | `adapters/outbound/fs/fs.c` host reads/writes, `fs_memory.c` mounted virtual files, `fs_path.c` paths, `fs_copy.c` copies, `fs_walk.c` project walk |
| Lex | `adapters/outbound/lexer/lexer*.c` |
| Parse | `adapters/outbound/parser/parser*.c` (statement, declarations, attrs, modules, control, React families) |
| Multi-file project | `adapters/outbound/compiler/compiler.c` |
| AST | `domain/ast.{c,h}` |
| Expr | `domain/expr.c`, `expr_lexer.c`, `expr_parser.c`, `expr.h` |
| Interpolation | `domain/interp.c` (AST splitting), `interp_js.c`, `interp_html.c` |
| IR | `domain/ir.{c,h}` |
| Diagnostics | `domain/diag.{c,h}` |
| Backend registry | `adapters/outbound/backends/registry.c` (static list) |
| Presets | `adapters/outbound/backends/preset_registry.c` metadata, `preset_packages.c` dependency merge, `preset_bridges.c` generated bridge files |
| React emit | `backends/react/react_ir.c`, `react_backend.c`, `react_scaffold.c` |
| Svelte emit | `backends/svelte/svelte_backend.c`, `svelte_scaffold.c` |
| Vue emit | `backends/vue/vue_ir.c`, `vue_backend.c`, `vue_scaffold.c` |
| Solid emit | `backends/solid/solid_ir.c`, `solid_backend.c`, `solid_scaffold.c` |
| Static HTML (email/pdf) | `backends/static_html/static_html.c` + `email/` + `pdf/` |
| Next meta | `backends/next/next_backend.c` (wraps React emit) |
| SvelteKit meta | `backends/sveltekit/sveltekit_backend.c` (wraps Svelte emit) |
| ESM native preview | `backends/esm/` — `esm_ir.c`, `esm_runtime.c`, `esm_runtime_part*.c`, `esm_error.c`, `esm_css.c`, `esm_backend.h` |
| Shared attr→class | `backends/cord_class*.c` (+ `cord_class.h`) — attr rules and class emission used by react/svelte/vue/solid/esm |
| Dev server (ESM run) | `runtime/dev_server.c`, `dev_server_http.c`, `dev_server_sse.c`, `dev_server_assets.c`, `dev_watch.c` — HTTP, SSE, static assets and file watching; wired by `preview_service.c` |
| HTML preview (legacy) | `backends/html/html_backend.c` + `runtime/preview_server.c` (`cordlang run html`) |
| Theme CSS | `backends/theme_css.c` AST entry, `theme_css_ir.c` IR entry, `theme_css_shared.c` shared token emission, `theme_css_core.c` CSS validation and buffering |
| Source maps | `backends/source_attr*.c` (VLQ from `cordlang: source=` markers; writing is isolated in `source_attr_file.c`) |

---

## Memory ownership

| Object | Created by | Freed by |
|--------|------------|----------|
| Tokens / parse scratch | lexer/parser | parser / compiler helpers |
| `Ast` / `Node*` tree | `compiler_parse_*` | `compiler_result_free` |
| `IrProgram` / `IrNode` | `ir_from_ast` | `ir_free` |
| Codegen string | `generate_from_ir` | caller (`free`) |
| `IrNode.origin` | weak pointer into AST | **do not free** via IR; AST must outlive emit |

Typical pattern in `compile_service_file_ex`:

```c
CompileResult result = compiler_parse_project(cord_path);
IrProgram *ir = ir_from_ast(result.ast->root, cord_path);
char *out = backend->generate_from_ir(ir);
ir_free(ir);
compiler_result_free(&result);
```

Backends must not retain IR or AST after returning from generate/scaffold.

---

## Errors and diagnostics

- Parse failures: `CompileResult.ok == 0` + `result.error` string; services print to stderr.
- Semantic issues: `check_service` / `domain/diag` → `file:line:col: error|warning: …`.
- Hard limits (routes, units, etc.): prefer **diagnostic + non-zero exit**, not silent truncation.
- Unknown backends: stderr + `NULL` return from compile service.

---

## How to add a backend (React-style checklist)

Add a new target by mirroring the React three-file layout under
`src/adapters/outbound/backends/<name>/`:

1. **`<name>_backend.h`** — declare `generate_from_ir`, `scaffold_from_ir`,
   AST wrappers, `emit_modules_from_ir`, and `<name>_backend_port()`.
2. **`<name>_backend.c`** (or split **`<name>_ir.c`**) — pure `IrNode` walkers
   for codegen; AST entrypoints only lower via `ir_from_ast` then call IR APIs.
   Do **not** walk AST in `generate_from_ir`. No LLM / network in emit.
3. **`<name>_scaffold.c`** — Vite (or equivalent) skeleton + write modules via
   `emit_modules_from_ir`; set `BackendPort` fields including
   `needs_node_check` (1 if `run --check` / `--watch` use npm+vite).
4. Register in `registry.c` (`backend_register_all`); keep `MAX_BACKENDS` headroom.
5. Wire sources in `Makefile`, `CMakeLists.txt`, and `build.bat` if present.
6. Add goldens: `tests/fixtures` + `tests/golden/*.<name>.txt`; extend
   `BACKENDS` in `tests/run_tests.sh` / `.ps1`.
7. Document Cord ↔ target map in `docs/<NAME>.md`; update ROADMAP parity.

See [`IR.md`](./IR.md) § “Contrato para un backend nuevo”.

---

## How to add an IR transform (Phase H3)

Passes are **in-tree**, opt-in (`--pass` / `cordlang.json` `"passes"`). They run after `ir_from_ast` and before codegen.

1. Implement `IrProgram *my_pass(IrProgram *ir)` in `src/domain/ir_pass.c` (may mutate; free dropped nodes with `ir_free_node`).
2. Register the pass in the `k_passes[]` table (`name`, fn, description).
3. Add a regression under `tests/regression/<slug>/` with `passes.txt` listing the pass name(s).
4. Default compile path must stay identical when no passes are requested.

```bash
cordlang compile file.cord --backend react --pass strip-debug
cordlang compile --list-passes
# cordlang.json: { "passes": "strip-debug" }
```

Dynamic plugins (`dlopen` / WASM) are **out of scope** for H3.

---

## Related docs

| Doc | Audience |
|-----|----------|
| [`PREVIEW.md`](./PREVIEW.md) | ESM native preview (`cordlang run`) |
| [`IR.md`](./IR.md) | IR kinds + backend port |
| [`SPEC.md`](./SPEC.md) | Normative language |
| [`LANGUAGE.md`](./LANGUAGE.md) | Design / history |
| [`CONTRIBUTING.md`](../CONTRIBUTING.md) | Workflow + regression policy |
| [`AGENTS.md`](../AGENTS.md) | Agent layout map |
