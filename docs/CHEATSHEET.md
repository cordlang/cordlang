# Cordlang cheatsheet

One-screen reference. Full guide: [GUIDE.md](./GUIDE.md).

## Skeleton

```cord
def Name
  state n=0
  props title: string = ""
  col gap=16 p=24
    h1 "#{title}" bold
    btn "+" @click=setN(n + 1)
```

## Decls

| Cord | React-ish | Svelte-ish |
|------|-----------|------------|
| `state x=0` | `useState` + `setX` | `$state` + `setX` |
| `props a=""` / `props a: string = ""` | props (+ `check` types) | `$props()` |
| `computed y = x*2` | `useMemo` | `$derived` |
| `effect` / `layoutEffect` | `useEffect` / `useLayoutEffect` | `$effect` / `$effect.pre` |
| `ref el` | `useRef` | `$state` + `bind:this` |
| `ctx t = Theme` | `useContext` | `getContext` |
| `context Theme = "light"` | `createContext` | Symbol key |
| `provide Theme value=t` | Provider | `setContext` |
| `params id` | `useParams` | route props |
| `action form = fn pending=p` | `useActionState` | form helper |
| `fetch data = "/api/x.json"` | state+effect fetch | same |
| `store n = 0` | — | `writable` + `{$n}` |
| `lazy X = path` | `lazy` + Suspense | dynamic import |

## UI

| Cord | Notes |
|------|--------|
| `col` `row` `stack` `grid` | flex/grid layouts |
| `btn` `link` `input` `h1`…`p` | primitives |
| `@click=…` | events |
| `bind=email` | two-way |
| `if cond` / `else` | conditionals |
| `for x in xs key=x.id` | lists |
| `#{expr}` | text bind |
| `link "Hi" to=/path` | router link |
| `slot` | outlet / children |
| `color=primary` | theme token when mapped |

## Svelte-only attrs (element)

| Cord | Emits |
|------|--------|
| `use=tooltip` | `use:tooltip` |
| `transition=fade` | `transition:fade` |
| `in=fly out=fade` | in/out |
| `portal to=document.body` | portal action |

## App wiring

```cord
theme app
  primary: "#2563eb"
use layouts/default
route / => pages/HomePage
route /p/:id => pages/ProductPage
```

## CLI

```
run | run react|svelte|vue [--check] [--watch] | run html
check [--json] | analyze [--json] | ai [check|context|doctor] | fmt | symbols | goto Name
compile file.cord --backend esm|react|svelte|vue|html|--ir
```
