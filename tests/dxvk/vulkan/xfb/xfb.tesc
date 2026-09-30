#version 450

layout(vertices = 3) out;
layout(location = 0) in vec4 in_pos[];
layout(location = 0) out vec4 tc_pos[];

void main() {
   tc_pos[gl_InvocationID] = in_pos[gl_InvocationID];
   if (gl_InvocationID == 0) {
      gl_TessLevelInner[0] = 1.0;
      gl_TessLevelOuter[0] = 1.0;
      gl_TessLevelOuter[1] = 1.0;
      gl_TessLevelOuter[2] = 1.0;
   }
}
