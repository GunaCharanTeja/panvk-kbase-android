/* VK_EXT_transform_feedback conformance-style test.
 * Verifies transform feedback capture, streams, tessellation capture,
 * pause/resume counter buffers, buffer overflow, draw indirect byte count,
 * and stream queries. */
#include "../dx7_harness.h"
#include "xfb_spv.h"
#include <math.h>

#define XFB_FUNCS(X)                                                           \
   X(EnumerateDeviceExtensionProperties)                                       \
   X(GetPhysicalDeviceFeatures2)                                               \
   X(GetPhysicalDeviceProperties2)                                             \
   X(QueueWaitIdle)                                                            \
   X(CmdBindTransformFeedbackBuffersEXT)                                       \
   X(CmdBeginTransformFeedbackEXT)                                             \
   X(CmdEndTransformFeedbackEXT)                                               \
   X(CmdBeginConditionalRenderingEXT)                                          \
   X(CmdEndConditionalRenderingEXT)                                            \
   X(CmdBeginQueryIndexedEXT)                                                  \
   X(CmdEndQueryIndexedEXT)                                                    \
   X(CmdDrawIndirectByteCountEXT)                                              \
   X(CreateQueryPool)                                                          \
   X(CmdResetQueryPool)                                                        \
   X(GetQueryPoolResults)                                                      \
   X(CmdCopyQueryPoolResults)                                                  \
   X(DestroyQueryPool)                                                         \
   X(DestroyBuffer)                                                            \
   X(FreeMemory)

XFB_FUNCS(DX7_DECL)

static const char *icd_path;
static int has_xfb_ext = 0;
static int has_cond_render_ext = 0;
static VkPhysicalDeviceFeatures enabled_features;
static VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb_features;
static VkPhysicalDeviceConditionalRenderingFeaturesEXT cond_render_features;
static VkPhysicalDeviceTransformFeedbackPropertiesEXT xfb_props;

struct xfb_pipe_desc {
   const uint32_t *vs;
   size_t vs_size;
   const uint32_t *gs;
   size_t gs_size;
   const uint32_t *tcs;
   size_t tcs_size;
   const uint32_t *tes;
   size_t tes_size;
   uint32_t patch_control_points;
   VkPrimitiveTopology topology;
};

static VkPipeline
xfb_pipeline(struct dx7 *t, const struct xfb_pipe_desc *d)
{
   VkShaderModule vs = dx7_module(t, d->vs, d->vs_size);
   VkShaderModule fs = dx7_module(t, fs_xfb, sizeof(fs_xfb));
   VkShaderModule gs = d->gs ? dx7_module(t, d->gs, d->gs_size) : VK_NULL_HANDLE;
   VkShaderModule tcs = d->tcs ? dx7_module(t, d->tcs, d->tcs_size) : VK_NULL_HANDLE;
   VkShaderModule tes = d->tes ? dx7_module(t, d->tes, d->tes_size) : VK_NULL_HANDLE;

   VkPipelineShaderStageCreateInfo stages[5];
   uint32_t nstages = 0;

   stages[nstages++] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vs,
      .pName = "main",
   };
   if (tcs) {
      stages[nstages++] = (VkPipelineShaderStageCreateInfo){
         .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
         .module = tcs,
         .pName = "main",
      };
   }
   if (tes) {
      stages[nstages++] = (VkPipelineShaderStageCreateInfo){
         .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
         .module = tes,
         .pName = "main",
      };
   }
   if (gs) {
      stages[nstages++] = (VkPipelineShaderStageCreateInfo){
         .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
         .module = gs,
         .pName = "main",
      };
   }
   stages[nstages++] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fs,
      .pName = "main",
   };

   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = NULL,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = NULL,
   };

   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = d->topology,
      .primitiveRestartEnable = VK_FALSE,
   };

   VkPipelineTessellationStateCreateInfo ts = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .patchControlPoints = d->patch_control_points ? d->patch_control_points : 3,
   };

   VkViewport vp = {0.0f, 0.0f, (float)RT_W, (float)RT_H, 0.0f, 1.0f};
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &vp,
      .scissorCount = 1,
      .pScissors = &sc,
   };

   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .rasterizerDiscardEnable = VK_TRUE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
   };

   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
   };

   VkGraphicsPipelineCreateInfo gpci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = nstages,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pTessellationState = tcs ? &ts : NULL,
      .pViewportState = &vps,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = NULL,
      .pDynamicState = NULL,
      .layout = t->layout,
      .renderPass = t->rp,
      .subpass = 0,
   };

   VkPipeline pipe;
   CK(vkCreateGraphicsPipelines(t->dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe),
      "CreatePipe");

   vkDestroyShaderModule(t->dev, vs, NULL);
   vkDestroyShaderModule(t->dev, fs, NULL);
   if (gs)
      vkDestroyShaderModule(t->dev, gs, NULL);
   if (tcs)
      vkDestroyShaderModule(t->dev, tcs, NULL);
   if (tes)
      vkDestroyShaderModule(t->dev, tes, NULL);

   return pipe;
}

static void
xfb_begin(struct dx7 *t)
{
   CK(vkResetFences(t->dev, 1, &t->fence), "ResetFence");
   CK(vkResetCommandBuffer(t->cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
   };
   CK(vkBeginCommandBuffer(t->cmd, &bbi), "BeginCmd");
}

static void
xfb_rp_begin(struct dx7 *t)
{
   VkClearValue clear = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = t->rp,
      .framebuffer = t->fb,
      .renderArea = {{0, 0}, {RT_W, RT_H}},
      .clearValueCount = 1,
      .pClearValues = &clear,
   };
   vkCmdBeginRenderPass(t->cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
}

static void
xfb_rp_end(struct dx7 *t)
{
   vkCmdEndRenderPass(t->cmd);
}

static void
xfb_end_and_wait(struct dx7 *t)
{
   VkMemoryBarrier mb = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
                       VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |
                       VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
   };
   vkCmdPipelineBarrier(t->cmd,
                        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
                        NULL);
   CK(vkEndCommandBuffer(t->cmd), "EndCmd");

   VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &t->cmd,
   };
   CK(vkQueueSubmit(t->queue, 1, &si, t->fence), "QueueSubmit");
   CK(vkWaitForFences(t->dev, 1, &t->fence, VK_TRUE, 30ull * 1000000000ull),
      "WaitForFences");
   if (vkQueueWaitIdle)
      vkQueueWaitIdle(t->queue);
}

