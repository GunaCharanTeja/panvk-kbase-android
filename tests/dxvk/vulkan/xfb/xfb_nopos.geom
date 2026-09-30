#version 450
#extension GL_ARB_enhanced_layouts : enable

layout(points) in;
layout(points, max_vertices = 1) out;

layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 o;

void main() {
   o = vec4(gl_PrimitiveIDIn * 1.0, -89.0, 30.0, 1.0);
   EmitVertex();
   EndPrimitive();
}
