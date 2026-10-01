#!/bin/sh
# bootstrap-host-tools.sh — build pinned host codegen tools (mesa_clc, vtn_bindgen2, ...)
# Inspects the Mesa revision instead of hard-coding an old tool list.
# Usage: ./scripts/bootstrap-host-tools.sh [--mesa work/mesa] [--out build/host-tools]
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MESA="$ROOT/work/mesa"; OUT="$ROOT/build/host-tools"
while [ $# -gt 0 ]; do case "$1" in --mesa) MESA="$2"; shift 2;; --out) OUT="$2"; shift 2;; *) echo "unknown $1" >&2; exit 2;; esac; done
mkdir -p "$OUT"
echo "mesa: $(git -C "$MESA" rev-parse --short HEAD 2>/dev/null || echo MISSING)"
if [ ! -f "$OUT/build.ninja" ]; then
  meson setup "$OUT" "$MESA" -Dbuildtype=release -Dgallium-drivers= -Dvulkan-drivers=panfrost -Dplatforms= -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Dmesa-clc=enabled -Dinstall-mesa-clc=true -Dprecomp-compiler=enabled -Dinstall-precomp-compiler=true -Dllvm=enabled -Dzstd=disabled
fi
ninja -C "$OUT" src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2 src/panfrost/clc/panfrost_compile
mkdir -p "$OUT/bin"
for p in src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2 src/panfrost/clc/panfrost_compile; do
  b="$(basename "$p")"
  rm -f "$OUT/bin/$b"
  ln -sf "../$p" "$OUT/bin/$b"
done
echo "OK out=$OUT (P4 full tool build runs on Poco/NativeCode; CI re-runs this script)"
