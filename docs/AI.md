# Cordlang for AI models

Cordlang is an **intermediate UI language** optimized for vibecode and AI agents: models write dense `.cord` (**fewer tokens** than JSX), a **deterministic compiler** expands it to React, Svelte, Vue, or ESM preview. There is **no LLM in `compile`**. Product north star = **token-efficient agent loops**. Cordlang is **not** the web framework — that product is **[Runix](./RUNIX.md)**. If other docs contradict this, prefer this contract + [`AI_CONTEXT.md`](./AI_CONTEXT.md).

This page is the contract for **any** model (Grok, Claude, GPT, Copilot, Cursor, …).

Also see:

- **Start here (compact):** [`AI_CONTEXT.md`](./AI_CONTEXT.md)
- **Runix (framework vision):** [`RUNIX.md`](./RUNIX.md)
- Root agent brief: [`AGENTS.md`](../AGENTS.md)
- **Skill:** [`skills/write-cord/SKILL.md`](../skills/write-cord/SKILL.md)
- **Attrs schema (machine-readable):** [`schema/attrs.json`](./schema/attrs.json)
- Prop types + check: this doc + `cordlang check` / `cordlang check --json`
- Workflow: [`AI_WORKFLOW.md`](./AI_WORKFLOW.md) · CLI `cordlang ai` / `ai context` / `ai doctor`
- Human guide: [GUIDE.md](./GUIDE.md) · [CHEATSHEET.md](./CHEATSHEET.md)

---

## Mission

When the user wants UI:

1. Prefer **`.cord` files**, not raw JSX/Svelte/HTML (unless they explicitly ask for one backend only and no Cordlang).
2. Keep sources **dense and valid** for the Cordlang compiler.
3. Split multi-page apps into `app.cord` + `pages/` + `components/` + `layouts/`.
4. Compile mentally to both React and Svelte when possible (same `.cord`).
5. After edits: `cordlang check` (and optionally `cordlang analyze`). Never invent keywords outside the schema/docs.

---

## Machine-readable contract

| Artifact | Use |
|----------|-----|
| [`schema/attrs.json`](./schema/attrs.json) | Official tags, style attrs, DOM attrs, forbidden JSX names |
| `cordlang check` | Errors/warnings: unknown comps, routes, typed props, unknown attrs, JSX traps |
| `cordlang symbols` / `goto` | Navigation for LSP / agents |
| `cordlang analyze` | Deterministic score (unused comps, `link` sin `to`, layout sin `slot`, …) — **no LLM** |

Allowed prop types (when declared): `string`, `number`, `boolean`, `any`.

```cord
props title: string = ""
props count: number = 0
props open: boolean = false
```

Types are validated by `check`; codegen still emits JS (types are contracts for humans/IA, not a TS host).

### Semantic metadata (optional)

Attrs `purpose` and `importance` are **known** (no `check` warning). Prefer the vocabulary in [`schema/attrs.json`](./schema/attrs.json) `semanticAttrs`:

```cord
btn "Save" variant=primary purpose=action importance=primary
p "Hint" muted purpose=content importance=optional
```

HTML/email backends may passthrough as `data-purpose` / `data-importance`. SPA backends may ignore them (metadata for AI/tooling, not layout).

---

## DO

| Do | Example / note |
|----|----------------|
| Use indentation for hierarchy | children under `col` / `row` |
| Use `state` / `props` / `computed` | not `useState` in source |
| Prefer typed props when known | `props label: string = "Hi"` |
| Use `#{expr}` for text | `p "Hi #{name}"` or `p "#{name}"` |
| Escape literal `#{` as `\#{` | only when you need the characters |
| Use `@event=handler` | `@click=setCount(count + 1)` |
| Use `setX` for state updates | matches emitted helpers |
| Use `if` / `for … key=` | not JSX `&&` / `.map` in `.cord` |
| Use `route` + multi-file modules | `route / => pages/HomePage` |
| Use `link … to=/path` | not `<Link>` |
| Use `bind=field` on inputs | controlled forms |
| Use `fetch x = "/api/…"` for JSON | with Loading/Error UI |
| Use `theme` for design tokens | CSS vars backend |
| Use `context` / `provide` / `ctx` | shared theme etc. |
| Stick to attrs in `schema/attrs.json` | unknown attrs → `check` warning |
| Optional `purpose` / `importance` | semantic metadata (see below) |
| Run `cordlang check` after edits | when CLI available |
| Point users to `cordlang run react\|svelte\|vue` | for real apps |

---

## DO NOT

| Don't | Why |
|-------|-----|
| Emit large JSX/TSX as the primary source | Defeats Cordlang |
| Invent keywords not in the docs/schema | Parse/check will fail |
| Use curly braces `{count}` as Cord syntax | Use `#{count}` — `check` code `bad-interp` |
| Use `className=` / `onClick=` / `onKeyDown=` / `tabIndex=` | Use style attrs / `@click` / `tabindex=` — `jsx-attr` |
| Use `useState` / `useEffect` / `<Link>` as Cord tags | Use `state` / `effect` / `link` — `jsx-hook` |
| Use `.map(` or `<div>…</div>` in `.cord` | Use `for … key=` / Cord tags — `jsx-map` / `jsx-tag` |
| Put `use:action` as form `action=` | Forms: `action=formAction`; elements: `use=name` |
| Assume SvelteKit file routing or Next RSC | SPA backends only (today); meta backends are experimental wraps |
| Write full TypeScript/JSX types as host language | Only Cord prop types above |
| Nest 500 lines in `app.cord` | Split files |
| Hand-edit `dist/**` as source of truth | Regenerated |
| Put `@ai` / LLM calls in compile path | Workflow only (`cordlang ai`, skills) |
| Claim features that are roadmap-only as done | Remote registry, full native, multi-file WASM playground |

---

## Minimal valid patterns (copy)

### Counter component

```cord
def Counter
  state count=0
  props label: string = "Counter"
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
- [ ] Attrs exist in `schema/attrs.json` (no `className` / `onClick`)
- [ ] Typed props use `string` \| `number` \| `boolean` \| `any` when declared
- [ ] `cordlang check` green (and `analyze` if touching structure/a11y)
- [ ] Mention how to run: `cordlang run` / `run react` / `check`

---

## Token economy tip

Prefer Cordlang denser forms:

```cord
btn "Save" variant=primary @click=save
```

over multi-line JSX with imports, hooks boilerplate, and className strings — the compiler expands them.
