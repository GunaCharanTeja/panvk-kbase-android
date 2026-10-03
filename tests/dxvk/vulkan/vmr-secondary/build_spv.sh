#!/bin/sh
# Regenerate vmr_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"

$G --vn vs_tri -o "$T/vs.h" vmr.vert >/dev/null
$G --vn fs_count -o "$T/fs.h" vmr.frag >/dev/null

cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > vmr_spv.h
rm -rf "$T"
