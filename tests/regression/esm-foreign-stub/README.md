# esm-foreign-stub

**Symptom.** `foreign Chart from "recharts"` compiled with `--backend esm`
emitted `h(Chart, …)` with no binding, so the preview died with
`ReferenceError: Chart is not defined`.

**Cause.** The ESM backend never handled `IR_FOREIGN`. Unlike React/Svelte
scaffolds, the native preview has no npm resolve path for host packages.

**Fix.** Collect `IR_FOREIGN` nodes and emit a visible stub component (same
pattern as unresolved `use` modules) with class `cord-runtime-error`. The
PascalCase tag still becomes `h(Chart, …)` but binds to the local `const Chart`
stub. No `import Chart from 'recharts'` is emitted.

This pin locks: stub present, `cord-runtime-error` present, no npm import for
the foreign package.
