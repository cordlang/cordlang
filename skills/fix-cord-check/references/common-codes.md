# Common `cordlang check` codes (skill bundle)

Apply each diagnostic `"hint"` when present. Full attr surface: bundled with `write-cord` as `references/attrs-summary.md`, or https://raw.githubusercontent.com/cordlang/cordlang/main/docs/schema/attrs.json

## Codes

| code | Meaning | Typical fix |
|------|---------|-------------|
| `jsx-attr` | Forbidden JSX/React attribute | `className` → `class` or style attrs; `onClick` → `@click=…`; `onChange` → `@change=…`; `htmlFor` → `for` |
| `jsx-hook` | Hook / JSX component used as Cord tag | `useState` → `state`; `useEffect` → `effect`; `Link` → `link … to=` |
| `jsx-tag` | Angle-bracket JSX in `.cord` | `<div>` → `div`; `<Link>` → `link` |
| `jsx-map` | Array `.map(` in `.cord` | use `for x in xs key=…` |
| `bad-interp` | JSX-style `{expr}` in text | `{count}` → `#{count}` |
| `semantic-vocab` | `purpose` / `importance` outside vocabulary | see vocabulary below |
| (unknown attr warn) | Attr not on known surface | drop attr or use a documented name from attrs summary |

## Semantic vocabulary

| Attr | Allowed values |
|------|----------------|
| `purpose` | `navigation` `content` `action` `form` `status` `decoration` `landmark` |
| `importance` | `primary` `secondary` `tertiary` `optional` `critical` |

## JSON loop

```bash
cordlang check --json [path]
```

Re-run until no `"level":"error"`. Then optionally `cordlang analyze [--json]`.
