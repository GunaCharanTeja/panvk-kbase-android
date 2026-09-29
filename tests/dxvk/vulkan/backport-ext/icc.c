/* Backport device probe: VK_EXT_image_compression_control + AFBC DRM-modifier
 * images (upstream 50bda0b8978 enables AFBC for WSI/modifier images by
 * default). Direct ICD load. For each case: create a 64x64 image, GPU clear
 * + full-screen draw via dynamic rendering (tile writeback), GPU copy to a
 * buffer, verify every texel on the host, and report the compression the
 * driver says it applied. Build SPIR-V first: sh build_spv.sh
 * usage: icc <icd.so> */
#define VK_NO_PROTOTYPES
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "fs_spv.h"

#define W 64
#define H 64

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, _r, __LINE__);                 \
         exit(1);                                                              \
      }                                                                        \
   } while (0)

#define FUNCS(X)                                                               \
   X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties)                  \
   X(GetPhysicalDeviceFeatures2) X(EnumerateDeviceExtensionProperties)         \
   X(GetPhysicalDeviceQueueFamilyProperties)                                   \
   X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceFormatProperties2)  \
   X(GetPhysicalDeviceImageFormatProperties2) X(CreateDevice) X(GetDeviceQueue) \
   X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory)            \
   X(BindBufferMemory) X(MapMemory) X(CreateImage) X(DestroyImage)             \
   X(FreeMemory) X(GetImageMemoryRequirements) X(BindImageMemory)              \
   X(CreateImageView) X(DestroyImageView) X(CreateCommandPool)                 \
   X(AllocateCommandBuffers) X(BeginCommandBuffer) X(ResetCommandBuffer)       \
   X(EndCommandBuffer) X(CmdPipelineBarrier) X(CmdBeginRendering)              \
   X(CmdEndRendering) X(CmdCopyImageToBuffer) X(CreateFence) X(QueueSubmit)    \
   X(WaitForFences) X(ResetFences) X(CreateShaderModule)                      \
   X(CreatePipelineLayout) X(CreateGraphicsPipelines) X(CmdBindPipeline)       \
   X(CmdSetViewport) X(CmdSetScissor) X(CmdPushConstants) X(CmdDraw)

#define DECL(n) static PFN_vk##n vk##n;
FUNCS(DECL)
static PFN_vkGetImageSubresourceLayout2EXT vkGetImageSubresourceLayout2EXT_;
static PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmModProps;

static VkPhysicalDevice pd;
static VkDevice dev;
static VkQueue q;
static uint32_t host_mi, dev_mi;
static VkCommandBuffer cmd;
static VkCommandPool pool;

/* Fresh command buffer per submit unless ICC_RESET=1 (reset-and-reuse is the
 * path that faults on kbase, see release()). */
static void
next_cmd(void)
{
   if (getenv("ICC_RESET")) {
      CK(vkResetCommandBuffer(cmd, 0), "reset");
      return;
   }
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(vkAllocateCommandBuffers(dev, &cai, &cmd), "cmd");
}
static VkFence fence;
static VkBuffer rbuf;
static int has_icc;
static uint8_t *rpx;

static int
has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
   for (uint32_t i = 0; i < n; i++)
      if (!strcmp(e[i].extensionName, name))
         return 1;
   return 0;
}

static const char *
cflags_str(VkImageCompressionFlagsEXT f)
{
   switch (f) {
   case VK_IMAGE_COMPRESSION_DEFAULT_EXT: return "DEFAULT";
   case VK_IMAGE_COMPRESSION_DISABLED_EXT: return "DISABLED";
   case VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT: return "FIXED_RATE_DEFAULT";
   case VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT: return "FIXED_RATE_EXPLICIT";
   default: return "?";
   }
}

static VkPipelineLayout playout;
static VkPipeline pipes[2];
static VkFormat pipe_fmt[2];

