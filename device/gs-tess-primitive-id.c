/* Gate 088 device: tess-fed GS gl_PrimitiveIDIn is the patch ID.
 *
 * Two patches, instance count 2, tess outer/inner 2 (6 triangles/patch).
 * GS atomicAdd stores gl_PrimitiveIDIn. Expected IDs are patch IDs
 * 0,1 repeated per instance, not the assembled triangle ordinal and not
 * ordinal+prim_id_base. A second draw of 600 patches (above the tess
 * arena cap, ~507) forces a GPU chunk; IDs must still be the API patch
 * index. Same recorded direct, indirect, and 600-patch buffers are each
 * submitted twice. A second indirect buffer draws 600 patches (arena
 * crossing). Memory type is chosen from each resource's memoryTypeBits.
 *
 * Features: tessellationShader, geometryShader,
 * vertexPipelineStoresAndAtomics, synchronization2 (Vulkan 1.3 feature
 * only). Counter is device-local STORAGE|TRANSFER_SRC|TRANSFER_DST.
 * HOST|TRANSFER -> GS before the render pass. After the pass,
 * GS-write -> transfer-read, copy, transfer-write -> host-read.
 *
 * usage: gs-tess-primitive-id <libvulkan_panfrost.so>
 */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, (int)_r, __LINE__);            \
         return 1;                                                             \
      }                                                                        \
   } while (0)

#define W 8
#define H 8
#define PATCHES 2
#define INSTANCES 2
/* equal_spacing triangles, outer=inner=2: 6 tris/patch (not 4). */
#define TRIS 6
#define CAP (PATCHES * INSTANCES * TRIS)
#define CHUNK_PATCHES 600
#define CHUNK_INST 1
#define SSBO_WORDS (1u + 8192u)

static const uint32_t vert_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000015u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0007000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000du, 0x00000011u, 0x00030003u,
   0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00060005u, 0x0000000bu,
   0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x0000000bu, 0x00000000u, 0x505f6c67u,
   0x7469736fu, 0x006e6f69u, 0x00070006u, 0x0000000bu, 0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u,
   0x00000000u, 0x00070006u, 0x0000000bu, 0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu,
   0x00070006u, 0x0000000bu, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00030005u,
   0x0000000du, 0x00000000u, 0x00030005u, 0x00000011u, 0x00000070u, 0x00030047u, 0x0000000bu, 0x00000002u,
   0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x0000000bu, 0x00000001u,
   0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u,
   0x0000000bu, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00040047u, 0x00000011u, 0x0000001eu, 0x00000000u,
   0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u,
   0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040015u, 0x00000008u, 0x00000020u, 0x00000000u,
   0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu, 0x0000000au, 0x00000006u, 0x00000009u,
   0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u, 0x0000000au, 0x0000000au, 0x00040020u, 0x0000000cu,
   0x00000003u, 0x0000000bu, 0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000003u, 0x00040015u, 0x0000000eu,
   0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000eu, 0x0000000fu, 0x00000000u, 0x00040020u, 0x00000010u,
   0x00000001u, 0x00000007u, 0x0004003bu, 0x00000010u, 0x00000011u, 0x00000001u, 0x00040020u, 0x00000013u,
   0x00000003u, 0x00000007u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
   0x00000005u, 0x0004003du, 0x00000007u, 0x00000012u, 0x00000011u, 0x00050041u, 0x00000013u, 0x00000014u,
   0x0000000du, 0x0000000fu, 0x0003003eu, 0x00000014u, 0x00000012u, 0x000100fdu, 0x00010038u,
};

