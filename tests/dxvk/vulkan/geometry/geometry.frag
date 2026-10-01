#version 450
layout(location = 0) in vec4 vcol;
layout(location = 0) out vec4 o;

void
main()
{
   o = vcol;
}