static VkPipeline
get_pipeline(VkFormat fmt)
{
   for (int i = 0; i < 2; i++)
      if (pipes[i] && pipe_fmt[i] == fmt)
         return pipes[i];
   if (!playout) {
      VkPushConstantRange pcr = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
      VkPipelineLayoutCreateInfo lci = {
         .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
         .pushConstantRangeCount = 1,
         .pPushConstantRanges = &pcr};
      CK(vkCreatePipelineLayout(dev, &lci, NULL, &playout), "layout");
   }
   VkShaderModuleCreateInfo vs_ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(fs_vert_spv),
      .pCode = fs_vert_spv};
   VkShaderModuleCreateInfo fs_ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(fs_frag_spv),
      .pCode = fs_frag_spv};
   VkShaderModule vs, fs;
   CK(vkCreateShaderModule(dev, &vs_ci, NULL, &vs), "vs");
   CK(vkCreateShaderModule(dev, &fs_ci, NULL, &fs), "fs");
   VkPipelineShaderStageCreateInfo st[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &cba};
   VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn};
   VkPipelineRenderingCreateInfo prci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &fmt};
   VkGraphicsPipelineCreateInfo gci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &prci,
      .stageCount = 2,
      .pStages = st,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .pDynamicState = &ds,
      .layout = playout};
   int i = pipes[0] ? 1 : 0;
   CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gci, NULL, &pipes[i]),
      "pipeline");
   pipe_fmt[i] = fmt;
   return pipes[i];
}

/* Render the case colour on the GPU (clear + full-screen draw; clear only
 * with ICC_CLEARONLY=1), copy to rbuf, verify every texel. */
static int
clear_copy_verify(VkImage img, VkFormat fmt, const float col[4])
{
   VkImageViewCreateInfo vci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = fmt,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   VkImageView view;
   CK(vkCreateImageView(dev, &vci, NULL, &view), "view");

   next_cmd();
   VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(cmd, &bi), "begin");
   VkImageMemoryBarrier b = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = img,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                        NULL, 0, NULL, 1, &b);
   VkRenderingAttachmentInfo att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue.color.float32 = {col[0], col[1], col[2], col[3]}};
   VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                         .renderArea = {{0, 0}, {W, H}},
                         .layerCount = 1,
                         .colorAttachmentCount = 1,
                         .pColorAttachments = &att};
   int clear_only = getenv("ICC_CLEARONLY") != NULL;
   if (!clear_only) {
      /* clear to black, then a full-screen draw writes the case colour */
      att.clearValue.color.float32[0] = 0.0f;
      att.clearValue.color.float32[1] = 0.0f;
      att.clearValue.color.float32[2] = 0.0f;
   }
   vkCmdBeginRendering(cmd, &ri);
   if (!clear_only) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, get_pipeline(fmt));
      VkViewport vp = {0, 0, W, H, 0, 1};
      VkRect2D sc = {{0, 0}, {W, H}};
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkCmdSetScissor(cmd, 0, 1, &sc);
      vkCmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         16, col);
      vkCmdDraw(cmd, 3, 1, 0, 0);
   }
   vkCmdEndRendering(cmd);
   b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
   b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                        &b);
   /* separate submits so a fault is attributed to clear or copy */
   CK(vkEndCommandBuffer(cmd), "end_clear");
   VkSubmitInfo si0 = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &cmd};
   CK(vkQueueSubmit(q, 1, &si0, fence), "submit_clear");
   CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull), "wait_clear");
   vkResetFences(dev, 1, &fence);
   next_cmd();
   CK(vkBeginCommandBuffer(cmd, &bi), "begin2");
   VkBufferImageCopy reg = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                 0, 1},
                            .imageExtent = {W, H, 1}};
   vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rbuf,
                          1, &reg);
   VkBufferMemoryBarrier bb = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .buffer = rbuf,
                               .size = VK_WHOLE_SIZE};
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0,
                        NULL);
   CK(vkEndCommandBuffer(cmd), "end");
   memset(rpx, 0xcd, W * H * 4);
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &cmd};
   CK(vkQueueSubmit(q, 1, &si, fence), "submit");
   CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull), "wait");
   vkResetFences(dev, 1, &fence);
   vkDestroyImageView(dev, view, NULL);

   int bgra = fmt == VK_FORMAT_B8G8R8A8_UNORM;
   uint8_t exp[4];
   for (int c = 0; c < 4; c++)
      exp[c] = (uint8_t)(col[bgra && c != 3 ? 2 - c : c] * 255.0f + 0.5f);
   int bad = 0;
   for (int i = 0; i < W * H; i++)
      for (int c = 0; c < 4; c++) {
         int d = rpx[i * 4 + c] - exp[c];
         if (d < -1 || d > 1)
            bad++;
      }
   return bad;
}

