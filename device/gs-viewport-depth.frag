#version 450
layout(location = 0) flat in int vp;
layout(location = 0) out vec4 o;
void main() { o = (vp == 0) ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0); }
