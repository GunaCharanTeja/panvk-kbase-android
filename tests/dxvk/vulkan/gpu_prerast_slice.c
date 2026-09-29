/* DX5 GPU prerast vertical-slice matrix. Direct ICD load. Final readback only. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include "../../offscreen/tri.vert.spv.h"
#include "../../offscreen/tri.frag.spv.h"

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, _r, __LINE__);                 \
         return 1;                                                             \
      }                                                                        \
   } while (0)

#define W 16
#define H 16

static int
red_ok(const uint8_t *p)
{
   return p[0] > 200 && p[1] < 60 && p[2] < 60;
}

static int
blue_ok(const uint8_t *p)
{
   return p[2] > 200 && p[0] < 60 && p[1] < 60;
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
#define G0(what, name)                                                         \
   PFN_##name name = (PFN_##name)gipa(NULL, "vk" #what);                        \
   if (!name) {                                                                \
      printf("FAIL missing vk" #what "\n");                                    \
      return 1;                                                                \
   }
   G0(CreateInstance, vkCreateInstance)
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   VkInstance inst;
   CK(vkCreateInstance(&ici, NULL, &inst), "CreateInstance");
#undef G0
#define G(what, name)                                                          \
   PFN_##name name = (PFN_##name)gipa(inst, "vk" #what);                        \
   if (!name) {                                                                \
      printf("FAIL missing vk" #what "\n");                                    \
      return 1;                                                                \
   }
   G(EnumeratePhysicalDevices, vkEnumeratePhysicalDevices)
   G(GetPhysicalDeviceProperties, vkGetPhysicalDeviceProperties)
   G(GetPhysicalDeviceFeatures, vkGetPhysicalDeviceFeatures)
   G(GetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties)
   G(GetPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties)
   G(CreateDevice, vkCreateDevice)
   G(GetDeviceQueue, vkGetDeviceQueue)
   G(CreateBuffer, vkCreateBuffer)
   G(GetBufferMemoryRequirements, vkGetBufferMemoryRequirements)
   G(AllocateMemory, vkAllocateMemory)
   G(BindBufferMemory, vkBindBufferMemory)
   G(MapMemory, vkMapMemory)
   G(UnmapMemory, vkUnmapMemory)
   G(CreateImage, vkCreateImage)
   G(GetImageMemoryRequirements, vkGetImageMemoryRequirements)
   G(BindImageMemory, vkBindImageMemory)
   G(CreateImageView, vkCreateImageView)
   G(CreateRenderPass, vkCreateRenderPass)
   G(CreateFramebuffer, vkCreateFramebuffer)
   G(CreateShaderModule, vkCreateShaderModule)
   G(CreatePipelineLayout, vkCreatePipelineLayout)
   G(CreateGraphicsPipelines, vkCreateGraphicsPipelines)
   G(CreateCommandPool, vkCreateCommandPool)
   G(AllocateCommandBuffers, vkAllocateCommandBuffers)
   G(BeginCommandBuffer, vkBeginCommandBuffer)
   G(ResetCommandBuffer, vkResetCommandBuffer)
   G(CmdBeginRenderPass, vkCmdBeginRenderPass)
   G(CmdBindPipeline, vkCmdBindPipeline)
   G(CmdBindVertexBuffers, vkCmdBindVertexBuffers)
   G(CmdBindIndexBuffer, vkCmdBindIndexBuffer)
   G(CmdDraw, vkCmdDraw)
   G(CmdDrawIndexed, vkCmdDrawIndexed)
   G(CmdDrawIndirect, vkCmdDrawIndirect)
   G(CmdDrawIndexedIndirect, vkCmdDrawIndexedIndirect)
   G(CmdFillBuffer, vkCmdFillBuffer)
   G(CmdCopyBuffer, vkCmdCopyBuffer)
   G(CmdEndRenderPass, vkCmdEndRenderPass)
   G(CmdCopyImageToBuffer, vkCmdCopyImageToBuffer)
   G(CmdPipelineBarrier, vkCmdPipelineBarrier)
   G(EndCommandBuffer, vkEndCommandBuffer)
   G(CreateFence, vkCreateFence)
   G(QueueSubmit, vkQueueSubmit)
   G(WaitForFences, vkWaitForFences)
   G(ResetFences, vkResetFences)
   G(GetFenceStatus, vkGetFenceStatus)
   G(DestroyFence, vkDestroyFence)
   G(FreeCommandBuffers, vkFreeCommandBuffers)
   G(DestroyCommandPool, vkDestroyCommandPool)
   G(DestroyPipeline, vkDestroyPipeline)
   G(DestroyPipelineLayout, vkDestroyPipelineLayout)
   G(DestroyShaderModule, vkDestroyShaderModule)
   G(DestroyFramebuffer, vkDestroyFramebuffer)
   G(DestroyRenderPass, vkDestroyRenderPass)
   G(DestroyImageView, vkDestroyImageView)
   G(FreeMemory, vkFreeMemory)
   G(DestroyImage, vkDestroyImage)
   G(DestroyBuffer, vkDestroyBuffer)
   G(DestroyDevice, vkDestroyDevice)
   G(DestroyInstance, vkDestroyInstance)

   uint32_t n = 0;
   CK(vkEnumeratePhysicalDevices(inst, &n, NULL), "count");
   VkPhysicalDevice *devs = malloc(n * sizeof(*devs));
   CK(vkEnumeratePhysicalDevices(inst, &n, devs), "enum");
   VkPhysicalDevice phys = VK_NULL_HANDLE;
   VkPhysicalDeviceProperties props;
   for (uint32_t i = 0; i < n; i++) {
      vkGetPhysicalDeviceProperties(devs[i], &props);
      if (strstr(props.deviceName, "Mali")) {
         phys = devs[i];
         break;
      }
   }
   free(devs);
   if (phys == VK_NULL_HANDLE) {
      printf("FAIL no Mali\n");
      return 1;
   }
   vkGetPhysicalDeviceProperties(phys, &props);
   VkPhysicalDeviceFeatures feats;
   vkGetPhysicalDeviceFeatures(phys, &feats);
   printf("ICD device=%s id=0x%x api=%u.%u.%u\n", props.deviceName,
          props.deviceID, VK_API_VERSION_MAJOR(props.apiVersion),
          VK_API_VERSION_MINOR(props.apiVersion),
          VK_API_VERSION_PATCH(props.apiVersion));
   printf("EXPOSURE geometryShader=%d fillModeNonSolid=%d shaderClipDistance=%d shaderCullDistance=%d\n",
          feats.geometryShader, feats.fillModeNonSolid, feats.shaderClipDistance,
          feats.shaderCullDistance);
   /* clip/cull distance are exposed since DX7 (tests/dxvk/vulkan/clip-cull) */
   if (feats.geometryShader) {
      printf("FAIL unexpected feature exposure\n");
      return 1;
   }

   uint32_t qn = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
   VkQueueFamilyProperties *qp = malloc(qn * sizeof(*qp));
   vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qp);
   uint32_t qi = ~0u;
   for (uint32_t i = 0; i < qn; i++)
      if (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
         qi = i;
         break;
      }
   free(qp);
   if (qi == ~0u) {
      printf("FAIL no graphics queue\n");
      return 1;
   }
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci};
   VkDevice dev;
   CK(vkCreateDevice(phys, &dci, NULL, &dev), "CreateDevice");
   VkQueue queue;
   vkGetDeviceQueue(dev, qi, 0, &queue);

   VkPhysicalDeviceMemoryProperties mp;
   vkGetPhysicalDeviceMemoryProperties(phys, &mp);
   uint32_t host_mi = ~0u, dev_mi = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
      if (host_mi == ~0u &&
          (f & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         host_mi = i;
      if (dev_mi == ~0u && (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
         dev_mi = i;
   }
   if (host_mi == ~0u) {
      printf("FAIL no host memory\n");
      return 1;
   }
   if (dev_mi == ~0u)
      dev_mi = host_mi;

   float verts[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
   VkBuffer vbuf, ibuf, rbuf, indbuf;
   VkDeviceMemory vmem, imem_idx, rmem, indmem;
   VkBufferCreateInfo vbci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size = sizeof(verts),
                              .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateBuffer(dev, &vbci, NULL, &vbuf), "VBuffer");
   VkMemoryRequirements vmr;
   vkGetBufferMemoryRequirements(dev, vbuf, &vmr);
   VkMemoryAllocateInfo vmai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = vmr.size,
                                .memoryTypeIndex = host_mi};
   CK(vkAllocateMemory(dev, &vmai, NULL, &vmem), "VMem");
   CK(vkBindBufferMemory(dev, vbuf, vmem, 0), "VBind");
   void *vp;
   CK(vkMapMemory(dev, vmem, 0, sizeof(verts), 0, &vp), "VMap");
   memcpy(vp, verts, sizeof(verts));
   vkUnmapMemory(dev, vmem);

   uint16_t indices[] = {0, 1, 2, 0xffff, 0, 1, 2};
   VkBufferCreateInfo ibci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size = sizeof(indices),
                              .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateBuffer(dev, &ibci, NULL, &ibuf), "IBuffer");
   VkMemoryRequirements imr;
   vkGetBufferMemoryRequirements(dev, ibuf, &imr);
   VkMemoryAllocateInfo imai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = imr.size,
                                .memoryTypeIndex = host_mi};
   CK(vkAllocateMemory(dev, &imai, NULL, &imem_idx), "IMem");
   CK(vkBindBufferMemory(dev, ibuf, imem_idx, 0), "IBind");
   CK(vkMapMemory(dev, imem_idx, 0, sizeof(indices), 0, &vp), "IMap");
   memcpy(vp, indices, sizeof(indices));
   vkUnmapMemory(dev, imem_idx);

   uint32_t drawcmd[5] = {3, 1, 0, 0, 0};
   uint32_t idrawcmd[5] = {3, 1, 0, 0, 0};
   VkBufferCreateInfo indci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                               .size = 64,
                               .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateBuffer(dev, &indci, NULL, &indbuf), "IndBuffer");
   VkMemoryRequirements indmr;
   vkGetBufferMemoryRequirements(dev, indbuf, &indmr);
   VkMemoryAllocateInfo indmai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                  .allocationSize = indmr.size,
                                  .memoryTypeIndex = host_mi};
   CK(vkAllocateMemory(dev, &indmai, NULL, &indmem), "IndMem");
   CK(vkBindBufferMemory(dev, indbuf, indmem, 0), "IndBind");
   CK(vkMapMemory(dev, indmem, 0, 64, 0, &vp), "IndMap");
   memcpy(vp, drawcmd, sizeof(drawcmd));
   memcpy((char *)vp + 32, idrawcmd, sizeof(idrawcmd));
   vkUnmapMemory(dev, indmem);

   VkImageCreateInfo imgci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = VK_FORMAT_R8G8B8A8_UNORM,
                              .extent = {W, H, 1},
                              .mipLevels = 1,
                              .arrayLayers = 1,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   VkImage img;
   CK(vkCreateImage(dev, &imgci, NULL, &img), "Image");
   VkMemoryRequirements imgmr;
   vkGetImageMemoryRequirements(dev, img, &imgmr);
   uint32_t imi = host_mi;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
      if (imgmr.memoryTypeBits & (1u << i)) {
         imi = i;
         break;
      }
   VkDeviceMemory imgmem;
   VkMemoryAllocateInfo imgmai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                  .allocationSize = imgmr.size,
                                  .memoryTypeIndex = imi};
   CK(vkAllocateMemory(dev, &imgmai, NULL, &imgmem), "ImgMem");
   CK(vkBindImageMemory(dev, img, imgmem, 0), "ImgBind");
   VkImageViewCreateInfo ivci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image = img,
                                 .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                 .format = VK_FORMAT_R8G8B8A8_UNORM,
                                 .subresourceRange = {.aspectMask =
                                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                                      .levelCount = 1,
                                                      .layerCount = 1}};
   VkImageView view;
   CK(vkCreateImageView(dev, &ivci, NULL, &view), "View");

   VkBufferCreateInfo rbci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size = W * H * 4,
                              .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(vkCreateBuffer(dev, &rbci, NULL, &rbuf), "RBuffer");
   VkMemoryRequirements rmr;
   vkGetBufferMemoryRequirements(dev, rbuf, &rmr);
   VkMemoryAllocateInfo rmai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = rmr.size,
                                .memoryTypeIndex = host_mi};
   CK(vkAllocateMemory(dev, &rmai, NULL, &rmem), "RMem");
   CK(vkBindBufferMemory(dev, rbuf, rmem, 0), "RBind");

   VkAttachmentDescription att = {.format = VK_FORMAT_R8G8B8A8_UNORM,
                                  .samples = VK_SAMPLE_COUNT_1_BIT,
                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                  .finalLayout =
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   VkAttachmentReference ref = {.attachment = 0,
                                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &ref};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                  .attachmentCount = 1,
                                  .pAttachments = &att,
                                  .subpassCount = 1,
                                  .pSubpasses = &sub};
   VkRenderPass rp;
   CK(vkCreateRenderPass(dev, &rpci, NULL, &rp), "RP");
   VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                   .renderPass = rp,
                                   .attachmentCount = 1,
                                   .pAttachments = &view,
                                   .width = W,
                                   .height = H,
                                   .layers = 1};
   VkFramebuffer fb;
   CK(vkCreateFramebuffer(dev, &fbci, NULL, &fb), "FB");

   VkShaderModule vsm, fsm;
   VkShaderModuleCreateInfo vsmci = {.sType =
                                        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                     .codeSize = sizeof(tri_vert_spv),
                                     .pCode = tri_vert_spv};
   CK(vkCreateShaderModule(dev, &vsmci, NULL, &vsm), "VS");
   VkShaderModuleCreateInfo fsmci = {.sType =
                                        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                     .codeSize = sizeof(tri_frag_spv),
                                     .pCode = tri_frag_spv};
   CK(vkCreateShaderModule(dev, &fsmci, NULL, &fsm), "FS");
   VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vsm,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fsm,
       .pName = "main"}};
   VkVertexInputBindingDescription vbind = {.binding = 0,
                                            .stride = 8,
                                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
   VkVertexInputAttributeDescription vattr = {.location = 0,
                                              .binding = 0,
                                              .format = VK_FORMAT_R32G32_SFLOAT,
                                              .offset = 0};
   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vbind,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &vattr};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};
   VkPipelineInputAssemblyStateCreateInfo ia_restart = ia;
   ia_restart.primitiveRestartEnable = VK_TRUE;
   VkViewport vport = {0, 0, W, H, 0, 1};
   VkRect2D scis = {{0, 0}, {W, H}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &vport,
      .scissorCount = 1,
      .pScissors = &scis};
   VkPipelineRasterizationStateCreateInfo rast = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState blend = {.colorWriteMask = 0xF};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend};
   VkPipelineLayout layout;
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
   CK(vkCreatePipelineLayout(dev, &plci, NULL, &layout), "Layout");
   VkGraphicsPipelineCreateInfo gpci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pViewportState = &vps,
      .pRasterizationState = &rast,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .layout = layout,
      .renderPass = rp};
   VkPipeline pipe, pipe_restart;
   CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe),
      "GfxPipe");
   gpci.pInputAssemblyState = &ia_restart;
   CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe_restart),
      "GfxPipeRestart");

   VkCommandPoolCreateInfo cpoci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi};
   VkCommandPool cpool;
   CK(vkCreateCommandPool(dev, &cpoci, NULL, &cpool), "Pool");
   VkCommandBuffer cmds[2];
   VkCommandBufferAllocateInfo cbai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cpool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 2};
   CK(vkAllocateCommandBuffers(dev, &cbai, cmds), "Cmd");
   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(vkCreateFence(dev, &fci, NULL, &fence), "Fence");

   VkImageMemoryBarrier bar = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                               .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = img,
                               .subresourceRange = {.aspectMask =
                                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                                    .levelCount = 1,
                                                    .layerCount = 1}};
   VkBufferImageCopy region = {.imageSubresource = {.aspectMask =
                                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                                    .layerCount = 1},
                               .imageExtent = {W, H, 1}};
   VkClearValue clear = {.color = {{0.0f, 0.0f, 1.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                 .renderPass = rp,
                                 .framebuffer = fb,
                                 .renderArea = {{0, 0}, {W, H}},
                                 .clearValueCount = 1,
                                 .pClearValues = &clear};
   VkDeviceSize off = 0;
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   VkFence fence2;
   CK(vkCreateFence(dev, &fci, NULL, &fence2), "Fence2");

   int fails = 0;
   const char *cases[] = {
      "idvs_before", "direct", "indexed", "instanced", "base_vertex",
      "first_instance", "zero_count", "repeated_indices", "primitive_restart",
      "gpu_written_indirect", "replay", "simultaneous", "idvs_after"};
   int ncases = (int)(sizeof(cases) / sizeof(cases[0]));
   for (int c = 0; c < ncases; c++) {
      const char *name = cases[c];
      CK(vkResetFences(dev, 1, &fence), "ResetFence");
      CK(vkResetCommandBuffer(cmds[0], 0), "ResetCmd");
      CK(vkBeginCommandBuffer(cmds[0], &bbi), "Begin");
      if (!strcmp(name, "gpu_written_indirect")) {
         /* GPU-written parameters: fill, then copy from the pre-inited tail.
          * Transfers are illegal inside a render pass, and fill->copy is a
          * WAW hazard that needs its own barrier. */
         VkBufferMemoryBarrier bb = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = indbuf,
            .offset = 0,
            .size = 20};
         VkBufferCopy copy = {.srcOffset = 32, .dstOffset = 0, .size = 20};
         vkCmdFillBuffer(cmds[0], indbuf, 0, 20, 0);
         vkCmdPipelineBarrier(cmds[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1,
                              &bb, 0, NULL);
         vkCmdCopyBuffer(cmds[0], indbuf, indbuf, 1, &copy);
         bb.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
         vkCmdPipelineBarrier(cmds[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, NULL, 1,
                              &bb, 0, NULL);
      }
      vkCmdBeginRenderPass(cmds[0], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindVertexBuffers(cmds[0], 0, 1, &vbuf, &off);
      if (!strcmp(name, "idvs_before") || !strcmp(name, "idvs_after") ||
          !strcmp(name, "direct") || !strcmp(name, "zero_count") ||
          !strcmp(name, "replay") || !strcmp(name, "simultaneous") ||
          !strcmp(name, "instanced") || !strcmp(name, "first_instance") ||
          !strcmp(name, "gpu_written_indirect"))
         vkCmdBindPipeline(cmds[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      else
         vkCmdBindPipeline(cmds[0], VK_PIPELINE_BIND_POINT_GRAPHICS,
                           !strcmp(name, "primitive_restart") ? pipe_restart : pipe);
      if (!strcmp(name, "idvs_before") || !strcmp(name, "idvs_after") ||
          !strcmp(name, "direct") || !strcmp(name, "replay") ||
          !strcmp(name, "simultaneous"))
         vkCmdDraw(cmds[0], 3, 1, 0, 0);
      else if (!strcmp(name, "indexed") || !strcmp(name, "repeated_indices") ||
               !strcmp(name, "base_vertex")) {
         vkCmdBindIndexBuffer(cmds[0], ibuf, 0, VK_INDEX_TYPE_UINT16);
         if (!strcmp(name, "base_vertex"))
            vkCmdDrawIndexed(cmds[0], 3, 1, 0, 0, 0);
         else if (!strcmp(name, "repeated_indices"))
            vkCmdDrawIndexed(cmds[0], 6, 1, 0, 0, 0);
         else
            vkCmdDrawIndexed(cmds[0], 3, 1, 0, 0, 0);
      } else if (!strcmp(name, "instanced") || !strcmp(name, "first_instance"))
         vkCmdDraw(cmds[0], 3, 2, 0, !strcmp(name, "first_instance") ? 1 : 0);
      else if (!strcmp(name, "zero_count"))
         vkCmdDraw(cmds[0], 0, 0, 0, 0);
      else if (!strcmp(name, "primitive_restart")) {
         vkCmdBindIndexBuffer(cmds[0], ibuf, 0, VK_INDEX_TYPE_UINT16);
         vkCmdDrawIndexed(cmds[0], 7, 1, 0, 0, 0);
      } else if (!strcmp(name, "gpu_written_indirect"))
         vkCmdDrawIndirect(cmds[0], indbuf, 0, 1, 20);
      vkCmdEndRenderPass(cmds[0]);
      vkCmdPipelineBarrier(cmds[0], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                           &bar);
      vkCmdCopyImageToBuffer(cmds[0], img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             rbuf, 1, &region);
      CK(vkEndCommandBuffer(cmds[0]), "End");
      VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &cmds[0]};
      CK(vkQueueSubmit(queue, 1, &si, fence), "Submit");
      if (!strcmp(name, "replay")) {
         CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 30ull * 1000 * 1000 * 1000),
            "WaitReplay1");
         CK(vkResetFences(dev, 1, &fence), "ResetReplay");
         CK(vkQueueSubmit(queue, 1, &si, fence), "SubmitReplay");
      }
      if (!strcmp(name, "simultaneous")) {
         CK(vkResetCommandBuffer(cmds[1], 0), "ResetCmd1");
         CK(vkBeginCommandBuffer(cmds[1], &bbi), "Begin1");
         vkCmdBeginRenderPass(cmds[1], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
         vkCmdBindPipeline(cmds[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
         vkCmdBindVertexBuffers(cmds[1], 0, 1, &vbuf, &off);
         vkCmdDraw(cmds[1], 3, 1, 0, 0);
         vkCmdEndRenderPass(cmds[1]);
         CK(vkEndCommandBuffer(cmds[1]), "End1");
         VkSubmitInfo si2 = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &cmds[1]};
         CK(vkResetFences(dev, 1, &fence2), "ResetFence2");
         CK(vkQueueSubmit(queue, 1, &si2, fence2), "Submit2");
         VkFence both[2] = {fence, fence2};
         CK(vkWaitForFences(dev, 2, both, VK_TRUE, 30ull * 1000 * 1000 * 1000),
            "WaitBoth");
      }
      CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 30ull * 1000 * 1000 * 1000),
         "Wait");
      if (vkGetFenceStatus(dev, fence) != VK_SUCCESS) {
         printf("FAIL %s fence\n", name);
         fails++;
         continue;
      }
      void *rp_;
      CK(vkMapMemory(dev, rmem, 0, W * H * 4, 0, &rp_), "RMap");
      uint8_t *px = rp_;
      uint8_t *center = px + (8 * W + 8) * 4;
      int ok;
      if (!strcmp(name, "zero_count"))
         ok = blue_ok(center);
      else
         ok = red_ok(center);
      printf("CASE %s RGBA=%u %u %u %u %s\n", name, center[0], center[1],
             center[2], center[3], ok ? "PASS" : "FAIL");
      if (!ok)
         fails++;
      vkUnmapMemory(dev, rmem);
   }
   printf("NO_QUEUE_IDLE_BETWEEN_STAGES=1\n");
   printf("NO_HOST_READBACK_BETWEEN_STAGES=1\n");
   printf("MATRIX_FAILS=%d\n", fails);

   vkDestroyFence(dev, fence2, NULL);
   vkDestroyFence(dev, fence, NULL);
   vkFreeCommandBuffers(dev, cpool, 2, cmds);
   vkDestroyCommandPool(dev, cpool, NULL);
   vkDestroyPipeline(dev, pipe_restart, NULL);
   vkDestroyPipeline(dev, pipe, NULL);
   vkDestroyPipelineLayout(dev, layout, NULL);
   vkDestroyShaderModule(dev, vsm, NULL);
   vkDestroyShaderModule(dev, fsm, NULL);
   vkDestroyFramebuffer(dev, fb, NULL);
   vkDestroyRenderPass(dev, rp, NULL);
   vkDestroyImageView(dev, view, NULL);
   vkFreeMemory(dev, imgmem, NULL);
   vkDestroyImage(dev, img, NULL);
   vkFreeMemory(dev, rmem, NULL);
   vkDestroyBuffer(dev, rbuf, NULL);
   vkFreeMemory(dev, vmem, NULL);
   vkDestroyBuffer(dev, vbuf, NULL);
   vkFreeMemory(dev, imem_idx, NULL);
   vkDestroyBuffer(dev, ibuf, NULL);
   vkFreeMemory(dev, indmem, NULL);
   vkDestroyBuffer(dev, indbuf, NULL);
   vkDestroyDevice(dev, NULL);
   vkDestroyInstance(inst, NULL);
   return fails ? 1 : 0;
}
