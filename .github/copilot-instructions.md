# GitHub Copilot — Cordlang

This repository is **Cordlang**: dense `.cord` **language** for **vibecode / AI** (fewer tokens than JSX), compiled to React/Svelte/Vue/ESM. Deterministic `check` — no LLM in `compile`. The web **framework** product is **Runix** (`docs/RUNIX.md`) — do not invent it inside this CLI. Prefer `docs/AI_CONTEXT.md` if other docs sound like “another Next.”

## Prefer

- Edit or create **`.cord`** files under `src/`, `examples/`, `my-app/src/`.
- Follow `docs/AI_CONTEXT.md`, `docs/AI.md`, `docs/CHEATSHEET.md`, `AGENTS.md`, `docs/RUNIX.md`.
- Multi-file apps: thin `app.cord` + `pages/` + `components/` + `layouts/`.
- Keep sources dense — token cost matters.

## Avoid

- Generating large JSX/Svelte as the main answer when Cordlang can express the UI.
- Inventing syntax (`{count}`, `onClick=`, `className=` inside `.cord`).
- Treating `dist/` as hand-maintained source.
- Treating Cordlang as the SEO/SSR framework (that is Runix) or expanding meta backends unless asked.

## After UI changes

Suggest: `cordlang check`, `cordlang run`, `cordlang run react|svelte|vue`.
