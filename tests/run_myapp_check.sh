#!/usr/bin/env bash
# G4: scaffold my-app + vite build for react and svelte
# Usage (from repo root, after `make`):
#   ./tests/run_myapp_check.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ -x "$ROOT/cordlang" ]]; then
  CORDLANG="$ROOT/cordlang"
elif [[ -x "$ROOT/cordlang.exe" ]]; then
  CORDLANG="$ROOT/cordlang.exe"
else
  echo "FAIL: cordlang not found. Run make / build.bat first."
  exit 1
fi

MYAPP="$ROOT/my-app"
if [[ ! -f "$MYAPP/cordlang.json" ]]; then
  echo "FAIL: my-app/cordlang.json missing"
  exit 1
fi

echo "Cordlang my-app --check"
echo "  root: $ROOT"
echo "  exe:  $CORDLANG"
echo "  app:  $MYAPP"

failed=0
for backend in react svelte; do
  echo ""
  echo "=== cordlang run $backend --check ==="
  if (cd "$MYAPP" && "$CORDLANG" run "$backend" --check); then
    echo "PASS: run $backend --check"
  else
    echo "FAIL: run $backend --check"
    failed=$((failed + 1))
  fi
done

if [[ "$failed" -gt 0 ]]; then
  echo ""
  echo "Results: $failed backend check(s) failed"
  exit 1
fi

echo ""
echo "Results: react + svelte --check OK"
exit 0
