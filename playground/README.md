# Cordlang Playground (stub)

Static MVP — **no WASM binary** yet. Use the Cordlang CLI locally and open `index.html` for a sample + workflow notes.

See also: [`docs/PLAYGROUND.md`](../docs/PLAYGROUND.md).

## Local compile workflow

```bash
# from repo root
make
./cordlang compile examples/counter.cord --ir
./cordlang compile examples/counter.cord --backend react
./cordlang check examples/counter.cord
```

## Future WASM path

1. Compile lexer/parser/IR/codegen to WASM (Emscripten).
2. JS API: `compile(source, opts) → { code, ir, diagnostics }`.
3. Replace this stub with an in-browser editor.

Open `index.html` in a browser (file:// is fine for the stub).
