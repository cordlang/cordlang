# Cordlang — Language design (reference)

> **Nota:** este documento nació como `idea.md` cuando Cordlang era un boceto.
> Hoy el compilador y los backends React/Svelte/HTML son **reales** (ver README y roadmap).
> Se conserva aquí como **especificación de diseño y sintaxis** — no normativa.
>
> **Normative language spec:** [`docs/SPEC.md`](./SPEC.md)  
> Estado de implementación: [`docs/ROADMAP.md`](./ROADMAP.md) · mapas [`REACT.md`](./REACT.md) · [`SVELTE.md`](./SVELTE.md) · [`IR.md`](./IR.md) · [`ARCHITECTURE.md`](./ARCHITECTURE.md)

---
# Cordlang — Language Design

## Arquitectura hexagonal

```
CLI (inbound) → Application services → Ports ← Outbound adapters
                                           ├── Lexer / Parser / Compiler
                                           ├── FS
                                           └── Backends (React, …)
```

Commands:
- `cordlang init [name]` → scaffold Cordlang project
- `cordlang run react` → compile entry + full React app in `dist/react`
- `cordlang build react` → compile only
- `cordlang compile file.cord` → single-file compile

## Filosofía

Cordlang es un lenguaje intermedio ultra-compacto para describir interfaces. No es un framework — es una capa universal que se compila a React, Vue, Svelte, HTML, etc.

El beneficio principal: ~3-5x menos tokens al generar UI con IA.

## Sintaxis

### Regla #1: Todo es una línea

```
tag [child...] [attr...]
```

- El primer token después del `tag` es el child implícito (string o type shorthand)
- Los `key=value` son atributos
- `@evento=handler` son eventos
- Solo el nombre = boolean attr

### Elementos básicos

```
// Tipografía — child string directo
h1 "Bienvenido"
h2 "Productos" size=xl
p "Descripción" muted
span user.name color=primary bold

// Layout — child es contenido anidado
row between center p=16
col gap=16
stack gap=8
box w=400 h=300 bg=gray-100 rounded=12
grid cols=3 gap=16

// Media
img src=product.url aspect=4/3 fit=cover
icon name=search size=24
video src=tutorial.mp4

// Inputs
input text placeholder="Nombre" required
input email placeholder="Email" bind=form.email
input password
textarea rows=4 placeholder="Mensaje"
select options=categories bind=form.category
checkbox label="Acepto términos" bind=form.agree
radio name=tier value=basic label="Básico"

// Interactivos
btn "Enviar" @click=submitForm variant=primary
link "Ver más" to=/products
button "X" @click=close variant=ghost size=sm

// Semánticos
header sticky top=0 bg=white z=50
nav
main
section
article
aside
footer muted

// Contenedores lógicos
fragment
group
```

### Eventos con @

```
btn "Login" @click=handleLogin
input text @change=(setQuery e.value)
form @submit.prevent=handleSubmit
div @mouseenter=onHover @mouseleave=onLeave
```

Los eventos mapean a:
- React: `onClick={handleLogin}`
- Vue: `@click="handleLogin"`
- Svelte: `on:click={handleLogin}`
- HTML: `onclick="handleLogin()"`

### Estilos compactos

Estilo inline con mapa entre paréntesis:

```
col style=(bg=white p=16 rounded=12 shadow=md gap=8)
```

Shorthands de estilo:
```
p     → padding
px    → padding-left padding-right
py    → padding-top padding-bottom
m, mx, my → margin
bg    → background
rounded → border-radius
shadow → box-shadow
gap   → gap
size  → font-size
leading → line-height
tracking → letter-spacing
w, h  → width, height
max-w → max-width
min-h → min-height
fit   → object-fit
aspect → aspect-ratio
z     → z-index
op    → opacity
```

Colores con escala: `blue-500`, `gray-100`, `red-200`, `green-600`, o tokens: `$primary`, `$bg`, `$text`.

### String interpolation

```
h1 "Hola, #{user.name}!"
p "Precio: $#{product.price}"
btn "Carrito (#{cart.length})"
```

### Indentación = anidación

```
page
  header
    nav
      link "Inicio" to=/
      link "Productos" to=/products
    btn "Carrito" @click=toggleCart
  main
    grid cols=3 gap=16
      for product in products
        ProductCard product=product
  footer
    p "© 2026" muted center
```

### Control de flujo

```
// Loop
for product in products
  ProductCard product=product key=product.id

for i in range(0, 5)
  btn "#{i}" @click=(selectPage i)

// Condicional
if isLoggedIn
  btn "Perfil" @click=goToProfile
else
  btn "Login" @click=showLogin

// If inline (en una línea)
span (if inStock "En stock" "Agotado")
```

### Componentes

```
def ProductCard
  props product, onAdd

  col style=(bg=white rounded=12 shadow=md overflow=hidden)
    img src=product.image aspect=4/3 fit=cover
    stack gap=8 p=16
      h3 product.name
      p product.description muted lines=2
      row between center
        span "$#{product.price}" color=primary bold size=xl
        btn "Agregar" @click=onAdd variant=primary
```

