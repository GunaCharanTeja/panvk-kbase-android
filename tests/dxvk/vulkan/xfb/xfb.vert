#version 450
#extension GL_ARB_enhanced_layouts : enable

#if MODE == 0
/* vs_capture: vec4 at xfb_offset 0, float at 16, xfb_stride 20 */
layout(location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 20) out vec4 out_vec;
layout(location = 1, xfb_buffer = 0, xfb_offset = 16) out float out_f;
void main() {
   gl_Position = vec4(0.0);
   out_vec = vec4(float(gl_VertexIndex), float(gl_VertexIndex) * 2.0, float(gl_VertexIndex) * 3.0, 1.0);
   out_f = float(gl_VertexIndex) * 10.0;
}
#elif MODE == 1
/* vs_passthrough: passes position to GS / TCS */
layout(location = 0) out vec4 out_pos;
void main() {
   gl_Position = vec4(0.0);
   out_pos = vec4(float(gl_VertexIndex), float(gl_VertexIndex) * 2.0, 0.0, 1.0);
}
#elif MODE == 2
/* vs_points / triangles: vec4 at offset 0, stride 16 */
layout(location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16) out vec4 out_val;
void main() {
   gl_Position = vec4(0.0);
   out_val = vec4(float(gl_VertexIndex), float(gl_VertexIndex) + 1.0, float(gl_VertexIndex) + 2.0, 1.0);
}
#elif MODE == 3
/* vs_indirect_idx: captures uint gl_VertexIndex, stride 4 */
layout(location = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4) out uint out_idx;
void main() {
   gl_Position = vec4(0.0);
   out_idx = uint(gl_VertexIndex);
}
#elif MODE == 4
layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out Blk {
   vec4 v;
} blk;
void main() {
   blk.v = vec4(gl_VertexIndex * 1.0, 7.0, -89.0, 3.5);
}
#elif MODE == 5
void main() {
}
#elif MODE == 6
layout(xfb_buffer = 0, xfb_stride = 8) out gl_PerVertex {
   vec4 gl_Position;
   layout(xfb_offset = 0) float gl_ClipDistance[1];
   layout(xfb_offset = 4) float gl_CullDistance[1];
};
void main() {
   gl_Position = vec4(0, 0, 0, 1);
   gl_ClipDistance[0] = 0.25 * gl_VertexIndex - 0.5;
   gl_CullDistance[0] = -1.5 + gl_VertexIndex;
}
#endif
