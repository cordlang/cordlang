# Cordlang compile benchmarks

Measures **compiler wall time** (not framework runtime FPS).

## Usage

```bash
make            # build cordlang
make bench      # or: ./bench/run_bench.sh
```

Options (env):

| Variable | Default | Meaning |
|----------|---------|---------|
| `BENCH_ITERS` | `20` | iterations per case |
| `BENCH_BACKEND` | `react` | `react` or `svelte` |

Reports median / min / max milliseconds per fixture size:

- **S** — `bench/fixtures/small.cord` (single component)
- **M** — `tests/fixtures/nested_routes.cord` (routes + layout surface)
- **L** — `bench/fixtures/large.cord` (generated many nodes)
- **template-counter** — `templates/counter/src/app.cord` (multi-file project parse)

## Compare suite

See [`compare/`](./compare/) for Cord vs hand-written JSX/Svelte **source/output sizes**.

## AI contract eval

See [`ai_eval/`](./ai_eval/) — deterministic `check` baseline for IA-fail fixtures (no LLM).

## CI

Not part of the default PR CI (timing noise). Optional:

```bash
gh workflow run bench.yml   # if workflow is present
```
