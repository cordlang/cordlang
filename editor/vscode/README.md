# VS Code / Cursor — Cordlang

Soporte de lenguaje para `.cord`: resaltado, snippets y **errores en el
editor** (subrayados + Problems).

Ver [`docs/LSP.md`](../../docs/LSP.md).

## Qué incluye

| Pieza | Rol |
|-------|-----|
| Language id `cordlang` + `.cord` | Detecta archivos |
| `languages.icon` (`logo.png`) | Icono fallback (estilo Vue) |
| TextMate grammar | Color de sintaxis |
| Snippets | Plantillas rápidas |
| **Language Client → `cordlang lsp`** | Diagnósticos en vivo (buffer) + code actions |
| **Fallback `check --json`** | Si el LSP no arranca, igual hay squiggles |
| Task `cordlang: check` | Problem matcher |

## Errores en el editor

1. Abre un `.cord`.
2. Escribe algo inválido (`className=`, `"string sin cerrar`, etc.).
3. Deberías ver subrayado rojo/amarillo y entradas en **Problems**.

La extensión busca el binario en este orden:

1. Setting `cordlang.lsp.path`
2. `cordlang.exe` / `cordlang` / `cordlang_r5.exe` en la raíz del workspace
3. `cordlang` en el `PATH`

Barra de estado: `Cordlang LSP` (ok) o `Cordlang check` (fallback).

## Desarrollo

```bash
cd editor/vscode
npm install
npm run check
npx @vscode/vsce package   # → cordlang-*.vsix
```

Instalar: **Install from VSIX…** → `cordlang-1.0.3.vsix` (desinstala la
versión anterior antes). En Cursor: misma ruta, o *Developer: Install Extension
from Location…* apuntando a `editor/vscode` en modo desarrollo.
