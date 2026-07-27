# Cordlang user guide

Practical guide with **copy-paste samples**. For design notes see [LANGUAGE.md](./LANGUAGE.md); for framework maps see [REACT.md](./REACT.md) and [SVELTE.md](./SVELTE.md).

---

## 1. Install & first app

```bash
# Build CLI (Windows)
build.bat

# Scaffold
cordlang init demo
cd demo

# Instant preview (no Node)
cordlang run

# Real frameworks (default product targets)
cordlang run react     # → dist/react (Vite + React + Tailwind)
cordlang run svelte    # → dist/svelte (Vite + Svelte 5 runes)
```

### ESM preview contract

`cordlang run` is the **native ESM preview** (no Node): each `.cord` is a real ES
module. See [`PREVIEW.md`](./PREVIEW.md). Legacy HTML: `cordlang run html`.

| Supported in ESM preview | Use SPA backends (`run react\|svelte`) for |
|--------------------------|-------------------------------------------|
| Multi-file `use` / `route` / layouts | Full preset libs (lucide, framer, recharts) |
| `state` + `setX` / `#{…}` / `if` / `for` | npm `foreign` packages |
| Soft update (leaf `.cord`) or full reload | Production React/Svelte deploy |

```bash
cordlang init demo --template counter
cd demo
cordlang run
cordlang run react --check
cordlang symbols
```

---

## 2. Project shape

```
demo/   # or any name from cordlang init
  cordlang.json          # entry, outDir
  public/                # static assets → dist/*/public
  src/
    app.cord             # theme + layout + routes (keep thin)
    layouts/default.cord
    pages/*.cord
    components/*.cord
```

`cordlang.json`:

```json
{
  "name": "cordlang-app",
  "version": "0.1.0",
  "entry": "src/app.cord",
  "defaultBackend": "react",
  "outDir": "dist"
}
```

---

## 3. Core syntax (samples)

### 3.1 Component + state + props

```cord
def Counter
  state count=0
  props label="Counter"

  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold
    p "Count: #{count}" muted
    row gap=8 center
      btn "-" @click=setCount(count - 1) variant=outline
      span "#{count}" size=4xl bold
      btn "+" @click=setCount(count + 1) variant=primary
```

- `state x=0` → React `useState` / Svelte `$state` + helper `setX`
- `props a=""` → props with default
- `#{expr}` → interpolation
- `@click=…` → events
- Indentation = children

### 3.2 Body-only file (filename is the component)

`src/components/Counter.cord`:

```cord
state count=0
props label=""

col gap=12 center p=16
  p "Count: #{count}" size=lg
  btn "+" @click=setCount(count + 1) variant=primary
```

### 3.3 Conditionals & lists

```cord
def ListDemo
  state showHint=1
  props title="Items"

  col gap=16 p=24
    h1 "#{title}" size=2xl bold
    if showHint
      p "Hint: click items" muted
    for item in items key=item.id
      row gap=8 center
        span "#{item.name}" bold
        btn "Select" @click=selectItem(item.id) variant=outline
```

### 3.4 Theme tokens

```cord
theme shop
  primary: "#2563eb"
  bg: "#ffffff"
  text: "#1a1a1a"
  muted: "#6b7280"
  radius: 12
```

→ `src/theme.css` CSS variables (`--color-primary`, `--radius`, …) plus design-system
defaults (type scale, elevation, density). See [`DESIGN.md`](./DESIGN.md).

Use `color=primary`, `type=display`, `elevate=2`, `section`, `md:p=24` on tags when the backend maps tokens.

### 3.5 Multi-file app

**`src/app.cord`** (entry — keep thin):

```cord
theme shop
  primary: "#2563eb"
  bg: "#ffffff"
  text: "#1a1a1a"
  muted: "#6b7280"
  radius: 12

use layouts/default

route / => pages/HomePage
route /counter => pages/CounterPage
route /shop => pages/ShopPage
route /about => pages/AboutPage
```

**`src/layouts/default.cord`**:

