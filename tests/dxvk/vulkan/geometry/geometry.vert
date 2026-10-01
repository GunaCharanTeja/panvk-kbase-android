#version 450
/* a.xy = NDC position, a.z = red channel forwarded through the GS */
layout(location = 0) in vec4 a;
layout(location = 0) out vec4 vcol;

void
main()
{
   gl_Position = vec4(a.xy + vec2(0.25 * float(gl_InstanceIndex), 0.0), 0.0, 1.0);
   vcol = vec4(a.z, 0.0, 0.0, 1.0);
}
