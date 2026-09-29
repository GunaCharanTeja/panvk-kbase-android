#!/bin/sh
# Regenerate clip_cull_spv.h from the GLSL sources (glslangValidator).
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
v() {
   name=$1
   shift
   glslangValidator -V --target-env vulkan1.1 -S vert "$@" --vn "$name" \
      -o "$T/$name.h" clip_cull.vert >/dev/null
}
v vs_none -DNCLIP=0 -DNCULL=0
v vs_clip1_x -DNCLIP=1 -DNCULL=0 -DCLIPX=0
v vs_clip2_xy -DNCLIP=2 -DNCULL=0 -DCLIPX=0 -DCLIPY=1
v vs_clip6_x5 -DNCLIP=6 -DNCULL=0 -DCLIPX=5
v vs_clip8_y7 -DNCLIP=8 -DNCULL=0 -DCLIPY=7
v vs_cull1 -DNCLIP=0 -DNCULL=1 -DCULLI=0
v vs_cull5_i4 -DNCLIP=0 -DNCULL=5 -DCULLI=4
v vs_cull8_i7 -DNCLIP=0 -DNCULL=8 -DCULLI=7
v vs_clip4y3_cull4i2 -DNCLIP=4 -DNCULL=4 -DCLIPY=3 -DCULLI=2
glslangValidator -V --target-env vulkan1.1 --vn fs_color -o "$T/fs_color.h" \
   clip_cull.frag >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn fs_clip_read \
   -o "$T/fs_clip_read.h" clip_read.frag >/dev/null
cat "$T"/*.h | sed 's/^const uint32_t/static const uint32_t/' > clip_cull_spv.h
rm -rf "$T"
