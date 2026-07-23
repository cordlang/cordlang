# Cordlang AI workflow (no LLM in compile)

Cordlang keeps **AI in the edit loop**, never in the AST/`compile` path.

## Happy path (vibecode)

```bash
# 1. Propose / edit .cord (human or agent + skills/write-cord)
# 2. Validate
cordlang check
cordlang analyze          # optional score / structure heuristics
# 3. Preview or build
cordlang run react --watch
# or
cordlang run svelte --check
```

## CLI helper

```bash
cordlang ai               # print this workflow + pointers
cordlang ai check [path]  # same as: cordlang check [path]
```

## Agent loop

1. Read [`AI.md`](./AI.md) + [`schema/attrs.json`](./schema/attrs.json).
2. Patch `.cord` only (not `dist/**`).
3. Run `cordlang check` — fix until zero errors.
4. Optionally `cordlang analyze` for unused comps / missing `to` / layout `slot`.
5. Mention run commands to the user.

## Fixtures (IA-fail → fix)

Under `tests/fixtures/`:

| Fixture | Trap |
|---------|------|
| `ia_fail_classname.cord` | `className=` / JSX attr |
| `ia_fail_interp.cord` | `{count}` style mistake patterns caught as forbidden attrs / check |
| `ia_fail_unknown_attr.cord` | attr not in schema on built-in tag |

Agents should turn these red cases into green by rewriting to Cord idioms.

## Skills

| Path | Role |
|------|------|
| [`skills/write-cord/`](../skills/write-cord/) | Author `.cord` |
| This doc + `cordlang ai` | Propose → check loop |

**Never** emit `@ai` blocks that the compiler must call an LLM to expand.
