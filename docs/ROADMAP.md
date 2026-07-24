# Cordlang Super Roadmap — IR para IA (no otro React)

> **Posicionamiento (no negociable):** Cordlang es un **lenguaje intermedio** optimizado para vibecode/IA que describe UI y compila a destinos reales. **No** es otro framework (router/estado/bundler/Vite/Next propios): reutiliza ecosistemas (Vite, React Router, Svelte runes).

Mensaje público:

> La forma más rápida de construir UI con IA — un `.cord` → IR → React / Svelte / HTML (y más después).

Documentos relacionados:

- [`docs/REACT.md`](./REACT.md) — mapa React → Cordlang
- [`docs/SVELTE.md`](./SVELTE.md) — mapa Svelte → Cordlang + checklist de paridad
- [`docs/IR.md`](./IR.md) — contrato del IR para backends
- [`docs/AI.md`](./AI.md) — contrato para modelos + schema de attrs
- [`docs/TEMPLATES.md`](./TEMPLATES.md) — plantillas Cord-nativas
- [`docs/LSP.md`](./LSP.md) — editor / LSP mínimo

---

## 0. Norte (visión)

```
src/**/*.cord
   │
   ├─► AST / IR canónico
   │
   ├──► Backend React   → dist/react  (Vite + RR)
   ├──► Backend Svelte  → dist/svelte (Vite + runes)
   ├──► Backend HTML    → preview nativo en el CLI
   └──► (Horizonte B) Vue / Solid / email / …

IA / skills / cordlang ai  ──escribe──►  .cord
check / fmt / LSP / analyze ──valida──►  .cord
```

**Éxito 12 meses (Horizonte A):** IA escribe `.cord` → `check`/`analyze` atrapan traps → `run react|svelte --check` verde; LSP mínimo usable; paridad SPA documentada; 3+ templates Cord.

**Éxito producto:** el mismo `src/**/*.cord` sin reescribir UI:

```bash
cordlang run              # preview instantáneo
cordlang run react        # app React
cordlang run svelte       # app Svelte
```

---

## 1. Estado actual (baseline)

### 1.1 Núcleo del lenguaje (compartido)

| Feature | Estado |
|---------|--------|
| Sintaxis indentada, tags, attrs, eventos `@` | ✅ |
| Componentes `def` / body-only por archivo | ✅ |
| `props` / `state` / `computed` | ✅ |
| `if` / `else` / `for` + `key=` | ✅ |
| Interpolación `#{expr}` | ✅ |
| Módulos `use` / `import` / rutas por path | ✅ |
| `layout` + `slot` | ✅ |
| `route` | ✅ |
| `theme` → CSS vars | ✅ parse + `theme_css` IR (G1) |
| Preview HTML nativo en el `.exe` | ✅ (state reactivo básico C8) |
| IR canónico + dump `--ir` | ✅ |
| Expr mini-parser | ✅ |
| `cordlang check` + diagnostics | ✅ |
| `cordlang fmt` / `fmt --check` | ✅ |
| `cordlang symbols` / `goto` | ✅ |
| Source maps VLQ + attribution | ✅ (G7) |

### 1.2 Backend React

| Área | Estado |
|------|--------|
| Scaffold Vite + Tailwind + multi-file | ✅ |
| React Router (layout, Link, Outlet, params, navigate) | ✅ |
| useState / useReducer / useEffect / useLayoutEffect | ✅ |
| useRef / useContext / provide | ✅ |
| useMemo / useCallback / useTransition / useDeferredValue / useId | ✅ |
| useActionState + form action | ✅ |
| Suspense / lazy / portal / errorBoundary | ✅ |
| bind controlado | ✅ |
| RSC / Next / Remix | 🟡 meta-backend `next` (client wrap; no full RSC) |
| useSyncExternalStore / useInsertionEffect / useEffectEvent | ✅ |
| useImperativeHandle + forwardRef | ✅ |
| Tests de snapshot del codegen | ✅ |

### 1.3 Backend Svelte

