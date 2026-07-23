---
name: write-cord
description: >
  Write and edit Cordlang (.cord) UI instead of JSX/Svelte. Use when the user
  wants UI, pages, components, forms, routes, or says "cord", "cordlang",
  "write cord not jsx", "UI compacta", or /write-cord. Prefer multi-file
  .cord apps and valid compiler syntax. Do not invent keywords.
---

# Write Cordlang (not JSX)

> **Portable skill** — path: `skills/write-cord/` (vendor-agnostic).

You are authoring **Cordlang** — a dense UI DSL compiled to React, Svelte 5, or HTML.

## Read first (in repo)

1. `docs/AI.md` — hard do / don't  
2. `docs/CHEATSHEET.md` — one-screen syntax  
3. `docs/GUIDE.md` — samples  
4. `AGENTS.md` — repo rules  
5. `skills/write-cord/references/patterns.md` — copy-paste patterns  

## Default behavior

1. Deliver **`.cord` source** as the primary artifact.  
2. For apps: structure like `my-app/` (`app.cord` + `pages/` + `components/` + `layouts/`).  
3. Mention how to run: `cordlang run`, `cordlang run react`, `cordlang run svelte`, `cordlang check`.  
4. Only show generated React/Svelte if the user asks for emitted code.

## Syntax rules (non-negotiable)

```cord
def Widget
  state open=false
  props title=""
  col gap=16 p=24
    h1 "#{title}" size=2xl bold
    if open
      p "Visible" muted
    btn "Toggle" @click=setOpen(!open) variant=primary
```

| Rule | Correct | Wrong |
|------|---------|--------|
| Interpolation | `#{count}` | `{count}` |
| Events | `@click=setCount(count+1)` | `onClick={...}` |
| State | `state n=0` + `setN(...)` | `useState` in .cord |
| Lists | `for x in xs key=x.id` | `.map` in .cord |
| Links | `link "Home" to=/` | `<Link to>` |
| Forms | `form action=formAction` + `bind=` | random HTML forms |
| Svelte actions | `use=tooltip` on elements | `action=` on non-forms (form submit only) |

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

**Use when needed:** lazy, portal, errorBoundary, suspense, title/head, store (Svelte), await/snippet (Svelte), Phase D React hooks.

**Do not claim as done:** SvelteKit file routing, Next RSC, full LSP (see `docs/ROADMAP.md`).

## Self-check

- [ ] No `<jsx>` tags in `.cord`  
- [ ] Indentation defines tree  
- [ ] Multi-page → multi-file  
- [ ] No edits to `dist/` as source of truth  
- [ ] User can compile with existing CLI commands  

## References

- `docs/EXAMPLES.md` — file catalog  
- `examples/counter.cord`, `examples/fetch_form.cord`, `my-app/`  
- `docs/REACT.md` / `docs/SVELTE.md` — backend maps  
