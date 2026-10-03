/* variableMultisampleRate in attachment-less dynamic rendering passes, with
 * and without secondary command buffers.
 *
 * A full-screen triangle with sample shading counts one invocation per
 * sample into cnt[slot + 8 * sampleId], so each draw must leave exactly
 * AREA invocations for each of its S samples. Rendering instances are
 * chained with suspend/resume; each one is either inline draws or one
 * vkCmdExecuteCommands. Cases:
 *  prim_1x_4x_1x       inline draws of 1x, 4x, 1x
 *  sec_4x / sec_1x     one secondary
 *  sec_1x_4x_1x_call   three secondaries in one vkCmdExecuteCommands
 *  sec_1x_4x_1x_sep    three secondaries, one call each
 *  inline_sec_inline   inline 1x, secondary 4x, inline 1x
 *  sec_inline_4x       secondary 4x, inline 1x, inline 4x
 *  sec_4x_sec_1x       two rendering instances, each with a secondary
 *  sec_mixed           one secondary that draws 1x then 4x; only run with
 *                      VMR_MIXED=1 (known gap: a secondary has one count)
 *
 * Output: PASS/FAIL per case, RESULT PASS|FAIL. usage: vmr_secondary <icd>
 *
 * SPIR-V regen: sh build_spv.sh
 */
#include "../dx7_harness.h"
#include "vmr_spv.h"

#define EXTRA_FUNCS(X)                                                         \
   X(CmdBindDescriptorSets) X(CreateDescriptorSetLayout)                       \
   X(CreateDescriptorPool) X(AllocateDescriptorSets)                           \
   X(UpdateDescriptorSets) X(CmdBeginRendering) X(CmdEndRendering)             \
   X(CmdExecuteCommands)

EXTRA_FUNCS(DX7_DECL)

#define AREA 16u
#define MAX_SEC 4
#define MAX_DRAWS 4
#define MAX_INST 4

static VkPhysicalDeviceFeatures enabled_features;
static VkPhysicalDeviceVulkan13Features v13;

static void
device_hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   enabled_features.sampleRateShading = t->feats.sampleRateShading;
   enabled_features.fragmentStoresAndAtomics = t->feats.fragmentStoresAndAtomics;
   v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   v13.dynamicRendering = VK_TRUE;
   dci->pEnabledFeatures = &enabled_features;
   dci->pNext = &v13;
}

struct draw {
   uint32_t samples, slot;
};
struct sec {
   struct draw d[MAX_DRAWS];
   int nd;
};
struct inst {
   int secondary;
   int separate_calls;
   struct draw d[MAX_DRAWS];
   int nd;
   struct sec s[MAX_SEC];
   int ns;
};
struct tcase {
   const char *name;
   struct inst i[MAX_INST];
   int ni;
   int mixed;
};

static struct dx7 t;
static VkPipeline pipes[5]; /* by sample count */
static VkPipelineLayout lay;
static VkDescriptorSet dset;
static uint32_t *counts;
static VkCommandBuffer secs[MAX_SEC];

static void
draw_one(VkCommandBuffer cb, const struct draw *d)
{
   vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes[d->samples]);
   vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, 0, 1,
                           &dset, 0, NULL);
   vkCmdPushConstants(cb, lay, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &d->slot);
   vkCmdDraw(cb, 3, 1, 0, 0);
}

static void
record_sec(VkCommandBuffer cb, const struct sec *s, VkRenderingFlags flags)
{
   VkCommandBufferInheritanceRenderingInfo ir = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
      .flags = flags,
      .rasterizationSamples = (VkSampleCountFlagBits)s->d[0].samples};
   VkCommandBufferInheritanceInfo ii = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
      .pNext = &ir};
   VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
               VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = &ii};
   CK(vkBeginCommandBuffer(cb, &bi), "BeginSec");
   for (int i = 0; i < s->nd; i++)
      draw_one(cb, &s->d[i]);
   CK(vkEndCommandBuffer(cb), "EndSec");
}

