/* Gate 087 device regression: tessellation conditional rendering.
 *
 * One SIMULTANEOUS_USE primary records, inside one render pass:
 *   begin-cond, 2 patch draws, end-cond, 1 unconditional patch draw.
 * Each draw is one 3-vertex patch. TCS does atomicAdd(+1) per invocation.
 * Two conditional draws plus one unconditional draw: skip counts 3,
 * pass counts 9. TES emits one triangle (outer level 1).
 * The same command buffer is submitted twice: predicate 0 then 1, then
 * again with the inverted flag (separate recording). Indirect is one
 * VkDrawIndirectCommand of the same patch. A secondary that inherits
 * conditional rendering records the same two draws and is executed from
 * a primary that begins conditional rendering first.
 *
 * Features enabled explicitly: tessellationShader,
 * vertexPipelineStoresAndAtomics (queried; test fails if absent),
 * conditionalRendering + inheritedConditionalRendering,
 * synchronization2. Predicate buffer is host-written, then a HOST ->
 * CONDITIONAL_RENDERING barrier, before submit.
 *
 * usage: tess-conditional-regression <libvulkan_panfrost.so>
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

#define W 32
#define H 32
#define PATCHES 2

static const uint32_t vs_spv[] = {
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
static const uint32_t tcs_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x0000003cu, 0x00000000u, 0x00020011u, 0x00000003u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x000a000fu, 0x00000001u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000fu, 0x00000012u, 0x00000019u,
   0x0000002fu, 0x00000036u, 0x00040010u, 0x00000004u, 0x0000001au, 0x00000003u, 0x00030003u, 0x00000002u,
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
   0x0065636eu, 0x00040005u, 0x00000019u, 0x695f6c67u, 0x0000006eu, 0x00030005u, 0x00000020u, 0x00000043u,
   0x00040006u, 0x00000020u, 0x00000000u, 0x00000076u, 0x00030005u, 0x00000022u, 0x00000063u, 0x00070005u,
   0x0000002fu, 0x545f6c67u, 0x4c737365u, 0x6c657665u, 0x656e6e49u, 0x00000072u, 0x00070005u, 0x00000036u,
   0x545f6c67u, 0x4c737365u, 0x6c657665u, 0x6574754fu, 0x00000072u, 0x00030047u, 0x0000000bu, 0x00000002u,
   0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x0000000bu, 0x00000001u,
   0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u,
   0x0000000bu, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00040047u, 0x00000012u, 0x0000000bu, 0x00000008u,
   0x00030047u, 0x00000015u, 0x00000002u, 0x00050048u, 0x00000015u, 0x00000000u, 0x0000000bu, 0x00000000u,
   0x00050048u, 0x00000015u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x00000015u, 0x00000002u,
   0x0000000bu, 0x00000003u, 0x00050048u, 0x00000015u, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00030047u,
   0x00000020u, 0x00000003u, 0x00050048u, 0x00000020u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00040047u,
   0x00000022u, 0x00000021u, 0x00000000u, 0x00040047u, 0x00000022u, 0x00000022u, 0x00000000u, 0x00040047u,
   0x0000002fu, 0x0000000bu, 0x0000000cu, 0x00030047u, 0x0000002fu, 0x0000000fu, 0x00040047u, 0x00000036u,
   0x0000000bu, 0x0000000bu, 0x00030047u, 0x00000036u, 0x0000000fu, 0x00020013u, 0x00000002u, 0x00030021u,
   0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u,
   0x00000004u, 0x00040015u, 0x00000008u, 0x00000020u, 0x00000000u, 0x0004002bu, 0x00000008u, 0x00000009u,
   0x00000001u, 0x0004001cu, 0x0000000au, 0x00000006u, 0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u,
   0x00000006u, 0x0000000au, 0x0000000au, 0x0004002bu, 0x00000008u, 0x0000000cu, 0x00000003u, 0x0004001cu,
   0x0000000du, 0x0000000bu, 0x0000000cu, 0x00040020u, 0x0000000eu, 0x00000003u, 0x0000000du, 0x0004003bu,
   0x0000000eu, 0x0000000fu, 0x00000003u, 0x00040015u, 0x00000010u, 0x00000020u, 0x00000001u, 0x00040020u,
   0x00000011u, 0x00000001u, 0x00000010u, 0x0004003bu, 0x00000011u, 0x00000012u, 0x00000001u, 0x0004002bu,
   0x00000010u, 0x00000014u, 0x00000000u, 0x0006001eu, 0x00000015u, 0x00000007u, 0x00000006u, 0x0000000au,
   0x0000000au, 0x0004002bu, 0x00000008u, 0x00000016u, 0x00000020u, 0x0004001cu, 0x00000017u, 0x00000015u,
   0x00000016u, 0x00040020u, 0x00000018u, 0x00000001u, 0x00000017u, 0x0004003bu, 0x00000018u, 0x00000019u,
   0x00000001u, 0x00040020u, 0x0000001bu, 0x00000001u, 0x00000007u, 0x00040020u, 0x0000001eu, 0x00000003u,
   0x00000007u, 0x0003001eu, 0x00000020u, 0x00000008u, 0x00040020u, 0x00000021u, 0x00000002u, 0x00000020u,
   0x0004003bu, 0x00000021u, 0x00000022u, 0x00000002u, 0x00040020u, 0x00000023u, 0x00000002u, 0x00000008u,
   0x0004002bu, 0x00000008u, 0x00000025u, 0x00000000u, 0x00020014u, 0x00000028u, 0x0004002bu, 0x00000008u,
   0x0000002cu, 0x00000002u, 0x0004001cu, 0x0000002du, 0x00000006u, 0x0000002cu, 0x00040020u, 0x0000002eu,
   0x00000003u, 0x0000002du, 0x0004003bu, 0x0000002eu, 0x0000002fu, 0x00000003u, 0x0004002bu, 0x00000006u,
   0x00000030u, 0x3f800000u, 0x00040020u, 0x00000031u, 0x00000003u, 0x00000006u, 0x0004002bu, 0x00000008u,
   0x00000033u, 0x00000004u, 0x0004001cu, 0x00000034u, 0x00000006u, 0x00000033u, 0x00040020u, 0x00000035u,
   0x00000003u, 0x00000034u, 0x0004003bu, 0x00000035u, 0x00000036u, 0x00000003u, 0x0004002bu, 0x00000010u,
   0x00000038u, 0x00000001u, 0x0004002bu, 0x00000010u, 0x0000003au, 0x00000002u, 0x00050036u, 0x00000002u,
   0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003du, 0x00000010u, 0x00000013u,
   0x00000012u, 0x0004003du, 0x00000010u, 0x0000001au, 0x00000012u, 0x00060041u, 0x0000001bu, 0x0000001cu,
   0x00000019u, 0x0000001au, 0x00000014u, 0x0004003du, 0x00000007u, 0x0000001du, 0x0000001cu, 0x00060041u,
   0x0000001eu, 0x0000001fu, 0x0000000fu, 0x00000013u, 0x00000014u, 0x0003003eu, 0x0000001fu, 0x0000001du,
   0x00050041u, 0x00000023u, 0x00000024u, 0x00000022u, 0x00000014u, 0x000700eau, 0x00000008u, 0x00000026u,
   0x00000024u, 0x00000009u, 0x00000025u, 0x00000009u, 0x0004003du, 0x00000010u, 0x00000027u, 0x00000012u,
   0x000500aau, 0x00000028u, 0x00000029u, 0x00000027u, 0x00000014u, 0x000300f7u, 0x0000002bu, 0x00000000u,
   0x000400fau, 0x00000029u, 0x0000002au, 0x0000002bu, 0x000200f8u, 0x0000002au, 0x00050041u, 0x00000031u,
   0x00000032u, 0x0000002fu, 0x00000014u, 0x0003003eu, 0x00000032u, 0x00000030u, 0x00050041u, 0x00000031u,
   0x00000037u, 0x00000036u, 0x00000014u, 0x0003003eu, 0x00000037u, 0x00000030u, 0x00050041u, 0x00000031u,
   0x00000039u, 0x00000036u, 0x00000038u, 0x0003003eu, 0x00000039u, 0x00000030u, 0x00050041u, 0x00000031u,
   0x0000003bu, 0x00000036u, 0x0000003au, 0x0003003eu, 0x0000003bu, 0x00000030u, 0x000200f9u, 0x0000002bu,
   0x000200f8u, 0x0000002bu, 0x000100fdu, 0x00010038u,
};
static const uint32_t tes_spv[] = {
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
static const uint32_t fs_spv[] = {
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
   uint32_t qi, mem_host, mem_local;
   VkCommandPool pool;
   PFN_vkGetDeviceProcAddr gdpa;
   PFN_vkCreateBuffer CreateBuffer;
   PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkBindBufferMemory BindBufferMemory;
   PFN_vkMapMemory MapMemory;
   PFN_vkGetBufferDeviceAddress GetBufferDeviceAddress;
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
   PFN_vkCmdBeginRendering CmdBeginRendering;
   PFN_vkCmdEndRendering CmdEndRendering;
   PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
   PFN_vkCmdEndRenderPass CmdEndRenderPass;
   PFN_vkCmdBindPipeline CmdBindPipeline;
   PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
   PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
   PFN_vkCmdDraw CmdDraw;
   PFN_vkCmdDrawIndirect CmdDrawIndirect;
   PFN_vkCmdBeginConditionalRenderingEXT CmdBeginConditionalRenderingEXT;
   PFN_vkCmdEndConditionalRenderingEXT CmdEndConditionalRenderingEXT;
   PFN_vkCmdExecuteCommands CmdExecuteCommands;
   PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
   PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
   PFN_vkCmdPipelineBarrier2 CmdPipelineBarrier2;
   PFN_vkCmdCopyBuffer CmdCopyBuffer;
   PFN_vkCmdFillBuffer CmdFillBuffer;
   PFN_vkCreateFence CreateFence;
   PFN_vkWaitForFences WaitForFences;
   PFN_vkResetFences ResetFences;
   PFN_vkQueueSubmit QueueSubmit;
   PFN_vkCreateQueryPool CreateQueryPool;
   PFN_vkCmdResetQueryPool CmdResetQueryPool;
   PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
   PFN_vkGetQueryPoolResults GetQueryPoolResults;
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
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex =
                                  local ? g->mem_local : g->mem_host};
   if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
      VkMemoryAllocateFlagsInfo fi = {
         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
         .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
      mai.pNext = &fi;
      CK(g->AllocateMemory(g->dev, &mai, NULL, &o->m), "AllocateMemory");
   } else {
      CK(g->AllocateMemory(g->dev, &mai, NULL, &o->m), "AllocateMemory");
   }
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
   for (int k = 0; k < 20 && wr == VK_TIMEOUT; k++)
      wr = g->WaitForFences(g->dev, 1, &f, VK_TRUE, 500000000ull);
   printf("FENCE %s r=%d dt=%.3f\n", what, (int)wr, now_s() - t0);
   if (wr == VK_TIMEOUT)
      return 3;
   return wr == VK_SUCCESS ? 0 : 1;
}

static void
host_to_cond(struct gpu *g, VkCommandBuffer cmd, VkBuffer b)
{
   VkBufferMemoryBarrier bb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = 4};
   g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT, 0, 0,
                         NULL, 1, &bb, 0, NULL);
}

/* Counter is device-local. Make the atomic visible to the later copy. */
static void
shader_to_copy(struct gpu *g, VkCommandBuffer cmd, VkBuffer b)
{
   VkBufferMemoryBarrier bb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = 4};
   g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &bb, 0,
                         NULL);
}