static const uint32_t tesc_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000035u, 0x00000000u, 0x00020011u, 0x00000003u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x000a000fu, 0x00000001u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000fu, 0x00000012u, 0x00000019u,
   0x00000028u, 0x0000002fu, 0x00040010u, 0x00000004u, 0x0000001au, 0x00000003u, 0x00030003u, 0x00000002u,
   0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00060005u, 0x0000000bu, 0x505f6c67u,
   0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x0000000bu, 0x00000000u, 0x505f6c67u, 0x7469736fu,
   0x006e6f69u, 0x00070006u, 0x0000000bu, 0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u,
   0x00070006u, 0x0000000bu, 0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u,
   0x0000000bu, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00040005u, 0x0000000fu,
   0x6f5f6c67u, 0x00007475u, 0x00060005u, 0x00000012u, 0x495f6c67u, 0x636f766eu, 0x6f697461u, 0x0044496eu,
   0x00060005u, 0x00000015u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x00000015u,
   0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000015u, 0x00000001u, 0x505f6c67u,
   0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x00000015u, 0x00000002u, 0x435f6c67u, 0x4470696cu,
   0x61747369u, 0x0065636eu, 0x00070006u, 0x00000015u, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u,
   0x0065636eu, 0x00040005u, 0x00000019u, 0x695f6c67u, 0x0000006eu, 0x00070005u, 0x00000028u, 0x545f6c67u,
   0x4c737365u, 0x6c657665u, 0x656e6e49u, 0x00000072u, 0x00070005u, 0x0000002fu, 0x545f6c67u, 0x4c737365u,
   0x6c657665u, 0x6574754fu, 0x00000072u, 0x00030047u, 0x0000000bu, 0x00000002u, 0x00050048u, 0x0000000bu,
   0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x0000000bu, 0x00000001u, 0x0000000bu, 0x00000001u,
   0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x0000000bu, 0x00000003u,
   0x0000000bu, 0x00000004u, 0x00040047u, 0x00000012u, 0x0000000bu, 0x00000008u, 0x00030047u, 0x00000015u,
   0x00000002u, 0x00050048u, 0x00000015u, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x00000015u,
   0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x00000015u, 0x00000002u, 0x0000000bu, 0x00000003u,
   0x00050048u, 0x00000015u, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00040047u, 0x00000028u, 0x0000000bu,
   0x0000000cu, 0x00030047u, 0x00000028u, 0x0000000fu, 0x00040047u, 0x0000002fu, 0x0000000bu, 0x0000000bu,
   0x00030047u, 0x0000002fu, 0x0000000fu, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u,
   0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040015u,
   0x00000008u, 0x00000020u, 0x00000000u, 0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu,
   0x0000000au, 0x00000006u, 0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u, 0x0000000au,
   0x0000000au, 0x0004002bu, 0x00000008u, 0x0000000cu, 0x00000003u, 0x0004001cu, 0x0000000du, 0x0000000bu,
   0x0000000cu, 0x00040020u, 0x0000000eu, 0x00000003u, 0x0000000du, 0x0004003bu, 0x0000000eu, 0x0000000fu,
   0x00000003u, 0x00040015u, 0x00000010u, 0x00000020u, 0x00000001u, 0x00040020u, 0x00000011u, 0x00000001u,
   0x00000010u, 0x0004003bu, 0x00000011u, 0x00000012u, 0x00000001u, 0x0004002bu, 0x00000010u, 0x00000014u,
   0x00000000u, 0x0006001eu, 0x00000015u, 0x00000007u, 0x00000006u, 0x0000000au, 0x0000000au, 0x0004002bu,
   0x00000008u, 0x00000016u, 0x00000020u, 0x0004001cu, 0x00000017u, 0x00000015u, 0x00000016u, 0x00040020u,
   0x00000018u, 0x00000001u, 0x00000017u, 0x0004003bu, 0x00000018u, 0x00000019u, 0x00000001u, 0x00040020u,
   0x0000001bu, 0x00000001u, 0x00000007u, 0x00040020u, 0x0000001eu, 0x00000003u, 0x00000007u, 0x00020014u,
   0x00000021u, 0x0004002bu, 0x00000008u, 0x00000025u, 0x00000002u, 0x0004001cu, 0x00000026u, 0x00000006u,
   0x00000025u, 0x00040020u, 0x00000027u, 0x00000003u, 0x00000026u, 0x0004003bu, 0x00000027u, 0x00000028u,
   0x00000003u, 0x0004002bu, 0x00000006u, 0x00000029u, 0x40000000u, 0x00040020u, 0x0000002au, 0x00000003u,
   0x00000006u, 0x0004002bu, 0x00000008u, 0x0000002cu, 0x00000004u, 0x0004001cu, 0x0000002du, 0x00000006u,
   0x0000002cu, 0x00040020u, 0x0000002eu, 0x00000003u, 0x0000002du, 0x0004003bu, 0x0000002eu, 0x0000002fu,
   0x00000003u, 0x0004002bu, 0x00000010u, 0x00000031u, 0x00000001u, 0x0004002bu, 0x00000010u, 0x00000033u,
   0x00000002u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u,
   0x0004003du, 0x00000010u, 0x00000013u, 0x00000012u, 0x0004003du, 0x00000010u, 0x0000001au, 0x00000012u,
   0x00060041u, 0x0000001bu, 0x0000001cu, 0x00000019u, 0x0000001au, 0x00000014u, 0x0004003du, 0x00000007u,
   0x0000001du, 0x0000001cu, 0x00060041u, 0x0000001eu, 0x0000001fu, 0x0000000fu, 0x00000013u, 0x00000014u,
   0x0003003eu, 0x0000001fu, 0x0000001du, 0x0004003du, 0x00000010u, 0x00000020u, 0x00000012u, 0x000500aau,
   0x00000021u, 0x00000022u, 0x00000020u, 0x00000014u, 0x000300f7u, 0x00000024u, 0x00000000u, 0x000400fau,
   0x00000022u, 0x00000023u, 0x00000024u, 0x000200f8u, 0x00000023u, 0x00050041u, 0x0000002au, 0x0000002bu,
   0x00000028u, 0x00000014u, 0x0003003eu, 0x0000002bu, 0x00000029u, 0x00050041u, 0x0000002au, 0x00000030u,
   0x0000002fu, 0x00000014u, 0x0003003eu, 0x00000030u, 0x00000029u, 0x00050041u, 0x0000002au, 0x00000032u,
   0x0000002fu, 0x00000031u, 0x0003003eu, 0x00000032u, 0x00000029u, 0x00050041u, 0x0000002au, 0x00000034u,
   0x0000002fu, 0x00000033u, 0x0003003eu, 0x00000034u, 0x00000029u, 0x000200f9u, 0x00000024u, 0x000200f8u,
   0x00000024u, 0x000100fdu, 0x00010038u,
};

static const uint32_t tese_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000031u, 0x00000000u, 0x00020011u, 0x00000003u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0008000fu, 0x00000002u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000du, 0x00000012u, 0x0000001bu,
   0x00030010u, 0x00000004u, 0x00000016u, 0x00030010u, 0x00000004u, 0x00000001u, 0x00030010u, 0x00000004u,
   0x00000005u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
   0x00060005u, 0x0000000bu, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x0000000bu,
   0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x0000000bu, 0x00000001u, 0x505f6c67u,
   0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x0000000bu, 0x00000002u, 0x435f6c67u, 0x4470696cu,
   0x61747369u, 0x0065636eu, 0x00070006u, 0x0000000bu, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u,
   0x0065636eu, 0x00030005u, 0x0000000du, 0x00000000u, 0x00060005u, 0x00000012u, 0x545f6c67u, 0x43737365u,
   0x64726f6fu, 0x00000000u, 0x00060005u, 0x00000017u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u,
   0x00060006u, 0x00000017u, 0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000017u,
   0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x00000017u, 0x00000002u,
   0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u, 0x00000017u, 0x00000003u, 0x435f6c67u,
   0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00040005u, 0x0000001bu, 0x695f6c67u, 0x0000006eu, 0x00030047u,
   0x0000000bu, 0x00000002u, 0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u,
   0x0000000bu, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu,
   0x00000003u, 0x00050048u, 0x0000000bu, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00040047u, 0x00000012u,
   0x0000000bu, 0x0000000du, 0x00030047u, 0x00000017u, 0x00000002u, 0x00050048u, 0x00000017u, 0x00000000u,
   0x0000000bu, 0x00000000u, 0x00050048u, 0x00000017u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u,
   0x00000017u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000017u, 0x00000003u, 0x0000000bu,
   0x00000004u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u,
   0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040015u, 0x00000008u, 0x00000020u,
   0x00000000u, 0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu, 0x0000000au, 0x00000006u,
   0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u, 0x0000000au, 0x0000000au, 0x00040020u,
   0x0000000cu, 0x00000003u, 0x0000000bu, 0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000003u, 0x00040015u,
   0x0000000eu, 0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000eu, 0x0000000fu, 0x00000000u, 0x00040017u,
   0x00000010u, 0x00000006u, 0x00000003u, 0x00040020u, 0x00000011u, 0x00000001u, 0x00000010u, 0x0004003bu,
   0x00000011u, 0x00000012u, 0x00000001u, 0x0004002bu, 0x00000008u, 0x00000013u, 0x00000000u, 0x00040020u,
   0x00000014u, 0x00000001u, 0x00000006u, 0x0006001eu, 0x00000017u, 0x00000007u, 0x00000006u, 0x0000000au,
   0x0000000au, 0x0004002bu, 0x00000008u, 0x00000018u, 0x00000020u, 0x0004001cu, 0x00000019u, 0x00000017u,
   0x00000018u, 0x00040020u, 0x0000001au, 0x00000001u, 0x00000019u, 0x0004003bu, 0x0000001au, 0x0000001bu,
   0x00000001u, 0x00040020u, 0x0000001cu, 0x00000001u, 0x00000007u, 0x0004002bu, 0x0000000eu, 0x00000022u,
   0x00000001u, 0x0004002bu, 0x00000008u, 0x00000027u, 0x00000002u, 0x0004002bu, 0x0000000eu, 0x0000002au,
   0x00000002u, 0x00040020u, 0x0000002fu, 0x00000003u, 0x00000007u, 0x00050036u, 0x00000002u, 0x00000004u,
   0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x00050041u, 0x00000014u, 0x00000015u, 0x00000012u,
   0x00000013u, 0x0004003du, 0x00000006u, 0x00000016u, 0x00000015u, 0x00060041u, 0x0000001cu, 0x0000001du,
   0x0000001bu, 0x0000000fu, 0x0000000fu, 0x0004003du, 0x00000007u, 0x0000001eu, 0x0000001du, 0x0005008eu,
   0x00000007u, 0x0000001fu, 0x0000001eu, 0x00000016u, 0x00050041u, 0x00000014u, 0x00000020u, 0x00000012u,
   0x00000009u, 0x0004003du, 0x00000006u, 0x00000021u, 0x00000020u, 0x00060041u, 0x0000001cu, 0x00000023u,
   0x0000001bu, 0x00000022u, 0x0000000fu, 0x0004003du, 0x00000007u, 0x00000024u, 0x00000023u, 0x0005008eu,
   0x00000007u, 0x00000025u, 0x00000024u, 0x00000021u, 0x00050081u, 0x00000007u, 0x00000026u, 0x0000001fu,
   0x00000025u, 0x00050041u, 0x00000014u, 0x00000028u, 0x00000012u, 0x00000027u, 0x0004003du, 0x00000006u,
   0x00000029u, 0x00000028u, 0x00060041u, 0x0000001cu, 0x0000002bu, 0x0000001bu, 0x0000002au, 0x0000000fu,
   0x0004003du, 0x00000007u, 0x0000002cu, 0x0000002bu, 0x0005008eu, 0x00000007u, 0x0000002du, 0x0000002cu,
   0x00000029u, 0x00050081u, 0x00000007u, 0x0000002eu, 0x00000026u, 0x0000002du, 0x00050041u, 0x0000002fu,
   0x00000030u, 0x0000000du, 0x0000000fu, 0x0003003eu, 0x00000030u, 0x0000002eu, 0x000100fdu, 0x00010038u,
};

