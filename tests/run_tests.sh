#!/usr/bin/env bash
# Cordlang golden tests (Unix)
# Usage:
#   ./tests/run_tests.sh
#   ./tests/run_tests.sh --update

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

UPDATE=0
if [[ "${1:-}" == "--update" || "${1:-}" == "-UpdateGoldens" ]]; then
  UPDATE=1
fi

FIXTURES=(basic_counter routes_simple interp if_for nested_routes component_slot string_dotted)
BACKENDS=(react svelte)
GOLDEN_DIR="$ROOT/tests/golden"
mkdir -p "$GOLDEN_DIR"

normalize() {
  # strip CR
  tr -d '\r'
}

failed=0
passed=0
updated=0

for name in "${FIXTURES[@]}"; do
  fixture="$ROOT/tests/fixtures/$name.cord"
  if [[ ! -f "$fixture" ]]; then
    echo "FAIL: missing fixture $fixture"
    failed=$((failed + 1))
    continue
  fi
  for backend in "${BACKENDS[@]}"; do
    golden="$GOLDEN_DIR/$name.$backend.txt"
    label="$name ($backend)"
    actual="$("$CORDLANG" compile "$fixture" --backend "$backend" 2>/dev/null | normalize || true)"
    if ! "$CORDLANG" compile "$fixture" --backend "$backend" >/dev/null 2>&1; then
      echo "FAIL: $label — compile failed"
      failed=$((failed + 1))
      continue
    fi
    actual="$("$CORDLANG" compile "$fixture" --backend "$backend" | normalize)"

    if [[ "$UPDATE" -eq 1 || ! -f "$golden" ]]; then
      printf '%s' "$actual" > "$golden"
      if [[ "$UPDATE" -eq 1 ]]; then
        echo "UPDATE: $label -> $golden"
      else
        echo "CREATE: $label -> $golden (first run)"
      fi
      updated=$((updated + 1))
      passed=$((passed + 1))
      continue
    fi

    expected="$(normalize < "$golden")"
    if [[ "$actual" == "$expected" ]]; then
      echo "PASS: $label"
      passed=$((passed + 1))
    else
      echo "FAIL: $label — output differs from golden"
      echo "  golden: $golden"
      failed=$((failed + 1))
    fi
  done
done

# ── Formatter tests (Phase C6) ─────────────────────────────
echo ""
echo "Formatter tests (fmt)"

messy="$ROOT/tests/fixtures/messy_fmt.cord"
# tabs, trailing spaces, triple blank lines, no final newline
printf '%s' $'# messy fixture for fmt\n\n\n\ndef Counter\t\n  state count=0  \n\n\n  props label="Counter"\t\n  col gap=16 p=24 center   \n    h1 "#{label}" size=2xl bold' > "$messy"

if ! "$CORDLANG" fmt --check "$messy" >/dev/null 2>&1; then
  echo "PASS: fmt --check messy (would change)"
  passed=$((passed + 1))
else
  echo "FAIL: fmt --check messy expected non-zero"
  failed=$((failed + 1))
fi

if "$CORDLANG" fmt "$messy" >/dev/null 2>&1; then
  echo "PASS: fmt messy (in-place)"
  passed=$((passed + 1))
else
  echo "FAIL: fmt messy exit non-zero"
  failed=$((failed + 1))
fi

if "$CORDLANG" fmt --check "$messy" >/dev/null 2>&1; then
  echo "PASS: fmt --check clean after fmt"
  passed=$((passed + 1))
else
  echo "FAIL: fmt --check after fmt expected zero"
  failed=$((failed + 1))
fi

if ! grep -q $'\t' "$messy" && [[ "$(tail -c1 "$messy" | wc -l)" -eq 1 ]]; then
  echo "PASS: fmt normalize (tabs/final newline)"
  passed=$((passed + 1))
else
  echo "FAIL: fmt normalize unexpected content"
  failed=$((failed + 1))
fi

# restore messy for next run
printf '%s' $'# messy fixture for fmt\n\n\n\ndef Counter\t\n  state count=0  \n\n\n  props label="Counter"\t\n  col gap=16 p=24 center   \n    h1 "#{label}" size=2xl bold' > "$messy"

# ── Semantic check tests (Phase C3/C4) ─────────────────────
echo ""
echo "Semantic check tests"

unknown="$ROOT/tests/fixtures/unknown_comp.cord"
if [[ -f "$unknown" ]]; then
  if ! "$CORDLANG" check "$unknown" >/dev/null 2>&1; then
    echo "PASS: check unknown_comp (non-zero)"
    passed=$((passed + 1))
  else
    echo "FAIL: check unknown_comp expected non-zero exit"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: missing fixture unknown_comp.cord"
  failed=$((failed + 1))
fi

if [[ -f "$ROOT/my-app/cordlang.json" ]]; then
  if (cd "$ROOT/my-app" && "$CORDLANG" check >/dev/null 2>&1); then
    echo "PASS: check my-app (zero)"
    passed=$((passed + 1))
  else
    echo "FAIL: check my-app expected zero exit"
    failed=$((failed + 1))
  fi
else
  echo "SKIP: my-app project not present"
fi

echo ""
echo "Results: $passed passed, $failed failed, $updated golden writes"
if [[ "$failed" -gt 0 ]]; then
  exit 1
fi
exit 0
