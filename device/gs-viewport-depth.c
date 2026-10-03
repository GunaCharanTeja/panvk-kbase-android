/* Gate 089 device: per-viewport depth range for GS-selected viewports.
 *
 * Two viewports split the image into left/right halves:
 *   vp0: minDepth 0.00 maxDepth 0.25      vp1: minDepth 0.50 maxDepth 1.00
 * (union [0,1] differs from both). A geometry shader picks the viewport per
 * primitive with gl_ViewportIndex; each input point yields one NDC-covering
 * triangle at a given clip-space z (w=1). Depth attachment D32_SFLOAT cleared
 * to 1.0, depth test ALWAYS, so the last write per pixel wins. Depth and
 * color are read back and compared (tolerance 1e-5).
 *
 * Old driver: programs the UNION of all viewports' depth ranges (here [0,1])
 * for depth clamp / clip. Case A then fails: vp0 z=1.5 -> window z =
 * 0 + 1.5*0.25 = 0.375, inside the union, so nothing clamps and 0.375 is
 * written instead of 0.25; vp1 z=-0.5 -> 0.5 - 0.25 = 0.25, inside the union,
 * written instead of clamped 0.5. Fixed driver programs each run's own range.
 *
 *  A: depthClamp=TRUE, clip off (default): vp0 -> 0.25, vp1 -> 0.5
 *  B: depthClamp=TRUE + VK_EXT_depth_clip_enable clip=TRUE: out-of-range
 *     triangles clipped (depth stays 1.0 only if nothing else wrote), in-range
 *     z=0.5 -> 0.125 / 0.75. SKIP if the extension is missing.
 *  C: depthClamp=FALSE: in-range z=0.5 -> 0.125 / 0.75, z=1.5 / -0.5 clipped.
 * Sequences mix viewports (0,1,0,...) to test run ordering.
 *
 * Features: geometryShader, multiViewport, depthClamp.
 * Output: PASS/FAIL/SKIP per case, then "RESULT PASS" or "RESULT FAIL".
 * usage: gs-viewport-depth <libvulkan_panfrost.so>
 *
 * SPIR-V regen: glslangValidator -V gs-viewport-depth.{vert,geom,frag}
 */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define W 32u
#define H 32u
#define MAXPTS 8u
#define TOL 1e-5f

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, (int)_r, __LINE__);            \
         return 1;                                                             \
      }                                                                        \
   } while (0)

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

