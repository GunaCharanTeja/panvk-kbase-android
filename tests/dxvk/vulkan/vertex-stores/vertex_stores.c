/* Standalone Vulkan vertexPipelineStoresAndAtomics probe.
 * Tests shader storage buffer atomic operations and writes in vertex shader. */
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include("../dx7_harness.h")
#include "../dx7_harness.h"
#else
#include "dx7_harness.h"
#endif

#if __has_include("vertex_stores_spv.h")
#include "vertex_stores_spv.h"
#else
#include "vertex-stores/vertex_stores_spv.h"
#endif

#define EXTRA_FUNCS(X)                                                         \
   X(QueueWaitIdle)                                                            \
   X(CreateDescriptorSetLayout)                                                \
   X(DestroyDescriptorSetLayout)                                               \
   X(CreateDescriptorPool)                                                     \
   X(DestroyDescriptorPool)                                                    \
   X(ResetDescriptorPool)                                                      \
   X(AllocateDescriptorSets)                                                   \
   X(UpdateDescriptorSets)                                                     \
   X(CmdBindDescriptorSets)                                                    \
   X(DestroyBuffer)                                                            \
   X(FreeMemory)                                                               \
   X(DestroyFramebuffer)                                                       \
   X(DestroyImageView)                                                         \
   X(DestroyImage)                                                             \
   X(DestroyRenderPass)                                                        \
   X(DestroyPipelineLayout)                                                    \
   X(DestroyCommandPool)                                                       \
   X(ResetCommandPool)

EXTRA_FUNCS(DX7_DECL)

static void
silence_unused(void)
{
   (void)dx7_pipeline;
   (void)dx7_run;
   (void)dx7_check;
   (void)dx7_px;
   (void)dx7_is_red;
   (void)dx7_is_blue;
}

static VkPhysicalDeviceFeatures enabled_features;

static void
device_hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   if (!t->feats.vertexPipelineStoresAndAtomics) {
      printf("SKIP vertexPipelineStoresAndAtomics not supported\n");
      exit(0);
   }
   enabled_features.vertexPipelineStoresAndAtomics = VK_TRUE;
   dci->pEnabledFeatures = &enabled_features;
}

static void
load_extra_funcs(struct dx7 *t, const char *icd)
{
   void *h = dlopen(icd, RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      exit(1);
   }
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   if (!gipa) {
      printf("FAIL missing gipa\n");
      exit(1);
   }
#define LOAD_EXTRA(n)                                                          \
   vk##n = (PFN_vk##n)gipa(t->inst, "vk" #n);                                  \
   if (!vk##n) {                                                               \
      printf("FAIL missing vk" #n "\n");                                       \
      exit(1);                                                                 \
   }
   EXTRA_FUNCS(LOAD_EXTRA)
#undef LOAD_EXTRA
}

#define NUM_HIT 64
#define NUM_INOUT 32
#define NUM_OUTV 64
#define GUARD_BYTES 4096

struct ssbo_data {
   uint32_t hit[NUM_HIT];
   uint32_t inout[NUM_INOUT];
   uint32_t outv[NUM_OUTV];
   uint32_t total;
};

struct rt4 {
   VkImage img;
   VkDeviceMemory mem;
   VkImageView view;
   VkRenderPass rp;
   VkFramebuffer fb;
};

static void
create_rt4(struct dx7 *t, struct rt4 *rt)
{
   VkImageCreateInfo imgci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {4, 4, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
   };
   CK(vkCreateImage(t->dev, &imgci, NULL, &rt->img), "CreateImage4");

   VkMemoryRequirements imr;
   vkGetImageMemoryRequirements(t->dev, rt->img, &imr);
   uint32_t imi = 0;
   while (!(imr.memoryTypeBits & (1u << imi)))
      imi++;
   VkMemoryAllocateInfo imai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = imr.size,
      .memoryTypeIndex = imi,
   };
   CK(vkAllocateMemory(t->dev, &imai, NULL, &rt->mem), "AllocImageMem4");
   CK(vkBindImageMemory(t->dev, rt->img, rt->mem, 0), "BindImageMem4");

   VkImageViewCreateInfo ivci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = rt->img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
   };
   CK(vkCreateImageView(t->dev, &ivci, NULL, &rt->view), "CreateView4");

   VkAttachmentDescription att = {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
   };
   VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &ref,
   };
   VkRenderPassCreateInfo rpci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &att,
      .subpassCount = 1,
      .pSubpasses = &sub,
   };
   CK(vkCreateRenderPass(t->dev, &rpci, NULL, &rt->rp), "CreateRP4");

   VkFramebufferCreateInfo fbci = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = rt->rp,
      .attachmentCount = 1,
      .pAttachments = &rt->view,
      .width = 4,
      .height = 4,
      .layers = 1,
   };
   CK(vkCreateFramebuffer(t->dev, &fbci, NULL, &rt->fb), "CreateFB4");
}

