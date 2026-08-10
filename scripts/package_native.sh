#!/usr/bin/env bash
# Build native install packages from a compiled cordlang binary.
#
# Usage:
#   scripts/package_native.sh \
#     --binary ./cordlang \
#     --version 0.0.013-alpha.1 \
#     --os linux|macos \
#     --arch x64|arm64 \
#     --out dist
#
# Outputs (into --out):
#   Linux:  .zip .tar.gz .deb .rpm
#   macOS:  .zip .dmg

set -euo pipefail

BINARY=""
VERSION=""
OS_NAME=""
ARCH=""
OUT_DIR="dist"

die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary) BINARY="${2:-}"; shift 2 ;;
    --version) VERSION="${2:-}"; shift 2 ;;
    --os) OS_NAME="${2:-}"; shift 2 ;;
    --arch) ARCH="${2:-}"; shift 2 ;;
    --out) OUT_DIR="${2:-}"; shift 2 ;;
    -h|--help)
      sed -n '2,16p' "$0"
      exit 0
      ;;
    *) die "unknown arg: $1" ;;
  esac
done

[[ -n "$BINARY" && -f "$BINARY" ]] || die "--binary required and must exist"
[[ -n "$VERSION" ]] || die "--version required"
[[ "$OS_NAME" == "linux" || "$OS_NAME" == "macos" ]] || die "--os must be linux|macos"
[[ "$ARCH" == "x64" || "$ARCH" == "arm64" ]] || die "--arch must be x64|arm64"

VERSION="${VERSION#v}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# Debian/RPM-safe upstream version: 0.0.013-alpha.1 → 0.0.013~alpha.1
pkg_version() {
  local v="$1"
  v="${v//-alpha./~alpha.}"
  v="${v//-beta./~beta.}"
  v="${v//-rc./~rc.}"
  v="${v//-pre./~pre.}"
  v="${v//-dev./~dev.}"
  # Remaining hyphens would confuse dpkg (debian_revision); squash them.
  if [[ "$v" == *-* ]]; then
    v="${v//-/.}"
  fi
  printf '%s' "$v"
}

PKG_VER="$(pkg_version "$VERSION")"
ART="cordlang-${OS_NAME}-${ARCH}"
STAGE="$OUT_DIR/_stage_${ART}"
rm -rf "$STAGE"
mkdir -p "$STAGE/$ART"

cp "$BINARY" "$STAGE/$ART/cordlang"
chmod 755 "$STAGE/$ART/cordlang"
[[ -f "$ROOT/LICENSE" ]] && cp "$ROOT/LICENSE" "$STAGE/$ART/"
[[ -f "$ROOT/README.md" ]] && cp "$ROOT/README.md" "$STAGE/$ART/"

cat > "$STAGE/$ART/INSTALL.txt" <<EOF
Cordlang CLI ${VERSION}
Artifact: ${ART}
Arch: ${ARCH}
Binary: cordlang

chmod +x cordlang
./cordlang --version

Optional: sudo mv cordlang /usr/local/bin/cordlang
Docs: https://github.com/cordlangorg/cordlang
EOF

# ── zip (portable) ─────────────────────────────────────────
rm -f "$OUT_DIR/${ART}.zip"
(
  cd "$STAGE"
  if command -v zip >/dev/null 2>&1; then
    zip -r "$OUT_DIR/${ART}.zip" "$ART"
  else
    python3 - <<PY
import shutil
shutil.make_archive(r"$OUT_DIR/${ART}", "zip", r"$STAGE", "$ART")
PY
  fi
)
echo "OK: $OUT_DIR/${ART}.zip"

# ── tar.gz (Linux/macOS portable) ──────────────────────────
rm -f "$OUT_DIR/${ART}.tar.gz"
tar -C "$STAGE" -czf "$OUT_DIR/${ART}.tar.gz" "$ART"
echo "OK: $OUT_DIR/${ART}.tar.gz"

