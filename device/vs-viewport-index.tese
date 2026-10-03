#version 450
#extension GL_ARB_shader_viewport_layer_array : require
/* One patch = one NDC-covering triangle. p.x = viewport index, p.y = z. */
layout(triangles, equal_spacing, ccw) in;
layout(location = 0) flat out int vp;
void main() {
   vec4 p = gl_in[0].gl_Position;
   int v = int(p.x + 0.5);
   vec2 xy = gl_TessCoord.x * vec2(-1.0, -1.0) + gl_TessCoord.y * vec2(3.0, -1.0) +
             gl_TessCoord.z * vec2(-1.0, 3.0);
   gl_ViewportIndex = v;
   vp = v;
   gl_Position = vec4(xy, p.y, 1.0);
}
