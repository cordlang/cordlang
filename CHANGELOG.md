# Changelog

All notable changes to the Cordlang **CLI / tooling** are documented here.
Language surface versions follow [`docs/SPEC.md`](./docs/SPEC.md) and [`docs/VERSIONING.md`](./docs/VERSIONING.md).

## [Unreleased]

### Fixed — Overlay CSS + preview logs
- **Overlay CSS** — `/@cord/styles.css` still serves base+overlay when the project does not compile; runtime also injects critical overlay styles
- **Preview terminal** — colored `info`/`err`/`ok` tags, banner, deduped compile errors (no spam per request)

### Added — Preview DX fixes
- **Unterminated strings** — lexer errors on missing `"` (no silent swallow); overlay shows file:line
- **Watch reload** — full page reload only (no stacked `?hmr=`/`?v=` module graphs in DevTools)

### Added — Fase R5: native error overlay (`cordlang run`)
- Full-screen compile/runtime overlay (`showErrorOverlay` / `showCompileError`) with `file:line:col`, message, `code`/`hint`, and source excerpt
- Parser spans (`error_line`/`error_col`) propagated through `CompileResult`
- ESM JIT runs `check_service_on_ast` per `.cord`; failures emit structured error modules (HTTP 200, `no-store`)
- Docs: ROADMAP R5, PREVIEW.md overlay contract; smoke covers parse + `jsx-attr` paths

### Added — Fase R: ESM preview perfection
- **R1 DX/perf** — in-process emit cache (mtime+bust), watch `public/**`, stderr on import truncation / parse errors, `cordlang run --smoke`, docs drift (GUIDE/LIBRARIES/skills)
- **R2 Soft HMR** — SSE `update:/path.cord` remounts entry (import bust `?v=`); entry/public/config → full `reload`
- **R3 `cordlang build esm`** — static tree under `dist/esm/` (no Node, no HMR client)
- **R4 Runtime** — `ErrorBoundary` / `Portal` / `Suspense`; presets icon (SVG) / motion (CSS fade) / chart (axes stub)

### Added — ESM native preview (`cordlang run`)
- **ESM backend** (`src/adapters/outbound/backends/esm/`): one ES module per `.cord`, JIT-compiled by the embedded dev server (no Node / npm / bundler). Docs: [`docs/PREVIEW.md`](./docs/PREVIEW.md)
  - `esm_ir.c` — IR → module emit (`component` modules + entry with `routes` / `theme`)
  - `esm_runtime.c` — client runtime (`h`, `frag`, `txt`, `keyed`, `component`, `mount`, `navigate`, `$` hooks) + shell HTML + HMR client
  - `esm_css.c` — utility `base.css` JIT for classes used by the project
- **Shared attr→class mapping** — `backends/cord_class.{c,h}` (react / svelte / vue / solid stay byte-identical)
- **Module resolution API** — `compiler_port` helpers for multi-file `use` / route refs without flattening
- **Dev server + SSE reload** — `runtime/dev_server.{c,h}` + `preview_service` URL map (`/`, `*.cord`, `/@cord/*`, statics, SPA fallback)
- **CLI** — `cordlang run` / `run preview` = ESM native server; `run html` = legacy single-document preview; `run --no-open` skips browser open
- **IR_FOREIGN stubs** — `foreign Chart from "recharts"` (etc.) emit a visible `cord-runtime-error` stub component; no npm import in the ESM preview
- Regressions: `tests/regression/esm-attr-not-identifier/`, `tests/regression/esm-routes-layouts/`, `tests/regression/esm-foreign-stub/`

### Fixed
- **esm-attr-not-identifier** — bare attr words (`purpose=action`, `type=button`, `id=forma`) no longer emit as free JS identifiers; scope tracking only treats declared names as expressions
- **esm-routes-layouts** — `IR_ROUTE` path/target + layout resolution (explicit `layout=` → layout named `default` → first declared layout) correctly populate `export const routes`
- **esm-foreign-stub** — `IR_FOREIGN` no longer leaves a dangling PascalCase identifier; binds a stub with class `cord-runtime-error` (no package resolve)
- **Test harness** — `tests/run_tests.ps1` fixed (full suite green again)
- **Portability** — `clock()` → `now_ms()` in `dev_server.c` (non-Windows builds)

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
