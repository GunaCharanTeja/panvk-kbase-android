#version 450
/* MODE: 0 pass tri, 1 pass line, 2 point->quad, 3 tri->3 lines,
 * 4 two tris, 5 4-vertex strip, 6 invocations, 7 even primitive IDs,
 * 8 tri adjacency, 9 line adjacency, 10 gl_Layer write */
#if MODE == 1
layout(lines) in;
layout(line_strip, max_vertices = 2) out;
#elif MODE == 2
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;
#elif MODE == 3
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;
#elif MODE == 6
layout(triangles, invocations = 3) in;
layout(triangle_strip, max_vertices = 3) out;
#elif MODE == 8
layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 3) out;
#elif MODE == 9
layout(lines_adjacency) in;
layout(line_strip, max_vertices = 2) out;
#else
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;
#endif

layout(location = 0) in vec4 vcol_in[];
layout(location = 0) out vec4 vcol;

in gl_PerVertex {
   vec4 gl_Position;
} gl_in[];

out gl_PerVertex {
   vec4 gl_Position;
};

void
emit(int i, vec2 d)
{
   gl_Position = gl_in[i].gl_Position + vec4(d, 0.0, 0.0);
   vcol = vcol_in[i];
#if MODE == 10
   gl_Layer = 0;
#endif
   EmitVertex();
}

void
main()
{
#if MODE == 0 || MODE == 10
   emit(0, vec2(0.0));
   emit(1, vec2(0.0));
   emit(2, vec2(0.0));
#elif MODE == 1
   emit(0, vec2(0.0));
   emit(1, vec2(0.0));
#elif MODE == 2
   const float h = 3.0 / 32.0;
   emit(0, vec2(-h, -h));
   emit(0, vec2(h, -h));
   emit(0, vec2(-h, h));
   emit(0, vec2(h, h));
#elif MODE == 3
   emit(0, vec2(0.0));
   emit(1, vec2(0.0));
   EndPrimitive();
   emit(1, vec2(0.0));
   emit(2, vec2(0.0));
   EndPrimitive();
   emit(2, vec2(0.0));
   emit(0, vec2(0.0));
#elif MODE == 4
   emit(0, vec2(0.0));
   emit(1, vec2(0.0));
   emit(2, vec2(0.0));
   EndPrimitive();
   emit(0, vec2(0.125));
   emit(1, vec2(0.125));
   emit(2, vec2(0.125));
#elif MODE == 5
   emit(0, vec2(0.0));
   emit(1, vec2(0.0));
   emit(2, vec2(0.0));
   emit(2, vec2(0.25, 0.0));
#elif MODE == 6
   vec2 d = vec2(0.0, 0.25 * float(gl_InvocationID));
   emit(0, d);
   emit(1, d);
   emit(2, d);
#elif MODE == 7
   if ((gl_PrimitiveIDIn & 1) == 0) {
      emit(0, vec2(0.0));
      emit(1, vec2(0.0));
      emit(2, vec2(0.0));
   }
#elif MODE == 8
   emit(0, vec2(0.0));
   emit(2, vec2(0.0));
   emit(4, vec2(0.0));
#elif MODE == 9
   emit(1, vec2(0.0));
   emit(2, vec2(0.0));
#endif
}