struct draw_env {
   struct gpu *g;
   VkRenderPass rp;
   VkFramebuffer fb;
   VkImage img;
   VkPipeline pipe;
   VkPipeline tri;
   VkPipelineLayout pl;
   VkDescriptorSet set;
   VkBuffer vb;
   VkBuffer pred;
   VkBuffer counter;
   VkBuffer indirect;
   VkQueryPool ts;
   int inverted;
   int use_indirect;
};

/* pred, then PATCHES conditional draws, then one unconditional draw.
 * counter is zeroed by the host before submit; TCS only adds. */
static int
record_draws(struct draw_env *e, VkCommandBuffer cmd, int secondary)
{
   struct gpu *g = e->g;
    if (!secondary) {
      VkBufferMemoryBarrier hb = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = e->counter,
         .size = 4};
      /* VUID-vkCmdPipelineBarrier-srcStageMask-01178: HOST is illegal
       * inside a render pass. */
      g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, 0,
                            0, NULL, 1, &hb, 0, NULL);
      VkClearValue cv = {.color = {{1.f, 0.f, 0.f, 1.f}}};
      VkRenderPassBeginInfo rbi = {
         .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
         .renderPass = e->rp,
         .framebuffer = e->fb,
         .renderArea = {{0, 0}, {W, H}},
         .clearValueCount = 1,
         .pClearValues = &cv};
      g->CmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
      if (e->ts)
         g->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, e->ts, 0);
   }
   g->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e->pipe);
   g->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e->pl, 0, 1,
                            &e->set, 0, NULL);
   VkDeviceSize off = 0;
   g->CmdBindVertexBuffers(cmd, 0, 1, &e->vb, &off);
   VkViewport vp = {0, 0, W, H, 0, 1};
   VkRect2D sc = {{0, 0}, {W, H}};
   /* Viewport/scissor are dynamic so a secondary inside an inherited
    * pass can set them. */
   PFN_vkCmdSetViewport CmdSetViewport =
      (PFN_vkCmdSetViewport)g->gdpa(g->dev, "vkCmdSetViewport");
   PFN_vkCmdSetScissor CmdSetScissor =
      (PFN_vkCmdSetScissor)g->gdpa(g->dev, "vkCmdSetScissor");
   if (!CmdSetViewport || !CmdSetScissor) {
      printf("FAIL missing set viewport/scissor\n");
      return 1;
   }
    CmdSetViewport(cmd, 0, 1, &vp);
   CmdSetScissor(cmd, 0, 1, &sc);

   if (!secondary) {
      VkConditionalRenderingBeginInfoEXT cri = {
         .sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
         .buffer = e->pred,
         .offset = 0,
         .flags = e->inverted ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0};
      g->CmdBeginConditionalRenderingEXT(cmd, &cri);
   }
   for (int i = 0; i < PATCHES; i++) {
      if (e->use_indirect)
         g->CmdDrawIndirect(cmd, e->indirect, 0, 1, 0);
      else
         g->CmdDraw(cmd, 3, 1, (uint32_t)i * 3, 0);
   }
   if (!secondary)
      g->CmdEndConditionalRenderingEXT(cmd);
   /* Unconditional: scratch/replay must still tessellate after a skip. */
   if (!secondary)
      g->CmdDraw(cmd, 3, 1, 0, 0);
   if (!secondary) {
      /* Plain triangle after the tess draws. No predicate. */
      PFN_vkCmdBindPipeline Bind =
         (PFN_vkCmdBindPipeline)g->gdpa(g->dev, "vkCmdBindPipeline");
      /* tri_pipe is stashed in the high bit of nothing: passed via env. */
      if (e->tri)
         Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e->tri);
      g->CmdDraw(cmd, 3, 1, 0, 0);
      if (e->ts)
         g->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, e->ts,
                              1);
      g->CmdEndRenderPass(cmd);
   }
   return 0;
}

