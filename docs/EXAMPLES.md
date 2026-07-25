# Examples catalog

All paths relative to the repo root. Compile with:

```bash
cordlang compile <file> --backend react
cordlang compile <file> --backend svelte
cordlang compile <file> --ir
```

## Single-file (`examples/`)

| File | What it shows |
|------|----------------|
| [`counter.cord`](../examples/counter.cord) | `def`, `state`, `props`, buttons, `#{…}` |
| [`interp.cord`](../examples/interp.cord) | Interpolation edge cases |
| [`routes.cord`](../examples/routes.cord) | Basic routing sketch |
| [`shop.cord`](../examples/shop.cord) | Shop-ish UI sample |
| [`react_hooks.cord`](../examples/react_hooks.cord) | Context, effects, hooks surface |
| [`react_advanced.cord`](../examples/react_advanced.cord) | Advanced React patterns |
| [`phase_b_lazy_head.cord`](../examples/phase_b_lazy_head.cord) | Lazy routes + document title |
| [`fetch_form.cord`](../examples/fetch_form.cord) | `fetch` + form `action` + `bind` |
| [`phase_d_hooks.cord`](../examples/phase_d_hooks.cord) | Phase D: insertionEffect, externalStore, forwardRef, action |
| [`phase_e_svelte.cord`](../examples/phase_e_svelte.cord) | Phase E: store, await, snippet, use:, transition, portal |
| [`svelte_ctx_params.cord`](../examples/svelte_ctx_params.cord) | Context + route `:id` params |
| [`public/api/products.json`](../examples/public/api/products.json) | Static API for fetch demos |

## Multi-file starters (`templates/`)

| Path | Role |
|------|------|
| [`templates/counter/`](../templates/counter/) | Minimal multi-page + Counter component |
| [`templates/landing/`](../templates/landing/) | Marketing landing |
| [`templates/dashboard/`](../templates/dashboard/) | Shell layout + settings |
| [`templates/docs-shell/`](../templates/docs-shell/) | Docs navigation shell |
| [`templates/form-fetch/`](../templates/form-fetch/) | Form + fetch sample |

```bash
cordlang init demo --template counter
cd demo
cordlang run
cordlang run react --check
cordlang check
cordlang symbols
```

## Golden fixtures (`tests/fixtures/`)

Used by CI for codegen snapshots — good minimal reproductions:

| Fixture | Focus |
|---------|--------|
| `basic_counter.cord` | state + UI |
| `if_for.cord` | if + for + key |
| `interp.cord` | interpolation |
| `routes_simple.cord` | layout + routes |
| `nested_routes.cord` | nested layout= |
| `react_phase_d.cord` | advanced React hooks |
| `svelte_phase_e.cord` | advanced Svelte surface |
| `unknown_comp.cord` | `cordlang check` must fail |

```bash
cordlang compile tests/fixtures/basic_counter.cord --backend react
# compare to tests/golden/basic_counter.react.txt
```
