# Backend tiers

Single source of truth for Cordlang **compile target** status. If README / ROADMAP / CLI help disagree, **this file wins** for tiers.

**Product split:** Cordlang = language + these backends. The web **framework** product is **[Runix](./RUNIX.md)** — not a new row in this table.

## Tiers

| Tier | Backends | Meaning |
|------|----------|---------|
| **Official** | `esm` / `preview`, `react`, `svelte`, `vue` | AI / language contract. Goldens + template `--check` + preview smoke. |
| **Experimental / meta** | `solid`, `html` (legacy), `email`, `pdf`, `next`, `sveltekit` | Useful, not the default AI loop. Soft / smoke only. |

Native (Flutter / SwiftUI / Compose) is **experimental** and not a registered CLI backend — see [NATIVE.md](./NATIVE.md). WASM playground MVP ships under `playground/` — see [PLAYGROUND.md](./PLAYGROUND.md).

**Freeze:** do not add new CLI backends until multi-file WASM / registry epics need them. Do not add “Runix” as a backend name here — Runix is a separate product surface. Close half-done work first.

## Feature matrix (honesty, not marketing)

| Feature | esm | react | svelte | vue |
|---------|-----|-------|--------|-----|
| state / props / if / for / events | yes | yes | yes | yes |
| routes / layouts | yes | yes | yes | yes |
| context provide/ctx | yes | yes | yes | yes |
| forms / actions | partial | yes | yes | partial |
| assets / fonts / theme | yes | yes | yes | yes |
| `check` / `analyze` | yes | yes | yes | n/a (source-level) |
| scaffold Vite `--check` | n/a (native) | yes (CI) | yes (CI) | yes (CI) |
| presets icons/motion/charts | stub in preview | yes | yes | yes |

## Meta wrappers (frozen claims)

| Backend | Reality |
|---------|---------|
| `next` | SPA React emit inside a Next App Router shell. **Not** RSC / SSR / file-routing parity. [NEXT.md](./NEXT.md) |
| `sveltekit` | SPA Svelte emit inside a Kit page. **Not** SSR/SSG/file-routing parity. [SVELTEKIT.md](./SVELTEKIT.md) |

## Tests

| Script | Role |
|--------|------|
| `tests/run_tests.ps1` / `.sh` | Goldens: react, svelte, **vue**, solid |
| `tests/run_template_check.ps1` | Official SPA: react + svelte + **vue** `--check` |
| `tests/run_preview_smoke.ps1` | Official ESM: `run --smoke` |
| `tests/run_backend_parity.ps1` / `.sh` | Prints tiers + compile smoke; Official must pass |

## CLI one-liners

```text
Official:   preview|esm, react, svelte, vue
Meta:       solid, html, email, pdf, next, sveltekit
```