static const uint32_t geom_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000037u, 0x00000000u, 0x00020011u, 0x00000002u, 0x00020011u,
   0x00000039u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu,
   0x00000000u, 0x00000001u, 0x0009000fu, 0x00000003u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000011u,
   0x0000001fu, 0x00000021u, 0x00000025u, 0x00030010u, 0x00000004u, 0x00000013u, 0x00040010u, 0x00000004u,
   0x00000000u, 0x00000001u, 0x00030010u, 0x00000004u, 0x0000001du, 0x00040010u, 0x00000004u, 0x0000001au,
   0x00000003u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
   0x00030005u, 0x00000008u, 0x00000076u, 0x00060005u, 0x0000000eu, 0x505f6c67u, 0x65567265u, 0x78657472u,
   0x00000000u, 0x00060006u, 0x0000000eu, 0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u,
   0x0000000eu, 0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x0000000eu,
   0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u, 0x0000000eu, 0x00000003u,
   0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00040005u, 0x00000011u, 0x695f6c67u, 0x0000006eu,
   0x00030005u, 0x0000001bu, 0x0000007au, 0x00070005u, 0x0000001fu, 0x565f6c67u, 0x70776569u, 0x4974726fu,
   0x7865646eu, 0x00000000u, 0x00030005u, 0x00000021u, 0x00007076u, 0x00060005u, 0x00000023u, 0x505f6c67u,
   0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x00000023u, 0x00000000u, 0x505f6c67u, 0x7469736fu,
   0x006e6f69u, 0x00070006u, 0x00000023u, 0x00000001u, 0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u,
   0x00070006u, 0x00000023u, 0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u,
   0x00000023u, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu, 0x00030005u, 0x00000025u,
   0x00000000u, 0x00030047u, 0x0000000eu, 0x00000002u, 0x00050048u, 0x0000000eu, 0x00000000u, 0x0000000bu,
   0x00000000u, 0x00050048u, 0x0000000eu, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000eu,
   0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x0000000eu, 0x00000003u, 0x0000000bu, 0x00000004u,
   0x00040047u, 0x0000001fu, 0x0000000bu, 0x0000000au, 0x00030047u, 0x00000021u, 0x0000000eu, 0x00040047u,
   0x00000021u, 0x0000001eu, 0x00000000u, 0x00030047u, 0x00000023u, 0x00000002u, 0x00050048u, 0x00000023u,
   0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x00000023u, 0x00000001u, 0x0000000bu, 0x00000001u,
   0x00050048u, 0x00000023u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000023u, 0x00000003u,
   0x0000000bu, 0x00000004u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u,
   0x00000006u, 0x00000020u, 0x00000001u, 0x00040020u, 0x00000007u, 0x00000007u, 0x00000006u, 0x00030016u,
   0x00000009u, 0x00000020u, 0x00040017u, 0x0000000au, 0x00000009u, 0x00000004u, 0x00040015u, 0x0000000bu,
   0x00000020u, 0x00000000u, 0x0004002bu, 0x0000000bu, 0x0000000cu, 0x00000001u, 0x0004001cu, 0x0000000du,
   0x00000009u, 0x0000000cu, 0x0006001eu, 0x0000000eu, 0x0000000au, 0x00000009u, 0x0000000du, 0x0000000du,
   0x0004001cu, 0x0000000fu, 0x0000000eu, 0x0000000cu, 0x00040020u, 0x00000010u, 0x00000001u, 0x0000000fu,
   0x0004003bu, 0x00000010u, 0x00000011u, 0x00000001u, 0x0004002bu, 0x00000006u, 0x00000012u, 0x00000000u,
   0x0004002bu, 0x0000000bu, 0x00000013u, 0x00000000u, 0x00040020u, 0x00000014u, 0x00000001u, 0x00000009u,
   0x0004002bu, 0x00000009u, 0x00000017u, 0x3f000000u, 0x00040020u, 0x0000001au, 0x00000007u, 0x00000009u,
   0x00040020u, 0x0000001eu, 0x00000003u, 0x00000006u, 0x0004003bu, 0x0000001eu, 0x0000001fu, 0x00000003u,
   0x0004003bu, 0x0000001eu, 0x00000021u, 0x00000003u, 0x0006001eu, 0x00000023u, 0x0000000au, 0x00000009u,
   0x0000000du, 0x0000000du, 0x00040020u, 0x00000024u, 0x00000003u, 0x00000023u, 0x0004003bu, 0x00000024u,
   0x00000025u, 0x00000003u, 0x0004002bu, 0x00000009u, 0x00000026u, 0xbf800000u, 0x0004002bu, 0x00000009u,
   0x00000028u, 0x3f800000u, 0x00040020u, 0x0000002au, 0x00000003u, 0x0000000au, 0x0004002bu, 0x00000009u,
   0x0000002eu, 0x40400000u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
   0x00000005u, 0x0004003bu, 0x00000007u, 0x00000008u, 0x00000007u, 0x0004003bu, 0x0000001au, 0x0000001bu,
   0x00000007u, 0x00070041u, 0x00000014u, 0x00000015u, 0x00000011u, 0x00000012u, 0x00000012u, 0x00000013u,
   0x0004003du, 0x00000009u, 0x00000016u, 0x00000015u, 0x00050081u, 0x00000009u, 0x00000018u, 0x00000016u,
   0x00000017u, 0x0004006eu, 0x00000006u, 0x00000019u, 0x00000018u, 0x0003003eu, 0x00000008u, 0x00000019u,
   0x00070041u, 0x00000014u, 0x0000001cu, 0x00000011u, 0x00000012u, 0x00000012u, 0x0000000cu, 0x0004003du,
   0x00000009u, 0x0000001du, 0x0000001cu, 0x0003003eu, 0x0000001bu, 0x0000001du, 0x0004003du, 0x00000006u,
   0x00000020u, 0x00000008u, 0x0003003eu, 0x0000001fu, 0x00000020u, 0x0004003du, 0x00000006u, 0x00000022u,
   0x00000008u, 0x0003003eu, 0x00000021u, 0x00000022u, 0x0004003du, 0x00000009u, 0x00000027u, 0x0000001bu,
   0x00070050u, 0x0000000au, 0x00000029u, 0x00000026u, 0x00000026u, 0x00000027u, 0x00000028u, 0x00050041u,
   0x0000002au, 0x0000002bu, 0x00000025u, 0x00000012u, 0x0003003eu, 0x0000002bu, 0x00000029u, 0x000100dau,
   0x0004003du, 0x00000006u, 0x0000002cu, 0x00000008u, 0x0003003eu, 0x0000001fu, 0x0000002cu, 0x0004003du,
   0x00000006u, 0x0000002du, 0x00000008u, 0x0003003eu, 0x00000021u, 0x0000002du, 0x0004003du, 0x00000009u,
   0x0000002fu, 0x0000001bu, 0x00070050u, 0x0000000au, 0x00000030u, 0x0000002eu, 0x00000026u, 0x0000002fu,
   0x00000028u, 0x00050041u, 0x0000002au, 0x00000031u, 0x00000025u, 0x00000012u, 0x0003003eu, 0x00000031u,
   0x00000030u, 0x000100dau, 0x0004003du, 0x00000006u, 0x00000032u, 0x00000008u, 0x0003003eu, 0x0000001fu,
   0x00000032u, 0x0004003du, 0x00000006u, 0x00000033u, 0x00000008u, 0x0003003eu, 0x00000021u, 0x00000033u,
   0x0004003du, 0x00000009u, 0x00000034u, 0x0000001bu, 0x00070050u, 0x0000000au, 0x00000035u, 0x00000026u,
   0x0000002eu, 0x00000034u, 0x00000028u, 0x00050041u, 0x0000002au, 0x00000036u, 0x00000025u, 0x00000012u,
   0x0003003eu, 0x00000036u, 0x00000035u, 0x000100dau, 0x000100dbu, 0x000100fdu, 0x00010038u,
};

