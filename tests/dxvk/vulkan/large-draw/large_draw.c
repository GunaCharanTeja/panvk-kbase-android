/* Large draws through the gpu_prerast paths (transform feedback, geometry and
 * tessellation shaders): every draw is far over the 65536 invocations one
 * prerast arena holds, so the driver has to chunk it.
 *
 * All cases capture the pre-raster output with transform feedback and check
 * every captured record, the byte counter and the transform feedback stream
 * query (and the primitives generated query when the device has it):
 *  points_direct / points_indirect       200000 points, firstVertex 7,
 *                                        firstInstance 3
 *  instanced_direct / instanced_indirect 4 vertices x 60000 instances
 *  strip_direct / strip_indirect         150001 vertex triangle strip
 *  restart_direct / restart_indirect     indexed triangle strip with primitive
 *                                        restart: 140k indices, a 70001 vertex
 *                                        strip, empty and 1-3 vertex strips
 *  gs_direct / gs_indirect               geometry shader, 200000 points, the
 *                                        GS primitive IDs must run 0..N-1
 *  tess_direct / tess_indirect           70000 isoline patches in point mode
 *  indirect_count                        two 150000 point draws, the count
 *                                        buffer lets only the first run
 *  multi_indirect                        two indirect draws in one call
 *
 * Output: PASS/FAIL per case, XFB_FAILS=n. usage: large_draw <icd>
 *
 * SPIR-V regen: sh build_spv.sh
 */
#include "../dx7_harness.h"
#include "large_draw_spv.h"
#include <math.h>

#define XFB_FUNCS(X)                                                           \
   X(EnumerateDeviceExtensionProperties)                                       \
   X(GetPhysicalDeviceFeatures2)                                               \
   X(QueueWaitIdle)                                                            \
   X(CmdBindTransformFeedbackBuffersEXT)                                       \
   X(CmdBeginTransformFeedbackEXT)                                             \
   X(CmdEndTransformFeedbackEXT)                                               \
   X(CmdBeginQueryIndexedEXT)                                                  \
   X(CmdEndQueryIndexedEXT)                                                    \
   X(CmdDrawIndirectCount)                                                     \
   X(CreateQueryPool)                                                          \
   X(CmdResetQueryPool)                                                        \
   X(GetQueryPoolResults)                                                      \
   X(DestroyQueryPool)                                                         \
   X(DestroyBuffer)                                                            \
   X(FreeMemory)

XFB_FUNCS(DX7_DECL)

static const char *icd_path;
static int has_xfb_ext, has_pg_ext;
static VkPhysicalDeviceFeatures enabled_features;
static VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb_features;
static VkPhysicalDeviceVulkan12Features v12_features;
static VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT pg_features;
static int pg_ok; /* usable together with rasterizer discard */

#define NPTS 200000u
#define FV 7u
#define FI 3u
#define VPI 4u
#define NINST 60000u
#define NSTRIP 150001u
#define NPATCH 70000u

