---
name: write-cord
description: >
  Write and edit Cordlang (.cord) UI instead of JSX/Svelte. Use when the user
  wants UI, pages, components, forms, routes, or says "cord", "cordlang",
  "write cord not jsx", "UI compacta", or /write-cord. Prefer multi-file
  .cord apps and valid compiler syntax. Do not invent keywords.
---

# Write Cordlang (not JSX)

> **Portable skill** — install: `npx skills add cordlang/cordlang -s write-cord`

You are authoring **Cordlang** — a dense UI **language** optimized for **vibecode / AI token savings**, compiled to React, Svelte 5, Vue 3, or ESM preview. Prefer `.cord` over JSX: fewer tokens in prompts and diffs. No LLM in `compile`; use `cordlang check` after edits. The web **framework** product is **Runix** (`docs/RUNIX.md` in the Cordlang repo) — do not treat Cordlang itself as Next/Vite.

## Read first

Bundled with this skill (works after `npx skills` install):

1. `references/ai-context.md` — compact contract (preferred)
2. `references/cheatsheet.md` — one-screen syntax
3. `references/attrs-summary.md` — known attrs / forbidden JSX / check codes
4. `references/patterns.md` — copy-paste patterns
5. If `cordlang check` fails → companion skill `fix-cord-check`

If the workspace is a **Cordlang checkout**, you may also read `docs/AI_CONTEXT.md`, `docs/AI.md`, `AGENTS.md`, `docs/GUIDE.md`, and full `docs/schema/attrs.json`.

Long-form (fetch when needed):  
https://raw.githubusercontent.com/cordlang/cordlang/main/docs/AI_CONTEXT.md

## Default behavior

1. Deliver **`.cord` source** as the primary artifact.
2. For apps: `app.cord` + `pages/` + `components/` + `layouts/`.
3. Mention how to run: `cordlang run` (ESM preview), `cordlang run react`, `cordlang run svelte`, `cordlang check`, `cordlang analyze`.
4. Prefer typed props: `props title: string = ""` (`string` \| `number` \| `boolean` \| `any`).
5. Only show generated React/Svelte if the user asks for emitted code.
6. After edits: `cordlang check` (or `cordlang ai check` / `check --json`).
7. If check fails: follow `fix-cord-check`.

## Syntax rules (non-negotiable)

```cord
def Widget
  state open=false
  props title: string = ""
  col gap=16 p=24
    h1 "#{title}" size=2xl bold
    if open
      p "Visible" muted
    btn "Toggle" @click=setOpen(!open) variant=primary
```

| Rule | Correct | Wrong | `check` code |
|------|---------|-------|--------------|
| Interpolation | `#{count}` | `{count}` | `bad-interp` |
| Events | `@click=setCount(count+1)` | `onClick={...}` / `className=` | `jsx-attr` |
| State | `state n=0` + `setN(...)` | `useState` in .cord | `jsx-hook` |
| Props | `props t: string = ""` | TypeScript interfaces in .cord | — |
| Lists | `for x in xs key=x.id` | `.map` in .cord | `jsx-map` |
| Tags | `div` / `col` / `link` | `<div>` / `<Link>` | `jsx-tag` / `jsx-hook` |
| Links | `link "Home" to=/` | `<Link to>` | `jsx-hook` |
| Forms | `form action=formAction` + `bind=` | random HTML forms | — |
| Svelte actions | `use=tooltip` on elements | `action=` on non-forms (form submit only) | — |
| Attrs | only those in `references/attrs-summary.md` | invented DOM/React names | warn / `jsx-attr` |

## Multi-file app template

**app.cord**

```cord
theme app
  primary: "#2563eb"
  bg: "#fff"
  text: "#111"
  muted: "#6b7280"
  radius: 12

use layouts/default
route / => pages/HomePage
route /about => pages/AboutPage
```

**layouts/default.cord** — header + `slot`  
**pages/*.cord** — screens  
**components/*.cord** — reusable components  

## Feature tiers

**Always OK (core):** state, props, computed, if/for, events, bind, route, layout/slot, link, theme, fetch, form action, context provide/ctx.

**Use when needed:** lazy, portal, errorBoundary, suspense, title/head, store (Svelte), await/snippet (Svelte), Phase D React hooks, **presets** (`icon`/`motion`/`chart` after `cordlang preset add`), **foreign** multi-backend widgets.

**Do not claim as done:** remote package registry, Flutter/SwiftUI backends, live playground preview (WASM playground compiles codegen only). Meta Next/Kit are client wraps only.

**Libraries:** Prefer capabilities + `foreign` maps — never write `import … from 'framer-motion'` in `.cord`.

**Visual quality:** Prefer `theme` + `type=` / `elevate=` / `section` / `density=` / `md:` prefixes over raw utility sprawl. Motion budget: 1–2 per viewport.

## Self-check

- [ ] No `<jsx>` tags in `.cord`
- [ ] Indentation defines tree
- [ ] Multi-page → multi-file
- [ ] No `className` / `onClick`
- [ ] Attrs ⊆ `references/attrs-summary.md`
- [ ] Theme / type / elevate / section used when building polished UI
- [ ] `cordlang check` green
- [ ] No edits to `dist/` as source of truth
- [ ] User can compile with existing CLI commands

## References

- Bundled: `references/ai-context.md` · `cheatsheet.md` · `attrs-summary.md` · `patterns.md`
- Upstream docs (Cordlang repo / raw GitHub): `AI.md` · `AI_WORKFLOW.md` · `DESIGN.md` · `REACT.md` · `SVELTE.md` · `PREVIEW.md`
- Seeds: `templates/` · `examples/counter.cord` (when present in workspace)
