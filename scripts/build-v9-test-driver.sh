#!/bin/sh
# Build the Mali v9 (Valhall JM) test driver (Android/Bionic)
# Usage: scripts/build-v9-test-driver.sh [mesa-dir] [tag]
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
V=/var/tmp/panvk/v9
MESA="${1:-$V/mesa-test}"
TAG="${2:-v9-test}"

if [ ! -d "$MESA/.git" ]; then
  mkdir -p "$MESA"
  git -C "$MESA" init
  git -C "$ROOT/work/mesa" archive 5a07217f | tar -x -C "$MESA"
  git -C "$MESA" add -A
  git -C "$MESA" -c user.name="panvk-builder" -c user.email="builder@localhost" commit -m "base-5a07217f"
fi

"$ROOT/scripts/apply-patches.sh" --profile g615-v11-csf --mesa "$MESA"

export LD_LIBRARY_PATH=/var/tmp/panvk/llvm22/usr/lib
export HOST_TOOLS="$ROOT/tmp/rel9/src/build/host-tools/bin"
export MESA BDIR="$V/build-$TAG" DDIR="$V/dist-$TAG"
mkdir -p "$DDIR"
"$ROOT/scripts/build-android.sh" --profile g615-v11-csf
