#version 450
/* MODE: 0 quad L4, 1 tri L3, 2 quad L3.5, 3 quad L2.5, 4 one input point
 * expanded to 4 corners, 5 corners from inputs 0,7,31,24 of 32,
 * 6 corners in[min(i, gl_PatchVerticesIn - 1)], 7 quad L16,
 * 8 quad L64, 9 quad unequal.
 * Quad corner order: c0 TL, c1 TR, c2 BR, c3 BL. */
#if MODE == 1
layout(vertices = 3) out;
#else
layout(vertices = 4) out;
#endif

#if MODE == 1
#define LEVEL 3.0
#elif MODE == 2
#define LEVEL 3.5
#elif MODE == 3
#define LEVEL 2.5
#elif MODE == 7
#define LEVEL 16.0
#elif MODE == 8
#define LEVEL 64.0
#else
#define LEVEL 4.0
#endif

void
main()
{
   int i = gl_InvocationID;
#if MODE == 4
   const vec2 off[4] = vec2[](vec2(0.0), vec2(1.5, 0.0), vec2(1.5, 1.0),
                              vec2(0.0, 1.0));
   gl_out[gl_InvocationID].gl_Position = gl_in[0].gl_Position + vec4(off[i], 0.0, 0.0);
#elif MODE == 5
   const int pick[4] = int[](0, 7, 31, 24);
   gl_out[gl_InvocationID].gl_Position = gl_in[pick[i]].gl_Position;
#elif MODE == 6
   gl_out[gl_InvocationID].gl_Position = gl_in[min(i, gl_PatchVerticesIn - 1)].gl_Position;
#else
   gl_out[gl_InvocationID].gl_Position = gl_in[i].gl_Position;
#endif
#if MODE == 9
   gl_TessLevelOuter[0] = 1.0;
   gl_TessLevelOuter[1] = 7.0;
   gl_TessLevelOuter[2] = 3.0;
   gl_TessLevelOuter[3] = 64.0;
   gl_TessLevelInner[0] = 5.0;
   gl_TessLevelInner[1] = 2.0;
#else
   gl_TessLevelOuter[0] = LEVEL;
   gl_TessLevelOuter[1] = LEVEL;
   gl_TessLevelOuter[2] = LEVEL;
   gl_TessLevelOuter[3] = LEVEL;
   gl_TessLevelInner[0] = LEVEL;
   gl_TessLevelInner[1] = LEVEL;
#endif
}
