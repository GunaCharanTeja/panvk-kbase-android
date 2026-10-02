#!/bin/sh
# prepare-x11-headers.sh — header-only X11/XCB pkg-config prefix for the Android X11 WSI.
# The Android ICD dlopens libxcb & co. at runtime (patch csf-v11/083), so the build
# needs headers and pkg-config versions only, never target libraries.
# Usage: ./scripts/prepare-x11-headers.sh [OUT]   (default work/android-deps-x11)
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/work/android-deps-x11}"
INC="${X11_INCLUDE:-/usr/include}"
[ -f "$INC/xcb/xcb.h" ] && [ -f "$INC/X11/Xlib-xcb.h" ] && [ -f "$INC/X11/xshmfence.h" ] || {
  echo "need host libxcb, libx11 and libxshmfence headers under $INC" >&2; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT/include" "$OUT/lib/pkgconfig"
cp -r "$INC/xcb" "$INC/X11" "$OUT/include/"
ver() { pkg-config --modversion "$1" 2>/dev/null || echo "$2"; }
XCB="$(ver xcb 1.17.0)"
for pc in xcb xcb-randr xcb-dri3 xcb-present xcb-shm xcb-sync xcb-xfixes x11-xcb xshmfence; do
  case $pc in x11-xcb) v="$(ver x11-xcb 1.8.12)";; xshmfence) v="$(ver xshmfence 1.3.3)";; *) v="$XCB";; esac
  printf 'prefix=%s\nincludedir=${prefix}/include\nName: %s\nDescription: headers only (runtime dlopen)\nVersion: %s\nCflags: -I${includedir}\nLibs:\n' \
    "$OUT" "$pc" "$v" > "$OUT/lib/pkgconfig/$pc.pc"
done
echo "OK x11 headers=$OUT xcb=$XCB"
