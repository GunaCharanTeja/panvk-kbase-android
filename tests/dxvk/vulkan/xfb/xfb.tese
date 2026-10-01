#version 450
#extension GL_ARB_enhanced_layouts : enable

layout(triangles, equal_spacing, ccw) in;
layout(location = 0) in vec4 tc_pos[];
layout(location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16) out vec4 out_pos;

void main() {
   gl_Position = vec4(0.0);
   out_pos = gl_TessCoord.x * tc_pos[0] + gl_TessCoord.y * tc_pos[1] + gl_TessCoord.z * tc_pos[2];
}
