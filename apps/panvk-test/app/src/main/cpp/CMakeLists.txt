cmake_minimum_required(VERSION 3.22)
project(panvktest C)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
set(VULKAN_INCLUDE_DIR "${REPO_ROOT}/work/mesa-dxint/include" CACHE PATH "Vulkan headers")

function(add_vk_test test_name test_rel_path)
    set(target_name "t_${test_name}")
    set(src_file "${REPO_ROOT}/${test_rel_path}")
    get_filename_component(src_dir "${src_file}" DIRECTORY)

    add_library(${target_name} SHARED "${src_file}")
    target_compile_options(${target_name} PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/pt_compat.h
        -w -O2 -fvisibility=default
    )
    target_include_directories(${target_name} PRIVATE
        "${VULKAN_INCLUDE_DIR}"
        "${src_dir}"
    )
    if(test_name STREQUAL "bc_decode")
        target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat")
    endif()
    target_link_libraries(${target_name} PRIVATE dl m)
endfunction()

# 9 standalone Vulkan test programs
add_vk_test(gpu_prerast_slice tests/dxvk/vulkan/gpu_prerast_slice.c)
add_vk_test(clip_cull tests/dxvk/vulkan/clip-cull/clip_cull.c)
add_vk_test(multi_viewport tests/dxvk/vulkan/multi-viewport/multi_viewport.c)
add_vk_test(fill_mode tests/dxvk/vulkan/fill-mode/fill_mode.c)
add_vk_test(bc_decode tests/dxvk/vulkan/bc/bc_decode.c)
add_vk_test(geometry tests/dxvk/vulkan/geometry/geometry.c)
add_vk_test(tessellation tests/dxvk/vulkan/tessellation/tessellation.c)
add_vk_test(xfb tests/dxvk/vulkan/xfb/xfb.c)
add_vk_test(pipeline_stats tests/dxvk/vulkan/pipeline_stats/pipeline_stats.c)

# SHARED lib t_vkinfo from vkinfo.c
add_library(t_vkinfo SHARED "${CMAKE_CURRENT_SOURCE_DIR}/vkinfo.c")
target_compile_options(t_vkinfo PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/pt_compat.h
    -w -O2 -fvisibility=default
)
target_include_directories(t_vkinfo PRIVATE
    "${VULKAN_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(t_vkinfo PRIVATE dl m)

# SHARED JNI lib panvktest from jni.c
add_library(panvktest SHARED "${CMAKE_CURRENT_SOURCE_DIR}/jni.c")
target_compile_options(panvktest PRIVATE -w -O2 -fvisibility=default)
target_link_libraries(panvktest PRIVATE dl log)