static const uint32_t geom_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x0000003au, 0x00000000u, 0x00020011u, 0x00000002u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0008000fu, 0x00000003u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000001fu, 0x00000028u, 0x0000002du,
   0x00030010u, 0x00000004u, 0x00000016u, 0x00040010u, 0x00000004u, 0x00000000u, 0x00000001u, 0x00030010u,
   0x00000004u, 0x0000001du, 0x00040010u, 0x00000004u, 0x0000001au, 0x00000003u, 0x00030003u, 0x00000002u,
   0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00040005u, 0x00000008u, 0x746f6c73u,
   0x00000000u, 0x00030005u, 0x0000000au, 0x0000004fu, 0x00040006u, 0x0000000au, 0x00000000u, 0x0000006eu,
   0x00040006u, 0x0000000au, 0x00000001u, 0x00736469u, 0x00030005u, 0x0000000cu, 0x0000006fu, 0x00070005u,
   0x0000001fu, 0x505f6c67u, 0x696d6972u, 0x65766974u, 0x6e494449u, 0x00000000u, 0x00060005u, 0x00000026u,
   0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x00000026u, 0x00000000u, 0x505f6c67u,
   0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000026u, 0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u,
   0x00000000u, 0x00070006u, 0x00000026u, 0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu,
   0x00070006u, 0x00000026u, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00030005u,
   0x00000028u, 0x00000000u, 0x00060005u, 0x00000029u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u,
   0x00060006u, 0x00000029u, 0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000029u,
   0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x00000029u, 0x00000002u,
   0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u, 0x00000029u, 0x00000003u, 0x435f6c67u,
   0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00040005u, 0x0000002du, 0x695f6c67u, 0x0000006eu, 0x00040047u,
   0x00000009u, 0x00000006u, 0x00000004u, 0x00030047u, 0x0000000au, 0x00000003u, 0x00050048u, 0x0000000au,
   0x00000000u, 0x00000023u, 0x00000000u, 0x00050048u, 0x0000000au, 0x00000001u, 0x00000023u, 0x00000004u,
   0x00040047u, 0x0000000cu, 0x00000021u, 0x00000000u, 0x00040047u, 0x0000000cu, 0x00000022u, 0x00000000u,
   0x00040047u, 0x0000001fu, 0x0000000bu, 0x00000007u, 0x00030047u, 0x00000026u, 0x00000002u, 0x00050048u,
   0x00000026u, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x00000026u, 0x00000001u, 0x0000000bu,
   0x00000001u, 0x00050048u, 0x00000026u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000026u,
   0x00000003u, 0x0000000bu, 0x00000004u, 0x00030047u, 0x00000029u, 0x00000002u, 0x00050048u, 0x00000029u,
   0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x00000029u, 0x00000001u, 0x0000000bu, 0x00000001u,
   0x00050048u, 0x00000029u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000029u, 0x00000003u,
   0x0000000bu, 0x00000004u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u,
   0x00000006u, 0x00000020u, 0x00000000u, 0x00040020u, 0x00000007u, 0x00000007u, 0x00000006u, 0x0003001du,
   0x00000009u, 0x00000006u, 0x0004001eu, 0x0000000au, 0x00000006u, 0x00000009u, 0x00040020u, 0x0000000bu,
   0x00000002u, 0x0000000au, 0x0004003bu, 0x0000000bu, 0x0000000cu, 0x00000002u, 0x00040015u, 0x0000000du,
   0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000du, 0x0000000eu, 0x00000000u, 0x00040020u, 0x0000000fu,
   0x00000002u, 0x00000006u, 0x0004002bu, 0x00000006u, 0x00000011u, 0x00000001u, 0x0004002bu, 0x00000006u,
   0x00000012u, 0x00000000u, 0x00020014u, 0x00000018u, 0x0004002bu, 0x0000000du, 0x0000001cu, 0x00000001u,
   0x00040020u, 0x0000001eu, 0x00000001u, 0x0000000du, 0x0004003bu, 0x0000001eu, 0x0000001fu, 0x00000001u,
   0x00030016u, 0x00000023u, 0x00000020u, 0x00040017u, 0x00000024u, 0x00000023u, 0x00000004u, 0x0004001cu,
   0x00000025u, 0x00000023u, 0x00000011u, 0x0006001eu, 0x00000026u, 0x00000024u, 0x00000023u, 0x00000025u,
   0x00000025u, 0x00040020u, 0x00000027u, 0x00000003u, 0x00000026u, 0x0004003bu, 0x00000027u, 0x00000028u,
   0x00000003u, 0x0006001eu, 0x00000029u, 0x00000024u, 0x00000023u, 0x00000025u, 0x00000025u, 0x0004002bu,
   0x00000006u, 0x0000002au, 0x00000003u, 0x0004001cu, 0x0000002bu, 0x00000029u, 0x0000002au, 0x00040020u,
   0x0000002cu, 0x00000001u, 0x0000002bu, 0x0004003bu, 0x0000002cu, 0x0000002du, 0x00000001u, 0x00040020u,
   0x0000002eu, 0x00000001u, 0x00000024u, 0x00040020u, 0x00000031u, 0x00000003u, 0x00000024u, 0x0004002bu,
   0x0000000du, 0x00000036u, 0x00000002u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u,
   0x000200f8u, 0x00000005u, 0x0004003bu, 0x00000007u, 0x00000008u, 0x00000007u, 0x00050041u, 0x0000000fu,
   0x00000010u, 0x0000000cu, 0x0000000eu, 0x000700eau, 0x00000006u, 0x00000013u, 0x00000010u, 0x00000011u,
   0x00000012u, 0x00000011u, 0x0003003eu, 0x00000008u, 0x00000013u, 0x0004003du, 0x00000006u, 0x00000014u,
   0x00000008u, 0x00050044u, 0x00000006u, 0x00000015u, 0x0000000cu, 0x00000001u, 0x0004007cu, 0x0000000du,
   0x00000016u, 0x00000015u, 0x0004007cu, 0x00000006u, 0x00000017u, 0x00000016u, 0x000500b0u, 0x00000018u,
   0x00000019u, 0x00000014u, 0x00000017u, 0x000300f7u, 0x0000001bu, 0x00000000u, 0x000400fau, 0x00000019u,
   0x0000001au, 0x0000001bu, 0x000200f8u, 0x0000001au, 0x0004003du, 0x00000006u, 0x0000001du, 0x00000008u,
   0x0004003du, 0x0000000du, 0x00000020u, 0x0000001fu, 0x0004007cu, 0x00000006u, 0x00000021u, 0x00000020u,
   0x00060041u, 0x0000000fu, 0x00000022u, 0x0000000cu, 0x0000001cu, 0x0000001du, 0x0003003eu, 0x00000022u,
   0x00000021u, 0x000200f9u, 0x0000001bu, 0x000200f8u, 0x0000001bu, 0x00060041u, 0x0000002eu, 0x0000002fu,
   0x0000002du, 0x0000000eu, 0x0000000eu, 0x0004003du, 0x00000024u, 0x00000030u, 0x0000002fu, 0x00050041u,
   0x00000031u, 0x00000032u, 0x00000028u, 0x0000000eu, 0x0003003eu, 0x00000032u, 0x00000030u, 0x000100dau,
   0x00060041u, 0x0000002eu, 0x00000033u, 0x0000002du, 0x0000001cu, 0x0000000eu, 0x0004003du, 0x00000024u,
   0x00000034u, 0x00000033u, 0x00050041u, 0x00000031u, 0x00000035u, 0x00000028u, 0x0000000eu, 0x0003003eu,
   0x00000035u, 0x00000034u, 0x000100dau, 0x00060041u, 0x0000002eu, 0x00000037u, 0x0000002du, 0x00000036u,
   0x0000000eu, 0x0004003du, 0x00000024u, 0x00000038u, 0x00000037u, 0x00050041u, 0x00000031u, 0x00000039u,
   0x00000028u, 0x0000000eu, 0x0003003eu, 0x00000039u, 0x00000038u, 0x000100dau, 0x000100dbu, 0x000100fdu,
   0x00010038u,
};