static void
destroy_rt4(struct dx7 *t, struct rt4 *rt)
{
   vkDestroyFramebuffer(t->dev, rt->fb, NULL);
   vkDestroyRenderPass(t->dev, rt->rp, NULL);
   vkDestroyImageView(t->dev, rt->view, NULL);
   vkDestroyImage(t->dev, rt->img, NULL);
   vkFreeMemory(t->dev, rt->mem, NULL);
}

static VkPipeline
create_pipe(struct dx7 *t, VkPipelineLayout layout, VkRenderPass rp,
            VkPrimitiveTopology topo, VkBool32 restart_enable)
{
   VkShaderModule vs = dx7_module(t, vs_stores, sizeof(vs_stores));
   VkShaderModule fs = dx7_module(t, fs_stores, sizeof(fs_stores));

   VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fs,
       .pName = "main"},
   };

   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
   };

   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = topo,
      .primitiveRestartEnable = restart_enable,
   };

   VkViewport vp = {0.0f, 0.0f, 4.0f, 4.0f, 0.0f, 1.0f};
   VkRect2D sc = {{0, 0}, {4, 4}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &vp,
      .scissorCount = 1,
      .pScissors = &sc,
   };

   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
   };

   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
   };

   VkPipelineColorBlendAttachmentState ba = {
      .colorWriteMask = 0xf,
   };
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &ba,
   };

   VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn,
   };

   VkGraphicsPipelineCreateInfo gpci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .pDynamicState = &ds,
      .layout = layout,
      .renderPass = rp,
      .subpass = 0,
   };

   VkPipeline pipe;
   CK(vkCreateGraphicsPipelines(t->dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe),
      "CreatePipeline");

   vkDestroyShaderModule(t->dev, vs, NULL);
   vkDestroyShaderModule(t->dev, fs, NULL);
   return pipe;
}

struct test_case {
   const char *name;
   VkPipeline pipe;
   int indexed;
   const uint16_t *indices;
   uint32_t index_count;
   uint32_t vertex_count;
   uint32_t instance_count;
   uint32_t first_vertex;
   uint32_t first_instance;
   int (*check_fn)(const struct ssbo_data *res, const uint8_t *guard,
                   char *fail_msg, size_t fail_msg_len);
};

