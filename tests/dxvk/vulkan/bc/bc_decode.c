/* DX6 BC1-BC7 device test. Direct ICD load. GPU upload/copy/blit/sample,
 * host readback only. Dumps inputs and sampled texels to OUTDIR for
 * bc_verify.py (host reference decode). */
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "bc_fetch.comp.spv.h"

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, _r, __LINE__);                 \
         exit(1);                                                              \
      }                                                                        \
   } while (0)

#define IW 20
#define IH 12
#define MIPS 2
#define LAYERS 2
/* texels sampled per image: (20*12 + 10*6) * 2 layers */
#define TEXELS 600

static const struct {
   VkFormat f;
   const char *name;
} fmts[] = {
   {VK_FORMAT_BC1_RGB_UNORM_BLOCK, "BC1_RGB_UNORM"},
   {VK_FORMAT_BC1_RGB_SRGB_BLOCK, "BC1_RGB_SRGB"},
   {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "BC1_RGBA_UNORM"},
   {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, "BC1_RGBA_SRGB"},
   {VK_FORMAT_BC2_UNORM_BLOCK, "BC2_UNORM"},
   {VK_FORMAT_BC2_SRGB_BLOCK, "BC2_SRGB"},
   {VK_FORMAT_BC3_UNORM_BLOCK, "BC3_UNORM"},
   {VK_FORMAT_BC3_SRGB_BLOCK, "BC3_SRGB"},
   {VK_FORMAT_BC4_UNORM_BLOCK, "BC4_UNORM"},
   {VK_FORMAT_BC4_SNORM_BLOCK, "BC4_SNORM"},
   {VK_FORMAT_BC5_UNORM_BLOCK, "BC5_UNORM"},
   {VK_FORMAT_BC5_SNORM_BLOCK, "BC5_SNORM"},
   {VK_FORMAT_BC6H_UFLOAT_BLOCK, "BC6H_UFLOAT"},
   {VK_FORMAT_BC6H_SFLOAT_BLOCK, "BC6H_SFLOAT"},
   {VK_FORMAT_BC7_UNORM_BLOCK, "BC7_UNORM"},
   {VK_FORMAT_BC7_SRGB_BLOCK, "BC7_SRGB"},
};

static icd_gipa_fn gipa;
static VkInstance inst;
static VkDevice dev;
static uint32_t host_mi, dev_mi;

#define PFN(name) static PFN_vk##name vk##name;
#define DEV_FUNCS(X)                                                           \
   X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory)            \
   X(BindBufferMemory) X(MapMemory) X(CreateImage)                             \
   X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView)         \
   X(CreateShaderModule) X(CreatePipelineLayout) X(CreateComputePipelines)     \
   X(CreateDescriptorSetLayout) X(CreateDescriptorPool)                        \
   X(AllocateDescriptorSets) X(UpdateDescriptorSets) X(CreateSampler)          \
   X(CreateCommandPool) X(AllocateCommandBuffers) X(BeginCommandBuffer)        \
   X(EndCommandBuffer) X(CmdBindPipeline) X(CmdBindDescriptorSets)             \
   X(CmdPushConstants) X(CmdDispatch) X(CmdPipelineBarrier)                    \
   X(CmdCopyBufferToImage) X(CmdCopyImageToBuffer) X(CmdCopyImage)             \
   X(CmdBlitImage) X(CreateFence) X(QueueSubmit) X(WaitForFences)              \
   X(ResetFences) X(ResetCommandBuffer) X(GetDeviceQueue)                      \
   X(DestroyImage) X(DestroyImageView) X(FreeMemory) X(DestroyBuffer)          \
   X(ResetDescriptorPool)
DEV_FUNCS(PFN)

static uint32_t rng;
static uint32_t
lcg(void)
{
   rng = rng * 1664525u + 1013904223u;
   return rng >> 8;
}

struct buf {
   VkBuffer b;
   VkDeviceMemory m;
   void *p;
};

