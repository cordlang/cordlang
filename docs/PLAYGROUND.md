# Playground (WASM)

Compile `.cord` **in the browser** — no CLI install. Same IR path as
`cordlang compile` (single-buffer; no multi-file `use` resolution).

## Status

| Pieza | Estado |
|-------|--------|
| Núcleo C → WASM | ✅ `playground/build_wasm.ps1` / `.sh` (Docker `emscripten/emsdk` o `emcc` local) |
| API JS | ✅ `cordlang_compile(source, backend)` → JSON |
| UI | ✅ editor + panel codegen en [`../playground/index.html`](../playground/index.html) |
| Multi-file `use` / routes | ❌ single buffer only (CLI / `cordlang run` for apps) |

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
// result: { ok, backend, code, diagnostics:[{level,message,line,col,code?}] }
```

## CLI equivalent

```bash
cordlang compile examples/counter.cord --ir
cordlang compile examples/counter.cord --backend react
cordlang check examples/counter.cord
```

Roadmap: F5 / M11 — [`ROADMAP.md`](./ROADMAP.md).
