#!/usr/bin/env bash
# Package browser playground assets for CI artifacts / GitHub Releases.
# Usage: scripts/package_playground.sh [--out dist]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT/dist"
if [[ "${1:-}" == "--out" && -n "${2:-}" ]]; then
  OUT_DIR="$2"
fi

PLAY="$ROOT/playground"
for f in index.html cordlang.js cordlang.wasm README.md; do
  if [[ ! -f "$PLAY/$f" ]]; then
    echo "FAIL: missing playground/$f — run playground/build_wasm.sh first" >&2
    exit 1
  fi
done

mkdir -p "$OUT_DIR"
ZIP="$OUT_DIR/cordlang-playground.zip"
rm -f "$ZIP"
(
  cd "$PLAY"
  zip -q -r "$ZIP" index.html cordlang.js cordlang.wasm README.md
)
ls -la "$ZIP"
echo "OK: $ZIP"