static const uint32_t frag_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000018u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0007000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x0000000cu, 0x00030010u,
   0x00000004u, 0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du,
   0x00000000u, 0x00030005u, 0x00000009u, 0x0000006fu, 0x00030005u, 0x0000000cu, 0x00007076u, 0x00040047u,
   0x00000009u, 0x0000001eu, 0x00000000u, 0x00030047u, 0x0000000cu, 0x0000000eu, 0x00040047u, 0x0000000cu,
   0x0000001eu, 0x00000000u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u,
   0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u, 0x00000008u,
   0x00000003u, 0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000003u, 0x00040015u, 0x0000000au,
   0x00000020u, 0x00000001u, 0x00040020u, 0x0000000bu, 0x00000001u, 0x0000000au, 0x0004003bu, 0x0000000bu,
   0x0000000cu, 0x00000001u, 0x0004002bu, 0x0000000au, 0x0000000eu, 0x00000000u, 0x00020014u, 0x0000000fu,
   0x0004002bu, 0x00000006u, 0x00000011u, 0x3f800000u, 0x0004002bu, 0x00000006u, 0x00000012u, 0x00000000u,
   0x0007002cu, 0x00000007u, 0x00000013u, 0x00000011u, 0x00000012u, 0x00000012u, 0x00000011u, 0x0007002cu,
   0x00000007u, 0x00000014u, 0x00000012u, 0x00000011u, 0x00000012u, 0x00000011u, 0x00040017u, 0x00000015u,
   0x0000000fu, 0x00000004u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
   0x00000005u, 0x0004003du, 0x0000000au, 0x0000000du, 0x0000000cu, 0x000500aau, 0x0000000fu, 0x00000010u,
   0x0000000du, 0x0000000eu, 0x00070050u, 0x00000015u, 0x00000016u, 0x00000010u, 0x00000010u, 0x00000010u,
   0x00000010u, 0x000600a9u, 0x00000007u, 0x00000017u, 0x00000016u, 0x00000013u, 0x00000014u, 0x0003003eu,
   0x00000009u, 0x00000017u, 0x000100fdu, 0x00010038u,
};

