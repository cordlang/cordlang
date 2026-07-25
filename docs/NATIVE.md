# Native targets (experimental)

> **Experimental.** No hay backend Flutter / SwiftUI / Compose registrado en el CLI.
> El contrato web (React, Svelte, Vue, Solid + email/PDF/meta) es el gate de entrada;
> native es un spike de mapeo IR → UI nativa.

## Criterio de entrada

Antes de invertir en un backend nativo de producción:

1. **≥ 4 backends web en CI** — cumplido (React, Svelte, Vue, Solid; más email/PDF/Next/Kit).
2. IR canónico estable para los kinds listados abajo.
3. Spike documentado (este doc + ejemplos) sin romper `make` / goldens.

## Mapeo IR → Flutter / SwiftUI / Compose (borrador)

| Cord / IR | Flutter | SwiftUI | Compose |
|-----------|---------|---------|---------|
| Texto (`p` / `h*` / `span` / `IR_TEXT`) | `Text` | `Text` | `Text` |
| `col` | `Column` | `VStack` | `Column` |
| `row` | `Row` | `HStack` | `Row` |
| `btn` + `@click` | `ElevatedButton` / `onPressed` | `Button` / action | `Button` / `onClick` |
| `state x=…` + `setX` | `StatefulWidget` / `setState` o Riverpod | `@State` | `mutableStateOf` |
| `props` | constructor params | `let` / init | composable params |
| `if` / `for` | condicionales / `ListView` | `if` / `ForEach` | `if` / `LazyColumn` |
| `route` / layout | Navigator 2 / go_router | `NavigationStack` | Navigation Compose |

**Fuera de scope del spike:** fetch, forms action, portals, suspense, theme tokens CSS, binders DOM.

## Spike en el repo

Sin registrar un backend `flutter` roto:

| Archivo | Qué es |
|---------|--------|
| [`examples/native/counter.cord`](../examples/native/counter.cord) | Counter Cord mínimo (state + btn + text) |
| [`examples/native/counter.dart.md`](../examples/native/counter.dart.md) | Boceto Flutter/Dart a mano que espeja el Counter |

```bash
cordlang compile examples/native/counter.cord --ir
cordlang check examples/native/counter.cord
```

Un backend `flutter` real (emit `main.dart` desde IR) solo se registrará cuando el emit compile como Dart válido y tenga smoke test **fuera** del loop golden principal.

## No-objetivos

- No sustituir el preview HTML nativo del CLI.
- No prometer paridad de apps SPA en mobile.
- No mezclar widgets de plataforma en el AST compartido.
