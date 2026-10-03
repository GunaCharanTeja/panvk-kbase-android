#version 450
#extension GL_ARB_shader_viewport_layer_array : require
/* Three vertices per primitive, one NDC-covering triangle.
 * p.x = viewport index, p.y = clip-space z (w = 1). */
layout(location = 0) in vec4 p;
layout(location = 0) flat out int vp;
void main() {
   int c = gl_VertexIndex % 3;
   vec2 xy = c == 0 ? vec2(-1.0, -1.0) : c == 1 ? vec2(3.0, -1.0) : vec2(-1.0, 3.0);
   int v = int(p.x + 0.5);
   gl_ViewportIndex = v;
   vp = v;
   gl_Position = vec4(xy, p.y, 1.0);
}
