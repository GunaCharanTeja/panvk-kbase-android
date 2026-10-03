#version 450
/* One input point = one fullscreen-NDC triangle.
 * p.x = viewport index, p.y = clip-space z (w = 1). */
layout(points) in;
layout(triangle_strip, max_vertices = 3) out;
layout(location = 0) flat out int vp;

void main() {
   int v = int(gl_in[0].gl_Position.x + 0.5);
   float z = gl_in[0].gl_Position.y;
   gl_ViewportIndex = v; vp = v; gl_Position = vec4(-1.0, -1.0, z, 1.0); EmitVertex();
   gl_ViewportIndex = v; vp = v; gl_Position = vec4( 3.0, -1.0, z, 1.0); EmitVertex();
   gl_ViewportIndex = v; vp = v; gl_Position = vec4(-1.0,  3.0, z, 1.0); EmitVertex();
   EndPrimitive();
}
