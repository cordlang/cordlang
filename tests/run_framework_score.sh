#!/usr/bin/env bash
# Framework scoreboard for Cordlang.
#
# 1. Uses an already-built cordlang binary (does not build the compiler).
# 2. Materializes many projects (stock templates + fixtures as mini-apps).
# 3. Runs each project against every framework/backend.
# 4. Scores each framework: (passed / total) * 100.
# 5. Writes SCOREBOARD.md + results.json with where/how each failure happened.
#
# Usage (from repo root, after make / build.bat):
#   ./tests/run_framework_score.sh
#   SCORE_MODE=full ./tests/run_framework_score.sh   # + vite --check for SPA
#   CORDLANG_BIN=./cordlang OUT_DIR=score-out ./tests/run_framework_score.sh
#
# Env:
#   CORDLANG_BIN   path to cordlang (default: ./cordlang or ./cordlang.exe)
#   OUT_DIR        report + projects workspace (default: score-out)
#   SCORE_MODE     compile (default, fast) | scaffold | full
#                    compile  = compile/build emit only (~minutes on CI)
#                    scaffold = compile + `cordlang run <fw>` project scaffold
#                    full     = scaffold + vite `--check` for SPA backends (slow)
#   SCORE_FAIL_UNDER  if set (0-100), exit 1 when any framework score is below it
#   FRAMEWORKS     space-separated override (default: all public backends)
#   MAX_LOG_BYTES  stderr capture cap per case (default: 4000)
#   CASE_TIMEOUT_SEC  kill a hung case after N seconds (default: 120; 0 = off)

