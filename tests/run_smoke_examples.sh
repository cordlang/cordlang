#!/usr/bin/env bash
# Smoke: cordlang check on templates + single-file examples.
# Usage: ./tests/run_smoke_examples.sh

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CORDLANG="$ROOT/cordlang"
if [[ ! -x "$CORDLANG" ]]; then
  CORDLANG="$ROOT/cordlang.exe"
fi
if [[ ! -x "$CORDLANG" && ! -f "$CORDLANG" ]]; then
  echo "FAIL: cordlang executable not found. Build first."
  exit 1
fi

fail=0
pass=0

check_one() {
  local label="$1"
  shift
  if "$CORDLANG" check "$@" >/tmp/cord_smoke_out.txt 2>/tmp/cord_smoke_err.txt; then
    echo "PASS: $label"
    pass=$((pass + 1))
  else
    echo "FAIL: $label"
    cat /tmp/cord_smoke_err.txt /tmp/cord_smoke_out.txt | head -40
    fail=$((fail + 1))
  fi
}

echo "Smoke: templates"
for tpl in "$ROOT"/templates/*/cordlang.json; do
  dir="$(dirname "$tpl")"
  name="$(basename "$dir")"
  if [[ -f "$dir/src/app.cord" ]]; then
    (cd "$dir" && check_one "template/$name" )
  else
    echo "SKIP: template/$name (no src/app.cord)"
  fi
done

echo "Smoke: examples (*.cord)"
# Single-file samples that are self-contained (skip multi-file demos).
SKIP_EXAMPLES="react_advanced.cord routes.cord"
shopt -s nullglob
for f in "$ROOT"/examples/*.cord; do
  name="$(basename "$f")"
  skip=0
  for s in $SKIP_EXAMPLES; do
    if [[ "$name" == "$s" ]]; then skip=1; break; fi
  done
  if [[ "$skip" -eq 1 ]]; then
    echo "SKIP: examples/$name (multi-file / incomplete sample)"
    continue
  fi
  check_one "examples/$name" "$f"
done

echo
echo "Results: $pass passed, $fail failed"
exit "$fail"
