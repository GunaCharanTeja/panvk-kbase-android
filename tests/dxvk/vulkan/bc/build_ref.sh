#!/bin/sh
# Build host-only reference decoder bc_ref from a Mesa tree.
# usage: build_ref.sh <mesa_src_root> <out_binary>
set -eu
M="$1"
OUT="$2"
HERE="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
gcc -O2 -DHAVE_PTHREAD -DUTIL_ARCH_LITTLE_ENDIAN=1 -DUTIL_ARCH_BIG_ENDIAN=0 \
   -include stdlib.h -I"$M/src" -I"$M/include" -o "$OUT" \
   "$HERE/bc_ref.c" "$M/src/util/half_float.c" "$M/src/util/softfloat.c" -lm
