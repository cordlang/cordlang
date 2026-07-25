#!/usr/bin/env bash
# Scaffold a stock template + vite build for react and svelte.
# Usage (from repo root, after `make`):
#   ./tests/run_template_check.sh [counter|landing|dashboard|…]

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
TEMPLATE="${1:-counter}"

if [[ -x "$ROOT/cordlang" ]]; then
  CORDLANG="$ROOT/cordlang"
elif [[ -x "$ROOT/cordlang.exe" ]]; then
  CORDLANG="$ROOT/cordlang.exe"
else
  echo "FAIL: cordlang not found. Run make / build.bat first."
  exit 1
fi

APP="$ROOT/templates/$TEMPLATE"
if [[ ! -f "$APP/cordlang.json" ]]; then
  echo "FAIL: templates/$TEMPLATE/cordlang.json missing"
  exit 1
fi

echo "Cordlang template --check"
echo "  root: $ROOT"
echo "  exe:  $CORDLANG"
echo "  app:  $APP"

failed=0
for backend in react svelte; do
  echo ""
  echo "=== cordlang run $backend --check ==="
  if (cd "$APP" && "$CORDLANG" run "$backend" --check); then
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
echo "Results: react + svelte --check OK ($TEMPLATE)"
exit 0