static int
red_count(const uint8_t *px)
{
   int red = 0;
   for (int i = 0; i < W * H; i++) {
      const uint8_t *p = px + i * 4;
      if (p[0] > 200 && p[1] < 40 && p[2] < 40)
         red++;
   }
   return red;
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
   /* Poll a file the runner creates only after it has stamped maps.
    * A fifo lost the race: open returned before the stamp. */
   char gate[64];
   snprintf(gate, sizeof(gate), "/tmp/087-go-%d", (int)getpid());
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
   GI(EnumerateDeviceExtensionProperties)

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
    VkPhysicalDeviceVulkan12Features v12 = {
       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
       .pNext = &v13};
    VkPhysicalDeviceConditionalRenderingFeaturesEXT cr = {
       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,
       .pNext = &v12};
    VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                    .pNext = &cr};
    GetPhysicalDeviceFeatures2(g.phys, &f2);
    printf("FEATURE tess=%u vpsa=%u cond=%u inherited=%u sync2=%u bda=%u\n",
           f2.features.tessellationShader, f2.features.vertexPipelineStoresAndAtomics,
           cr.conditionalRendering, cr.inheritedConditionalRendering,
           v13.synchronization2, v12.bufferDeviceAddress);
    if (!f2.features.tessellationShader ||
        !f2.features.vertexPipelineStoresAndAtomics || !cr.conditionalRendering ||
        !cr.inheritedConditionalRendering || !v13.synchronization2 ||
        !v12.bufferDeviceAddress) {
       printf("FAIL required feature missing\n");
       return 1;
    }
    int have_vpsa = 1;

   uint32_t qn = 0;
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, NULL);
   VkQueueFamilyProperties *qp = calloc(qn, sizeof(*qp));
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, qp);
   g.qi = ~0u;
   for (uint32_t i = 0; i < qn; i++)
      if ((qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
         g.qi = i;
         printf("QFAM %u flags=0x%x count=%u ts=%u\n", i, qp[i].queueFlags,
                qp[i].queueCount, qp[i].timestampValidBits);
         break;
      }
   free(qp);
   if (g.qi == ~0u) {
      printf("FAIL no graphics queue\n");
      return 1;
   }

   VkPhysicalDeviceMemoryProperties mp;
   GetPhysicalDeviceMemoryProperties(g.phys, &mp);
   g.mem_host = g.mem_local = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
      printf("MEM %u flags=0x%x\n", i, f);
      if (g.mem_host == ~0u && (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
          (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         g.mem_host = i;
      if ((f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
          !(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
         g.mem_local = i;
   }
   if (g.mem_host == ~0u) {
      printf("FAIL no host-coherent memory\n");
      return 1;
   }
   if (g.mem_local == ~0u)
      g.mem_local = g.mem_host;
   printf("MEM host=%u local=%u\n", g.mem_host, g.mem_local);

   float prio = 1.f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = g.qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceFeatures ef = {0};
   ef.tessellationShader = VK_TRUE;
   ef.vertexPipelineStoresAndAtomics = have_vpsa ? VK_TRUE : VK_FALSE;
    VkPhysicalDeviceVulkan12Features ev12 = {
       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
       .bufferDeviceAddress = VK_TRUE};
    /* synchronization2 is the Vulkan 1.3 feature. A separate
     * VkPhysicalDeviceSynchronization2Features next to it is VUID-06532. */
    VkPhysicalDeviceVulkan13Features ev13 = {
       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
       .pNext = &ev12,
       .synchronization2 = VK_TRUE};
    VkPhysicalDeviceConditionalRenderingFeaturesEXT ecr = {
       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,
       .pNext = &ev13,
       .conditionalRendering = VK_TRUE,
       .inheritedConditionalRendering = VK_TRUE};
    const char *dexts[] = {"VK_EXT_conditional_rendering",
                           "VK_KHR_buffer_device_address"};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                              .pNext = &ecr,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                              .enabledExtensionCount = 2,
                             .ppEnabledExtensionNames = dexts,
                             .pEnabledFeatures = &ef};
   CK(CreateDevice(g.phys, &dci, NULL, &g.dev), "CreateDevice");
   GetDeviceQueue(g.dev, g.qi, 0, &g.q);
   g.gdpa = GetDeviceProcAddr;
    printf("FEATURES enabled tess vpsa=%d cond inherited sync2 bda\n", have_vpsa);

#define LP(n)                                                                  \
   if (proc(&g, "vk" #n, &g.n))                                                \
      return 1;
   LP(CreateBuffer) LP(GetBufferMemoryRequirements) LP(AllocateMemory)
   LP(BindBufferMemory) LP(MapMemory) LP(GetBufferDeviceAddress)
   LP(CreateImage) LP(GetImageMemoryRequirements) LP(BindImageMemory)
   LP(CreateImageView) LP(CreateRenderPass) LP(CreateFramebuffer)
   LP(CreateShaderModule) LP(CreateDescriptorSetLayout) LP(CreatePipelineLayout)
   LP(CreateGraphicsPipelines) LP(CreateDescriptorPool) LP(AllocateDescriptorSets)
   LP(UpdateDescriptorSets)    LP(AllocateCommandBuffers) LP(BeginCommandBuffer) LP(EndCommandBuffer)
   LP(ResetCommandBuffer) LP(CmdBeginRendering) LP(CmdEndRendering)
   LP(CmdBeginRenderPass) LP(CmdEndRenderPass)
   LP(CmdBindPipeline) LP(CmdBindDescriptorSets) LP(CmdBindVertexBuffers)
   LP(CmdDraw) LP(CmdDrawIndirect) LP(CmdBeginConditionalRenderingEXT)
   LP(CmdEndConditionalRenderingEXT) LP(CmdExecuteCommands) LP(CmdPipelineBarrier)
   LP(CmdCopyImageToBuffer) LP(CmdPipelineBarrier2) LP(CmdCopyBuffer) LP(CmdFillBuffer)
   LP(CreateFence) LP(WaitForFences) LP(ResetFences)
   LP(QueueSubmit) LP(CreateQueryPool) LP(CmdResetQueryPool) LP(CmdWriteTimestamp)
   LP(GetQueryPoolResults)
#undef LP

   VkCommandPoolCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = g.qi};
   CK(CreateCommandPool(g.dev, &pci, NULL, &g.pool), "Pool");

    struct buf pred, counter, counter_rb, indirect, rb, vb;
   if (make_buf(&g, 4,
                VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                1, 0, &pred) ||
       make_buf(&g, 4,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                0, 1, &counter) ||
       make_buf(&g, 4,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, 0, &counter_rb) ||
       make_buf(&g, 16, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 1, 0, &indirect) ||
       make_buf(&g, W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, 0, &rb))
      return 1;
   /* Three patches, each a fullscreen triangle. Same 3 vertices repeated
    * so firstVertex selects a patch. */
   float verts[9 * 4] = {
      -1, -1, 0, 1,  3, -1, 0, 1,  -1, 3, 0, 1,
      -1, -1, 0, 1,  3, -1, 0, 1,  -1, 3, 0, 1,
      -1, -1, 0, 1,  3, -1, 0, 1,  -1, 3, 0, 1,
   };
    if (make_buf(&g, sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1, 0, &vb))
      return 1;
   memcpy(vb.p, verts, sizeof(verts));
   uint32_t ind[4] = {3, 1, 0, 0};
   memcpy(indirect.p, ind, sizeof(ind));
   VkBufferDeviceAddressInfo bai = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pred.b};
   printf("PRED_ADDR 0x%llx\n",
          (unsigned long long)g.GetBufferDeviceAddress(g.dev, &bai));

   VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = VK_FORMAT_R8G8B8A8_UNORM,
                           .extent = {W, H, 1},
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   VkImage img;
   CK(g.CreateImage(g.dev, &ii, NULL, &img), "Image");
   VkMemoryRequirements imr;
   g.GetImageMemoryRequirements(g.dev, img, &imr);
   VkMemoryAllocateInfo imi = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = imr.size,
                               .memoryTypeIndex = g.mem_host};
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
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   VkAttachmentReference aref = {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &aref};
   VkSubpassDependency dep = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                  .attachmentCount = 1,
                                  .pAttachments = &att,
                                  .subpassCount = 1,
                                  .pSubpasses = &sub,
                                  .dependencyCount = 1,
                                  .pDependencies = &dep};
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
                                      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                      NULL};
   VkDescriptorSetLayoutCreateInfo dlci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = have_vpsa ? 1 : 0,
      .pBindings = &db};
   VkDescriptorSetLayout dsl;
   CK(g.CreateDescriptorSetLayout(g.dev, &dlci, NULL, &dsl), "DSL");
   VkDescriptorPoolSize psz = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
   VkDescriptorPoolCreateInfo dpci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 2,
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
   if (have_vpsa) {
      VkDescriptorBufferInfo bi = {counter.b, 0, 4};
      VkWriteDescriptorSet wr = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                 .dstSet = set,
                                 .dstBinding = 0,
                                 .descriptorCount = 1,
                                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                 .pBufferInfo = &bi};
      g.UpdateDescriptorSets(g.dev, 1, &wr, 0, NULL);
   }

   VkShaderModule vs, tcs, tes, fs;
   VkShaderModuleCreateInfo sm = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
