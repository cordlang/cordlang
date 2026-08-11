# Playground (WASM)

Compile `.cord` **in the browser** — no CLI install. It uses the same IR path
as `cordlang compile`. The visible editor is still single-buffer. M11.1 adds a
multi-file project API in source; rebuild the WASM bundle before calling it in
the browser.

This is a **language** playground (Cordlang compiler in WASM), not the **Runix**
framework product — see [`RUNIX.md`](./RUNIX.md).

## Status

| Pieza | Estado |
|-------|--------|
| Núcleo C → WASM | ✅ `playground/build_wasm.ps1` / `.sh` (Docker `emscripten/emsdk` o `emcc` local) |
| API JS (single file) | ✅ `cordlang_compile(source, backend)` → JSON |
| API JS (project) | 🟡 source ✅ `cordlang_compile_project(entry, files_json, backend)`; bundle rebuild pendiente |
| UI | 🟡 editor + panel codegen en [`../playground/index.html`](../playground/index.html); aún sin árbol ni tabs |
| Multi-file `use` / routes | 🟡 disponible tras rebuild en la API de proyecto; la UI sigue single-buffer hasta M11.2 |
| Native project smoke | ✅ Windows + Ubuntu CI compilan el mismo source set con `CORDLANG_WASM` |
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

After rebuilding the WASM bundle, pass an absolute virtual entry path plus a
JSON file map. All resolver reads are contained in that map: missing files never
fall back to the host filesystem.

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
