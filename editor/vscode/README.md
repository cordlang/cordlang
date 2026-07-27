# VS Code / Cursor — Cordlang

Soporte de lenguaje para `.cord`: resaltado de sintaxis, snippets y diagnósticos
LSP (`cordlang lsp`). Como Vue: el icono del lenguaje se declara en la
contribución `languages` y el tema de iconos activo lo usa como fallback —
**sin** reemplazar el File Icon Theme de VS Code/Cursor.

Ver [`docs/LSP.md`](../../docs/LSP.md).

## Qué incluye

| Pieza | Rol |
|-------|-----|
| Language id `cordlang` + `.cord` | Detecta archivos |
| `languages.icon` (`logo.png`) | Icono fallback (estilo Vue) en explorador/pestañas |
| TextMate grammar | Color de sintaxis |
| Snippets | Plantillas rápidas |
| Language Client → `cordlang lsp` | Correcciones / diagnósticos |
| Task `cordlang: check` | Problem matcher |

## Desarrollo

```bash
cd editor/vscode
npm install
npm run check
npx @vscode/vsce package   # → cordlang-*.vsix
```

Instalar: **Install from VSIX…** → `cordlang-1.0.2.vsix` (desinstala la versión anterior antes).
