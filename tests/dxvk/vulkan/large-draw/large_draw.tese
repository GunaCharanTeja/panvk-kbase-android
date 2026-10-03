#version 450
#extension GL_ARB_enhanced_layouts : enable

layout(isolines, equal_spacing, point_mode) in;
layout(location = 0) in vec4 tc_v[];
layout(location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16) out vec4 out_val;

void main() {
   gl_Position = vec4(0.0);
   out_val = vec4(tc_v[0].x, gl_TessCoord.x, float(gl_PrimitiveID), 1.0);
}
