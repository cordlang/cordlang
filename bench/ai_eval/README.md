# AI contract eval (no LLM)

Deterministic harness for Cordlang’s **anti-hallucination contract**.

It does **not** call a model. It verifies that known IA-fail fixtures stay red and a few green fixtures stay green under `cordlang check`.

## Run

```bash
make -C ../..   # from repo cordlang/
./bench/ai_eval/run_ai_eval.sh
# or:
CORDLANG_BIN=/path/to/cordlang ./bench/ai_eval/run_ai_eval.sh
```

## Cases

| Kind | Sources |
|------|---------|
| Must fail | `tests/fixtures/ia_fail_*.cord` |
| Must pass | `typed_props_ok.cord`, `basic_counter.cord` |

## Hooking a model later

1. Give the model a prompt (e.g. “write a counter in Cordlang”).
2. Write the model output to a temp `.cord` file.
3. Run `cordlang check --json` on that file.
4. Score: % of prompts with zero errors. Reuse the same CLI surface; do not put the LLM in `compile`.
