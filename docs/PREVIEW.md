# Cordlang ↔ ESM native preview

Dev server embebido en el CLI (`cordlang run`). Cada `.cord` se compila **por
petición** a un módulo ES real; el navegador camina el grafo con `import`. Sin
Node, sin npm, sin bundler.

No es un claim de paridad con React/Svelte. El loop de preview por defecto es
`cordlang run`; el export estático es `cordlang build esm` → `dist/esm`
(mismo subset del runtime, sin HMR).

## Uso

```bash
cordlang init demo --template counter && cd demo
cordlang run              # http://127.0.0.1:4173 — abre el navegador
cordlang run --no-open    # mismo server, sin open
cordlang run preview      # alias de cordlang run
cordlang build esm        # export estático → dist/esm

cordlang run html         # preview legacy: un solo HTML estático
cordlang run react        # Vite + React (necesita Node)
cordlang compile src/app.cord --backend esm   # dump del emit a stdout
```

## Por qué ningún navegador parsea `.cord`

Igual que **ningún** navegador parsea `.vue` o `.svelte` nativos: el formato de
fuente no es JavaScript. El modelo funciona porque el **dev server** intercepta
las peticiones a `*.cord` y responde con:

```
Content-Type: text/javascript; charset=utf-8
```

y un cuerpo que **sí** es un ES module válido (`import` / `export`). El shell
carga la entry como Vite:

```html
<link rel="modulepreload" href="/@cord/runtime.js" />
<link rel="stylesheet" href="/@cord/styles.css" />
<script type="module" src="/src/app.cord"></script>
<!-- HMR client se carga idle (requestIdleCallback) -->
```

El browser pide ese URL; el server compila on-the-fly; los
`import Home from '/src/pages/Home.cord'` del grafo se resuelven igual.
La entry se **auto-monta** en `#app` (como `main.tsx` llama a `createRoot`).

Eso es el mismo truco que Vite con SFC: **el servidor es el compilador**.

## Mapa de URLs

| URL | Qué devuelve |
|-----|----------------|
| `/` | Shell HTML compacto (estilo Vite: `#app` + scripts `src=`) |
| `*.cord` | Módulo ES compilado JIT (`text/javascript`) — entry o componente |
| `/@cord/runtime.js` | Runtime embebido (`immutable` cache) |
| `/@cord/styles.css` | theme + base en un solo request |
| `/@cord/base.css` | CSS utilitario JIT (clases del proyecto) + reset/base |
| `/@cord/theme.css` | Tokens `theme` + `@font-face` nativos |
| `/@cord/fonts/<file>` | WOFF2 desde `~/.cordlang/cache/fonts` (`immutable`) |
| `/@cord/client` | Cliente SSE de reload (carga diferida) |
| `/@cord/hmr.js` | Alias de `/@cord/client` |
| `/@cord/hmr` | Canal SSE (`text/event-stream`); mensaje `reload` → `location.reload()` |
| estáticos | `public/**` primero (raíz del sitio), luego archivos del proyecto |
| SPA fallback | Paths **sin extensión** desconocidos → shell HTML (el router client-side) |
| 404 | Paths con extensión (asset inexistente) → `404` en texto plano |

Notas prácticas:

- `public/site.css` y `public/site.js` se enganchan en el shell si existen.
- Parse error en un `.cord` → módulo de error (HTTP 200) que pinta overlay vía
  `moduleError` — el browser sigue pudiendo ejecutar el grafo.
- Watch: al guardar `.cord`, `public/**` o `cordlang.json` el server manda SSE.
  Leaf `.cord` (no entry) → soft update (`update:` + remount). Entry / public /
  config / CSS global → `reload` (full page). **Best-effort**; no es HMR de Vite
  con preservación perfecta de estado.

## Forma del módulo emitido

Cada `.cord` → **un** ES module. Imports apuntan a otras URLs `.cord` (o al
runtime). El emit del dev server **no aplana** el proyecto: cada archivo se
parsea solo; `use` / `route` siguen siendo refs de módulo.

### Componente (cualquier `.cord` que no sea la entry)