static const uint32_t frag_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000du, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0006000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x00030010u, 0x00000004u,
   0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
   0x00030005u, 0x00000009u, 0x0000006fu, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000000u, 0x00020013u,
   0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u,
   0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u, 0x00000008u, 0x00000003u, 0x00000007u, 0x0004003bu,
   0x00000008u, 0x00000009u, 0x00000003u, 0x0004002bu, 0x00000006u, 0x0000000au, 0x3f800000u, 0x0004002bu,
   0x00000006u, 0x0000000bu, 0x00000000u, 0x0007002cu, 0x00000007u, 0x0000000cu, 0x0000000au, 0x0000000bu,
   0x0000000bu, 0x0000000au, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
   0x00000005u, 0x0003003eu, 0x00000009u, 0x0000000cu, 0x000100fdu, 0x00010038u,
};

struct gpu {
   icd_gipa_fn gipa;
   VkInstance inst;
   VkPhysicalDevice phys;
   VkDevice dev;
   VkQueue q;
   uint32_t qi;
   VkPhysicalDeviceMemoryProperties mp;
   VkCommandPool pool;
   PFN_vkGetDeviceProcAddr gdpa;
   PFN_vkCreateBuffer CreateBuffer;
   PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkBindBufferMemory BindBufferMemory;
   PFN_vkMapMemory MapMemory;
   PFN_vkCreateImage CreateImage;
   PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
   PFN_vkBindImageMemory BindImageMemory;
   PFN_vkCreateImageView CreateImageView;
   PFN_vkCreateRenderPass CreateRenderPass;
   PFN_vkCreateFramebuffer CreateFramebuffer;
   PFN_vkCreateShaderModule CreateShaderModule;
   PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
   PFN_vkCreatePipelineLayout CreatePipelineLayout;
   PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
   PFN_vkCreateDescriptorPool CreateDescriptorPool;
   PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
   PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
   PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
   PFN_vkBeginCommandBuffer BeginCommandBuffer;
   PFN_vkEndCommandBuffer EndCommandBuffer;
   PFN_vkResetCommandBuffer ResetCommandBuffer;
   PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
   PFN_vkCmdEndRenderPass CmdEndRenderPass;
   PFN_vkCmdBindPipeline CmdBindPipeline;
   PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
   PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
   PFN_vkCmdDraw CmdDraw;
   PFN_vkCmdDrawIndirect CmdDrawIndirect;
   PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
   PFN_vkCmdCopyBuffer CmdCopyBuffer;
   PFN_vkCmdFillBuffer CmdFillBuffer;
   PFN_vkCreateFence CreateFence;
   PFN_vkWaitForFences WaitForFences;
   PFN_vkResetFences ResetFences;
   PFN_vkQueueSubmit QueueSubmit;
};

