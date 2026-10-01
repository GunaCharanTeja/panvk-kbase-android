#version 450
/* a.xy = NDC position, a.z = palette id (reference path). REF writes
 * gl_PointSize for POINT_LIST references. */
layout(location = 0) in vec4 a;
layout(location = 0) out vec4 vcol;

vec4
pal(int id)
{
   return vec4(1.0, float(id & 63) / 64.0, float((id >> 6) & 63) / 64.0, 1.0);
}

void
main()
{
   gl_Position = vec4(a.xy + vec2(0.75 * float(gl_InstanceIndex), 0.0), 0.0, 1.0);
   vcol = pal(int(a.z));
#ifdef REF
   gl_PointSize = 1.0;
#endif
}