#define DEV_FNS(X)                                                             \
   X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory)            \
   X(BindBufferMemory) X(MapMemory) X(CreateImage)                             \
   X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView)         \
   X(CreateRenderPass) X(CreateFramebuffer) X(CreateShaderModule)              \
   X(CreatePipelineLayout) X(CreateGraphicsPipelines) X(CreateCommandPool)     \
   X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer)         \
   X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline)                \
   X(CmdBindVertexBuffers) X(CmdDraw) X(CmdCopyImageToBuffer)                  \
   X(CreateFence) X(WaitForFences) X(ResetFences) X(QueueSubmit)               \
   X(ResetCommandBuffer) X(GetDeviceQueue)

#define DECL(n) static PFN_vk##n p_##n;
DEV_FNS(DECL)
#undef DECL

static VkDevice dev;
static VkQueue queue;
static VkPhysicalDeviceMemoryProperties mp;
static VkRenderPass rp;
static VkPipelineLayout pl;
static VkShaderModule sm_vs, sm_gs, sm_fs;
static VkFramebuffer fb;
static VkImage color_img, depth_img;
static VkBuffer vbuf, rb_depth, rb_color;
static float *vmap;
static const float *dmap;
static const uint8_t *cmap;
static VkCommandBuffer cmd;
static VkFence fence;

/* Pick a type from this resource's memoryTypeBits with all `want` flags. */
static int
pick_type(uint32_t bits, VkMemoryPropertyFlags want, uint32_t *out)
{
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      if ((bits & (1u << i)) &&
          (mp.memoryTypes[i].propertyFlags & want) == want) {
         *out = i;
         return 0;
      }
   }
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      if (bits & (1u << i)) {
         *out = i;
         return 0;
      }
   }
   printf("FAIL no memory type bits=0x%x\n", bits);
   return 1;
}

static int
alloc_bind(const VkMemoryRequirements *mr, VkMemoryPropertyFlags want,
           VkDeviceMemory *m, uint32_t *ti)
{
   if (pick_type(mr->memoryTypeBits, want, ti))
      return 1;
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr->size,
                               .memoryTypeIndex = *ti};
   CK(p_AllocateMemory(dev, &mai, NULL, m), "AllocateMemory");
   return 0;
}

static int
make_buf(VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer *b, void **map)
{
   VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size = sz,
                            .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(p_CreateBuffer(dev, &bi, NULL, b), "CreateBuffer");
   VkMemoryRequirements mr;
   p_GetBufferMemoryRequirements(dev, *b, &mr);
   VkDeviceMemory m;
   uint32_t ti;
   if (alloc_bind(&mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m, &ti))
      return 1;
   CK(p_BindBufferMemory(dev, *b, m, 0), "BindBufferMemory");
   CK(p_MapMemory(dev, m, 0, sz, 0, map), "MapMemory");
   return 0;
}