struct vec4 {
   float x, y, z, w;
};

static void
sort3_vec4(struct vec4 v[3])
{
   for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2 - i; j++) {
         if (v[j].x > v[j + 1].x) {
            struct vec4 tmp = v[j];
            v[j] = v[j + 1];
            v[j + 1] = tmp;
         }
      }
   }
}

/* 1. vs_capture: VS-only TRIANGLE_LIST and TRIANGLE_STRIP */
static int
test_vs_capture(struct dx7 *t)
{
   (void)dx7_pipeline;
   (void)dx7_run;
   (void)dx7_check;

   int fails = 0;
   const VkDeviceSize sz = 256;
   VkBuffer buf;
   VkDeviceMemory mem;
   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);

   /* 1A. TRIANGLE_LIST */
   struct xfb_pipe_desc d_list = {
      .vs = vs_mode0,
      .vs_size = sizeof(vs_mode0),
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
   };
   VkPipeline pipe_list = xfb_pipeline(t, &d_list);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_list);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 6, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   int bad_list = 0;
   for (int i = 0; i < 6; i++) {
      const float *v = (const float *)(ptr + i * 20);
      float want[5] = {(float)i, (float)i * 2.0f, (float)i * 3.0f, 1.0f,
                       (float)i * 10.0f};
      for (int j = 0; j < 5; j++) {
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad_list++;
      }
   }
   vkUnmapMemory(t->dev, mem);
   printf("CASE vs_capture_tri_list mismatch=%d %s\n", bad_list,
          bad_list ? "FAIL" : "PASS");
   if (bad_list)
      fails++;
   vkDestroyPipeline(t->dev, pipe_list, NULL);

   /* 1B. TRIANGLE_STRIP */
   struct xfb_pipe_desc d_strip = {
      .vs = vs_mode0,
      .vs_size = sizeof(vs_mode0),
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
   };
   VkPipeline pipe_strip = xfb_pipeline(t, &d_strip);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_strip);
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 4, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   int bad_strip = 0;
   /* Order {i, i+1+(i%2), i+2-(i%2)} for triangles i=0, 1 => {0,1,2, 1,3,2} */
   int want_strip_idx[6] = {0, 1, 2, 1, 3, 2};
   for (int i = 0; i < 6; i++) {
      const float *v = (const float *)(ptr + i * 20);
      int idx = want_strip_idx[i];
      float want[5] = {(float)idx, (float)idx * 2.0f, (float)idx * 3.0f, 1.0f,
                       (float)idx * 10.0f};
      for (int j = 0; j < 5; j++) {
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad_strip++;
      }
   }
   vkUnmapMemory(t->dev, mem);
   printf("CASE vs_capture_tri_strip mismatch=%d %s\n", bad_strip,
          bad_strip ? "FAIL" : "PASS");
   if (bad_strip)
      fails++;
   vkDestroyPipeline(t->dev, pipe_strip, NULL);

   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
   }
   return fails;
}

/* 2. gs_streams: GS with points input emitting on stream 0 and stream 1 */
static int
test_gs_streams(struct dx7 *t)
{
   if (!t->feats.geometryShader) {
      printf("SKIP gs_streams: geometryShader not exposed\n");
      return 0;
   }
   if (!xfb_features.geometryStreams) {
      printf("SKIP gs_streams: geometryStreams not exposed\n");
      return 0;
   }

   struct xfb_pipe_desc d = {
      .vs = vs_mode1,
      .vs_size = sizeof(vs_mode1),
      .gs = gs_xfb,
      .gs_size = sizeof(gs_xfb),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   VkBuffer bufs[2], cbufs[2];
   VkDeviceMemory mems[2], cmems[2];
   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;

   for (int i = 0; i < 2; i++) {
      dx7_buffer(t, sz,
                 VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 NULL, &bufs[i], &mems[i]);
      dx7_buffer(t, csz,
                 VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 NULL, &cbufs[i], &cmems[i]);
   }

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize offs[2] = {0, 0};
   VkDeviceSize szs[2] = {sz, sz};
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 2, bufs, offs, szs);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 2, 1, 0, 0);
   VkDeviceSize coffs[2] = {0, 0};
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 2, cbufs, coffs);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint32_t *c0 = NULL, *c1 = NULL;
   CK(vkMapMemory(t->dev, cmems[0], 0, csz, 0, (void **)&c0), "MapC0");
   CK(vkMapMemory(t->dev, cmems[1], 0, csz, 0, (void **)&c1), "MapC1");
   uint32_t cnt0 = *c0;
   uint32_t cnt1 = *c1;
   vkUnmapMemory(t->dev, cmems[0]);
   vkUnmapMemory(t->dev, cmems[1]);

   int bad = 0;
   if (cnt0 != 4 * 16) {
      printf("cnt0 mismatch: got %u want %u\n", cnt0, 4 * 16);
      bad++;
   }
   if (cnt1 != 2 * 16) {
      printf("cnt1 mismatch: got %u want %u\n", cnt1, 2 * 16);
      bad++;
   }

   uint8_t *p0 = NULL, *p1 = NULL;
   CK(vkMapMemory(t->dev, mems[0], 0, sz, 0, (void **)&p0), "MapP0");
   CK(vkMapMemory(t->dev, mems[1], 0, sz, 0, (void **)&p1), "MapP1");

   /* Stream 0 expected: 4 vertices (stride 16) */
   float want_s0[4][4] = {
      {1.0f, 2.0f, 3.0f, 4.0f},
      {2.0f, 6.0f, 7.0f, 8.0f},
      {11.0f, 2.0f, 3.0f, 4.0f},
      {12.0f, 6.0f, 7.0f, 8.0f},
   };
   for (int i = 0; i < 4; i++) {
      const float *v = (const float *)(p0 + i * 16);
      for (int j = 0; j < 4; j++)
         if (fabsf(v[j] - want_s0[i][j]) > 1e-4f)
            bad++;
   }

   /* Stream 1 expected: 2 vertices (stride 16) */
   float want_s1[2][4] = {
      {1.0f, 20.0f, 30.0f, 40.0f},
      {101.0f, 20.0f, 30.0f, 40.0f},
   };
   for (int i = 0; i < 2; i++) {
      const float *v = (const float *)(p1 + i * 16);
      for (int j = 0; j < 4; j++)
         if (fabsf(v[j] - want_s1[i][j]) > 1e-4f)
            bad++;
   }

   vkUnmapMemory(t->dev, mems[0]);
   vkUnmapMemory(t->dev, mems[1]);

   printf("CASE gs_streams s0_cnt=%u s1_cnt=%u mismatch=%d %s\n", cnt0, cnt1,
          bad, bad ? "FAIL" : "PASS");

   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      for (int i = 0; i < 2; i++) {
         vkDestroyBuffer(t->dev, bufs[i], NULL);
         vkDestroyBuffer(t->dev, cbufs[i], NULL);
         vkFreeMemory(t->dev, mems[i], NULL);
         vkFreeMemory(t->dev, cmems[i], NULL);
      }
   }
   return bad ? 1 : 0;
}

