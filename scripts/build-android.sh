#!/bin/sh
# build-android.sh — Android/Bionic arm64-v8a PanVK (hot-loadable ICD + Android/X11 WSI + AHB + Kbase)
# Usage: ./scripts/build-android.sh --profile g615-v11-csf [--api 35] [--ndk $ANDROID_NDK_ROOT]
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROFILE=""; API="${ANDROID_API:-35}"; NDK="${ANDROID_NDK_ROOT:-${ANDROID_HOME:-/opt/android-sdk}/ndk}"
while [ $# -gt 0 ]; do case "$1" in
  --profile) PROFILE="$2"; shift 2;; --api) API="$2"; shift 2;; --ndk) NDK="$2"; shift 2;; *) echo "unknown $1" >&2; exit 2;; esac; done
[ -n "$PROFILE" ] || { echo "--profile required" >&2; exit 2; }
NDK_BIN="$(ls -d "$NDK"/*/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | sort -V | tail -n1)"
if [ -z "$NDK_BIN" ]; then NDK_BIN="$(ls -d "$NDK"/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | head -n1)"; fi
[ -n "$NDK_BIN" ] || { echo "NDK toolchain not found under $NDK" >&2; exit 1; }
MESA="${MESA:-$ROOT/work/mesa}"; BDIR="${BDIR:-$ROOT/build/android-bionic}"; DDIR="${DDIR:-$ROOT/dist/android-$PROFILE}"
if [ -z "${HOST_TOOLS:-}" ]; then
  n="$(basename "$MESA")"
  HT="$ROOT/build/host-tools${n#mesa}"
  if [ -f "$HT/build.ninja" ] || [ ! -x "$HT/bin/mesa_clc" ]; then
    "$ROOT/scripts/bootstrap-host-tools.sh" --mesa "$MESA" --out "$HT" >&2
  fi
  HOST_TOOLS="$HT/bin"
fi
export PATH="$HOST_TOOLS:$NDK_BIN:$PATH"
CC_TRIPLE="aarch64-linux-android$API-clang"
mkdir -p "$BDIR"
sed "s/aarch64-linux-android[0-9]*-clang/$CC_TRIPLE/g" "$ROOT/meson/android-aarch64.ini" > "$BDIR.cross.ini"
# Pin target pkg-config to our NDK-built deps prefix so host /usr/lib .pc
# files (zlib, libudev, ...) can never leak host paths into the link.
DEPS_PCDIR="$ROOT/work/android-deps/lib/pkgconfig"
# X11 WSI: headers + pkg-config only, libxcb & co. are dlopened at runtime (csf-v11/083).
X11_PCDIR="$ROOT/work/android-deps-x11/lib/pkgconfig"
[ -f "$X11_PCDIR/xcb.pc" ] || "$ROOT/scripts/prepare-x11-headers.sh" "$ROOT/work/android-deps-x11" >&2
if [ -d "$DEPS_PCDIR" ]; then
  python3 - "$BDIR.cross.ini" "$DEPS_PCDIR" "$X11_PCDIR" <<'EOF'
import sys
p, d, x = sys.argv[1], sys.argv[2], sys.argv[3]
t = open(p).read()
line = f"pkg_config_libdir = ['{d}', '{x}']\n"
assert '[properties]' in t
t = t.replace('[properties]', '[properties]\n' + line, 1)
open(p, 'w').write(t)
EOF
fi
export PKG_CONFIG_PATH="$DEPS_PCDIR:${PKG_CONFIG_PATH:-}"
mkdir -p "$DDIR"
# Host codegen tools (mesa_clc, vtn_bindgen2, panfrost_compile) must come from a native build:
# default is derived from MESA (see bootstrap-host-tools.sh) and HOST_TOOLS overrides it.
LOG="$BDIR.log"; RECONF=""; [ -f "$BDIR/build.ninja" ] && RECONF="--reconfigure"
meson setup $RECONF "$BDIR" "$MESA" --cross-file "$BDIR.cross.ini" \
  -Dbuildtype=release -Dplatforms=android,x11 -Dandroid-stub=true -Dandroid-strict=false \
  -Dgallium-drivers= -Dvulkan-drivers=panfrost -Dpanfrost-kmds=kbase \
  -Dmesa-clc=system -Dprecomp-compiler=system -Dxlib-lease=disabled \
  -Degl=disabled -Dgles1=disabled -Dgles2=disabled -Dopengl=false \
  -Dglx=disabled -Dgbm=disabled -Dlibunwind=disabled -Dzstd=disabled \
  -Dcpp_link_args=-static-libstdc++ >"$LOG" 2>&1 || { tail -n 20 "$LOG"; echo "BUILD-FAIL: meson setup (log $LOG)" >&2; exit 1; }
ninja -j"$(nproc)" -C "$BDIR" >>"$LOG" 2>&1 || { tail -n 20 "$LOG"; echo "BUILD-FAIL: ninja (log $LOG)" >&2; exit 1; }
SO="$(find "$BDIR" -name libvulkan_panfrost.so | head -n1)"
[ -n "$SO" ] || { echo "BUILD-FAIL: libvulkan_panfrost.so not produced" >&2; exit 1; }
cp "$SO" "$DDIR/"
echo "OK profile=$PROFILE so=$DDIR/libvulkan_panfrost.so"
