/* Gate 092 device: depthBounds.
 *
 * 32x32 target, D32_SFLOAT cleared to 1.0. Pass 1 stores a depth gradient
 * d(x) = (x + 0.5) / 32 with a full-screen strip (color writes off). Pass 2
 * draws the same strip with depth test ALWAYS, depth writes off, color writes
 * on and the depth bounds test on: a pixel must be green only where the
 * STORED depth is inside [min, max] (inclusive). Pixels within 0.01 of a
 * bound are not checked. The depth buffer must still hold the gradient.
 *
 *  A: static bounds [0.3, 0.6]
 *  B: dynamic bounds [0.3, 0.6] (vkCmdSetDepthBounds)
 *  C: dynamic bounds [0.1, 0.2]
 *  D: dynamic enable (vkCmdSetDepthBoundsTestEnable) off: all green
 *  E: dynamic enable on, bounds [0.3, 0.6]
 *  F: 4x MSAA, static bounds [0.3, 0.6], resolved: each pixel's green must be
 *     255 * (samples whose stored depth is inside) / 4, so a test of one
 *     sample per pixel fails on the pixels that straddle a bound.
 *  G: no fragment shader, static bounds, depth writes on with the viewport
 *     depth range [0.9, 0.9]: depth must become 0.9 only inside the bounds.
 *     (Run before F.)
 *
 * Without the depthBounds feature the test reports FAIL and stops.
 * Output: PASS/FAIL/SKIP per case, then "RESULT PASS" or "RESULT FAIL".
 * usage: depth-bounds <libvulkan_panfrost.so>
 *
 * SPIR-V regen: glslangValidator -V depth-bounds.vert depth-bounds.frag
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

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, (int)_r, __LINE__);            \
         return 1;                                                             \
      }                                                                        \
   } while (0)

static const uint32_t vs_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000030u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0007000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000cu, 0x00000022u, 0x00030003u,
   0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00030005u, 0x00000009u,
   0x00000070u, 0x00060005u, 0x0000000cu, 0x565f6c67u, 0x65747265u, 0x646e4978u, 0x00007865u, 0x00060005u,
   0x00000020u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x00000020u, 0x00000000u,
   0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000020u, 0x00000001u, 0x505f6c67u, 0x746e696fu,
   0x657a6953u, 0x00000000u, 0x00070006u, 0x00000020u, 0x00000002u, 0x435f6c67u, 0x4470696cu, 0x61747369u,
   0x0065636eu, 0x00070006u, 0x00000020u, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u, 0x0065636eu,
   0x00030005u, 0x00000022u, 0x00000000u, 0x00040047u, 0x0000000cu, 0x0000000bu, 0x0000002au, 0x00030047u,
   0x00000020u, 0x00000002u, 0x00050048u, 0x00000020u, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u,
   0x00000020u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x00000020u, 0x00000002u, 0x0000000bu,
   0x00000003u, 0x00050048u, 0x00000020u, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00020013u, 0x00000002u,
   0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u,
   0x00000006u, 0x00000002u, 0x00040020u, 0x00000008u, 0x00000007u, 0x00000007u, 0x00040015u, 0x0000000au,
   0x00000020u, 0x00000001u, 0x00040020u, 0x0000000bu, 0x00000001u, 0x0000000au, 0x0004003bu, 0x0000000bu,
   0x0000000cu, 0x00000001u, 0x0004002bu, 0x0000000au, 0x0000000eu, 0x00000001u, 0x0004002bu, 0x0000000au,
   0x00000010u, 0x00000000u, 0x00020014u, 0x00000011u, 0x0004002bu, 0x00000006u, 0x00000013u, 0xbf800000u,
   0x0004002bu, 0x00000006u, 0x00000014u, 0x3f800000u, 0x0004002bu, 0x0000000au, 0x00000017u, 0x00000002u,
   0x00040017u, 0x0000001cu, 0x00000006u, 0x00000004u, 0x00040015u, 0x0000001du, 0x00000020u, 0x00000000u,
   0x0004002bu, 0x0000001du, 0x0000001eu, 0x00000001u, 0x0004001cu, 0x0000001fu, 0x00000006u, 0x0000001eu,
   0x0006001eu, 0x00000020u, 0x0000001cu, 0x00000006u, 0x0000001fu, 0x0000001fu, 0x00040020u, 0x00000021u,
   0x00000003u, 0x00000020u, 0x0004003bu, 0x00000021u, 0x00000022u, 0x00000003u, 0x0004002bu, 0x0000001du,
   0x00000024u, 0x00000000u, 0x00040020u, 0x00000025u, 0x00000007u, 0x00000006u, 0x0004002bu, 0x00000006u,
   0x00000028u, 0x3f000000u, 0x00040020u, 0x0000002eu, 0x00000003u, 0x0000001cu, 0x00050036u, 0x00000002u,
   0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u,
   0x00000007u, 0x0004003du, 0x0000000au, 0x0000000du, 0x0000000cu, 0x000500c7u, 0x0000000au, 0x0000000fu,
   0x0000000du, 0x0000000eu, 0x000500aau, 0x00000011u, 0x00000012u, 0x0000000fu, 0x00000010u, 0x000600a9u,
   0x00000006u, 0x00000015u, 0x00000012u, 0x00000013u, 0x00000014u, 0x0004003du, 0x0000000au, 0x00000016u,
   0x0000000cu, 0x000500c7u, 0x0000000au, 0x00000018u, 0x00000016u, 0x00000017u, 0x000500aau, 0x00000011u,
   0x00000019u, 0x00000018u, 0x00000010u, 0x000600a9u, 0x00000006u, 0x0000001au, 0x00000019u, 0x00000013u,
   0x00000014u, 0x00050050u, 0x00000007u, 0x0000001bu, 0x00000015u, 0x0000001au, 0x0003003eu, 0x00000009u,
   0x0000001bu, 0x0004003du, 0x00000007u, 0x00000023u, 0x00000009u, 0x00050041u, 0x00000025u, 0x00000026u,
   0x00000009u, 0x00000024u, 0x0004003du, 0x00000006u, 0x00000027u, 0x00000026u, 0x00050085u, 0x00000006u,
   0x00000029u, 0x00000027u, 0x00000028u, 0x00050081u, 0x00000006u, 0x0000002au, 0x00000029u, 0x00000028u,
   0x00050051u, 0x00000006u, 0x0000002bu, 0x00000023u, 0x00000000u, 0x00050051u, 0x00000006u, 0x0000002cu,
   0x00000023u, 0x00000001u, 0x00070050u, 0x0000001cu, 0x0000002du, 0x0000002bu, 0x0000002cu, 0x0000002au,
   0x00000014u, 0x00050041u, 0x0000002eu, 0x0000002fu, 0x00000022u, 0x00000010u, 0x0003003eu, 0x0000002fu,
   0x0000002du, 0x000100fdu, 0x00010038u,
};

static const uint32_t fs_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000du, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
   0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
   0x0006000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x00030010u, 0x00000004u,
   0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
   0x00030005u, 0x00000009u, 0x0000006fu, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000000u, 0x00020013u,
   0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u,
   0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u, 0x00000008u, 0x00000003u, 0x00000007u, 0x0004003bu,
   0x00000008u, 0x00000009u, 0x00000003u, 0x0004002bu, 0x00000006u, 0x0000000au, 0x00000000u, 0x0004002bu,
   0x00000006u, 0x0000000bu, 0x3f800000u, 0x0007002cu, 0x00000007u, 0x0000000cu, 0x0000000au, 0x0000000bu,
   0x0000000au, 0x0000000bu, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
   0x00000005u, 0x0003003eu, 0x00000009u, 0x0000000cu, 0x000100fdu, 0x00010038u,
};

#define DEV_FNS(X)                                                             \
   X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory)            \
   X(BindBufferMemory) X(MapMemory) X(CreateImage)                             \
   X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView)         \
   X(CreateRenderPass) X(CreateFramebuffer) X(CreateShaderModule)              \
   X(CreatePipelineLayout) X(CreateGraphicsPipelines) X(CreateCommandPool)     \
   X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer)         \
   X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline)                \
   X(CmdSetDepthBounds) X(CmdDraw) X(CmdCopyImageToBuffer)                  \
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
static VkShaderModule sm_vs, sm_fs;
static VkFramebuffer fb;
static VkImage color_img, depth_img;
static VkBuffer rb_depth, rb_color;
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
make_image(VkFormat fmt, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
           VkImageAspectFlags aspect, VkImage *img, VkImageView *view)
{
   VkImageCreateInfo ii = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = fmt,
      .extent = {W, H, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = samples,
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

static PFN_vkCmdSetDepthBoundsTestEnable p_CmdSetDepthBoundsTestEnable;

enum mode { M_FILL, M_STATIC, M_DYN, M_DYN_EN, M_NOFS };

static int
make_pipe(enum mode mode, VkSampleCountFlagBits samples, VkRenderPass r,
          VkPipeline *out)
{
   VkPipelineShaderStageCreateInfo st[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = sm_vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = sm_fs, .pName = "main"}};
   VkPipelineVertexInputStateCreateInfo vis = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo ias = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP};
   /* M_NOFS: no FS, writes the constant depth 0.9 where the bounds pass. */
   VkViewport vp = {0.0f, 0.0f, (float)W, (float)H, mode == M_NOFS ? 0.9f : 0.0f,
                    mode == M_NOFS ? 0.9f : 1.0f};
   VkRect2D sc = {{0, 0}, {W, H}};
   VkPipelineViewportStateCreateInfo vs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = samples};
   VkPipelineDepthStencilStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = mode == M_FILL || mode == M_NOFS ? VK_TRUE : VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_ALWAYS,
      .depthBoundsTestEnable = mode == M_FILL ? VK_FALSE : VK_TRUE,
      .minDepthBounds = 0.3f, .maxDepthBounds = 0.6f};
   VkPipelineColorBlendAttachmentState cba = {
      .colorWriteMask = mode == M_FILL || mode == M_NOFS
                           ? 0
                           : VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
   VkPipelineColorBlendStateCreateInfo cbs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &cba};
   VkDynamicState dyn[2];
   uint32_t nd = 0;
   if (mode == M_DYN || mode == M_DYN_EN)
      dyn[nd++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
   if (mode == M_DYN_EN)
      dyn[nd++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
   VkPipelineDynamicStateCreateInfo dsi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = nd, .pDynamicStates = dyn};
   VkGraphicsPipelineCreateInfo gpi = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = mode == M_NOFS ? 1 : 2, .pStages = st,
      .pVertexInputState = &vis,
      .pInputAssemblyState = &ias, .pViewportState = &vs,
      .pRasterizationState = &rs, .pMultisampleState = &ms,
      .pDepthStencilState = &ds, .pColorBlendState = &cbs,
      .pDynamicState = nd ? &dsi : NULL, .layout = pl, .renderPass = r,
      .subpass = 0};
   CK(p_CreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, out),
      "CreateGraphicsPipelines");
   return 0;
}