struct buf {
   VkBuffer b;
   VkDeviceMemory m;
   void *p;
   VkDeviceSize sz;
};

static int
proc(struct gpu *g, const char *name, void *dst)
{
   PFN_vkVoidFunction p = g->gdpa(g->dev, name);
   if (!p) {
      printf("FAIL missing %s\n", name);
      return 1;
   }
   memcpy(dst, &p, sizeof(p));
   return 0;
}

static double
now_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void
stamp(const char *what)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   printf("T %ld.%09ld %s\n", (long)ts.tv_sec, ts.tv_nsec, what);
}

/* host: HOST_VISIBLE|HOST_COHERENT. local: DEVICE_LOCAL and not host-visible.
 * The type must be set in this resource's memoryTypeBits. */
static int
pick_type(struct gpu *g, uint32_t bits, int host, int local, uint32_t *out)
{
   uint32_t fb = ~0u;
   for (uint32_t i = 0; i < g->mp.memoryTypeCount; i++) {
      if (!(bits & (1u << i)))
         continue;
      VkMemoryPropertyFlags f = g->mp.memoryTypes[i].propertyFlags;
      int hv = (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
               (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      int dl = (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
               !(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      if (local && dl) {
         *out = i;
         return 0;
      }
      if (host && hv) {
         *out = i;
         return 0;
      }
      if (fb == ~0u && ((host && hv) || (local && (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))))
         fb = i;
   }
   if (local && fb != ~0u) {
      *out = fb;
      return 0;
   }
   printf("FAIL no memory type host=%d local=%d bits=0x%x\n", host, local, bits);
   return 1;
}

static int
make_buf(struct gpu *g, VkDeviceSize sz, VkBufferUsageFlags usage, int host,
         int local, struct buf *o)
{
   VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size = sz,
                            .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(g->CreateBuffer(g->dev, &bi, NULL, &o->b), "CreateBuffer");
   VkMemoryRequirements mr;
   g->GetBufferMemoryRequirements(g->dev, o->b, &mr);
   uint32_t ti;
   if (pick_type(g, mr.memoryTypeBits, host, local, &ti))
      return 1;
   if ((mr.memoryTypeBits & (1u << ti)) == 0) {
      printf("FAIL type %u not in bits 0x%x\n", ti, mr.memoryTypeBits);
      return 1;
   }
   printf("ALLOC buf host=%d local=%d type=%u bits=0x%x flags=0x%x\n", host,
          local, ti, mr.memoryTypeBits, g->mp.memoryTypes[ti].propertyFlags);
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex = ti};
   CK(g->AllocateMemory(g->dev, &mai, NULL, &o->m), "AllocateMemory");
   CK(g->BindBufferMemory(g->dev, o->b, o->m, 0), "BindBufferMemory");
   o->sz = sz;
   o->p = NULL;
   if (host)
      CK(g->MapMemory(g->dev, o->m, 0, sz, 0, &o->p), "MapMemory");
   return 0;
}

static int
wait_fence(struct gpu *g, VkFence f, double t0, const char *what)
{
   VkResult wr = VK_TIMEOUT;
   for (int k = 0; k < 40 && wr == VK_TIMEOUT; k++)
      wr = g->WaitForFences(g->dev, 1, &f, VK_TRUE, 500000000ull);
   printf("FENCE %s r=%d dt=%.3f\n", what, (int)wr, now_s() - t0);
   if (wr == VK_TIMEOUT)
      return 3;
   return wr == VK_SUCCESS ? 0 : 1;
}

/* Device-local SSBO: GS stores visible to the later copy. Not inside a
 * render pass. */
static void
gs_to_copy(struct gpu *g, VkCommandBuffer cmd, VkBuffer b, VkDeviceSize sz)
{
   VkBufferMemoryBarrier bb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = sz};
   g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &bb, 0,
                         NULL);
}

/* words = 1+n. Writes /tmp/088-ids-<name>.bin (uint32 n, then n IDs) and
 * prints the full histogram so an oracle can recompute without the log's
 * four-ID prefix. */
static int
check_ids(const char *name, const uint32_t *rb, uint32_t patches,
          uint32_t instances)
{
   uint32_t n = rb[0];
   uint32_t expect_n = patches * instances * TRIS;
   printf("CASE %s n=%u expect=%u\n", name, n, expect_n);
   if (n != expect_n || n + 1 > SSBO_WORDS) {
      printf("FAIL %s count\n", name);
      return 1;
   }
   char path[128];
   snprintf(path, sizeof(path), "/tmp/088-ids-%s.bin", name);
   int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
   if (fd < 0) {
      printf("FAIL dump %s errno=%d\n", path, errno);
      return 1;
   }
   size_t bytes = (size_t)(n + 1) * 4;
   if (write(fd, rb, bytes) != (ssize_t)bytes) {
      printf("FAIL dump write %s\n", path);
      close(fd);
      return 1;
   }
   close(fd);

   uint32_t *full = calloc(patches, sizeof(uint32_t));
   if (!full)
      return 1;
   uint32_t bad = 0;
   for (uint32_t i = 0; i < n; i++) {
      uint32_t id = rb[1 + i];
      if (id >= patches)
         bad++;
      else
         full[id]++;
   }
   uint32_t per = instances * TRIS;
   printf("HIST %s", name);
   for (uint32_t p = 0; p < patches; p++) {
      printf(" %u", full[p]);
      if (full[p] != per)
         bad++;
   }
   printf("\n");
   free(full);
   if (bad) {
      printf("FAIL %s ids bad=%u first=%u %u %u %u\n", name, bad, rb[1], rb[2],
             rb[3], rb[4]);
      return 1;
   }
   printf("CASE %s PASS\n", name);
   return 0;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <libvulkan_panfrost.so>\n", argv[0]);
      return 2;
   }
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      return 1;
   }
   printf("PID %d\n", (int)getpid());
   FILE *stf = fopen("/proc/self/status", "r");
   if (stf) {
      char line[256];
      while (fgets(line, sizeof(line), stf))
         if (!strncmp(line, "Tgid:", 5) || !strncmp(line, "Pid:", 4) ||
             !strncmp(line, "PPid:", 5))
            fputs(line, stdout);
      fclose(stf);
   }
   printf("READY\n");
   char gate[64];
   snprintf(gate, sizeof(gate), "/tmp/088-go-%d", (int)getpid());
   for (;;) {
      if (access(gate, F_OK) == 0)
         break;
      struct timespec d = {.tv_nsec = 50000000};
      nanosleep(&d, NULL);
   }
   stamp("go");
   {
      double up = 0;
      FILE *uf = fopen("/proc/uptime", "r");
      if (uf) {
         if (fscanf(uf, "%lf", &up) == 1)
            printf("UPTIME_AT_GO %.2f\n", up);
         fclose(uf);
      }
   }

   icd_gipa_fn gipa = (icd_gipa_fn)dlsym(h, "vk_icdGetInstanceProcAddr");
   if (!gipa) {
      printf("FAIL gipa\n");
      return 1;
   }
   PFN_vkCreateInstance CreateInstance =
      (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   const char *iexts[] = {"VK_KHR_get_physical_device_properties2"};
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app,
                               .enabledExtensionCount = 1,
                               .ppEnabledExtensionNames = iexts};
   struct gpu g = {.gipa = gipa};
   CK(CreateInstance(&ici, NULL, &g.inst), "CreateInstance");

