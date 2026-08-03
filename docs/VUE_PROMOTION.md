# Vue → Official promotion checklist

Vue is **Official** ([BACKENDS.md](./BACKENDS.md)). This checklist records the promotion gate; keep boxes green.

## Required

- [x] [VUE.md](./VUE.md) depth roughly matches [REACT.md](./REACT.md) / [SVELTE.md](./SVELTE.md) (limits, presets, routing caveats, AI traps)
- [x] README + [AI_CONTEXT.md](./AI_CONTEXT.md) list Vue as a supported SPA target (not “meta only”)
- [x] [BACKENDS.md](./BACKENDS.md) tier table moves `vue` from Candidate → Official; CLI `help` updated
- [x] `tests/run_template_check.ps1` includes `cordlang run vue --check` on `templates/counter`
- [x] At least one Vue-oriented callout in [EXAMPLES.md](./EXAMPLES.md)
- [x] Preset merge smoke: `cordlang preset add icons` + vue scaffold lists `lucide-vue-next` (parity with react/svelte in `run_tests.sh`)
- [x] `tests/run_backend_parity.ps1` Official green on CI (compile smoke + template check)
- [x] Explicit “not Nuxt” note (same honesty as Next/Kit SPA wrappers)

## Already true (do not regress)

- [x] IR-first codegen (`vue_ir.c`) + Vite scaffold
- [x] Goldens in `tests/run_tests.ps1` matrix (`vue` alongside react/svelte/solid)
- [x] Routes / layouts / context mapped in [VUE.md](./VUE.md)
- [x] `cordlang run vue --check` works locally (Node)

## After promotion

1. [x] Update [ROADMAP.md](./ROADMAP.md) § meta: Vue leaves Candidate / experimental.
2. [x] Mention Vue next to react/svelte in [AGENTS.md](../AGENTS.md) preview table.
3. Optionally add Vue to default AI wording in skills (`write-cord` run commands).
