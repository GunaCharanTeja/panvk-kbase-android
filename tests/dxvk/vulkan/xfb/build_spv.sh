#!/bin/sh
# Regenerate xfb_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"

for m in 0 1 2 3 4 5 6; do
   $G -S vert -DMODE=$m --vn vs_mode$m -o "$T/vs$m.h" xfb.vert >/dev/null
done
$G --vn gs_xfb -o "$T/gs.h" xfb.geom >/dev/null
$G --vn gs_nopos -o "$T/gs_nopos.h" xfb_nopos.geom >/dev/null
$G --vn tcs_xfb -o "$T/tcs.h" xfb.tesc >/dev/null
$G --vn tes_xfb -o "$T/tes.h" xfb.tese >/dev/null
$G --vn fs_xfb -o "$T/fs.h" xfb.frag >/dev/null

cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > xfb_spv.h
rm -rf "$T"