/* Standard 4x sample x offsets, by sample index. */
static const float sx4[4] = {0.375f, 0.875f, 0.125f, 0.625f};

/* en: -1 leaves the enable static. kind 1: 4x pass resolved into color_img,
 * each pixel's green must match its count of in-bounds samples. kind 2: no
 * FS (M_NOFS), checked on the depth buffer. */
static int
run_case(const char *name, VkRenderPass r, VkFramebuffer f, int kind,
         VkPipeline fill, VkPipeline pipe, int dynb, float lo, float hi, int en,
         int expect_all)
{
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
      .renderPass = r, .framebuffer = f,
      .renderArea = {{0, 0}, {W, H}}, .clearValueCount = 2, .pClearValues = cv};
   p_CmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
   p_CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fill);
   p_CmdDraw(cmd, 4, 1, 0, 0);
   p_CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   if (dynb)
      p_CmdSetDepthBounds(cmd, lo, hi);
   if (en >= 0)
      p_CmdSetDepthBoundsTestEnable(cmd, en ? VK_TRUE : VK_FALSE);
   p_CmdDraw(cmd, 4, 1, 0, 0);
   p_CmdEndRenderPass(cmd);
   VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1},
                           .imageExtent = {W, H, 1}};
   if (kind != 1)
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

   uint32_t badC = 0, badD = 0, in = 0, out = 0, skipped = 0, partial = 0;
   for (uint32_t y = 0; kind == 1 && y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
         int n = 0, near = 0;
         for (int s = 0; s < 4; s++) {
            float d = (x + sx4[s]) / (float)W;
            near |= fabsf(d - lo) < 1e-4f || fabsf(d - hi) < 1e-4f;
            n += d >= lo && d <= hi;
         }
         if (near) {
            skipped++;
            continue;
         }
         const uint8_t *c = cmap + (y * W + x) * 4;
         int g = 255 * n / 4;
         if (c[0] != 0 || c[2] != 0 || abs((int)c[1] - g) > 2) {
            if (badC < 8)
               printf("BAD %s x=%u y=%u rgba=%u,%u,%u,%u want_g=%d\n", name, x,
                      y, c[0], c[1], c[2], c[3], g);
            badC++;
         }
         if (n == 4)
            in++;
         else if (n == 0)
            out++;
         else
            partial++;
      }
   }
   for (uint32_t y = 0; kind != 1 && y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
         float d = (x + 0.5f) / (float)W;
         int near = fabsf(d - lo) < 0.01f || fabsf(d - hi) < 0.01f;
         int want = expect_all || (d >= lo && d <= hi);
         /* kind 2: depth becomes 0.9 where the bounds pass, color untouched. */
         float wd = kind == 2 && want ? 0.9f : d;
         if (!(kind == 2 && near) && fabsf(dmap[y * W + x] - wd) > 1e-4f)
            badD++;
         if (!expect_all && near) {
            skipped++;
            continue;
         }
         const uint8_t *c = cmap + (y * W + x) * 4;
         int green = c[0] == 0 && c[1] == 255 && c[2] == 0 && c[3] == 255;
         int clear = c[0] == 0 && c[1] == 0 && c[2] == 0 && c[3] == 0;
         if (want && kind != 2 ? !green : !clear)
            badC++;
         if (want)
            in++;
         else
            out++;
      }
   }
   int ok = !badC && !badD && in && (expect_all || out) && (kind != 1 || partial);
   printf("%s case %s bounds=[%.2f,%.2f] checkedIn=%u checkedOut=%u "
          "partial=%u skipped=%u badColor=%u badDepth=%u\n",
          ok ? "PASS" : "FAIL", name, lo, hi, in, out, partial, skipped, badC,
          badD);
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

   VkPhysicalDeviceFeatures2 f2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
   GetPhysicalDeviceFeatures2(phys, &f2);
   printf("FEATURE depthBounds=%u\n", f2.features.depthBounds);
   if (!f2.features.depthBounds) {
      printf("FAIL depthBounds not reported\n");
      printf("RESULT FAIL\n");
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
   ef.depthBounds = VK_TRUE;
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
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
   {
      PFN_vkVoidFunction _p = GetDeviceProcAddr(dev, "vkCmdSetDepthBoundsTestEnable");
      memcpy(&p_CmdSetDepthBoundsTestEnable, &_p, sizeof(_p));
   }
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
   if (make_buf(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &rb_depth, &map))
      return 1;
   dmap = map;
   if (make_buf(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &rb_color, &map))
      return 1;
   cmap = map;
   VkImageView cview, dview;
   if (make_image(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_IMAGE_ASPECT_COLOR_BIT, &color_img, &cview) ||
       make_image(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
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
   VkShaderModuleCreateInfo smi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
#define MOD(var, arr, what)                                                    \
   smi.codeSize = sizeof(arr);                                                 \
   smi.pCode = arr;                                                            \
   CK(p_CreateShaderModule(dev, &smi, NULL, &(var)), what)
   MOD(sm_vs, vs_spv, "vs");
   MOD(sm_fs, fs_spv, "fs");
#undef MOD

   const VkSampleCountFlagBits S1 = VK_SAMPLE_COUNT_1_BIT;
   VkPipeline fill, st, dy, de;
   if (make_pipe(M_FILL, S1, rp, &fill) || make_pipe(M_STATIC, S1, rp, &st) ||
       make_pipe(M_DYN, S1, rp, &dy))
      return 1;
   int fails = 0;
   fails += run_case("A_static", rp, fb, 0, fill, st, 0, 0.3f, 0.6f, -1, 0);
   fails += run_case("B_dynamic", rp, fb, 0, fill, dy, 1, 0.3f, 0.6f, -1, 0);
   fails += run_case("C_dynamic_low", rp, fb, 0, fill, dy, 1, 0.1f, 0.2f, -1, 0);
   if (p_CmdSetDepthBoundsTestEnable) {
      if (make_pipe(M_DYN_EN, S1, rp, &de))
         return 1;
      fails += run_case("D_dyn_enable_off", rp, fb, 0, fill, de, 1, 0.3f, 0.6f,
                        0, 1);
      fails += run_case("E_dyn_enable_on", rp, fb, 0, fill, de, 1, 0.3f, 0.6f,
                        1, 0);
   } else {
      printf("SKIP cases D/E (vkCmdSetDepthBoundsTestEnable unavailable)\n");
   }

   VkPipeline nofs;
   if (make_pipe(M_NOFS, S1, rp, &nofs))
      return 1;
   fails += run_case("G_no_fs_static", rp, fb, 2, fill, nofs, 0, 0.3f, 0.6f, -1,
                     0);

   /* F: 4x MSAA, static bounds, resolved into color_img. */
   VkPhysicalDeviceProperties pp;
   GetPhysicalDeviceProperties(phys, &pp);
   const VkSampleCountFlagBits S4 = VK_SAMPLE_COUNT_4_BIT;
   if (!(pp.limits.framebufferColorSampleCounts & S4) ||
       !(pp.limits.framebufferDepthSampleCounts & S4) ||
       !pp.limits.standardSampleLocations) {
      printf("SKIP case F (no standard 4x MSAA)\n");
   } else {
      VkImage c4, d4;
      VkImageView c4v, d4v;
      if (make_image(VK_FORMAT_R8G8B8A8_UNORM, S4,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, &c4, &c4v) ||
          make_image(VK_FORMAT_D32_SFLOAT, S4,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT, &d4, &d4v))
         return 1;
      VkAttachmentDescription ad4[3] = {ad[0], ad[1], ad[0]};
      ad4[0].samples = S4;
      ad4[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      ad4[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      ad4[1].samples = S4;
      ad4[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      ad4[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      ad4[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      VkAttachmentReference rref = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkSubpassDescription sub4 = sub;
      sub4.pResolveAttachments = &rref;
      VkRenderPassCreateInfo rpi4 = rpi;
      rpi4.attachmentCount = 3;
      rpi4.pAttachments = ad4;
      rpi4.pSubpasses = &sub4;
      VkRenderPass rp4;
      CK(p_CreateRenderPass(dev, &rpi4, NULL, &rp4), "CreateRenderPass4");
      VkImageView atts4[3] = {c4v, d4v, cview};
      VkFramebufferCreateInfo fbi4 = fbi;
      fbi4.renderPass = rp4;
      fbi4.attachmentCount = 3;
      fbi4.pAttachments = atts4;
      VkFramebuffer fb4;
      CK(p_CreateFramebuffer(dev, &fbi4, NULL, &fb4), "CreateFramebuffer4");
      VkPipeline fill4, st4;
      if (make_pipe(M_FILL, S4, rp4, &fill4) ||
          make_pipe(M_STATIC, S4, rp4, &st4))
         return 1;
      fails += run_case("F_msaa4_static", rp4, fb4, 1, fill4, st4, 0, 0.3f,
                        0.6f, -1, 0);
   }

   printf("RESULT %s\n", fails ? "FAIL" : "PASS");
   return fails ? 1 : 0;
}
