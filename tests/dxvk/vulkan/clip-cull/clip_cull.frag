#version 450
layout(location = 0) in vec4 vcol;
layout(location = 1) in vec2 vuv;
layout(location = 0) out vec4 o;

void
main()
{
   /* vuv must interpolate to [0,1]; a broken varying layout shows as green */
   bool uv_ok = all(greaterThanEqual(vuv, vec2(0.0))) &&
                all(lessThanEqual(vuv, vec2(1.0)));
   o = uv_ok ? vcol : vec4(0.0, 1.0, 0.0, 1.0);
}