static int
make_image(VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect,
           VkImage *img, VkImageView *view)
{
   VkImageCreateInfo ii = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = fmt,
      .extent = {W, H, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   CK(p_CreateImage(dev, &ii, NULL, img), "CreateImage");
   VkMemoryRequirements mr;
   p_GetImageMemoryRequirements(dev, *img, &mr);
   VkDeviceMemory m;
   uint32_t ti;
   if (alloc_bind(&mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m, &ti))
      return 1;
   printf("ALLOC image fmt=%d type=%u bits=0x%x flags=0x%x\n", (int)fmt, ti,
          mr.memoryTypeBits, mp.memoryTypes[ti].propertyFlags);
   CK(p_BindImageMemory(dev, *img, m, 0), "BindImageMemory");
   VkImageViewCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = *img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = fmt,
      .subresourceRange = {aspect, 0, 1, 0, 1}};
   CK(p_CreateImageView(dev, &vi, NULL, view), "CreateImageView");
   return 0;
}

static int
make_pipe(int clamp, int chain_clip, VkBool32 clip, VkPipeline *out)
{
   VkPipelineShaderStageCreateInfo st[3] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = sm_vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = sm_gs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = sm_fs, .pName = "main"}};
   VkVertexInputBindingDescription vb = {0, 16, VK_VERTEX_INPUT_RATE_VERTEX};
   VkVertexInputAttributeDescription va = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
   VkPipelineVertexInputStateCreateInfo vis = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vb,
      .vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &va};
   VkPipelineInputAssemblyStateCreateInfo ias = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST};
   VkViewport vps[2] = {{0.0f, 0.0f, (float)(W / 2), (float)H, 0.0f, 0.25f},
                        {(float)(W / 2), 0.0f, (float)(W / 2), (float)H, 0.5f, 1.0f}};
   VkRect2D sc[2] = {{{0, 0}, {W / 2, H}}, {{(int32_t)(W / 2), 0}, {W / 2, H}}};
   VkPipelineViewportStateCreateInfo vs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 2, .pViewports = vps, .scissorCount = 2, .pScissors = sc};
   VkPipelineRasterizationDepthClipStateCreateInfoEXT dcs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
      .depthClipEnable = clip};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = chain_clip ? &dcs : NULL,
      .depthClampEnable = clamp ? VK_TRUE : VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineDepthStencilStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_ALWAYS, .maxDepthBounds = 1.0f};
   VkPipelineColorBlendAttachmentState cba = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
   VkPipelineColorBlendStateCreateInfo cbs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &cba};
   VkGraphicsPipelineCreateInfo gpi = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 3, .pStages = st, .pVertexInputState = &vis,
      .pInputAssemblyState = &ias, .pViewportState = &vs,
      .pRasterizationState = &rs, .pMultisampleState = &ms,
      .pDepthStencilState = &ds, .pColorBlendState = &cbs, .layout = pl,
      .renderPass = rp, .subpass = 0};
   CK(p_CreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, out),
      "CreateGraphicsPipelines");
   return 0;
}

struct pt {
   float vp, z;
};

