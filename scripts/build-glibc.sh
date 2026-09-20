#!/bin/sh
# build-glibc.sh — ARM64 glibc PanVK ICD (Bachata S4 / NativeCode AI)
# Usage: ./scripts/build-glibc.sh --profile g615-v11-csf [options]
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROFILE=""
CLEAN=0
JOBS=2
SERIAL="${ANDROID_SERIAL:-}"
DEVICE_ROOT="/tmp/panvk-glibc"
CHROOT_WRAPPER="/data/local/tmp/nativecode_chroot.sh"
while [ $# -gt 0 ]; do
  case "$1" in
    --profile) PROFILE="$2"; shift 2;;
    --clean) CLEAN=1; shift 1;;
    --jobs) JOBS="$2"; shift 2;;
    --serial) SERIAL="$2"; shift 2;;
    --device-root) DEVICE_ROOT="$2"; shift 2;;
    --chroot-wrapper) CHROOT_WRAPPER="$2"; shift 2;;
    *) echo "unknown $1" >&2; exit 2;;
  esac
done
[ -n "$PROFILE" ] || { echo "--profile required" >&2; exit 2; }
case "$JOBS" in 1|2) ;; *) echo "--jobs must be 1 or 2" >&2; exit 2;; esac
MESA_PIN="$(python3 -c "import json;print(json.load(open('$ROOT/sources.lock'))['mesaCommit'])")"
PATCH_ID="$(python3 "$ROOT/scripts/compute-patch-series-id.py" --profile "$PROFILE" --bare)"
SOURCE_ID="${MESA_PIN}-${PATCH_ID}"
SOURCE_ROOT="$ROOT/build/reconstructed-$SOURCE_ID"
MESA="$SOURCE_ROOT/mesa"
rm -rf "$SOURCE_ROOT"
"$ROOT/scripts/fetch-mesa.sh" "$SOURCE_ROOT"
"$ROOT/scripts/apply-patches.sh" --profile "$PROFILE" --mesa "$MESA"
[ "$(git -C "$MESA" rev-parse HEAD)" = "$MESA_PIN" ] || {
  echo "BUILD-FAIL: Mesa HEAD does not match sources.lock" >&2; exit 1;
}

DDIR="$ROOT/dist/glibc-$PROFILE"
mkdir -p "$DDIR"

if [ "$(uname -m)" = "aarch64" ]; then
  BDIR="$ROOT/build/linux-glibc-$SOURCE_ID"
  if [ "$CLEAN" = "1" ] || [ ! -f "$BDIR/build.ninja" ]; then
    rm -rf "$BDIR"
    mkdir -p "$BDIR"
    meson setup "$BDIR" "$MESA" \
      --native-file "$ROOT/meson/linux-aarch64-native.ini" \
      -Dbuildtype=release \
      -Dplatforms=x11,wayland \
      -Dgallium-drivers= \
      -Dvulkan-drivers=panfrost \
      -Dpanfrost-kmds=kbase \
      -Degl=disabled -Dgles1=disabled -Dgles2=disabled -Dopengl=false \
      -Dglx=disabled -Dgbm=disabled -Dzstd=disabled -Dlibunwind=disabled
  fi
  ninja -j"$JOBS" -C "$BDIR"
  SO="$(find "$BDIR" -name libvulkan_panfrost.so | head -n1)"
  [ -n "$SO" ] || { echo "BUILD-FAIL: ICD missing from $BDIR" >&2; exit 1; }
  cp "$SO" "$DDIR/libvulkan_panfrost.so"
  ICD="$(find "$BDIR" -name 'panfrost_icd.*.json' | head -n1)"
  [ -n "$ICD" ] || { echo "BUILD-FAIL: ICD manifest missing from $BDIR" >&2; exit 1; }
  cp "$ICD" "$DDIR/panfrost_icd.aarch64.json"
