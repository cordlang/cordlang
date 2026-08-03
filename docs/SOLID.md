# Cordlang ↔ SolidJS

**Tier: Experimental** — not Official. Tiers: [BACKENDS.md](./BACKENDS.md).

Mapeo del modelo mental de [Solid](https://www.solidjs.com/) al lenguaje `.cord`.
Cordlang compila a **JSX** + Vite (`vite-plugin-solid`) + Tailwind +
[`@solidjs/router`](https://github.com/solidjs/solid-router).

## Uso

```bash
cordlang run solid
cd dist/solid
npm install
npm run dev

cordlang run solid --check
cordlang compile src/app.cord --backend solid
```

## Tabla de mapeo

| Cordlang | SolidJS |
|----------|---------|
| `state n=0` | `const [n, setN] = createSignal(0)` (lectura `n()` en JSX) |
| `props title=""` | props del componente (`{ title = "…" }`) |
| `computed x=…` | `const x = createMemo(() => …)` |
| `effect` | `createEffect(() => { … })` |
| elementos + estilo | JSX + `class="…"` (Tailwind) |
| `@click=…` | `onClick={…}` |
| `bind=x` | binding controlado (signal + onInput) |
| `if` / `for` | condicionales / `.map` en JSX (como React IR) |
| `route` + `layout`/`slot` | `<Router>` / `<Route component={…}>` + `<Outlet />` |
| `context` / `provide` / `ctx` | `createContext` / `Provider` / `useContext` |
| `theme` | `theme.css` vía `theme_css_generate_from_ir` |
| `link to=…` | `<Link href="…">` |

## Archivos generados

- `dist/solid/` — scaffold Vite (`vite-plugin-solid`, `solid-js`, `@solidjs/router`)
- Módulos: `src/**/*.jsx`
- Extension de puerto: `.jsx`
- `needs_node_check=1` (`--check` / `--watch`)

## Notas

- Codegen camina **solo** `IrNode` (`solid_generate_from_ir` / `solid_ir.c`).
- Los identificadores de signal en JSX/handlers se reescriben a `name()` automáticamente.
- No hay LLM en el path de compile.
