# Cord vs JSX / Svelte size comparison

Honest framing: Cordlang’s main win is **source density / tokens for AI**.  
This harness measures **bytes of Cord source**, **bytes of `cordlang compile` output**, and **bytes of hand-written JSX/Svelte** for the same micro-UI. It does **not** claim runtime FPS superiority.

## Usage

```bash
./bench/compare/run_compare.sh
# writes bench/compare/RESULTS.md
```

## Cases

| Case | Cord | Hand-written |
|------|------|--------------|
| `counter` | `cases/counter/app.cord` | `Counter.jsx`, `Counter.svelte` |
| `list_if` | `cases/list_if/app.cord` | `ListIf.jsx`, `ListIf.svelte` |
