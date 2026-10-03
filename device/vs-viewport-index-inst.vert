#version 450
#extension GL_ARB_shader_viewport_layer_array : require
/* Instance k draws to viewport k % 2. Even instances z = 1.5, odd -0.5. */
layout(location = 0) flat out int vp;
void main() {
   int c = gl_VertexIndex % 3;
   vec2 xy = c == 0 ? vec2(-1.0, -1.0) : c == 1 ? vec2(3.0, -1.0) : vec2(-1.0, 3.0);
   int v = gl_InstanceIndex % 2;
   gl_ViewportIndex = v;
   vp = v;
   gl_Position = vec4(xy, v == 0 ? 1.5 : -0.5, 1.0);
}
