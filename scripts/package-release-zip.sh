#!/bin/sh
set -eu

# package-release-zip.sh — Package PanVK Mali-G615 driver for AdrenoTools / Winlator or Termux X11
# Usage: ./scripts/package-release-zip.sh --flavor [adrenotools|x11] [--so <path>] [--version <ver>]

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FLAVOR=""
SO_PATH=""
VERSION="0.1.0-beta.1"
OUT_DIR="$ROOT"

while [ $# -gt 0 ]; do
  case "$1" in
    --flavor)
      FLAVOR="$2"
      shift 2
      ;;
    --so)
      SO_PATH="$2"
      shift 2
      ;;
    --version)
      VERSION="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

if [ -z "$FLAVOR" ]; then
  echo "Error: --flavor is required (adrenotools or x11)" >&2
  exit 1
fi

if [ -z "$SO_PATH" ]; then
  SO_PATH="$ROOT/build/src/panfrost/vulkan/libvulkan_panfrost.so"
fi

if [ ! -f "$SO_PATH" ]; then
  echo "Error: Driver binary not found at $SO_PATH" >&2
  exit 1
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

echo "==> Packaging PanVK Mali-G615 [$FLAVOR] version $VERSION"
echo "==> Driver source: $SO_PATH"

cp "$SO_PATH" "$STAGE/libvulkan_panfrost.so"
SHA256=$(sha256sum "$STAGE/libvulkan_panfrost.so" | awk '{print $1}')
echo "==> Driver SHA256: $SHA256"

# AdrenoTools / Winlator meta.json
python3 - <<EOF
import json

flavor = "$FLAVOR"
sha = "$SHA256"
ver = "$VERSION"

if flavor == "adrenotools":
    name = "PanVK G615"
    desc = "PanVK Mali-G615/Kbase-CSF driver with Phase 1-7 support (D3D12 FL 11_1, Geometry & Tessellation Shaders, Transform Feedback, EDS 1-3, Descriptor Buffer, DGC, 187 Extensions) for Winlator and AdrenoTools."
else:
    name = "PanVK G615 (X11)"
    desc = "PanVK Mali-G615/Kbase-CSF driver with Phase 1-7 support (D3D12 FL 11_1, Geometry & Tessellation Shaders, Transform Feedback, EDS 1-3, Descriptor Buffer, DGC, 187 Extensions) for Termux X11."

meta = {
    "schemaVersion": 1,
    "name": name,
    "description": desc,
    "author": "GunaCharanTeja",
    "packageVersion": ver,
    "vendor": "Mesa",
    "driverVersion": f"Mesa 26.3.0-devel / PanVK G615 {ver}",
    "minApi": 30,
    "libraryName": "libvulkan_panfrost.so",
    "sanitizedArtifactSha256": sha
}

with open("$STAGE/meta.json", "w") as f:
    json.dump(meta, f, indent=2)
EOF

ZIP_NAME="PanVK-G615-$VERSION-$FLAVOR.zip"
mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/$ZIP_NAME"

if [ "$FLAVOR" = "adrenotools" ]; then
  (cd "$STAGE" && zip -r "$OUT_DIR/$ZIP_NAME" libvulkan_panfrost.so meta.json)
else
  mkdir -p "$STAGE/shims"
  cp -P "$ROOT"/shims/*.so* "$STAGE/shims/"
  (cd "$STAGE" && zip -r "$OUT_DIR/$ZIP_NAME" libvulkan_panfrost.so shims meta.json)
fi

ZIP_SHA256=$(sha256sum "$OUT_DIR/$ZIP_NAME" | awk '{print $1}')

echo "$SHA256  libvulkan_panfrost.so" > "$OUT_DIR/SHA256SUMS.txt"
echo "$ZIP_SHA256  $ZIP_NAME" >> "$OUT_DIR/SHA256SUMS.txt"

cat <<EOF > "$OUT_DIR/PanVK-G615-$VERSION-$FLAVOR-MANIFEST.txt"
# PanVK Mali-G615 Driver Manifest
Flavor: $FLAVOR
Package Version: $VERSION
Library: libvulkan_panfrost.so
Driver SHA256: $SHA256
Archive SHA256: $ZIP_SHA256
EOF

echo "==> Successfully created $OUT_DIR/$ZIP_NAME"
echo "==> Package contents:"
unzip -l "$OUT_DIR/$ZIP_NAME"
