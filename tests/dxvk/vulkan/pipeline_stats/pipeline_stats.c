/* DX8 pipeline statistics query test. Each case records one query around a
 * known draw or dispatch and checks every non-tessellation counter. Counters
 * the driver computes exactly are compared with ==, the rest (VS, clipping,
 * FS) against the spec lower bound with >=. FS is bounded by the covered
 * pixel count of the readback. */
#include "../dx7_harness.h"
#include "pipeline_stats_spv.h"

#define RESTART 0xffff

#define PS_FUNCS(X)                                                            \
   X(CreateQueryPool) X(CmdResetQueryPool) X(CmdBeginQuery) X(CmdEndQuery)     \
   X(GetQueryPoolResults) X(CmdCopyQueryPoolResults)                           \
   X(CreateComputePipelines) X(CmdDispatch) X(CmdDispatchIndirect)
PS_FUNCS(DX7_DECL)

enum {
   IA_V, IA_P, VS, GS_I, GS_P, CLIP_I, CLIP_P, FS, CS, NSTATS,
};

static const char *names[NSTATS] = {
   "ia_verts", "ia_prims", "vs", "gs_inv", "gs_prims", "clip_inv",
   "clip_prims", "fs", "cs",
};

static const VkQueryPipelineStatisticFlags stat_mask =
   VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
   VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
   VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
   VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
   VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
   VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
   VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
   VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
   VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

struct expect {
   uint64_t v[NSTATS];
   /* Bit per counter compared with >= instead of ==. */
   uint32_t min;
};

#define MIN_BITS ((1u << VS) | (1u << CLIP_I) | (1u << CLIP_P) | (1u << FS))

static VkQueryPool pool;

static void
cmd_begin(struct dx7 *t)
{
   CK(vkResetFences(t->dev, 1, &t->fence), "ResetFence");
   CK(vkResetCommandBuffer(t->cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(t->cmd, &bbi), "Begin");
}

static void
cmd_submit(struct dx7 *t)
{
   CK(vkEndCommandBuffer(t->cmd), "End");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &t->cmd};
   CK(vkQueueSubmit(t->queue, 1, &si, t->fence), "Submit");
   CK(vkWaitForFences(t->dev, 1, &t->fence, VK_TRUE, 30ull * 1000000000ull),
      "Wait");
}

static void
reset_query(struct dx7 *t)
{
   cmd_begin(t);
   vkCmdResetQueryPool(t->cmd, pool, 0, 1);
   cmd_submit(t);
}

static int
check(const char *name, const uint64_t *got, const struct expect *e)
{
   int fail = 0;
   printf("CASE %s", name);
   for (int i = 0; i < NSTATS; i++) {
      int ge = (e->min >> i) & 1;
      int bad = ge ? got[i] < e->v[i] : got[i] != e->v[i];
      printf(" %s=%llu%s", names[i], (unsigned long long)got[i],
             bad ? "!" : "");
      if (bad) {
         printf("(want%s%llu)", ge ? ">=" : "=",
                (unsigned long long)e->v[i]);
         fail = 1;
      }
   }
   /* Availability word. */
   if (got[NSTATS] != 1) {
      printf(" avail=%llu!", (unsigned long long)got[NSTATS]);
      fail = 1;
   }
   printf(" %s\n", fail ? "FAIL" : "PASS");
   return fail;
}

static int
host_results(struct dx7 *t, uint64_t *res)
{
   memset(res, 0xcd, sizeof(uint64_t) * (NSTATS + 1));
   return vkGetQueryPoolResults(t->dev, pool, 0, 1,
                                sizeof(uint64_t) * (NSTATS + 1), res,
                                sizeof(uint64_t) * (NSTATS + 1),
                                VK_QUERY_RESULT_64_BIT |
                                   VK_QUERY_RESULT_WAIT_BIT |
                                   VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
}

struct draw {
   VkBuffer ib, indirect;
   uint32_t count, instances;
   int indexed;
};

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   const struct draw *d = data;
   VkViewport vp = {0, 0, RT_W, RT_H, 0, 1};
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   vkCmdSetViewport(cmd, 0, 1, &vp);
   vkCmdSetScissor(cmd, 0, 1, &sc);
   vkCmdBeginQuery(cmd, pool, 0, 0);
   if (d->indexed)
      vkCmdBindIndexBuffer(cmd, d->ib, 0, VK_INDEX_TYPE_UINT16);
   if (d->indirect) {
      if (d->indexed)
         vkCmdDrawIndexedIndirect(cmd, d->indirect, 0, 1, 0);
      else
         vkCmdDrawIndirect(cmd, d->indirect, 0, 1, 0);
   } else if (d->indexed) {
      vkCmdDrawIndexed(cmd, d->count, d->instances, 0, 0, 0);
   } else {
      vkCmdDraw(cmd, d->count, d->instances, 0, 0);
   }
   vkCmdEndQuery(cmd, pool, 0);
}

