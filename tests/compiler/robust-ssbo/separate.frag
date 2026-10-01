#version 460
layout(set = 0, binding = 0, std430) readonly buffer Data {
    uint values[];
} data;
layout(set = 0, binding = 1, std430) readonly buffer Other {
    uint values[];
} other;
layout(push_constant) uniform Params {
    uint base_dword;
} pc;
layout(location = 0) out uvec4 out_color;
void main() {
    uint d0 = data.values[pc.base_dword + 0u];
    uint o0 = other.values[pc.base_dword + 1u];
    uint d1 = data.values[pc.base_dword + 2u];
    uint o1 = other.values[pc.base_dword + 3u];
    uint d2 = data.values[pc.base_dword + 4u];
    uint o2 = other.values[pc.base_dword + 5u];
    uint d3 = data.values[pc.base_dword + 6u];
    uint o3 = other.values[pc.base_dword + 7u];
    out_color = uvec4(d0 ^ o0, d1 ^ o1, d2 ^ o2, d3 ^ o3);
}
