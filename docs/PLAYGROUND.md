# Playground (WASM)

Compile `.cord` **in the browser** — no CLI install. It uses the same IR path
as `cordlang compile`. The editor is a **multi-file virtual project** (M11.2):
tree + tabs call `cordlang_compile_project` so `use` / `route` / layouts
resolve the same way as the native smoke.

This is a **language** playground (Cordlang compiler in WASM), not the **Runix**
framework product — see [`RUNIX.md`](./RUNIX.md).

## Status

| Pieza | Estado |
|-------|--------|
| Núcleo C → WASM | ✅ `playground/build_wasm.ps1` / `.sh` (Docker `emscripten/emsdk` o `emcc` local) |
| API JS (single file) | ✅ `cordlang_compile(source, backend)` → JSON |
| API JS (project) | ✅ `cordlang_compile_project(entry, files_json, backend)` exported in the tracked bundle |
| UI | ✅ tree + tabs + add/reset in [`../playground/index.html`](../playground/index.html) |
| Multi-file `use` / routes | ✅ sample project (`app.cord` + layout + `HomePage` + `Counter`) |
| Native project smoke | ✅ Windows + Ubuntu CI; also pins the UI contract (`compile_project` + entry path) |
| Bundles versionados en el repositorio | ✅ `playground/cordlang.js` + `playground/cordlang.wasm` están trackeados |
| Rebuild / verificación WASM en CI | ✅ `playground/smoke_native.*` en CI + workflow `.github/workflows/playground.yml` (rebuild Docker + drift check) |
| Artefacto o release publicado | ✅ `cordlang-playground.zip` en CI artifacts y GitHub Releases |

## Build WASM

```bash
# Docker (recommended if emcc not installed)
powershell -ExecutionPolicy Bypass -File playground/build_wasm.ps1
# or
bash playground/build_wasm.sh
# or
make wasm
```

Outputs: `playground/cordlang.js` + `playground/cordlang.wasm`.

## Serve (required)

Browsers block ES-module WASM from `file://`:

```bash
npx --yes serve playground
# open the printed URL
```

The default sample is a virtual tree under `/playground/`:

- `src/app.cord` — entry (`use` + `route / => pages/HomePage`)
- `src/layouts/default.cord`
- `src/pages/HomePage.cord`
- `src/components/Counter.cord`
- `cordlang.json`

Edit any file; Compile (or the debounce) sends the whole map through
`cordlang_compile_project`. Diagnostics include `file` — click one to jump.

## JS API

```js
const Module = await createCordlang();
const compile = Module.cwrap("cordlang_compile", "number", ["string", "string"]);
const free = Module.cwrap("cordlang_free", null, ["number"]);
const ptr = compile(source, "react"); // react|svelte|vue|html|esm|email|ir
const result = JSON.parse(Module.UTF8ToString(ptr));
free(ptr);
// result: { ok, backend, code, diagnostics:[{level,message,file?,line,col,code?,hint?}] }
```

## Project API (M11.1)

Pass an absolute virtual entry path plus a JSON file map. All resolver reads
are contained in that map: missing files never fall back to the host
filesystem.

```js
const files = {
  "/playground/cordlang.json": '{"entry":"src/app.cord"}',
  "/playground/src/app.cord": "route / => pages/HomePage\n",
  "/playground/src/pages/HomePage.cord": 'h1 "Hello"\n'
};
const compileProject = Module.cwrap("cordlang_compile_project", "number", [
  "string", "string", "string"
]);
const ptr = compileProject(
  "/playground/src/app.cord",
  JSON.stringify(files),
  "react"
);
const result = JSON.parse(Module.UTF8ToString(ptr));
free(ptr);
```

`entry` and every file-map key must be absolute virtual paths. The manifest
accepts up to 128 files; module resolution cannot escape the project root.

## CLI equivalent

```bash
cordlang compile examples/counter.cord --ir
cordlang compile examples/counter.cord --backend react
cordlang check examples/counter.cord
```

Roadmap: F5 / M11 — [`ROADMAP.md`](./ROADMAP.md).
