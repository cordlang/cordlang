# Playground (WASM) — MVP stub

El playground **en el navegador** (compilar `.cord` vía WASM) es un objetivo de ecosistema (ROADMAP F5). Este directorio es el **stub estático**: documenta el camino y deja una página HTML mínima sin binario WASM.

## Estado

| Pieza | Estado |
|-------|--------|
| Compilar Cordlang a WASM | No empaquetado aún (C → emscripten / similar) |
| UI playground | Stub HTML en [`../playground/index.html`](../playground/index.html) |
| Workflow local equivalente | `cordlang compile file.cord --ir` (+ backends) |

## Camino WASM (plan)

1. Compilar el núcleo (lexer → parser → AST → IR → emit) a WASM con Emscripten o similar.
2. Exponer una API JS mínima: `compile(source, { backend, ir }) → string | diagnostics`.
3. Sustituir el stub HTML por un editor + panel IR/codegen.

Hasta entonces, el flujo recomendado es el CLI:

```bash
cordlang compile examples/counter.cord --ir
cordlang compile examples/counter.cord --backend react
cordlang check examples/counter.cord
```

Abrir [`../playground/index.html`](../playground/index.html) en el navegador para ver el sample embebido y las instrucciones.