/* 3. tes_capture: VS->TCS->TES capturing TES outputs */
static int
test_tes_capture(struct dx7 *t)
{
   if (!t->feats.tessellationShader) {
      printf("SKIP tes_capture: tessellationShader not exposed\n");
      return 0;
   }

   struct xfb_pipe_desc d = {
      .vs = vs_mode1,
      .vs_size = sizeof(vs_mode1),
      .tcs = tcs_xfb,
      .tcs_size = sizeof(tcs_xfb),
      .tes = tes_xfb,
      .tes_size = sizeof(tes_xfb),
      .patch_control_points = 3,
      .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;
   VkBuffer buf, cbuf;
   VkDeviceMemory mem, cmem;

   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf, &cmem);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   /* Draw 2 patches (6 vertices total) */
   vkCmdDraw(t->cmd, 6, 1, 0, 0);
   VkDeviceSize coff = 0;
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint32_t *c = NULL;
   CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
   uint32_t cnt = *c;
   vkUnmapMemory(t->dev, cmem);

   int bad = 0;
   if (cnt != 6 * 16) {
      printf("tes_capture counter mismatch: got %u want %u\n", cnt, 6 * 16);
      bad++;
   }

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");

   /* Patch 0: input vertices 0, 1, 2 */
   struct vec4 p0[3];
   memcpy(p0, ptr, sizeof(p0));
   sort3_vec4(p0);

   struct vec4 want0[3] = {
      {0.0f, 0.0f, 0.0f, 1.0f},
      {1.0f, 2.0f, 0.0f, 1.0f},
      {2.0f, 4.0f, 0.0f, 1.0f},
   };
   for (int i = 0; i < 3; i++) {
      if (fabsf(p0[i].x - want0[i].x) > 1e-4f ||
          fabsf(p0[i].y - want0[i].y) > 1e-4f ||
          fabsf(p0[i].z - want0[i].z) > 1e-4f ||
          fabsf(p0[i].w - want0[i].w) > 1e-4f)
         bad++;
   }

   /* Patch 1: input vertices 3, 4, 5 */
   struct vec4 p1[3];
   memcpy(p1, ptr + 3 * 16, sizeof(p1));
   sort3_vec4(p1);

   struct vec4 want1[3] = {
      {3.0f, 6.0f, 0.0f, 1.0f},
      {4.0f, 8.0f, 0.0f, 1.0f},
      {5.0f, 10.0f, 0.0f, 1.0f},
   };
   for (int i = 0; i < 3; i++) {
      if (fabsf(p1[i].x - want1[i].x) > 1e-4f ||
          fabsf(p1[i].y - want1[i].y) > 1e-4f ||
          fabsf(p1[i].z - want1[i].z) > 1e-4f ||
          fabsf(p1[i].w - want1[i].w) > 1e-4f)
         bad++;
   }

   vkUnmapMemory(t->dev, mem);
   printf("CASE tes_capture counter=%u mismatch=%d %s\n", cnt, bad,
          bad ? "FAIL" : "PASS");

   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkDestroyBuffer(t->dev, cbuf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
      vkFreeMemory(t->dev, cmem, NULL);
   }
   return bad ? 1 : 0;
}

/* 4. pause_resume: two render sections using counter buffer */
static int
test_pause_resume(struct dx7 *t)
{
   struct xfb_pipe_desc d = {
      .vs = vs_mode2,
      .vs_size = sizeof(vs_mode2),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;
   VkBuffer buf, cbuf;
   VkDeviceMemory mem, cmem;

   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf, &cmem);

   VkDeviceSize off = 0;
   VkDeviceSize coff = 0;

   xfb_begin(t);

   /* Section 1: draw 3 points, write counter buffer */
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 3, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
   xfb_rp_end(t);

   /* Barrier on counter buffer */
   VkBufferMemoryBarrier bar = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
      .dstAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = cbuf,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
   };
   vkCmdPipelineBarrier(t->cmd, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, 0, 0,
                        NULL, 1, &bar, 0, NULL);

   /* Section 2: resume with counter buffer, draw 3 more points, write counter */
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
   vkCmdDraw(t->cmd, 3, 1, 3, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
   xfb_rp_end(t);

   xfb_end_and_wait(t);

   uint32_t *c = NULL;
   CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
   uint32_t cnt = *c;
   vkUnmapMemory(t->dev, cmem);

   int bad = 0;
   if (cnt != 6 * 16) {
      printf("pause_resume counter mismatch: got %u want %u\n", cnt, 6 * 16);
      bad++;
   }

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   for (int i = 0; i < 6; i++) {
      const float *v = (const float *)(ptr + i * 16);
      float want[4] = {(float)i, (float)i + 1.0f, (float)i + 2.0f, 1.0f};
      for (int j = 0; j < 4; j++)
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad++;
   }
   vkUnmapMemory(t->dev, mem);

   printf("CASE pause_resume counter=%u mismatch=%d %s\n", cnt, bad,
          bad ? "FAIL" : "PASS");

   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkDestroyBuffer(t->dev, cbuf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
      vkFreeMemory(t->dev, cmem, NULL);
   }
   return bad ? 1 : 0;
}

