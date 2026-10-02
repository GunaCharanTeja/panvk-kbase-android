cmake_minimum_required(VERSION 3.22)
project(panvklauncher C)

add_library(panvklauncher SHARED
    probe.c
)

target_compile_options(panvklauncher PRIVATE -w -O2 -fvisibility=default)
target_link_libraries(panvklauncher PRIVATE log dl)

add_library(VkLayer_fbread SHARED
    fbread_layer.c
)
target_compile_options(VkLayer_fbread PRIVATE -O2 -fvisibility=hidden)
target_link_options(VkLayer_fbread PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--as-needed")
target_link_libraries(VkLayer_fbread PRIVATE log)