if [[ "$OS_NAME" == "linux" ]]; then
  case "$ARCH" in
    x64) DEB_ARCH=amd64; RPM_ARCH=x86_64 ;;
    arm64) DEB_ARCH=arm64; RPM_ARCH=aarch64 ;;
  esac

  # ── .deb ─────────────────────────────────────────────────
  if command -v dpkg-deb >/dev/null 2>&1; then
    DEB_ROOT="$STAGE/deb"
    rm -rf "$DEB_ROOT"
    mkdir -p "$DEB_ROOT/DEBIAN" "$DEB_ROOT/usr/local/bin" "$DEB_ROOT/usr/share/doc/cordlang"
    cp "$BINARY" "$DEB_ROOT/usr/local/bin/cordlang"
    chmod 755 "$DEB_ROOT/usr/local/bin/cordlang"
    [[ -f "$ROOT/LICENSE" ]] && cp "$ROOT/LICENSE" "$DEB_ROOT/usr/share/doc/cordlang/copyright"
    [[ -f "$ROOT/README.md" ]] && cp "$ROOT/README.md" "$DEB_ROOT/usr/share/doc/cordlang/"
    cat > "$DEB_ROOT/DEBIAN/control" <<EOF
Package: cordlang
Version: ${PKG_VER}
Section: devel
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Cordlang <noreply@cordlang.org>
Homepage: https://github.com/cordlangorg/cordlang
Description: Cordlang UI compiler CLI
 Dense .cord UI sources compiled to React, Svelte, Vue, or native ESM preview.
EOF
    DEB_NAME="cordlang_${PKG_VER}_${DEB_ARCH}.deb"
    rm -f "$OUT_DIR/$DEB_NAME"
    dpkg-deb --build --root-owner-group "$DEB_ROOT" "$OUT_DIR/$DEB_NAME"
    echo "OK: $OUT_DIR/$DEB_NAME"
  else
    echo "warn: dpkg-deb not found — skipping .deb" >&2
  fi

  # ── .rpm (via fpm when available) ────────────────────────
  if command -v fpm >/dev/null 2>&1; then
    RPM_DIR="$STAGE/rpm"
    rm -rf "$RPM_DIR"
    mkdir -p "$RPM_DIR/usr/local/bin"
    cp "$BINARY" "$RPM_DIR/usr/local/bin/cordlang"
    chmod 755 "$RPM_DIR/usr/local/bin/cordlang"
    # RPM Version: only [A-Za-z0-9._+] — map prerelease separators to dots
    RPM_VER="$(printf '%s' "$VERSION" | sed 's/[^A-Za-z0-9._+]/\./g; s/\.\+/./g; s/^\.//; s/\.$//')"
    fpm -s dir -t rpm \
      -n cordlang \
      -v "$RPM_VER" \
      -a "$RPM_ARCH" \
      --license MIT \
      --url "https://github.com/cordlangorg/cordlang" \
      --description "Cordlang UI compiler CLI" \
      -C "$RPM_DIR" \
      -p "$OUT_DIR" \
      usr/local/bin/cordlang
    echo "OK: rpm in $OUT_DIR"
  else
    echo "warn: fpm not found — skipping .rpm" >&2
  fi
fi

if [[ "$OS_NAME" == "macos" ]]; then
  # ── .dmg ─────────────────────────────────────────────────
  if command -v hdiutil >/dev/null 2>&1; then
    DMG_STAGE="$STAGE/dmg"
    rm -rf "$DMG_STAGE"
    mkdir -p "$DMG_STAGE"
    cp "$BINARY" "$DMG_STAGE/cordlang"
    chmod 755 "$DMG_STAGE/cordlang"
    [[ -f "$ROOT/LICENSE" ]] && cp "$ROOT/LICENSE" "$DMG_STAGE/"
    [[ -f "$ROOT/README.md" ]] && cp "$ROOT/README.md" "$DMG_STAGE/"
    cat > "$DMG_STAGE/INSTALL.txt" <<EOF
Cordlang CLI ${VERSION}

1. Drag cordlang somewhere on your PATH, e.g.:
   sudo mkdir -p /usr/local/bin
   sudo cp cordlang /usr/local/bin/cordlang
2. cordlang --version
EOF
    DMG_NAME="${ART}.dmg"
    rm -f "$OUT_DIR/$DMG_NAME"
    hdiutil create \
      -volname "Cordlang ${VERSION}" \
      -srcfolder "$DMG_STAGE" \
      -ov \
      -format UDZO \
      "$OUT_DIR/$DMG_NAME"
    echo "OK: $OUT_DIR/$DMG_NAME"
  else
    echo "warn: hdiutil not found — skipping .dmg" >&2
  fi
fi

rm -rf "$STAGE"
echo "Packaging complete for ${ART}"