static void
device_hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   enabled_features.geometryShader = t->feats.geometryShader;
   enabled_features.tessellationShader = t->feats.tessellationShader;
   enabled_features.multiDrawIndirect = t->feats.multiDrawIndirect;
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
#define XFB_LOAD_INST(n) vk##n = (PFN_vk##n)gipa(t->inst, "vk" #n);
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
      if (!strcmp(exts[i].extensionName, VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME))
         has_xfb_ext = 1;
      else if (!strcmp(exts[i].extensionName,
                       VK_EXT_PRIMITIVES_GENERATED_QUERY_EXTENSION_NAME))
         has_pg_ext = 1;
   }
   free(exts);
   if (!has_xfb_ext)
      return;

   xfb_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
   v12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   v12_features.pNext = &xfb_features;
   pg_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT;
   xfb_features.pNext = has_pg_ext ? (void *)&pg_features : NULL;
   VkPhysicalDeviceFeatures2 f2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &v12_features,
   };
   vkGetPhysicalDeviceFeatures2(t->phys, &f2);

   static VkPhysicalDeviceTransformFeedbackFeaturesEXT en_xfb;
   en_xfb.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
   en_xfb.pNext = (void *)dci->pNext;
   en_xfb.transformFeedback = VK_TRUE;
   dci->pNext = &en_xfb;

   static VkPhysicalDeviceVulkan12Features en_v12;
   en_v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   en_v12.pNext = (void *)dci->pNext;
   en_v12.drawIndirectCount = v12_features.drawIndirectCount;
   dci->pNext = &en_v12;

   static VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT en_pg;
   static const char *ext_names[2];
   uint32_t n = 0;
   ext_names[n++] = VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME;
   if (has_pg_ext && pg_features.primitivesGeneratedQuery &&
       pg_features.primitivesGeneratedQueryWithRasterizerDiscard) {
      en_pg.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT;
      en_pg.pNext = (void *)dci->pNext;
      en_pg.primitivesGeneratedQuery = VK_TRUE;
      en_pg.primitivesGeneratedQueryWithRasterizerDiscard = VK_TRUE;
      dci->pNext = &en_pg;
      ext_names[n++] = VK_EXT_PRIMITIVES_GENERATED_QUERY_EXTENSION_NAME;
      pg_ok = 1;
   }
   dci->enabledExtensionCount = n;
   dci->ppEnabledExtensionNames = ext_names;
}

struct pipe_desc {
   const uint32_t *vs;
   size_t vs_size;
   const uint32_t *gs;
   size_t gs_size;
   const uint32_t *tcs;
   size_t tcs_size;
   const uint32_t *tes;
   size_t tes_size;
   VkPrimitiveTopology topology;
   VkBool32 restart;
};

static VkPipeline
make_pipe(struct dx7 *t, const struct pipe_desc *d)
{
   VkShaderModule vs = dx7_module(t, d->vs, d->vs_size);
   VkShaderModule fs = dx7_module(t, fs_white, sizeof(fs_white));
   VkShaderModule gs = d->gs ? dx7_module(t, d->gs, d->gs_size) : VK_NULL_HANDLE;
   VkShaderModule tcs = d->tcs ? dx7_module(t, d->tcs, d->tcs_size) : VK_NULL_HANDLE;
   VkShaderModule tes = d->tes ? dx7_module(t, d->tes, d->tes_size) : VK_NULL_HANDLE;

   VkPipelineShaderStageCreateInfo st[5];
   uint32_t n = 0;
#define STAGE(bit, mod)                                                        \
   st[n++] = (VkPipelineShaderStageCreateInfo){                                \
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,            \
      .stage = bit, .module = mod, .pName = "main"}
   STAGE(VK_SHADER_STAGE_VERTEX_BIT, vs);
   if (tcs)
      STAGE(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tcs);
   if (tes)
      STAGE(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tes);
   if (gs)
      STAGE(VK_SHADER_STAGE_GEOMETRY_BIT, gs);
   STAGE(VK_SHADER_STAGE_FRAGMENT_BIT, fs);
#undef STAGE

   VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = d->topology, .primitiveRestartEnable = d->restart};
   VkPipelineTessellationStateCreateInfo ts = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .patchControlPoints = 1};
   VkViewport vp = {0.0f, 0.0f, (float)RT_W, (float)RT_H, 0.0f, 1.0f};
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   VkPipelineViewportStateCreateInfo vps = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .rasterizerDiscardEnable = VK_TRUE, .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkGraphicsPipelineCreateInfo gpci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = n, .pStages = st, .pVertexInputState = &vi,
      .pInputAssemblyState = &ia, .pTessellationState = tcs ? &ts : NULL,
      .pViewportState = &vps, .pRasterizationState = &rs,
      .pMultisampleState = &ms, .layout = t->layout, .renderPass = t->rp};
   VkPipeline pipe;
   CK(vkCreateGraphicsPipelines(t->dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe),
      "CreatePipe");
   return pipe;
}

/* Vk[Indexed]DrawIndirectCommand, as the direct draw arguments too. */
struct cmd {
   uint32_t count, instances, first, offset, first_instance;
};

