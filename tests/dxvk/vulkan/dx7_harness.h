/* DX7 draw-test harness: direct ICD load, 64x64 RGBA8 target, one draw per
 * submit, final RGBA readback only. */
#define VK_NO_PROTOTYPES
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define RT_W 64
#define RT_H 64

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, _r, __LINE__);                 \
         exit(1);                                                              \
      }                                                                        \
   } while (0)

#define DX7_FUNCS(X)                                                           \
   X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties)                  \
   X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceQueueFamilyProperties)      \
   X(GetPhysicalDeviceMemoryProperties) X(CreateDevice) X(GetDeviceQueue)      \
   X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory)            \
   X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateImage)              \
   X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView)         \
   X(CreateRenderPass) X(CreateFramebuffer) X(CreateShaderModule)              \
   X(CreatePipelineLayout) X(CreateGraphicsPipelines) X(CreateCommandPool)     \
   X(AllocateCommandBuffers) X(BeginCommandBuffer) X(ResetCommandBuffer)       \
   X(CmdBeginRenderPass) X(CmdBindPipeline) X(CmdBindVertexBuffers)            \
   X(CmdBindIndexBuffer) X(CmdDraw) X(CmdDrawIndexed) X(CmdPushConstants)      \
   X(CmdSetViewport) X(CmdSetScissor) X(CmdEndRenderPass)                      \
   X(CmdCopyImageToBuffer) X(CmdPipelineBarrier) X(EndCommandBuffer)           \
   X(CreateFence) X(QueueSubmit) X(WaitForFences) X(ResetFences)               \
   X(DestroyPipeline) X(DestroyShaderModule)

#define DX7_DECL(n) static PFN_vk##n vk##n;
DX7_FUNCS(DX7_DECL)

struct dx7 {
   VkInstance inst;
   VkPhysicalDevice phys;
   VkPhysicalDeviceFeatures feats;
   VkPhysicalDeviceProperties props;
   VkDevice dev;
   VkQueue queue;
   uint32_t host_mi;
   VkImage img;
   VkRenderPass rp;
   VkFramebuffer fb;
   VkPipelineLayout layout;
   VkCommandBuffer cmd;
   VkFence fence;
   VkBuffer rbuf;
   VkDeviceMemory rmem;
   uint8_t *px;
};

static void
dx7_buffer(struct dx7 *t, VkDeviceSize size, VkBufferUsageFlags usage,
           const void *data, VkBuffer *buf, VkDeviceMemory *mem)
{
   VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = size,
                             .usage = usage,
                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateBuffer(t->dev, &bci, NULL, buf), "Buffer");
   VkMemoryRequirements mr;
   vkGetBufferMemoryRequirements(t->dev, *buf, &mr);
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex = t->host_mi};
   CK(vkAllocateMemory(t->dev, &mai, NULL, mem), "BufMem");
   CK(vkBindBufferMemory(t->dev, *buf, *mem, 0), "BufBind");
   if (data) {
      void *p;
      CK(vkMapMemory(t->dev, *mem, 0, size, 0, &p), "BufMap");
      memcpy(p, data, size);
      vkUnmapMemory(t->dev, *mem);
   }
}

