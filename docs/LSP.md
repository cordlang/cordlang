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
| `initialize` | sync + documentSymbol + definition + completion + hover + codeAction + **formatting** |
| `textDocument/didOpen\|didChange\|didSave` | diagnostics from **buffer text** via `check_service_run_source` (no save required); `code` + `data.hint` |
| `textDocument/documentSymbol` | components from project parse |
| `textDocument/definition` | goto component under cursor |
| `textDocument/completion` | **context-aware**: `@` → events; after tag → attrs; line-start → keywords/tags |
| `textDocument/hover` | tag/attr/keyword docs; prop types when known |
| `textDocument/codeAction` | quickfix: `jsx-attr` rename; `bad-interp` `{x}` → `#{x}` |
| `textDocument/formatting` | `fmt` normalize (tabs→spaces, trim, blank collapse) |
| `shutdown` / `exit` | clean shutdown |

## Agent contract

Diagnostics match CLI `cordlang check --json`: each item may include `code` and `data.hint` (same strings as `hint` in JSON check output).

Trap codes: `jsx-attr`, `jsx-hook`, `jsx-tag`, `jsx-map`, `bad-interp`, `semantic-vocab`.

## Manual smoke

```bash
# From repo root after `make`
printf '%s' 'Content-Length: 132

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}' | ./cordlang lsp
# Expect capabilities including completionProvider, hoverProvider, documentFormattingProvider
```

In the editor: open a `.cord`, type `className=` or `{count}` without saving — diagnostics should appear with codes; code actions offer fixes.

## Roadmap leftovers

- Rename / references via LSP
- Dynamic load of `docs/schema/attrs.json` at runtime (today: lists synced in `known_attrs.h` + completion tables)
- Autofix for more trap codes beyond `jsx-attr` / `bad-interp`
