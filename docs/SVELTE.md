# Cordlang ↔ Svelte

Mapeo del modelo mental de [Svelte](https://svelte.dev/docs/svelte/overview) al
lenguaje `.cord`. Cordlang **no es un subset de Svelte**: es una capa compacta
que se compila a **Svelte 5 (runes)** + Vite + Tailwind + hash router, con
prácticas idiomáticas de la documentación oficial.

Referencias oficiales usadas:

- [Overview](https://svelte.dev/docs/svelte/overview)
- [What are runes?](https://svelte.dev/docs/svelte/what-are-runes)
- [`$state`](https://svelte.dev/docs/svelte/$state) · [`$derived`](https://svelte.dev/docs/svelte/$derived) · [`$effect`](https://svelte.dev/docs/svelte/$effect)
- [`$props`](https://svelte.dev/docs/svelte/$props) · [`$bindable`](https://svelte.dev/docs/svelte/$bindable)
- [Logic blocks](https://svelte.dev/docs/svelte/if) (`if` / `each` / `await` / `key` / `snippet`)
- [`use:` actions](https://svelte.dev/docs/svelte/use) · [transitions](https://svelte.dev/docs/svelte/transition)
- [`svelte` runtime](https://svelte.dev/docs/svelte/svelte) (`setContext`, `getContext`, `onMount`, …)
- [Stores](https://svelte.dev/docs/svelte/stores) (legacy / compartido; preferir runes)

---

## Uso

```bash
cordlang run svelte
cd dist/svelte
npm install
npm run dev

# o check de producción
cordlang run svelte --check
```

---

## 1. Componentes (`.svelte`)

Un componente Svelte es un archivo con `<script>`, markup y opcional `<style>`.
Cordlang emite lo mismo desde `def` / archivos `.cord`.

| Svelte | Cordlang |
|--------|----------|
| `MyCard.svelte` | `def MyCard` o `components/MyCard.cord` |
| markup HTML | tags indentados (`col`, `btn`, `p`, …) |
| `{@render children?.()}` | `slot` |
| `import X from './X.svelte'` | `use ./components/X` / multi-file project |

```cord
# components/Card.cord
props title=""
col p=16
  h3 "#{title}" bold
  slot
```

→

```svelte
<script>
  let { title = '', children } = $props();
</script>

<div class="...">
  <h3 class="font-bold">{title}</h3>
  {@render children?.()}
</div>
```

---

## 2. Runes de reactividad

### `$state` — [docs](https://svelte.dev/docs/svelte/$state)

| Svelte | Cordlang |
|--------|----------|
| `let count = $state(0)` | `state count=0` |
| actualizar: `count = count + 1` | en handlers: `setCount(count + 1)` helper emitido, o reasignación en expr |

```cord
state count=0
btn "+" @click=setCount(count + 1)
p "#{count}"
```

→

```svelte
<script>
  let count = $state(0);
  function setCount(v) { count = typeof v === 'function' ? v(count) : v; }
</script>
<button onclick={() => setCount(count + 1)}>+</button>
<p>{count}</p>
```

> **Nota:** en Svelte nativo se escribe `count++`; Cordlang emite helpers `setX`
> para compartir la misma forma de handlers con el backend React.

### `$derived` — [docs](https://svelte.dev/docs/svelte/$derived)

| Svelte | Cordlang |
|--------|----------|
| `let doubled = $derived(count * 2)` | `computed doubled = count * 2` |

### `$effect` / `$effect.pre` — [docs](https://svelte.dev/docs/svelte/$effect)

| Svelte | Cordlang |
|--------|----------|
| `$effect(() => { … })` | `effect` + body indentado o `run=` |
| `$effect.pre(() => { … })` | `layoutEffect` (antes del paint; paridad con `useLayoutEffect`) |
| cleanup `return () => …` | `cleanup=expr` (cuando el parser lo soporta) |

```cord
effect
  console.log(count)
layoutEffect
  measure(el)
```

### `$props` — [docs](https://svelte.dev/docs/svelte/$props)

| Svelte | Cordlang |
|--------|----------|
| `let { title = '' } = $props()` | `props title=""` |
| rest props | *(escape en dist / roadmap)* |
| `$props.id()` | `id fieldId` → `const fieldId = $props.id()` |

### `$bindable` — [docs](https://svelte.dev/docs/svelte/$bindable)

| Svelte | Cordlang |
|--------|----------|
| `let { value = $bindable() } = $props()` | `props value bindable` *(parcial / roadmap)* |
| padre: `bind:value={x}` | en hijo DOM: `bind=x` ya emite `bind:value={x}` |

---

## 3. Bloques lógicos

Documentación: [if](https://svelte.dev/docs/svelte/if) · [each](https://svelte.dev/docs/svelte/each) · [await](https://svelte.dev/docs/svelte/await) · [snippet](https://svelte.dev/docs/svelte/snippet)

| Svelte | Cordlang |
|--------|----------|
| `{#if cond}…{:else}…{/if}` | `if cond` / `else` |
| `{#each items as item (key)}` | `for item in items key=item.id` |
| `{#await p}…{:then v}…{:catch e}…{/await}` | `await p then=v` + bloques `loading` / `error` |
| `{#snippet name(args)}…{/snippet}` | `snippet name(args)` |
| `{@render name(args)}` | `render name(args)` |

### Await

```cord
await loadProducts() then=products
  p "Got #{products}"
  loading
    p "Loading…" muted
  error err
    p "Fail #{err}" muted
```

→

```svelte
{#await loadProducts()}
  <p class="muted">Loading…</p>
{:then products}
  <p>Got {products}</p>
{:catch err}
  <p class="muted">Fail {err}</p>
{/await}
```

### Snippets

```cord
snippet card(title)
  col p=8
    h3 "#{title}"

render card("Hello")
```

→

```svelte
{#snippet card(title)}
  <div class="..."><h3>{title}</h3></div>
{/snippet}

{@render card("Hello")}
```

---

## 4. Eventos y formularios

| Svelte | Cordlang |
|--------|----------|
| `onclick={fn}` | `@click=fn` |
| `onclick={() => count++}` | `@click=setCount(count + 1)` |
| `bind:value={name}` | `bind=name` |
| `bind:this={el}` | `ref=el` (+ `ref el`) |
| `onsubmit` + `preventDefault` | `form @submit=handler` o `form action=formAction` |

### Form actions productivos

```cord
action form = submitForm init=null pending=saving
form action=formAction
  input text bind=email name=email
  btn "Send" variant=primary
```

→ `$state` + `async function formAction(e)` con `FormData` (progressive enhancement
estilo SPA; no es SvelteKit `+page.server`).

---

## 5. Element directives (Svelte template)

### Actions — [`use:`](https://svelte.dev/docs/svelte/use)

> En Svelte 5 las *actions* clásicas siguen funcionando; *attachments* (`@attach`)
> son el camino nuevo. Cordlang emite `use:` por compatibilidad y simplicidad.

| Svelte | Cordlang |
|--------|----------|
| `use:tooltip` | `use=tooltip` |
| `use:tooltip={opts}` | `use=tooltip(opts)` (si el valor es llamada) |

```cord
div use=tooltip
  p "hover me"
```

→ `<div use:tooltip>…</div>`

> **Conflicto:** `form action=formAction` es el *handler de submit* (no `use:`).
> Las element actions usan siempre el attr **`use=`**.

### Transitions / animations — [transition](https://svelte.dev/docs/svelte/transition)

| Svelte | Cordlang |
|--------|----------|
| `transition:fade` | `transition=fade` |
| `in:fly out:fade` | `in=fly out=fade` |
| `animate:flip` | `animate=flip` |

Imports automáticos desde `svelte/transition` / `svelte/animate` cuando se usan
nombres conocidos (`fade`, `fly`, `slide`, `scale`, `blur`, `draw`, `crossfade`, `flip`).

---

## 6. Context API — [setContext / getContext](https://svelte.dev/docs/svelte/svelte#setContext)

| Svelte | Cordlang |
|--------|----------|
| `setContext(key, value)` | `provide Key value=expr` |
| `getContext(key)` | `ctx var = Key` |
| key module | `context Theme = "light"` → `src/contexts.js` (`Symbol`) |

```cord
context Theme = "light"

def Shell
  state theme="dark"
  provide Theme value=theme
    slot

def Child
  ctx theme = Theme
  p "Tema: #{theme}"
```

Genera `import { setContext, getContext } from 'svelte'` y keys en `contexts.js`.

Context solo fluye **padre → hijo** (poner `provide` en layout para app-wide).

---

## 7. Portal / attach DOM

| Svelte | Cordlang |
|--------|----------|
| action que mueve el nodo a `document.body` | `portal to=document.body` |
| (futuro) `@attach` | roadmap |

```cord
portal to=document.body
  col p=24
    h3 "Modal"
```

→ wrapper con `use:portal={document.body}` + helper `portal` emitido en el módulo.

---

## 8. Stores — [svelte/store](https://svelte.dev/docs/svelte/stores)

En Svelte 5 se prefieren runes (`$state` compartido en `.svelte.js`). Los stores
siguen siendo válidos para estado **compartido clásico** o libs externas.

| Svelte | Cordlang |
|--------|----------|
| `writable(0)` | `store count = 0` o `writable count = 0` |
| `{$count}` en markup | `#{count}` (auto `$` si es store) |

```cord
store count = 0
p "#{count}"
btn "+" @click=count.update(n => n + 1)
```

→

```svelte
<script>
  import { writable } from 'svelte/store';
  const count = writable(0);
</script>
<p>{$count}</p>
```

---

## 9. Routing (ecosistema; Svelte no trae router built-in)

Cordlang usa un **hash router mínimo** (sin deps), análogo a React Router en el
backend React:

| Concepto | Cordlang → Svelte |
|----------|-------------------|
| `route / => pages/Home` | mapa `routes` en `App.svelte` |
| `layout` + `slot` | layout con `children` + `{@render children?.()}` |
| `link to=/x` | `<a href="#/x">` |
| `params id` | props desde `matchRoute` |
| `lazy Page = pages/Heavy` | `() => import(...)` + `{#await}` |
| **SvelteKit** file routing / SSR | roadmap backend `kit` (Fase E6) |

---

## 10. Special elements

| Svelte | Cordlang |
|--------|----------|
| `<svelte:head>` | `title "…"` / `head` |
| `<svelte:window>` / `document` / `body` | *(roadmap)* |
| `<svelte:boundary>` (error) | `errorBoundary` + `fallback` *(parcial)* |
| `<svelte:element this={tag}>` | *(roadmap)* |

---

## 11. Fetch / data loading

| Patrón Svelte | Cordlang |
|---------------|----------|
| `$effect` + `fetch` | `fetch products = "/api/products.json"` |
| `{#await fetch(...)}` | `await fetch("/api/…") then=data` |
| SvelteKit `load` | roadmap (backend kit) |

Emite: `products` / `productsLoading` / `productsError` como `$state`.

---

## 12. Checklist vs documentación Svelte

### Introduction / overview

- [x] Componentes `.svelte` multi-archivo  
- [x] Markup + script  
- [x] Eventos  
- [x] Estilos vía utilidades / theme CSS vars  
- [ ] Scoped `<style>` arbitrario en `.cord`  

### Runes

- [x] `$state`  
- [x] `$derived`  
- [x] `$effect`  
- [x] `$effect.pre` (`layoutEffect`)  
- [x] `$props`  
- [ ] `$bindable` completo  
- [x] `$props.id()` (`id name`)  
- [ ] `$host` / `$inspect`  

### Logic blocks

- [x] `{#if}` / `{:else}`  
- [x] `{#each}` + key  
- [x] `{#await}`  
- [x] `{#snippet}` / `{@render}`  
- [ ] `{#key}`  

### Directives

- [x] `bind:value` / `bind:this`  
- [x] `use:` actions  
- [x] `transition:` / `in:` / `out:` / `animate:`  
- [ ] `class:` / `style:` directives avanzadas  

### Runtime (`svelte`)

- [x] `setContext` / `getContext`  
- [ ] `hasContext` / `createContext` helpers  
- [ ] `onMount` / `onDestroy` explícitos (parcial vía `$effect`)  
- [ ] `tick` / `untrack`  
- [x] `mount` en scaffold (`main.js`)  

### Stores

- [x] `writable` básico (`store` / `writable`)  
- [ ] `readable` / `derived` store / `readonly`  

### Meta

- [ ] **SvelteKit** (SSR, file routes, form actions server)  
- [x] Assets `public/`  
- [x] Theme tokens → CSS vars  

---

## 13. Estructura generada

```
dist/svelte/
  package.json
  vite.config.js
  svelte.config.js   # compilerOptions.runes: true
  public/            # copiado del project public/
  src/
    App.svelte       # hash router + layout
    main.js          # mount(App, { target })
    app.css
    theme.css        # si hay theme
    contexts.js      # si hay context *
    pages/*.svelte
    components/*.svelte
    layouts/*.svelte
```

---

## 14. Qué no se traduce 1:1 (por diseño)

| Tema Svelte | Enfoque Cordlang |
|-------------|------------------|
| Lógica JS arbitraria | expr en handlers / hooks; JS libre en `dist/` |
| SvelteKit full stack | backend futuro `kit`, no el CLI SPA actual |
| Legacy Svelte 4 (`export let`, `$:` ) | **solo runes** (Svelte 5) |
| Class components / custom elements | fuera de scope |
| Attachments `@attach` | preferir `use:`; migrate después |

---

## Ejemplo kitchen-sink

Ver `examples/phase_e_svelte.cord`, `examples/svelte_ctx_params.cord`,
`examples/fetch_form.cord` y `my-app/src/**/*.cord`.

Roadmap: [`docs/ROADMAP.md`](./ROADMAP.md) — Fase E.
