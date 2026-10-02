#!/bin/sh
# Build windowed d3d8/9/10/11 smoke PEs from dxsmoke.c.
# One binary per API per arch; every d3d entry point is resolved at
# runtime, so this links the CRT only.
#
# Proton 11.0-2-arm64ec ships system32 ARM64EC (0xA641) DXVK and
# can execute ARM64EC PEs (arm64ec-w64-mingw32).
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
MINGW=${MINGW:-/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64}
OUT=$ROOT/build
SRC=$ROOT/dxsmoke.c

mkdir -p "$OUT"
rm -f "$OUT"/dxsmoke-d3d*-*.exe
for triple in aarch64-w64-mingw32 arm64ec-w64-mingw32 i686-w64-mingw32 x86_64-w64-mingw32; do
    "$MINGW/bin/$triple-clang" -O2 -Wall -Wextra -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP \
        "$SRC" "$ROOT/dxsmoke_d3d8.c" -o "$OUT/dxsmoke-$triple.exe" -luser32 -lgdi32
    for api in d3d8 d3d9 d3d10 d3d11; do
        cp "$OUT/dxsmoke-$triple.exe" "$OUT/dxsmoke-$api-$triple.exe"
    done
done

echo "built:"
ls -l "$OUT"/dxsmoke-*.exe
