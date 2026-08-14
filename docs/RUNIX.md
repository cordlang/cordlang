# Runix — product framework (vision)

**Cordlang** is the **language** (this repository): dense `.cord` → IR → compile targets.  
**Runix** is the **framework** product built *on* Cordlang — web-first (SEO, runtime, deploy DX), not “another emit backend” inside this CLI.

## Split (non-negotiable)

| | Cordlang | Runix |
|---|----------|--------|
| What | Intermediate UI language + compiler | Opinionated web framework |
| Who | Vibecode / AI agents write `.cord` | Apps ship on Runix |
| Repo (today) | **This repo** (`cordlang/cordlang`) | Separate product (planned) |
| Owns | Syntax, IR, `check`/`analyze`, Official emit (ESM / React / Svelte / Vue), playground WASM | Routing/SSR/SEO/runtime/hosting opinions optimized for the web |

Cordlang **does not** become “the framework.” Existing CLI targets (`cordlang run`, `run react|svelte|vue`, meta Next/Kit) remain **compile/preview backends**, not the Runix product.

## How they relate

```
AI / humans  ──write──►  .cord  (Cordlang language)
                              │
                              ├─► cordlang check / analyze / compile
                              │
                              ├─► Official backends (React / Svelte / Vue / ESM preview)
                              │     = interop & preview while Runix matures
                              │
                              └─► Runix (framework)  = primary web product surface
                                    SEO · runtime · deploy · first-class .cord apps
```

## Implications for this repo

1. Prefer language / IR / DX work over inventing a full web framework here. The WASM playground is a language surface (M11 done), not a Runix host.
2. Do not market Cordlang as Next/Vite/router replacement — that story belongs to **Runix**.
3. WASM multi-file + CI artifacts support the **language** playground; Runix may host or wrap that later.
4. Meta backends stay frozen ([`BACKENDS.md`](./BACKENDS.md)); Runix is not a new CLI `--backend`.

## Status

**Vision documented.** Implementation of Runix lives outside Cordlang’s language north star until a dedicated Runix track exists. Until then: write `.cord`, use Official backends for preview/interop.

See also: [`AI_CONTEXT.md`](./AI_CONTEXT.md) · [`ROADMAP.md`](./ROADMAP.md) · [`LANGUAGE.md`](./LANGUAGE.md).