static struct buf
mkbuf(VkDeviceSize size, VkBufferUsageFlags usage)
{
   struct buf r;
   VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size = size,
                            .usage = usage};
   CK(vkCreateBuffer(dev, &ci, NULL, &r.b), "CreateBuffer");
   VkMemoryRequirements mr;
   vkGetBufferMemoryRequirements(dev, r.b, &mr);
   VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                              .allocationSize = mr.size,
                              .memoryTypeIndex = host_mi};
   CK(vkAllocateMemory(dev, &ai, NULL, &r.m), "AllocBuf");
   CK(vkBindBufferMemory(dev, r.b, r.m, 0), "BindBuf");
   CK(vkMapMemory(dev, r.m, 0, VK_WHOLE_SIZE, 0, &r.p), "MapBuf");
   memset(r.p, 0, size);
   return r;
}

struct img {
   VkImage i;
   VkDeviceMemory m;
};

static struct img
mkimg(VkFormat f, uint32_t w, uint32_t h, uint32_t mips, uint32_t layers,
      VkImageUsageFlags usage)
{
   struct img r;
   VkImageCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = f,
                           .extent = {w, h, 1},
                           .mipLevels = mips,
                           .arrayLayers = layers,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = usage,
                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   CK(vkCreateImage(dev, &ci, NULL, &r.i), "CreateImage");
   VkMemoryRequirements mr;
   vkGetImageMemoryRequirements(dev, r.i, &mr);
   VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                              .allocationSize = mr.size,
                              .memoryTypeIndex = dev_mi};
   CK(vkAllocateMemory(dev, &ai, NULL, &r.m), "AllocImg");
   CK(vkBindImageMemory(dev, r.i, r.m, 0), "BindImg");
   return r;
}

static void
barrier(VkCommandBuffer cb, VkImage *imgs, uint32_t n, VkImageLayout from)
{
   VkImageMemoryBarrier ib[4];
   for (uint32_t i = 0; i < n; i++) {
      ib[i] = (VkImageMemoryBarrier){
         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         .oldLayout = from,
         .newLayout = VK_IMAGE_LAYOUT_GENERAL,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = imgs[i],
         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                              VK_REMAINING_MIP_LEVELS, 0,
                              VK_REMAINING_ARRAY_LAYERS},
      };
   }
   VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                         .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                         .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT |
                                          VK_ACCESS_MEMORY_WRITE_BIT};
   vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, NULL,
                        n, ib);
}

static int
dump(const char *dir, const char *name, const char *ext, const void *p,
     size_t size)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s.%s", dir, name, ext);
   FILE *f = fopen(path, "wb");
   if (!f)
      return 0;
   fwrite(p, 1, size, f);
   fclose(f);
   return 1;
}

static void
on_segv(int sig)
{
   void *bt[32];
   int n = backtrace(bt, 32);
   printf("CRASH signal=%d\n", sig);
   backtrace_symbols_fd(bt, n, 1);
   _exit(139);
}