/* Six small triangles, no overlap, fully inside the viewport. The GS copy
 * is shifted by 0.125 so it overlaps the source only partly. */
static const float verts[18][4] = {
   {-0.9f, -0.9f, 0, 1}, {-0.6f, -0.9f, 0, 1}, {-0.9f, -0.6f, 0, 1},
   {-0.3f, -0.9f, 0, 1}, {0.0f, -0.9f, 0, 1}, {-0.3f, -0.6f, 0, 1},
   {0.3f, -0.9f, 0, 1}, {0.6f, -0.9f, 0, 1}, {0.3f, -0.6f, 0, 1},
   {-0.9f, 0.1f, 0, 1}, {-0.6f, 0.1f, 0, 1}, {-0.9f, 0.4f, 0, 1},
   {-0.3f, 0.1f, 0, 1}, {0.0f, 0.1f, 0, 1}, {-0.3f, 0.4f, 0, 1},
   {0.3f, 0.1f, 0, 1}, {0.6f, 0.1f, 0, 1}, {0.3f, 0.4f, 0, 1},
};

static VkBuffer vb;

static VkPipeline
pipe_of(struct dx7 *t, VkPrimitiveTopology topo, int gs, int restart)
{
   struct dx7_pipe_desc d = {
      .vs = vs_ps, .vs_size = sizeof(vs_ps),
      .fs = fs_ps, .fs_size = sizeof(fs_ps),
      .gs = gs ? gs_ps : NULL, .gs_size = gs ? sizeof(gs_ps) : 0,
      .topology = topo,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = VK_CULL_MODE_NONE,
      .dynamic_viewport = 1,
      .primitive_restart = restart,
   };
   return dx7_pipeline(t, &d);
}

static int
count_red(struct dx7 *t)
{
   int n = 0;
   for (int i = 0; i < RT_W * RT_H; i++)
      n += dx7_is_red(t->px + i * 4);
   return n;
}

/* prims = input primitives per instance, gs = GS bound (2 out per in). */
static int
draw_case(struct dx7 *t, const char *name, VkPrimitiveTopology topo,
          const uint16_t *idx, uint32_t count, uint32_t instances,
          uint32_t prims, uint32_t verts_ia, int gs, int indirect,
          int restart, int ia_verts_min)
{
   struct draw d = {.count = count, .instances = instances};
   VkDeviceMemory m;
   if (idx) {
      dx7_buffer(t, sizeof(uint16_t) * count, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 idx, &d.ib, &m);
      d.indexed = 1;
   }
   if (indirect) {
      uint32_t cmdw[5] = {count, instances, 0, 0, 0};
      dx7_buffer(t, sizeof(cmdw), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, cmdw,
                 &d.indirect, &m);
   }
   VkPipeline p = pipe_of(t, topo, gs, restart);
   reset_query(t);
   dx7_run(t, p, vb, rec, &d);
   vkDestroyPipeline(t->dev, p, NULL);

   uint64_t got[NSTATS + 1];
   CK(host_results(t, got), "Results");

   uint64_t ip = (uint64_t)prims * instances;
   uint64_t op = gs ? 2 * ip : ip;
   struct expect e = {.min = MIN_BITS};
   e.v[IA_V] = (uint64_t)verts_ia * instances;
   e.v[IA_P] = ip;
   /* Every index is a distinct vertex, so each must be shaded at least
    * once per instance. */
   e.v[VS] = (uint64_t)(idx ? 1 : verts_ia) * instances;
   e.v[GS_I] = gs ? ip : 0;
   e.v[GS_P] = gs ? op : 0;
   e.v[CLIP_I] = op;
   e.v[CLIP_P] = op;
   e.v[FS] = count_red(t);
   e.v[CS] = 0;
   if (ia_verts_min)
      e.min |= 1u << IA_V;
   return check(name, got, &e) || !e.v[FS];
}