/* Draw pts, read back, compare. Returns 0 on PASS. */
static int
run_case(const char *name, VkPipeline pipe, const struct pt *pts, uint32_t n,
         float expL, float expR)
{
   for (uint32_t i = 0; i < n; i++) {
      vmap[i * 4 + 0] = pts[i].vp;
      vmap[i * 4 + 1] = pts[i].z;
      vmap[i * 4 + 2] = 0.0f;
      vmap[i * 4 + 3] = 1.0f;
   }
   CK(p_ResetCommandBuffer(cmd, 0), "ResetCommandBuffer");
   VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
   CK(p_BeginCommandBuffer(cmd, &bi), "BeginCommandBuffer");
   VkClearValue cv[2];
   memset(cv, 0, sizeof(cv));
   cv[1].depthStencil.depth = 1.0f;
   VkRenderPassBeginInfo rbi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = rp, .framebuffer = fb,
      .renderArea = {{0, 0}, {W, H}}, .clearValueCount = 2, .pClearValues = cv};
   p_CmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
   p_CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   p_CmdBindVertexBuffers(cmd, 0, 1, &vbuf, &off);
   p_CmdDraw(cmd, n, 1, 0, 0);
   p_CmdEndRenderPass(cmd);
   VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1},
                           .imageExtent = {W, H, 1}};
   p_CmdCopyImageToBuffer(cmd, depth_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          rb_depth, 1, &bc);
   bc.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   p_CmdCopyImageToBuffer(cmd, color_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          rb_color, 1, &bc);
   CK(p_EndCommandBuffer(cmd), "EndCommandBuffer");
   CK(p_ResetFences(dev, 1, &fence), "ResetFences");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1, .pCommandBuffers = &cmd};
   CK(p_QueueSubmit(queue, 1, &si, fence), "QueueSubmit");
   VkResult wr = VK_TIMEOUT;
   for (int k = 0; k < 40 && wr == VK_TIMEOUT; k++)
      wr = p_WaitForFences(dev, 1, &fence, VK_TRUE, 500000000ull);
   if (wr != VK_SUCCESS) {
      printf("FAIL case %s fence r=%d\n", name, (int)wr);
      return 1;
   }

   float minL = 1e9f, maxL = -1e9f, minR = 1e9f, maxR = -1e9f;
   uint32_t badD = 0, badC = 0;
   for (uint32_t y = 0; y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
         int right = x >= W / 2;
         float d = dmap[y * W + x];
         float e = right ? expR : expL;
         if (right) {
            minR = fminf(minR, d);
            maxR = fmaxf(maxR, d);
         } else {
            minL = fminf(minL, d);
            maxL = fmaxf(maxL, d);
         }
         if (!(fabsf(d - e) <= TOL))
            badD++;
         const uint8_t *c = cmap + (y * W + x) * 4;
         if (right ? !(c[0] == 0 && c[1] == 255 && c[2] == 0 && c[3] == 255)
                   : !(c[0] == 255 && c[1] == 0 && c[2] == 0 && c[3] == 255))
            badC++;
      }
   }
   int ok = !badD && !badC;
   printf("%s case %s depthL=[%.6f..%.6f] expL=%.6f depthR=[%.6f..%.6f] "
          "expR=%.6f badDepth=%u badColor=%u\n",
          ok ? "PASS" : "FAIL", name, minL, maxL, expL, minR, maxR, expR, badD,
          badC);
   return !ok;
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
   VkInstance inst;
   CK(CreateInstance(&ici, NULL, &inst), "CreateInstance");