static VkDeviceMemory
bind_image(VkImage img)
{
   VkMemoryRequirements mr;
   vkGetImageMemoryRequirements(dev, img, &mr);
   uint32_t mi = getenv("ICC_MEMTYPE") ? (uint32_t)atoi(getenv("ICC_MEMTYPE"))
                                       : dev_mi;
   if (!(mr.memoryTypeBits & (1u << mi)))
      mi = __builtin_ctz(mr.memoryTypeBits);
   if (getenv("ICC_VERBOSE"))
      printf("IMGMEM size=%llu align=%llu bits=0x%x type=%u\n",
             (unsigned long long)mr.size, (unsigned long long)mr.alignment,
             mr.memoryTypeBits, mi);
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex = mi};
   VkDeviceMemory m;
   CK(vkAllocateMemory(dev, &mai, NULL, &m), "imgmem");
   CK(vkBindImageMemory(dev, img, m, 0), "imgbind");
   return m;
}

/* Images stay alive until exit by default: on kbase, freeing an image's
 * memory and allocating the same size again fails in the BO mmap with ENOMEM
 * (pre-existing kbase BO bug, independent of AFBC). ICC_FREE=1 reproduces. */
static void
release(VkImage img, VkDeviceMemory m)
{
   if (!getenv("ICC_FREE"))
      return;
   vkDestroyImage(dev, img, NULL);
   vkFreeMemory(dev, m, NULL);
}

static VkImageCompressionPropertiesEXT
image_compression(VkImage img)
{
   VkImageCompressionPropertiesEXT cp = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT};
   if (!has_icc)
      return cp;
   VkSubresourceLayout2EXT sl = {.sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_EXT,
                                 .pNext = &cp};
   VkImageSubresource2EXT sr = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0}};
   vkGetImageSubresourceLayout2EXT_(dev, img, &sr, &sl);
   return cp;
}

int
main(int argc, char **argv)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argc > 1 ? argv[1] : "libvulkan_panfrost.so", RTLD_NOW);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      return 1;
   }
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   VkInstance inst;
   CK(ci(&ici, NULL, &inst), "CreateInstance");