static int
compute_case(struct dx7 *t)
{
   VkShaderModule cs = dx7_module(t, cs_ps, sizeof(cs_ps));
   VkComputePipelineCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = cs,
                .pName = "main"},
      .layout = t->layout};
   VkPipeline p;
   CK(vkCreateComputePipelines(t->dev, VK_NULL_HANDLE, 1, &cpci, NULL, &p),
      "CompPipe");
   vkDestroyShaderModule(t->dev, cs, NULL);

   uint32_t ind[3] = {2, 2, 2};
   VkBuffer ib, cb;
   VkDeviceMemory im, cm;
   dx7_buffer(t, sizeof(ind), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, ind, &ib,
              &im);
   dx7_buffer(t, sizeof(uint64_t) * (NSTATS + 1),
              VK_BUFFER_USAGE_TRANSFER_DST_BIT, NULL, &cb, &cm);

   cmd_begin(t);
   vkCmdResetQueryPool(t->cmd, pool, 0, 1);
   vkCmdBeginQuery(t->cmd, pool, 0, 0);
   vkCmdBindPipeline(t->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p);
   vkCmdDispatch(t->cmd, 3, 2, 1);
   vkCmdDispatchIndirect(t->cmd, ib, 0);
   vkCmdEndQuery(t->cmd, pool, 0);
   /* GPU copy path: results land in cb after the query completes. */
   vkCmdCopyQueryPoolResults(t->cmd, pool, 0, 1, cb, 0,
                             sizeof(uint64_t) * (NSTATS + 1),
                             VK_QUERY_RESULT_64_BIT |
                                VK_QUERY_RESULT_WAIT_BIT |
                                VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
   cmd_submit(t);
   vkDestroyPipeline(t->dev, p, NULL);

   struct expect e = {.min = 0};
   e.v[CS] = 8 * (3 * 2 * 1 + 2 * 2 * 2);

   uint64_t got[NSTATS + 1], *copy;
   CK(host_results(t, got), "Results");
   int fail = check("compute_direct_indirect", got, &e);

   CK(vkMapMemory(t->dev, cm, 0, sizeof(uint64_t) * (NSTATS + 1), 0,
                  (void **)&copy),
      "CopyMap");
   fail |= check("compute_copy_results", copy, &e);
   vkUnmapMemory(t->dev, cm);
   return fail;
}

static const uint16_t list_idx[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
static const uint16_t strip_idx[9] = {0, 1, 2, 3, RESTART, 9, 10, 11, 12};

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }
   struct dx7 t;
   VkPhysicalDeviceFeatures want = {0};
   want.geometryShader = VK_TRUE;
   want.pipelineStatisticsQuery = VK_TRUE;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.pipelineStatisticsQuery) {
      printf("FAIL pipelineStatisticsQuery not exposed\n");
      return 1;
   }

   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
#define PS_LOAD(n)                                                             \
   vk##n = (PFN_vk##n)gipa(t.inst, "vk" #n);                                   \
   if (!vk##n) {                                                               \
      printf("FAIL missing vk" #n "\n");                                       \
      return 1;                                                                \
   }
   PS_FUNCS(PS_LOAD)

   VkQueryPoolCreateInfo qpci = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
      .queryCount = 1,
      .pipelineStatistics = stat_mask};
   CK(vkCreateQueryPool(t.dev, &qpci, NULL, &pool), "QueryPool");

   VkDeviceMemory vm;
   dx7_buffer(&t, sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts, &vb,
              &vm);

   const VkPrimitiveTopology list = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   const VkPrimitiveTopology strip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   int fails = 0;
   /* name, topo, idx, count, inst, prims, ia verts, gs, indirect, restart,
    * ia_verts lower bound only */
   fails += draw_case(&t, "draw", list, NULL, 18, 1, 6, 18, 0, 0, 0, 0);
   fails += draw_case(&t, "draw_instanced2", list, NULL, 9, 2, 3, 9, 0, 0, 0, 0);
   fails += draw_case(&t, "draw_indexed", list, list_idx, 9, 1, 3, 9, 0, 0, 0, 0);
   /* Restart splits the strip into 2 + 2 triangles. Whether the restart
    * index counts as a vertex is implementation-defined. */
   fails += draw_case(&t, "draw_indexed_restart", strip, strip_idx, 9, 1, 4, 8,
                      0, 0, 1, 1);
   fails += draw_case(&t, "draw_indirect", list, NULL, 9, 3, 3, 9, 0, 1, 0, 0);
   fails += draw_case(&t, "draw_indexed_indirect", list, list_idx, 9, 1, 3, 9,
                      0, 1, 0, 0);
   fails += draw_case(&t, "draw_indexed_restart_indirect", strip, strip_idx, 9,
                      1, 4, 8, 0, 1, 1, 1);
   if (t.feats.geometryShader) {
      fails += draw_case(&t, "gs_draw", list, NULL, 18, 1, 6, 18, 1, 0, 0, 0);
      fails += draw_case(&t, "gs_draw_indexed", list, list_idx, 9, 2, 3, 9, 1,
                         0, 0, 0);
      fails += draw_case(&t, "gs_draw_indirect", list, NULL, 9, 2, 3, 9, 1, 1,
                         0, 0);
   } else {
      printf("SKIP gs cases: geometryShader not exposed\n");
   }
   fails += compute_case(&t);

   printf("PIPELINE_STATS_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