#define GI(n)                                                                  \
   PFN_vk##n n = (PFN_vk##n)gipa(inst, "vk" #n);                               \
   if (!n) {                                                                   \
      printf("FAIL missing vk" #n "\n");                                       \
      return 1;                                                                \
   }
   GI(EnumeratePhysicalDevices)
   GI(GetPhysicalDeviceProperties)
   GI(GetPhysicalDeviceFeatures2)
   GI(GetPhysicalDeviceQueueFamilyProperties)
   GI(GetPhysicalDeviceMemoryProperties)
   GI(GetPhysicalDeviceFormatProperties)
   GI(EnumerateDeviceExtensionProperties)
   GI(CreateDevice)
   GI(GetDeviceProcAddr)
#undef GI

   uint32_t nd = 0;
   CK(EnumeratePhysicalDevices(inst, &nd, NULL), "count");
   VkPhysicalDevice *devs = calloc(nd, sizeof(*devs));
   if (!devs)
      return 1;
   CK(EnumeratePhysicalDevices(inst, &nd, devs), "enum");
   VkPhysicalDevice phys = VK_NULL_HANDLE;
   for (uint32_t i = 0; i < nd; i++) {
      VkPhysicalDeviceProperties p;
      GetPhysicalDeviceProperties(devs[i], &p);
      printf("PHYS %u %s vendor=0x%x device=0x%x\n", i, p.deviceName, p.vendorID,
             p.deviceID);
      if (strstr(p.deviceName, "Mali"))
         phys = devs[i];
   }
   free(devs);
   if (!phys) {
      printf("FAIL no Mali\n");
      return 1;
   }

   VkPhysicalDeviceDepthClipEnableFeaturesEXT dcf = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT};
   VkPhysicalDeviceFeatures2 f2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &dcf};
   GetPhysicalDeviceFeatures2(phys, &f2);
   uint32_t nx = 0;
   EnumerateDeviceExtensionProperties(phys, NULL, &nx, NULL);
   VkExtensionProperties *xp = calloc(nx ? nx : 1, sizeof(*xp));
   if (!xp)
      return 1;
   EnumerateDeviceExtensionProperties(phys, NULL, &nx, xp);
   int has_clip_ext = 0;
   for (uint32_t i = 0; i < nx; i++)
      if (!strcmp(xp[i].extensionName, VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME))
         has_clip_ext = 1;
   free(xp);
   int do_b = has_clip_ext && dcf.depthClipEnable;
   printf("FEATURE geom=%u multiVp=%u depthClamp=%u clipExt=%d depthClipEnable=%u\n",
          f2.features.geometryShader, f2.features.multiViewport,
          f2.features.depthClamp, has_clip_ext, dcf.depthClipEnable);
   if (!f2.features.geometryShader || !f2.features.multiViewport ||
       !f2.features.depthClamp) {
      printf("FAIL required feature missing\n");
      return 1;
   }
   VkFormatProperties fp;
   GetPhysicalDeviceFormatProperties(phys, VK_FORMAT_D32_SFLOAT, &fp);
   if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      printf("FAIL D32_SFLOAT not a depth attachment format\n");
      return 1;
   }

   uint32_t qn = 0;
   GetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
   VkQueueFamilyProperties *qp = calloc(qn, sizeof(*qp));
   if (!qp)
      return 1;
   GetPhysicalDeviceQueueFamilyProperties(phys, &qn, qp);
   uint32_t qi = ~0u;
   for (uint32_t i = 0; i < qn && qi == ~0u; i++)
      if (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
         qi = i;
   free(qp);
   if (qi == ~0u) {
      printf("FAIL no graphics queue\n");
      return 1;
   }
   GetPhysicalDeviceMemoryProperties(phys, &mp);

   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceFeatures ef;
   memset(&ef, 0, sizeof(ef));
   ef.geometryShader = VK_TRUE;
   ef.multiViewport = VK_TRUE;
   ef.depthClamp = VK_TRUE;
   VkPhysicalDeviceDepthClipEnableFeaturesEXT edcf = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,
      .depthClipEnable = VK_TRUE};
   const char *dexts[] = {VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .pNext = do_b ? &edcf : NULL,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .enabledExtensionCount = do_b ? 1u : 0u,
                             .ppEnabledExtensionNames = dexts,
                             .pEnabledFeatures = &ef};
   CK(CreateDevice(phys, &dci, NULL, &dev), "CreateDevice");

