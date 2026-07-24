# Cordlang AI context (compact)

Read this first. Full contract: [`AI.md`](./AI.md) · attrs: [`schema/attrs.json`](./schema/attrs.json) · workflow: [`AI_WORKFLOW.md`](./AI_WORKFLOW.md).

## Mission

1. Write **`.cord`**, not raw JSX/Svelte (unless the user forbids Cordlang).
2. Keep sources dense and valid for the compiler.
3. Multi-page: `app.cord` + `pages/` + `components/` + `layouts/`.
4. After edits: `cordlang check` (prefer `--json` for agents).

## Syntax (non-negotiable)

| Correct | Wrong |
|---------|-------|
| `#{count}` | `{count}` |
| `@click=setN(n+1)` | `onClick=` / `className=` |
| `state n=0` + `setN(...)` | `useState` in `.cord` |
| `props t: string = ""` | TypeScript interfaces |
| `for x in xs key=x.id` | `.map` |
| `link "Home" to=/` | `<Link>` |

## Minimal pattern

```cord
def Counter
  state count=0
  props label: string = "Counter"
  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold purpose=content importance=primary
    span "#{count}" size=4xl bold purpose=status
    row gap=8
      btn "-" @click=setCount(count - 1) variant=outline purpose=action
      btn "+" @click=setCount(count + 1) variant=primary purpose=action importance=primary
```

## Semantic attrs (optional)

- `purpose`: `navigation` \| `content` \| `action` \| `form` \| `status` \| `decoration` \| `landmark`
- `importance`: `primary` \| `secondary` \| `tertiary` \| `optional` \| `critical`

## CLI

```bash
cordlang check [--json] [path]
cordlang analyze [--json] [path]
cordlang ai context | ai doctor [path] | ai check [path]
cordlang run | run react | run svelte
```

## Self-check

- [ ] No `<jsx>` tags / `className` / `onClick`
- [ ] Interpolation is `#{…}`
- [ ] Attrs ⊆ `schema/attrs.json`
- [ ] `cordlang check` green
- [ ] No edits to `dist/` as source of truth

## Skills

- Author: `skills/write-cord/SKILL.md`
- Repair: `skills/fix-cord-check/SKILL.md`
