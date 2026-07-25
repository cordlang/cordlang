# VS Code / Cursor — Cordlang

Extensión liviana: language id, snippets, problem matcher y Language Client →
`cordlang lsp`.

Ver [`docs/LSP.md`](../../docs/LSP.md).

## Desarrollo

```bash
cd editor/vscode
npm install
npm run check          # syntax + static package validation
npx @vscode/vsce package   # → cordlang-*.vsix
```

> Note: do **not** `require('./extension.js')` under plain Node — the `vscode`
> API only exists inside the editor host. CI uses the same `npm run check` path.

Instalar en el editor:

- **Developer: Install Extension from Location…** → esta carpeta, o
- **Install from VSIX…** → el `.vsix` generado.

CI: `.github/workflows/vscode-extension.yml` (npm ci, check, package + artifact).
