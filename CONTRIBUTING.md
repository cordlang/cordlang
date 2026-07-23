# Contributing to Cordlang

## Setup

1. Clone the repo
2. Install a C17 compiler (`gcc` / MinGW on Windows)
3. Build:

```bash
build.bat    # Windows
make         # Unix
```

4. Run tests:

```bash
# Windows
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1

# Unix
./tests/run_tests.sh
```

## Workflow

- Prefer small, focused PRs
- Keep **golden tests** green; if codegen changes intentionally:

```bash
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -UpdateGoldens
```

- Update docs when the language surface changes:
  - `docs/REACT.md` / `docs/SVELTE.md` for maps
  - `docs/IR.md` for IR kinds
  - `docs/ROADMAP.md` for phase status
  - `docs/LANGUAGE.md` for syntax design

## Architecture rules of thumb

1. **Parse** into AST (`domain/ast`)
2. **Lower** to IR (`domain/ir`) — backends should not re-parse
3. **Emit** from IR (`generate_from_ir`) for React/Svelte
4. No I/O in `domain/`

## Demo app

`my-app/` is the multi-file showcase. Source lives in git; `my-app/dist/` and `node_modules` are ignored.

```bash
cd my-app
../cordlang.exe run react --check
```

## Questions

Open an issue with labels such as `lang:core`, `backend:react`, `backend:svelte`, `dx`, `ir`.
