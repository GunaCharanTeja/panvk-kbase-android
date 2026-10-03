#!/bin/sh
# Regenerate tess_cond_state_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"

$G --vn vs_pos -o "$T/vs.h" tcs.vert >/dev/null
$G --vn tcs_levels -o "$T/tcs.h" tcs.tesc >/dev/null
$G --vn tes_tri -o "$T/tes.h" tcs.tese >/dev/null
$G --vn fs_push -o "$T/fs0.h" tcs_push.frag >/dev/null
$G --vn fs_red -o "$T/fs1.h" tcs_red.frag >/dev/null

cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > tess_cond_state_spv.h
rm -rf "$T"
