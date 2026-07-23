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

## Multi-file demo (`my-app/`)

| Path | Role |
|------|------|
| [`my-app/src/app.cord`](../my-app/src/app.cord) | theme + routes |
| [`my-app/src/layouts/default.cord`](../my-app/src/layouts/default.cord) | shell + slot |
| [`my-app/src/pages/*`](../my-app/src/pages/) | pages |
| [`my-app/src/components/*`](../my-app/src/components/) | Counter, ProductCard |
| [`my-app/public/`](../my-app/public/) | static assets |

```bash
cd my-app
../cordlang.exe run
../cordlang.exe run react --check
../cordlang.exe run svelte --check
../cordlang.exe check
../cordlang.exe symbols
../cordlang.exe goto Counter
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
