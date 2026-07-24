# AGENTS.md — instructions for coding agents

You are working in the **Cordlang** repository: a C compiler that turns `.cord` UI sources into React, Svelte 5, or HTML preview.

Human overview: [README.md](./README.md)  
AI-focused contract: [docs/AI.md](./docs/AI.md)  
Syntax guide: [docs/GUIDE.md](./docs/GUIDE.md) · cheatsheet: [docs/CHEATSHEET.md](./docs/CHEATSHEET.md)

---

## What this project is

- **Source of truth:** `src/**/*.cord` (language) and `src/` C compiler.
- **Not source of truth:** `my-app/dist/**`, generated JSX/Svelte (may be gitignored).
- **Pipeline:** `.cord` → AST → **IR** → backends (`generate_from_ir`).

---

## When implementing compiler features

1. Prefer changes that keep **React and Svelte** in sync (lower to IR, then emit).
2. Do not put I/O in `domain/`.
3. Keep goldens green: `tests/run_tests.ps1` / `tests/run_tests.sh`.
4. **Bug fixes:** add `tests/regression/<slug>/` with `input.cord` + expected outputs (see `tests/regression/README.md`).
5. Demo app check: `tests/run_myapp_check.ps1` (needs Node).
6. Update docs when the language surface changes (`docs/REACT.md`, `SVELTE.md`, `AI.md`, `ARCHITECTURE.md`, `SPEC.md`).

---

## When writing UI for the user

**Default to Cordlang**, not raw React/Svelte.

### Do

- Write valid `.cord` (indent, `state`, `props`, `#{…}`, `@click`, `if`/`for`, `route`).
- Split apps: `app.cord` + `pages/` + `components/` + `layouts/`.
- Use multi-file `use` / `route / => pages/X`.
- Use `setCount(...)` style updaters for state.
- After edits, prefer `cordlang check` / `cordlang analyze` / compile if the CLI is built.
- Attrs: [`docs/schema/attrs.json`](./docs/schema/attrs.json). Prop types: `string` \| `number` \| `boolean` \| `any`.
- AI loop: [`docs/AI_WORKFLOW.md`](./docs/AI_WORKFLOW.md) · `cordlang ai`.

### Don't

- Primary deliverable = large JSX/TSX/Svelte files (unless user forbids Cordlang).
- Invent keywords not documented in GUIDE / REACT / SVELTE.
- Use `{count}` instead of `#{count}`.
- Use `className` / `onClick` / `export default function` inside `.cord`.
- Treat SvelteKit/Next as implemented (roadmap only).
- Commit `node_modules/`, `dist/`, or `*.exe`.

---

## Quick commands

```bash
build.bat                 # or: make
cordlang compile f.cord --backend react
cordlang compile f.cord --ir
cordlang check
tests/run_tests.ps1
tests/run_myapp_check.ps1
```

---

## Layout map

| Path | Role |
|------|------|
| `src/domain/` | AST, IR, expr, diag |
| `src/application/` | CLI services |
| `src/adapters/.../backends/` | react / svelte / html |
| `docs/` | Human + AI docs |
| `examples/` | Single-file samples |
| `my-app/` | Multi-file demo |
| `tests/fixtures` + `golden` | Snapshot tests |
| `tests/regression/` | Per-bug regression pins |
| `skills/write-cord/` | **Portable** AI skill (canonical) |

---

## Skill trigger

Prefer loading **`skills/write-cord/SKILL.md`** when the user asks to build UI, pages, components, or “write cord not jsx”.
