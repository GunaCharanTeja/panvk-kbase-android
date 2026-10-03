#!/bin/sh
# Build DXVK from a pinned upstream tag + runtimes/dxvk/clear-before-external-rendering.patch with pinned
# llvm-mingw: ARM64EC -> system32 (loaded by x64 and ARM64EC apps under Proton arm64ec), i686 -> syswow64.
# Same layout as the WCP Hub dxvk-arm64ec-3.1.1 component. Output: $OUT/dxvk-<ver>.wcp (+ .sha256).
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/components.env"
RT=$HERE/../runtimes/dxvk

WORK=${WORK:-/var/tmp/panvk/components-build}
OUT=${OUT:-/var/tmp/panvk/components}
JOBS=${JOBS:-8}
mkdir -p "$WORK" "$OUT"
. "$HERE/llvm-mingw.sh"

SRC=$WORK/dxvk-src
if [ ! -d "$SRC/.git" ]; then
    git clone --depth 1 --branch "$DXVK_TAG" https://github.com/doitsujin/dxvk.git "$SRC"
fi
test "$(git -C "$SRC" rev-parse HEAD)" = "$DXVK_COMMIT" || { echo "DXVK commit mismatch" >&2; exit 1; }
git -C "$SRC" submodule update --init --recursive --depth 1
git -C "$SRC" checkout -- .
git -C "$SRC" apply "$RT/clear-before-external-rendering.patch"

PKG=$WORK/dxvk-package
rm -rf "$PKG"
files=
for arch in arm64ec i686; do
    B=$WORK/dxvk-build-$arch
    rm -rf "$B"
    meson setup "$B" "$SRC" --cross-file "$RT/$arch.txt" --buildtype release -Db_ndebug=true
    ninja -C "$B" -j"$JOBS"
    case $arch in arm64ec) dest=system32;; i686) dest=syswow64;; esac
    mkdir -p "$PKG/$dest"
    for pair in d3d8/d3d8 d3d9/d3d9 d3d10/d3d10core d3d11/d3d11 dxgi/dxgi; do
        cp "$B/src/$pair.dll" "$PKG/$dest/"
        dll=${pair#*/}.dll
        files="$files    { \"source\": \"$dest/$dll\", \"target\": \"\${$dest}/$dll\" },
"
    done
done
files=$(printf '%s' "$files" | sed '$ s/,$//')

cat > "$PKG/profile.json" <<EOF
{
  "type": "DXVK",
  "versionName": "$DXVK_VERSION",
  "versionCode": 1,
  "description": "DXVK $DXVK_TAG ($DXVK_COMMIT) + clear-before-external-rendering fix, built from source with llvm-mingw $LLVM_MINGW_VER (scripts/build-dxvk.sh). ARM64EC system32 + i686 syswow64.",
  "files": [
$files
  ]
}
EOF

wcp=$OUT/dxvk-$DXVK_VERSION.wcp
tar -C "$PKG" --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf - . | zstd -19 -T0 -q -f -o "$wcp"
sha256sum "$wcp" | tee "$wcp.sha256"
