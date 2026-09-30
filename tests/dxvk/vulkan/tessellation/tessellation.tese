#version 450
/* MODE: 0 quads equal, 1 triangles equal ccw, 2 quads fractional_odd,
 * 3 quads fractional_even, 4 isolines equal, 5 quads equal point_mode,
 * 6 triangles equal cw. u -> +x, v -> +y (framebuffer y-down). */
#if MODE == 1
layout(triangles, equal_spacing, ccw) in;
#elif MODE == 6
layout(triangles, equal_spacing, cw) in;
#elif MODE == 2
layout(quads, fractional_odd_spacing, ccw) in;
#elif MODE == 3
layout(quads, fractional_even_spacing, ccw) in;
#elif MODE == 4
layout(isolines, equal_spacing) in;
#elif MODE == 5
layout(quads, equal_spacing, ccw, point_mode) in;
#else
layout(quads, equal_spacing, ccw) in;
#endif

layout(location = 0) out vec4 vcol;

vec4
pal(int id)
{
   return vec4(1.0, float(id & 63) / 64.0, float((id >> 6) & 63) / 64.0, 1.0);
}

void
main()
{
   float u = gl_TessCoord.x, v = gl_TessCoord.y;
#if MODE == 1 || MODE == 6
   /* in0 = +u corner, in1 = +v corner, in2 = origin. Edge-exact form. */
   vec2 o = gl_in[2].gl_Position.xy;
   vec2 pos = o + u * (gl_in[0].gl_Position.xy - o) +
              v * (gl_in[1].gl_Position.xy - o);
#else
   vec2 c0 = gl_in[0].gl_Position.xy, c1 = gl_in[1].gl_Position.xy;
   vec2 c2 = gl_in[2].gl_Position.xy, c3 = gl_in[3].gl_Position.xy;
   vec2 p0 = c0 + u * (c1 - c0);
   vec2 p1 = c3 + u * (c2 - c3);
   vec2 pos = p0 + v * (p1 - p0);
#endif
   gl_Position = vec4(pos, 0.0, 1.0);
   vcol = pal(gl_PrimitiveID);
#if MODE == 5
   gl_PointSize = 1.0;
#endif
}
