# Cordlang

**La forma más rápida de construir UI con IA** — escribe `.cord`, el compilador baja a un **IR canónico** y emite **React**, **Svelte 5** o un **preview HTML** nativo.

Cordlang **no** es otro framework JS (sin Vite/Next/router propios). Es un **lenguaje intermedio** denso para humanos y modelos: vibecode → `check` → backends reales.

```cord
def Counter
  state count=0
  props label: string = "Counter"

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
cordlang check            # diagnostics anti-alucinación
cordlang analyze          # score heurístico (sin LLM)
```

Same multi-file `src/**/*.cord` for every backend.

---

## Why

| Goal | How Cordlang helps |
|------|---------------------|
| Less boilerplate for UI | Indent + attrs + `#{expr}` instead of JSX/Svelte ceremony |
| AI-friendly surface | Fewer tokens; schema + `check` atrapan traps comunes |
| One source, many targets | IR → React / Svelte / HTML (more backends when IR is boring) |
| Real apps | Routes, layouts, state, forms, lazy, context, fetch… |

**No somos “JSX más corto”.** Somos el IR + DX alrededor para que la IA escriba UI válida.

---

## Status

**Working compiler** (not a sketch):

- Multi-file modules (`use` / routes by path)
- Canonical **IR** (React + Svelte emit from `IrNode`)
- DX: `check`, `fmt`, `symbols`, `goto`, `--watch`, `--check` (vite build)
- Demo app: [`my-app/`](./my-app/)

### Docs

| Doc | What |
|-----|------|
| [docs/GUIDE.md](./docs/GUIDE.md) | **User guide + samples** |
| [docs/CHEATSHEET.md](./docs/CHEATSHEET.md) | One-screen syntax |
| [docs/EXAMPLES.md](./docs/EXAMPLES.md) | Catalog of examples |
| [docs/AI.md](./docs/AI.md) | **For AI models** — what to write / avoid |
| [docs/schema/attrs.json](./docs/schema/attrs.json) | Machine-readable attrs (LSP / IA) |
| [docs/TEMPLATES.md](./docs/TEMPLATES.md) | Cord-native project templates |
| [docs/LSP.md](./docs/LSP.md) | Editor / LSP mínimo |
| [AGENTS.md](./AGENTS.md) | Coding-agent brief |
| [docs/ROADMAP.md](./docs/ROADMAP.md) | Horizonte A/B + histórico |
| [docs/REACT.md](./docs/REACT.md) · [SVELTE.md](./docs/SVELTE.md) · [IR.md](./docs/IR.md) | Maps & IR |

### AI / agents

Cordlang is designed for **LLM-authored UI**. Point tools at:

| Path | Role |
|------|------|
| [`docs/AI.md`](./docs/AI.md) | Hard rules (do / don't) for any model |
| [`AGENTS.md`](./AGENTS.md) | Repo-wide coding-agent brief |
| [`skills/write-cord/`](./skills/write-cord/) | **Portable skill** (any tool / LLM) |
| [`.github/copilot-instructions.md`](./.github/copilot-instructions.md) | GitHub Copilot |

Rule of thumb for models: **write `.cord`, not JSX**, then `cordlang run react|svelte`.

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
│   ├── ARCHITECTURE.md  # compiler internals
│   ├── LANGUAGE.md      # design & syntax reference
│   ├── REACT.md         # Cordlang ↔ React map
│   ├── SVELTE.md        # Cordlang ↔ Svelte map
│   └── IR.md            # IR pipeline
├── examples/            # single-file samples
├── my-app/              # multi-file demo (source only in git)
├── tests/               # fixtures + golden + regression
│   ├── fixtures/
│   ├── golden/
│   ├── regression/
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
# Demo app: scaffold + vite build (react + svelte)
powershell -ExecutionPolicy Bypass -File tests\run_myapp_check.ps1

# Update goldens after intentional codegen changes:
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -UpdateGoldens

# Unix
make && ./tests/run_tests.sh
./tests/run_myapp_check.sh   # needs Node.js / npm
```

CI (`.github/workflows/ci.yml`) runs goldens **and** `my-app` `--check` on Windows and Ubuntu, with npm/`node_modules` cache.

---

## Documentation

| Doc | Content |
|------|---------|
| [docs/ROADMAP.md](./docs/ROADMAP.md) | Phases A–F, IR, next sprint |
| [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md) | Compiler internals (parser → IR → codegen) |
| [docs/SPEC.md](./docs/SPEC.md) | Normative language specification (v0.x) |
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

Domain stays free of I/O. Backends consume the **canonical IR** (IR-2). Details: [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md).

---

## Contributing

- **How to contribute:** [CONTRIBUTING.md](./CONTRIBUTING.md) (build, tests, PR rules)  
- **Who made Cordlang:** [AUTHORS.md](./AUTHORS.md)  
- Roadmap / issues ideas: [docs/ROADMAP.md](./docs/ROADMAP.md)

```bash
build.bat   # or: make
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

Prefer additive language features + IR mapping before backend-specific hacks. Keep goldens green.

---

## License

[MIT](./LICENSE) — Copyright (c) 2026 [owellandry](https://github.com/owellandry) (shimonikg) and Cordlang contributors.

Free to use, modify, and redistribute (including commercial use), with attribution.

---

## Next

See **Fase G** in [docs/ROADMAP.md](./docs/ROADMAP.md): IR residuals (theme + HTML preview), CI polish, then meta-frameworks (SvelteKit / Next) or editor DX.
