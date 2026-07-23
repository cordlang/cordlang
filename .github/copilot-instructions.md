# GitHub Copilot — Cordlang

This repository is **Cordlang**: write UI in `.cord`, compile to React/Svelte/HTML.

## Prefer

- Edit or create **`.cord`** files under `src/`, `examples/`, `my-app/src/`.
- Follow `docs/AI.md`, `docs/CHEATSHEET.md`, `AGENTS.md`.
- Multi-file apps: thin `app.cord` + `pages/` + `components/` + `layouts/`.

## Avoid

- Generating large JSX/Svelte as the main answer when Cordlang can express the UI.
- Inventing syntax (`{count}`, `onClick=`, `className=` inside `.cord`).
- Treating `dist/` as hand-maintained source.

## After UI changes

Suggest: `cordlang check`, `cordlang run`, `cordlang run react|svelte`.
