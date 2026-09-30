#version 450
#extension GL_ARB_enhanced_layouts : enable
#extension GL_ARB_gpu_shader5 : enable

layout(points) in;
layout(points, max_vertices = 4) out;

layout(stream = 0, location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16) out vec4 out_s0;
layout(stream = 1, location = 1, xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16) out vec4 out_s1;

void main() {
   gl_Position = vec4(0.0);
   out_s0 = vec4(float(gl_PrimitiveIDIn) * 10.0 + 1.0, 2.0, 3.0, 4.0);
   EmitStreamVertex(0);
   EndStreamPrimitive(0);

   out_s0 = vec4(float(gl_PrimitiveIDIn) * 10.0 + 2.0, 6.0, 7.0, 8.0);
   EmitStreamVertex(0);
   EndStreamPrimitive(0);

   out_s1 = vec4(float(gl_PrimitiveIDIn) * 100.0 + 1.0, 20.0, 30.0, 40.0);
   EmitStreamVertex(1);
   EndStreamPrimitive(1);
}