/* 5. overflow: buffer sized for 2.5 triangles, draw 4 triangles with query */
static int
test_overflow(struct dx7 *t)
{
   if (!xfb_props.transformFeedbackQueries) {
      printf("SKIP overflow: transformFeedbackQueries not exposed\n");
      return 0;
   }

   struct xfb_pipe_desc d = {
      .vs = vs_mode2,
      .vs_size = sizeof(vs_mode2),
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;
   const VkDeviceSize bound_sz = 120; /* 2.5 triangles * 3 vertices * 16 bytes = 120 */

   /* Prefill with 0xdeadbeef */
   uint32_t init_pattern[64];
   for (int i = 0; i < 64; i++)
      init_pattern[i] = 0xdeadbeefu;

   VkBuffer buf, cbuf;
   VkDeviceMemory mem, cmem;

   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              init_pattern, &buf, &mem);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf, &cmem);

   VkQueryPool pool;
   VkQueryPoolCreateInfo qpci = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
      .queryCount = 1,
   };
   CK(vkCreateQueryPool(t->dev, &qpci, NULL, &pool), "CreateQueryPool");

   xfb_begin(t);
   vkCmdResetQueryPool(t->cmd, pool, 0, 1);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &bound_sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdBeginQueryIndexedEXT(t->cmd, pool, 0, 0, 0);
   /* Draw 4 triangles = 12 vertices */
   vkCmdDraw(t->cmd, 12, 1, 0, 0);
   vkCmdEndQueryIndexedEXT(t->cmd, pool, 0, 0);
   VkDeviceSize coff = 0;
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint64_t qres[2] = {0, 0};
   CK(vkGetQueryPoolResults(t->dev, pool, 0, 1, sizeof(qres), qres,
                            sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
      "GetQueryPoolResults");

   uint32_t *c = NULL;
   CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
   uint32_t cnt = *c;
   vkUnmapMemory(t->dev, cmem);

   int bad = 0;
   if (qres[0] != 2) {
      printf("overflow written mismatch: got %llu want 2\n",
             (unsigned long long)qres[0]);
      bad++;
   }
   if (qres[1] != 4) {
      printf("overflow needed mismatch: got %llu want 4\n",
             (unsigned long long)qres[1]);
      bad++;
   }
   if (cnt != 2 * 3 * 16) {
      printf("overflow counter mismatch: got %u want %u\n", cnt, 2 * 3 * 16);
      bad++;
   }

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");

   /* 2 whole triangles (6 vertices, 96 bytes) verified */
   for (int i = 0; i < 6; i++) {
      const float *v = (const float *)(ptr + i * 16);
      float want[4] = {(float)i, (float)i + 1.0f, (float)i + 2.0f, 1.0f};
      for (int j = 0; j < 4; j++)
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad++;
   }

   /* Bytes after 96 untouched: still 0xdeadbeef */
   const uint32_t *u32 = (const uint32_t *)ptr;
   for (uint32_t i = 96 / 4; i < sz / 4; i++) {
      if (u32[i] != 0xdeadbeefu)
         bad++;
   }
   vkUnmapMemory(t->dev, mem);

   printf("CASE overflow written=%llu needed=%llu counter=%u mismatch=%d %s\n",
          (unsigned long long)qres[0], (unsigned long long)qres[1], cnt, bad,
          bad ? "FAIL" : "PASS");

   vkDestroyQueryPool(t->dev, pool, NULL);
   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkDestroyBuffer(t->dev, cbuf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
      vkFreeMemory(t->dev, cmem, NULL);
   }
   return bad ? 1 : 0;
}

/* 6. draw_indirect_byte_count */
static int
test_draw_indirect_byte_count(struct dx7 *t)
{
   if (!xfb_props.transformFeedbackDraw) {
      printf("SKIP draw_indirect_byte_count: transformFeedbackDraw not exposed\n");
      return 0;
   }

   const uint32_t n = 5;
   const VkDeviceSize sz1 = 256;
   const VkDeviceSize sz2 = 256;
   const VkDeviceSize csz = 64;

   struct xfb_pipe_desc d1 = {
      .vs = vs_mode2,
      .vs_size = sizeof(vs_mode2),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe1 = xfb_pipeline(t, &d1);

   struct xfb_pipe_desc d2 = {
      .vs = vs_mode3,
      .vs_size = sizeof(vs_mode3),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe2 = xfb_pipeline(t, &d2);

   VkBuffer buf1, cbuf1, buf2, cbuf2;
   VkDeviceMemory mem1, cmem1, mem2, cmem2;

   dx7_buffer(t, sz1,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf1, &mem1);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf1, &cmem1);
   dx7_buffer(t, sz2,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf2, &mem2);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf2, &cmem2);

   VkDeviceSize off = 0;
   VkDeviceSize coff = 0;

   xfb_begin(t);

   /* Pass 1: capture N points, write counter */
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe1);
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf1, &off, &sz1);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, n, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf1, &coff);
   xfb_rp_end(t);

   /* Barrier on cbuf1 for indirect draw */
   VkBufferMemoryBarrier bar = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
      .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = cbuf1,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
   };
   vkCmdPipelineBarrier(t->cmd, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, NULL, 1,
                        &bar, 0, NULL);

   /* Pass 2: draw indirect byte count using cbuf1 */
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf2, &off, &sz2);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDrawIndirectByteCountEXT(t->cmd, 1, 0, cbuf1, 0, 0, 16);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf2, &coff);
   xfb_rp_end(t);

   xfb_end_and_wait(t);

   uint32_t *c2 = NULL;
   CK(vkMapMemory(t->dev, cmem2, 0, csz, 0, (void **)&c2), "MapC2");
   uint32_t cnt2 = *c2;
   vkUnmapMemory(t->dev, cmem2);

   int bad = 0;
   if (cnt2 != n * 4) {
      printf("pass2 counter mismatch: got %u want %u\n", cnt2, n * 4);
      bad++;
   }

   uint32_t *indices = NULL;
   CK(vkMapMemory(t->dev, mem2, 0, sz2, 0, (void **)&indices), "MapIndices");
   for (uint32_t i = 0; i < n; i++) {
      if (indices[i] != i) {
         printf("index mismatch at %u: got %u want %u\n", i, indices[i], i);
         bad++;
      }
   }
   vkUnmapMemory(t->dev, mem2);

   printf("CASE draw_indirect_byte_count count=%u mismatch=%d %s\n", cnt2 / 4,
          bad, bad ? "FAIL" : "PASS");

   vkDestroyPipeline(t->dev, pipe1, NULL);
   vkDestroyPipeline(t->dev, pipe2, NULL);
   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf1, NULL);
      vkDestroyBuffer(t->dev, cbuf1, NULL);
      vkDestroyBuffer(t->dev, buf2, NULL);
      vkDestroyBuffer(t->dev, cbuf2, NULL);
      vkFreeMemory(t->dev, mem1, NULL);
      vkFreeMemory(t->dev, cmem1, NULL);
      vkFreeMemory(t->dev, mem2, NULL);
      vkFreeMemory(t->dev, cmem2, NULL);
   }
   return bad ? 1 : 0;
}

