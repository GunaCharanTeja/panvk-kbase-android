#version 450
layout(location = 0) in vec4 vcol;
layout(location = 0) out vec4 o;
in float gl_ClipDistance[1];

void
main()
{
   /* surviving fragments have x >= 0; the FS must see the same value */
   float x = gl_FragCoord.x / 32.0 - 1.0;
   bool ok = gl_ClipDistance[0] >= 0.0 && abs(gl_ClipDistance[0] - x) < 0.01;
   o = ok ? vcol : vec4(0.0, 1.0, 0.0, 1.0);
}
