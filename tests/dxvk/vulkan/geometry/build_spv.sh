#!/bin/sh
# Regenerate geometry_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
for m in 0 1 2 3 4 5 6 7 8 9 10; do
   glslangValidator -V --target-env vulkan1.1 -S geom -DMODE=$m --vn gs_mode$m \
      -o "$T/gs$m.h" geometry.geom >/dev/null
done
glslangValidator -V --target-env vulkan1.1 --vn vs_geom -o "$T/vs.h" \
   geometry.vert >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn fs_geom -o "$T/fs.h" \
   geometry.frag >/dev/null
cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > geometry_spv.h
rm -rf "$T"