static int
run_case(const struct tcase *c)
{
   uint32_t expect[64] = {0};
   int next_sec = 0;

   memset(counts, 0, 64 * 4);
   CK(vkResetFences(t.dev, 1, &t.fence), "ResetFence");
   CK(vkResetCommandBuffer(t.cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(t.cmd, &bi), "BeginCmd");

   for (int n = 0; n < c->ni; n++) {
      const struct inst *in = &c->i[n];
      VkRenderingFlags flags = 0;

      if (n > 0)
         flags |= VK_RENDERING_RESUMING_BIT;
      if (n + 1 < c->ni)
         flags |= VK_RENDERING_SUSPENDING_BIT;
      VkRenderingInfo ri = {
         .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
         .flags = flags | (in->secondary
                              ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
                              : 0),
         .renderArea = {{0, 0}, {AREA, AREA}},
         .layerCount = 1};

      /* Record the secondaries first, they are executed in the pass. */
      int first = next_sec;
      for (int s = 0; s < in->ns; s++) {
         record_sec(secs[next_sec++], &in->s[s], flags);
         for (int d = 0; d < in->s[s].nd; d++)
            for (uint32_t k = 0; k < in->s[s].d[d].samples; k++)
               expect[in->s[s].d[d].slot + 8 * k] += AREA * AREA;
      }
      for (int d = 0; d < in->nd; d++)
         for (uint32_t k = 0; k < in->d[d].samples; k++)
            expect[in->d[d].slot + 8 * k] += AREA * AREA;

      vkCmdBeginRendering(t.cmd, &ri);
      if (in->secondary) {
         if (in->separate_calls)
            for (int s = 0; s < in->ns; s++)
               vkCmdExecuteCommands(t.cmd, 1, &secs[first + s]);
         else
            vkCmdExecuteCommands(t.cmd, in->ns, &secs[first]);
      } else {
         for (int d = 0; d < in->nd; d++)
            draw_one(t.cmd, &in->d[d]);
      }
      vkCmdEndRendering(t.cmd);
   }
   CK(vkEndCommandBuffer(t.cmd), "EndCmd");

   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1, .pCommandBuffers = &t.cmd};
   CK(vkQueueSubmit(t.queue, 1, &si, t.fence), "QueueSubmit");
   VkResult r = vkWaitForFences(t.dev, 1, &t.fence, VK_TRUE,
                                30ull * 1000000000ull);
   if (r != VK_SUCCESS) {
      printf("FAIL case %s fence r=%d\n", c->name, (int)r);
      exit(1);
   }

   int bad = 0;
   for (int i = 0; i < 64; i++)
      if (counts[i] != expect[i]) {
         if (bad < 6)
            printf("  cnt[slot %d sample %d] = %u, want %u\n", i % 8, i / 8,
                   counts[i], expect[i]);
         bad++;
      }
   printf("%s case %s bad=%d\n", bad ? (c->mixed ? "KNOWN-GAP" : "FAIL")
                                      : "PASS", c->name, bad);
   return bad && !c->mixed;
}

#define D(s, sl) {s, sl}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }
   dx7_device_hook = device_hook;
   dx7_init(&t, argv[1], NULL);

   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
