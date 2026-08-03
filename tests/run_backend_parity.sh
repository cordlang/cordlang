#!/usr/bin/env bash
# Backend parity report — see docs/BACKENDS.md and run_backend_parity.ps1
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

INCLUDE_META=0
STRICT_META=0
FULL=0
for arg in "$@"; do
  case "$arg" in
    --include-meta) INCLUDE_META=1 ;;
    --strict-meta) STRICT_META=1 ;;
    --full) FULL=1 ;;
  esac
done

if [[ -x "$ROOT/cordlang" ]]; then
  COR="$ROOT/cordlang"
elif [[ -x "$ROOT/cordlang.exe" ]]; then
  COR="$ROOT/cordlang.exe"
else
  echo "FAIL: cordlang not found. Build first."
  exit 1
fi

FIXTURE="$ROOT/tests/fixtures/basic_counter.cord"
[[ -f "$FIXTURE" ]] || { echo "FAIL: missing fixture"; exit 1; }

echo "Cordlang backend parity"
echo "  root: $ROOT"
echo ""
echo "Tiers (docs/BACKENDS.md):"
echo "  Official:     esm/preview, react, svelte, vue"
echo "  Experimental: solid, html, email, pdf, next, sveltekit"
echo ""

failed=0
meta_failed=0

compile_be() {
  local be="$1" tier="$2"
  echo "=== compile [$tier] --backend $be ==="
  if ! "$COR" compile "$FIXTURE" --backend "$be" >/dev/null; then
    echo "FAIL: compile --backend $be"
    return 1
  fi
  echo "PASS: $be"
  return 0
}

compile_be react Official || failed=$((failed + 1))
compile_be svelte Official || failed=$((failed + 1))
compile_be vue Official || failed=$((failed + 1))

APP="$ROOT/templates/counter"
echo "=== build [Official] esm (templates/counter) ==="
if [[ -f "$APP/cordlang.json" ]]; then
  (cd "$APP" && "$COR" build esm >/dev/null) || { echo "FAIL: build esm"; failed=$((failed + 1)); }
  echo "PASS: build esm"
else
  echo "SKIP: templates/counter missing"
fi

if [[ "$INCLUDE_META" -eq 1 ]]; then
  for be in solid email pdf next sveltekit html; do
    if ! compile_be "$be" Experimental; then
      if [[ "$STRICT_META" -eq 1 ]]; then
        failed=$((failed + 1))
      else
        meta_failed=$((meta_failed + 1))
        echo "(soft) experimental failure"
      fi
    fi
  done
fi

if [[ "$FULL" -eq 1 ]]; then
  if [[ -x "$ROOT/tests/run_template_check.sh" ]]; then
    "$ROOT/tests/run_template_check.sh" || failed=$((failed + 1))
  fi
  if [[ -f "$ROOT/tests/run_preview_smoke.ps1" ]] && command -v pwsh >/dev/null 2>&1; then
    pwsh -File "$ROOT/tests/run_preview_smoke.ps1" || failed=$((failed + 1))
  elif [[ -x "$COR" ]]; then
    (cd "$APP" && "$COR" run --smoke) || failed=$((failed + 1))
  fi
fi

echo ""
echo "Matrix:"
echo "  feature           esm  react  svelte  vue"
echo "  compile smoke     yes  yes    yes     yes"
echo "  template --check  n/a  yes*   yes*    yes*"
echo ""

if [[ "$meta_failed" -gt 0 ]]; then
  echo "Experimental soft failures: $meta_failed"
fi

if [[ "$failed" -gt 0 ]]; then
  echo "Results: $failed Official failure(s)"
  exit 1
fi

echo "Results: Official OK"
exit 0