#define LOAD(n)                                                                \
   vk##n = (PFN_vk##n)gipa(inst, "vk" #n);                                     \
   if (!vk##n) {                                                               \
      printf("FAIL missing vk" #n "\n");                                       \
      return 1;                                                                \
   }
   FUNCS(LOAD)

   uint32_t nd = 8;
   VkPhysicalDevice pds[8];
   CK(vkEnumeratePhysicalDevices(inst, &nd, pds), "enum");
   VkPhysicalDeviceProperties props;
   for (uint32_t i = 0; i < nd; i++) {
      vkGetPhysicalDeviceProperties(pds[i], &props);
      if (strstr(props.deviceName, "Mali")) {
         pd = pds[i];
         break;
      }
   }
   if (!pd) {
      printf("FAIL no Mali\n");
      return 1;
   }
   uint32_t ne = 0;
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
   VkExtensionProperties *de = calloc(ne, sizeof(*de));
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, de);
   has_icc = has_ext(de, ne, VK_EXT_IMAGE_COMPRESSION_CONTROL_EXTENSION_NAME);
   int has_mod = has_ext(de, ne, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
   VkPhysicalDeviceImageCompressionControlFeaturesEXT iccf = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT};
   VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   .pNext = &iccf};
   vkGetPhysicalDeviceFeatures2(pd, &f2);
   printf("ICD device=%s EXT_image_compression_control=%d imageCompressionControl=%d "
          "EXT_image_drm_format_modifier=%d\n",
          props.deviceName, has_icc, iccf.imageCompressionControl, has_mod);
   /* ICC_BASELINE=1: run the same GPU cases on an ICD without the extension
    * (no control structs chained, compression not queried). */
   int baseline = !has_icc && getenv("ICC_BASELINE");
   if ((!has_icc || !iccf.imageCompressionControl) && !baseline) {
      printf("ICC_FAILS=1\n");
      return 1;
   }

   uint32_t qn = 8;
   VkQueueFamilyProperties qp[8];
   vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qp);
   uint32_t qi = 0;
   while (qi < qn && !(qp[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      qi++;
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = qi,
      .queueCount = 1,
      .pQueuePriorities = &prio};
   const char *dexts[2];
   uint32_t ndext = 0;
   if (has_icc)
      dexts[ndext++] = VK_EXT_IMAGE_COMPRESSION_CONTROL_EXTENSION_NAME;
   if (has_mod)
      dexts[ndext++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
   VkPhysicalDeviceVulkan13Features v13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = has_icc ? &iccf : NULL,
      .dynamicRendering = VK_TRUE};
   iccf.pNext = NULL;
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .pNext = &v13,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .enabledExtensionCount = ndext,
                             .ppEnabledExtensionNames = dexts};
   CK(vkCreateDevice(pd, &dci, NULL, &dev), "CreateDevice");
   vkGetDeviceQueue(dev, qi, 0, &q);
   PFN_vkGetDeviceProcAddr gdpa =
      (PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr");
   vkGetImageSubresourceLayout2EXT_ = (PFN_vkGetImageSubresourceLayout2EXT)gdpa(
      dev, "vkGetImageSubresourceLayout2EXT");
   vkGetImageDrmModProps = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)gdpa(
      dev, "vkGetImageDrmFormatModifierPropertiesEXT");
   if (has_icc && !vkGetImageSubresourceLayout2EXT_) {
      printf("FAIL missing vkGetImageSubresourceLayout2EXT\nICC_FAILS=1\n");
      return 1;
   }

   VkPhysicalDeviceMemoryProperties mp;
   vkGetPhysicalDeviceMemoryProperties(pd, &mp);
   const VkMemoryPropertyFlags hv =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   host_mi = dev_mi = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      if (host_mi == ~0u && (mp.memoryTypes[i].propertyFlags & hv) == hv)
         host_mi = i;
      if (dev_mi == ~0u &&
          (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
         dev_mi = i;
      if (getenv("ICC_VERBOSE"))
         printf("MEMTYPE %u flags=0x%x heap=%u\n", i,
                mp.memoryTypes[i].propertyFlags, mp.memoryTypes[i].heapIndex);
   }

   VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = W * H * 4,
                             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
   CK(vkCreateBuffer(dev, &bci, NULL, &rbuf), "rbuf");
   VkMemoryRequirements bmr;
   vkGetBufferMemoryRequirements(dev, rbuf, &bmr);
   VkMemoryAllocateInfo bmai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = bmr.size,
                                .memoryTypeIndex = host_mi};
   VkDeviceMemory bmem;
   CK(vkAllocateMemory(dev, &bmai, NULL, &bmem), "rmem");
   CK(vkBindBufferMemory(dev, rbuf, bmem, 0), "rbind");
   CK(vkMapMemory(dev, bmem, 0, VK_WHOLE_SIZE, 0, (void **)&rpx), "rmap");

   VkCommandPoolCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi};
   CK(vkCreateCommandPool(dev, &pci, NULL, &pool), "pool");
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(vkAllocateCommandBuffers(dev, &cai, &cmd), "cmd");
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(vkCreateFence(dev, &fci, NULL, &fence), "fence");

   int fails = 0;
   const VkImageUsageFlags usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      (getenv("ICC_NOSAMPLED") ? 0 : VK_IMAGE_USAGE_SAMPLED_BIT);
   const VkFormat fmts[2] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM};
   const VkImageCompressionFlagsEXT modes[3] = {
      VK_IMAGE_COMPRESSION_DEFAULT_EXT, VK_IMAGE_COMPRESSION_DISABLED_EXT,
      VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT};

   /* 1. optimal tiling, compression control on the create info */
   for (int fi = 0; fi < 2 && !getenv("ICC_ONLY_MOD"); fi++) {
      for (int mo = 0; mo < 3; mo++) {
         if (getenv("ICC_MODE") && atoi(getenv("ICC_MODE")) != mo)
            continue;
         VkImageCompressionControlEXT ctl = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT,
            .flags = modes[mo]};
         /* format query with the same control chained */
         VkPhysicalDeviceImageFormatInfo2 ifi = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .pNext = has_icc ? &ctl : NULL,
            .format = fmts[fi],
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage};
         VkImageCompressionPropertiesEXT qcp = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT};
         VkImageFormatProperties2 ifp = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = has_icc ? &qcp : NULL};
         VkResult qr = vkGetPhysicalDeviceImageFormatProperties2(pd, &ifi, &ifp);

         VkImageCreateInfo ci2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = has_icc ? &ctl : NULL,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = fmts[fi],
            .extent = {W, H, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage};
         VkImage img;
         CK(vkCreateImage(dev, &ci2, NULL, &img), "img");
         VkDeviceMemory m = bind_image(img);
         VkImageCompressionPropertiesEXT cp = image_compression(img);
         const float col[4] = {0.25f, 0.5f + 0.1f * mo, 0.75f, 1.0f};
         int bad = clear_copy_verify(img, fmts[fi], col);
         /* DISABLED must report DISABLED; fixed-rate is unsupported so the
          * driver must not claim a fixed rate. */
         int ok = bad == 0 && qr == VK_SUCCESS &&
                  cp.imageCompressionFixedRateFlags == 0 &&
                  (!has_icc || modes[mo] != VK_IMAGE_COMPRESSION_DISABLED_EXT ||
                   cp.imageCompressionFlags == VK_IMAGE_COMPRESSION_DISABLED_EXT);
         printf("CASE optimal fmt=%d req=%s query=%s image=%s bad_texels=%d %s\n",
                fmts[fi], cflags_str(modes[mo]),
                qr == VK_SUCCESS ? cflags_str(qcp.imageCompressionFlags) : "unsupported",
                cflags_str(cp.imageCompressionFlags), bad, ok ? "PASS" : "FAIL");
         fails += !ok;
         release(img, m);
      }
   }

   /* 2. DRM format modifier images: every advertised modifier, incl. AFBC */
   if (has_mod && vkGetImageDrmModProps) {
      for (int fi = 0; fi < 2; fi++) {
         VkDrmFormatModifierPropertiesListEXT ml = {
            .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT};
         VkFormatProperties2 fp = {.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
                                   .pNext = &ml};
         vkGetPhysicalDeviceFormatProperties2(pd, fmts[fi], &fp);
         VkDrmFormatModifierPropertiesEXT mods[32];
         if (ml.drmFormatModifierCount > 32)
            ml.drmFormatModifierCount = 32;
         ml.pDrmFormatModifierProperties = mods;
         vkGetPhysicalDeviceFormatProperties2(pd, fmts[fi], &fp);
         int n_afbc = 0;
         for (uint32_t i = 0; i < ml.drmFormatModifierCount; i++) {
            uint64_t mod = mods[i].drmFormatModifier;
            int afbc = (mod >> 56) == 0x08 && ((mod >> 52) & 0xf) == 0;
            n_afbc += afbc && mod != 0;
            if (!(mods[i].drmFormatModifierTilingFeatures &
                  VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
               continue;
            VkImageDrmFormatModifierListCreateInfoEXT mlci = {
               .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
               .drmFormatModifierCount = 1,
               .pDrmFormatModifiers = &mod};
            VkImageCreateInfo ci3 = {
               .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
               .pNext = &mlci,
               .imageType = VK_IMAGE_TYPE_2D,
               .format = fmts[fi],
               .extent = {W, H, 1},
               .mipLevels = 1,
               .arrayLayers = 1,
               .samples = VK_SAMPLE_COUNT_1_BIT,
               .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
               .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
            VkImage img;
            VkResult r = vkCreateImage(dev, &ci3, NULL, &img);
            if (r != VK_SUCCESS) {
               printf("CASE modifier fmt=%d mod=0x%016llx create r=%d FAIL\n",
                      fmts[fi], (unsigned long long)mod, r);
               fails++;
               continue;
            }
            VkDeviceMemory m = bind_image(img);
            VkImageDrmFormatModifierPropertiesEXT ip = {
               .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT};
            vkGetImageDrmModProps(dev, img, &ip);
            VkImageCompressionPropertiesEXT cp = image_compression(img);
            const float col[4] = {0.8f, 0.2f, 0.4f, 1.0f};
            int bad = clear_copy_verify(img, fmts[fi], col);
            int ok = bad == 0 && ip.drmFormatModifier == mod &&
                     (!has_icc ||
                      (afbc ? cp.imageCompressionFlags == VK_IMAGE_COMPRESSION_DEFAULT_EXT
                            : cp.imageCompressionFlags == VK_IMAGE_COMPRESSION_DISABLED_EXT));
            printf("CASE modifier fmt=%d mod=0x%016llx afbc=%d image_mod=0x%016llx "
                   "compression=%s bad_texels=%d %s\n",
                   fmts[fi], (unsigned long long)mod, afbc,
                   (unsigned long long)ip.drmFormatModifier,
                   cflags_str(cp.imageCompressionFlags), bad, ok ? "PASS" : "FAIL");
            fails += !ok;
            release(img, m);
         }
         printf("MODS fmt=%d count=%u afbc=%d\n", fmts[fi],
                ml.drmFormatModifierCount, n_afbc);
      }
   }
   printf("ICC_FAILS=%d\n", fails);
   return fails != 0;
}