static int
run_test_case(struct dx7 *t, struct rt4 *rt, VkPipelineLayout playout,
              VkDescriptorPool dpool, VkDescriptorSetLayout dsl,
              VkCommandPool cmd_pool, const struct test_case *tc)
{
   size_t total_buf_size = sizeof(struct ssbo_data) + GUARD_BYTES;
   uint8_t *init_bytes = malloc(total_buf_size);
   if (!init_bytes) {
      printf("FAIL oom init_bytes\n");
      exit(1);
   }

   struct ssbo_data *init_data = (struct ssbo_data *)init_bytes;
   memset(init_data->hit, 0, sizeof(init_data->hit));
   for (int i = 0; i < NUM_INOUT; i++)
      init_data->inout[i] = (uint32_t)i * 7;
   memset(init_data->outv, 0xcd, sizeof(init_data->outv));
   init_data->total = 0;
   memset(init_bytes + sizeof(struct ssbo_data), 0xab, GUARD_BYTES);

   VkBuffer sbuf;
   VkDeviceMemory smem;
   dx7_buffer(t, total_buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
              init_bytes, &sbuf, &smem);
   free(init_bytes);

   VkDescriptorSetAllocateInfo dsai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dpool,
      .descriptorSetCount = 1,
      .pSetLayouts = &dsl,
   };
   VkDescriptorSet ds;
   CK(vkAllocateDescriptorSets(t->dev, &dsai, &ds), "AllocDS");

   VkDescriptorBufferInfo dbi = {
      .buffer = sbuf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
   };
   VkWriteDescriptorSet wds = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ds,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .pBufferInfo = &dbi,
   };
   vkUpdateDescriptorSets(t->dev, 1, &wds, 0, NULL);

   VkBuffer ibuf = VK_NULL_HANDLE;
   VkDeviceMemory imem = VK_NULL_HANDLE;
   if (tc->indexed) {
      dx7_buffer(t, tc->index_count * sizeof(uint16_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 tc->indices, &ibuf, &imem);
   }

   VkCommandBufferAllocateInfo cbai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
   };
   VkCommandBuffer cmd;
   CK(vkAllocateCommandBuffers(t->dev, &cbai, &cmd), "AllocCmd");

   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
   };
   CK(vkBeginCommandBuffer(cmd, &bbi), "BeginCmd");

   VkClearValue clear = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = rt->rp,
      .framebuffer = rt->fb,
      .renderArea = {{0, 0}, {4, 4}},
      .clearValueCount = 1,
      .pClearValues = &clear,
   };
   vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

   VkViewport vp = {0.0f, 0.0f, 4.0f, 4.0f, 0.0f, 1.0f};
   VkRect2D sc = {{0, 0}, {4, 4}};
   vkCmdSetViewport(cmd, 0, 1, &vp);
   vkCmdSetScissor(cmd, 0, 1, &sc);

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tc->pipe);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, playout, 0, 1,
                           &ds, 0, NULL);

   if (tc->indexed) {
      vkCmdBindIndexBuffer(cmd, ibuf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, tc->index_count, tc->instance_count, 0, 0,
                       tc->first_instance);
   } else {
      vkCmdDraw(cmd, tc->vertex_count, tc->instance_count, tc->first_vertex,
                tc->first_instance);
   }

   vkCmdEndRenderPass(cmd);
   CK(vkEndCommandBuffer(cmd), "EndCmd");

   VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
   };
   CK(vkQueueSubmit(t->queue, 1, &si, VK_NULL_HANDLE), "Submit");
   CK(vkQueueWaitIdle(t->queue), "QueueWaitIdle");

   void *mapped = NULL;
   CK(vkMapMemory(t->dev, smem, 0, total_buf_size, 0, &mapped), "MapRead");
   const struct ssbo_data *res = (const struct ssbo_data *)mapped;
   const uint8_t *guard_res = (const uint8_t *)mapped + sizeof(struct ssbo_data);

   char fail_msg[256] = {0};
   int fail = tc->check_fn(res, guard_res, fail_msg, sizeof(fail_msg));

   vkUnmapMemory(t->dev, smem);

   if (fail) {
      printf("CASE %s FAIL %s\n", tc->name, fail_msg);
   } else {
      printf("CASE %s PASS\n", tc->name);
   }

   vkDestroyBuffer(t->dev, sbuf, NULL);
   vkFreeMemory(t->dev, smem, NULL);
   if (ibuf) {
      vkDestroyBuffer(t->dev, ibuf, NULL);
      vkFreeMemory(t->dev, imem, NULL);
   }
   vkResetCommandPool(t->dev, cmd_pool, 0);
   vkResetDescriptorPool(t->dev, dpool, 0);

   return fail ? 1 : 0;
}