| Área | Estado |
|------|--------|
| Scaffold Vite + Svelte 5 runes + Tailwind | ✅ |
| Multi-file pages/components/layouts | ✅ |
| `$state` / `$props` / `$derived` / `$effect` | ✅ |
| if / each / interpolación / eventos / bind | ✅ |
| Hash router propio + links `#/…` | ✅ |
| setContext / getContext | ✅ |
| Snippets / actions / await / portal | ✅ (Fase E) |
| SvelteKit (SSR/SSG/file routing) | 🟡 MVP meta-backend `sveltekit` (wrap SPA emit) |
| Lazy routes (`import()` + `{#await}`) | ✅ |
| Transitions / animations (`transition=` / in/out) | ✅ |
| Stores (`store` / `writable`) | ✅ |

---

## 2. Matriz de paridad React ↔ Svelte

Leyenda: ✅ hecho · 🟡 parcial · ❌ no · ≈ equivalente idiomático · — no aplica

| Concepto Cordlang / UI | React target | Svelte target | Prioridad |
|------------------------|--------------|---------------|-----------|
| Componentes + props | ✅ function + props | ✅ `$props()` | P0 done |
| State local | ✅ useState | ✅ `$state` | P0 done |
| Derived / memo | ✅ useMemo | ✅ `$derived` | P0 done |
| Effects | ✅ useEffect | ✅ `$effect` | P0 done |
| Layout effects | ✅ useLayoutEffect | ≈ `$effect.pre` | P1 |
| Lists + keys | ✅ map + Fragment | ✅ `{#each (key)}` | P0 done |
| Conditionals | ✅ && / ?: | ✅ `{#if}` | P0 done |
| Events | ✅ onClick | ✅ onclick | P0 done |
| Two-way bind | ✅ controlled | ✅ bind:value | P0 done |
| Refs / DOM | ✅ useRef | ✅ bind:this | P0 done |
| Context | ✅ createContext/Provider | ✅ setContext/getContext | P0 done |
| Routes + layout | ✅ React Router | ✅ hash router | P0 done |
| Nested routes / params | ✅ params + nested layouts | ✅ path params + nested | P1 done |
| Lazy / code-split | ✅ lazy+Suspense | ✅ import() + `{#await}` | P1 done |
| Portals | ✅ createPortal | ✅ `portal` / `use:portal` | P2 done |
| Error boundaries | ✅ class EB | ≈ onerror / boundary | P2 ≈ |
| Forms / actions | ✅ useActionState | ✅ formAction helper | P1 done |
| Transitions UI | ✅ useTransition | ✅ `transition=` / in/out | P2 done |
| Theme tokens | ✅ CSS vars | ✅ CSS vars | P0 done |
| Accessibility attrs | ✅ `aria-*`/`data-*` + img alt check | ✅ same | P1 done |
| Preview = same AST live | 🟡 HTML + state básico | 🟡 HTML + state básico | P1 done-ish |
| Typecheck / diagnostics | ✅ `cordlang check` | ✅ shared | P1 done |
| Source maps .cord→out | ✅ VLQ from markers | ✅ VLQ from markers | P2 done |
| Hot reload .cord | ✅ `--watch` | ✅ `--watch` | P1 done |
| Vue / Solid backends | ✅ Vue 3 + Solid (IR-first) | — | F1/F2 done |
| email / PDF / Next / Kit | ✅ estáticos + meta | ✅ | F8–F11 |

---

## 3. Fases del roadmap

### Fase A — Consolidar el core ✅ DONE (agents 2026-07-23)

**Objetivo:** que `my-app` sea demo de producción confiable en React *y* Svelte.

| # | Item | React | Svelte | Estado |
|---|------|-------|--------|--------|
| A1 | Suite de fixtures `.cord` → snapshot codegen | ✅ | ✅ | `tests/fixtures` + `tests/golden` + `tests/run_tests.ps1` (8/8) |
| A2 | Context nativo Svelte (`context` / `ctx` / `provide`) | ✅ | ✅ | `contexts.js` + setContext/getContext |
| A3 | Theme tokens → CSS vars en ambos | ✅ | ✅ | `theme_css.c` → `src/theme.css` + `color=$primary` |
| A4 | Params de ruta `:id` en Svelte hash router | ✅ RR | ✅ | `matchRoute` + props `{...params}` |
| A5 | Fix quality codegen (expr, strings, deps) | 🟡 | 🟡 | goldens estables; polish residual en edges |
| A6 | CLI: `cordlang run react\|svelte --check` | ✅ | ✅ | npm install si falta + `vite build` |