int
main(int argc, char **argv)
{
   signal(SIGSEGV, on_segv);
   signal(SIGABRT, on_segv);
   if (argc < 3) {
      printf("usage: %s <icd.so> <outdir>\n", argv[0]);
      return 2;
   }
   const char *outdir = argv[2];
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      return 1;
   }
   gipa = (icd_gipa_fn)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkCreateInstance vkCreateInstance =
      (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   CK(vkCreateInstance(&ici, NULL, &inst), "CreateInstance");
#define IG(name) PFN_vk##name vk##name = (PFN_vk##name)gipa(inst, "vk" #name);
   IG(EnumeratePhysicalDevices)
   IG(GetPhysicalDeviceProperties)
   IG(GetPhysicalDeviceFeatures)
   IG(GetPhysicalDeviceFormatProperties)
   IG(GetPhysicalDeviceImageFormatProperties)
   IG(GetPhysicalDeviceMemoryProperties)
   IG(CreateDevice)
   IG(GetDeviceProcAddr)

   uint32_t n = 8;
   VkPhysicalDevice devs[8];
   CK(vkEnumeratePhysicalDevices(inst, &n, devs), "enum");
   VkPhysicalDevice phys = devs[0];
   VkPhysicalDeviceProperties props;
   vkGetPhysicalDeviceProperties(phys, &props);
   VkPhysicalDeviceFeatures feats;
   vkGetPhysicalDeviceFeatures(phys, &feats);
   printf("ICD device=%s textureCompressionBC=%d\n", props.deviceName,
          feats.textureCompressionBC);

   int fmt_fail = 0;
   for (unsigned i = 0; i < sizeof(fmts) / sizeof(fmts[0]); i++) {
      VkFormatProperties fp;
      vkGetPhysicalDeviceFormatProperties(phys, fmts[i].f, &fp);
      VkImageFormatProperties ifp;
      VkResult r = vkGetPhysicalDeviceImageFormatProperties(
         phys, fmts[i].f, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
         0, &ifp);
      const VkFormatFeatureFlags need =
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
         VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
         VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
      int ok = (fp.optimalTilingFeatures & need) == need && r == VK_SUCCESS &&
               fp.linearTilingFeatures == 0;
      printf("FORMAT %s optimal=0x%x linear=0x%x ifp=%d %s\n", fmts[i].name,
             fp.optimalTilingFeatures, fp.linearTilingFeatures, r,
             ok ? "PASS" : "FAIL");
      fmt_fail += !ok;
   }
   if (!feats.textureCompressionBC || fmt_fail) {
      printf("BC_SUMMARY FAIL feature/format (fmt_fail=%d)\n", fmt_fail);
      return 1;
   }

   VkPhysicalDeviceMemoryProperties mp;
   vkGetPhysicalDeviceMemoryProperties(phys, &mp);
   host_mi = dev_mi = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
      if (host_mi == ~0u && (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
          (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         host_mi = i;
      if (dev_mi == ~0u && (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
         dev_mi = i;
   }

   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = 0,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceFeatures en = {.textureCompressionBC = VK_TRUE};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .pEnabledFeatures = &en};
   CK(vkCreateDevice(phys, &dci, NULL, &dev), "CreateDevice");
#define DG(name) vk##name = (PFN_vk##name)vkGetDeviceProcAddr(dev, "vk" #name);
   DEV_FUNCS(DG)
   VkQueue q;
   vkGetDeviceQueue(dev, 0, 0, &q);
   printf("STEP device\n");

   /* compute fetch pipeline */
   VkDescriptorSetLayoutBinding binds[2] = {
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
   };
   VkDescriptorSetLayoutCreateInfo dslci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = binds};
   VkDescriptorSetLayout dsl;
   CK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl), "DSL");
   VkPushConstantRange pcr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 20};
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &dsl,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pcr};
   VkPipelineLayout pl;
   CK(vkCreatePipelineLayout(dev, &plci, NULL, &pl), "PL");
   VkShaderModuleCreateInfo smci = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                    .codeSize = sizeof(bc_fetch_spv),
                                    .pCode = bc_fetch_spv};
   VkShaderModule sm;
   CK(vkCreateShaderModule(dev, &smci, NULL, &sm), "SM");
   VkComputePipelineCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = sm,
                .pName = "main"},
      .layout = pl};
   VkPipeline pipe;
   CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe), "CP");
   VkDescriptorPoolSize ps[2] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
   };
   VkDescriptorPoolCreateInfo dpci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets = 4,
                                      .poolSizeCount = 2,
                                      .pPoolSizes = ps};
   VkDescriptorPool dp;
   CK(vkCreateDescriptorPool(dev, &dpci, NULL, &dp), "DP");
   VkSamplerCreateInfo sci = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                              .magFilter = VK_FILTER_NEAREST,
                              .minFilter = VK_FILTER_NEAREST,
                              .maxLod = 16.0f};
   VkSampler smp;
   CK(vkCreateSampler(dev, &sci, NULL, &smp), "Sampler");

   VkCommandPoolCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  .queueFamilyIndex = 0};
   VkCommandPool pool;
   CK(vkCreateCommandPool(dev, &cpi, NULL, &pool), "Pool");
   VkCommandBufferAllocateInfo cbai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                       .commandPool = pool,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                       .commandBufferCount = 1};
   VkCommandBuffer cb;
   CK(vkAllocateCommandBuffers(dev, &cbai, &cb), "CB");
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   VkFence fence;
   CK(vkCreateFence(dev, &fci, NULL, &fence), "Fence");

   if (getenv("BLIT_PROBE")) {
      struct img s = mkimg(VK_FORMAT_R8G8B8A8_UNORM, IW, IH, 1,
                           atoi(getenv("BLIT_PROBE")),
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT);
      struct img d = mkimg(VK_FORMAT_R32G32B32A32_SFLOAT, IW, IH, 1, 1,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT);
      VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      CK(vkBeginCommandBuffer(cb, &bi), "Begin");
      VkImage all[2] = {s.i, d.i};
      barrier(cb, all, 2, VK_IMAGE_LAYOUT_UNDEFINED);
      VkImageBlit bl = {
         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {{0, 0, 0}, {IW, IH, 1}},
         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {{0, 0, 0}, {IW, IH, 1}}};
      printf("STEP probe blit record\n");
      vkCmdBlitImage(cb, s.i, VK_IMAGE_LAYOUT_GENERAL, d.i,
                     VK_IMAGE_LAYOUT_GENERAL, 1, &bl, VK_FILTER_NEAREST);
      CK(vkEndCommandBuffer(cb), "End");
      printf("BLIT_PROBE layers=%s recorded OK\n", getenv("BLIT_PROBE"));
      return 0;
   }

   int fails = 0;
   for (unsigned fi = 0; fi < sizeof(fmts) / sizeof(fmts[0]); fi++) {
      const VkFormat f = fmts[fi].f;
      const char *name = fmts[fi].name;
      const uint32_t bb = (f <= VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
                           f == VK_FORMAT_BC4_UNORM_BLOCK ||
                           f == VK_FORMAT_BC4_SNORM_BLOCK) ? 8 : 16;
      /* mip0: 5x3 blocks, mip1: 3x2 blocks, 2 layers each */
      const uint32_t mip0_B = 5 * 3 * bb * LAYERS, mip1_B = 3 * 2 * bb * LAYERS;
      const uint32_t in_B = mip0_B + mip1_B;
      const uint32_t sub_B = 2 * 2 * bb; /* 8x8 texel sub update */

      printf("STEP %s begin\n", name);
      struct buf in = mkbuf(in_B, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      struct buf sub = mkbuf(sub_B, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      struct buf raw = mkbuf(in_B, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      struct buf out = mkbuf(TEXELS * 16 * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      struct buf blit = mkbuf(IW * IH * 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

      rng = 0x9e3779b9u ^ (f * 2654435761u);
      for (uint32_t k = 0; k < in_B; k++)
         ((uint8_t *)in.p)[k] = lcg() & 0xff;
      for (uint32_t k = 0; k < sub_B; k++)
         ((uint8_t *)sub.p)[k] = lcg() & 0xff;

      const VkImageUsageFlags u = VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      struct img a = mkimg(f, IW, IH, MIPS, LAYERS, u);
      struct img b = mkimg(f, IW, IH, MIPS, LAYERS, u);
      struct img dst = mkimg(VK_FORMAT_R32G32B32A32_SFLOAT, IW, IH, 1, 1,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

      VkImageView va, vb;
      VkImageViewCreateInfo vci = {
         .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
         .image = a.i,
         .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
         .format = f,
         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, MIPS, 0, LAYERS}};
      CK(vkCreateImageView(dev, &vci, NULL, &va), "ViewA");
      vci.image = b.i;
      CK(vkCreateImageView(dev, &vci, NULL, &vb), "ViewB");

      VkDescriptorSet ds[2];
      VkDescriptorSetLayout dsls[2] = {dsl, dsl};
      VkDescriptorSetAllocateInfo dsai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                          .descriptorPool = dp,
                                          .descriptorSetCount = 2,
                                          .pSetLayouts = dsls};
      CK(vkResetDescriptorPool(dev, dp, 0), "ResetDP");
      CK(vkAllocateDescriptorSets(dev, &dsai, ds), "DS");
      VkDescriptorImageInfo dii[2] = {{smp, va, VK_IMAGE_LAYOUT_GENERAL},
                                      {smp, vb, VK_IMAGE_LAYOUT_GENERAL}};
      VkDescriptorBufferInfo dbi = {out.b, 0, VK_WHOLE_SIZE};
      VkWriteDescriptorSet wds[4];
      for (int s = 0; s < 2; s++) {
         wds[2 * s] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds[s], .dstBinding = 0, .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &dii[s]};
         wds[2 * s + 1] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds[s], .dstBinding = 1, .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &dbi};
      }
      vkUpdateDescriptorSets(dev, 4, wds, 0, NULL);

      VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
      CK(vkResetCommandBuffer(cb, 0), "ResetCB");
      CK(vkBeginCommandBuffer(cb, &bi), "Begin");
      printf("STEP %s resources\n", name);
      VkImage all[3] = {a.i, b.i, dst.i};
      barrier(cb, all, 3, VK_IMAGE_LAYOUT_UNDEFINED);

      /* 1: full upload, both mips/layers */
      VkBufferImageCopy up[2] = {
         {.bufferOffset = 0,
          .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, LAYERS},
          .imageExtent = {IW, IH, 1}},
         {.bufferOffset = mip0_B,
          .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, LAYERS},
          .imageExtent = {IW / 2, IH / 2, 1}},
      };
      vkCmdCopyBufferToImage(cb, in.b, a.i, VK_IMAGE_LAYOUT_GENERAL, 2, up);
      barrier(cb, all, 0, VK_IMAGE_LAYOUT_GENERAL);

      /* 2: image->image copy of the full chain */
      VkImageCopy ic[2] = {
         {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, LAYERS}, {0, 0, 0},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, LAYERS}, {0, 0, 0}, {IW, IH, 1}},
         {{VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, LAYERS}, {0, 0, 0},
          {VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, LAYERS}, {0, 0, 0},
          {IW / 2, IH / 2, 1}},
      };
      vkCmdCopyImage(cb, a.i, VK_IMAGE_LAYOUT_GENERAL, b.i,
                     VK_IMAGE_LAYOUT_GENERAL, 2, ic);
      /* 3: raw readback of a */
      vkCmdCopyImageToBuffer(cb, a.i, VK_IMAGE_LAYOUT_GENERAL, raw.b, 2, up);
      /* 4: blit a mip0 layer0 -> RGBA32F, nearest */
      VkImageBlit bl = {
         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {{0, 0, 0}, {IW, IH, 1}},
         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {{0, 0, 0}, {IW, IH, 1}}};
      vkCmdBlitImage(cb, a.i, VK_IMAGE_LAYOUT_GENERAL, dst.i,
                     VK_IMAGE_LAYOUT_GENERAL, 1, &bl, VK_FILTER_NEAREST);
      barrier(cb, all, 0, VK_IMAGE_LAYOUT_GENERAL);
      VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                              .imageExtent = {IW, IH, 1}};
      vkCmdCopyImageToBuffer(cb, dst.i, VK_IMAGE_LAYOUT_GENERAL, blit.b, 1, &bc);

      /* 5: fetch a -> slot 0, b -> slot 1 */
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
      printf("STEP %s recorded transfers\n", name);
      int32_t off = 0;
      for (int s = 0; s < 2; s++) {
         vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1,
                                 &ds[s], 0, NULL);
         for (int lvl = 0; lvl < MIPS; lvl++)
            for (int l = 0; l < LAYERS; l++) {
               int32_t pc[5] = {lvl, l, IW >> lvl, IH >> lvl, off};
               vkCmdPushConstants(cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, pc);
               vkCmdDispatch(cb, ((IW >> lvl) + 7) / 8, ((IH >> lvl) + 7) / 8, 1);
               off += (IW >> lvl) * (IH >> lvl);
            }
      }
      barrier(cb, all, 0, VK_IMAGE_LAYOUT_GENERAL);

      /* 6: partial block-aligned update of a (mip0 layer1, 8x8 @ 8,4) */
      VkBufferImageCopy su = {
         .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1},
         .imageOffset = {8, 4, 0},
         .imageExtent = {8, 8, 1}};
      vkCmdCopyBufferToImage(cb, sub.b, a.i, VK_IMAGE_LAYOUT_GENERAL, 1, &su);
      barrier(cb, all, 0, VK_IMAGE_LAYOUT_GENERAL);
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1,
                              &ds[0], 0, NULL);
      for (int lvl = 0; lvl < MIPS; lvl++)
         for (int l = 0; l < LAYERS; l++) {
            int32_t pc[5] = {lvl, l, IW >> lvl, IH >> lvl, off};
            vkCmdPushConstants(cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, pc);
            vkCmdDispatch(cb, ((IW >> lvl) + 7) / 8, ((IH >> lvl) + 7) / 8, 1);
            off += (IW >> lvl) * (IH >> lvl);
         }
      VkMemoryBarrier hb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                            .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hb, 0, NULL, 0,
                           NULL);
      CK(vkEndCommandBuffer(cb), "End");
      VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &cb};
      CK(vkResetFences(dev, 1, &fence), "ResetFence");
      CK(vkQueueSubmit(q, 1, &si, fence), "Submit");
      CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull), "Wait");

      const float *o = out.p;
      int raw_ok = !memcmp(raw.p, in.p, in_B);
      int cp_ok = !memcmp(o, o + TEXELS * 4, TEXELS * 16);
      int blit_ok = !memcmp(blit.p, o, IW * IH * 16);
      int nonzero = 0;
      for (int k = 0; k < TEXELS * 4; k++)
         nonzero += o[k] != 0.0f;
      printf("CASE %s raw=%s copy=%s blit=%s nonzero=%d\n", name,
             raw_ok ? "PASS" : "FAIL", cp_ok ? "PASS" : "FAIL",
             blit_ok ? "PASS" : "FAIL", nonzero);
      fails += !raw_ok + !cp_ok + !blit_ok + (nonzero == 0);
      if (!dump(outdir, name, "in", in.p, in_B) ||
          !dump(outdir, name, "sub", sub.p, sub_B) ||
          !dump(outdir, name, "up", o, TEXELS * 16) ||
          !dump(outdir, name, "up2", o + 2 * TEXELS * 4, TEXELS * 16)) {
         printf("FAIL dump %s\n", name);
         fails++;
      }
      vkDestroyImageView(dev, va, NULL);
      vkDestroyImageView(dev, vb, NULL);
      vkDestroyImage(dev, a.i, NULL);
      vkDestroyImage(dev, b.i, NULL);
      vkDestroyImage(dev, dst.i, NULL);
      vkFreeMemory(dev, a.m, NULL);
      vkFreeMemory(dev, b.m, NULL);
      vkFreeMemory(dev, dst.m, NULL);
      struct buf *bufs[] = {&in, &sub, &raw, &out, &blit};
      for (unsigned k = 0; k < 5; k++) {
         vkDestroyBuffer(dev, bufs[k]->b, NULL);
         vkFreeMemory(dev, bufs[k]->m, NULL);
      }
   }
   printf("BC_DEVICE_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
