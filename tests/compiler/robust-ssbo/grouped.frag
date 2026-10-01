#version 460
layout(set = 0, binding = 0, std430) readonly buffer Data {
    uint values[];
} data;
layout(push_constant) uniform Params {
    uint base_dword;
    uint stride_dword;
} pc;
layout(location = 0) out uvec4 out_color;
void main() {
    uint base_dword = pc.base_dword + uint(gl_FragCoord.x) * pc.stride_dword;
    uint a0 = data.values[base_dword + 0u];
    uint a1 = data.values[base_dword + 1u];
    uint a2 = data.values[base_dword + 2u];
    uint a3 = data.values[base_dword + 3u];
    uint a4 = data.values[base_dword + 4u];
    uint a5 = data.values[base_dword + 5u];
    uint a6 = data.values[base_dword + 6u];
    uint a7 = data.values[base_dword + 7u];
    out_color = uvec4(a0 ^ a4, a1 ^ a5, a2 ^ a6, a3 ^ a7);
}
