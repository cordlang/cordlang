#!/usr/bin/env bash
# Deterministic AI-contract eval (no LLM).
# - ia_fail_* that are ERRORS must exit non-zero
# - ia_fail_unknown_attr must emit a warning (may exit 0)
# - green fixtures must exit 0
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="${CORDLANG_BIN:-$ROOT/cordlang}"
FIX="$ROOT/tests/fixtures"

if [[ ! -x "$CLI" ]]; then
  echo "error: cordlang binary not found at $CLI (build with make)" >&2
  exit 1
fi

pass=0
fail=0

run_expect_fail() {
  local f="$1"
  local label="$2"
  if "$CLI" check "$f" >/dev/null 2>&1; then
    echo "FAIL: $label (expected non-zero check)"
    fail=$((fail + 1))
  else
    echo "PASS: $label (check fails as expected)"
    pass=$((pass + 1))
  fi
}

run_expect_warn() {
  local f="$1"
  local label="$2"
  local out
  out="$("$CLI" check "$f" 2>&1 || true)"
  if echo "$out" | grep -qiE 'warning|unknown attribute'; then
    echo "PASS: $label (warning as expected)"
    pass=$((pass + 1))
  else
    echo "FAIL: $label (expected warning in check output)"
    fail=$((fail + 1))
  fi
}

run_expect_ok() {
  local f="$1"
  local label="$2"
  if "$CLI" check "$f" >/dev/null 2>&1; then
    echo "PASS: $label (check ok)"
    pass=$((pass + 1))
  else
    echo "FAIL: $label (expected zero check)"
    fail=$((fail + 1))
  fi
}

echo "cordlang ai_eval — contract baseline (no LLM)"
echo "cli: $CLI"
echo

for f in "$FIX"/ia_fail_*.cord; do
  [[ -f "$f" ]] || continue
  base="$(basename "$f")"
  if [[ "$base" == "ia_fail_unknown_attr.cord" ]]; then
    run_expect_warn "$f" "$base"
  else
    run_expect_fail "$f" "$base"
  fi
done

run_expect_ok "$FIX/typed_props_ok.cord" "typed_props_ok.cord"
run_expect_ok "$FIX/basic_counter.cord" "basic_counter.cord"

echo
echo "Results: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