#define GI(n)                                                                  \
   PFN_vk##n n = (PFN_vk##n)gipa(g.inst, "vk" #n);                             \
   if (!n) {                                                                   \
      printf("FAIL missing vk" #n "\n");                                       \
      return 1;                                                                \
   }
   GI(EnumeratePhysicalDevices)
   GI(GetPhysicalDeviceProperties)
   GI(GetPhysicalDeviceFeatures2)
   GI(GetPhysicalDeviceQueueFamilyProperties)
   GI(GetPhysicalDeviceMemoryProperties)
   GI(CreateDevice)
   GI(GetDeviceQueue)
   GI(GetDeviceProcAddr)
   GI(CreateCommandPool)

   uint32_t nd = 0;
   CK(EnumeratePhysicalDevices(g.inst, &nd, NULL), "count");
   VkPhysicalDevice *devs = calloc(nd, sizeof(*devs));
   CK(EnumeratePhysicalDevices(g.inst, &nd, devs), "enum");
   for (uint32_t i = 0; i < nd; i++) {
      VkPhysicalDeviceProperties p;
      GetPhysicalDeviceProperties(devs[i], &p);
      printf("PHYS %u %s vendor=0x%x device=0x%x\n", i, p.deviceName, p.vendorID,
             p.deviceID);
      if (strstr(p.deviceName, "Mali"))
         g.phys = devs[i];
   }
   free(devs);
   if (!g.phys) {
      printf("FAIL no Mali\n");
      return 1;
   }

   VkPhysicalDeviceVulkan13Features v13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
   VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   .pNext = &v13};
   GetPhysicalDeviceFeatures2(g.phys, &f2);
   printf("FEATURE tess=%u geom=%u vpsa=%u sync2=%u\n",
          f2.features.tessellationShader, f2.features.geometryShader,
          f2.features.vertexPipelineStoresAndAtomics, v13.synchronization2);
   if (!f2.features.tessellationShader || !f2.features.geometryShader ||
       !f2.features.vertexPipelineStoresAndAtomics || !v13.synchronization2) {
      printf("FAIL required feature missing\n");
      return 1;
   }

   uint32_t qn = 0;
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, NULL);
   VkQueueFamilyProperties *qp = calloc(qn, sizeof(*qp));
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, qp);
   g.qi = ~0u;
   for (uint32_t i = 0; i < qn; i++)
      if ((qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
         g.qi = i;
         printf("QFAM %u flags=0x%x count=%u\n", i, qp[i].queueFlags,
                qp[i].queueCount);
         break;
      }
   free(qp);
   if (g.qi == ~0u) {
      printf("FAIL no graphics queue\n");
      return 1;
   }

    GetPhysicalDeviceMemoryProperties(g.phys, &g.mp);
    for (uint32_t i = 0; i < g.mp.memoryTypeCount; i++)
       printf("MEM %u flags=0x%x\n", i, g.mp.memoryTypes[i].propertyFlags);

   float prio = 1.f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = g.qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceFeatures ef = {0};
   ef.tessellationShader = VK_TRUE;
   ef.geometryShader = VK_TRUE;
   ef.vertexPipelineStoresAndAtomics = VK_TRUE;
   VkPhysicalDeviceVulkan13Features ev13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .pNext = &ev13,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .pEnabledFeatures = &ef};
   CK(CreateDevice(g.phys, &dci, NULL, &g.dev), "CreateDevice");
   GetDeviceQueue(g.dev, g.qi, 0, &g.q);
   g.gdpa = GetDeviceProcAddr;
   printf("FEATURES enabled tess geom vpsa sync2\n");

