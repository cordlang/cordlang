# Cordlang ↔ SvelteKit (meta-backend)

Wrapper alrededor del emit **Svelte** (`svelte_emit_modules_from_ir`). MVP de
esqueleto Kit; no pretende paridad SSR/SSG completa.

## Uso

```bash
cordlang run sveltekit
cd dist/sveltekit
npm install
npm run dev

cordlang compile src/app.cord --backend sveltekit
cordlang run sveltekit --check
```

## Qué genera

| Path | Rol |
|------|-----|
| `src/routes/+page.svelte` | Importa `$lib/App.svelte` |
| `src/routes/+layout.svelte` | Layout mínimo |
| `src/lib/**` | Módulos Svelte del IR |
| `package.json` | `@sveltejs/kit` + `svelte` + `vite` |

## Subconjunto / límites

- Una página Kit que monta el `App` generado (hash router del emit Svelte SPA
  puede coexistir; file routes IR→`+page` profundas quedan fuera del MVP)
- No modifica `dist/svelte` (SPA Vite independiente)

## Archivos

- Puerto: `name=sveltekit`, `needs_node_check=1`
- [SVELTE.md](./SVELTE.md) para el mapa de lenguaje
- Smoke: `tests/regression/sveltekit-smoke/`
