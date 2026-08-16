# Cordlang for VS Code / Cursor

Editor support for [Cordlang](https://github.com/cordlang/cordlang) — the dense UI language that compiles to React, Svelte, or a native ESM preview.

## What you get

Open a `.cord` file and the extension gives you:

- **Syntax highlighting** for Cordlang keywords, tags, `#{…}`, events, and attrs
- **Snippets** for common patterns (`state`, `props`, `if`/`for`, routes, components)
- **Live diagnostics** — red/yellow squiggles and Problems panel entries as you type (via `cordlang lsp`)
- **Quick fixes** for common mistakes (e.g. `className` → `class`, `{x}` → `#{x}`)
- **Completion, hover, go-to-definition, find-all-references, rename, symbols, and format-on-save** through the language server
- **Task** `cordlang: check` to validate the whole project from the command palette

Requires the `cordlang` CLI on your `PATH` (or set `cordlang.lsp.path` to the binary). If the language server cannot start, the extension falls back to `cordlang check --json` so you still get squiggles.

## Settings

| Setting | Default | Purpose |
|---------|---------|---------|
| `cordlang.lsp.path` | `cordlang` | Path to the CLI binary |
| `cordlang.diagnostics.fallback` | `true` | Use `check --json` when LSP fails |

The extension also auto-detects `cordlang.exe` in the workspace root.

Status bar: **Cordlang LSP** when the server is up, **Cordlang check** when using the fallback.