```
def Counter
  state count=0
  props label=""

  col gap=8 center p=16
    p "Cuenta: #{count}" size=lg
    row gap=8
      btn "-" @click=setCount(count - 1) variant=outline
      span count size=2xl bold
      btn "+" @click=setCount(count + 1) variant=primary
```

### Expresiones

```
// Aritméticas
btn @click=setCount(count + 1)
p "Total: $#{items |> sum(.price)}"

// Lógicas
if cart.length > 0 && isAuthenticated
  btn "Checkout" @click=checkout

// Pipe operator
computed total = items |> filter(.active) |> map(.price) |> sum()

// Llamadas a funciones
btn @click=submitForm(form)
input @change=(validateField field.name e.value)
```

### Temas / Design Tokens

```
theme shop
  primary: "#2563eb"
  accent: "#f59e0b"  
  bg: "#ffffff"
  text: "#1a1a1a"
  muted: "#6b7280"
  radius: 12
  font: "Inter"
  shadow: "0 4px 6px -1px rgba(0,0,0,0.1)"
```

Uso: `color=$primary`, `bg=$bg`, `rounded=$radius`

### Routing

```
route / => HomePage
route /products => ProductListPage
route /products/:id => ProductDetailPage props=productId
route /cart => CartPage

layout default
  header
    nav
      link "Home" to=/
    slot  // ← donde se renderiza la ruta
  footer
```

## Token Efficiency (comparativa)

### React (JSX) — ~45 tokens
```jsx
<div className="flex flex-col gap-4 p-6 bg-white rounded-xl shadow-md">
  <h2 className="text-xl font-bold text-gray-900">Hello</h2>
  <button onClick={handleSubmit}>Click</button>
</div>
```

### Cordlang — ~15 tokens (3x menos)
```
col gap=16 p=24 bg=white rounded-xl shadow=md
  h2 "Hello" size=xl bold
  btn "Click" @click=handleSubmit
```

## Gramática Formal (BNF draft)

```
program       = { stmt | def | theme | route }
stmt          = tag [args] { attr } [ "\n" indent { stmt } dedent ]
tag           = identifier | "for" | "if" | "else"
args          = string | identifier       (* child implícito *)
attr          = "@" ident "=" expr        (* evento *)
              | ident "=" expr            (* key=value *)
              | ident                     (* boolean flag *)
expr          = string | number | ident | "(" ... ")"
              | "#{" expr "}"             (* interpolación *)
              | expr "|>" ident "(" ... ")"  (* pipe *)
def           = "def" ident "\n" indent { def_stmt } dedent
def_stmt      = "props" ident { "," ident }
              | "state" ident "=" expr { "," ident "=" expr }
              | "computed" ident "=" expr
              | stmt
theme         = "theme" ident "\n" indent { ident ":" value } dedent
route         = "route" string "=>" ident [ "props" ident { "," ident } ]
```

## Pipeline de Compilación

```
┌──────────┐    ┌─────────┐    ┌──────────┐    ┌──────────┐
│  .cord   │───▶│  Lexer  │───▶│  Parser  │───▶│   AST    │
│  source  │    │ (token) │    │ (indent) │    │  (tree)  │
└──────────┘    └─────────┘    └──────────┘    └──────────┘
                                                    │
                    ┌───────────────────────────────┘
                    ▼
             ┌──────────────┐    ┌──────────┐    ┌──────────────┐
             │  Semantic    │───▶│    IR    │───▶│   Backend    │
             │  Analysis    │    │ (canon)  │    │  (generator) │
             └──────────────┘    └──────────┘    └──────────────┘
                                                     │
                          ┌──────────────────────────┤
                          ▼           ▼              ▼
                    ┌─────────┐ ┌─────────┐    ┌─────────┐
                    │  React  │ │   Vue   │    │  HTML   │
                    │  .jsx   │ │  .vue   │    │  .html  │
                    └─────────┘ └─────────┘    └─────────┘
```

## Backend Mapping Examples

### Cordlang → React

```
btn "Click" @click=handle primary
```
→
```jsx
<button className="btn btn-primary" onClick={handle}>Click</button>
```

```
col gap=16 p=24 bg=white
  h1 "Title"
  p "Body"
```
→
```jsx
<div className="flex flex-col gap-4 p-6 bg-white">
  <h1 className="text-2xl font-bold">Title</h1>
  <p className="text-base">Body</p>
</div>
```

### Cordlang → Vue

Mismo source Cordlang, output Vue SFC:
```vue
<template>
  <div class="flex flex-col gap-4 p-6 bg-white">
    <h1 class="text-2xl font-bold">Title</h1>
    <p class="text-base">Body</p>
  </div>
</template>
```

### Cordlang → HTML+CSS

Mismo source → HTML semántico con CSS nativo o Tailwind:
```html
<div class="flex flex-col gap-4 p-6 bg-white rounded-xl shadow-md">
  <h1 class="text-2xl font-bold text-gray-900">Title</h1>
  <p class="text-base text-gray-600">Body</p>
</div>
```