/* 7. queries: stream query on index 1 with GS, plus CmdCopyQueryPoolResults */
static int
test_queries(struct dx7 *t)
{
   if (!t->feats.geometryShader) {
      printf("SKIP queries: geometryShader not exposed\n");
      return 0;
   }
   if (!xfb_features.geometryStreams) {
      printf("SKIP queries: geometryStreams not exposed\n");
      return 0;
   }
   if (!xfb_props.transformFeedbackQueries) {
      printf("SKIP queries: transformFeedbackQueries not exposed\n");
      return 0;
   }

   struct xfb_pipe_desc d = {
      .vs = vs_mode1,
      .vs_size = sizeof(vs_mode1),
      .gs = gs_xfb,
      .gs_size = sizeof(gs_xfb),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   const VkDeviceSize sz = 256;
   const VkDeviceSize cp_sz = sizeof(uint64_t) * 3;
   VkBuffer bufs[2], copy_buf;
   VkDeviceMemory mems[2], copy_mem;

   for (int i = 0; i < 2; i++) {
      dx7_buffer(t, sz,
                 VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 NULL, &bufs[i], &mems[i]);
   }
   dx7_buffer(t, cp_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT, NULL, &copy_buf,
              &copy_mem);

   VkQueryPool pool;
   VkQueryPoolCreateInfo qpci = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
      .queryCount = 1,
   };
   CK(vkCreateQueryPool(t->dev, &qpci, NULL, &pool), "CreateQueryPool");

   xfb_begin(t);
   vkCmdResetQueryPool(t->cmd, pool, 0, 1);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize offs[2] = {0, 0};
   VkDeviceSize szs[2] = {sz, sz};
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 2, bufs, offs, szs);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   /* Query index 1 for stream 1 */
   vkCmdBeginQueryIndexedEXT(t->cmd, pool, 0, 0, 1);
   /* Draw 3 points: stream 1 emits 3 points (primitives) */
   vkCmdDraw(t->cmd, 3, 1, 0, 0);
   vkCmdEndQueryIndexedEXT(t->cmd, pool, 0, 1);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);

   /* Copy query pool results into buffer with availability */
   vkCmdCopyQueryPoolResults(t->cmd, pool, 0, 1, copy_buf, 0, sizeof(uint64_t),
                             VK_QUERY_RESULT_64_BIT |
                                VK_QUERY_RESULT_WAIT_BIT |
                                VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
   xfb_end_and_wait(t);

   uint64_t host_res[3] = {0, 0, 0};
   CK(vkGetQueryPoolResults(t->dev, pool, 0, 1, sizeof(host_res), host_res,
                            sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT |
                               VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
      "GetQueryPoolResults");

   uint64_t *copy_res = NULL;
   CK(vkMapMemory(t->dev, copy_mem, 0, cp_sz, 0, (void **)&copy_res),
      "MapCopy");

   int bad = 0;
   if (host_res[0] != 3) {
      printf("host written mismatch: got %llu want 3\n",
             (unsigned long long)host_res[0]);
      bad++;
   }
   if (host_res[1] != 3) {
      printf("host needed mismatch: got %llu want 3\n",
             (unsigned long long)host_res[1]);
      bad++;
   }
   if (!host_res[2]) {
      printf("host availability is zero\n");
      bad++;
   }
   if (copy_res[0] != host_res[0]) {
      printf("copy vs host written mismatch: copy=%llu host=%llu\n",
             (unsigned long long)copy_res[0],
             (unsigned long long)host_res[0]);
      bad++;
   }
   if (copy_res[1] != host_res[1]) {
      printf("copy vs host needed mismatch: copy=%llu host=%llu\n",
             (unsigned long long)copy_res[1],
             (unsigned long long)host_res[1]);
      bad++;
   }
   if (!copy_res[2]) {
      printf("copy availability is zero\n");
      bad++;
   }

   vkUnmapMemory(t->dev, copy_mem);

   printf("CASE queries host_written=%llu copy_written=%llu avail=%llu mismatch=%d %s\n",
          (unsigned long long)host_res[0], (unsigned long long)copy_res[0],
          (unsigned long long)host_res[2], bad, bad ? "FAIL" : "PASS");

   vkDestroyQueryPool(t->dev, pool, NULL);
   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      for (int i = 0; i < 2; i++) {
         vkDestroyBuffer(t->dev, bufs[i], NULL);
         vkFreeMemory(t->dev, mems[i], NULL);
      }
      vkDestroyBuffer(t->dev, copy_buf, NULL);
      vkFreeMemory(t->dev, copy_mem, NULL);
   }
   return bad ? 1 : 0;
}

