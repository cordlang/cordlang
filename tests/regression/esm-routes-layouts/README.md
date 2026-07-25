# esm-routes-layouts

**Symptom.** `cordlang run` served an entry module with `export const routes = []`.
The router matched nothing, so every URL rendered the 404 view even though the
project declared nine routes.

**Cause.** `ir_from_ast` maps `NODE_ROUTE` to `IR_ROUTE` with the URL in `name`
and the target in `value` (see the comment in `src/domain/ir.c`). The ESM emitter
read `value` / `value2`, so the target came back NULL and every route was skipped.

**Fix.** Read `name` for the path and `value` for the target, matching the React
backend.

This pin locks the whole route surface:

- explicit `layout=docs` resolves to the `Docs` layout;
- a route with no `layout=` falls back to the layout named `default`
  (`DefaultLayout`), same rule as `find_layout_for_route_ir` in React;
- `slot` inside a layout lowers to `props.children`;
- `:slug` reaches the page through `$.params()`;
- `theme` is exported as tokens for the generated theme.css.
