# Playground (WASM) — stub → next adoption epic

El playground **en el navegador** (compilar `.cord` vía WASM) es el siguiente cierre de ecosistema tras Vue Official (ROADMAP F5 / “cerrar a medias”). **Aplazado** respecto al loop IA (check / LSP / preview) y a no sumar más backends. Este directorio es el **stub estático**: documenta el camino y deja una página HTML mínima sin binario WASM.

## Estado

| Pieza | Estado |
|-------|--------|
| Compilar Cordlang a WASM | No empaquetado aún (C → Emscripten / similar) |
| UI playground | Stub HTML en [`../playground/index.html`](../playground/index.html) |
| Workflow local equivalente | `cordlang compile file.cord --ir` (+ backends) |

## Camino WASM (plan)

1. Compilar el núcleo (lexer → parser → AST → IR → emit) a WASM con Emscripten o similar.
2. Exponer una API JS mínima: `compile(source, { backend, ir }) → string | diagnostics`.
3. Sustituir el stub HTML por un editor + panel IR/codegen (tipo Rust playground / Svelte REPL).

Hasta entonces, el flujo recomendado es el CLI:

```bash
cordlang compile examples/counter.cord --ir
cordlang compile examples/counter.cord --backend react
cordlang check examples/counter.cord
```

Abrir [`../playground/index.html`](../playground/index.html) en el navegador para ver el sample embebido y las instrucciones.
