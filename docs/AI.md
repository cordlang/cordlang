# Cordlang for AI models

Cordlang is optimized so **language models write less UI code** with fewer tokens, then a **real compiler** expands it to React or Svelte.

This page is the contract for **any** model (Grok, Claude, GPT, Copilot, Cursor, …).

Also see:

- Root agent brief: [`AGENTS.md`](../AGENTS.md)
- **Skill:** [`skills/write-cord/SKILL.md`](../skills/write-cord/SKILL.md)
- Human guide: [GUIDE.md](./GUIDE.md) · [CHEATSHEET.md](./CHEATSHEET.md)

---

## Mission

When the user wants UI:

1. Prefer **`.cord` files**, not raw JSX/Svelte/HTML (unless they explicitly ask for one backend only and no Cordlang).
2. Keep sources **dense and valid** for the Cordlang compiler.
3. Split multi-page apps into `app.cord` + `pages/` + `components/` + `layouts/`.
4. Compile mentally to both React and Svelte when possible (same `.cord`).

---

## DO

| Do | Example / note |
|----|----------------|
| Use indentation for hierarchy | children under `col` / `row` |
| Use `state` / `props` / `computed` | not `useState` in source |
| Use `#{expr}` for text | `p "Hi #{name}"` or `p "#{name}"` |
| Use `@event=handler` | `@click=setCount(count + 1)` |
| Use `setX` for state updates | matches emitted helpers |
| Use `if` / `for … key=` | not JSX `&&` / `.map` in `.cord` |
| Use `route` + multi-file modules | `route / => pages/HomePage` |
| Use `link … to=/path` | not `<Link>` |
| Use `bind=field` on inputs | controlled forms |
| Use `fetch x = "/api/…"` for JSON | with Loading/Error UI |
| Use `theme` for design tokens | CSS vars backend |
| Use `context` / `provide` / `ctx` | shared theme etc. |
| Run `cordlang check` after edits | when CLI available |
| Point users to `cordlang run react\|svelte` | for real apps |

---

## DO NOT

| Don't | Why |
|-------|-----|
| Emit large JSX/TSX as the primary source | Defeats Cordlang |
| Invent keywords not in the docs | Parse/check will fail |
| Use curly braces `{count}` as Cord syntax | Use `#{count}` |
| Use `className=` / `onClick=` in `.cord` | Use classes via attrs / `@click` |
| Put `use:action` as form `action=` | Forms: `action=formAction`; elements: `use=name` |
| Assume SvelteKit file routing or Next RSC | SPA backends only (today) |
| Write TypeScript types inside `.cord` | Not a TS host |
| Nest 500 lines in `app.cord` | Split files |
| Hand-edit `dist/**` as source of truth | Regenerated |
| Claim features that are roadmap-only as done | Kit/Next, full LSP, etc. |

---

## Minimal valid patterns (copy)

### Counter component

```cord
def Counter
  state count=0
  props label="Counter"
  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold
    span "#{count}" size=4xl bold
    row gap=8
      btn "-" @click=setCount(count - 1) variant=outline
      btn "+" @click=setCount(count + 1) variant=primary
```

### Page shell

```cord
# app.cord
use layouts/default
route / => pages/HomePage
route /about => pages/AboutPage
```

### Form

```cord
action form = submitForm init=null pending=saving
form action=formAction
  input text bind=email name=email placeholder="Email"
  btn "Send" variant=primary
```

---

## When user asks for React or Svelte specifically

Still write **`.cord`**, then:

```bash
cordlang run react
# or
cordlang run svelte
```

Only dump raw React/Svelte if they say “show me the generated code” or “I don’t want Cordlang”.

---

## Backend-specific extras (optional)

Use only when targeting that feature set:

**React Phase D-ish:** `insertionEffect`, `effectEvent`, `externalStore`, `imperativeHandle`, `forwardRef` on `def`, `errorBoundary`, `portal`, `lazy`, `suspense`.

**Svelte Phase E-ish:** `store`/`writable`, `await … then=`, `snippet` / `render`, `use=`, `transition=`, `portal`.

If unsure a keyword exists → prefer core subset (state, if, for, route, bind, fetch) and link [ROADMAP.md](./ROADMAP.md).

---

## Self-check before finishing

- [ ] Indentation consistent (spaces)
- [ ] No JSX tags like `<div>`
- [ ] State updates via `setName(...)`
- [ ] Routes/pages split for multi-page
- [ ] No invented attrs that look like React-only DOM props without Cord mapping
- [ ] Mention how to run: `cordlang run` / `run react` / `check`

---

## Token economy tip

Prefer Cordlang denser forms:

```cord
btn "Save" variant=primary @click=save
```

over multi-line JSX with imports, hooks boilerplate, and className strings — the compiler expands them.
