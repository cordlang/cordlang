# Cordlang ↔ Vue 3

**Tier: Official** — SPA target co-equal with React/Svelte for the AI loop. Tiers: [BACKENDS.md](./BACKENDS.md).

Mapeo del modelo mental de [Vue 3](https://vuejs.org/) al lenguaje `.cord`.
Cordlang compila a **Vue 3 SFCs** (`<script setup>` + `<template>`) + Vite +
Tailwind + [vue-router](https://router.vuejs.org/) (hash history).

**Not Nuxt.** No SSR, no file-based routing, no Nuxt modules — same honesty bar as
Next/Kit SPA wrappers ([NEXT.md](./NEXT.md) / [SVELTEKIT.md](./SVELTEKIT.md)).

## Uso

```bash
cordlang run vue
cd dist/vue
npm install
npm run dev

cordlang run vue --check
cordlang compile src/app.cord --backend vue
```

## Tabla de mapeo

| Cordlang | Vue 3 |
|----------|-------|
| `state n=0` | `const n = ref(0)` + `setN` helper |
| `props title=""` | `defineProps({ title: { default: … } })` |
| `computed x=…` | `const x = computed(() => …)` |
| `effect` | `watchEffect(() => { … })` |
| elementos + estilo | `<template>` + clases Tailwind |
| `@click=…` | `@click="…"` |
| `bind=x` | `v-model="x"` |
| `if` / `for` | `<template v-if>` / `<template v-for>` |
| `route` + `layout`/`slot` | `vue-router` + `<RouterView>` / `<slot />` |
| `context` / `provide` / `ctx` | `provide` / `inject` |
| `theme` | `theme.css` vía `theme_css_generate_from_ir` |
| `link to=…` | `<router-link :to="…">` |
| `fetch` | `$effect`-style load in script (same IR as SPA peers) |
| `title "…"` | `document.title` effect |
| `lazy` routes | dynamic `() => import(…)` in router |
| `transition=` / `in=` / `out=` | Vue `<Transition>` when mapped; else plain |
| presets `icon` / `motion` / `chart` | `CordIcon.vue` / `CordMotion.vue` / `CordChart.vue` |

## Archivos generados

- `dist/vue/` — scaffold Vite (`@vitejs/plugin-vue`, `vue`, `vue-router`)
- Componentes: `src/**/*.vue`
- Extension de puerto: `.vue`
- `needs_node_check=1` (`--check` / `--watch`)

## Presets

When `cordlang.json` lists `"presets": ["icons", …]`:

| Capability | Vue bridge | npm |
|------------|------------|-----|
| `icons` | `src/CordIcon.vue` | `lucide-vue-next` |
| `motion` / `charts` | thin CSS/SVG bridges | no heavy deps |

```bash
cordlang preset add icons
cordlang run vue --check
# package.json must list lucide-vue-next
```

See [LIBRARIES.md](./LIBRARIES.md).

## Limits (honest)

- **Hash router** only (`createWebHashHistory`) — not Vue Router history mode / SSR.
- **Not Nuxt** — no `pages/` file routing, no server routes, no Nuxt modules.
- Forms / actions: SPA-level bind + handlers; no Vue Server Actions claim.
- Idiomatic Vue-only polish (`defineModel`, `<Suspense>` deep, pinia) is not a Cordlang 1.0 requirement — prefer shared IR features that also work on React/Svelte.
- ESM preview (`cordlang run`) degrades preset tags to stubs; full icons/motion need `run vue|react|svelte`.

## AI traps (Vue)

| Wrong | Right |
|-------|-------|
| `v-if` / `v-for` / `v-model` in `.cord` | `if` / `for` / `bind=` |
| `{{ count }}` in `.cord` | `#{count}` |
| `onClick` / `className` | `@click` / style attrs |
| `import { ref } from 'vue'` in `.cord` | `state` / presets / `foreign` |
| Assume Nuxt file routing | SPA `route` + `layout` only |

## Notas

- Codegen camina **solo** `IrNode` (`vue_generate_from_ir` / `vue_ir.c`).
- En plantilla, los `ref` se auto-desenvuelven (`{{ count }}`); en script se usa `.value`.
- Goldens CI + `tests/run_template_check` include `vue`.
- Promotion history: [VUE_PROMOTION.md](./VUE_PROMOTION.md).

Roadmap: [`docs/ROADMAP.md`](./ROADMAP.md).
