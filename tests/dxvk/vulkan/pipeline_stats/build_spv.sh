#!/bin/sh
# Regenerate pipeline_stats_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
glslangValidator -V --target-env vulkan1.1 --vn vs_ps -o "$T/vs.h" \
   pipeline_stats.vert >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn fs_ps -o "$T/fs.h" \
   pipeline_stats.frag >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn gs_ps -o "$T/gs.h" \
   pipeline_stats.geom >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn cs_ps -o "$T/cs.h" \
   pipeline_stats.comp >/dev/null
cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > pipeline_stats_spv.h
rm -rf "$T"
