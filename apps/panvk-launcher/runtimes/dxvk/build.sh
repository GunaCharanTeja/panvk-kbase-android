#!/bin/sh
# Testable local WCP; upstream v3.1.1 + one external-rendering clear fix.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=${DXVK_WORK:-/var/tmp/panvk/dxvk-clear-rebuild}
mkdir -p "$WORK"
if [ ! -d "$WORK/source/.git" ]; then
    git clone --branch v3.1.1 --depth 1 https://github.com/doitsujin/dxvk.git "$WORK/source"
fi
test "$(git -C "$WORK/source" rev-parse HEAD)" = b1a1c99ab52b687cf950d62c88bc2fa316b41663
git -C "$WORK/source" submodule update --init --recursive
git -C "$WORK/source" apply --check "$ROOT/clear-before-external-rendering.patch"
git -C "$WORK/source" apply "$ROOT/clear-before-external-rendering.patch"
for arch in arm64ec i686; do
    meson setup "$WORK/$arch" "$WORK/source" --cross-file "$ROOT/$arch.txt" --buildtype release -Db_ndebug=true
    ninja -C "$WORK/$arch" -j"${JOBS:-8}"
    case $arch in arm64ec) dest=system32;; i686) dest=syswow64;; esac
    mkdir -p "$WORK/package/$dest"
    for pair in d3d8/d3d8 d3d9/d3d9 d3d10/d3d10core d3d11/d3d11 dxgi/dxgi; do
        cp "$WORK/$arch/src/$pair.dll" "$WORK/package/$dest/"
    done
done
cp "$ROOT/profile.json" "$WORK/package/profile.json"
tar -C "$WORK/package" -cJf "$WORK/dxvk-3.1.1-clearfix.wcp" .
sha256sum "$WORK/dxvk-3.1.1-clearfix.wcp"