#define MOD(dst, arr)                                                          \
   sm.codeSize = sizeof(arr);                                                  \
   sm.pCode = arr;                                                             \
   CK(g.CreateShaderModule(g.dev, &sm, NULL, &dst), #dst);
   MOD(vs, vs_spv) MOD(tcs, tcs_spv) MOD(tes, tes_spv) MOD(fs, fs_spv)
#undef MOD
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &dsl};
   VkPipelineLayout pl;
   CK(g.CreatePipelineLayout(g.dev, &plci, NULL, &pl), "PL");
   VkPipelineShaderStageCreateInfo stages[4] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
       .module = tcs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
       .module = tes,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fs,
       .pName = "main"}};
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
      .viewportCount = 1,
      .pViewports = &vp,
      .scissorCount = 1,
      .pScissors = &sc};
   VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo dsi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn};
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
      .attachmentCount = 1,
      .pAttachments = &cba};
   VkGraphicsPipelineCreateInfo gp = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 4,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pTessellationState = &ts,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .pDynamicState = &dsi,
      .layout = pl,
      .renderPass = rp};
   VkPipeline pipe;
   CK(g.CreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &gp, NULL, &pipe), "Pipe");

   /* Same VS+FS, no tessellation. One triangle, recorded in the same
    * command buffer, so a black replay is the command buffer and a red
    * replay with a zero counter is the tessellation loop. */
   VkPipelineShaderStageCreateInfo tri_st[2] = {stages[0], stages[3]};
   VkPipelineInputAssemblyStateCreateInfo tri_ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkGraphicsPipelineCreateInfo tgp = gp;
   tgp.stageCount = 2;
   tgp.pStages = tri_st;
   tgp.pInputAssemblyState = &tri_ia;
   tgp.pTessellationState = NULL;
   VkPipeline tri_pipe;
   CK(g.CreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &tgp, NULL, &tri_pipe),
      "TriPipe");

   VkQueryPoolCreateInfo qpi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                .queryCount = 2};
   VkQueryPool tspool;
   CK(g.CreateQueryPool(g.dev, &qpi, NULL, &tspool), "TS");

   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = g.pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   VkCommandBuffer cmd, copy, icmd, psec_cmd;
   CK(g.AllocateCommandBuffers(g.dev, &cai, &cmd), "Cmd");
   CK(g.AllocateCommandBuffers(g.dev, &cai, &copy), "Copy");
   CK(g.AllocateCommandBuffers(g.dev, &cai, &icmd), "ICmd");
   CK(g.AllocateCommandBuffers(g.dev, &cai, &psec_cmd), "PSec");

    struct draw_env env = {.g = &g,
                           .rp = rp,
                           .fb = fb,
                           .img = img,
                           .pipe = pipe,
                          .tri = tri_pipe,
                          .pl = pl,
                          .set = set,
                          .vb = vb.b,
                          .pred = pred.b,
                          .counter = counter.b,
                          .indirect = indirect.b,
                          .ts = tspool,
                          .inverted = 0,
                          .use_indirect = 0};
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
    CK(g.BeginCommandBuffer(cmd, &bbi), "Begin");
   g.CmdResetQueryPool(cmd, tspool, 0, 2);
   host_to_cond(&g, cmd, pred.b);
   if (record_draws(&env, cmd, 0))
      return 1;
   shader_to_copy(&g, cmd, counter.b);
   CK(g.EndCommandBuffer(cmd), "End");

   env.use_indirect = 1;
   CK(g.BeginCommandBuffer(icmd, &bbi), "BeginI");
   host_to_cond(&g, icmd, pred.b);
   if (record_draws(&env, icmd, 0))
      return 1;
   shader_to_copy(&g, icmd, counter.b);
   CK(g.EndCommandBuffer(icmd), "EndI");

   /* Inverted recording is a different command buffer: the flag is
    * record-time state. Same predicate buffer, opposite sense. */
   VkCommandBuffer inv;
   CK(g.AllocateCommandBuffers(g.dev, &cai, &inv), "Inv");
   env.use_indirect = 0;
   env.inverted = 1;
   env.ts = VK_NULL_HANDLE;
   CK(g.BeginCommandBuffer(inv, &bbi), "BeginInv");
   host_to_cond(&g, inv, pred.b);
   if (record_draws(&env, inv, 0))
      return 1;
   shader_to_copy(&g, inv, counter.b);
   CK(g.EndCommandBuffer(inv), "EndInv");

   /* Secondary inherits the predicate. Primary begins conditional
    * rendering, executes the secondary (2 draws), ends, then the
    * unconditional draw so a skip cannot deadlock the next tess. */
   VkCommandBuffer sec;
   VkCommandBufferAllocateInfo sai = cai;
   sai.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
   CK(g.AllocateCommandBuffers(g.dev, &sai, &sec), "Sec");
   VkCommandBufferInheritanceConditionalRenderingInfoEXT ihc = {
      .sType =
         VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,
      .conditionalRenderingEnable = VK_TRUE};
    VkCommandBufferInheritanceInfo ihi = {
       .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
       .pNext = &ihc,
       .renderPass = rp,
       .subpass = 0,
       .framebuffer = fb};
   VkCommandBufferBeginInfo sbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT |
               VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
      .pInheritanceInfo = &ihi};
   env.inverted = 0;
   env.ts = VK_NULL_HANDLE;
   CK(g.BeginCommandBuffer(sec, &sbi), "BeginSec");
   if (record_draws(&env, sec, 1))
      return 1;
   CK(g.EndCommandBuffer(sec), "EndSec");

    CK(g.BeginCommandBuffer(psec_cmd, &bbi), "BeginP");
   host_to_cond(&g, psec_cmd, pred.b);
   {
      VkBufferMemoryBarrier hb = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = counter.b,
         .size = 4};
      g.CmdPipelineBarrier(psec_cmd, VK_PIPELINE_STAGE_HOST_BIT |
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, 0,
                           0, NULL, 1, &hb, 0, NULL);
   }
   VkClearValue pcv = {.color = {{1.f, 0.f, 0.f, 1.f}}};
   VkRenderPassBeginInfo prbi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = rp,
      .framebuffer = fb,
      .renderArea = {{0, 0}, {W, H}},
      .clearValueCount = 1,
      .pClearValues = &pcv};
   g.CmdBeginRenderPass(psec_cmd, &prbi,
                        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
   VkConditionalRenderingBeginInfoEXT cri = {
      .sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
      .buffer = pred.b};
   g.CmdBeginConditionalRenderingEXT(psec_cmd, &cri);
   g.CmdExecuteCommands(psec_cmd, 1, &sec);
   g.CmdEndConditionalRenderingEXT(psec_cmd);
   g.CmdEndRenderPass(psec_cmd);
   /* Following draw, its own render pass, proves the inherited skip
    * released the tess arenas. */
   g.CmdBeginRenderPass(psec_cmd, &prbi, VK_SUBPASS_CONTENTS_INLINE);
   env.use_indirect = 0;
   g.CmdBindPipeline(psec_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   g.CmdBindDescriptorSets(psec_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1,
                           &set, 0, NULL);
   VkDeviceSize voff = 0;
   g.CmdBindVertexBuffers(psec_cmd, 0, 1, &vb.b, &voff);
   PFN_vkCmdSetViewport SetVP =
      (PFN_vkCmdSetViewport)g.gdpa(g.dev, "vkCmdSetViewport");
   PFN_vkCmdSetScissor SetSC = (PFN_vkCmdSetScissor)g.gdpa(g.dev, "vkCmdSetScissor");
   VkViewport vpd = {0, 0, W, H, 0, 1};
   VkRect2D scd = {{0, 0}, {W, H}};
   SetVP(psec_cmd, 0, 1, &vpd);
   SetSC(psec_cmd, 0, 1, &scd);
   g.CmdDraw(psec_cmd, 3, 1, 0, 0);
   g.CmdEndRenderPass(psec_cmd);
   shader_to_copy(&g, psec_cmd, counter.b);
   CK(g.EndCommandBuffer(psec_cmd), "EndP");

   /* Copy is separate so the recorded draw cmdbuf stays untouched. */
   VkCommandBufferBeginInfo cbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
   /* rebuilt per submit below via reset. */

   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(g.CreateFence(g.dev, &fci, NULL, &fence), "Fence");

    struct run {
      VkCommandBuffer c;
      const char *name;
      uint32_t c0, c1;
      int red0, red1;
      int inverted;
   };
    /* Each draw is one patch, 3 TCS invocations. Skip keeps the
     * unconditional draw (3). Pass runs both conditional draws too (9).
     * The inherited secondary has the same two draws plus the primary's
     * following draw. */
    struct run runs[] = {
      {cmd, "direct", 3, 9, 1, 1, 0},
      {icmd, "indirect", 3, 9, 1, 1, 0},
      {inv, "inverted", 9, 3, 1, 1, 1},
      {psec_cmd, "inherit", 3, 9, 1, 1, 0},
   };

    int failed = 0;
    /* Raster check with no tessellation in the command buffer. */
    {
       VkCommandBuffer plain;
       CK(g.AllocateCommandBuffers(g.dev, &cai, &plain), "Plain");
       CK(g.BeginCommandBuffer(plain, &cbi), "BeginPlain");
       VkClearValue cv = {.color = {{1.f, 0.f, 0.f, 1.f}}};
       VkRenderPassBeginInfo rbi = {
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .renderPass = rp,
          .framebuffer = fb,
          .renderArea = {{0, 0}, {W, H}},
          .clearValueCount = 1,
          .pClearValues = &cv};
       g.CmdBeginRenderPass(plain, &rbi, VK_SUBPASS_CONTENTS_INLINE);
       g.CmdBindPipeline(plain, VK_PIPELINE_BIND_POINT_GRAPHICS, tri_pipe);
       VkDeviceSize off = 0;
       g.CmdBindVertexBuffers(plain, 0, 1, &vb.b, &off);
       VkViewport vp = {0, 0, W, H, 0, 1};
       VkRect2D sc = {{0, 0}, {W, H}};
       PFN_vkCmdSetViewport SetVP =
          (PFN_vkCmdSetViewport)g.gdpa(g.dev, "vkCmdSetViewport");
       PFN_vkCmdSetScissor SetSC =
          (PFN_vkCmdSetScissor)g.gdpa(g.dev, "vkCmdSetScissor");
       SetVP(plain, 0, 1, &vp);
       SetSC(plain, 0, 1, &sc);
       g.CmdDraw(plain, 3, 1, 0, 0);
       g.CmdEndRenderPass(plain);
       CK(g.EndCommandBuffer(plain), "EndPlain");
       memset(rb.p, 0x5a, (size_t)rb.sz);
       CK(g.ResetFences(g.dev, 1, &fence), "ResetPlain");
       VkSubmitInfo sp = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &plain};
       CK(g.QueueSubmit(g.q, 1, &sp, fence), "SubmitPlain");
       int pr = wait_fence(&g, fence, now_s(), "plain");
       if (pr)
          return pr;
       CK(g.ResetCommandBuffer(copy, 0), "ResetPlainCopy");
       CK(g.BeginCommandBuffer(copy, &cbi), "BeginPlainCopy");
        VkBufferImageCopy region = {
           .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
           .imageExtent = {W, H, 1}};
        g.CmdCopyImageToBuffer(copy, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               rb.b, 1, &region);
        VkImageMemoryBarrier2 ib = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
         .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
         .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
         .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = img,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
       VkDependencyInfo idi = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .imageMemoryBarrierCount = 1,
                               .pImageMemoryBarriers = &ib};
       g.CmdPipelineBarrier2(copy, &idi);
       VkBufferMemoryBarrier hb = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = rb.b,
          .size = rb.sz};
       g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &hb, 0,
                            NULL);
       CK(g.EndCommandBuffer(copy), "EndPlainCopy");
       CK(g.ResetFences(g.dev, 1, &fence), "ResetPlainCopyF");
       VkSubmitInfo spi = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &copy};
       CK(g.QueueSubmit(g.q, 1, &spi, fence), "SubmitPlainCopy");
       pr = wait_fence(&g, fence, now_s(), "plain-copy");
       if (pr)
          return pr;
       const uint8_t *c = (const uint8_t *)rb.p + ((H / 2) * W + W / 2) * 4;
       printf("DIAG plain pixel %u %u %u %u red=%d\n", c[0], c[1], c[2], c[3],
              red_count(rb.p));
    }
    for (unsigned r = 0; r < sizeof(runs) / sizeof(runs[0]); r++) {
       /* 0,1,0. Non-inverted expects 3,9,3. Inverted expects 9,3,9. */
       static const uint32_t seq[3] = {0, 1, 0};
       for (int pass = 0; pass < 3; pass++) {
         uint32_t pv = seq[pass];
          memcpy(pred.p, &pv, 4);
         memset(rb.p, 0, (size_t)rb.sz);
         /* counter is device-local. Fill 0, then let the recorded HOST|TRANSFER
          * barrier make that fill visible to TCS. */
         CK(g.ResetCommandBuffer(copy, 0), "ResetZero");
         CK(g.BeginCommandBuffer(copy, &cbi), "BeginZero");
         g.CmdFillBuffer(copy, counter.b, 0, 4, 0);
         CK(g.EndCommandBuffer(copy), "EndZero");
         CK(g.ResetFences(g.dev, 1, &fence), "ResetFZ");
         VkSubmitInfo sz = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &copy};
         CK(g.QueueSubmit(g.q, 1, &sz, fence), "SubmitZero");
         int zr = wait_fence(&g, fence, now_s(), "zero");
         if (zr)
            return zr;
         CK(g.ResetFences(g.dev, 1, &fence), "ResetF");
         VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &runs[r].c};
          const char *nm = runs[r].name;
          uint32_t expect_tab[3] = {runs[r].c0, runs[r].c1, runs[r].c0};
         double t0 = now_s();
         stamp(nm);
         CK(g.QueueSubmit(g.q, 1, &si, fence), "Submit");
         int wr = wait_fence(&g, fence, t0, nm);
         if (wr)
            return wr;
         /* copy color */
         CK(g.ResetCommandBuffer(copy, 0), "ResetCopy");
         CK(g.BeginCommandBuffer(copy, &cbi), "BeginCopy");
         VkBufferImageCopy region = {
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent = {W, H, 1}};
          VkBufferMemoryBarrier pre = {
             .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
             .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
             .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .buffer = counter.b,
             .size = 4};
          g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &pre,
                               0, NULL);
          g.CmdCopyImageToBuffer(copy, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 rb.b, 1, &region);
          VkBufferCopy bc = {.size = 4};
          g.CmdCopyBuffer(copy, counter.b, counter_rb.b, 1, &bc);
          VkImageMemoryBarrier2 ib = {
             .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
             .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
             .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = img,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
         VkDependencyInfo idi = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                 .imageMemoryBarrierCount = 1,
                                 .pImageMemoryBarriers = &ib};
          g.CmdPipelineBarrier2(copy, &idi);
          VkBufferMemoryBarrier hb = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = rb.b,
            .size = rb.sz};
         VkBufferMemoryBarrier cb = hb;
         cb.buffer = counter_rb.b;
         cb.size = 4;
         VkBufferMemoryBarrier both[2] = {hb, cb};
         g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 2, both, 0,
                              NULL);
         CK(g.EndCommandBuffer(copy), "EndCopy");
         CK(g.ResetFences(g.dev, 1, &fence), "ResetF2");
         VkSubmitInfo sc = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &copy};
         CK(g.QueueSubmit(g.q, 1, &sc, fence), "SubmitCopy");
         wr = wait_fence(&g, fence, t0, "copy");
         if (wr)
            return wr;
          uint32_t got;
          memcpy(&got, counter_rb.p, 4);
          printf("RAW %08x\n", got);
         int red = red_count(rb.p);
         {
            const uint8_t *c = (const uint8_t *)rb.p + ((H / 2) * W + W / 2) * 4;
            printf("PIXEL %u %u %u %u\n", c[0], c[1], c[2], c[3]);
         }
          uint32_t expect = expect_tab[pass];
          int want_red = 1;
         printf("CASE %s pred=%u counter=%u expect=%u red=%d\n", nm, pv, got,
                expect, red);
         if (!have_vpsa)
            printf("CASE %s counter unchecked (no vpsa)\n", nm);
         else if (got != expect)
            failed = 1;
         if (want_red && red == 0)
            failed = 1;
          if (runs[r].c == cmd && pass == 2) {
            uint64_t ts[2] = {0, 0};
            VkResult qr = g.GetQueryPoolResults(
               g.dev, tspool, 0, 2, sizeof(ts), ts, sizeof(uint64_t),
               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            printf("TIMESTAMP r=%d t0=%llu t1=%llu\n", (int)qr,
                   (unsigned long long)ts[0], (unsigned long long)ts[1]);
            if (qr != VK_SUCCESS || ts[1] < ts[0])
               failed = 1;
         }
      }
   }
   /* Replay the direct buffer once more (third submit) to show the
    * recorded source survived both predicate values. */
   {
      uint32_t pv = 1;
      memcpy(pred.p, &pv, 4);
      CK(g.ResetCommandBuffer(copy, 0), "ResetZeroR");
      CK(g.BeginCommandBuffer(copy, &cbi), "BeginZeroR");
      g.CmdFillBuffer(copy, counter.b, 0, 4, 0);
      CK(g.EndCommandBuffer(copy), "EndZeroR");
      CK(g.ResetFences(g.dev, 1, &fence), "ResetFZR");
      VkSubmitInfo szr = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &copy};
      CK(g.QueueSubmit(g.q, 1, &szr, fence), "SubmitZeroR");
      int zrr = wait_fence(&g, fence, now_s(), "zero-replay");
      if (zrr)
         return zrr;
      CK(g.ResetFences(g.dev, 1, &fence), "ResetF3");
      VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &cmd};
      stamp("direct-replay");
      double t0 = now_s();
      CK(g.QueueSubmit(g.q, 1, &si, fence), "SubmitReplay");
      int wr = wait_fence(&g, fence, t0, "direct-replay");
      if (wr)
         return wr;
      CK(g.ResetCommandBuffer(copy, 0), "ResetCopyR");
      CK(g.BeginCommandBuffer(copy, &cbi), "BeginCopyR");
      VkBufferMemoryBarrier pre = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = counter.b,
         .size = 4};
      g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &pre, 0,
                           NULL);
      VkBufferCopy bc = {.size = 4};
      g.CmdCopyBuffer(copy, counter.b, counter_rb.b, 1, &bc);
      VkBufferMemoryBarrier cb = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = counter_rb.b,
         .size = 4};
      g.CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &cb, 0,
                           NULL);
      CK(g.EndCommandBuffer(copy), "EndCopyR");
      CK(g.ResetFences(g.dev, 1, &fence), "ResetF5");
      VkSubmitInfo scr = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &copy};
      CK(g.QueueSubmit(g.q, 1, &scr, fence), "SubmitCopyR");
      wr = wait_fence(&g, fence, t0, "copy-replay");
      if (wr)
         return wr;
      uint32_t got;
      memcpy(&got, counter_rb.p, 4);
      printf("CASE direct-replay pred=1 counter=%u expect=9\n", got);
      if (have_vpsa && got != 9)
         failed = 1;
   }

   printf("RESULT %s\n", failed ? "FAIL" : "PASS");
   return failed ? 1 : 0;
}