/* Case 1: iter0..iter5 */
static int
check_iter(const struct ssbo_data *res, const uint8_t *guard,
           char *fail_msg, size_t fail_msg_len)
{
   for (int i = 0; i < 32; i++) {
      if (res->hit[i] != 1) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 1 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   for (int i = 32; i < NUM_HIT; i++) {
      if (res->hit[i] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   for (int k = 0; k < 16; k++) {
      uint32_t want = (uint32_t)(k * 7 + (k + 1) + (k + 17));
      if (res->inout[k] != want) {
         snprintf(fail_msg, fail_msg_len,
                  "inout[%d] want %u got %u total=%u", k, want, res->inout[k], res->total);
         return 1;
      }
   }
   for (int k = 16; k < NUM_INOUT; k++) {
      uint32_t want = (uint32_t)(k * 7);
      if (res->inout[k] != want) {
         snprintf(fail_msg, fail_msg_len,
                  "inout[%d] want %u got %u total=%u", k, want, res->inout[k], res->total);
         return 1;
      }
   }
   for (int k = 0; k < 16; k++) {
      int idxA = k;
      int idxB = k + 16;
      if (res->outv[idxA] < 1000u) {
         snprintf(fail_msg, fail_msg_len,
                  "outv[%d] < 1000 got %u total=%u", idxA, res->outv[idxA], res->total);
         return 1;
      }
      if (res->outv[idxB] < 1000u) {
         snprintf(fail_msg, fail_msg_len,
                  "outv[%d] < 1000 got %u total=%u", idxB, res->outv[idxB], res->total);
         return 1;
      }
      uint32_t valA = res->outv[idxA] - 1000u;
      uint32_t valB = res->outv[idxB] - 1000u;
      uint32_t base = (uint32_t)(k * 7);
      uint32_t opt1_A = base;
      uint32_t opt1_B = base + (uint32_t)(idxA + 1);
      uint32_t opt2_B = base;
      uint32_t opt2_A = base + (uint32_t)(idxB + 1);
      bool ok1 = (valA == opt1_A && valB == opt1_B);
      bool ok2 = (valA == opt2_A && valB == opt2_B);
      if (!ok1 && !ok2) {
         snprintf(fail_msg, fail_msg_len,
                  "outv[%d,%d] inconsistent got (%u,%u) want (%u,%u) or (%u,%u) total=%u",
                  idxA, idxB, res->outv[idxA], res->outv[idxB],
                  opt1_A + 1000u, opt1_B + 1000u, opt2_A + 1000u, opt2_B + 1000u, res->total);
         return 1;
      }
   }
   for (int i = 32; i < NUM_OUTV; i++) {
      if (res->outv[i] != 0xcdcdcdcd) {
         snprintf(fail_msg, fail_msg_len,
                  "outv[%d] want 0xcdcdcdcd got 0x%08x total=%u",
                  i, res->outv[i], res->total);
         return 1;
      }
   }
   if (res->total != 32) {
      snprintf(fail_msg, fail_msg_len,
               "total want 32 got %u", res->total);
      return 1;
   }
   for (int g = 0; g < GUARD_BYTES; g++) {
      if (guard[g] != 0xab) {
         snprintf(fail_msg, fail_msg_len,
                  "guard corrupted at +%d want 0xab got 0x%02x total=%u",
                  g, guard[g], res->total);
         return 1;
      }
   }
   return 0;
}

/* Case 2: indexed_dup */
static int
check_indexed_dup(const struct ssbo_data *res, const uint8_t *guard,
                  char *fail_msg, size_t fail_msg_len)
{
   static const int expected_v[] = {0, 1, 2, 3, 5};
   for (size_t j = 0; j < sizeof(expected_v) / sizeof(expected_v[0]); j++) {
      int v = expected_v[j];
      if (res->hit[v] < 1) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want >= 1 got %u total=%u", v, res->hit[v], res->total);
         return 1;
      }
   }
   if (res->hit[4] != 0) {
      snprintf(fail_msg, fail_msg_len,
               "hit[4] want 0 got %u total=%u", res->hit[4], res->total);
      return 1;
   }
   for (int v = 6; v < NUM_HIT; v++) {
      if (res->hit[v] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", v, res->hit[v], res->total);
         return 1;
      }
   }
   if (res->total < 5) {
      snprintf(fail_msg, fail_msg_len,
               "total want >= 5 got %u", res->total);
      return 1;
   }
   for (int g = 0; g < GUARD_BYTES; g++) {
      if (guard[g] != 0xab) {
         snprintf(fail_msg, fail_msg_len,
                  "guard corrupted at +%d want 0xab got 0x%02x total=%u",
                  g, guard[g], res->total);
         return 1;
      }
   }
   return 0;
}

/* Case 3: restart */
static int
check_restart(const struct ssbo_data *res, const uint8_t *guard,
              char *fail_msg, size_t fail_msg_len)
{
   for (int v = 0; v <= 5; v++) {
      if (res->hit[v] < 1) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want >= 1 got %u total=%u", v, res->hit[v], res->total);
         return 1;
      }
   }
   for (int v = 6; v < NUM_HIT; v++) {
      if (res->hit[v] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", v, res->hit[v], res->total);
         return 1;
      }
   }
   if (res->total > 6) {
      snprintf(fail_msg, fail_msg_len,
               "total want <= 6 got %u", res->total);
      return 1;
   }
   for (int g = 0; g < GUARD_BYTES; g++) {
      if (guard[g] != 0xab) {
         snprintf(fail_msg, fail_msg_len,
                  "guard corrupted at +%d want 0xab got 0x%02x total=%u",
                  g, guard[g], res->total);
         return 1;
      }
   }
   return 0;
}

/* Case 4: instanced */
static int
check_instanced(const struct ssbo_data *res, const uint8_t *guard,
                char *fail_msg, size_t fail_msg_len)
{
   for (int i = 0; i < 8; i++) {
      if (res->hit[i] != 3) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 3 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   for (int i = 8; i < NUM_HIT; i++) {
      if (res->hit[i] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   if (res->total != 24) {
      snprintf(fail_msg, fail_msg_len,
               "total want 24 got %u", res->total);
      return 1;
   }
   for (int g = 0; g < GUARD_BYTES; g++) {
      if (guard[g] != 0xab) {
         snprintf(fail_msg, fail_msg_len,
                  "guard corrupted at +%d want 0xab got 0x%02x total=%u",
                  g, guard[g], res->total);
         return 1;
      }
   }
   return 0;
}

/* Case 5: firstvertex */
static int
check_firstvertex(const struct ssbo_data *res, const uint8_t *guard,
                  char *fail_msg, size_t fail_msg_len)
{
   for (int i = 0; i < 20; i++) {
      if (res->hit[i] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   for (int i = 20; i <= 27; i++) {
      if (res->hit[i] != 1) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 1 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   for (int i = 28; i < NUM_HIT; i++) {
      if (res->hit[i] != 0) {
         snprintf(fail_msg, fail_msg_len,
                  "hit[%d] want 0 got %u total=%u", i, res->hit[i], res->total);
         return 1;
      }
   }
   if (res->total != 8) {
      snprintf(fail_msg, fail_msg_len,
               "total want 8 got %u", res->total);
      return 1;
   }
   for (int g = 0; g < GUARD_BYTES; g++) {
      if (guard[g] != 0xab) {
         snprintf(fail_msg, fail_msg_len,
                  "guard corrupted at +%d want 0xab got 0x%02x total=%u",
                  g, guard[g], res->total);
         return 1;
      }
   }
   return 0;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }

   (void)silence_unused;

   dx7_device_hook = device_hook;

   struct dx7 t;
   dx7_init(&t, argv[1], NULL);

   if (!enabled_features.vertexPipelineStoresAndAtomics) {
      printf("SKIP vertexPipelineStoresAndAtomics not supported\n");
      return 0;
   }

   load_extra_funcs(&t, argv[1]);

   struct rt4 rt;
   create_rt4(&t, &rt);

   /* Descriptor set layout for storage buffer at set 0 binding 0 */
   VkDescriptorSetLayoutBinding bind = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = NULL,
   };
   VkDescriptorSetLayoutCreateInfo dslci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &bind,
   };
   VkDescriptorSetLayout dsl;
   CK(vkCreateDescriptorSetLayout(t.dev, &dslci, NULL, &dsl), "CreateDSL");

   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &dsl,
   };
   VkPipelineLayout playout;
   CK(vkCreatePipelineLayout(t.dev, &plci, NULL, &playout), "CreatePipelineLayout");

   VkDescriptorPoolSize pool_size = {
      .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 64,
   };
   VkDescriptorPoolCreateInfo dpci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 64,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
   };
   VkDescriptorPool dpool;
   CK(vkCreateDescriptorPool(t.dev, &dpci, NULL, &dpool), "CreateDP");

   /* Find graphics queue family index */
   uint32_t qn = 8;
   VkQueueFamilyProperties qp[8];
   vkGetPhysicalDeviceQueueFamilyProperties(t.phys, &qn, qp);
   uint32_t qi = 0;
   while (qi < qn && !(qp[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      qi++;

   VkCommandPoolCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi,
   };
   VkCommandPool cmd_pool;
   CK(vkCreateCommandPool(t.dev, &cpci, NULL, &cmd_pool), "CreateCommandPool");

   int fails = 0;

   /* 1. iter0..iter5 */
   /* same pipeline reused for iter0-2 */
   VkPipeline pipe_iter0_2 =
      create_pipe(&t, playout, rt.rp, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE);

   const char *iter_names[] = {"iter0", "iter1", "iter2", "iter3", "iter4", "iter5"};
   for (int i = 0; i < 3; i++) {
      struct test_case tc = {
         .name = iter_names[i],
         .pipe = pipe_iter0_2,
         .vertex_count = 32,
         .instance_count = 1,
         .first_vertex = 0,
         .first_instance = 0,
         .check_fn = check_iter,
      };
      fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc);
   }

   /* a NEWLY created pipeline for iter3-5 */
   VkPipeline pipe_iter3_5 =
      create_pipe(&t, playout, rt.rp, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE);

   for (int i = 3; i < 6; i++) {
      struct test_case tc = {
         .name = iter_names[i],
         .pipe = pipe_iter3_5,
         .vertex_count = 32,
         .instance_count = 1,
         .first_vertex = 0,
         .first_instance = 0,
         .check_fn = check_iter,
      };
      fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc);
   }

   /* 2. indexed_dup: uint16 indices {0,1,2,2,3,3,3,5} */
   uint16_t indices_dup[] = {0, 1, 2, 2, 3, 3, 3, 5};
   struct test_case tc_dup = {
      .name = "indexed_dup",
      .pipe = pipe_iter3_5,
      .indexed = 1,
      .indices = indices_dup,
      .index_count = sizeof(indices_dup) / sizeof(indices_dup[0]),
      .instance_count = 1,
      .first_instance = 0,
      .check_fn = check_indexed_dup,
   };
   fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc_dup);

   /* 3. restart: triangle strip, uint16 indices {0,1,2,0xFFFF,3,4,5}, primitiveRestartEnable=VK_TRUE */
   uint16_t indices_restart[] = {0, 1, 2, 0xFFFF, 3, 4, 5};
   VkPipeline pipe_restart =
      create_pipe(&t, playout, rt.rp, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE);

   struct test_case tc_restart = {
      .name = "restart",
      .pipe = pipe_restart,
      .indexed = 1,
      .indices = indices_restart,
      .index_count = sizeof(indices_restart) / sizeof(indices_restart[0]),
      .instance_count = 1,
      .first_instance = 0,
      .check_fn = check_restart,
   };
   fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc_restart);

   /* 4. instanced: non-indexed 8 vertices x 3 instances */
   struct test_case tc_instanced = {
      .name = "instanced",
      .pipe = pipe_iter3_5,
      .vertex_count = 8,
      .instance_count = 3,
      .first_vertex = 0,
      .first_instance = 0,
      .check_fn = check_instanced,
   };
   fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc_instanced);

   /* 5. firstvertex: non-indexed draw vertexCount=8 firstVertex=20 */
   struct test_case tc_firstvertex = {
      .name = "firstvertex",
      .pipe = pipe_iter3_5,
      .vertex_count = 8,
      .instance_count = 1,
      .first_vertex = 20,
      .first_instance = 0,
      .check_fn = check_firstvertex,
   };
   fails += run_test_case(&t, &rt, playout, dpool, dsl, cmd_pool, &tc_firstvertex);

   /* Cleanup */
   vkDestroyPipeline(t.dev, pipe_iter0_2, NULL);
   vkDestroyPipeline(t.dev, pipe_iter3_5, NULL);
   vkDestroyPipeline(t.dev, pipe_restart, NULL);

   vkDestroyCommandPool(t.dev, cmd_pool, NULL);
   vkDestroyDescriptorPool(t.dev, dpool, NULL);
   vkDestroyPipelineLayout(t.dev, playout, NULL);
   vkDestroyDescriptorSetLayout(t.dev, dsl, NULL);

   destroy_rt4(&t, &rt);

   printf("VERTEX_STORES_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