struct job {
   VkPipeline pipe;
   int indexed;
   /* 0 direct, 1 indirect, 2 indirect count, 3 multi indirect */
   int mode;
   uint32_t draws;
   uint32_t count_value;
   struct cmd cmd[2];
   const uint32_t *indices;
   uint32_t index_count;
   VkDeviceSize xfb_bytes;
};

struct result {
   const uint32_t *words;
   uint32_t counter;
   uint64_t written, needed;
   uint64_t generated;
   int has_generated;
};

static void *
map_buf(struct dx7 *t, VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer *b,
        VkDeviceMemory *m)
{
   void *p;

   dx7_buffer(t, sz, usage, NULL, b, m);
   CK(vkMapMemory(t->dev, *m, 0, sz, 0, &p), "Map");
   return p;
}

static void
begin(struct dx7 *t)
{
   CK(vkResetFences(t->dev, 1, &t->fence), "ResetFence");
   CK(vkResetCommandBuffer(t->cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(t->cmd, &bbi), "BeginCmd");
}

static int
end_and_wait(struct dx7 *t)
{
   VkMemoryBarrier mb = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
                       VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
   vkCmdPipelineBarrier(t->cmd, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
   CK(vkEndCommandBuffer(t->cmd), "EndCmd");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1, .pCommandBuffers = &t->cmd};
   VkResult sr = vkQueueSubmit(t->queue, 1, &si, t->fence);
   if (sr != VK_SUCCESS) {
      printf("FAIL QueueSubmit r=%d\n", (int)sr);
      return 1;
   }
   VkResult r = vkWaitForFences(t->dev, 1, &t->fence, VK_TRUE,
                                60ull * 1000000000ull);
   if (r != VK_SUCCESS) {
      printf("FAIL wait for fence r=%d\n", (int)r);
      return 1;
   }
   return 0;
}

static VkQueryPool
make_pool(struct dx7 *t, VkQueryType type)
{
   VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                               .queryType = type, .queryCount = 1};
   VkQueryPool q;
   CK(vkCreateQueryPool(t->dev, &qi, NULL, &q), "CreateQueryPool");
   return q;
}