```cord
header sticky bg=white shadow=sm
  row between center p=16
    h1 "My App" size=xl bold
    nav
      row gap=16 center
        link "Home" to=/
        link "Counter" to=/counter
        link "Shop" to=/shop
slot
```

**`src/pages/HomePage.cord`**:

```cord
col gap=16 max-w=640
  h1 "Welcome" size=2xl bold
  p "Pages live in their own .cord files" muted
  link "Try Counter" to=/counter
```

**`src/pages/CounterPage.cord`**:

```cord
use ../components/Counter

col gap=16 p=24
  h1 "Counter page" size=2xl bold
  Counter label="Live"
```

| Directive | Meaning |
|-----------|---------|
| `use path` | Load `.cord` module |
| `use path as Name` | Alias |
| `route /x => pages/Foo` | Route + load page module |
| `layout` + `slot` | Shell + outlet / children |

---

## 4. Data & forms

### Fetch

```cord
def Shop
  fetch products = "/api/products.json"

  col gap=16 p=24
    if productsLoading
      p "Loading…" muted
    if productsError
      p "Failed to load" muted
    if products
      p "Loaded" muted
```

Emits loading / error / data state on both backends. Put JSON under project `public/`.

### Form + action (React 19 style / Svelte progressive)

```cord
def Contact
  action form = submitForm init=null pending=saving

  col gap=16 p=24
    if saving
      p "Sending…" muted
    form action=formAction
      col gap=8
        input email bind=email name=email placeholder="you@example.com"
        input text bind=name name=name placeholder="Name"
        btn "Send" variant=primary
```

---

## 5. Advanced samples (by backend)

| Topic | Example file | Notes |
|-------|--------------|--------|
| Counter | `examples/counter.cord` | state + events |
| Hooks / context | `examples/react_hooks.cord` | context, effects |
| Lazy + head | `examples/phase_b_lazy_head.cord` | Phase B |
| Fetch + form | `examples/fetch_form.cord` | Phase B |
| React advanced | `examples/phase_d_hooks.cord` | insertionEffect, action, forwardRef… |
| Svelte advanced | `examples/phase_e_svelte.cord` | await, snippet, store, portal, use: |
| Context + params | `examples/svelte_ctx_params.cord` | provide/ctx + `:id` |
| Full multi-file | `templates/` | stock starters (`init --template`) |

Compile any example:

```bash
cordlang compile examples/counter.cord --backend react
cordlang compile examples/phase_e_svelte.cord --backend svelte
cordlang compile examples/counter.cord --ir
```

---

## 6. CLI cheat sheet

| Command | Purpose |
|---------|---------|
| `cordlang run` | ESM native preview (no Node) — [PREVIEW.md](./PREVIEW.md) |
| `cordlang run html` | Legacy single-document HTML preview |
| `cordlang run react\|svelte` | Scaffold Vite app |
| `cordlang build esm` | Static export → `dist/esm` |
| `cordlang run react --check` | + vite production build |
| `cordlang run react --watch` | Rebuild on `.cord` change |
| `cordlang check` | Semantic errors (`file:line:col`) |
| `cordlang fmt src/` | Format |
| `cordlang symbols` | Components / routes / layouts |
| `cordlang goto Counter` | Definition path |
| `cordlang compile f.cord --ir` | Dump IR |

---

## 7. Pipeline (mental model)

```
.cord → Lexer → Parser → AST → IR → React | Svelte | HTML
```

Write **Cordlang**, not JSX/Svelte, unless you are editing the generated `dist/` escape hatch.

---

## 8. Common mistakes

| Don't | Do |
|-------|-----|
| Put all UI in one giant `app.cord` | Split pages/components |
| Write JSX inside `.cord` | Use tags + `#{expr}` + hooks DSL |
| Use `form action=` for Svelte element actions | Element actions: `use=tooltip`; forms: `action=formAction` |
| Expect full SvelteKit/Next | Use SPA backends; Kit/Next are roadmap |
| Hand-edit `dist/` and expect persistence | Change `src/**/*.cord` and re-run |

More for AI authors: [AI.md](./AI.md) and root [AGENTS.md](../AGENTS.md).
