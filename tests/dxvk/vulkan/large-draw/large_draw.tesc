#version 450

layout(vertices = 1) out;
layout(location = 0) in vec4 in_v[];
layout(location = 0) out vec4 tc_v[];

void main() {
   tc_v[gl_InvocationID] = in_v[gl_InvocationID];
   gl_TessLevelOuter[0] = 1.0;
   gl_TessLevelOuter[1] = 1.0;
}
