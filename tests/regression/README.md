# Regression tests

Each subdirectory under `tests/regression/<slug>/` pins a past bug or trap.

## Layout

| File | Role |
|------|------|
| `input.cord` | Required source under test |
| `expected.react.txt` / `expected.svelte.txt` / `expected.<backend>.txt` | Optional codegen snapshots (any registered backend) |
| `expect_check_nonzero` | Marker: `cordlang check` must exit non-zero |
| `expect_check_contains.txt` | Optional substring that check output must include |
| `passes.txt` | Optional: one IR pass name per line (passed as `--pass`) |
| `README.md` | Short symptom + why this case exists |

The suite runners (`tests/run_tests.sh`, `tests/run_tests.ps1`) discover every subdirectory that contains `input.cord`.

## Policy

**Every bug fix that merges must add a regression case here** (or extend an existing one). Prefer a dedicated slug over only relying on goldens in `tests/fixtures/`.
