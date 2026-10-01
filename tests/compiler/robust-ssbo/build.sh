#!/usr/bin/env bash
set -euo pipefail

probe_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$probe_dir/../../.." && pwd)
build_dir=${BUILD_DIR:-"$repo_root/build/tests/robust-ssbo"}
compiler=${CXX:-aarch64-linux-gnu-g++}
glslang=${GLSLANG_VALIDATOR:-glslangValidator}
spirv_val=${SPIRV_VAL:-spirv-val}
vulkan_include=${VULKAN_INCLUDE_DIR:-"$repo_root/work/mesa/include"}

for tool in "$compiler" "$glslang" "$spirv_val"; do
    command -v "$tool" >/dev/null || {
        echo "Missing required tool: $tool" >&2
        exit 1
    }
done
test -f "$vulkan_include/vulkan/vulkan.h" || {
    echo "Set VULKAN_INCLUDE_DIR to the Vulkan-Headers include directory" >&2
    exit 1
}
mkdir -p "$build_dir"
for shader in robust_ssbo.vert grouped.frag separate.frag; do
    "$glslang" -V --target-env vulkan1.3 "$probe_dir/$shader" \
        -o "$build_dir/$shader.spv"
    "$spirv_val" --target-env vulkan1.3 "$build_dir/$shader.spv"
done
"$compiler" -std=c++20 -O2 -Wall -Wextra -Wno-missing-field-initializers \
    -I"$vulkan_include" "$probe_dir/robust_ssbo_driver_probe.cpp" -ldl \
    -o "$build_dir/robust_ssbo_driver_probe"
printf 'Built %s\n' "$build_dir/robust_ssbo_driver_probe"
sha256sum "$build_dir/robust_ssbo_driver_probe" "$build_dir/"*.spv
