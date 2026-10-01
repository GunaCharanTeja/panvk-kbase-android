#version 450

#define inout inout_

layout(std430, set = 0, binding = 0) buffer B {
   uint hit[64];
   uint inout[32];
   uint outv[64];
   uint total;
};

void main()
{
   uint idx = uint(gl_VertexIndex);
   atomicAdd(total, 1u);
   if (atomicAdd(hit[idx], 1u) == 0u)
      outv[idx] = atomicAdd(inout[idx % 16u], idx + 1u) + 1000u;

   gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
   gl_PointSize = 1.0;
}
