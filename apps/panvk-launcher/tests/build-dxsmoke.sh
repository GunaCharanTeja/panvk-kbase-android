#!/bin/sh
# Build windowed d3d8/9/10/11 smoke PEs from dxsmoke.c.
# One binary per API per arch; every d3d entry point is resolved at
# runtime, so this links the CRT only.
#
# Proton 11.0-2-arm64ec ships aarch64-windows (PE machine 0xAA64, not
# ARM64EC) and i386-windows, so those two triples are the ones the
# installed runtime can load. x86_64 is built too, matching the existing
# d3d11probe-x86_64.exe, and is a no-op on that runtime.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
MINGW=${MINGW:-/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64}
OUT=$ROOT/build
SRC=$ROOT/dxsmoke.c

mkdir -p "$OUT"
rm -f "$OUT"/dxsmoke-d3d*-*.exe
for triple in aarch64-w64-mingw32 i686-w64-mingw32 x86_64-w64-mingw32; do
    "$MINGW/bin/$triple-clang" -O2 -Wall -Wextra -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP \
        "$SRC" "$ROOT/dxsmoke_d3d8.c" -o "$OUT/dxsmoke-$triple.exe" -luser32 -lgdi32
done

echo "built:"
ls -l "$OUT"/dxsmoke-*.exe
