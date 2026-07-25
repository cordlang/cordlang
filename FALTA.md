# FALTA.md — preview con módulos ES nativos

Estado: **funcional, documentado y verificado** en Windows
(`tests/run_tests.ps1` → suite green; foreign stub + my-app loop cubiertos).
Queda portabilidad Linux/macOS y las decisiones de alcance en §4.

---

## Hecho (no volver a tocar salvo que algo de abajo lo exija)

- [x] API de resolución de módulos → `src/application/ports/compiler_port.h`, `src/adapters/outbound/compiler/compiler.c`
- [x] Mapeo attr→clase compartido → `src/adapters/outbound/backends/cord_class.{c,h}` (react/svelte/vue/solid **byte-idénticos**, 104/104)
- [x] Runtime JS → `src/adapters/outbound/backends/esm/esm_runtime.c` (24/24 asserts bajo shim de DOM)
- [x] Emisor ESM (un módulo por `.cord`) → `src/adapters/outbound/backends/esm/esm_ir.c` (grafo de 20 módulos resuelve entero)
- [x] `base.css` JIT → `src/adapters/outbound/backends/esm/esm_css.c` (48/48 clases cubiertas)
- [x] Dev server + SSE reload → `src/adapters/outbound/runtime/dev_server.{c,h}`, `src/application/preview_service.{c,h}`
- [x] CLI (`run`, `run html`, `--no-open`) → `src/adapters/inbound/cli.c`, `registry.c`, `build.bat`, `Makefile`, `CMakeLists.txt`
- [x] 3 regresiones → `tests/regression/esm-attr-not-identifier/`, `tests/regression/esm-routes-layouts/`, `tests/regression/esm-foreign-stub/`
- [x] Harness de tests arreglado → `tests/run_tests.ps1`
- [x] `IR_FOREIGN` → stub `cord-runtime-error` (sin import npm) en `esm_ir.c`
- [x] Documentación principal (`docs/PREVIEW.md`, AGENTS, README, ARCHITECTURE, CHANGELOG, AI_CONTEXT)
- [x] Límites deliberados documentados en `docs/PREVIEW.md` (§3)

---

## 1. Documentación

- [x] **`docs/PREVIEW.md`** (nuevo) — el documento principal
  - [x] Por qué ningún navegador parsea `.cord` (ni `.vue` ni `.svelte`): lo que hace posible el modelo es el dev server respondiendo `Content-Type: text/javascript`
  - [x] Mapa de URLs: `/`, `*.cord`, `/@cord/runtime.js`, `/@cord/base.css`, `/@cord/theme.css`, `/@cord/hmr.js`, `/@cord/hmr`, estáticos, fallback SPA
  - [x] Forma del módulo emitido (componente y entry) con ejemplo real
  - [x] API del runtime: `h`, `frag`, `txt`, `keyed`, `component`, `mount`, `navigate`, y el objeto `$` (`state`, `effect`, `ref`, `resource`, `params`, `query`, `path`)
  - [x] Cómo se resuelve el layout de una ruta (explícito → `default` → primero declarado)
  - [x] Sección de límites (ver punto 3)
  - [x] `esm` vs `react`: cuándo usar cada uno
- [x] **`AGENTS.md`** — `run` = dev server ESM, `run html` = preview legacy; añadir `esm` a la tabla de backends
- [x] **`README.md`** — misma línea en Quick start; default `run` = ESM native preview
- [x] **`docs/ARCHITECTURE.md`** — añadir al mapa: `backends/esm/`, `runtime/dev_server.c`, `backends/cord_class.c`
- [x] **`CHANGELOG.md`** — entrada Unreleased (ESM + bugs + `IR_FOREIGN` stub / `esm-foreign-stub`)
- [x] **`docs/AI_CONTEXT.md`** — mencionar `cordlang run` como el loop de preview (es el doc que gana para intención de producto)

## 2. Verificaciones pendientes

- [x] **Componentes `foreign` (`IR_FOREIGN`)** — stub visible con clase `cord-runtime-error`, sin import npm.
  - Archivos: `src/adapters/outbound/backends/esm/esm_ir.c` (rama `IR_FOREIGN` / `collect_foreigns`)
  - Fixture: `tests/regression/esm-foreign-stub/`
- [x] **`my-app/`** (demo del repo) end-to-end con `cordlang run`
  - Sin cambios de código esperados; si falla, apunta a `esm_ir.c`
- [ ] **Compilación en Linux/macOS** — no puedo probarlo aquí. Ya arreglé el `clock()` → `now_ms()` en `dev_server.c`; el resto usa `dirent`/`select` con guardas `#ifdef`.

## 3. Límites deliberados (documentar, no arreglar)

- [x] `suspense` / `loading` / `errorBoundary` / `portal` → renderizan su contenido inline (el runtime no tiene frontera async)
- [x] `icon` / `motion` / `chart` → degradan a `span`/`div` con clase `cord-<tag>`
- [x] La recarga es de página completa, no HMR con preservación de estado
- [x] Clases de `class=` en el fuente no generan CSS: las resuelve tu `public/site.css`
- [x] `foreign` / `IR_FOREIGN` → stub `cord-runtime-error` (documentado + pin `esm-foreign-stub`)

## 4. Decisión tuya — fuera del alcance actual

- [ ] **`cordlang build esm`**: escribir los mismos módulos como archivos estáticos para desplegar.
      Hoy el ESM solo existe dentro del dev server. Si "para prod" significaba **desplegar** así y no solo previsualizar, esto falta.
      - Archivos: `src/adapters/outbound/backends/esm/esm_scaffold.c` (nuevo), `esm_backend.h` (`scaffold_from_ir`), `cli.c` (`cmd_build`), los 3 build files
      - Aviso técnico: ESM sin bundle en producción = una petición HTTP por componente (waterfall). Correcto para intranet/HTTP2, malo para una landing pública.
- [ ] **Bug preexistente en React**: `purpose=action` emite `purpose={action}` → `ReferenceError`. `templates/landing` crashea también en React.
      - Archivo: `src/adapters/outbound/backends/react/react_ir.c` (`emit_jsx_value`)
      - Arreglarlo requiere el mismo tracking de scope que hice en ESM y **cambia goldens** de react/svelte/vue/solid → decisión tuya
      - Anotado en `tests/regression/esm-attr-not-identifier/README.md`

---

## Comandos para retomar

```powershell
build.bat
.\tests\run_tests.ps1                 # suite green
cd <proyecto>; cordlang run --no-open # dev server en 127.0.0.1:4173
```

Los scripts de verificación que escribí viven en el scratchpad de la sesión
(`crawl.mjs`, `render-test.mjs`, `sweep-test.mjs`, `css-coverage.mjs`,
`hmr-test.mjs`, `dom-shim.mjs`). Si los quieres permanentes → `tests/esm/`.