#define LP(n)                                                                  \
   if (proc(&g, "vk" #n, &g.n))                                                \
      return 1;
   LP(CreateBuffer) LP(GetBufferMemoryRequirements) LP(AllocateMemory)
   LP(BindBufferMemory) LP(MapMemory)
   LP(CreateImage) LP(GetImageMemoryRequirements) LP(BindImageMemory)
   LP(CreateImageView) LP(CreateRenderPass) LP(CreateFramebuffer)
   LP(CreateShaderModule) LP(CreateDescriptorSetLayout) LP(CreatePipelineLayout)
   LP(CreateGraphicsPipelines) LP(CreateDescriptorPool) LP(AllocateDescriptorSets)
   LP(UpdateDescriptorSets) LP(AllocateCommandBuffers) LP(BeginCommandBuffer)
   LP(EndCommandBuffer) LP(ResetCommandBuffer)
   LP(CmdBeginRenderPass) LP(CmdEndRenderPass)
   LP(CmdBindPipeline) LP(CmdBindDescriptorSets) LP(CmdBindVertexBuffers)
   LP(CmdDraw) LP(CmdDrawIndirect) LP(CmdPipelineBarrier)
   LP(CmdCopyBuffer) LP(CmdFillBuffer)
   LP(CreateFence) LP(WaitForFences) LP(ResetFences) LP(QueueSubmit)
#undef LP

   VkCommandPoolCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = g.qi};
   CK(CreateCommandPool(g.dev, &pci, NULL, &g.pool), "Pool");

   VkDeviceSize ssbo_sz = SSBO_WORDS * 4u;
    struct buf ssbo, rb, indirect, ichunk, vb;
    if (make_buf(&g, ssbo_sz,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 0, 1, &ssbo) ||
        make_buf(&g, ssbo_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, 0, &rb) ||
        make_buf(&g, 16, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 1, 0, &indirect) ||
        make_buf(&g, 16, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 1, 0, &ichunk))
       return 1;

   /* One triangle per patch, repeated. firstVertex selects the patch. */
   float tri[12] = {-1, -1, 0, 1, 3, -1, 0, 1, -1, 3, 0, 1};
   float *verts = calloc((size_t)CHUNK_PATCHES * 12, sizeof(float));
   if (!verts)
      return 1;
   for (uint32_t i = 0; i < CHUNK_PATCHES; i++)
      memcpy(verts + i * 12, tri, sizeof(tri));
   if (make_buf(&g, CHUNK_PATCHES * 12 * sizeof(float),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1, 0, &vb))
      return 1;
   memcpy(vb.p, verts, CHUNK_PATCHES * 12 * sizeof(float));
   free(verts);
    uint32_t ind[4] = {PATCHES * 3u, INSTANCES, 0, 0};
    memcpy(indirect.p, ind, sizeof(ind));
    uint32_t indc[4] = {CHUNK_PATCHES * 3u, CHUNK_INST, 0, 0};
    memcpy(ichunk.p, indc, sizeof(indc));

   VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = VK_FORMAT_R8G8B8A8_UNORM,
                           .extent = {W, H, 1},
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   VkImage img;
   CK(g.CreateImage(g.dev, &ii, NULL, &img), "Image");
    VkMemoryRequirements imr;
    g.GetImageMemoryRequirements(g.dev, img, &imr);
    uint32_t iti;
    if (pick_type(&g, imr.memoryTypeBits, 1, 0, &iti))
       return 1;
    if ((imr.memoryTypeBits & (1u << iti)) == 0) {
       printf("FAIL image type %u not in bits 0x%x\n", iti, imr.memoryTypeBits);
       return 1;
    }
    printf("ALLOC img type=%u bits=0x%x flags=0x%x\n", iti, imr.memoryTypeBits,
           g.mp.memoryTypes[iti].propertyFlags);
    VkMemoryAllocateInfo imi = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = imr.size,
                                .memoryTypeIndex = iti};
    VkDeviceMemory imem;
    CK(g.AllocateMemory(g.dev, &imi, NULL, &imem), "ImgMem");
   CK(g.BindImageMemory(g.dev, img, imem, 0), "BindImg");
   VkImageViewCreateInfo ivi = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   VkImageView view;
   CK(g.CreateImageView(g.dev, &ivi, NULL, &view), "View");
   VkAttachmentDescription att = {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkAttachmentReference aref = {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &aref};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                  .attachmentCount = 1,
                                  .pAttachments = &att,
                                  .subpassCount = 1,
                                  .pSubpasses = &sub};
   VkRenderPass rp;
   CK(g.CreateRenderPass(g.dev, &rpci, NULL, &rp), "RP");
   VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                   .renderPass = rp,
                                   .attachmentCount = 1,
                                   .pAttachments = &view,
                                   .width = W,
                                   .height = H,
                                   .layers = 1};
   VkFramebuffer fb;
   CK(g.CreateFramebuffer(g.dev, &fbci, NULL, &fb), "FB");

   VkDescriptorSetLayoutBinding db = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                      VK_SHADER_STAGE_GEOMETRY_BIT, NULL};
   VkDescriptorSetLayoutCreateInfo dlci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &db};
   VkDescriptorSetLayout dsl;
   CK(g.CreateDescriptorSetLayout(g.dev, &dlci, NULL, &dsl), "DSL");
   VkDescriptorPoolSize psz = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
   VkDescriptorPoolCreateInfo dpci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &psz};
   VkDescriptorPool dpool;
   CK(g.CreateDescriptorPool(g.dev, &dpci, NULL, &dpool), "DPool");
   VkDescriptorSetAllocateInfo dsai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dpool,
      .descriptorSetCount = 1,
      .pSetLayouts = &dsl};
   VkDescriptorSet set;
   CK(g.AllocateDescriptorSets(g.dev, &dsai, &set), "DS");
   VkDescriptorBufferInfo dbi = {ssbo.b, 0, ssbo_sz};
   VkWriteDescriptorSet wr = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = set,
                              .dstBinding = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pBufferInfo = &dbi};
   g.UpdateDescriptorSets(g.dev, 1, &wr, 0, NULL);

   VkShaderModule vs, tcs, tes, gs, fs;
   VkShaderModuleCreateInfo sm = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
