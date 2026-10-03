#version 450

/* One invocation per sample (sample shading), counted per sample index:
 * cnt[slot + 8 * sampleId]. */
layout(set = 0, binding = 0) buffer Counts { uint cnt[64]; };
layout(push_constant) uniform P { uint slot; };

void main() {
   atomicAdd(cnt[slot + 8u * uint(gl_SampleID)], 1u);
}
