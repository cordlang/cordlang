# Cordlang AI context (skill bundle)

Self-contained compact contract for agents. Upstream (Cordlang checkout): `docs/AI_CONTEXT.md`. Raw: https://raw.githubusercontent.com/cordlang/cordlang/main/docs/AI_CONTEXT.md

**North star:** vibecode + AI with **minimal token spend**. Dense `.cord` beats JSX. Deterministic `check` — **no LLM in `compile`**.

**Cordlang = language.** The web framework product is **Runix** (see `docs/RUNIX.md`) — do not treat this CLI as Next/Vite replacement.

## Mission

1. Write **`.cord`**, not raw JSX/Svelte (unless the user forbids Cordlang).
2. Keep sources dense and valid — every ceremonial line costs tokens.
3. Multi-page: `app.cord` + `pages/` + `components/` + `layouts/`.
4. After edits: `cordlang check` (prefer `--json` for agents).
5. Preview: **`cordlang run`** — ESM native (no Node). Full UI with presets: `run react` / `run svelte` / `run vue`.

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

## Libraries

- Capabilities via `cordlang.json` → `"presets": ["icons","motion","charts"]` — **not** framework imports in `.cord`.
- Tags: `icon name=…`, `motion`, `chart`; escape: `foreign Name` + `react from "pkg"` / `svelte from "…"`.
- CLI: `cordlang preset add icons motion`.

## Visual quality

- Prefer `theme` tokens; `type=display|title|body|caption`, `elevate=0..4`, `section` / `stack density=`, `md:p=` responsive.
- Motion budget: 1–2 motions per viewport (`motion fade`). ESM preview stubs `icon`/`motion`/`chart` — full UI needs `run react|svelte|vue`.

## CLI

```bash
cordlang check [--json] [path]
cordlang analyze [--json] [path]
cordlang ai context | ai doctor [path] | ai check [path]
cordlang preset list | preset add <id>…
cordlang run              # ESM native (no Node)
cordlang run --no-open
cordlang run html         # legacy HTML
cordlang run react        # Vite + React
cordlang run svelte       # Vite + Svelte 5
```

## Self-check

- [ ] No `<jsx>` / `className` / `onClick`
- [ ] Interpolation is `#{…}`
- [ ] Attrs ⊆ known surface (see `attrs-summary.md`)
- [ ] No framework `import` in `.cord` (presets / foreign)
- [ ] Theme + type/elevate/section for polished UI
- [ ] `cordlang check` green
- [ ] No edits to `dist/` as source of truth

## Companion skill

Repair after check fails: `fix-cord-check` (same install: `npx skills add cordlang/cordlang -s fix-cord-check`).
