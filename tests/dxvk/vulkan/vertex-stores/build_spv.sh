#!/bin/sh
# Regenerate vertex_stores_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
G="glslangValidator -V --target-env vulkan1.1"
$G --vn vs_stores -o "$T/vs.h" vertex_stores.vert >/dev/null
$G --vn fs_stores -o "$T/fs.h" vertex_stores.frag >/dev/null
cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > vertex_stores_spv.h
rm -rf "$T"
