# Cordlang ↔ Next.js (meta-backend)

**Tier: Experimental / meta.** SPA wrapper around React emit — **not** a claim of
RSC, SSR, or App Router file-routing parity. Tiers: [BACKENDS.md](./BACKENDS.md).

Wrapper alrededor del emit **React** (`react_emit_modules_from_ir`).

## Uso

```bash
cordlang run next
cd dist/next
npm install
npm run dev

cordlang compile src/app.cord --backend next
cordlang run next --check   # npm install + npm run build
```

## Qué genera

| Path | Rol |
|------|-----|
| `app/layout.jsx` | Root layout App Router |
| `app/page.jsx` | `'use client'` → importa `src/App.jsx` |
| `src/**/*.jsx` | Módulos React del IR (igual que backend `react`) |
| `package.json` | `next` + `react` + `react-dom` |

## Subconjunto / límites

- Client components only en el entry; **sin** Server Components / server actions
- Rutas Cord `route` siguen el modelo React (hash/SPA helpers del emit React),
  no file-based App Router por path
- No ensucia el scaffold SPA de `react` (`dist/react` sigue independiente)

## Archivos

- Puerto: `name=next`, `needs_node_check=1`
- Docs de paridad React: [REACT.md](./REACT.md)
- Smoke: `tests/regression/next-smoke/`