```js
/* cordlang: source=src/components/Counter.cord */
import { h, frag, txt, keyed, component } from '/@cord/runtime.js';

export const Counter = component('Counter', function (props, $) {
  const { label = 'Counter' } = props;
  const [count, setCount] = $.state('count', 0);
  return h('div', { class: 'flex flex-col gap-16 p-24' }, [
    h('h1', { class: 'text-2xl font-bold' }, [(label)]),
    h('span', { class: 'text-4xl font-bold' }, [(count)]),
    h('button', {
      class: 'btn btn-primary',
      onClick: (e) => { setCount(count + 1); },
    }, ['+']),
  ]);
});

export default Counter;
```

### Entry (routes + theme)

```js
/* cordlang: source=src/app.cord */
import { h, frag, txt, keyed, component, mount } from '/@cord/runtime.js';
import Home from '/src/pages/Home.cord';
import Guide from '/src/pages/Guide.cord';
import DefaultLayout from '/src/layouts/default.cord';

export const theme = { 'primary': '#0c0f12', 'accent': '#3DFFB5', 'radius': 12 };

export const routes = [
  { path: '/', component: Home, layout: DefaultLayout },
  { path: '/guia', component: Guide, layout: DefaultLayout },
  { path: '/post/:slug', component: Post, layout: Docs },
];

const __cord_root = { __cord: 'app', routes: routes, theme: theme };
export default __cord_root;
if (typeof document !== 'undefined') {
  const __el = document.getElementById('app');
  if (__el) mount(__cord_root, __el);
}
```

El shell HTML (estilo Vite) solo referencia la entry:

```html
<script type="module" src="/@cord/client"></script>
<script type="module" src="/src/app.cord"></script>
```

Si el default export lleva `__cord: 'app'`, `mount` monta el `RouterRoot`.
Si es una función de componente, la monta directa (apps single-file sin
`route`).

`compile --backend esm` (sin server) emite **un** módulo aplanado del proyecto
entero — útil para goldens/inspección, no es lo que sirve el dev server.

## API del runtime (`/@cord/runtime.js`)

### VNodes / helpers

| Export | Rol |
|--------|-----|
| `h(type, props, children)` | Crear vnode (tag string o función componente) |
| `frag(children)` | Fragmento (varios nodos hermanos) |
| `txt(v)` | Nodo de texto |
| `keyed(key, node)` | Marca `key` para listas `for` (reconciliación) |
| `component(name, fn)` | Marca `fn.cordName = name` y devuelve `fn` |
| `mount(mod, el)` | Monta app o componente en `#app` / `el` |
| `navigate(to, opts?)` | `history.pushState` / `replaceState` + re-render del router |

### Objeto `$` (segundo argumento del componente)

Cada componente es `(props, $) => vnode`. `$` es la API por instancia:

| API | Significado |
|-----|-------------|
| `$.state(name, init)` | → `[value, setValue]`. Keyed por **nombre** (estable entre re-renders) |
| `$.effect(fn, deps?)` | Effect post-commit; cleanup = return de `fn` |
| `$.ref(init?)` | → `{ current }` |
| `$.resource(name, url, init?)` | `fetch` una vez por URL; slot `{ data, loading, error, url }` |
| `$.params()` | Params de la ruta activa (`:slug`, …) |
| `$.query()` | `URLSearchParams` de `location.search` |
| `$.path()` | Pathname actual del router |
| `$.navigate` | Alias de `navigate` |

El emit de `state count=0` baja a `const [count, setCount] = $.state('count', 0)`.
`params slug` → `const { slug } = $.params()`.

## Resolución de layout por ruta

Misma regla que el backend React, en el entry:

1. **`layout=` explícito** en la ruta (`route /guia => Guide layout=docs`)
2. Si no hay attr: layout cuyo basenombre es **`default`** (p.ej.
   `layouts/default.cord` / `layout default`)
3. Si no hay `default`: el **primer** layout declarado / importado que “parece”
   layout (path con `layout` / `Layout`)

Resultado en el array `routes`:

```js
{ path: '/guia', component: Guide, layout: Docs }
```