static void
dx7_init(struct dx7 *t, const char *icd, const VkPhysicalDeviceFeatures *want)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   memset(t, 0, sizeof(*t));
   void *h = dlopen(icd, RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      exit(1);
   }
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkCreateInstance ci =
      (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   CK(ci(&ici, NULL, &t->inst), "CreateInstance");
#define DX7_LOAD(n)                                                            \
   vk##n = (PFN_vk##n)gipa(t->inst, "vk" #n);                                  \
   if (!vk##n) {                                                               \
      printf("FAIL missing vk" #n "\n");                                       \
      exit(1);                                                                 \
   }
   DX7_FUNCS(DX7_LOAD)

   uint32_t n = 8;
   VkPhysicalDevice devs[8];
   CK(vkEnumeratePhysicalDevices(t->inst, &n, devs), "enum");
   for (uint32_t i = 0; i < n; i++) {
      vkGetPhysicalDeviceProperties(devs[i], &t->props);
      if (strstr(t->props.deviceName, "Mali")) {
         t->phys = devs[i];
         break;
      }
   }
   if (!t->phys) {
      printf("FAIL no Mali\n");
      exit(1);
   }
   vkGetPhysicalDeviceFeatures(t->phys, &t->feats);
   printf("ICD device=%s geometryShader=%d fillModeNonSolid=%d "
          "multiViewport=%d shaderClipDistance=%d shaderCullDistance=%d "
          "maxViewports=%u maxClip=%u maxCull=%u maxCombined=%u\n",
          t->props.deviceName, t->feats.geometryShader,
          t->feats.fillModeNonSolid, t->feats.multiViewport,
          t->feats.shaderClipDistance, t->feats.shaderCullDistance,
          t->props.limits.maxViewports, t->props.limits.maxClipDistances,
          t->props.limits.maxCullDistances,
          t->props.limits.maxCombinedClipAndCullDistances);

   uint32_t qn = 8;
   VkQueueFamilyProperties qp[8];
   vkGetPhysicalDeviceQueueFamilyProperties(t->phys, &qn, qp);
   uint32_t qi = 0;
   while (qi < qn && !(qp[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      qi++;
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .pEnabledFeatures = want};
   CK(vkCreateDevice(t->phys, &dci, NULL, &t->dev), "CreateDevice");
   vkGetDeviceQueue(t->dev, qi, 0, &t->queue);

   VkPhysicalDeviceMemoryProperties mp;
   vkGetPhysicalDeviceMemoryProperties(t->phys, &mp);
   const VkMemoryPropertyFlags hv = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   t->host_mi = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
      if ((mp.memoryTypes[i].propertyFlags & hv) == hv) {
         t->host_mi = i;
         break;
      }

   VkImageCreateInfo imgci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = VK_FORMAT_R8G8B8A8_UNORM,
                              .extent = {RT_W, RT_H, 1},
                              .mipLevels = 1,
                              .arrayLayers = 1,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateImage(t->dev, &imgci, NULL, &t->img), "Image");
   VkMemoryRequirements imr;
   vkGetImageMemoryRequirements(t->dev, t->img, &imr);
   uint32_t imi = 0;
   while (!(imr.memoryTypeBits & (1u << imi)))
      imi++;
   VkDeviceMemory imem;
   VkMemoryAllocateInfo imai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = imr.size,
                                .memoryTypeIndex = imi};
   CK(vkAllocateMemory(t->dev, &imai, NULL, &imem), "ImgMem");
   CK(vkBindImageMemory(t->dev, t->img, imem, 0), "ImgBind");
   VkImageViewCreateInfo ivci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image = t->img,
                                 .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                 .format = VK_FORMAT_R8G8B8A8_UNORM,
                                 .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,
                                                      0, 1, 0, 1}};
   VkImageView view;
   CK(vkCreateImageView(t->dev, &ivci, NULL, &view), "View");

   VkAttachmentDescription att = {.format = VK_FORMAT_R8G8B8A8_UNORM,
                                  .samples = VK_SAMPLE_COUNT_1_BIT,
                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                  .finalLayout =
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &ref};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                  .attachmentCount = 1,
                                  .pAttachments = &att,
                                  .subpassCount = 1,
                                  .pSubpasses = &sub};
   CK(vkCreateRenderPass(t->dev, &rpci, NULL, &t->rp), "RP");
   VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                   .renderPass = t->rp,
                                   .attachmentCount = 1,
                                   .pAttachments = &view,
                                   .width = RT_W,
                                   .height = RT_H,
                                   .layers = 1};
   CK(vkCreateFramebuffer(t->dev, &fbci, NULL, &t->fb), "FB");

   VkPushConstantRange pcr = {VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, 16};
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pcr};
   CK(vkCreatePipelineLayout(t->dev, &plci, NULL, &t->layout), "Layout");

   VkCommandPoolCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi};
   VkCommandPool pool;
   CK(vkCreateCommandPool(t->dev, &cpci, NULL, &pool), "Pool");
   VkCommandBufferAllocateInfo cbai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(vkAllocateCommandBuffers(t->dev, &cbai, &t->cmd), "Cmd");
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(vkCreateFence(t->dev, &fci, NULL, &t->fence), "Fence");
   dx7_buffer(t, RT_W * RT_H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, NULL,
              &t->rbuf, &t->rmem);
   CK(vkMapMemory(t->dev, t->rmem, 0, RT_W * RT_H * 4, 0, (void **)&t->px),
      "RMap");
}

static VkShaderModule
dx7_module(struct dx7 *t, const uint32_t *code, size_t size)
{
   VkShaderModuleCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                  .codeSize = size,
                                  .pCode = code};
   VkShaderModule m;
   CK(vkCreateShaderModule(t->dev, &ci, NULL, &m), "Module");
   return m;
}

struct dx7_pipe_desc {
   const uint32_t *vs, *fs, *gs;
   size_t vs_size, fs_size, gs_size;
   VkPrimitiveTopology topology;
   VkPolygonMode polygon_mode;
   VkCullModeFlags cull_mode;
   uint32_t viewport_count;
   int dynamic_viewport;
   const VkViewport *viewports;
   const VkRect2D *scissors;
   uint32_t vertex_stride;
};