**Definition of Done A:** ✅  
`cordlang run react --check` y `cordlang run svelte --check` en `my-app` → vite build OK.  
Suite golden: `tests\run_tests.bat`.

**Notas post-A:**
- Context en Svelte solo fluye padre→hijo (preferir `provide` en layout).
- Ampliar goldens con theme/context/params cuando se toque codegen otra vez.
- Siguiente: ~~Fase B~~ → ~~Fase C~~ → **Fase D/E** (deep React/Svelte) o backends sobre IR.

---

### Fase B — Paridad de app real ✅ DONE (agents 2026-07-23)

**Objetivo:** features que piden apps no-triviales.

| # | Item | React | Svelte | Estado |
|---|------|-------|--------|--------|
| B1 | Nested routes + layouts | ✅ `layout=shop` + RR groups | ✅ layout dinámico | ✅ |
| B2 | Lazy routes | ✅ lazy + Suspense | ✅ `import()` + `{#await}` | ✅ |
| B3 | Forms productivos | ✅ useActionState + bind | ✅ formAction helper | ✅ |
| B4 | Loading / empty | ✅ loading→Suspense, empty | ✅ empty + await UI | ✅ |
| B5 | Head / title | ✅ document.title effect | ✅ `<svelte:head>` | ✅ |
| B6 | Assets `public/` | ✅ copy tree | ✅ copy tree | ✅ |
| B7 | Fetch idiomático | ✅ `fetch x = "url"` | ✅ `$effect`+fetch | ✅ |
| B8 | Hot reload | ✅ `--watch` | ✅ `--watch` | ✅ |

**Definition of Done B:** ✅ goldens **10/10**; integration review **PASS**.

```bash
cordlang run react --watch
cordlang run svelte --watch --check
# route /shop/:id => ProductPage layout=shop lazy
# fetch products = "/api/products.json"
# title "Shop"
```

Ejemplos: `examples/phase_b_lazy_head.cord`, `examples/fetch_form.cord`, `tests/fixtures/nested_routes.cord`.

---

### Fase C — IR canónico + DX ✅ DONE (agents 2026-07-23)

**Objetivo:** IR compartido + tooling DX (check / fmt / symbols / preview state).

| # | Item | Estado | Notas |
|---|------|--------|-------|
| C1 | **IR canónico** post-AST | ✅ | `domain/ir.{c,h}` → `ir_from_ast` + `ir_dump`; `compile --ir` |
| C2 | Expr language | ✅ | `domain/expr.{c,h}` normalize/validate; usado al bajar expr al IR |
| C3 | Type checker liviano | ✅ | `check_service`: unknown components, routes, dups, use vacío |
| C4 | Diagnostics con span | ✅ | `domain/diag` → `file:line:col: error\|warning: …` |
| C5 | Source maps | ✅ | VLQ mappings from `cordlang: source=` markers (`--sourcemap`) |
| C6 | Formatter `cordlang fmt` | ✅ | in-place + `--check`; whitespace/tabs/blanks |
| C7 | Symbols / goto | ✅ | CLI `symbols` + `goto <Name>` (LSP full = post-C) |
| C8 | Preview con state | ✅ | HTML runtime: `var count` + `setCount` + `data-bind` + `clUpdate` |

**Definition of Done C (ajustado MVP):** ✅  
- IR + dump + expr module livables  
- DX: check, fmt, symbols/goto, preview state, source attribution  
- Suite: **16/16** (`tests\run_tests.ps1`)  
- **Post-C IR-1 + IR-2 ✅:** services → IR; React (`react_ir.c`) y Svelte emiten body **solo desde `IrNode`**.  
Residual: theme.css scaffold puede usar origin; HTML preview no es IR-puro.

```bash
cordlang compile file.cord --ir
cordlang check
cordlang fmt src/ && cordlang fmt --check src/
cd my-app && cordlang symbols && cordlang goto Counter
cordlang compile file.cord --backend react --sourcemap -o App.jsx
cordlang run   # preview con state reactivo (setX / #\{x\})
```

