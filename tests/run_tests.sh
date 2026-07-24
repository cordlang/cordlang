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

FIXTURES=(basic_counter routes_simple interp if_for nested_routes component_slot string_dotted escape_hash_brace react_phase_d svelte_phase_e preset_caps)
# Main matrix: SPA backends with full golden coverage.
# email/pdf/next/sveltekit use dedicated fixtures + regression/ (see below).
BACKENDS=(react svelte vue solid)
EXTRA_GOLDENS=(
  "email_static:email"
  "email_static:pdf"
)
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

# Dedicated goldens for static/meta backends (not in main BACKENDS matrix)
for pair in "${EXTRA_GOLDENS[@]}"; do
  name="${pair%%:*}"
  backend="${pair##*:}"
  fixture="$ROOT/tests/fixtures/$name.cord"
  golden="$GOLDEN_DIR/$name.$backend.txt"
  label="$name ($backend)"
  if [[ ! -f "$fixture" ]]; then
    echo "FAIL: missing fixture $fixture"
    failed=$((failed + 1))
    continue
  fi
  if ! "$CORDLANG" compile "$fixture" --backend "$backend" >/dev/null 2>&1; then
    echo "FAIL: $label — compile failed"
    failed=$((failed + 1))
    continue
  fi
  actual="$("$CORDLANG" compile "$fixture" --backend "$backend" 2>/dev/null | normalize)"
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

# ── Formatter tests (Phase C6) ─────────────────────────────
# ── Regression tests (Phase H1) ─────────────────────────────
echo ""
echo "Regression tests"

