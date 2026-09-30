#!/bin/sh
# Regenerate tessellation_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"
for m in 0 1 2 3 4 5 6 7 8 9; do
   $G -S tesc -DMODE=$m --vn tcs_mode$m -o "$T/tcs$m.h" tessellation.tesc >/dev/null
done
for m in 0 1 2 3 4 5 6; do
   $G -S tese -DMODE=$m --vn tes_mode$m -o "$T/tes$m.h" tessellation.tese >/dev/null
done
$G --vn vs_tess -o "$T/vs.h" tessellation.vert >/dev/null
$G -DREF --vn vs_ref -o "$T/vsref.h" tessellation.vert >/dev/null
$G --vn gs_tess -o "$T/gs.h" tessellation.geom >/dev/null
$G --vn fs_tess -o "$T/fs.h" tessellation.frag >/dev/null
cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > tessellation_spv.h
rm -rf "$T"
