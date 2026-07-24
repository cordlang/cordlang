# Changelog

All notable changes to the Cordlang **CLI / tooling** are documented here.
Language surface versions follow [`docs/SPEC.md`](./docs/SPEC.md) and [`docs/VERSIONING.md`](./docs/VERSIONING.md).

## [1.0.0] — 2026-07

### Added
- SPEC 1.0 language freeze (SPA subset) and CLI `1.0.0`
- IR backends: React, Svelte 5, Vue 3, Solid, HTML preview, email/PDF static, Next/SvelteKit meta
- DX: `check`, `fmt`, `analyze`, `symbols`/`goto`, LSP, `init --template`, local `add`
- CI: Windows + Ubuntu goldens, ASAN with leak detection, template/example smoke

### Fixed
- Parser `token_str` / `node_create` double-copy leaks (`node_adopt`)
- Unmerged `NODE_USE` nodes dropped without free in module merge
- Theme CSS emission rejects keys/values that could break out of custom properties
