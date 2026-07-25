# Skills (portable)

Agent skills live here so they are **not tied to a single vendor** (Grok, Claude, Cursor, Copilot, …).

| Path | Skill | Purpose |
|------|--------|---------|
| [`write-cord/`](./write-cord/) | write-cord | Write UI as `.cord`, not JSX/Svelte |
| [`fix-cord-check/`](./fix-cord-check/) | fix-cord-check | Repair `.cord` from `cordlang check --json` |

## Why `skills/` and not `.grok/` / `.cursor/`?

| Location | Role |
|----------|------|
| **`skills/<name>/SKILL.md`** | **Canonical** skill — commit this, share with everyone |
| **`docs/AI.md`** / **`docs/AI_CONTEXT.md`** | Long-form / compact do/don’t for any model |
| **`AGENTS.md`** (repo root) | Short rules for coding agents |
| **`.github/copilot-instructions.md`** | GitHub Copilot only |
| **`.cursor/rules/cordlang.mdc`** | Thin Cursor mirror — points here; does not replace `skills/` |

Hidden vendor folders (`.grok/`, `.cursor/`, `.claude/`) are optional tool-specific mirrors. This repo keeps **one** portable tree under `skills/` so any agent can `@skills/write-cord/SKILL.md` without a proprietary path.

## Using a skill

1. Open `skills/write-cord/SKILL.md` and follow it.  
2. Point your agent at `AGENTS.md` + `docs/AI_CONTEXT.md` (or `docs/AI.md`) for full context.  
3. In chat: “follow skills/write-cord” or attach that file.
4. In Cursor, `.cursor/rules/cordlang.mdc` auto-attaches for `*.cord` edits.
