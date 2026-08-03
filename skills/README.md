# Skills (portable)

Agent skills live here so they are **not tied to a single vendor** (Grok, Claude, Cursor, Copilot, …).

| Path | Skill | Purpose |
|------|--------|---------|
| [`write-cord/`](./write-cord/) | write-cord | Write UI as `.cord`, not JSX/Svelte |
| [`fix-cord-check/`](./fix-cord-check/) | fix-cord-check | Repair `.cord` from `cordlang check --json` |

## Install (recommended)

Use the open [skills](https://skills.sh) CLI — installs into each agent’s skill directory (project or global):

```bash
# Project (commit with the app)
npx skills add cordlang/cordlang -s write-cord -s fix-cord-check -y

# Global + specific agents
npx skills add cordlang/cordlang -s write-cord -a cursor -a claude-code -g -y

# List skills in this repo
npx skills add cordlang/cordlang --list
```

Canonical source remains **`skills/<name>/SKILL.md` in this monorepo**. Each skill bundles `references/` so it stays usable after install outside the Cordlang checkout.

## Why `skills/` and not `.grok/` / `.cursor/`?

| Location | Role |
|----------|------|
| **`skills/<name>/SKILL.md`** | **Canonical** skill — commit this, share with everyone |
| **`docs/AI.md`** / **`docs/AI_CONTEXT.md`** | Long-form / compact do/don’t for any model |
| **`AGENTS.md`** (repo root) | Short rules for coding agents |
| **`.github/copilot-instructions.md`** | GitHub Copilot only |
| **`.cursor/rules/cordlang.mdc`** | Thin Cursor mirror — points here; does not replace `skills/` |

Hidden vendor folders (`.grok/`, `.cursor/`, `.claude/`) are optional tool-specific mirrors. This repo keeps **one** portable tree under `skills/` so any agent can `@skills/write-cord/SKILL.md` without a proprietary path — or install via `npx skills`.

## Using a skill

1. Prefer install: `npx skills add cordlang/cordlang -s write-cord` (then the agent loads it from its skills path).
2. Or open `skills/write-cord/SKILL.md` in this repo and follow it.
3. Point your agent at `AGENTS.md` + `docs/AI_CONTEXT.md` (or `docs/AI.md`) when working inside the Cordlang checkout.
4. In chat: “follow write-cord” / “follow skills/write-cord”.
5. In Cursor, `.cursor/rules/cordlang.mdc` auto-attaches for `*.cord` edits in this repo.