/* 8. vs_nopos: VS output without gl_Position write */
static int
test_vs_nopos(struct dx7 *t)
{
   int fails = 0;
   const VkDeviceSize sz = 256;
   VkBuffer buf;
   VkDeviceMemory mem;
   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);

   struct xfb_pipe_desc d = {
      .vs = vs_mode4,
      .vs_size = sizeof(vs_mode4),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 3, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   int bad = 0;
   for (int i = 0; i < 3; i++) {
      const float *v = (const float *)(ptr + i * 16);
      float want[4] = {(float)i * 1.0f, 7.0f, -89.0f, 3.5f};
      for (int j = 0; j < 4; j++) {
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad++;
      }
   }
   vkUnmapMemory(t->dev, mem);
   printf("CASE vs_nopos mismatch=%d %s\n", bad, bad ? "FAIL" : "PASS");
   if (bad)
      fails++;
   vkDestroyPipeline(t->dev, pipe, NULL);

   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
   }
   return fails;
}

/* 9. gs_nopos: GS output without gl_Position write */
static int
test_gs_nopos(struct dx7 *t)
{
   if (!t->feats.geometryShader) {
      printf("SKIP gs_nopos: geometryShader not exposed\n");
      return 0;
   }

   int fails = 0;
   const VkDeviceSize sz = 256;
   VkBuffer buf;
   VkDeviceMemory mem;
   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);

   struct xfb_pipe_desc d = {
      .vs = vs_mode5,
      .vs_size = sizeof(vs_mode5),
      .gs = gs_nopos,
      .gs_size = sizeof(gs_nopos),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 2, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   int bad = 0;
   for (int i = 0; i < 2; i++) {
      const float *v = (const float *)(ptr + i * 16);
      float want[4] = {(float)i * 1.0f, -89.0f, 30.0f, 1.0f};
      for (int j = 0; j < 4; j++) {
         if (fabsf(v[j] - want[j]) > 1e-4f)
            bad++;
      }
   }
   vkUnmapMemory(t->dev, mem);
   printf("CASE gs_nopos mismatch=%d %s\n", bad, bad ? "FAIL" : "PASS");
   if (bad)
      fails++;
   vkDestroyPipeline(t->dev, pipe, NULL);

   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
   }
   return fails;
}

/* 10. vs_adjacency: line and triangle adjacency topologies */
static int
test_vs_adjacency(struct dx7 *t)
{
   int fails = 0;
   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;
   VkBuffer buf, cbuf;
   VkDeviceMemory mem, cmem;
   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf, &cmem);

   static const struct {
      const char *name;
      VkPrimitiveTopology topology;
      uint32_t draw_count;
      uint32_t want_verts;
      int want_indices[6];
   } subs[] = {
      {"vs_adj_line_list", VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, 8, 4, {1, 2, 5, 6}},
      {"vs_adj_line_strip", VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, 5, 4, {1, 2, 2, 3}},
      {"vs_adj_tri_list", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, 12, 6, {0, 2, 4, 6, 8, 10}},
      {"vs_adj_tri_strip", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, 8, 6, {0, 2, 4, 2, 6, 4}},
   };

   for (int s = 0; s < 4; s++) {
      void *p = NULL;
      CK(vkMapMemory(t->dev, mem, 0, sz, 0, &p), "MapZero");
      memset(p, 0, sz);
      vkUnmapMemory(t->dev, mem);

      CK(vkMapMemory(t->dev, cmem, 0, csz, 0, &p), "MapCZero");
      memset(p, 0, csz);
      vkUnmapMemory(t->dev, cmem);

      struct xfb_pipe_desc d = {
         .vs = vs_mode2,
         .vs_size = sizeof(vs_mode2),
         .topology = subs[s].topology,
      };
      VkPipeline pipe = xfb_pipeline(t, &d);

      xfb_begin(t);
      xfb_rp_begin(t);
      vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      VkDeviceSize off = 0;
      vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
      vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
      vkCmdDraw(t->cmd, subs[s].draw_count, 1, 0, 0);
      VkDeviceSize coff = 0;
      vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
      xfb_rp_end(t);
      xfb_end_and_wait(t);

      uint32_t *c = NULL;
      CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
      uint32_t cnt = *c;
      vkUnmapMemory(t->dev, cmem);

      int bad = 0;
      uint32_t want_bytes = subs[s].want_verts * 16;
      if (cnt != want_bytes)
         bad++;

      uint8_t *ptr = NULL;
      CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
      for (uint32_t i = 0; i < subs[s].want_verts; i++) {
         const float *v = (const float *)(ptr + i * 16);
         int idx = subs[s].want_indices[i];
         float want[4] = {(float)idx, (float)idx + 1.0f, (float)idx + 2.0f, 1.0f};
         for (int j = 0; j < 4; j++) {
            if (fabsf(v[j] - want[j]) > 1e-4f)
               bad++;
         }
      }
      vkUnmapMemory(t->dev, mem);

      printf("CASE %s mismatch=%d %s\n", subs[s].name, bad,
             bad ? "FAIL" : "PASS");
      if (bad)
         fails++;

      vkDestroyPipeline(t->dev, pipe, NULL);
   }

   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkDestroyBuffer(t->dev, cbuf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
      vkFreeMemory(t->dev, cmem, NULL);
   }
   return fails;
}