set -u
# do not set -e: we collect failures

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Prefer repo-relative paths so Windows cordlang.exe (invoked from Git Bash /
# WSL paths like /mnt/c/...) still resolves destinations correctly.
OUT_DIR_ABS="${OUT_DIR:-$ROOT/score-out}"
# If OUT_DIR is absolute under ROOT, also keep a relative form for CLI args.
if [[ "$OUT_DIR_ABS" == "$ROOT"/* ]]; then
  OUT_DIR_REL="${OUT_DIR_ABS#$ROOT/}"
elif [[ "$OUT_DIR_ABS" == "$ROOT" ]]; then
  OUT_DIR_REL="."
else
  OUT_DIR_REL="$OUT_DIR_ABS"
fi
OUT_DIR="$OUT_DIR_ABS"
MODE="${SCORE_MODE:-compile}"
MAX_LOG_BYTES="${MAX_LOG_BYTES:-4000}"
CASE_TIMEOUT_SEC="${CASE_TIMEOUT_SEC:-120}"
case "$MODE" in
  compile|scaffold|full) ;;
  *)
    echo "FAIL: SCORE_MODE must be compile|scaffold|full (got: $MODE)" >&2
    exit 1
    ;;
esac
PROJECTS_DIR="$OUT_DIR/projects"
PROJECTS_DIR_REL="$OUT_DIR_REL/projects"
LOGS_DIR="$OUT_DIR/logs"
RESULTS_TSV="$OUT_DIR/results.tsv"
SCOREBOARD="$OUT_DIR/SCOREBOARD.md"
RESULTS_JSON="$OUT_DIR/results.json"

# ── resolve binary ───────────────────────────────────────────────────
resolve_bin() {
  if [[ -n "${CORDLANG_BIN:-}" ]]; then
    echo "$CORDLANG_BIN"
    return
  fi
  if [[ -x "$ROOT/cordlang" ]]; then
    echo "$ROOT/cordlang"
  elif [[ -f "$ROOT/cordlang.exe" ]]; then
    echo "$ROOT/cordlang.exe"
  else
    echo ""
  fi
}

CORDLANG="$(resolve_bin)"
if [[ -z "$CORDLANG" || ! -f "$CORDLANG" ]]; then
  echo "FAIL: cordlang binary not found. Build first (make / build.bat)," >&2
  echo "      or set CORDLANG_BIN=/path/to/cordlang" >&2
  exit 1
fi
# Always absolute: we cd into project dirs for each case.
case "$CORDLANG" in
  /*) ;;
  [A-Za-z]:/* | [A-Za-z]:\\*) ;; # Windows drive path
  *) CORDLANG="$(cd "$(dirname "$CORDLANG")" && pwd)/$(basename "$CORDLANG")" ;;
esac
if [[ ! -x "$CORDLANG" ]]; then
  chmod +x "$CORDLANG" 2>/dev/null || true
fi

# ── frameworks ───────────────────────────────────────────────────────
# SPA / meta that scaffold via `cordlang run <fw>`
# esm uses `cordlang build esm`
DEFAULT_FRAMEWORKS="react svelte vue solid esm html next sveltekit email pdf"
# shellcheck disable=SC2206
FRAMEWORKS_ARR=(${FRAMEWORKS:-$DEFAULT_FRAMEWORKS})

# Backends that support `run --check` (npm + vite)
is_spa_checkable() {
  case "$1" in
    react|svelte|vue|solid|next|sveltekit) return 0 ;;
    *) return 1 ;;
  esac
}

# ── workspace ────────────────────────────────────────────────────────
rm -rf "$OUT_DIR"
mkdir -p "$PROJECTS_DIR" "$LOGS_DIR"
printf 'project\tframework\tstatus\tstep\tdetail\n' >"$RESULTS_TSV"

echo "Cordlang framework score"
echo "  root:      $ROOT"
echo "  binary:    $CORDLANG"
echo "  mode:      $MODE"
echo "  out:       $OUT_DIR"
echo "  frameworks:${FRAMEWORKS_ARR[*]}"
echo ""

# ── materialize projects ─────────────────────────────────────────────
# Each project is a real cordlang project dir with cordlang.json + src/

make_fixture_project() {
  local slug="$1"
  local src_file="$2"
  local dest="$PROJECTS_DIR/$slug"
  mkdir -p "$dest/src"
  cp "$src_file" "$dest/src/app.cord"
  printf '{\n  "name": "%s",\n  "entry": "src/app.cord"\n}\n' "$slug" >"$dest/cordlang.json"
  # verify materialization
  if [[ ! -f "$dest/cordlang.json" || ! -f "$dest/src/app.cord" ]]; then
    echo "WARN: fixture project $slug incomplete" >&2
    return 1
  fi
  echo "$dest"
}

make_template_project() {
  local tmpl="$1"
  local dest_rel="$PROJECTS_DIR_REL/tpl-$tmpl"
  local dest_abs="$PROJECTS_DIR/tpl-$tmpl"
  rm -rf "$dest_abs"
  mkdir -p "$PROJECTS_DIR"
  # Use repo-relative dest so Windows .exe does not choke on /mnt/c/... paths.
  # init copies template; CWD is ROOT so templates/ resolves.
  if ! "$CORDLANG" init "$dest_rel" --template "$tmpl" >/dev/null 2>"$LOGS_DIR/init-$tmpl.err"; then
    echo "WARN: init template $tmpl failed" >&2
    cat "$LOGS_DIR/init-$tmpl.err" >&2 || true
    return 1
  fi
  if [[ ! -f "$dest_abs/cordlang.json" ]]; then
    echo "WARN: init template $tmpl did not create $dest_rel/cordlang.json" >&2
    cat "$LOGS_DIR/init-$tmpl.err" >&2 || true
    return 1
  fi
  echo "$dest_abs"
}

PROJECT_NAMES=()

echo "== Materializing projects =="

# Stock templates (multi-file real apps)
for tmpl in counter landing dashboard form-fetch docs-shell; do
  if [[ -f "$ROOT/templates/$tmpl/cordlang.json" ]]; then
    if dest="$(make_template_project "$tmpl")"; then
      PROJECT_NAMES+=("tpl-$tmpl")
      echo "  + tpl-$tmpl"
    fi
  fi
done

# Positive fixtures → mini projects (skip intentional fail / noise)
SKIP_FIXTURES="messy_fmt unknown_comp"
for f in "$ROOT"/tests/fixtures/*.cord; do
  [[ -f "$f" ]] || continue
  base="$(basename "$f" .cord)"
  case "$base" in
    ia_fail_*) continue ;;
  esac
  skip=0
  for s in $SKIP_FIXTURES; do
    if [[ "$base" == "$s" ]]; then skip=1; break; fi
  done
  [[ $skip -eq 1 ]] && continue
  make_fixture_project "fix-$base" "$f" >/dev/null
  PROJECT_NAMES+=("fix-$base")
  echo "  + fix-$base"
done

# Representative single-file examples
for ex in counter.cord shop.cord fetch_form.cord interp.cord preset_motion_icons.cord; do
  if [[ -f "$ROOT/examples/$ex" ]]; then
    base="${ex%.cord}"
    make_fixture_project "ex-$base" "$ROOT/examples/$ex" >/dev/null
    PROJECT_NAMES+=("ex-$base")
    echo "  + ex-$base"
  fi
done

echo ""
echo "Projects: ${#PROJECT_NAMES[@]}"
echo "Cases:    $(( ${#PROJECT_NAMES[@]} * ${#FRAMEWORKS_ARR[@]} )) (project × framework)"
echo ""

# ── run one case ─────────────────────────────────────────────────────
# Writes: status, step, detail via globals LAST_*
LAST_STATUS=""
LAST_STEP=""
LAST_DETAIL=""

truncate_log() {
  local file="$1"
  if [[ ! -f "$file" ]]; then
    echo ""
    return
  fi
  # portable head by bytes
  if command -v head >/dev/null 2>&1; then
    head -c "$MAX_LOG_BYTES" "$file" 2>/dev/null || cat "$file"
  else
    cat "$file"
  fi
}

# Resolve a path that the cordlang binary can open (Windows .exe prefers
# native or relative paths; pure /mnt/c/... often breaks).
cli_path() {
  local p="$1"
  if [[ "$p" == /* && "$p" == "$ROOT"/* ]]; then
    echo "${p#$ROOT/}"
  else
    echo "$p"
  fi
}

# Run a command under optional timeout (GNU timeout on Ubuntu CI).
run_with_timeout() {
  if [[ "${CASE_TIMEOUT_SEC}" -gt 0 ]] && command -v timeout >/dev/null 2>&1; then
    timeout --signal=KILL "${CASE_TIMEOUT_SEC}" "$@"
  else
    "$@"
  fi
}

run_case() {
  local project="$1"
  local fw="$2"
  local proj_dir="$PROJECTS_DIR/$project"
  local proj_cli
  proj_cli="$(cli_path "$proj_dir")"
  local log="$LOGS_DIR/${project}__${fw}.log"
  LAST_STATUS="fail"
  LAST_STEP="missing"
  LAST_DETAIL="project dir missing"
  : >"$log"

  if [[ ! -d "$proj_dir" || ! -f "$proj_dir/cordlang.json" ]]; then
    LAST_DETAIL="no cordlang.json in $proj_cli"
    return 1
  fi

  # Fresh dist so frameworks do not stomp each other mid-flight
  rm -rf "$proj_dir/dist" 2>/dev/null || true

  local entry="src/app.cord"
  if [[ ! -f "$proj_dir/$entry" ]]; then
    # try resolve entry from cordlang.json naively
    if grep -q '"entry"' "$proj_dir/cordlang.json" 2>/dev/null; then
      entry="$(grep -o '"entry"[[:space:]]*:[[:space:]]*"[^"]*"' "$proj_dir/cordlang.json" | head -1 | sed 's/.*"entry"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')"
    fi
  fi

  # Step 1: compile emit (proves backend IR path)
  # CORDLANG is absolute so it still works after cd into the project.
  LAST_STEP="compile"
  if ! (
    cd "$proj_cli" || cd "$proj_dir" || exit 1
    if [[ "$fw" == "esm" ]]; then
      # ESM static export is the project-level path
      run_with_timeout "$CORDLANG" build esm
    else
      run_with_timeout "$CORDLANG" compile "$entry" --backend "$fw"
    fi
  ) >>"$log" 2>&1; then
    LAST_STATUS="fail"
    LAST_DETAIL="$(truncate_log "$log" | tr '\n' ' ' | tr '\t' ' ' | cut -c1-500)"
    [[ -z "$LAST_DETAIL" ]] && LAST_DETAIL="compile/build exit non-zero (see logs/${project}__${fw}.log)"
    return 1
  fi

  # Step 2: scaffold via `run` — only in scaffold|full (NOT default compile).
  # Doing full scaffold for every project×framework is what made GitHub look
  # "stuck" for 30+ minutes (250× heavy filesystem writes).
  if [[ "$MODE" == "scaffold" || "$MODE" == "full" ]]; then
    if [[ "$fw" != "esm" ]]; then
      LAST_STEP="scaffold"
      if ! (
        cd "$proj_cli" || cd "$proj_dir" || exit 1
        run_with_timeout "$CORDLANG" run "$fw"
      ) >>"$log" 2>&1; then
        LAST_STATUS="fail"
        LAST_DETAIL="$(truncate_log "$log" | tr '\n' ' ' | tr '\t' ' ' | cut -c1-500)"
        [[ -z "$LAST_DETAIL" ]] && LAST_DETAIL="run $fw scaffold exit non-zero"
        return 1
      fi
    fi
  fi

  # Step 3 (full mode only): vite production build for SPA/meta backends
  if [[ "$MODE" == "full" ]] && is_spa_checkable "$fw"; then
    LAST_STEP="vite-check"
    if ! (
      cd "$proj_cli" || cd "$proj_dir" || exit 1
      # vite can be slow; allow 2× case timeout when available
      if [[ "${CASE_TIMEOUT_SEC}" -gt 0 ]] && command -v timeout >/dev/null 2>&1; then
        timeout --signal=KILL "$((CASE_TIMEOUT_SEC * 3))" "$CORDLANG" run "$fw" --check
      else
        "$CORDLANG" run "$fw" --check
      fi
    ) >>"$log" 2>&1; then
      LAST_STATUS="fail"
      LAST_DETAIL="$(truncate_log "$log" | tr '\n' ' ' | tr '\t' ' ' | cut -c1-500)"
      [[ -z "$LAST_DETAIL" ]] && LAST_DETAIL="run $fw --check failed"
      return 1
    fi
  fi

  LAST_STATUS="pass"
  LAST_STEP="ok"
  LAST_DETAIL="ok"
  return 0
}

# ── matrix ───────────────────────────────────────────────────────────
echo "== Running project × framework matrix (mode=$MODE) =="
case_i=0
total_cases=$(( ${#PROJECT_NAMES[@]} * ${#FRAMEWORKS_ARR[@]} ))
# Force line-buffered progress so GitHub Actions live logs update per case.
if command -v stdbuf >/dev/null 2>&1; then
  # re-exec self with line buffering only when we are not already wrapped
  :
fi

for project in "${PROJECT_NAMES[@]}"; do
  for fw in "${FRAMEWORKS_ARR[@]}"; do
    case_i=$((case_i + 1))
    # One full line per case (not "… " then later PASS) so GHA streams progress.
    start_ts=$(date +%s 2>/dev/null || echo 0)
    if run_case "$project" "$fw"; then
      end_ts=$(date +%s 2>/dev/null || echo 0)
      dur=0
      [[ "$start_ts" != 0 && "$end_ts" != 0 ]] && dur=$((end_ts - start_ts))
      echo "  [${case_i}/${total_cases}] ${project} × ${fw} → PASS (${dur}s)"
      printf '%s\t%s\tpass\t%s\t%s\n' "$project" "$fw" "$LAST_STEP" "$LAST_DETAIL" >>"$RESULTS_TSV"
    else
      end_ts=$(date +%s 2>/dev/null || echo 0)
      dur=0
      [[ "$start_ts" != 0 && "$end_ts" != 0 ]] && dur=$((end_ts - start_ts))
      echo "  [${case_i}/${total_cases}] ${project} × ${fw} → FAIL @ ${LAST_STEP} (${dur}s)"
      printf '%s\t%s\tfail\t%s\t%s\n' "$project" "$fw" "$LAST_STEP" "$LAST_DETAIL" >>"$RESULTS_TSV"
    fi
  done
done

# ── score aggregation ────────────────────────────────────────────────
# Per-framework counters in parallel files (bash 3 portable)
for fw in "${FRAMEWORKS_ARR[@]}"; do
  echo 0 >"$OUT_DIR/.pass.$fw"
  echo 0 >"$OUT_DIR/.fail.$fw"
  echo 0 >"$OUT_DIR/.total.$fw"
done

while IFS=$'\t' read -r project fw status step detail; do
  [[ "$project" == "project" ]] && continue
  [[ -z "$fw" ]] && continue
  total_f="$OUT_DIR/.total.$fw"
  pass_f="$OUT_DIR/.pass.$fw"
  fail_f="$OUT_DIR/.fail.$fw"
  [[ -f "$total_f" ]] || continue
  t=$(cat "$total_f"); echo $((t + 1)) >"$total_f"
  if [[ "$status" == "pass" ]]; then
    p=$(cat "$pass_f"); echo $((p + 1)) >"$pass_f"
  else
    f=$(cat "$fail_f"); echo $((f + 1)) >"$fail_f"
  fi
done <"$RESULTS_TSV"

overall_pass=0
overall_fail=0
overall_total=0
for fw in "${FRAMEWORKS_ARR[@]}"; do
  p=$(cat "$OUT_DIR/.pass.$fw")
  f=$(cat "$OUT_DIR/.fail.$fw")
  t=$(cat "$OUT_DIR/.total.$fw")
  overall_pass=$((overall_pass + p))
  overall_fail=$((overall_fail + f))
  overall_total=$((overall_total + t))
done

score_pct() {
  local p="$1" t="$2"
  if [[ "$t" -eq 0 ]]; then
    echo "0.0"
    return
  fi
  # one decimal: (p*1000)/t → whole.frac
  local tenths=$(( (p * 1000) / t ))
  echo "$((tenths / 10)).$((tenths % 10))"
}

overall_score="$(score_pct "$overall_pass" "$overall_total")"

# ── SCOREBOARD.md ────────────────────────────────────────────────────
{
  echo "# Cordlang framework scoreboard"
  echo ""
  echo "| | |"
  echo "|--|--|"
  echo "| **Overall score** | **${overall_score} / 100** |"
  echo "| Passed | ${overall_pass} |"
  echo "| Failed | ${overall_fail} |"
  echo "| Total cases | ${overall_total} |"
  echo "| Projects | ${#PROJECT_NAMES[@]} |"
  echo "| Frameworks | ${#FRAMEWORKS_ARR[@]} |"
  echo "| Mode | \`${MODE}\` |"
  echo "| Binary | \`${CORDLANG}\` |"
  echo ""
  echo "Formula: \`score = (passed / total) × 100\` per framework and overall."
  echo ""
  echo "## Score by framework"
  echo ""
  echo "| Framework | Passed | Failed | Total | Score / 100 |"
  echo "|-----------|--------|--------|-------|-------------|"
  for fw in "${FRAMEWORKS_ARR[@]}"; do
    p=$(cat "$OUT_DIR/.pass.$fw")
    f=$(cat "$OUT_DIR/.fail.$fw")
    t=$(cat "$OUT_DIR/.total.$fw")
    s="$(score_pct "$p" "$t")"
    echo "| \`${fw}\` | ${p} | ${f} | ${t} | **${s}** |"
  done
  echo ""
  echo "## Failures (where & how)"
  echo ""
  fail_count=0
  while IFS=$'\t' read -r project fw status step detail; do
    [[ "$project" == "project" ]] && continue
    [[ "$status" == "pass" ]] && continue
    fail_count=$((fail_count + 1))
    echo "### ${fail_count}. \`${project}\` × \`${fw}\`"
    echo ""
    echo "- **Where (step):** \`${step}\`"
    echo "- **How:** ${detail}"
    echo "- **Log:** \`logs/${project}__${fw}.log\`"
    echo ""
  done <"$RESULTS_TSV"
  if [[ "$fail_count" -eq 0 ]]; then
    echo "_No failures — all project × framework cases passed._"
    echo ""
  fi
  echo "## Projects under test"
  echo ""
  for p in "${PROJECT_NAMES[@]}"; do
    echo "- \`${p}\`"
  done
  echo ""
  echo "## How to re-run"
  echo ""
  echo '```bash'
  echo "make"
  echo "./tests/run_framework_score.sh"
  echo "SCORE_MODE=full ./tests/run_framework_score.sh"
  echo "CORDLANG_BIN=./cordlang OUT_DIR=score-out ./tests/run_framework_score.sh"
  echo '```'
  echo ""
} >"$SCOREBOARD"

# ── results.json (minimal, no jq required) ───────────────────────────
{
  echo "{"
  echo "  \"overall_score\": ${overall_score},"
  echo "  \"passed\": ${overall_pass},"
  echo "  \"failed\": ${overall_fail},"
  echo "  \"total\": ${overall_total},"
  echo "  \"mode\": \"${MODE}\","
  echo "  \"frameworks\": {"
  fi=0
  for fw in "${FRAMEWORKS_ARR[@]}"; do
    p=$(cat "$OUT_DIR/.pass.$fw")
    f=$(cat "$OUT_DIR/.fail.$fw")
    t=$(cat "$OUT_DIR/.total.$fw")
    s="$(score_pct "$p" "$t")"
    fi=$((fi + 1))
    comma=","
    [[ $fi -eq ${#FRAMEWORKS_ARR[@]} ]] && comma=""
    echo "    \"${fw}\": { \"passed\": ${p}, \"failed\": ${f}, \"total\": ${t}, \"score\": ${s} }${comma}"
  done
  echo "  },"
  echo "  \"failures\": ["
  first_fail=1
  while IFS=$'\t' read -r project fw status step detail; do
    [[ "$project" == "project" ]] && continue
    [[ "$status" == "pass" ]] && continue
    # JSON-escape detail roughly
    safe_detail="$(printf '%s' "$detail" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    if [[ $first_fail -eq 0 ]]; then
      echo ","
    fi
    first_fail=0
    printf '    {"project":"%s","framework":"%s","step":"%s","detail":"%s"}' \
      "$project" "$fw" "$step" "$safe_detail"
  done <"$RESULTS_TSV"
  echo ""
  echo "  ]"
  echo "}"
} >"$RESULTS_JSON"

# cleanup counter files
rm -f "$OUT_DIR"/.pass.* "$OUT_DIR"/.fail.* "$OUT_DIR"/.total.*

echo ""
echo "========================================"
echo " Overall: ${overall_score} / 100"
echo " ${overall_pass} passed, ${overall_fail} failed, ${overall_total} total"
echo " Report:  $SCOREBOARD"
echo " JSON:    $RESULTS_JSON"
echo "========================================"
echo ""
echo "Per-framework:"
for fw in "${FRAMEWORKS_ARR[@]}"; do
  # re-read from json-ish via TSV recount
  p=0; f=0; t=0
  while IFS=$'\t' read -r project rfw status step detail; do
    [[ "$project" == "project" ]] && continue
    [[ "$rfw" != "$fw" ]] && continue
    t=$((t + 1))
    if [[ "$status" == "pass" ]]; then p=$((p + 1)); else f=$((f + 1)); fi
  done <"$RESULTS_TSV"
  s="$(score_pct "$p" "$t")"
  printf '  %-12s %5s / 100  (%d/%d)\n' "$fw" "$s" "$p" "$t"
done

# Optional gate
if [[ -n "${SCORE_FAIL_UNDER:-}" ]]; then
  under=0
  for fw in "${FRAMEWORKS_ARR[@]}"; do
    p=0; t=0
    while IFS=$'\t' read -r project rfw status step detail; do
      [[ "$project" == "project" ]] && continue
      [[ "$rfw" != "$fw" ]] && continue
      t=$((t + 1))
      [[ "$status" == "pass" ]] && p=$((p + 1))
    done <"$RESULTS_TSV"
    # integer compare on whole percent
    whole=0
    [[ "$t" -gt 0 ]] && whole=$(( (p * 100) / t ))
    if [[ "$whole" -lt "$SCORE_FAIL_UNDER" ]]; then
      echo "GATE: framework $fw score ${whole} < SCORE_FAIL_UNDER=${SCORE_FAIL_UNDER}" >&2
      under=1
    fi
  done
  if [[ "$under" -eq 1 ]]; then
    exit 1
  fi
fi

exit 0
