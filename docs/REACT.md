# Cordlang ↔ React

Mapeo del modelo mental de [React](https://es.react.dev/) al lenguaje `.cord`.
Cordlang **no es un subset de JSX**: es una capa compacta que se compila a React
(hooks, JSX, React Router) con buenas prácticas.

Referencias oficiales usadas:

- [Aprende React](https://es.react.dev/learn)
- [Hooks integrados](https://react.dev/reference/react/hooks)
- [API de React](https://es.react.dev/reference/react)

---

## 1. Componentes

| React | Cordlang |
|-------|----------|
| `function Card({ title }) { return ... }` | `def Card` + `props title` o archivo `Card.cord` |
| Composición `<Card><p/></Card>` | anidar bajo el tag PascalCase |
| `children` | `slot` dentro del componente (no layout) |
| `Fragment` `<>...</>` | `fragment` o varios roots en el body |
| `key` en listas | `for x in xs key=x.id` |

```cord
# components/Card.cord
props title=""
col p=16
  h3 "#{title}" bold
  slot
```

```cord
Card title="Hola"
  p "cuerpo"
```

→

```jsx
function Card({ title = "", children }) {
  return (
    <>
      <div className="..."><h3>{title}</h3>{children}</div>
    </>
  );
}
```

---

## 2. State Hooks

### useState

| React | Cordlang |
|-------|----------|
| `const [n, setN] = useState(0)` | `state n=0` |
| `setN(n + 1)` | `@click=setN(n + 1)` |

### useReducer

```cord
reducer cart = cartReducer init=[]
```

→ `const [cart, dispatch] = useReducer(cartReducer, [])`

*(El reducer JS se define en el proyecto exportado o se importa por nombre.)*

---

## 3. Context Hooks

### createContext + Provider + useContext

```cord
context Theme = "light"

def AppShell
  ctx theme = Theme
  provide Theme value=theme
    slot
```

→

```jsx
// contexts.jsx
export const Theme = createContext("light");

// component
const theme = useContext(Theme);
return <Theme.Provider value={theme}>{children}</Theme.Provider>;
```

| React | Cordlang |
|-------|----------|
| `createContext` | `context Name [= default]` |
| `useContext(X)` | `ctx var = X` |
| `<X.Provider value={v}>` | `provide X value=v` |

---

## 4. Ref Hooks

```cord
ref inputEl
input text ref=inputEl
```

→ `const inputEl = useRef(null)` + `ref={inputEl}`

---

## 5. Effect Hooks

```cord
effect deps=(roomId) cleanup=disconnect(roomId)
  connect(roomId)
```

→

```jsx
useEffect(() => {
  connect(roomId);
  return () => {
    disconnect(roomId);
  };
}, [roomId]);
```

| React | Cordlang |
|-------|----------|
| `useEffect(fn, deps)` | `effect deps=(a,b)` + body indentado o `run=` |
| cleanup | `cleanup=expr` |
| `useLayoutEffect` | `layoutEffect deps=(...)` + body |
| `useInsertionEffect` | `insertionEffect deps=(...)` + body |
| `useEffectEvent` | `effectEvent name = fn` o body indentado |

```cord
insertionEffect deps=()
  injectStyles()

effectEvent onMsg = handleMessage
# o:
effectEvent onMsg
  handleMessage(msg)
```

→

```jsx
useInsertionEffect(() => {
  injectStyles();
}, []);

const onMsg = useEffectEvent(handleMessage);
// o: const onMsg = useEffectEvent((..._args) => { handleMessage(msg); });
```

---

## 6. Performance Hooks

| React | Cordlang |
|-------|----------|
| `useMemo(() => expr, deps)` | `computed name = expr` o `memo name = expr` |
| `useCallback(fn, deps)` | `callback name = fn deps=(a)` |
| `useTransition` | `transition isPending, startTransition` |
| `useDeferredValue(x)` | `deferred deferredX = x` |

---

## 7. Other Hooks

| React | Cordlang |
|-------|----------|
| `useId()` | `id fieldId` |
| `useParams()` | `params id, slug` |
| `useNavigate()` | `navigate go` |
| `useSyncExternalStore` | `externalStore` / `syncStore` |
| `useActionState` | `action form = submitForm init=null pending=saving` |
| `useImperativeHandle` | `imperativeHandle` (+ `def X forwardRef`) |
| Custom hooks | archivo `.cord` + `use` / o JS importado en dist |

### useSyncExternalStore

```cord
externalStore snapshot = subscribe getSnapshot getServerSnapshot=null
# o:
syncStore selected = store.subscribe store.getSnapshot
```

→

```jsx
const snapshot = useSyncExternalStore(subscribe, getSnapshot);
const selected = useSyncExternalStore(store.subscribe, store.getSnapshot);
```

### useImperativeHandle + forwardRef

```cord
def FancyInput forwardRef
  ref inputEl
  imperativeHandle ref=inputEl
    focus: () => inputEl.current?.focus()

  input text ref=inputEl
```

→

```jsx
export default forwardRef(function FancyInput({}, ref) {
  const inputEl = useRef(null);
  useImperativeHandle(inputEl, () => ({
    focus: () => inputEl.current?.focus()
  }), []);
  return (<>...</>);
});
```

`imperativeHandle` defaults `ref` to the `forwardRef` parameter when you omit
`ref=…`. Optional `deps=(a,b)` maps to the third argument of
`useImperativeHandle`.

### React 19 Actions polish

```cord
action form = submitForm init=null pending=saving
form action=formAction
  btn "Enviar"
```

- Emits `const [form, formAction, saving] = useActionState(submitForm, null)`
- If `submitForm` is a bare identifier, emits a default stub:
  `async function submitForm(prev, formData) { return null }`
- `form action=formAction` gets `aria-busy={saving}`
- Nested `btn` gets `disabled={saving}` when pending alias is known
- `if saving` works via the pending name end-to-end

---

## 8. Renderizado

| React | Cordlang |
|-------|----------|
| `{cond && <A/>}` | `if cond` / `else` |
| `list.map` | `for item in list key=item.id` |
| `{expr}` | `#{expr}` en strings |
| Eventos `onClick` | `@click=handler` |
| Controlled input | `input text bind=name` → `value` + `setName` |

---

## 9. Routing (ecosistema React)

React no incluye router; Cordlang usa **React Router** (recomendado en apps SPA):

| React Router | Cordlang |
|--------------|----------|
| `<BrowserRouter><Routes>` | `route` en `app.cord` |
| `<Route path element>` | `route /path => pages/Page` |
| layout routes | `layout` + `slot` → `<Outlet/>` |
| `<Link to>` | `link "Text" to=/path` |
| `useParams` | `params id` |
| `useNavigate` | `navigate go` |

---

## 10. Módulos / archivos

| React / JS | Cordlang |
|------------|----------|
| `import X from './X'` | `use ./components/X` |
| `import { X } from '…'` | `import X from path` |
| pages / components folders | `src/pages`, `src/components`, `src/layouts` |

---

## 11. Qué no se traduce 1:1 (por diseño)

Cordlang es un **DSL de UI**. Estas piezas de React se contemplan así:

| Tema React | Enfoque Cordlang |
|------------|------------------|
| Server Components / RSC | Roadmap (Next/Remix target) |
| Suspense / lazy | ✅ `suspense` + `lazy Chart = components/Chart` |
| Portals | ✅ `portal to=document.body` |
| Error boundaries | ✅ `errorBoundary` + `fallback` (helper class en scaffold) |
| useActionState (React 19) | ✅ `action form = submitForm init=null` |
| useLayoutEffect | ✅ `layoutEffect deps=(...)` |
| Class components (UI) | No — solo función + hooks (EB es la excepción necesaria) |
| Lógica JS arbitraria | Expresiones en hooks / handlers; JS libre en `dist/` |
| Meta-framework (Next) | Fuera de scope del CLI actual (Vite SPA) |

---

## 12. Checklist vs documentación React

### Aprendizaje (es.react.dev/learn)

- [x] Componentes y props  
- [x] Estado (`useState`)  
- [x] Render condicional  
- [x] Listas y keys  
- [x] Eventos  
- [x] Formularios controlados (`bind`)  
- [x] Compartir estado / lifting (props + context)  
- [x] Effects  
- [x] Refs  
- [x] Context  
- [x] Memo / callback  
- [x] Suspense / lazy / portal / error boundary  
- [x] Escape hatches (imperative handle, forwardRef, external store)

### Hooks (react.dev/reference/react/hooks)

- [x] useState  
- [x] useReducer  
- [x] useContext  
- [x] useRef  
- [x] useEffect  
- [x] useLayoutEffect  
- [x] useMemo  
- [x] useCallback  
- [x] useTransition  
- [x] useDeferredValue  
- [x] useId  
- [x] useActionState (React 19)  
- [x] useInsertionEffect / useEffectEvent  
- [x] useSyncExternalStore  
- [ ] useDebugValue (no DSL surface — use JS escape)  
- [x] useImperativeHandle  

### App structure

- [x] Multi-file components  
- [x] Client router (React Router)  
- [x] Layouts  
- [x] Suspense / lazy code-splitting  
- [x] Error boundaries  
- [x] Portals  
- [x] Build Vite + React 18/19  

### APIs de componentes

| React | Cordlang |
|-------|----------|
| `<Suspense fallback={...}>` | `suspense` + `fallback` / bloque `fallback` |
| `lazy(() => import(...))` | `lazy Name = path/to/mod` |
| `createPortal(ui, node)` | `portal to=document.body` |
| Error boundary | `errorBoundary` + `fallback` |
| `useActionState` | `action form = submitForm init=null pending=saving` |
| `<form action={fn}>` | `form action=formAction` (+ `aria-busy` + btn `disabled`) |
| `forwardRef` + `useImperativeHandle` | `def X forwardRef` + `imperativeHandle` |
| `fetch` + `useEffect` | `fetch products = "/api/products.json"` → `products` / `productsLoading` / `productsError` |

### Static assets

Project `public/` is copied to `dist/react/public` on scaffold. Vite serves
`public/logo.png` as `/logo.png`.

---

## Phase D — Advanced hooks + Testing Library

Kitchen-sink: `examples/phase_d_hooks.cord`.

| Feature | Cordlang |
|---------|----------|
| `useInsertionEffect` | `insertionEffect deps=()` + body |
| `useEffectEvent` | `effectEvent onMsg = handleMessage` |
| `useSyncExternalStore` | `externalStore` / `syncStore` |
| `forwardRef` | `def FancyInput forwardRef` |
| `useImperativeHandle` | `imperativeHandle ref=inputEl` + object body |
| Actions polish | `pending=saving` + form `aria-busy` + btn `disabled` |

### Out of scope vs meta backends

- **Remix** / **React Native** — not implemented (docs / roadmap only).
- **Next.js** — **meta-backend MVP** exists (`cordlang run next` / `--backend next`):
  wraps the React SPA emit + scaffold notes. **Not** full RSC, App Router, or
  server components. Details: [`NEXT.md`](./NEXT.md).
- The DSL surface above remains client React (Vite SPA) for day-to-day use.

### Testing Library recipe

With Vitest + `@testing-library/react` on the generated project:

```jsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import PhaseDDemo from './PhaseDDemo'; // or Counter

describe('PhaseDDemo', () => {
  it('increments counter', () => {
    render(<PhaseDDemo />);
    fireEvent.click(screen.getByText('+'));
    expect(screen.getByText(/Count:\s*1/)).toBeInTheDocument();
  });
});
```

Pattern: compile `.cord` → JSX, then test the **generated** component as any
React component. Prefer RTL queries by role/text over implementation details.

---

## Ejemplo kitchen-sink

Ver `examples/react_hooks.cord`, `examples/phase_d_hooks.cord`,
`examples/fetch_form.cord` y `my-app/src/pages/*`.