/* Runs the job into a zeroed capture buffer and fills in the result. */
static int
run_job(struct dx7 *t, const struct job *j, struct result *r)
{
   VkBuffer xb, cb, ib = VK_NULL_HANDLE, db = VK_NULL_HANDLE, nb = VK_NULL_HANDLE;
   VkDeviceMemory xm, cm, im, dm, nm;
   const VkBufferUsageFlags host = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

   uint32_t *xw = map_buf(t, j->xfb_bytes,
                          VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | host,
                          &xb, &xm);
   memset(xw, 0, j->xfb_bytes);
   uint32_t *cw = map_buf(
      t, 64, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | host,
      &cb, &cm);
   memset(cw, 0, 64);
   if (j->indexed) {
      uint32_t *iw = map_buf(t, j->index_count * 4,
                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT | host, &ib, &im);
      memcpy(iw, j->indices, j->index_count * 4);
   }
   if (j->mode) {
      uint32_t *dw = map_buf(t, 256, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | host,
                             &db, &dm);
      memset(dw, 0, 256);
      for (uint32_t i = 0; i < 2; i++) {
         const struct cmd *c = &j->cmd[i];
         uint32_t *w = dw + i * 8;
         /* stride 32 bytes between draws */
         w[0] = c->count;
         w[1] = c->instances;
         w[2] = c->first;
         if (j->indexed) {
            w[3] = c->offset;
            w[4] = c->first_instance;
         } else {
            w[3] = c->first_instance;
         }
      }
      uint32_t *nw = map_buf(t, 16, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | host,
                             &nb, &nm);
      nw[0] = j->count_value;
   }

   VkQueryPool xq = make_pool(t, VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT);
   VkQueryPool pq = pg_ok ? make_pool(t, VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT)
                          : VK_NULL_HANDLE;

   begin(t);
   vkCmdResetQueryPool(t->cmd, xq, 0, 1);
   if (pq)
      vkCmdResetQueryPool(t->cmd, pq, 0, 1);
   VkClearValue clear = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = t->rp,
      .framebuffer = t->fb, .renderArea = {{0, 0}, {RT_W, RT_H}},
      .clearValueCount = 1, .pClearValues = &clear};
   vkCmdBeginRenderPass(t->cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, j->pipe);
   VkDeviceSize off = 0, size = j->xfb_bytes;
   vkCmdBindTransformFeedbackBuffersEXT(t->cmd, 0, 1, &xb, &off, &size);
   if (j->indexed)
      vkCmdBindIndexBuffer(t->cmd, ib, 0, VK_INDEX_TYPE_UINT32);
   vkCmdBeginQueryIndexedEXT(t->cmd, xq, 0, 0, 0);
   if (pq)
      vkCmdBeginQueryIndexedEXT(t->cmd, pq, 0, 0, 0);
   vkCmdBeginTransformFeedbackEXT(t->cmd, 0, 0, NULL, NULL);
   const struct cmd *c = &j->cmd[0];
   const uint32_t stride = 32;
   switch (j->mode) {
   case 0:
      if (j->indexed)
         vkCmdDrawIndexed(t->cmd, c->count, c->instances, c->first,
                          (int32_t)c->offset, c->first_instance);
      else
         vkCmdDraw(t->cmd, c->count, c->instances, c->first, c->first_instance);
      break;
   case 1:
   case 3:
      if (j->indexed)
         vkCmdDrawIndexedIndirect(t->cmd, db, 0, j->draws, stride);
      else
         vkCmdDrawIndirect(t->cmd, db, 0, j->draws, stride);
      break;
   case 2:
      vkCmdDrawIndirectCount(t->cmd, db, 0, nb, 0, j->draws, stride);
      break;
   }
   VkDeviceSize coff = 0;
   vkCmdEndTransformFeedbackEXT(t->cmd, 0, 1, &cb, &coff);
   if (pq)
      vkCmdEndQueryIndexedEXT(t->cmd, pq, 0, 0);
   vkCmdEndQueryIndexedEXT(t->cmd, xq, 0, 0);
   vkCmdEndRenderPass(t->cmd);
   if (end_and_wait(t)) {
      /* The memory stays mapped: show how far the GPU got. */
      uint32_t done = 0;

      for (VkDeviceSize i = 0; i < j->xfb_bytes / 16; i++)
         if (xw[i * 4 + 3])
            done = i + 1;
      printf("  hang: last non-zero record %u of %llu\n", done,
             (unsigned long long)(j->xfb_bytes / 16));
      return 1;
   }

   uint64_t q[2] = {0, 0};
   CK(vkGetQueryPoolResults(t->dev, xq, 0, 1, sizeof(q), q, sizeof(q),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
      "GetQueryPoolResults");
   r->written = q[0];
   r->needed = q[1];
   r->has_generated = pq != VK_NULL_HANDLE;
   if (pq) {
      uint64_t g = 0;
      CK(vkGetQueryPoolResults(t->dev, pq, 0, 1, sizeof(g), &g, sizeof(g),
                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
         "GetQueryPoolResults PG");
      r->generated = g;
      vkDestroyQueryPool(t->dev, pq, NULL);
   }
   vkDestroyQueryPool(t->dev, xq, NULL);
   r->words = xw;
   r->counter = cw[0];
   return 0;
}

/* LARGE_DRAW_ONLY=substr runs only the cases whose name contains it. */
static int
want(const char *name)
{
   const char *only = getenv("LARGE_DRAW_ONLY");

   return !only || strstr(name, only);
}

static float
wf(const uint32_t *w, uint32_t rec, uint32_t comp)
{
   float f;

   memcpy(&f, &w[rec * 4 + comp], 4);
   return f;
}

static int
report(const char *name, int bad, const struct result *r, uint64_t bytes,
       uint64_t prims, int check_generated)
{
   if (r->counter != bytes) {
      printf("  counter %u, want %llu\n", r->counter, (unsigned long long)bytes);
      bad++;
   }
   if (r->written != prims || r->needed != prims) {
      printf("  xfb query written=%llu needed=%llu, want %llu\n",
             (unsigned long long)r->written, (unsigned long long)r->needed,
             (unsigned long long)prims);
      bad++;
   }
   if (check_generated && r->has_generated && r->generated != prims) {
      printf("  primitives generated %llu, want %llu\n",
             (unsigned long long)r->generated, (unsigned long long)prims);
      bad++;
   }
   printf("%s case %s bad=%d bytes=%u written=%llu generated=%lld\n",
          bad ? "FAIL" : "PASS", name, bad, r->counter,
          (unsigned long long)r->written,
          r->has_generated ? (long long)r->generated : -1ll);
   return bad ? 1 : 0;
}

/* Points: record i is vertex (i % vpi), instance (i / vpi). */
static int
check_points(const char *name, struct dx7 *t, const struct job *j, uint32_t n,
             uint32_t vpi, uint32_t first_vertex, uint32_t first_instance,
             uint32_t extra_zero_at)
{
   if (!want(name))
      return 0;
   struct result r;
   if (run_job(t, j, &r)) {
      printf("FAIL case %s did not complete\n", name);
      return 1;
   }
   int bad = 0;
   for (uint32_t i = 0; i < n; i++) {
      float x = first_vertex + i % vpi, y = first_instance + i / vpi;
      if (wf(r.words, i, 0) != x || wf(r.words, i, 1) != y ||
          wf(r.words, i, 3) != 1.0f) {
         if (bad < 4)
            printf("  rec %u: %g %g %g %g, want %g %g 0 1\n", i,
                   wf(r.words, i, 0), wf(r.words, i, 1), wf(r.words, i, 2),
                   wf(r.words, i, 3), x, y);
         bad++;
      }
   }
   /* Nothing may be written after the last record. */
   for (uint32_t i = extra_zero_at; i < extra_zero_at + 64 &&
                                    (i + 1) * 16 <= j->xfb_bytes; i++)
      if (wf(r.words, i, 3) != 0.0f)
         bad++;
   return report(name, bad, &r, (uint64_t)n * 16, n, 1);
}

static int
check_gs(const char *name, struct dx7 *t, const struct job *j, uint32_t n,
         uint32_t first_vertex, uint32_t first_instance)
{
   if (!want(name))
      return 0;
   struct result r;
   if (run_job(t, j, &r)) {
      printf("FAIL case %s did not complete\n", name);
      return 1;
   }
   int bad = 0;
   for (uint32_t i = 0; i < n; i++) {
      if (wf(r.words, i, 0) != (float)(first_vertex + i) ||
          wf(r.words, i, 1) != (float)first_instance ||
          wf(r.words, i, 2) != (float)i) {
         if (bad < 4)
            printf("  rec %u: %g %g %g, want %u %u %u\n", i, wf(r.words, i, 0),
                   wf(r.words, i, 1), wf(r.words, i, 2), first_vertex + i,
                   first_instance, i);
         bad++;
      }
   }
   return report(name, bad, &r, (uint64_t)n * 16, n, 1);
}

static int
check_tess(const char *name, struct dx7 *t, const struct job *j, uint32_t patches,
           uint32_t first_vertex)
{
   if (!want(name))
      return 0;
   struct result r;
   if (run_job(t, j, &r)) {
      printf("FAIL case %s did not complete\n", name);
      return 1;
   }
   uint8_t *cnt = calloc(patches, 1);
   float *sum = calloc(patches, sizeof(float));
   int bad = 0;

   for (uint32_t i = 0; i < patches * 2; i++) {
      float p = wf(r.words, i, 2);
      if (!(p >= 0.0f && p < (float)patches) ||
          wf(r.words, i, 0) != first_vertex + p) {
         if (bad < 4)
            printf("  rec %u: %g %g %g\n", i, wf(r.words, i, 0),
                   wf(r.words, i, 1), wf(r.words, i, 2));
         bad++;
         continue;
      }
      cnt[(uint32_t)p]++;
      sum[(uint32_t)p] += wf(r.words, i, 1);
   }
   for (uint32_t p = 0; p < patches; p++) {
      if (cnt[p] != 2 || sum[p] != 1.0f) {
         if (bad < 4)
            printf("  patch %u: %u records, coord sum %g\n", p, cnt[p], sum[p]);
         bad++;
      }
   }
   free(cnt);
   free(sum);
   /* The tessellation primitive count is the isoline point count. */
   return report(name, bad, &r, (uint64_t)patches * 2 * 16, patches * 2, 0);
}

/* Canonical (sorted) triangle, so the strip winding does not matter. */
static int
check_strip_prims(struct result *r, const uint32_t *exp, uint32_t nprims,
                  const char *name)
{
   int bad = 0;

   for (uint32_t k = 0; k < nprims; k++) {
      uint32_t v[3];
      for (int c = 0; c < 3; c++)
         v[c] = (uint32_t)wf(r->words, k * 3 + c, 0);
      for (int a = 0; a < 2; a++)
         for (int b = 0; b < 2 - a; b++)
            if (v[b] > v[b + 1]) {
               uint32_t s = v[b];
               v[b] = v[b + 1];
               v[b + 1] = s;
            }
      if (v[0] != exp[k * 3] || v[1] != exp[k * 3 + 1] || v[2] != exp[k * 3 + 2]) {
         if (bad < 4)
            printf("  prim %u: %u %u %u, want %u %u %u\n", k, v[0], v[1], v[2],
                   exp[k * 3], exp[k * 3 + 1], exp[k * 3 + 2]);
         bad++;
      }
   }
   return bad;
}

static int
check_strip(const char *name, struct dx7 *t, const struct job *j,
            const uint32_t *exp, uint32_t nprims)
{
   if (!want(name))
      return 0;
   struct result r;
   if (run_job(t, j, &r)) {
      printf("FAIL case %s did not complete\n", name);
      return 1;
   }
   int bad = check_strip_prims(&r, exp, nprims, name);
   return report(name, bad, &r, (uint64_t)nprims * 3 * 16, nprims, 1);
}

static const uint32_t strip_lens[] = {1000, 70001, 777, 1500, 3, 0, 2,
                                      1,    45000, 998, 20000};
#define NLENS (sizeof(strip_lens) / sizeof(strip_lens[0]))
#define LEAD 3u /* ignored indices before firstIndex */
#define VOFF 5u

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
   if (!has_xfb_ext || !xfb_features.transformFeedback) {
      printf("FAIL VK_EXT_transform_feedback not exposed\n");
      return 1;
   }
   printf("FEATURES gs=%d tess=%d multiDrawIndirect=%d drawIndirectCount=%d "
          "primGenQuery=%d\n",
          t.feats.geometryShader, t.feats.tessellationShader,
          t.feats.multiDrawIndirect, v12_features.drawIndirectCount, pg_ok);

   void *h = dlopen(icd_path, RTLD_NOW | RTLD_LOCAL);
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkGetDeviceProcAddr gdpa =
      (PFN_vkGetDeviceProcAddr)gipa(t.inst, "vkGetDeviceProcAddr");
#define XFB_LOAD_DEV(n) vk##n = (PFN_vk##n)gdpa(t.dev, "vk" #n);
   XFB_FUNCS(XFB_LOAD_DEV)

   int fails = 0;

   struct pipe_desc pd = {.vs = vs_capture, .vs_size = sizeof(vs_capture),
                          .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST};
   VkPipeline p_points = make_pipe(&t, &pd);
   pd.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   VkPipeline p_strip = make_pipe(&t, &pd);
   pd.restart = VK_TRUE;
   VkPipeline p_restart = make_pipe(&t, &pd);
   VkPipeline p_gs = VK_NULL_HANDLE, p_tess = VK_NULL_HANDLE;
   if (t.feats.geometryShader) {
      struct pipe_desc g = {.vs = vs_feed, .vs_size = sizeof(vs_feed),
                            .gs = gs_capture, .gs_size = sizeof(gs_capture),
                            .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST};
      p_gs = make_pipe(&t, &g);
   }
   if (t.feats.tessellationShader) {
      struct pipe_desc g = {.vs = vs_feed, .vs_size = sizeof(vs_feed),
                            .tcs = tcs_levels, .tcs_size = sizeof(tcs_levels),
                            .tes = tes_capture, .tes_size = sizeof(tes_capture),
                            .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST};
      p_tess = make_pipe(&t, &g);
   }

   /* Points. */
   struct job j = {.pipe = p_points, .xfb_bytes = (NPTS + 64) * 16ull};
   j.cmd[0] = (struct cmd){NPTS, 1, FV, 0, FI};
   fails += check_points("points_direct", &t, &j, NPTS, NPTS, FV, FI, NPTS);
   j.mode = 1;
   j.draws = 1;
   fails += check_points("points_indirect", &t, &j, NPTS, NPTS, FV, FI, NPTS);

   /* Many small instances. */
   j.mode = 0;
   j.xfb_bytes = (VPI * NINST + 64) * 16ull;
   j.cmd[0] = (struct cmd){VPI, NINST, FV, 0, FI};
   fails += check_points("instanced_direct", &t, &j, VPI * NINST, VPI, FV, FI,
                         VPI * NINST);
   j.mode = 1;
   fails += check_points("instanced_indirect", &t, &j, VPI * NINST, VPI, FV, FI,
                         VPI * NINST);

   /* Triangle strip without restart: record k, vertices k..k+2. */
   uint32_t nprims = NSTRIP - 2;
   uint32_t *exp = malloc((size_t)nprims * 12);
   for (uint32_t k = 0; k < nprims; k++) {
      exp[k * 3] = FV + k;
      exp[k * 3 + 1] = FV + k + 1;
      exp[k * 3 + 2] = FV + k + 2;
   }
   struct job js = {.pipe = p_strip, .xfb_bytes = (uint64_t)nprims * 48 + 1024};
   js.cmd[0] = (struct cmd){NSTRIP, 1, FV, 0, 0};
   fails += check_strip("strip_direct", &t, &js, exp, nprims);
   js.mode = 1;
   js.draws = 1;
   fails += check_strip("strip_indirect", &t, &js, exp, nprims);
   free(exp);

   /* Indexed strip with primitive restart. */
   uint32_t nidx = LEAD, v = 100000, nexp = 0;
   for (uint32_t k = 0; k < NLENS; k++) {
      nidx += strip_lens[k] + 1;
      nexp += strip_lens[k] >= 3 ? strip_lens[k] - 2 : 0;
   }
   nidx--; /* no restart after the last strip */
   uint32_t *idx = malloc((size_t)nidx * 4);
   exp = malloc((size_t)nexp * 12);
   uint32_t pos = 0, e = 0;
   for (; pos < LEAD; pos++)
      idx[pos] = 999999;
   for (uint32_t k = 0; k < NLENS; k++) {
      uint32_t first = pos;
      for (uint32_t i = 0; i < strip_lens[k]; i++)
         idx[pos++] = v++;
      for (uint32_t i = 0; i + 2 < strip_lens[k]; i++) {
         exp[e++] = idx[first + i] + VOFF;
         exp[e++] = idx[first + i + 1] + VOFF;
         exp[e++] = idx[first + i + 2] + VOFF;
      }
      if (k + 1 < NLENS)
         idx[pos++] = 0xffffffffu;
   }
   if (pos != nidx || e != nexp * 3) {
      printf("FAIL test setup pos=%u nidx=%u\n", pos, nidx);
      return 1;
   }
   /* The strips' vertex ids are consecutive per strip, so sorting keeps the
    * expected triples in order. */
   for (uint32_t k = 0; k < nexp; k++) {
      uint32_t *w = &exp[k * 3];
      for (int a = 0; a < 2; a++)
         for (int b = 0; b < 2 - a; b++)
            if (w[b] > w[b + 1]) {
               uint32_t s = w[b];
               w[b] = w[b + 1];
               w[b + 1] = s;
            }
   }
   struct job jr = {.pipe = p_restart, .indexed = 1, .indices = idx,
                    .index_count = nidx, .xfb_bytes = (uint64_t)nexp * 48 + 1024};
   jr.cmd[0] = (struct cmd){nidx - LEAD, 1, LEAD, VOFF, 0};
   fails += check_strip("restart_direct", &t, &jr, exp, nexp);
   jr.mode = 1;
   jr.draws = 1;
   fails += check_strip("restart_indirect", &t, &jr, exp, nexp);
   free(idx);
   free(exp);

   /* Geometry shader. */
   if (p_gs) {
      struct job jg = {.pipe = p_gs, .xfb_bytes = (NPTS + 64) * 16ull};
      jg.cmd[0] = (struct cmd){NPTS, 1, FV, 0, FI};
      fails += check_gs("gs_direct", &t, &jg, NPTS, FV, FI);
      jg.mode = 1;
      jg.draws = 1;
      fails += check_gs("gs_indirect", &t, &jg, NPTS, FV, FI);
   } else {
      printf("SKIP gs cases: geometryShader not exposed\n");
   }

   /* Tessellation. */
   if (p_tess) {
      struct job jt = {.pipe = p_tess, .xfb_bytes = (NPATCH * 2 + 64) * 16ull};
      jt.cmd[0] = (struct cmd){NPATCH, 1, FV, 0, 0};
      fails += check_tess("tess_direct", &t, &jt, NPATCH, FV);
      jt.mode = 1;
      jt.draws = 1;
      fails += check_tess("tess_indirect", &t, &jt, NPATCH, FV);
   } else {
      printf("SKIP tess cases: tessellationShader not exposed\n");
   }

   /* Indirect count: the count buffer holds 1 of 2 draws. */
   if (v12_features.drawIndirectCount && want("indirect_count")) {
      struct job jc = {.pipe = p_points, .mode = 2, .draws = 2, .count_value = 1,
                       .xfb_bytes = (2 * 150000 + 64) * 16ull};
      jc.cmd[0] = (struct cmd){150000, 1, FV, 0, FI};
      jc.cmd[1] = (struct cmd){150000, 1, 500, 0, 9};
      struct result r;
      if (run_job(&t, &jc, &r)) {
         printf("FAIL case indirect_count did not complete\n");
         fails++;
      } else {
         int bad = 0;
         for (uint32_t i = 0; i < 150000; i++)
            if (wf(r.words, i, 0) != (float)(FV + i) ||
                wf(r.words, i, 1) != (float)FI)
               bad++;
         for (uint32_t i = 150000; i < 150064; i++)
            if (wf(r.words, i, 3) != 0.0f)
               bad++;
         fails += report("indirect_count", bad, &r, 150000ull * 16, 150000, 1);
      }
   } else {
      printf("SKIP indirect_count: drawIndirectCount not exposed\n");
   }

   /* Two indirect draws in one call. */
   if (t.feats.multiDrawIndirect && want("multi_indirect")) {
      struct job jm = {.pipe = p_points, .mode = 3, .draws = 2,
                       .xfb_bytes = (150000 + 90000 + 64) * 16ull};
      jm.cmd[0] = (struct cmd){150000, 1, FV, 0, FI};
      jm.cmd[1] = (struct cmd){90000, 1, 500, 0, 9};
      struct result r;
      if (run_job(&t, &jm, &r)) {
         printf("FAIL case multi_indirect did not complete\n");
         fails++;
      } else {
         int bad = 0;
         for (uint32_t i = 0; i < 150000; i++)
            if (wf(r.words, i, 0) != (float)(FV + i) ||
                wf(r.words, i, 1) != (float)FI)
               bad++;
         for (uint32_t i = 0; i < 90000; i++)
            if (wf(r.words, 150000 + i, 0) != (float)(500 + i) ||
                wf(r.words, 150000 + i, 1) != 9.0f)
               bad++;
         fails += report("multi_indirect", bad, &r, 240000ull * 16, 240000, 1);
      }
   } else {
      printf("SKIP multi_indirect: multiDrawIndirect not exposed\n");
   }

   printf("XFB_FAILS=%d\n", fails);
   printf("RESULT %s\n", fails ? "FAIL" : "PASS");
   return fails ? 1 : 0;
}