REG_ROOT="$ROOT/tests/regression"
if [[ -d "$REG_ROOT" ]]; then
  shopt -s nullglob
  for reg_dir in "$REG_ROOT"/*/; do
    [[ -d "$reg_dir" ]] || continue
    slug="$(basename "$reg_dir")"
    [[ "$slug" == .* ]] && continue
    input="$reg_dir/input.cord"
    if [[ ! -f "$input" ]]; then
      continue
    fi

    if [[ -f "$reg_dir/expect_check_nonzero" ]]; then
      label="regression/$slug (check)"
      set +e
      out=$("$CORDLANG" check "$input" 2>&1)
      ec=$?
      set -e
      if [[ "$ec" -eq 0 ]]; then
        echo "FAIL: $label — expected non-zero check"
        failed=$((failed + 1))
      else
        if [[ -f "$reg_dir/expect_check_contains.txt" ]]; then
          needle="$(tr -d '\r' < "$reg_dir/expect_check_contains.txt" | head -n1)"
          if echo "$out" | grep -Fq "$needle"; then
            echo "PASS: $label"
            passed=$((passed + 1))
          else
            echo "FAIL: $label — missing substring '$needle'"
            echo "$out"
            failed=$((failed + 1))
          fi
        else
          echo "PASS: $label"
          passed=$((passed + 1))
        fi
      fi
    fi

    # Any expected.<backend>.txt — including email/pdf/next/sveltekit smokes
    for expected in "$reg_dir"/expected.*.txt; do
      [[ -f "$expected" ]] || continue
      base="$(basename "$expected")"
      backend="${base#expected.}"
      backend="${backend%.txt}"
      label="regression/$slug ($backend)"
      pass_args=()
      if [[ -f "$reg_dir/passes.txt" ]]; then
        while IFS= read -r pname || [[ -n "$pname" ]]; do
          pname="${pname//$'\r'/}"
          [[ -z "$pname" || "$pname" == \#* ]] && continue
          pass_args+=(--pass "$pname")
        done < "$reg_dir/passes.txt"
      fi
      if ! "$CORDLANG" compile "$input" --backend "$backend" "${pass_args[@]}" >/dev/null 2>&1; then
        echo "FAIL: $label — compile failed"
        failed=$((failed + 1))
        continue
      fi
      actual="$("$CORDLANG" compile "$input" --backend "$backend" "${pass_args[@]}" 2>/dev/null | normalize)"
      if [[ "$UPDATE" -eq 1 ]]; then
        printf '%s' "$actual" > "$expected"
        echo "UPDATE: $label -> $expected"
        updated=$((updated + 1))
        passed=$((passed + 1))
        continue
      fi
      want="$(normalize < "$expected")"
      if [[ "$actual" == "$want" ]]; then
        echo "PASS: $label"
        passed=$((passed + 1))
      else
        echo "FAIL: $label — output differs from expected"
        echo "  expected: $expected"
        failed=$((failed + 1))
      fi
    done
  done
  shopt -u nullglob
else
  echo "SKIP: tests/regression not present"
fi

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

ia_cn="$ROOT/tests/fixtures/ia_fail_classname.cord"
if [[ -f "$ia_cn" ]]; then
  if ! "$CORDLANG" check "$ia_cn" >/dev/null 2>&1; then
    echo "PASS: check ia_fail_classname (non-zero)"
    passed=$((passed + 1))
  else
    echo "FAIL: check ia_fail_classname expected non-zero"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: missing ia_fail_classname.cord"
  failed=$((failed + 1))
fi

ia_ty="$ROOT/tests/fixtures/ia_fail_bad_prop_type.cord"
if [[ -f "$ia_ty" ]]; then
  if ! "$CORDLANG" check "$ia_ty" >/dev/null 2>&1; then
    echo "PASS: check ia_fail_bad_prop_type (non-zero)"
    passed=$((passed + 1))
  else
    echo "FAIL: check ia_fail_bad_prop_type expected non-zero"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: missing ia_fail_bad_prop_type.cord"
  failed=$((failed + 1))
fi

typed_ok="$ROOT/tests/fixtures/typed_props_ok.cord"
if [[ -f "$typed_ok" ]]; then
  if "$CORDLANG" check "$typed_ok" >/dev/null 2>&1; then
    echo "PASS: check typed_props_ok (zero)"
    passed=$((passed + 1))
  else
    echo "FAIL: check typed_props_ok expected zero"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: missing typed_props_ok.cord"
  failed=$((failed + 1))
fi

ia_ua="$ROOT/tests/fixtures/ia_fail_unknown_attr.cord"
if [[ -f "$ia_ua" ]]; then
  # unknown attr is warning-only → exit 0 but should print warning
  out=$("$CORDLANG" check "$ia_ua" 2>&1 || true)
  if echo "$out" | grep -q "unknown attribute"; then
    echo "PASS: check ia_fail_unknown_attr (warn)"
    passed=$((passed + 1))
  else
    echo "FAIL: check ia_fail_unknown_attr expected warning"
    echo "$out"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: missing ia_fail_unknown_attr.cord"
  failed=$((failed + 1))
fi

# AI trap codes: jsx-hook / jsx-map / jsx-tag
for ia_pair in "ia_fail_usestate.cord:jsx-hook" "ia_fail_map.cord:jsx-map" "ia_fail_jsx_tag.cord:jsx-tag" "ia_fail_missing_preset.cord:missing-preset" "ia_fail_foreign_unbound.cord:foreign-unbound"; do
  ia_file="${ia_pair%%:*}"
  ia_code="${ia_pair##*:}"
  ia_path="$ROOT/tests/fixtures/$ia_file"
  if [[ -f "$ia_path" ]]; then
    out=$("$CORDLANG" check --json "$ia_path" 2>/dev/null || true)
    if echo "$out" | grep -q "\"code\":\"$ia_code\""; then
      echo "PASS: check $ia_file → $ia_code"
      passed=$((passed + 1))
    else
      echo "FAIL: check $ia_file expected code $ia_code"
      echo "$out"
      failed=$((failed + 1))
    fi
  else
    echo "FAIL: missing $ia_file"
    failed=$((failed + 1))
  fi
done

# analyze smoke
if "$CORDLANG" analyze "$typed_ok" >/dev/null 2>&1; then
  echo "PASS: analyze typed_props_ok"
  passed=$((passed + 1))
else
  echo "FAIL: analyze typed_props_ok"
  failed=$((failed + 1))
fi

# preset CLI + scaffold merge smoke
preset_tmp=$(mktemp -d)
mkdir -p "$preset_tmp/src"
cat > "$preset_tmp/cordlang.json" <<'EOF'
{
  "name": "preset-smoke",
  "entry": "src/app.cord",
  "presets": []
}
EOF
cp "$ROOT/examples/preset_motion_icons.cord" "$preset_tmp/src/app.cord"
if (cd "$preset_tmp" && "$CORDLANG" preset add icons motion charts >/dev/null 2>&1); then
  if grep -q '"icons"' "$preset_tmp/cordlang.json" && grep -q '"motion"' "$preset_tmp/cordlang.json"; then
    echo "PASS: preset add updates cordlang.json"
    passed=$((passed + 1))
  else
    echo "FAIL: preset add did not write presets"
    failed=$((failed + 1))
  fi
else
  echo "FAIL: preset add"
  failed=$((failed + 1))
fi
if (cd "$preset_tmp" && "$CORDLANG" check >/dev/null 2>&1); then
  echo "PASS: check with presets (zero)"
  passed=$((passed + 1))
else
  echo "FAIL: check with presets expected zero"
  failed=$((failed + 1))
fi
for be in react svelte; do
  if (cd "$preset_tmp" && "$CORDLANG" run "$be" --check >/dev/null 2>&1); then
    echo "PASS: run $be --check (presets)"
    passed=$((passed + 1))
  else
    echo "FAIL: run $be --check (presets)"
    failed=$((failed + 1))
  fi
  pkg="$preset_tmp/dist/$be/package.json"
  if [[ -f "$pkg" ]]; then
    if [[ "$be" == "react" ]] && grep -q 'lucide-react\|framer-motion\|recharts' "$pkg"; then
      echo "PASS: react package.json merged preset deps"
      passed=$((passed + 1))
    elif [[ "$be" == "svelte" ]] && grep -q '@lucide/svelte\|lucide-svelte' "$pkg"; then
      echo "PASS: svelte package.json merged preset deps"
      passed=$((passed + 1))
    else
      echo "FAIL: $be package.json missing preset deps"
      cat "$pkg" || true
      failed=$((failed + 1))
    fi
  else
    echo "FAIL: missing $pkg"
    failed=$((failed + 1))
  fi
done
rm -rf "$preset_tmp"

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
