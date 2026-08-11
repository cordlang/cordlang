# Cordlang Language Specification (v1.0)

**Status:** **1.0 syntax freeze** for the documented web subset. Official targets are React / Svelte / Vue and native ESM preview; HTML remains the legacy preview target.
**Implementation truth:** `src/adapters/outbound/parser/parser.c` + `src/domain/{ast,ir,expr}.*`.  
**Design / history:** [`LANGUAGE.md`](./LANGUAGE.md) (non-normative).  
**Attrs catalog:** [`schema/attrs.json`](./schema/attrs.json).

When this document and the parser disagree, **file an issue** and treat the parser as temporary source of truth until the mismatch is resolved here.

Version: **1.0** (Horizonte A gate). Breaking changes require a major bump and ROADMAP note.

---

## 1. Source form

- Encoding: UTF-8.
- Line endings: LF preferred; CRLF accepted (normalized in tools).
- Comments: `#` to end of line (outside strings).
- Nesting: **indentation** (spaces). A child block is a newline + deeper indent than its parent. Dedent closes the block.
- Tabs: discouraged; `cordlang fmt` converts to spaces.

A **program** is a sequence of top-level statements: component defs, routes, layouts, theme blocks, imports, and UI/control statements (file-as-component when no `def` wraps the body).

---

## 2. Lexical grammar (informative)

```
ident       = letter { letter | digit | "_" | "-" }
number      = digit { digit } [ "." digit { digit } ]
string      = '"' { char | escape } '"'
escape      = '\' any
interp      = '#{' expr '}'          ; interpolation inside strings / text
lit_hash    = '\#{'                  ; literal characters # { … } (not interp)
event       = '@' ident [ '.' ident ] '=' expr
attr_kv     = ident '=' value
attr_flag   = ident
```

`value` may be a string, number, ident, dotted path, parenthesized expr, or color/token form (`$primary`, `blue-500`).

---

## 3. Core syntax (BNF)

Aligned with the parser’s keyword dispatch (not a full PEG of every tag).

```
program        = { top_stmt }

top_stmt       = def_stmt
               | use_stmt
               | route_stmt
               | layout_stmt
               | theme_stmt
               | stmt

def_stmt       = "def" ident [ "forwardRef" ] NL INDENT { def_body } DEDENT
def_body       = props_decl | state_decl | computed_decl
               | hook_decl | stmt

props_decl     = "props" prop_item { "," prop_item }
prop_item      = ident [ ":" type_name ] [ "=" value ]
type_name      = "string" | "number" | "boolean" | "any"

state_decl     = "state" ident "=" value { "," ident "=" value }
computed_decl  = "computed" ident "=" expr

hook_decl      = effect_like | "ref" ident
               | "ctx" ident "=" ident
               | "context" ident "=" value
               | "provide" ident "value" "=" value
               | "params" ident { "," ident }
               | "action" ident "=" expr { attr_kv }
               | "fetch" ident "=" string
               | "load" ident "=" string
               | "lazy" ident "=" path
               | "store" ident "=" value
               | "writable" ident "=" value
               | "await" expr [ "then" "=" ident ]
               | "snippet" ident [ "(" idents ")" ] NL INDENT { stmt } DEDENT
               | "render" ident [ "(" args ")" ]

effect_like    = ( "effect" | "layoutEffect" | "insertionEffect"
                 | "effectEvent" | "externalStore" | "syncStore"
                 | "imperativeHandle" ) …

use_stmt       = ( "use" | "import" ) path

route_stmt     = "route" path_pattern "=>" target { route_opt }
route_opt      = "props" idents | "layout" "=" ident | "lazy"

layout_stmt    = "layout" [ ident ] NL INDENT { stmt } DEDENT

theme_stmt     = "theme" ident NL INDENT { ident ":" value } DEDENT

stmt           = control_stmt | element_stmt | "slot" | "portal" …

control_stmt   = "if" expr NL INDENT { stmt } DEDENT [ "else" … ]
               | "for" ident "in" expr [ "key" "=" expr ] NL INDENT { stmt } DEDENT

element_stmt   = tag [ implicit_child ] { attr } [ NL INDENT { stmt } DEDENT ]
tag            = ident
implicit_child = string | ident
attr           = event | attr_kv | attr_flag
```

`path_pattern` supports `/`, static segments, and `:param` (e.g. `/shop/:id`).

---

## 4. Semantics (summary)

### 4.1 Components

