# VS Code / Cursor — Cordlang

Extensión liviana: language id, snippets, problem matcher y Language Client →
`cordlang lsp`.

Ver [`docs/LSP.md`](../../docs/LSP.md).

## Desarrollo

```bash
cd editor/vscode
npm install
npm run check          # syntax + load module
npx @vscode/vsce package   # → cordlang-*.vsix
```

Instalar en el editor:

- **Developer: Install Extension from Location…** → esta carpeta, o
- **Install from VSIX…** → el `.vsix` generado.

CI: `.github/workflows/vscode-extension.yml` (npm ci, check, package + artifact).
