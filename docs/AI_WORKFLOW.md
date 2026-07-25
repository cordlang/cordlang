# Cordlang AI workflow (no LLM in compile)

Cordlang keeps **AI in the edit loop**, never in the AST/`compile` path.

## Happy path (vibecode)

```bash
# 1. Propose / edit .cord (human or agent + skills/write-cord)
# 2. Validate
cordlang check
cordlang check --json     # machine-readable diags (+ code/hint)
cordlang analyze          # optional score / structure heuristics
cordlang analyze --json
# 3. Preview or build
cordlang run react --watch
# or
cordlang run svelte --check
```

## CLI helper

```bash
cordlang ai               # print this workflow + pointers
cordlang ai check [path]  # same as: cordlang check [path]
cordlang ai context       # compact contract (docs/AI_CONTEXT.md)
cordlang ai doctor [path] # check + analyze summary
```

## Agent loop

1. Prefer [`AI_CONTEXT.md`](./AI_CONTEXT.md); fall back to [`AI.md`](./AI.md) + [`schema/attrs.json`](./schema/attrs.json).
2. Patch `.cord` only (not `dist/**`).
3. Run `cordlang check` (or `check --json`) — fix until zero errors. Apply `hint` fields.
4. Optionally `cordlang analyze` for unused comps / missing `to` / layout `slot` / vibecode heuristics.
5. Mention run commands to the user.

## Analyze score weights (deterministic)

| Issue | Weight |
|-------|--------|
| `link` without `to=`/`href=` | −15 |
| layout-like component without `slot` | −10 |
| unused component | −5 |
| no `h1` / too many `h1` | −5 / −3 |
| fetch without loading/error UI | −8 |
| form/action without `pending=` | −6 |
| interactive `btn`/`link` without `purpose=` | −2 (info) |

Assume `check` is green before trusting analyze.

## Fixtures (IA-fail → fix)

Under `tests/fixtures/`:

| Fixture | Trap |
|---------|------|
| `ia_fail_classname.cord` | `className=` / JSX attr (`code: jsx-attr` + hint) |
| `ia_fail_interp.cord` | `{count}` in text instead of `#{count}` (`code: bad-interp`) |
| `ia_fail_unknown_attr.cord` | attr not in schema on built-in tag |
| `ia_fail_bad_prop_type.cord` | invalid prop type (not string\|number\|boolean\|any) |

Agents should turn these red cases into green by rewriting to Cord idioms. Prefer `skills/fix-cord-check` when check fails.

## Skills

| Path | Role |
|------|------|
| [`skills/write-cord/`](../skills/write-cord/) | Author `.cord` |
| [`skills/fix-cord-check/`](../skills/fix-cord-check/) | Repair from `check --json` hints |
| This doc + `cordlang ai` | Propose → check loop |

**Never** emit `@ai` blocks that the compiler must call an LLM to expand.
