# Cordlang — Internal architecture

Contributor-facing map of the C compiler: layers, pipeline, ownership, and how to extend it.

For the **IR contract** (kinds, backend port), see [`IR.md`](./IR.md).  
For **normative syntax**, see [`SPEC.md`](./SPEC.md) (when present).  
For **design notes**, see [`LANGUAGE.md`](./LANGUAGE.md).

---

## Hexagonal layers

```
adapters/inbound/cli.c
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
              └── backends/{react,svelte,html}
```

| Layer | Path | Responsibility |
|-------|------|----------------|
| Inbound | `src/adapters/inbound/` | CLI argv → service calls |
| Application | `src/application/` | `compile`, `check`, `run`, `fmt`, `lsp`, … |
| Ports | `src/application/ports/` | `BackendPort`, `compiler_port`, `fs_port`, … |
| Domain | `src/domain/` | Pure data + transforms: AST, IR, expr, diagnostics |
| Outbound | `src/adapters/outbound/` | Lexer, parser, FS, spawn, codegen backends |

**Rule:** no filesystem, network, or process I/O inside `src/domain/`.

---

## Compile pipeline

```
.cord source
    │
    ▼
Lexer          src/adapters/outbound/lexer/lexer.{c,h}
    │  tokens
    ▼
Parser         src/adapters/outbound/parser/parser.{c,h}
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
    ├── generate_from_ir()        →  React / Svelte / HTML body
    └── scaffold_from_ir()        →  dist/react | dist/svelte | preview
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
| CLI entry | `src/main.c` → `cli_run` in `adapters/inbound/cli.c` |
| Lex | `adapters/outbound/lexer/lexer.c` |
| Parse | `adapters/outbound/parser/parser.c` |
| Multi-file project | `adapters/outbound/compiler/compiler.c` |
| AST | `domain/ast.{c,h}` |
| Expr | `domain/expr.{c,h}` |
| IR | `domain/ir.{c,h}` |
| Diagnostics | `domain/diag.{c,h}` |
| Backend registry | `adapters/outbound/backends/registry.c` (static list) |
| React emit | `backends/react/react_ir.c`, `react_backend.c`, `react_scaffold.c` |
| Svelte emit | `backends/svelte/svelte_backend.c`, `svelte_scaffold.c` |
| HTML preview | `backends/html/html_backend.c` + `runtime/preview_server.c` |
| Theme CSS | `backends/theme_css.c` |
| Source maps | `backends/source_attr.c` (VLQ from `cordlang: source=` markers) |

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

## How to add a backend (today)

1. Implement `BackendPort` with `generate_from_ir` + `scaffold_from_ir` (walk **only** `IrNode`).
2. Register in `registry.c` (`backend_register_all`).
3. Add goldens under `tests/fixtures` + `tests/golden`, and regression pins if fixing bugs.
4. Document map in `docs/<BACKEND>.md`; update `ROADMAP.md` parity matrix.
5. No LLM or network in the emit path.

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
| [`IR.md`](./IR.md) | IR kinds + backend port |
| [`SPEC.md`](./SPEC.md) | Normative language |
| [`LANGUAGE.md`](./LANGUAGE.md) | Design / history |
| [`CONTRIBUTING.md`](../CONTRIBUTING.md) | Workflow + regression policy |
| [`AGENTS.md`](../AGENTS.md) | Agent layout map |