#define LP(n)                                                                  \
   do {                                                                        \
      PFN_vkVoidFunction _p = GetDeviceProcAddr(dev, "vk" #n);                 \
      if (!_p) {                                                               \
         printf("FAIL missing vk" #n "\n");                                    \
         return 1;                                                             \
      }                                                                        \
      memcpy(&p_##n, &_p, sizeof(_p));                                         \
   } while (0);
   DEV_FNS(LP)
#undef LP
   p_GetDeviceQueue(dev, qi, 0, &queue);

   VkCommandPool pool;
   VkCommandPoolCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi};
   CK(p_CreateCommandPool(dev, &pci, NULL, &pool), "Pool");
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(p_AllocateCommandBuffers(dev, &cai, &cmd), "AllocateCommandBuffers");
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(p_CreateFence(dev, &fci, NULL, &fence), "CreateFence");

   void *map;
   if (make_buf(MAXPTS * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vbuf, &map))
      return 1;
   vmap = map;
   if (make_buf(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &rb_depth, &map))
      return 1;
   dmap = map;
   if (make_buf(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &rb_color, &map))
      return 1;
   cmap = map;

   VkImageView cview, dview;
   if (make_image(VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_IMAGE_ASPECT_COLOR_BIT, &color_img, &cview) ||
       make_image(VK_FORMAT_D32_SFLOAT,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_IMAGE_ASPECT_DEPTH_BIT, &depth_img, &dview))
      return 1;

   VkAttachmentDescription ad[2] = {
      {.format = VK_FORMAT_R8G8B8A8_UNORM, .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
      {.format = VK_FORMAT_D32_SFLOAT, .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}};
   VkAttachmentReference cref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkAttachmentReference dref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &cref,
                               .pDepthStencilAttachment = &dref};
   VkSubpassDependency dep = {
      .srcSubpass = 0, .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
   VkRenderPassCreateInfo rpi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 2, .pAttachments = ad,
                                 .subpassCount = 1, .pSubpasses = &sub,
                                 .dependencyCount = 1, .pDependencies = &dep};
   CK(p_CreateRenderPass(dev, &rpi, NULL, &rp), "CreateRenderPass");
   VkImageView atts[2] = {cview, dview};
   VkFramebufferCreateInfo fbi = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                  .renderPass = rp, .attachmentCount = 2,
                                  .pAttachments = atts, .width = W, .height = H,
                                  .layers = 1};
   CK(p_CreateFramebuffer(dev, &fbi, NULL, &fb), "CreateFramebuffer");

   VkPipelineLayoutCreateInfo pli = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
   CK(p_CreatePipelineLayout(dev, &pli, NULL, &pl), "CreatePipelineLayout");
   VkShaderModuleCreateInfo smi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                   .codeSize = sizeof(vert_spv), .pCode = vert_spv};
   CK(p_CreateShaderModule(dev, &smi, NULL, &sm_vs), "vs");
   smi.codeSize = sizeof(geom_spv);
   smi.pCode = geom_spv;
   CK(p_CreateShaderModule(dev, &smi, NULL, &sm_gs), "gs");
   smi.codeSize = sizeof(frag_spv);
   smi.pCode = frag_spv;
   CK(p_CreateShaderModule(dev, &smi, NULL, &sm_fs), "fs");

   int fails = 0;
   VkPipeline pipe;

   /* A: clamp on, clip off. vp0 z>1 -> 0.25, vp1 z<0 -> 0.5. */
   {
      static const struct pt seq[] = {{0, 1.5f}, {1, -0.5f}, {0, 2.0f},
                                      {1, -2.0f}, {0, 1.5f}};
      if (make_pipe(1, 0, VK_FALSE, &pipe))
         return 1;
      fails += run_case("A_clamp", pipe, seq, 5, 0.25f, 0.5f);
   }
   /* B: clamp on + clip on. Out-of-range clipped, in-range 0.125 / 0.75. */
   if (do_b) {
      static const struct pt seq[] = {{0, 1.5f}, {1, -0.5f}, {0, 0.5f},
                                      {1, 0.5f}, {0, 1.5f}, {1, -0.5f}};
      if (make_pipe(1, 1, VK_TRUE, &pipe))
         return 1;
      fails += run_case("B_clamp_clip", pipe, seq, 6, 0.125f, 0.75f);
   } else {
      printf("SKIP case B_clamp_clip (VK_EXT_depth_clip_enable unavailable)\n");
   }
   /* C: clamp off. In-range viewport transform; out-of-range clipped. */
   {
      static const struct pt seq[] = {{0, 1.5f}, {1, -0.5f}, {0, 0.5f},
                                      {1, 0.5f}, {0, 1.5f}, {1, -0.5f}};
      if (make_pipe(0, 0, VK_FALSE, &pipe))
         return 1;
      fails += run_case("C_noclamp", pipe, seq, 6, 0.125f, 0.75f);
   }

   printf("RESULT %s\n", fails ? "FAIL" : "PASS");
   return fails ? 1 : 0;
}
