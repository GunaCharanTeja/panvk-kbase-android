#!/bin/sh
# fs.vert/fs.frag -> fs_spv.h (fullscreen triangle, push-constant colour)
set -e
D=$(dirname "$0")
glslangValidator -V --vn fs_vert_spv -o "$D/fs.vert.h" "$D/fs.vert" >/dev/null
glslangValidator -V --vn fs_frag_spv -o "$D/fs.frag.h" "$D/fs.frag" >/dev/null
cat "$D/fs.vert.h" "$D/fs.frag.h" > "$D/fs_spv.h"
rm -f "$D/fs.vert.h" "$D/fs.frag.h"
