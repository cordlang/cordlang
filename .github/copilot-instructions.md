# GitHub Copilot — Cordlang

This repository is **Cordlang**: dense `.cord` UI for **vibecode / AI** (fewer tokens than JSX), compiled to React/Svelte/HTML. Deterministic `check` — no LLM in `compile`. Prefer AI_CONTEXT.md if other docs sound like “another framework.”

## Prefer

- Edit or create **`.cord`** files under `src/`, `examples/`, `my-app/src/`.
- Follow `docs/AI_CONTEXT.md`, `docs/AI.md`, `docs/CHEATSHEET.md`, `AGENTS.md`.
- Multi-file apps: thin `app.cord` + `pages/` + `components/` + `layouts/`.
- Keep sources dense — token cost matters.

## Avoid

- Generating large JSX/Svelte as the main answer when Cordlang can express the UI.
- Inventing syntax (`{count}`, `onClick=`, `className=` inside `.cord`).
- Treating `dist/` as hand-maintained source.
- Prioritizing meta backends (Vue/Solid/Next/Kit) over the React/Svelte AI loop unless asked.

## After UI changes

Suggest: `cordlang check`, `cordlang run`, `cordlang run react|svelte`.
