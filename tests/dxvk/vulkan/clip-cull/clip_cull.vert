#version 450
/* a.xy = NDC position, a.z = per-vertex cull value */
layout(location = 0) in vec4 a;
layout(location = 0) out vec4 vcol;
layout(location = 1) out vec2 vuv;

out gl_PerVertex {
   vec4 gl_Position;
#if NCLIP > 0
   float gl_ClipDistance[NCLIP];
#endif
#if NCULL > 0
   float gl_CullDistance[NCULL];
#endif
};

void
main()
{
   gl_Position = vec4(a.xy, 0.0, 1.0);
   vcol = vec4(1.0, 0.0, 0.0, 1.0);
   vuv = a.xy * 0.5 + 0.5;
#if NCLIP > 0
   for (int i = 0; i < NCLIP; i++)
      gl_ClipDistance[i] = 1.0;
#ifdef CLIPX
   gl_ClipDistance[CLIPX] = a.x;
#endif
#ifdef CLIPY
   gl_ClipDistance[CLIPY] = a.y;
#endif
#endif
#if NCULL > 0
   for (int i = 0; i < NCULL; i++)
      gl_CullDistance[i] = 1.0;
   gl_CullDistance[CULLI] = a.z;
#endif
}
