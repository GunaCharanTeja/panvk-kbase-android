#version 450
/* Two triangles per input triangle: the input and a copy shifted right. */
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;
void main()
{
   for (int c = 0; c < 2; c++) {
      for (int i = 0; i < 3; i++) {
         gl_Position = gl_in[i].gl_Position + vec4(0.125 * c, 0.0, 0.0, 0.0);
         EmitVertex();
      }
      EndPrimitive();
   }
}