Docs: [`docs/IR.md`](./IR.md).

---

### Fase D — React avanzado / meta (paralelo o post-C)

| # | Item | Prioridad | Notas |
|---|------|-----------|-------|
| D1 | `useInsertionEffect` / `useEffectEvent` | ✅ | `insertionEffect` / `effectEvent` |
| D2 | `useSyncExternalStore` | ✅ | `externalStore` / `syncStore` |
| D3 | `useImperativeHandle` + forwardRef | ✅ | `def X forwardRef` + `imperativeHandle` |
| D4 | React 19 Actions polish | ✅ | pending alias, aria-busy, btn disabled, stub |
| D5 | Server Components / Next adapter | P3 | docs only (out of Phase D code) |
| D6 | Remix / TanStack Router option | P3 | docs only |
| D7 | React Native experimental | P3 | docs only |
| D8 | Testing Library recipes | ✅ | docs + `examples/phase_d_hooks.cord` |

---

### Fase E — Svelte avanzado / meta 🟡 (docs-first + codegen, 2026-07-23)

Mapa completo: [`docs/SVELTE.md`](./SVELTE.md) (basado en [svelte.dev/docs/svelte](https://svelte.dev/docs/svelte/overview)).

| # | Item | Estado | Notas |
|---|------|--------|-------|
| E1 | Context completo + context modules | ✅ | `setContext`/`getContext` + `contexts.js` |
| E2 | Svelte actions (`use:`) | ✅ | attr `use=tooltip` → `use:tooltip` (no confundir con `form action=`) |
| E3 | Transitions / animations | ✅ | `transition=` / `in=` / `out=` / `animate=` + imports |
| E4 | `{#await}` async UI | ✅ | `await expr then=v` + `loading` / `error` |
| E5 | Snippets reutilizables | ✅ | `snippet name(args)` + `render name(...)` |
| E6 | **SvelteKit** backend | ✅ MVP | `backends/sveltekit/` + `docs/SVELTEKIT.md` |
| E7 | Stores (`writable`) | ✅ | `store count = 0` → `writable` + `{$count}` |
| E8 | Attach/portal DOM | ✅ | `portal to=document.body` → `use:portal` helper |

**Demo:** `examples/phase_e_svelte.cord`  
**DoD E (MVP):** ✅ surface principal de [Svelte docs](https://svelte.dev/docs/svelte/overview) mapeada como React; Kit queda para post-E.

---

### Fase F — Ecosistema Cordlang (largo plazo)

| # | Item | Notas |
|---|------|-------|
| F1 | Backend **Vue 3** | ✅ `backends/vue/` + goldens + `docs/VUE.md` |
| F2 | Backend **Solid** | ✅ `backends/solid/` + goldens + `docs/SOLID.md` |
| F3 | Package registry de componentes `.cord` | |
| F4 | AI prompts / skill “write cord not jsx” | ✅ `docs/AI.md` + `AGENTS.md` + `skills/write-cord` |
| F5 | Playground web (WASM compile) | |
| F6 | CI multi-backend | ✅ goldens Win/Linux; demo `--check` = G4 |
| F7 | Versionado del lenguaje (0.x → 1.0 freeze) | |
| F8 | Backend **email** HTML estático | ✅ `backends/email/` + `static_html` + `docs/EMAIL.md` |
| F9 | Backend **pdf** (HTML + conversión externa) | ✅ `backends/pdf/` + `docs/PDF.md` |
| F10 | Meta **Next** (wrap React) | ✅ `backends/next/` + `docs/NEXT.md` + smoke |
| F11 | Meta **SvelteKit** (wrap Svelte) | ✅ `backends/sveltekit/` + `docs/SVELTEKIT.md` + smoke |

---

## 4. Priorización — Horizonte A (12 meses) / B (visión)

### Principios de ejecución

1. **IA en el workflow, no en el AST de build** — nada de `@ai` / `@generate` en el path crítico de compilación.
2. **Primero DX determinista** — errores claros > magia.
3. **Paridad React/Svelte antes de 10 backends.**
4. **Stdlib/templates Cord-nativos** antes de marketplace grande.
5. **Semántica / AI-score LLM** solo cuando el núcleo sea aburridamente sólido.

```
Hecho ──► Fases A–E MVP + IR-1/IR-2 + Fase G (G0–G6) + Fase H (madurez)
AHORA ──► Horizonte A residual (paridad docs, LSP polish) / Horizon B
DESPUÉS ► Horizonte B (semántica, marketplace, Vue/Solid, Kit/Next, …)
```

### 4.1 Horizonte A — épicas ejecutables

| # | Épica | P | Entregables | Criterio |
|---|-------|---|-------------|----------|
| **A1** | DX `.cord` | P0 | Diagnósticos en `check` (attrs, traps JSX); LSP mínimo / snippets; preview `run --watch` documentado; skill al día | Agente edita app con `check` verde sin inventar keywords |
| **A2** | Contratos anti-alucinación | P0 | `props name: string` + validación; [`schema/attrs.json`](./schema/attrs.json) para LSP/IA | Schema machine-readable + check tipado |
| **A3** | IR + paridad React/Svelte | P0–P1 | Checklist en SVELTE.md; goldens; IR.md como contrato de backends | Misma app compila React y Svelte con UI equivalente en features soportadas |
| **A4** | Tooling IA (CLI/skill) | P1 | `cordlang ai` + skill propose→`check`; fixtures IA-fail→fix | Sin LLM en `compile` |
| **A5** | Templates Cord | P1–P2 | 3–5 plantillas en `templates/`; docs; `cord add` más adelante | Repo + TEMPLATES.md |
| **A6** | Analyze determinista | P2 | `cordlang analyze` — reglas score **sin LLM** | Heurísticas a11y/estructura; LLM solo sugerencias opcionales post-A |

### 4.2 Fase G — hecho (histórico)

| # | Item | Estado |
|---|------|--------|
| G0–G6 | Repo público, theme/HTML IR, CI, my-app `--check`, docs/AI skill, MIT | ✅ |
| G7 | Source maps reales | ✅ VLQ from source= markers |
| G8 | LSP full | parcialmente → `cordlang lsp` (diagnostics/symbols/definition); completion/hover abiertos |

### 4.2b Fase H — Madurez del proyecto ✅ (2026-07-24)

Objetivo: pasar de “interesante” a **serio para contribuidores** (confianza → evidencia → extensibilidad).

| # | Item | Estado | Notas |
|---|------|--------|-------|
| H1.1 | Tests de regresión por bug | ✅ | `tests/regression/` + runners; política en CONTRIBUTING/AGENTS |
| H1.2 | Arquitectura interna | ✅ | [`ARCHITECTURE.md`](./ARCHITECTURE.md) |
| H2.1 | Spec formal v0.x | ✅ | [`SPEC.md`](./SPEC.md) |
| H2.2 | Benchmarks de compile | ✅ | `make bench` / `bench/run_bench.sh`; CI opcional `bench.yml` |
| H2.3 | Comparación Cord vs JSX/Svelte | ✅ | `bench/compare/` + `RESULTS.md` (tamaño / compile, no FPS) |
| H3 | IR passes (plugins v1) | ✅ | `domain/ir_pass` + `--pass` / `passes` en json; `strip-debug` |

**DoD H:** ✅ suite de regresión en CI; docs de arquitectura + SPEC; benches locales; passes opt-in sin romper goldens por defecto.

### 4.3 Horizonte B — visión posterior (no diluir A)

| Tema | Notas |
|------|--------|
| Metadata semántica (`purpose`, `importance`) | Vocabulario versionado + codegen; no attrs libres |
| Marketplace `cord add` | Paquetes 100 % Cord |
| Email HTML / PDF desde IR | Multi-salida barata |
| Vue / Solid | Backends nuevos sobre IR estable |
| SvelteKit / Next RSC | Backends meta aparte, no ensuciar SPA |
| Flutter / SwiftUI / Compose | Muy largo plazo |
| `@ai` en fuente | Evitar en build; si existe → preprocesador offline que escribe `.cord` normal |

### 4.4 No haremos

- Router / store / bundler / Vite / Next / servidor propios.
- Presentar Cord como “JSX más corto”.
- Priorizar 10 platforms antes de DX + contratos + paridad React/Svelte.
- Convertir templates React de internet a Cord como estrategia de UI/docs.

---

## 5. Roadmap por backend (vista rápida)

### React

1. ~~IR compartido~~ ✅ IR-2 (`react_ir.c`)  
2. Nested routes más profundos + loaders  
3. ~~Adapter **Next** (backend `next`, no ensuciar SPA)~~ ✅ MVP client wrap  
4. Más goldens Phase D / RSC profundo (fuera de MVP)

### Svelte

1. ~~IR compartido~~ ✅ IR-2  
2. `$bindable` / `{#key}` / special elements  
3. ~~Backend **SvelteKit**~~ ✅ MVP (`sveltekit` meta)  
4. Attachments `@attach` (Svelte 5 modern path)  

### Email / PDF

1. ~~HTML estático compartido~~ ✅ `static_html` + backends `email` / `pdf`  
2. Conversión PDF externa documentada (`run pdf --check` soft)

### HTML preview

1. State básico ✅ (C8)  
2. **IR-puro** (G2)  
3. Paridad limitada con events/bind  

---

## 6. Criterios de “listo para 1.0”

- [x] IR canónico usado por backends React/Svelte (emit body)  
- [x] Diagnostics útiles (`cordlang check`, spans)  
- [x] Preview nativo con state básico  
- [x] CI build + goldens Win/Linux (G3)  
- [x] Un solo proyecto demo pasa React + Svelte build en **CI** (G4)  
- [x] Paridad documentada ≥ 90% de la matriz P0/P1  
- [x] `docs/REACT.md` + `docs/SVELTE.md` al día con codegen (gaps residuales explícitos)  
- [x] Syntax freeze del subset 1.0 ([`docs/SPEC.md`](./SPEC.md) **1.0**)  
- [x] Theme + HTML sin residual AST en path IR (G1–G2)  
- [x] LICENSE publicada (MIT)  

---

## 7. No-objetivos (explícitos)

- No clonar el 100% de cada API de framework (libs internas, experimental flags).  
- No mantener class components React salvo ErrorBoundary.  
- No hacer Cordlang un lenguaje JS general-purpose **ni otro React/Vite**.  
- No meter LLM en el path de `compile` / CI.  
- No bloquear Svelte hasta “React perfecto”: **paridad por fases**, no por monolitismo.  
- No commitear `node_modules/`, `dist/` generados ni binarios locales.

---

## 8. Tracking sugerido (issues / milestones)

| Milestone | Contenido | Estado |
|-----------|-----------|--------|
| **M0 — Solid core** | A1–A6 | ✅ |
| **M1 — Real apps** | B1–B8 | ✅ |
| **M2 — Platform DX** | C1–C8 | ✅ |
| **M3 — Deep React** | D* selecto | 🟡 MVP |
| **M4 — Deep Svelte** | E* selecto | 🟡 MVP (sin Kit) |
| **M5 — IR platform** | IR-1 + IR-2 | ✅ |
| **M6 — Public + polish** | Fase G (G0–G6 ✅) | ✅ |
| **M7 — Horizonte A** | A1–A6 (DX, contratos, paridad, IA workflow, templates, analyze) | ✅ gate 1.0 |
| **M8 — Madurez** | Fase H (regresión, ARCHITECTURE, SPEC, bench, IR passes) | ✅ |
| **M9 — Horizonte B / meta** | Platform → Vue → Solid → email/PDF → Next/Kit → ecosystem → native | en curso |

Labels útiles: `lang:core`, `backend:react`, `backend:svelte`, `ir`, `dx`, `ci`, `docs`.

---

## 9. Cómo usar este roadmap

1. **Antes de codear un feature:** ¿vive en el AST/IR compartido o es solo emit de un backend?  
2. **Si es compartido** → parser + AST + ambos backends en el mismo PR si es P0.  
3. **Si es idiomático de un solo framework** → solo ese backend + nota en la matriz.  
4. **Actualizar** `REACT.md` / `SVELTE.md` / esta tabla en el mismo cambio.

---

*Última actualización: Fase H (madurez) ✅; prioridades = Horizonte A residual / B.*