static VkPipeline
dx7_pipeline(struct dx7 *t, const struct dx7_pipe_desc *d)
{
   VkShaderModule vs = dx7_module(t, d->vs, d->vs_size);
   VkShaderModule fs = dx7_module(t, d->fs, d->fs_size);
   VkShaderModule gs = d->gs ? dx7_module(t, d->gs, d->gs_size) : VK_NULL_HANDLE;
   VkPipelineShaderStageCreateInfo stages[3] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = gs, .pName = "main"}};
   VkVertexInputBindingDescription vb = {0, d->vertex_stride ? d->vertex_stride : 16,
                                         VK_VERTEX_INPUT_RATE_VERTEX};
   VkVertexInputAttributeDescription va = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vb,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &va};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = d->topology};
   VkViewport vp1 = {0, 0, RT_W, RT_H, 0, 1};
   VkRect2D sc1 = {{0, 0}, {RT_W, RT_H}};
   uint32_t nvp = d->viewport_count ? d->viewport_count : 1;
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = nvp,
      .pViewports = d->viewports ? d->viewports : &vp1,
      .scissorCount = nvp,
      .pScissors = d->scissors ? d->scissors : &sc1};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = d->polygon_mode,
      .cullMode = d->cull_mode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState ba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &ba};
   VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn};
   VkGraphicsPipelineCreateInfo gpci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = d->gs ? 3 : 2,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .pDynamicState = d->dynamic_viewport ? &ds : NULL,
      .layout = t->layout,
      .renderPass = t->rp};
   VkPipeline p;
   CK(vkCreateGraphicsPipelines(t->dev, VK_NULL_HANDLE, 1, &gpci, NULL, &p),
      "GfxPipe");
   vkDestroyShaderModule(t->dev, vs, NULL);
   vkDestroyShaderModule(t->dev, fs, NULL);
   if (gs)
      vkDestroyShaderModule(t->dev, gs, NULL);
   return p;
}

typedef void (*dx7_record_fn)(struct dx7 *t, VkCommandBuffer cmd, void *data);

/* Clear to blue, record the draw, read back the target. */
static void
dx7_run(struct dx7 *t, VkPipeline pipe, VkBuffer vbuf, dx7_record_fn rec,
        void *data)
{
   CK(vkResetFences(t->dev, 1, &t->fence), "ResetFence");
   CK(vkResetCommandBuffer(t->cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(t->cmd, &bbi), "Begin");
   VkClearValue clear = {.color = {{0.0f, 0.0f, 1.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                 .renderPass = t->rp,
                                 .framebuffer = t->fb,
                                 .renderArea = {{0, 0}, {RT_W, RT_H}},
                                 .clearValueCount = 1,
                                 .pClearValues = &clear};
   vkCmdBeginRenderPass(t->cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindVertexBuffers(t->cmd, 0, 1, &vbuf, &off);
   rec(t, t->cmd, data);
   vkCmdEndRenderPass(t->cmd);
   VkImageMemoryBarrier bar = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                               .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = t->img,
                               .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,
                                                    0, 1, 0, 1}};
   vkCmdPipelineBarrier(t->cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                        &bar);
   VkBufferImageCopy region = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,
                                                    0, 0, 1},
                               .imageExtent = {RT_W, RT_H, 1}};
   vkCmdCopyImageToBuffer(t->cmd, t->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          t->rbuf, 1, &region);
   CK(vkEndCommandBuffer(t->cmd), "End");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &t->cmd};
   CK(vkQueueSubmit(t->queue, 1, &si, t->fence), "Submit");
   CK(vkWaitForFences(t->dev, 1, &t->fence, VK_TRUE, 30ull * 1000000000ull),
      "Wait");
}

static const uint8_t *
dx7_px(struct dx7 *t, int x, int y)
{
   return t->px + (y * RT_W + x) * 4;
}

static int
dx7_is_red(const uint8_t *p)
{
   return p[0] > 200 && p[1] < 60 && p[2] < 60;
}

static int
dx7_is_blue(const uint8_t *p)
{
   return p[2] > 200 && p[0] < 60 && p[1] < 60;
}

/* Compare the target against expect(x, y): 1 = red, 0 = blue, -1 = either
 * (line end points). Prints the
 * mismatch count and the first mismatch. */
static int
dx7_check(struct dx7 *t, const char *name, int (*expect)(int, int, void *),
          void *data)
{
   int bad = 0, red = 0, fx = -1, fy = -1;
   for (int y = 0; y < RT_H; y++)
      for (int x = 0; x < RT_W; x++) {
         const uint8_t *p = dx7_px(t, x, y);
         int want = expect(x, y, data);
         int ok = want < 0 ? 1 : want ? dx7_is_red(p) : dx7_is_blue(p);
         red += dx7_is_red(p);
         if (!ok && !bad++) {
            fx = x;
            fy = y;
         }
      }
   if (bad) {
      const uint8_t *p = dx7_px(t, fx, fy);
      printf("CASE %s red=%d mismatch=%d first=(%d,%d) RGBA=%u %u %u %u FAIL\n",
             name, red, bad, fx, fy, p[0], p[1], p[2], p[3]);
   } else {
      printf("CASE %s red=%d mismatch=0 PASS\n", name, red);
   }
   return bad != 0;
}
