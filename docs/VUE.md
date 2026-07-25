# Cordlang ↔ Vue 3

Mapeo del modelo mental de [Vue 3](https://vuejs.org/) al lenguaje `.cord`.
Cordlang compila a **Vue 3 SFCs** (`<script setup>` + `<template>`) + Vite +
Tailwind + [vue-router](https://router.vuejs.org/) (hash history).

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

## Archivos generados

- `dist/vue/` — scaffold Vite (`@vitejs/plugin-vue`, `vue`, `vue-router`)
- Componentes: `src/**/*.vue`
- Extension de puerto: `.vue`
- `needs_node_check=1` (`--check` / `--watch`)

## Notas

- Codegen camina **solo** `IrNode` (`vue_generate_from_ir` / `vue_ir.c`).
- En plantilla, los `ref` se auto-desenvuelven (`{{ count }}`); en script se usa `.value`.
- Paridad SPA con React/Svelte documentada en esta tabla; detalles de Kit/Nuxt quedan fuera de alcance.
