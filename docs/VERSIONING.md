# Versionado Cordlang

## Dos versiones distintas

| Superficie | Qué significa | Hoy |
|------------|---------------|-----|
| **Lenguaje** | Sintaxis + semántica del subset estable (SPEC) | **1.0** documentado en [`SPEC.md`](./SPEC.md) |
| **CLI / tooling** | Binario `cordlang`, LSP `serverInfo`, flags | Semver del CLI — hoy **0.0.x (alpha)** en [`src/domain/version.h`](../src/domain/version.h) |

El freeze del **lenguaje 1.0** no implica que el CLI deje de evolucionar (nuevos backends, flags DX, LSP). Al revés: backends y tooling pueden subir minor/patch sin romper el subset 1.0.

## Canal alpha / beta (releases)

Mientras el CLI esté en **0.0.x**, los GitHub Releases se publican como **prerelease**:

| Tag ejemplo | Canal | GitHub |
|-------------|-------|--------|
| `v0.0.013-alpha.1` | alpha | prerelease |
| `v0.0.014-beta.1` | beta | prerelease |
| `v0.1.0` | primer “estable” de tooling | release normal (sin alpha/beta/rc en el tag) |

Workflow: [`.github/workflows/release.yml`](../.github/workflows/release.yml).

```bash
# Opción A — tag push
git tag -a v0.0.013-alpha.1 -m "alpha.1"
git push origin v0.0.013-alpha.1

# Opción B — Actions → Release → Run workflow
#   version: 0.0.013-alpha.1
#   prerelease: true
```

Artefactos por release: Windows x64, Linux x64 (Ubuntu), Fedora x64, Linux arm64, macOS arm64, macOS x64 + `SHA256SUMS.txt`.

El binario embebe la versión del tag (`make VERSION=…` / script Windows).

## Política de breaking changes

### Lenguaje (SPEC 1.0)

- **Breaking** (requiere bump mayor del lenguaje, p. ej. 2.0): eliminar o reinterpretar sintaxis del subset 1.0; cambiar semántica de tags/attrs documentados; invalidar programas que hoy pasan `cordlang check`.
- **No breaking:** attrs opcionales nuevos (`purpose`, `importance`, …); keywords opt-in; backends nuevos; passes IR opt-in; mensajes de diagnóstico más estrictos como *warnings*.

### CLI (semver)

- **MAJOR:** flags/comandos existentes cambian significado o se eliminan; salida estable de goldens de API pública del CLI se rompe a propósito.
- **MINOR:** comandos/backends/docs nuevos (`add`, `init --template`, …).
- **PATCH:** fixes, mensajes, docs.
- **0.0.x:** pre-1.0 tooling — se espera churn; preferir tags `-alpha.N` / `-beta.N`.

## Compatibilidad práctica

1. Programas del subset 1.0 deben seguir compilando en releases 1.x del CLI.
2. Goldens de codegen pueden actualizarse en el mismo PR que el cambio de emit (no son API del lenguaje).
3. Experimental (native Flutter/SwiftUI/Compose) **no** entra en el contrato 1.0 — ver [`NATIVE.md`](./NATIVE.md). Playground WASM MVP: [`PLAYGROUND.md`](./PLAYGROUND.md).

## Comandos

```bash
cordlang --version   # o: cordlang -V | cordlang version
cordlang lsp         # serverInfo.version = misma constante
```
