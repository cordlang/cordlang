# Versionado Cordlang

## Dos versiones distintas

| Superficie | Qué significa | Hoy |
|------------|---------------|-----|
| **Lenguaje** | Sintaxis + semántica del subset estable (SPEC) | **1.0** documentado en [`SPEC.md`](./SPEC.md) |
| **CLI / tooling** | Binario `cordlang`, LSP `serverInfo`, flags | Semver del CLI — `cordlang --version` → **1.0.0** (`src/domain/version.h`) |

El freeze del **lenguaje 1.0** no implica que el CLI deje de evolucionar (nuevos backends, flags DX, LSP). Al revés: backends y tooling pueden subir minor/patch sin romper el subset 1.0.

## Política de breaking changes

### Lenguaje (SPEC 1.0)

- **Breaking** (requiere bump mayor del lenguaje, p. ej. 2.0): eliminar o reinterpretar sintaxis del subset 1.0; cambiar semántica de tags/attrs documentados; invalidar programas que hoy pasan `cordlang check`.
- **No breaking:** attrs opcionales nuevos (`purpose`, `importance`, …); keywords opt-in; backends nuevos; passes IR opt-in; mensajes de diagnóstico más estrictos como *warnings*.

### CLI (semver)

- **MAJOR:** flags/comandos existentes cambian significado o se eliminan; salida estable de goldens de API pública del CLI se rompe a propósito.
- **MINOR:** comandos/backends/docs nuevos (`add`, `init --template`, …).
- **PATCH:** fixes, mensajes, docs.

## Compatibilidad práctica

1. Programas del subset 1.0 deben seguir compilando en releases 1.x del CLI.
2. Goldens de codegen pueden actualizarse en el mismo PR que el cambio de emit (no son API del lenguaje).
3. Experimental (native Flutter/SwiftUI/Compose, playground WASM) **no** entra en el contrato 1.0 — ver [`NATIVE.md`](./NATIVE.md), [`PLAYGROUND.md`](./PLAYGROUND.md).

## Comandos

```bash
cordlang --version   # o: cordlang -V | cordlang version
cordlang lsp         # serverInfo.version = misma constante
```
