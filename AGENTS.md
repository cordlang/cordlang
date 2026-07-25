# AGENTS.md — instructions for coding agents

You are working in the **Cordlang** repository: a C compiler that turns dense `.cord` UI sources into React, Svelte 5, or a native **ESM preview** (default `cordlang run`).

**North star:** vibecode + AI with **minimal token spend** — dense sources, deterministic `check`/`analyze`, no LLM in `compile`. Prefer the AI loop over expanding meta backends. If docs disagree, [`docs/AI_CONTEXT.md`](./docs/AI_CONTEXT.md) wins for product intent.

Human overview: [README.md](./README.md)  
AI-focused contract: [docs/AI_CONTEXT.md](./docs/AI_CONTEXT.md) · [docs/AI.md](./docs/AI.md)  
Syntax guide: [docs/GUIDE.md](./docs/GUIDE.md) · cheatsheet: [docs/CHEATSHEET.md](./docs/CHEATSHEET.md)  
ESM native preview: [docs/PREVIEW.md](./docs/PREVIEW.md)

---

## What this project is

- **Source of truth:** `src/**/*.cord` (language) and `src/` C compiler.
- **Not source of truth:** generated `dist/**`, JSX/Svelte scaffolds (gitignored).
- **Pipeline:** `.cord` → AST → **IR** → backends (`generate_from_ir`).

---

## When implementing compiler features

1. Prefer changes that keep **React and Svelte** in sync (lower to IR, then emit).
2. Do not put I/O in `domain/`.
3. Keep goldens green: `tests/run_tests.ps1` / `tests/run_tests.sh`.
4. **Bug fixes:** add `tests/regression/<slug>/` with `input.cord` + expected outputs (see `tests/regression/README.md`).
5. Template smoke: `tests/run_template_check.ps1` (needs Node; uses `templates/counter`).
6. Update docs when the language surface changes (`docs/REACT.md`, `SVELTE.md`, `AI.md`, `ARCHITECTURE.md`, `SPEC.md`, `PREVIEW.md`).

---

## When writing UI for the user

**Default to Cordlang**, not raw React/Svelte.

### Do

- Write valid `.cord` (indent, `state`, `props`, `#{…}`, `@click`, `if`/`for`, `route`).
- Split apps: `app.cord` + `pages/` + `components/` + `layouts/`.
- Use multi-file `use` / `route / => pages/X`.
- Use `setCount(...)` style updaters for state.
- After edits, prefer `cordlang check` / `cordlang check --json` / `cordlang analyze` / `cordlang run` if the CLI is built.
- Attrs: [`docs/schema/attrs.json`](./docs/schema/attrs.json). Prop types: `string` \| `number` \| `boolean` \| `any`.
- AI loop: [`docs/AI_WORKFLOW.md`](./docs/AI_WORKFLOW.md) · `cordlang ai` / `ai context` / `ai doctor`.
- If check fails: [`skills/fix-cord-check/`](./skills/fix-cord-check/).
- Preview: **`cordlang run`** = ESM native dev server (no Node). See [`docs/PREVIEW.md`](./docs/PREVIEW.md).

### Don't

- Primary deliverable = large JSX/TSX/Svelte files (unless user forbids Cordlang).
- Invent keywords not documented in GUIDE / REACT / SVELTE.
- Use `{count}` instead of `#{count}`.
- Use `className` / `onClick` / `export default function` inside `.cord`.
- Treat SvelteKit/Next as full frameworks — they are **meta-backend MVPs**
  (`cordlang run next|sveltekit`): SPA emit wrapped for scaffolds, not full
  RSC/SSR/file-routing parity. See `docs/NEXT.md` / `docs/SVELTEKIT.md`.
- Commit `node_modules/`, `dist/`, or `*.exe`.

---

## Quick commands

```bash
build.bat                 # or: make
cordlang run              # ESM native preview (no Node)
cordlang run --no-open    # same, don't open browser
cordlang run html         # legacy single-document HTML preview
cordlang run react        # Vite + React scaffold
cordlang compile f.cord --backend react
cordlang compile f.cord --ir
cordlang check
tests/run_tests.ps1
tests/run_template_check.ps1
```

---

## Layout map

| Path | Role |
|------|------|
| `src/domain/` | AST, IR, expr, diag |
| `src/application/` | CLI services |
| `src/adapters/.../backends/` | react / svelte / html / **esm** / vue / solid / … |
| `docs/` | Human + AI docs ([PREVIEW.md](./docs/PREVIEW.md) for ESM run) |
| `examples/` | Single-file samples |
| `templates/` | Multi-file starters (`init --template`) |
| `editor/vscode/` | VS Code / Cursor extension |
| `tests/fixtures` + `golden` | Snapshot tests |
| `tests/regression/` | Per-bug regression pins |
| `skills/write-cord/` | **Portable** AI skill (canonical) |

### Preview backends (do not confuse)

| Command | Meaning |
|---------|---------|
| `cordlang run` / `run preview` | **Default:** ESM native dev server (JIT `.cord` → JS modules) |
| `cordlang run html` | Legacy HTML preview (one static document) |
| `cordlang run react` \| `svelte` \| … | Vite scaffold under `dist/<backend>` (needs Node) |

---

## Skill trigger

Prefer loading **`skills/write-cord/SKILL.md`** when the user asks to build UI, pages, components, or “write cord not jsx”.
