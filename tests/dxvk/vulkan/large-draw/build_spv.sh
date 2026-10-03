#!/bin/sh
# Regenerate large_draw_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"

$G -S vert -DMODE=0 --vn vs_capture -o "$T/vs0.h" large_draw.vert >/dev/null
$G -S vert -DMODE=1 --vn vs_feed -o "$T/vs1.h" large_draw.vert >/dev/null
$G --vn gs_capture -o "$T/gs.h" large_draw.geom >/dev/null
$G --vn tcs_levels -o "$T/tcs.h" large_draw.tesc >/dev/null
$G --vn tes_capture -o "$T/tes.h" large_draw.tese >/dev/null
$G --vn fs_white -o "$T/fs.h" large_draw.frag >/dev/null

cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > large_draw_spv.h
rm -rf "$T"
