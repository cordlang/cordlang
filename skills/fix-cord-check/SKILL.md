---
name: fix-cord-check
description: >
  Repair Cordlang (.cord) files after cordlang check fails. Use when check
  reports errors/warnings, when the user says "fix cord check", "arregla el
  check", or when write-cord edits fail validation. Prefer cordlang check --json
  and apply each hint.
---

# Fix Cordlang check failures

> **Portable skill** — path: `skills/fix-cord-check/`

You repair **`.cord`** sources so `cordlang check` exits 0. Do not invent keywords.

## Loop

1. Run:

```bash
cordlang check --json [path]
```

2. For each diagnostic with `"hint"`, apply the suggested rewrite.
3. Re-run check until the JSON array is empty of `"level":"error"` (warnings OK unless user asks for clean).
4. Optionally: `cordlang analyze [--json]` (assumes check green).

## Common codes

| code | Typical fix |
|------|-------------|
| `jsx-attr` | `className` → `class` or style attrs; `onClick` → `@click=…` |
| `bad-interp` | `{count}` in text → `#{count}` |
| `semantic-vocab` | use documented `purpose` / `importance` vocabulary |
| (unknown attr warn) | drop attr or use name from `docs/schema/attrs.json` |

## Rules

- Patch `.cord` only — never `dist/**` as source of truth.
- Prefer minimal edits; do not rewrite working sections.
- After green check, mention `cordlang run` / `run react` / `run svelte` if relevant.
- Authoring new UI from scratch → switch to `skills/write-cord/SKILL.md`.

## References

- `docs/AI_CONTEXT.md` · `docs/AI_WORKFLOW.md` · `docs/schema/attrs.json`
- Fixtures: `tests/fixtures/ia_fail_*.cord`
