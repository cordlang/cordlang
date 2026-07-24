# Cordlang editor / LSP (A1 + G8)

Editors get DX from the **CLI** plus the VS Code/Cursor package under `editor/vscode/`.

## CLI surfaces (LSP building blocks)

| Command | LSP-ish role |
|---------|----------------|
| `cordlang check [path]` | diagnostics (errors/warnings with `file:line:col`) |
| `cordlang analyze [path]` | extra heuristics / score |
| `cordlang symbols [entry]` | document symbols (components, routes, layouts) |
| `cordlang goto <Name>` | go-to-definition |
| `cordlang fmt` | format |
| `cordlang lsp` | stdio Language Server |

## Package: `editor/vscode/`

- Language id `cordlang` for `*.cord`
- Snippets + `language-configuration.json`
- Tasks: `cordlang check`
- **LanguageClient** (`extension.js`) → `cordlang lsp` via `cordlang.lsp.path`

Install (dev): `cd editor/vscode && npm install`, then open as extension folder.

## LSP methods (`cordlang lsp`)

| Method | Behavior |
|--------|----------|
| `initialize` | sync + documentSymbol + definition + **completion** + **hover** + **codeAction** |
| `textDocument/didOpen\|didChange\|didSave` | `check` → `publishDiagnostics` (includes `code` when set) |
| `textDocument/documentSymbol` | components from project parse |
| `textDocument/definition` | goto component under cursor |
| `textDocument/completion` | tags / attrs / keywords (schema surface) |
| `textDocument/hover` | tag/attr/keyword docs; prop types when known |
| `textDocument/codeAction` | quickfix rename for JSX traps (`className`→`class`, `onClick`→`@click`, …) |
| `shutdown` / `exit` | clean shutdown |

## Roadmap leftovers

- Formatting / rename / references via LSP
- Schema JSON loaded dynamically (today: embedded lists + `known_attrs.h`)
- Autofix for `bad-interp` (`{x}` → `#{x}`) beyond attr renames
