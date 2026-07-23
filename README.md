# Cordlang

**Universal UI intermediate language** — write UI once in compact `.cord` files, compile to **React**, **Svelte 5**, or a **native HTML preview** inside the CLI.

Cordlang is not another JS framework. It is a small **compiler** (C, hexagonal architecture) that turns an indented UI DSL into idiomatic target code.

```cord
def Counter
  state count=0
  props label="Counter"

  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold
    p "Count: #{count}" muted
    row gap=8 center
      btn "-" @click=setCount(count - 1) variant=outline
      span "#{count}" size=4xl bold
      btn "+" @click=setCount(count + 1) variant=primary
```

```bash
cordlang run              # native HTML preview (no Node)
cordlang run react        # Vite + React + Tailwind → dist/react
cordlang run svelte       # Vite + Svelte 5 runes → dist/svelte
```

Same multi-file `src/**/*.cord` for every backend.

---

## Why

| Goal | How Cordlang helps |
|------|---------------------|
| Less boilerplate for UI | Indent + attrs + `#{expr}` instead of JSX/Svelte ceremony |
| AI-friendly surface | Fewer tokens to describe the same UI (~3–5× denser) |
| One source, many targets | IR → React / Svelte / HTML (more backends planned) |
| Real apps | Routes, layouts, state, forms, lazy, context, fetch… |

---

## Status

**Working compiler** (not a sketch):

- Multi-file modules (`use` / routes by path)
- Canonical **IR** (React + Svelte emit from `IrNode`)
- DX: `check`, `fmt`, `symbols`, `goto`, `--watch`, `--check` (vite build)
- Demo app: [`my-app/`](./my-app/)

Design notes & syntax reference: [`docs/LANGUAGE.md`](./docs/LANGUAGE.md)  
Roadmap: [`docs/ROADMAP.md`](./docs/ROADMAP.md)

---

## Quick start

### Requirements

- **Windows** (primary): `gcc` (MinGW) for `build.bat`
- Optional: Node.js + npm for `cordlang run react|svelte` (Vite scaffold)
- Linux/macOS: `make` or CMake (same sources)

### Build the CLI

```bash
# Windows
build.bat

# or
make
# or
cmake -B build && cmake --build build
```

Produces `cordlang.exe` (Windows) or `cordlang`.

### Create & run a project

```bash
cordlang init my-app
cd my-app

cordlang run                 # HTML preview at http://127.0.0.1:4173
cordlang run react           # generate dist/react
cordlang run svelte          # generate dist/svelte
cordlang run react --check   # npm install if needed + vite build
cordlang run react --watch   # rebuild on .cord changes
```

### Or use the included demo

```bash
cd my-app
..\cordlang.exe run
..\cordlang.exe run react
..\cordlang.exe check
..\cordlang.exe symbols
```

### Compile a single file

```bash
cordlang compile examples/counter.cord --backend react
cordlang compile examples/counter.cord --backend svelte
cordlang compile examples/counter.cord --ir          # dump IR tree
cordlang compile examples/counter.cord --check
```

### DX tooling

```bash
cordlang check [path]           # semantic diagnostics (file:line:col)
cordlang fmt src/               # format .cord
cordlang fmt --check src/
cordlang symbols [entry]        # components, routes, layouts
cordlang goto Counter [entry]   # definition path
```

---

## CLI reference

| Command | Description |
|---------|-------------|
| `init [name]` | Scaffold `cordlang.json` + `src/` |
| `run` / `run preview` | Native HTML runtime preview |
| `run react` / `run svelte` | Full Vite scaffold under `dist/<backend>` |
| `run <backend> --check` | Scaffold + `vite build` smoke |
| `run <backend> --watch` | Rebuild on `.cord` change |
| `build <backend>` | Compile entry only |
| `check [path]` | Semantic checker |
| `compile <file.cord>` | Emit to stdout (`--backend`, `-o`, `--ir`, `--ast`, `--tokens`, `--sourcemap`) |
| `fmt [path]` / `fmt --check` | Formatter |
| `symbols` / `goto <Name>` | Project symbols |

Backends: `preview`/`html`, `react`, `svelte`.

---

## Project layout

```
cordlang/
├── src/                 # Compiler (C, hexagonal)
│   ├── domain/          # AST, IR, expr, diagnostics
│   ├── application/     # services + ports
│   └── adapters/        # CLI, lexer, parser, backends
├── docs/
│   ├── ROADMAP.md       # phases & next steps
│   ├── LANGUAGE.md      # design & syntax reference
│   ├── REACT.md         # Cordlang ↔ React map
│   ├── SVELTE.md        # Cordlang ↔ Svelte map
│   └── IR.md            # IR pipeline
├── examples/            # single-file samples
├── my-app/              # multi-file demo (source only in git)
├── tests/               # fixtures + golden codegen
│   ├── run_tests.ps1
│   ├── run_tests.sh
│   └── run_tests.bat
├── build.bat
├── Makefile
└── CMakeLists.txt
```

**Pipeline:**

```
.cord → Lexer → Parser → AST → IR → React | Svelte | HTML
```

---

## Tests

```bash
# Windows
build.bat
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1

# Update goldens after intentional codegen changes:
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -UpdateGoldens

# Unix
make && ./tests/run_tests.sh
```

---

## Documentation

| Doc | Content |
|------|---------|
| [docs/ROADMAP.md](./docs/ROADMAP.md) | Phases A–F, IR, next sprint |
| [docs/LANGUAGE.md](./docs/LANGUAGE.md) | Language design & syntax |
| [docs/REACT.md](./docs/REACT.md) | Mapping to React APIs |
| [docs/SVELTE.md](./docs/SVELTE.md) | Mapping to Svelte 5 |
| [docs/IR.md](./docs/IR.md) | Intermediate representation |

---

## Architecture (hexagonal)

```
CLI (inbound)
    → application services (init, compile, run, check, fmt, …)
        → ports (backend, compiler, fs)
            → adapters: lexer, parser, IR, backends (react / svelte / html)
```

Domain stays free of I/O. Backends consume the **canonical IR** (IR-2).

---

## Contributing

1. Build with `build.bat` / `make`
2. Keep goldens green (`tests/run_tests.ps1`)
3. Prefer additive language features + IR mapping before backend-specific hacks
4. Update `docs/REACT.md` / `SVELTE.md` when the surface changes

Issues/milestones ideas: see end of [ROADMAP.md](./docs/ROADMAP.md).

---

## License

No license file is committed yet. Add one (e.g. MIT) before publishing if you intend open-source redistribution.

---

## Next

See **“Ahora”** in [docs/ROADMAP.md](./docs/ROADMAP.md): close IR residuals (theme + HTML preview), CI, then meta-frameworks (SvelteKit / Next) or editor DX.