- `def Name` introduces a named component; body decls then markup.
- A file without `def` may act as a single body-only component (entry / page).
- `props` are inputs; optional `: type` is validated by `cordlang check` (`string` \| `number` \| `boolean` \| `any`).
- `state x=…` creates reactive local state; codegen exposes `setX`-style updaters.
- `computed y = expr` is derived state (React `useMemo` / Svelte `$derived`).

### 4.2 Control flow

- `if` / `else`: exclusive branches; condition is an expr lowered via `expr_normalize`.
- `for x in xs key=…`: list iteration; `key=` recommended for stable identity.
- Interpolation `#{expr}` in strings/text binds to live values; `\#{` is literal.

### 4.3 Modules and routing

- `use` / `import` resolve relative module paths under the project root (jail).
- `route / => Target` wires SPA navigation; `layout=` and `lazy` are route options.
- `layout` + `slot` compose chrome around routed pages.
- `params id` reads path params in page components.

### 4.4 Events and binding

- `@click=handler` (and other `@event`) map per backend (React `onClick`, Svelte `onclick`, …).
- `bind=name` is two-way binding for form controls.

### 4.5 Lowering to IR

After parse, `ir_from_ast` builds `IrProgram` / `IrNode` (`domain/ir.h`).  
Expr strings are normalized with `domain/expr`.  
Backends must emit from IR (`generate_from_ir`), not re-parse Cord.  
Details: [`IR.md`](./IR.md), [`ARCHITECTURE.md`](./ARCHITECTURE.md).

---

## 5. Out of scope (this version)

The following are **not** part of the Cordlang **1.0** language surface (may arrive as backends / experimental):

- Solid / Flutter / SwiftUI / Compose backends (experimental). Vue is an **Official** SPA backend ([BACKENDS.md](./BACKENDS.md)) but adds **no** Vue-only language keywords.
- SvelteKit file routing / SSR, Next.js RSC as language features.
- Arbitrary JavaScript / TypeScript embedded in `.cord`.
- JSX/`className`/`onClick` keywords inside `.cord` (rejected by `check`).
- LLM calls on the `compile` path.
- Dynamic native plugins (`dlopen` / WASM) — IR passes are compiler features, not language syntax.

Experimental or backend-specific attrs (e.g. Svelte `transition=`, `use=`) may appear in codegen maps before they are frozen here; prefer [`REACT.md`](./REACT.md) / [`SVELTE.md`](./SVELTE.md) / [`VUE.md`](./VUE.md) until listed above.

---

## 6. Spec ↔ parser checklist

| Production / topic | Parser / IR | Fixture or golden |
|--------------------|-------------|-------------------|
| `def` + `state`/`props` | `parser.c` def body | `tests/fixtures/basic_counter.cord` |
| `#{…}` / `\#{…}` | interp + escape | `tests/regression/escape-hash-brace/` |
| Quoted strings / dotted names | string lex | `tests/regression/string-dotted/` |
| `if` / `for` | control keywords | `tests/fixtures/if_for.cord` |
| `route` / nested layout | route_stmt | `tests/fixtures/nested_routes.cord` |
| `slot` / components | layout/slot | `tests/fixtures/component_slot.cord` |
| Typed props | check_service | `tests/fixtures/typed_props_ok.cord` |
| JSX trap `className` | check | `tests/regression/ia-classname-trap/` |
| Phase D hooks | hook_decl | `tests/fixtures/react_phase_d.cord` |
| Phase E Svelte surface | snippet/store/await | `tests/fixtures/svelte_phase_e.cord` |
| Attr schema | `schema/attrs.json` | LSP / check warnings |

---

## 1.0 frozen subset

**In freeze (stable):** indentation UI, `def`/`props`/`state`/`computed`, `if`/`for`, `#{…}`, modules/`route`/`layout`/`slot`, events `@`, `bind`, theme tokens, typed props (`string|number|boolean|any`), shared hooks lowered to IR (`ref`, context/provide, lazy, portal, effects, fetch/await, snippets/stores where mapped).

**Explicitly out of 1.0 language freeze (backend / experimental):** Solid/email/PDF/Next/Kit/native backends; `$bindable` / `{#key}` / `class:`/`style:` Svelte-only polish; arbitrary embedded JS/TS; LLM-on-compile. (Vue is Official SPA emit — no Vue-only language keywords.)

---

## 7. Change process

1. Propose syntax in an issue / PR with a fixture under `tests/fixtures` or `tests/regression`.
2. Update this file and the matching map docs in the same change.
3. Keep React and Svelte backends in sync when the construct is shared IR.
4. After 1.0, breaking syntax changes require a major version bump and an entry in ROADMAP.md.

---

*Last updated: Horizonte A gate — SPEC v1.0.*
