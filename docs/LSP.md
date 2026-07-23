# Cordlang editor / LSP mínimo (A1)

Full Language Server Protocol is still **G8 / ongoing**. Today agents and editors get DX from the **CLI** plus a thin VS Code/Cursor package.

## CLI surfaces (LSP building blocks)

| Command | LSP-ish role |
|---------|----------------|
| `cordlang check [path]` | diagnostics (errors/warnings with `file:line:col`) |
| `cordlang analyze [path]` | extra heuristics / score |
| `cordlang symbols [entry]` | document symbols (components, routes, layouts) |
| `cordlang goto <Name>` | go-to-definition |
| `cordlang fmt` | format |

Problem matcher example (check output):

```text
path/to/file.cord:12:3: error: unknown component 'Foo'
path/to/file.cord:8:5: warning: unknown attribute 'fooBar' on tag 'col'
```

## Package: `editor/vscode/`

Contents:

- Language id `cordlang` for `*.cord`
- Snippets (def, state/props, route, if/for, fetch/form)
- `language-configuration.json` (comments, brackets, auto-indent hints)
- Tasks that run `cordlang check`

Install (dev): open `editor/vscode` as extension folder, or copy snippets into Cursor.

## Roadmap to full LSP

1. Wrap `check` + `symbols` + `goto` in stdio JSON-RPC.
2. Completion: tags/attrs from [`schema/attrs.json`](./schema/attrs.json).
3. Hover: show prop types + schema docs.
4. Source maps (G7) for stack traces from generated JS.

Until then: **snippets + `check` in watch/tasks** is the supported minimum.