#define MOD(dst, arr)                                                          \
   sm.codeSize = sizeof(arr);                                                  \
   sm.pCode = arr;                                                             \
   CK(g.CreateShaderModule(g.dev, &sm, NULL, &dst), #dst);
   MOD(vs, vert_spv) MOD(tcs, tesc_spv) MOD(tes, tese_spv) MOD(gs, geom_spv)
   MOD(fs, frag_spv)
#undef MOD
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &dsl};
   VkPipelineLayout pl;
   CK(g.CreatePipelineLayout(g.dev, &plci, NULL, &pl), "PL");
   VkPipelineShaderStageCreateInfo stages[5] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, .module = tcs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, .module = tes,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = gs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
   VkVertexInputBindingDescription vib = {0, 16, VK_VERTEX_INPUT_RATE_VERTEX};
   VkVertexInputAttributeDescription via = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vib,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &via};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST};
   VkPipelineTessellationStateCreateInfo ts = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .patchControlPoints = 3};
   VkViewport vp = {0, 0, W, H, 0, 1};
   VkRect2D sc = {{0, 0}, {W, H}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &cba};
   VkGraphicsPipelineCreateInfo gp = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 5,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pTessellationState = &ts,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .layout = pl,
      .renderPass = rp};
   VkPipeline pipe;
   CK(g.CreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &gp, NULL, &pipe), "Pipe");

   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = g.pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
    VkCommandBuffer cmd, icmd, chunk, ichunk_cmd, copy;
    CK(g.AllocateCommandBuffers(g.dev, &cai, &cmd), "Cmd");
    CK(g.AllocateCommandBuffers(g.dev, &cai, &icmd), "ICmd");
    CK(g.AllocateCommandBuffers(g.dev, &cai, &chunk), "Chunk");
    CK(g.AllocateCommandBuffers(g.dev, &cai, &ichunk_cmd), "IChunk");
    CK(g.AllocateCommandBuffers(g.dev, &cai, &copy), "Copy");

   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

   /* HOST|TRANSFER -> GS is outside the render pass (VUID-01178). */
   #define PREP(cbuf)                                                          \
      do {                                                                     \
         VkBufferMemoryBarrier hb = {                                          \
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                  \
            .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT |                        \
                             VK_ACCESS_TRANSFER_WRITE_BIT,                     \
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |                       \
                             VK_ACCESS_SHADER_WRITE_BIT,                       \
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,                    \
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,                    \
            .buffer = ssbo.b,                                                  \
            .size = ssbo_sz};                                                  \
         g.CmdPipelineBarrier(cbuf,                                            \
                              VK_PIPELINE_STAGE_HOST_BIT |                     \
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,               \
                              VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, 0, 0,     \
                              NULL, 1, &hb, 0, NULL);                          \
      } while (0)

    #define DRAW_RP(cbuf, ibuf, patches, instances)                             \
       do {                                                                     \
          VkClearValue cv = {.color = {{0.f, 0.f, 0.f, 1.f}}};                  \
          VkRenderPassBeginInfo rbi = {                                         \
             .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                 \
             .renderPass = rp,                                                  \
             .framebuffer = fb,                                                 \
             .renderArea = {{0, 0}, {W, H}},                                    \
             .clearValueCount = 1,                                              \
             .pClearValues = &cv};                                              \
          g.CmdBeginRenderPass(cbuf, &rbi, VK_SUBPASS_CONTENTS_INLINE);         \
          g.CmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);       \
          g.CmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, \
                                  1, &set, 0, NULL);                            \
          VkDeviceSize off = 0;                                                 \
          g.CmdBindVertexBuffers(cbuf, 0, 1, &vb.b, &off);                      \
          if ((ibuf) != VK_NULL_HANDLE)                                         \
             g.CmdDrawIndirect(cbuf, (ibuf), 0, 1, 0);                          \
          else                                                                  \
             g.CmdDraw(cbuf, (patches) * 3u, (instances), 0, 0);                \
          g.CmdEndRenderPass(cbuf);                                             \
          gs_to_copy(&g, cbuf, ssbo.b, ssbo_sz);                                \
       } while (0)

    CK(g.BeginCommandBuffer(cmd, &bbi), "Begin");
    PREP(cmd);
    DRAW_RP(cmd, VK_NULL_HANDLE, PATCHES, INSTANCES);
    CK(g.EndCommandBuffer(cmd), "End");

    CK(g.BeginCommandBuffer(icmd, &bbi), "BeginI");
    PREP(icmd);
    DRAW_RP(icmd, indirect.b, PATCHES, INSTANCES);
    CK(g.EndCommandBuffer(icmd), "EndI");

    CK(g.BeginCommandBuffer(chunk, &bbi), "BeginC");
    PREP(chunk);
    DRAW_RP(chunk, VK_NULL_HANDLE, CHUNK_PATCHES, CHUNK_INST);
    CK(g.EndCommandBuffer(chunk), "EndC");

    CK(g.BeginCommandBuffer(ichunk_cmd, &bbi), "BeginIC");
    PREP(ichunk_cmd);
    DRAW_RP(ichunk_cmd, ichunk.b, CHUNK_PATCHES, CHUNK_INST);
    CK(g.EndCommandBuffer(ichunk_cmd), "EndIC");
#undef PREP
#undef DRAW_RP

   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(g.CreateFence(g.dev, &fci, NULL, &fence), "Fence");
   VkCommandBufferBeginInfo cbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

   struct spec {
      VkCommandBuffer c;
      const char *name;
      uint32_t patches, instances;
   } runs[] = {
       {cmd, "direct", PATCHES, INSTANCES},
       {cmd, "direct-replay", PATCHES, INSTANCES},
       {icmd, "indirect", PATCHES, INSTANCES},
       {icmd, "indirect-replay", PATCHES, INSTANCES},
       {chunk, "chunk", CHUNK_PATCHES, CHUNK_INST},
       {chunk, "chunk-replay", CHUNK_PATCHES, CHUNK_INST},
       {ichunk_cmd, "indirect-chunk", CHUNK_PATCHES, CHUNK_INST},
    };
   int failed = 0;
   for (unsigned r = 0; r < sizeof(runs) / sizeof(runs[0]); r++) {
      CK(g.ResetCommandBuffer(copy, 0), "ResetZero");
      CK(g.BeginCommandBuffer(copy, &cbi), "BeginZero");
      g.CmdFillBuffer(copy, ssbo.b, 0, ssbo_sz, 0);
      CK(g.EndCommandBuffer(copy), "EndZero");
      CK(g.ResetFences(g.dev, 1, &fence), "ResetFZ");
      VkSubmitInfo sz = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1, .pCommandBuffers = &copy};
      CK(g.QueueSubmit(g.q, 1, &sz, fence), "SubmitZero");
      int zr = wait_fence(&g, fence, now_s(), "zero");
      if (zr)
         return zr;

      CK(g.ResetFences(g.dev, 1, &fence), "ResetF");
      VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &runs[r].c};
      double t0 = now_s();
      stamp(runs[r].name);
      CK(g.QueueSubmit(g.q, 1, &si, fence), "Submit");
      int wr = wait_fence(&g, fence, t0, runs[r].name);
      if (wr)
         return wr;

      CK(g.ResetCommandBuffer(copy, 0), "ResetCopy");
      CK(g.BeginCommandBuffer(copy, &cbi), "BeginCopy");
      /* Draw cmdbuf already did GS-write -> transfer-read. Copy reads it.
       * Same queue, submission order: the draw fence already waited. */
      VkBufferCopy bc = {.size = ssbo_sz};
      g.CmdCopyBuffer(copy, ssbo.b, rb.b, 1, &bc);
      VkBufferMemoryBarrier hb = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = rb.b,
         .size = ssbo_sz};
      g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &hb, 0,
                           NULL);
      CK(g.EndCommandBuffer(copy), "EndCopy");
      CK(g.ResetFences(g.dev, 1, &fence), "ResetF2");
      VkSubmitInfo sc = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1, .pCommandBuffers = &copy};
      CK(g.QueueSubmit(g.q, 1, &sc, fence), "SubmitCopy");
      wr = wait_fence(&g, fence, t0, "copy");
      if (wr)
         return wr;
      printf("RAW n=%u id0=%u id1=%u id2=%u id3=%u\n", rb.p ? ((uint32_t *)rb.p)[0] : 0,
             ((uint32_t *)rb.p)[1], ((uint32_t *)rb.p)[2], ((uint32_t *)rb.p)[3],
             ((uint32_t *)rb.p)[4]);
      if (check_ids(runs[r].name, rb.p, runs[r].patches, runs[r].instances))
         failed = 1;
   }
   printf("RESULT %s\n", failed ? "FAIL" : "PASS");
   return failed ? 1 : 0;
}