/* 11. vs_clip_cull_capture: capture ClipDistance and CullDistance builtins */
static int
test_vs_clip_cull_capture(struct dx7 *t)
{
   if (!t->feats.shaderClipDistance || !t->feats.shaderCullDistance) {
      printf("SKIP vs_clip_cull_capture: clip/cull distance not exposed\n");
      return 0;
   }

   int fails = 0;
   const VkDeviceSize sz = 256;
   VkBuffer buf;
   VkDeviceMemory mem;
   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);

   struct xfb_pipe_desc d = {
      .vs = vs_mode6,
      .vs_size = sizeof(vs_mode6),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   xfb_begin(t);
   xfb_rp_begin(t);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkDeviceSize off = 0;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   vkCmdDraw(t->cmd, 4, 1, 0, 0);
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   xfb_rp_end(t);
   xfb_end_and_wait(t);

   uint8_t *ptr = NULL;
   CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "Map");
   int bad = 0;
   for (int i = 0; i < 4; i++) {
      const float *v = (const float *)(ptr + i * 8);
      float want_clip = 0.25f * (float)i - 0.5f;
      float want_cull = -1.5f + (float)i;
      if (fabsf(v[0] - want_clip) > 1e-4f)
         bad++;
      if (fabsf(v[1] - want_cull) > 1e-4f)
         bad++;
   }
   vkUnmapMemory(t->dev, mem);
   printf("CASE vs_clip_cull_capture mismatch=%d %s\n", bad,
          bad ? "FAIL" : "PASS");
   if (bad)
      fails++;
   vkDestroyPipeline(t->dev, pipe, NULL);

   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
   }
   return fails;
}

/* 12. cond_render: conditional rendering with transform feedback */
static int
test_cond_render(struct dx7 *t)
{
   if (!has_cond_render_ext) {
      printf("SKIP cond_render: VK_EXT_conditional_rendering not exposed\n");
      return 0;
   }
   if (!cond_render_features.conditionalRendering) {
      printf("SKIP cond_render: conditionalRendering feature not exposed\n");
      return 0;
   }

   int fails = 0;
   struct xfb_pipe_desc d = {
      .vs = vs_mode2,
      .vs_size = sizeof(vs_mode2),
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
   };
   VkPipeline pipe = xfb_pipeline(t, &d);

   const VkDeviceSize sz = 256;
   const VkDeviceSize csz = 64;
   const VkDeviceSize psz = 4;
   VkBuffer buf, cbuf, pbuf;
   VkDeviceMemory mem, cmem, pmem;

   dx7_buffer(t, sz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &buf, &mem);
   dx7_buffer(t, csz,
              VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              NULL, &cbuf, &cmem);
   dx7_buffer(t, psz,
              VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
              NULL, &pbuf, &pmem);

   for (int run = 0; run < 2; run++) {
      uint32_t pred_val = (run == 0) ? 0 : 1;
      int num_verts = (run == 0) ? 2 : 5;
      uint32_t want_cnt = (run == 0) ? 32 : 80;
      const char *case_name = (run == 0) ? "cond_render_p0" : "cond_render_p1";
      int want_verts[5] = {0};
      if (run == 0) {
         want_verts[0] = 5;
         want_verts[1] = 6;
      } else {
         want_verts[0] = 0;
         want_verts[1] = 1;
         want_verts[2] = 2;
         want_verts[3] = 5;
         want_verts[4] = 6;
      }

      /* Zero XFB buffer */
      uint8_t *ptr = NULL;
      CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "MapBuf");
      memset(ptr, 0, sz);
      vkUnmapMemory(t->dev, mem);

      /* Initial counter = 0 via host write */
      uint32_t *c = NULL;
      CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
      *c = 0;
      vkUnmapMemory(t->dev, cmem);

      /* Write predicate */
      uint32_t *p = NULL;
      CK(vkMapMemory(t->dev, pmem, 0, psz, 0, (void **)&p), "MapP");
      *p = pred_val;
      vkUnmapMemory(t->dev, pmem);

      VkDeviceSize off = 0;
      VkDeviceSize coff = 0;

      xfb_begin(t);
      xfb_rp_begin(t);
      vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &buf, &off, &sz);
      vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);

      VkConditionalRenderingBeginInfoEXT cr_info = {
         .sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
         .pNext = NULL,
         .buffer = pbuf,
         .offset = 0,
         .flags = 0,
      };
      vkCmdBeginConditionalRenderingEXT(t->cmd, &cr_info);
      vkCmdDraw(t->cmd, 3, 1, 0, 0);
      vkCmdEndConditionalRenderingEXT(t->cmd);

      vkCmdDraw(t->cmd, 2, 1, 5, 0);
      vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cbuf, &coff);
      xfb_rp_end(t);
      xfb_end_and_wait(t);

      c = NULL;
      CK(vkMapMemory(t->dev, cmem, 0, csz, 0, (void **)&c), "MapC");
      uint32_t cnt = *c;
      vkUnmapMemory(t->dev, cmem);

      ptr = NULL;
      CK(vkMapMemory(t->dev, mem, 0, sz, 0, (void **)&ptr), "MapBuf");

      int bad = 0;
      if (cnt != want_cnt) {
         printf("%s counter mismatch: got %u want %u\n", case_name, cnt, want_cnt);
         bad++;
      }
      for (int i = 0; i < num_verts; i++) {
         int idx = want_verts[i];
         const float *v = (const float *)(ptr + i * 16);
         float want[4] = {(float)idx, (float)idx + 1.0f, (float)idx + 2.0f, 1.0f};
         for (int j = 0; j < 4; j++) {
            if (fabsf(v[j] - want[j]) > 1e-4f)
               bad++;
         }
      }
      const uint32_t *u32 = (const uint32_t *)ptr;
      for (uint32_t i = (num_verts * 16) / 4; i < sz / 4; i++) {
         if (u32[i] != 0)
            bad++;
      }
      vkUnmapMemory(t->dev, mem);

      printf("CASE %s counter=%u mismatch=%d %s\n", case_name, cnt, bad,
             bad ? "FAIL" : "PASS");
      if (bad)
         fails++;
   }

   vkDestroyPipeline(t->dev, pipe, NULL);
   if (vkDestroyBuffer) {
      vkDestroyBuffer(t->dev, buf, NULL);
      vkDestroyBuffer(t->dev, cbuf, NULL);
      vkDestroyBuffer(t->dev, pbuf, NULL);
      vkFreeMemory(t->dev, mem, NULL);
      vkFreeMemory(t->dev, cmem, NULL);
      vkFreeMemory(t->dev, pmem, NULL);
   }
   return fails;
}