En runtime, `RouterRoot` hace `h(layout, {}, page)` o solo `page` si no hay
layout.

## Límites deliberados

El preview ESM es un **subconjunto** a propósito. No son bugs abiertos del
runtime; documentarlos evita que un agente “arregle” lo que no debe.

| Superficie Cord | Comportamiento en ESM |
|-----------------|------------------------|
| `errorBoundary` | Boundary local: captura throw en render de hijos; UI fallback |
| `portal` | Monta hijos en `document.body` (host `.cord-portal`) |
| `suspense` / `loading` | Wrapper mínimo; muestra `__fallback__` si `loading` |
| `icon` / `motion` / `chart` (presets) | SVG inline / CSS fade / axes placeholder — **sin** lucide/framer/recharts |
| HMR | Soft update best-effort en leaf `.cord`; full reload en entry/public/config. Estado puede perderse |
| `class=` en fuente | Las clases **custom** no generan CSS en el JIT. Usa utilidades Cord o **`public/site.css`** |
| `foreign` / `IR_FOREIGN` | Stub visible `cord-runtime-error` (sin npm) |

Otros matices:

- Rutas client-side (`pathname` + `pushState`), no file-based SSR.
- Un módulo no encontrado en el grafo → stub `cord-runtime-error` en el emit.
- Error de render en un componente → overlay + caja `cord-runtime-error`.
- `cordlang build esm` escribe `dist/esm/` (shell sin HMR + módulos `.js` + runtime).
  Mismo subset de preview; no es paridad React.
## `esm` vs `react` (y el resto)

| Comando | Cuándo usarlo |
|---------|----------------|
| `cordlang run` / `run preview` | Loop diario de UI: sin Node, arranque instantáneo, multi-archivo real, router y estado por componente |
| `cordlang run html` | Escape hatch legacy: un solo documento HTML estático + data-bind; componentes/rutas colapsan |
| `cordlang run react` | Scaffold Vite + React + Tailwind en `dist/react`. Paridad de presets (`icon`/`motion`/`chart`), ecosistema npm, build de prod |
| `cordlang run svelte` | Igual con Svelte 5 runes |
| `cordlang compile … --backend esm` | Inspeccionar / golden del emit ESM (aplanado o por fixture) |

Regla práctica:

- **Vibecode / AI loop** → `check` + `cordlang run` (este doc).
- **Capacidades de librería o ship a prod con React** → `run react` (o svelte/vue/solid).
- Preview HTML antiguo solo si necesitas snapshot offline single-file.

## Archivos

| Path | Rol |
|------|-----|
| `src/adapters/outbound/backends/esm/esm_ir.c` | Emit IR → ES module |
| `src/adapters/outbound/backends/esm/esm_runtime.c` | Runtime JS + shell HTML + soft-update client |
| `src/adapters/outbound/backends/esm/esm_css.c` | JIT `base.css` |
| `src/adapters/outbound/backends/esm/esm_backend.h` | API del backend |
| `src/adapters/outbound/backends/cord_class.c` | Mapeo attr → clase (compartido con otros backends) |
| `src/adapters/outbound/runtime/dev_server.c` | HTTP + SSE (`reload` / `update:`) |
| `src/application/preview_service.c` | Handler de URLs / compile por request / cache / `build esm` |
| `src/adapters/outbound/runtime/preview_server.c` | Server del **legacy** `run html` |

Regresiones: `tests/regression/esm-attr-not-identifier/`,
`tests/regression/esm-routes-layouts/`,
`tests/regression/esm-foreign-stub/`.

## Related

- Paridad React: [REACT.md](./REACT.md)
- Paridad Svelte: [SVELTE.md](./SVELTE.md)
- Meta Next/SvelteKit (wrappers SPA, no RSC/SSR completo): [NEXT.md](./NEXT.md) · [SVELTEKIT.md](./SVELTEKIT.md)
- Contrato IA / loop: [AI_CONTEXT.md](./AI_CONTEXT.md) · [AI_WORKFLOW.md](./AI_WORKFLOW.md)
- Arquitectura del compilador: [ARCHITECTURE.md](./ARCHITECTURE.md)