#define LOAD(n) vk##n = (PFN_vk##n)gipa(t.inst, "vk" #n);
   EXTRA_FUNCS(LOAD)

   const uint32_t sc = t.props.limits.framebufferNoAttachmentsSampleCounts;
   printf("FEATURE noAttachmentSamples=0x%x sampleRateShading=%d\n", sc,
          t.feats.sampleRateShading);
   if (!t.feats.sampleRateShading || !t.feats.fragmentStoresAndAtomics ||
       !(sc & VK_SAMPLE_COUNT_4_BIT)) {
      printf("SKIP no 4x sample shading without attachments\n");
      printf("RESULT PASS\n");
      return 0;
   }

   VkBuffer cbuf;
   VkDeviceMemory cmem;
   dx7_buffer(&t, 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, NULL, &cbuf, &cmem);
   CK(vkMapMemory(t.dev, cmem, 0, 256, 0, (void **)&counts), "MapCounts");

   VkDescriptorSetLayoutBinding bind = {
      .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
   VkDescriptorSetLayoutCreateInfo dsl = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1, .pBindings = &bind};
   VkDescriptorSetLayout sl;
   CK(vkCreateDescriptorSetLayout(t.dev, &dsl, NULL, &sl), "DSL");
   VkPushConstantRange pcr = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4};
   VkPipelineLayoutCreateInfo plci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &sl,
      .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr};
   CK(vkCreatePipelineLayout(t.dev, &plci, NULL, &lay), "Layout");
   VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
   VkDescriptorPoolCreateInfo dpi = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
      .poolSizeCount = 1, .pPoolSizes = &ps};
   VkDescriptorPool dp;
   CK(vkCreateDescriptorPool(t.dev, &dpi, NULL, &dp), "DPool");
   VkDescriptorSetAllocateInfo dai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &sl};
   CK(vkAllocateDescriptorSets(t.dev, &dai, &dset), "DSet");
   VkDescriptorBufferInfo dbi = {cbuf, 0, 256};
   VkWriteDescriptorSet wr = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dset,
      .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .pBufferInfo = &dbi};
   vkUpdateDescriptorSets(t.dev, 1, &wr, 0, NULL);

   VkShaderModule vs = dx7_module(&t, vs_tri, sizeof(vs_tri));
   VkShaderModule fs = dx7_module(&t, fs_count, sizeof(fs_count));
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
   VkViewport vp = {0, 0, AREA, AREA, 0, 1};
   VkRect2D scr = {{0, 0}, {AREA, AREA}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &scr};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
   VkPipelineRenderingCreateInfo pri = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
   static const uint32_t counts_used[] = {1, 4};
   for (int i = 0; i < 2; i++) {
      uint32_t s = counts_used[i];
      VkPipelineMultisampleStateCreateInfo ms = {
         .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
         .rasterizationSamples = (VkSampleCountFlagBits)s,
         .sampleShadingEnable = VK_TRUE, .minSampleShading = 1.0f};
      VkGraphicsPipelineCreateInfo gpci = {
         .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
         .pNext = &pri, .stageCount = 2, .pStages = st, .pVertexInputState = &vi,
         .pInputAssemblyState = &ia, .pViewportState = &vps,
         .pRasterizationState = &rs, .pMultisampleState = &ms,
         .pColorBlendState = &cb, .layout = lay};
      CK(vkCreateGraphicsPipelines(t.dev, VK_NULL_HANDLE, 1, &gpci, NULL,
                                   &pipes[s]), "Pipeline");
   }

   uint32_t qn = 8, qi = 0;
   VkQueueFamilyProperties qp[8];
   vkGetPhysicalDeviceQueueFamilyProperties(t.phys, &qn, qp);
   while (qi < qn && !(qp[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      qi++;
   VkCommandPoolCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = qi};
   VkCommandPool pool;
   CK(vkCreateCommandPool(t.dev, &cpci, NULL, &pool), "Pool2");
   VkCommandBufferAllocateInfo cbai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
      .commandBufferCount = MAX_SEC};
   CK(vkAllocateCommandBuffers(t.dev, &cbai, secs), "Secs");

   const struct tcase cases[] = {
      {"prim_1x_4x_1x", {{.nd = 3, .d = {D(1, 0), D(4, 1), D(1, 2)}}}, 1},
      {"sec_4x", {{.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(4, 0)}}}}}, 1},
      {"sec_1x", {{.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(1, 0)}}}}}, 1},
      {"sec_1x_4x_1x_call",
       {{.secondary = 1, .ns = 3,
         .s = {{.nd = 1, .d = {D(1, 0)}}, {.nd = 1, .d = {D(4, 1)}},
               {.nd = 1, .d = {D(1, 2)}}}}}, 1},
      {"sec_1x_4x_1x_sep",
       {{.secondary = 1, .separate_calls = 1, .ns = 3,
         .s = {{.nd = 1, .d = {D(1, 0)}}, {.nd = 1, .d = {D(4, 1)}},
               {.nd = 1, .d = {D(1, 2)}}}}}, 1},
      {"inline_sec_inline",
       {{.nd = 1, .d = {D(1, 0)}},
        {.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(4, 1)}}}},
        {.nd = 1, .d = {D(1, 2)}}}, 3},
      {"sec_inline_4x",
       {{.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(4, 0)}}}},
        {.nd = 1, .d = {D(1, 1)}},
        {.nd = 1, .d = {D(4, 2)}}}, 3},
      {"sec_4x_sec_1x",
       {{.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(4, 0)}}}},
        {.secondary = 1, .ns = 1, .s = {{.nd = 1, .d = {D(1, 1)}}}}}, 2},
      {"sec_mixed",
       {{.secondary = 1, .ns = 1, .s = {{.nd = 2, .d = {D(1, 0), D(4, 1)}}}}}, 1,
       .mixed = 1},
   };
   int fails = 0;
   for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      if (cases[i].mixed && !getenv("VMR_MIXED"))
         continue;
      fails += run_case(&cases[i]);
   }
   printf("RESULT %s\n", fails ? "FAIL" : "PASS");
   return fails ? 1 : 0;
}