else
  [ -n "$SERIAL" ] || { echo "--serial required for device build" >&2; exit 2; }
  SRC="$DEVICE_ROOT/$SOURCE_ID/mesa"
  BDIR="$DEVICE_ROOT/$SOURCE_ID/build"
  ARCHIVE="$ROOT/build/mesa-$SOURCE_ID.tar.gz"
  DEVICE_ARCHIVE="/data/local/tmp/chrootAlpine$DEVICE_ROOT/$(basename "$ARCHIVE")"
  mkdir -p "$ROOT/build"
  tar --exclude=.git --sort=name --mtime='UTC 1970-01-01' --owner=0 --group=0 --numeric-owner \
    -C "$MESA" -czf "$ARCHIVE" .
  ARCHIVE_SHA="$(sha256sum "$ARCHIVE" | cut -d' ' -f1)"
  echo "Host is $(uname -m), dispatching isolated glibc build on device chroot via ADB..."
  adb -s "$SERIAL" shell "mkdir -p /data/local/tmp/chrootAlpine$DEVICE_ROOT"
  adb -s "$SERIAL" push --sync "$ARCHIVE" "$DEVICE_ARCHIVE"
  adb -s "$SERIAL" shell "$CHROOT_WRAPPER sh --user root -- 'set -eu; test \"\$(sha256sum \"$DEVICE_ROOT/$(basename "$ARCHIVE")\" | cut -d\" \" -f1)\" = \"$ARCHIVE_SHA\"; rm -rf \"$SRC\"; mkdir -p \"$SRC\"; tar -xzf \"$DEVICE_ROOT/$(basename "$ARCHIVE")\" -C \"$SRC\"'"
  if [ "$CLEAN" = "1" ]; then
    adb -s "$SERIAL" shell "$CHROOT_WRAPPER sh --user root -- 'rm -rf \"$BDIR\"'"
  fi
  echo "Configuring and building in chroot..."
  adb -s "$SERIAL" shell "$CHROOT_WRAPPER sh --user root -- '
    if [ ! -f \"$BDIR/build.ninja\" ]; then
      meson setup \"$BDIR\" \"$SRC\" \
        -Dbuildtype=release \
        -Dplatforms=x11,wayland \
        -Dgallium-drivers= \
        -Dvulkan-drivers=panfrost \
        -Dpanfrost-kmds=kbase \
        -Degl=disabled -Dgles1=disabled -Dgles2=disabled -Dopengl=false \
        -Dglx=disabled -Dgbm=disabled -Dzstd=disabled -Dlibunwind=disabled
    fi
    ninja -j$JOBS -C \"$BDIR\"
  '"
  echo "Pulling built glibc ICD..."
  adb -s "$SERIAL" pull "/data/local/tmp/chrootAlpine$BDIR/src/panfrost/vulkan/libvulkan_panfrost.so" "$DDIR/libvulkan_panfrost.so"
  adb -s "$SERIAL" pull "/data/local/tmp/chrootAlpine$BDIR/src/panfrost/vulkan/panfrost_icd.aarch64.json" "$DDIR/panfrost_icd.aarch64.json"
fi

SO="$DDIR/libvulkan_panfrost.so"
[ -f "$SO" ] || { echo "BUILD-FAIL: $SO not found" >&2; exit 1; }
[ -s "$DDIR/panfrost_icd.aarch64.json" ] || { echo "BUILD-FAIL: ICD manifest missing" >&2; exit 1; }
file "$SO" | grep -q 'ELF 64-bit.*ARM aarch64' || {
  echo "BUILD-FAIL: ICD is not AArch64 ELF" >&2; exit 1;
}
readelf --version-info "$SO" | grep -q 'GLIBC_' || {
  echo "BUILD-FAIL: ICD is not glibc-linked" >&2; exit 1;
}

# Static verification of kbase symbols
if readelf -s "$SO" 2>/dev/null | grep -q "kbase_kmod_ops"; then
  echo "VERIFIED: kbase_kmod_ops present in $SO"
elif strings "$SO" | grep -q "kbase_kmod"; then
  echo "VERIFIED: kbase_kmod present in $SO"
else
  echo "FAIL: kbase symbols missing in $SO" >&2
  exit 1
fi

if readelf -s "$SO" 2>/dev/null | grep -E "(panfrost_kmod|panthor_kmod)"; then
  echo "FAIL: unexpected panfrost/panthor symbols found in $SO" >&2
  exit 1
fi

echo "OK glibc build complete: $SO"
