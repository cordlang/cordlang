# Vue → Official promotion checklist

Vue is **Candidate** today ([BACKENDS.md](./BACKENDS.md)). Promote to **Official** only when every box below is green. Do not market Vue as co-equal with React/Svelte until then.

## Required

- [ ] [VUE.md](./VUE.md) depth roughly matches [REACT.md](./REACT.md) / [SVELTE.md](./SVELTE.md) (limits, presets, routing caveats, AI traps)
- [ ] README + [AI_CONTEXT.md](./AI_CONTEXT.md) list Vue as a supported SPA target (not “meta only”)
- [ ] [BACKENDS.md](./BACKENDS.md) tier table moves `vue` from Candidate → Official; CLI `help` updated
- [ ] `tests/run_template_check.ps1` (or parity `-Full`) includes `cordlang run vue --check` on `templates/counter`
- [ ] At least one Vue-oriented callout in [EXAMPLES.md](./EXAMPLES.md) (or a dedicated multi-file example)
- [ ] Preset merge smoke: `cordlang preset add icons` + vue scaffold lists expected deps (parity with react/svelte Linux checks)
- [ ] `tests/run_backend_parity.ps1` Official+Candidate green on CI (or documented job)
- [ ] Explicit “not Nuxt” note (same honesty as Next/Kit SPA wrappers)

## Already true (do not regress)

- [x] IR-first codegen (`vue_ir.c`) + Vite scaffold
- [x] Goldens in `tests/run_tests.ps1` matrix (`vue` alongside react/svelte/solid)
- [x] Routes / layouts / context mapped in [VUE.md](./VUE.md)
- [x] `cordlang run vue --check` works locally (Node)

## After promotion

1. Update [ROADMAP.md](./ROADMAP.md) § meta: Vue leaves “meta / experimental”.
2. Mention Vue next to react/svelte in [AGENTS.md](../AGENTS.md) preview table if agents should default to it.
3. Optionally add Vue to default AI wording in skills (`write-cord` run commands).
