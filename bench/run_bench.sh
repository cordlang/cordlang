#!/usr/bin/env bash
# Compile-time benchmarks for Cordlang (median wall clock).
# Usage: ./bench/run_bench.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CORDLANG="$ROOT/cordlang"
[[ -x "$CORDLANG" ]] || CORDLANG="$ROOT/cordlang.exe"
if [[ ! -x "$CORDLANG" && ! -f "$CORDLANG" ]]; then
  echo "FAIL: build cordlang first (make / build.bat)"
  exit 1
fi

ITERS="${BENCH_ITERS:-20}"
BACKEND="${BENCH_BACKEND:-react}"

median_ms() {
  local file="$1"
  local -a times=()
  local i t0 t1 ms
  for ((i = 0; i < ITERS; i++)); do
    t0=$(date +%s%N)
    "$CORDLANG" compile "$file" --backend "$BACKEND" >/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1 - t0) / 1000000 ))
    times+=("$ms")
  done
  # Sort copy without pipefail SIGPIPE from head|tail
  local -a sorted
  mapfile -t sorted < <(printf '%s\n' "${times[@]}" | sort -n)
  local n=${#sorted[@]}
  local mid=$((n / 2))
  local min="${sorted[0]}"
  local max="${sorted[$((n - 1))]}"
  local med="${sorted[$mid]}"
  printf '%s %s %s\n' "$med" "$min" "$max"
}

run_case() {
  local label="$1"
  local file="$2"
  if [[ ! -f "$file" ]]; then
    echo "SKIP: $label (missing $file)"
    return
  fi
  read -r med min max < <(median_ms "$file")
  printf 'PASS: %-12s backend=%-6s iters=%-3s  median=%4sms  min=%4sms  max=%4sms  (%s)\n' \
    "$label" "$BACKEND" "$ITERS" "$med" "$min" "$max" "$file"
}

echo "Cordlang compile bench"
echo "  exe=$CORDLANG  backend=$BACKEND  iters=$ITERS"
echo ""

run_case "S" "$ROOT/bench/fixtures/small.cord"
run_case "M" "$ROOT/tests/fixtures/nested_routes.cord"
run_case "L" "$ROOT/bench/fixtures/large.cord"
run_case "template-counter" "$ROOT/templates/counter/src/app.cord"

echo ""
echo "Done. (Wall time of cordlang compile only; not Vite/runtime.)"