static void
device_hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   enabled_features.geometryShader = t->feats.geometryShader;
   enabled_features.tessellationShader = t->feats.tessellationShader;
   enabled_features.shaderClipDistance = t->feats.shaderClipDistance;
   enabled_features.shaderCullDistance = t->feats.shaderCullDistance;
   dci->pEnabledFeatures = &enabled_features;

   void *h = dlopen(icd_path, RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      exit(1);
   }
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   if (!gipa) {
      printf("FAIL dlsym gipa\n");
      exit(1);
   }

#define XFB_LOAD_INST(n)                                                       \
   vk##n = (PFN_vk##n)gipa(t->inst, "vk" #n);
   XFB_FUNCS(XFB_LOAD_INST)

   uint32_t ext_count = 0;
   CK(vkEnumerateDeviceExtensionProperties(t->phys, NULL, &ext_count, NULL),
      "EnumExtCount");
   VkExtensionProperties *exts =
      malloc(sizeof(VkExtensionProperties) * (ext_count ? ext_count : 1));
   if (!exts) {
      printf("FAIL oom exts\n");
      exit(1);
   }
   CK(vkEnumerateDeviceExtensionProperties(t->phys, NULL, &ext_count, exts),
      "EnumExts");
   for (uint32_t i = 0; i < ext_count; i++) {
      if (strcmp(exts[i].extensionName,
                 VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) == 0) {
         has_xfb_ext = 1;
      } else if (strcmp(exts[i].extensionName,
                        VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME) == 0) {
         has_cond_render_ext = 1;
      }
   }
   free(exts);

   if (!has_xfb_ext)
      return;

   xfb_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
   xfb_features.pNext = NULL;
   VkPhysicalDeviceFeatures2 f2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &xfb_features,
   };
   vkGetPhysicalDeviceFeatures2(t->phys, &f2);

   if (has_cond_render_ext) {
      cond_render_features.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
      cond_render_features.pNext = NULL;
      VkPhysicalDeviceFeatures2 f2_cr = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
         .pNext = &cond_render_features,
      };
      vkGetPhysicalDeviceFeatures2(t->phys, &f2_cr);
   }

   xfb_props.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
   xfb_props.pNext = NULL;
   VkPhysicalDeviceProperties2 p2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &xfb_props,
   };
   vkGetPhysicalDeviceProperties2(t->phys, &p2);

   static VkPhysicalDeviceTransformFeedbackFeaturesEXT enabled_xfb_features;
   enabled_xfb_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
   enabled_xfb_features.pNext = (void *)dci->pNext;
   enabled_xfb_features.transformFeedback = VK_TRUE;
   enabled_xfb_features.geometryStreams = xfb_features.geometryStreams;
   dci->pNext = &enabled_xfb_features;

   static VkPhysicalDeviceConditionalRenderingFeaturesEXT
      enabled_cond_render_features;
   if (has_cond_render_ext) {
      enabled_cond_render_features.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
      enabled_cond_render_features.pNext = (void *)dci->pNext;
      enabled_cond_render_features.conditionalRendering =
         cond_render_features.conditionalRendering;
      dci->pNext = &enabled_cond_render_features;
   }

   static const char *ext_names[2];
   uint32_t ext_names_count = 0;
   ext_names[ext_names_count++] = VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME;
   if (has_cond_render_ext)
      ext_names[ext_names_count++] = VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME;
   dci->enabledExtensionCount = ext_names_count;
   dci->ppEnabledExtensionNames = ext_names;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }
   icd_path = argv[1];
   dx7_device_hook = device_hook;

   struct dx7 t;
   dx7_init(&t, argv[1], NULL);

   if (!has_xfb_ext) {
      printf("FAIL VK_EXT_transform_feedback not exposed\n");
      return 1;
   }
   if (!xfb_features.transformFeedback) {
      printf("FAIL transformFeedback feature not exposed\n");
      return 1;
   }

   void *h = dlopen(icd_path, RTLD_NOW | RTLD_LOCAL);
   if (h) {
      PFN_vkGetInstanceProcAddr gipa =
         (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
      if (gipa) {
         PFN_vkGetDeviceProcAddr gdpa =
            (PFN_vkGetDeviceProcAddr)gipa(t.inst, "vkGetDeviceProcAddr");
         if (gdpa) {
#define XFB_LOAD_DEV(n)                                                        \
   if (!vk##n)                                                                 \
      vk##n = (PFN_vk##n)gdpa(t.dev, "vk" #n);
            XFB_FUNCS(XFB_LOAD_DEV)
         }
      }
   }

   printf("XFB features: transformFeedback=%d geometryStreams=%d\n",
          xfb_features.transformFeedback, xfb_features.geometryStreams);
   printf("XFB props: maxStreams=%u maxBuffers=%u maxBufferSize=%llu "
          "queries=%d draw=%d\n",
          xfb_props.maxTransformFeedbackStreams,
          xfb_props.maxTransformFeedbackBuffers,
          (unsigned long long)xfb_props.maxTransformFeedbackBufferSize,
          xfb_props.transformFeedbackQueries, xfb_props.transformFeedbackDraw);

   (void)dx7_pipeline;
   (void)dx7_run;
   (void)dx7_check;

   int fails = 0;
   fails += test_vs_capture(&t);
   fails += test_gs_streams(&t);
   fails += test_tes_capture(&t);
   fails += test_pause_resume(&t);
   fails += test_overflow(&t);
   fails += test_draw_indirect_byte_count(&t);
   fails += test_queries(&t);
   fails += test_vs_nopos(&t);
   fails += test_gs_nopos(&t);
   fails += test_vs_adjacency(&t);
   fails += test_vs_clip_cull_capture(&t);
   fails += test_cond_render(&t);

   printf("XFB_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}

