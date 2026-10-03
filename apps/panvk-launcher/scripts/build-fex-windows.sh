#!/bin/sh
# Build FEX ARM64EC + WOW64 Windows-side DLLs (libarm64ecfex.dll, libwow64fex.dll) from a pinned
# upstream tag with pinned llvm-mingw, and package them as a FEXCore WCP (tar.zst) laid out like the
# WCP Hub FEXCore-2609 component (wine/aarch64-windows/*.dll -> ${system32}).
# Mirrors FEX's own .github/workflows/wine_build/action.yml configure flags.
# Output: $OUT/fexcore-<ver>.wcp (+ .sha256). Big files live in /var/tmp/panvk, never in git or /tmp.
set -eu
. "$(dirname -- "$0")/components.env"

WORK=${WORK:-/var/tmp/panvk/components-build}
OUT=${OUT:-/var/tmp/panvk/components}
JOBS=${JOBS:-8}
mkdir -p "$WORK" "$OUT"
. "$(dirname -- "$0")/llvm-mingw.sh"

CMAKE=${CMAKE:-$(command -v cmake || ls /opt/android-sdk/cmake/*/bin/cmake 2>/dev/null | tail -1)}
[ -x "$CMAKE" ] || { echo "cmake >= 3.14 required (set CMAKE=)" >&2; exit 1; }

SRC=$WORK/fex-src
if [ ! -d "$SRC/.git" ]; then
    git clone --depth 1 --branch "$FEX_TAG" https://github.com/FEX-Emu/FEX.git "$SRC"
fi
test "$(git -C "$SRC" rev-parse HEAD)" = "$FEX_COMMIT" || { echo "FEX commit mismatch" >&2; exit 1; }
# Test-binary and Catch2 submodules are not needed for the DLLs.
subs=$(git -C "$SRC" config -f .gitmodules --get-regexp 'submodule\..*\.path' | awk '{print $2}' |
    grep -v -e '^External/fex-.*-bins$' -e '^External/Catch2$')
git -C "$SRC" submodule update --init --depth 1 $subs

PKG=$WORK/fex-package
rm -rf "$PKG"
for target in arm64ec wow64; do
    case $target in wow64) cc=aarch64;; arm64ec) cc=arm64ec;; esac
    B=$WORK/fex-build-$target
    rm -rf "$B" "$WORK/fex-install-$target"
    "$CMAKE" -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$SRC/Data/CMake/toolchain_mingw.cmake" -DMINGW_TRIPLE=$cc-w64-mingw32 \
        -DCMAKE_INSTALL_LIBDIR=lib/wine/aarch64-windows -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_LTO=False -DENABLE_ASSERTIONS=False -DENABLE_JEMALLOC_GLIBC_ALLOC=False \
        -DBUILD_TESTING=False -DTUNE_ARCH=generic -DTUNE_CPU=none -DRANGES_NATIVE=OFF
    "$CMAKE" --build "$B" -j"$JOBS"
    DESTDIR="$WORK/fex-install-$target" "$CMAKE" --build "$B" -t install
    mkdir -p "$PKG/wine/aarch64-windows"
    cp "$WORK/fex-install-$target/usr/lib/wine/aarch64-windows/lib${target}fex.dll" "$PKG/wine/aarch64-windows/"
done

cat > "$PKG/profile.json" <<EOF
{
  "type": "FEXCore",
  "versionName": "$FEX_VERSION",
  "versionCode": 1,
  "description": "FEX $FEX_TAG ($FEX_COMMIT) built from source with llvm-mingw $LLVM_MINGW_VER (scripts/build-fex-windows.sh)",
  "files": [
    { "source": "wine/aarch64-windows/libarm64ecfex.dll", "target": "\${system32}/libarm64ecfex.dll" },
    { "source": "wine/aarch64-windows/libwow64fex.dll", "target": "\${system32}/libwow64fex.dll" }
  ]
}
EOF

wcp=$OUT/fexcore-$FEX_VERSION.wcp
tar -C "$PKG" --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf - . | zstd -19 -T0 -q -f -o "$wcp"
sha256sum "$wcp" | tee "$wcp.sha256"
