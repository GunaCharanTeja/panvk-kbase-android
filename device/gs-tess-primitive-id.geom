#version 450
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
layout(set = 0, binding = 0, std430) buffer O {
    uint n;
    uint ids[];
} o;
void main() {
    uint slot = atomicAdd(o.n, 1u);
    if (slot < o.ids.length())
        o.ids[slot] = uint(gl_PrimitiveIDIn);
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    EndPrimitive();
}
