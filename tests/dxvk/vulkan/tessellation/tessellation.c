/* DX8 tessellation shader draw test. Each case draws with a
 * VS(vs_tess)+TCS+TES(+GS)+FS(fs_tess) pipeline and compares the RGBA
 * readback pixel-exact against a reference drawn without tessellation:
 * vs_ref+fs_tess, TRIANGLE_LIST / LINE_LIST / POINT_LIST. */
#include "../dx7_harness.h"
#include "tessellation_spv.h"

#define TS_FUNCS(X)                                                            \
   X(EnumerateDeviceExtensionProperties)                                       \
   X(GetPhysicalDeviceFeatures2)                                               \
   X(CreateQueryPool)                                                          \
   X(CmdResetQueryPool)                                                        \
   X(CmdBeginQuery)                                                            \
   X(CmdEndQuery)                                                              \
   X(GetQueryPoolResults)                                                      \
   X(CmdSetPatchControlPointsEXT)

TS_FUNCS(DX7_DECL)

static const char *icd_path;
static VkPhysicalDeviceFeatures enabled_features;
static int has_eds2 = 0;

static const uint32_t *tcs_code[10] = {
   tcs_mode0, tcs_mode1, tcs_mode2, tcs_mode3,
   tcs_mode4, tcs_mode5, tcs_mode6, tcs_mode7,
   tcs_mode8, tcs_mode9,
};
static const size_t tcs_size[10] = {
   sizeof(tcs_mode0), sizeof(tcs_mode1), sizeof(tcs_mode2), sizeof(tcs_mode3),
   sizeof(tcs_mode4), sizeof(tcs_mode5), sizeof(tcs_mode6), sizeof(tcs_mode7),
   sizeof(tcs_mode8), sizeof(tcs_mode9),
};

static const uint32_t *tes_code[7] = {
   tes_mode0, tes_mode1, tes_mode2, tes_mode3,
   tes_mode4, tes_mode5, tes_mode6,
};
static const size_t tes_size[7] = {
   sizeof(tes_mode0), sizeof(tes_mode1), sizeof(tes_mode2), sizeof(tes_mode3),
   sizeof(tes_mode4), sizeof(tes_mode5), sizeof(tes_mode6),
};

struct vert {
   float x, y, z, w;
};

struct vert_array {
   struct vert *data;
   uint32_t count;
   uint32_t capacity;
};

static void
vert_array_init(struct vert_array *va)
{
   va->count = 0;
   va->capacity = 256;
   va->data = malloc(sizeof(struct vert) * va->capacity);
   if (!va->data) {
      printf("FAIL oom vert_array\n");
      exit(1);
   }
}

static void
vert_array_push(struct vert_array *va, float x, float y, float z, float w)
{
   if (va->count == va->capacity) {
      va->capacity *= 2;
      va->data = realloc(va->data, sizeof(struct vert) * va->capacity);
      if (!va->data) {
         printf("FAIL oom vert_array_push\n");
         exit(1);
      }
   }
   va->data[va->count++] = (struct vert){x, y, z, w};
}

static void
vert_array_free(struct vert_array *va)
{
   free(va->data);
   va->data = NULL;
   va->count = 0;
   va->capacity = 0;
}

static void
ref_point(struct vert_array *va, float x, float y, float id)
{
   vert_array_push(va, x, y, id, 0.0f);
}

static void
ref_line(struct vert_array *va, float x0, float y0, float x1, float y1, float id)
{
   vert_array_push(va, x0, y0, id, 0.0f);
   vert_array_push(va, x1, y1, id, 0.0f);
}

static void
ref_tri(struct vert_array *va, float x0, float y0, float x1, float y1,
        float x2, float y2, float id)
{
   vert_array_push(va, x0, y0, id, 0.0f);
   vert_array_push(va, x1, y1, id, 0.0f);
   vert_array_push(va, x2, y2, id, 0.0f);
}

static void
ref_quad(struct vert_array *va, float x0, float y0, float x1, float y1, float id)
{
   ref_tri(va, x0, y0, x1, y0, x1, y1, id);
   ref_tri(va, x0, y0, x1, y1, x0, y1, id);
}

static VkBuffer
vbuf(struct dx7 *t, const struct vert *verts, uint32_t n)
{
   VkBuffer b;
   VkDeviceMemory m;
   dx7_buffer(t, sizeof(struct vert) * (n ? n : 1),
              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts, &b, &m);
   return b;
}

static VkPipeline
pipe_of(struct dx7 *t, int tcs_idx, int tes_idx, int has_gs,
        VkPrimitiveTopology topo, VkCullModeFlags cull,
        uint32_t patch_cp, int dynamic_cp)
{
   int is_tess = tcs_idx >= 0;
   struct dx7_pipe_desc d = {
      .vs = is_tess ? vs_tess : vs_ref,
      .vs_size = is_tess ? sizeof(vs_tess) : sizeof(vs_ref),
      .fs = fs_tess,
      .fs_size = sizeof(fs_tess),
      .gs = (is_tess && has_gs) ? gs_tess : NULL,
      .gs_size = (is_tess && has_gs) ? sizeof(gs_tess) : 0,
      .tcs = is_tess ? tcs_code[tcs_idx] : NULL,
      .tcs_size = is_tess ? tcs_size[tcs_idx] : 0,
      .tes = is_tess ? tes_code[tes_idx] : NULL,
      .tes_size = is_tess ? tes_size[tes_idx] : 0,
      .patch_control_points = patch_cp,
      .dynamic_patch_control_points = dynamic_cp,
      .topology = topo,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = cull,
      .dynamic_viewport = 1,
   };
   return dx7_pipeline(t, &d);
}

struct draw {
   VkBuffer vb;
   VkBuffer ib;
   VkBuffer indirect;
   uint32_t count;
   uint32_t instances;
   uint32_t first_instance;
   int indexed;
   int32_t vertex_offset;
   int is_dyn;
   VkQueryPool pool;
};

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   (void)t;
   const struct draw *d = data;
   VkDeviceSize off = 0;
   VkViewport vp = {0, 0, RT_W, RT_H, 0, 1};
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   vkCmdSetViewport(cmd, 0, 1, &vp);
   vkCmdSetScissor(cmd, 0, 1, &sc);
   vkCmdBindVertexBuffers(cmd, 0, 1, &d->vb, &off);
   if (d->indexed)
      vkCmdBindIndexBuffer(cmd, d->ib, 0, VK_INDEX_TYPE_UINT16);

   if (d->pool)
      vkCmdBeginQuery(cmd, d->pool, 0, 0);

   if (d->is_dyn) {
      vkCmdSetPatchControlPointsEXT(cmd, 3);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdSetPatchControlPointsEXT(cmd, 4);
      vkCmdDraw(cmd, 4, 1, 3, 0);
   } else if (d->indirect) {
      if (d->indexed)
         vkCmdDrawIndexedIndirect(cmd, d->indirect, 0, 1, 0);
      else
         vkCmdDrawIndirect(cmd, d->indirect, 0, 1, 0);
   } else if (d->indexed) {
      vkCmdDrawIndexed(cmd, d->count, d->instances, 0, d->vertex_offset, d->first_instance);
   } else {
      vkCmdDraw(cmd, d->count, d->instances, 0, d->first_instance);
   }

   if (d->pool)
      vkCmdEndQuery(cmd, d->pool, 0);
}

static int
count_px(const uint8_t *px)
{
   int n = 0;
   for (int i = 0; i < RT_W * RT_H; i++) {
      const uint8_t *p = px + i * 4;
      if (p[0] != 0 || p[1] != 0 || p[2] != 255 || p[3] != 255)
         n++;
   }
   return n;
}

static void
cmd_begin(struct dx7 *t)
{
   CK(vkResetFences(t->dev, 1, &t->fence), "ResetFence");
   CK(vkResetCommandBuffer(t->cmd, 0), "ResetCmd");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
   };
   CK(vkBeginCommandBuffer(t->cmd, &bbi), "Begin");
}

static void
cmd_submit(struct dx7 *t)
{
   CK(vkEndCommandBuffer(t->cmd), "End");
   VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &t->cmd,
   };
   CK(vkQueueSubmit(t->queue, 1, &si, t->fence), "Submit");
   CK(vkWaitForFences(t->dev, 1, &t->fence, VK_TRUE, 30ull * 1000000000ull),
      "Wait");
}

static void
reset_query(struct dx7 *t, VkQueryPool pool)
{
   cmd_begin(t);
   vkCmdResetQueryPool(t->cmd, pool, 0, 1);
   cmd_submit(t);
}

struct case_desc {
   const char *name;
   int tcs_idx;
   int tes_idx;
   int gs;
   VkCullModeFlags cull;
   uint32_t patch_cp;
   int is_dyn;
   const struct vert *tess_verts;
   uint32_t tess_vert_count;
   const uint16_t *indices;
   uint32_t index_count;
   uint32_t draw_count;
   uint32_t instances;
   uint32_t first_instance;
   int32_t vertex_offset;
   int indirect;
   VkQueryPool pool;
   VkPrimitiveTopology ref_topology;
   const struct vert *ref_verts;
   uint32_t ref_vert_count;
   VkCullModeFlags ref_cull;
   int expect_empty;
};

static int
run_case(struct dx7 *t, const struct case_desc *c)
{
   static uint8_t ref[RT_W * RT_H * 4];
   int ref_px = 0;

   if (c->ref_vert_count > 0) {
      VkPipeline rp = pipe_of(t, -1, -1, 0, c->ref_topology, c->ref_cull, 0, 0);
      VkBuffer rb = vbuf(t, c->ref_verts, c->ref_vert_count);
      struct draw rd = {
         .vb = rb,
         .ib = VK_NULL_HANDLE,
         .indirect = VK_NULL_HANDLE,
         .count = c->ref_vert_count,
         .instances = 1,
         .first_instance = 0,
         .indexed = 0,
         .vertex_offset = 0,
         .is_dyn = 0,
         .pool = VK_NULL_HANDLE,
      };
      dx7_run(t, rp, rb, rec, &rd);
      memcpy(ref, t->px, sizeof(ref));
      ref_px = count_px(t->px);
      vkDestroyPipeline(t->dev, rp, NULL);
   } else {
      for (int i = 0; i < RT_W * RT_H; i++) {
         ref[i * 4 + 0] = 0;
         ref[i * 4 + 1] = 0;
         ref[i * 4 + 2] = 255;
         ref[i * 4 + 3] = 255;
      }
      ref_px = 0;
   }

   VkBuffer vb = vbuf(t, c->tess_verts, c->tess_vert_count);
   struct draw td = {
      .vb = vb,
      .ib = VK_NULL_HANDLE,
      .indirect = VK_NULL_HANDLE,
      .count = c->draw_count ? c->draw_count : c->tess_vert_count,
      .instances = c->instances ? c->instances : 1,
      .first_instance = c->first_instance,
      .indexed = c->indices != NULL,
      .vertex_offset = c->vertex_offset,
      .is_dyn = c->is_dyn,
      .pool = c->pool,
   };

   if (c->indices) {
      VkDeviceMemory im;
      dx7_buffer(t, sizeof(uint16_t) * c->index_count,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, c->indices, &td.ib, &im);
   }

   if (c->indirect) {
      if (c->indices) {
         VkDrawIndexedIndirectCommand cmdw = {
            .indexCount = td.count,
            .instanceCount = td.instances,
            .firstIndex = 0,
            .vertexOffset = td.vertex_offset,
            .firstInstance = td.first_instance,
         };
         VkDeviceMemory am;
         dx7_buffer(t, sizeof(cmdw), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                    &cmdw, &td.indirect, &am);
      } else {
         VkDrawIndirectCommand cmdw = {
            .vertexCount = td.count,
            .instanceCount = td.instances,
            .firstVertex = 0,
            .firstInstance = td.first_instance,
         };
         VkDeviceMemory am;
         dx7_buffer(t, sizeof(cmdw), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                    &cmdw, &td.indirect, &am);
      }
   }

   if (c->pool)
      reset_query(t, c->pool);

   VkPipeline tp = pipe_of(t, c->tcs_idx, c->tes_idx, c->gs,
                           VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, c->cull,
                           c->patch_cp, c->is_dyn);
   dx7_run(t, tp, vb, rec, &td);
   vkDestroyPipeline(t->dev, tp, NULL);

   int bad = 0, fx = -1, fy = -1;
   for (int i = 0; i < RT_W * RT_H; i++) {
      if (memcmp(ref + i * 4, t->px + i * 4, 4) != 0) {
         if (!bad) {
            fx = i % RT_W;
            fy = i / RT_W;
         }
         bad++;
      }
   }
   int px = count_px(t->px);
   int fail = bad || (c->expect_empty ? (ref_px != 0 || px != 0) : (ref_px == 0));

   uint64_t qres[3] = {0};
   if (c->pool) {
      CK(vkGetQueryPoolResults(t->dev, c->pool, 0, 1, sizeof(qres), qres,
                               sizeof(qres),
                               VK_QUERY_RESULT_64_BIT |
                                  VK_QUERY_RESULT_WAIT_BIT |
                                  VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
         "GetQueryPoolResults");
      if (qres[0] != 1 || qres[1] < 1 || qres[2] != 1)
         fail = 1;
   }

   printf("CASE %s ref_px=%d px=%d mismatch=%d", c->name, ref_px, px, bad);
   if (c->pool) {
      printf(" patches=%llu tes_inv=%llu avail=%llu",
             (unsigned long long)qres[0],
             (unsigned long long)qres[1],
             (unsigned long long)qres[2]);
   }
   if (bad) {
      const uint8_t *p = dx7_px(t, fx, fy);
      const uint8_t *q = ref + (fy * RT_W + fx) * 4;
      printf(" first=(%d,%d) got=%u %u %u want=%u %u %u", fx, fy,
             p[0], p[1], p[2], q[0], q[1], q[2]);
   }
   printf(" %s\n", fail ? "FAIL" : "PASS");
   return fail;
}

static void
device_hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   enabled_features.tessellationShader = t->feats.tessellationShader;
   enabled_features.geometryShader = t->feats.geometryShader;
   enabled_features.shaderTessellationAndGeometryPointSize =
      t->feats.shaderTessellationAndGeometryPointSize;
   enabled_features.pipelineStatisticsQuery =
      t->feats.pipelineStatisticsQuery;
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

#define TS_LOAD(n)                                                             \
   vk##n = (PFN_vk##n)gipa(t->inst, "vk" #n);                                  \
   if (!vk##n && strcmp(#n, "CmdSetPatchControlPointsEXT") != 0) {             \
      printf("FAIL missing vk" #n "\n");                                       \
      exit(1);                                                                 \
   }
   TS_FUNCS(TS_LOAD)

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

   int listed_eds2 = 0;
   for (uint32_t i = 0; i < ext_count; i++) {
      if (strcmp(exts[i].extensionName,
                 "VK_EXT_extended_dynamic_state2") == 0) {
         listed_eds2 = 1;
         break;
      }
   }
   free(exts);

   static VkPhysicalDeviceExtendedDynamicState2FeaturesEXT eds2_feat;
   static const char *ext_names[] = {"VK_EXT_extended_dynamic_state2"};

   if (listed_eds2) {
      VkPhysicalDeviceExtendedDynamicState2FeaturesEXT qfeat = {
         .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT,
      };
      VkPhysicalDeviceFeatures2 qf2 = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
         .pNext = &qfeat,
      };
      vkGetPhysicalDeviceFeatures2(t->phys, &qf2);
      if (qfeat.extendedDynamicState2PatchControlPoints &&
          vkCmdSetPatchControlPointsEXT) {
         eds2_feat.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
         eds2_feat.pNext = (void *)dci->pNext;
         eds2_feat.extendedDynamicState2PatchControlPoints = VK_TRUE;
         dci->pNext = &eds2_feat;
         dci->enabledExtensionCount = 1;
         dci->ppEnabledExtensionNames = ext_names;
         has_eds2 = 1;
      }
   }
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

   if (!enabled_features.tessellationShader) {
      printf("FAIL tessellationShader not exposed\n");
      return 1;
   }

   printf("features tess=%d geom=%d point_size=%d pstats=%d has_eds2=%d "
          "maxGenLevel=%u maxPatchSize=%u\n",
          enabled_features.tessellationShader,
          enabled_features.geometryShader,
          enabled_features.shaderTessellationAndGeometryPointSize,
          enabled_features.pipelineStatisticsQuery,
          has_eds2,
          t.props.limits.maxTessellationGenerationLevel,
          t.props.limits.maxTessellationPatchSize);

   struct vert q_verts[4] = {
      {-0.75f, -0.75f, 0, 0},
      { 0.75f, -0.75f, 0, 0},
      { 0.75f,  0.25f, 0, 0},
      {-0.75f,  0.25f, 0, 0},
   };

   struct vert_array ref_q;
   vert_array_init(&ref_q);
   ref_quad(&ref_q, -0.75f, -0.75f, 0.75f, 0.25f, 0);

   struct vert t_verts[3] = {
      { 0.734375f, -0.75f,     0, 0},
      {-0.75f,      0.734375f, 0, 0},
      {-0.75f,     -0.75f,     0, 0},
   };

   struct vert_array ref_t;
   vert_array_init(&ref_t);
   ref_tri(&ref_t, -0.75f, -0.75f, -0.75f, 0.734375f, 0.734375f, -0.75f, 0);

   struct vert p_verts[4] = {
      {-0.734375f, -0.734375f, 0, 0},
      { 0.765625f, -0.734375f, 0, 0},
      { 0.765625f,  0.765625f, 0, 0},
      {-0.734375f,  0.765625f, 0, 0},
   };

   struct vert_array ref_iso;
   vert_array_init(&ref_iso);
   float px0 = -0.734375f, py0 = -0.734375f;
   float pw = 1.5f, ph = 1.5f;
   for (int j = 0; j < 4; j++) {
      float y = py0 + (float)j / 4.0f * ph;
      for (int i = 0; i < 4; i++) {
         float x0 = px0 + (float)i / 4.0f * pw;
         float x1 = px0 + (float)(i + 1) / 4.0f * pw;
         ref_line(&ref_iso, x0, y, x1, y, 0);
      }
   }

   struct vert_array ref_pts;
   vert_array_init(&ref_pts);
   for (int j = 0; j <= 4; j++) {
      float y = py0 + (float)j / 4.0f * ph;
      for (int i = 0; i <= 4; i++) {
         float x = px0 + (float)i / 4.0f * pw;
         ref_point(&ref_pts, x, y, 0);
      }
   }

   int fails = 0;

   /* 1. quad_equal_l4 */
   struct case_desc c1 = {
      .name = "quad_equal_l4",
      .tcs_idx = 0,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = q_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c1);

   /* 2. tri_equal_l3 */
   struct case_desc c2 = {
      .name = "tri_equal_l3",
      .tcs_idx = 1,
      .tes_idx = 1,
      .patch_cp = 3,
      .tess_verts = t_verts,
      .tess_vert_count = 3,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_t.data,
      .ref_vert_count = ref_t.count,
   };
   fails += run_case(&t, &c2);

   /* 3. quad_frac_odd_l3.5 */
   struct case_desc c3 = {
      .name = "quad_frac_odd_l3.5",
      .tcs_idx = 2,
      .tes_idx = 2,
      .patch_cp = 4,
      .tess_verts = q_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c3);

   /* 4. quad_frac_even_l2.5 */
   struct case_desc c4 = {
      .name = "quad_frac_even_l2.5",
      .tcs_idx = 3,
      .tes_idx = 3,
      .patch_cp = 4,
      .tess_verts = q_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c4);

   /* 5. isolines_4x4 */
   struct case_desc c5 = {
      .name = "isolines_4x4",
      .tcs_idx = 0,
      .tes_idx = 4,
      .patch_cp = 4,
      .tess_verts = p_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .ref_verts = ref_iso.data,
      .ref_vert_count = ref_iso.count,
   };
   fails += run_case(&t, &c5);

   /* 6. point_mode_l4 */
   if (!enabled_features.shaderTessellationAndGeometryPointSize) {
      printf("SKIP point_mode_l4: shaderTessellationAndGeometryPointSize not exposed\n");
   } else {
      struct case_desc c6 = {
         .name = "point_mode_l4",
         .tcs_idx = 0,
         .tes_idx = 5,
         .patch_cp = 4,
         .tess_verts = p_verts,
         .tess_vert_count = 4,
         .ref_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
         .ref_verts = ref_pts.data,
         .ref_vert_count = ref_pts.count,
      };
      fails += run_case(&t, &c6);
   }

   /* 7. winding */
   struct case_desc c7a = {
      .name = "winding_ccw_cull_back",
      .tcs_idx = 1,
      .tes_idx = 1,
      .cull = VK_CULL_MODE_BACK_BIT,
      .patch_cp = 3,
      .tess_verts = t_verts,
      .tess_vert_count = 3,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_t.data,
      .ref_vert_count = ref_t.count,
      .ref_cull = VK_CULL_MODE_BACK_BIT,
   };
   fails += run_case(&t, &c7a);

   struct case_desc c7b = {
      .name = "winding_cw_cull_back",
      .tcs_idx = 1,
      .tes_idx = 6,
      .cull = VK_CULL_MODE_BACK_BIT,
      .patch_cp = 3,
      .tess_verts = t_verts,
      .tess_vert_count = 3,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = NULL,
      .ref_vert_count = 0,
      .ref_cull = VK_CULL_MODE_BACK_BIT,
      .expect_empty = 1,
   };
   fails += run_case(&t, &c7b);

   /* 8. control point count variations */
   struct vert cp1_vert = {-0.75f, -0.75f, 0, 0};
   struct case_desc c8a = {
      .name = "patch_cp1_expand",
      .tcs_idx = 4,
      .tes_idx = 0,
      .patch_cp = 1,
      .tess_verts = &cp1_vert,
      .tess_vert_count = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c8a);

   struct vert cp32_verts[32];
   for (int i = 0; i < 32; i++)
      cp32_verts[i] = (struct vert){0.9f, 0.9f, 0, 0};
   cp32_verts[0] = (struct vert){-0.75f, -0.75f, 0, 0};
   cp32_verts[7] = (struct vert){ 0.75f, -0.75f, 0, 0};
   cp32_verts[24] = (struct vert){-0.75f,  0.25f, 0, 0};
   cp32_verts[31] = (struct vert){ 0.75f,  0.25f, 0, 0};
   struct case_desc c8b = {
      .name = "patch_cp32",
      .tcs_idx = 5,
      .tes_idx = 0,
      .patch_cp = 32,
      .tess_verts = cp32_verts,
      .tess_vert_count = 32,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c8b);

   /* 9. dynamic patch control points */
   if (!has_eds2) {
      printf("SKIP dynamic_cp_3_then_4: extendedDynamicState2PatchControlPoints not supported\n");
   } else {
      struct vert dyn_verts[7] = {
         {-0.875f, -0.75f, 0, 0},
         {-0.125f, -0.75f, 0, 0},
         {-0.125f,  0.25f, 0, 0},
         { 0.125f, -0.75f, 0, 0},
         { 0.875f, -0.75f, 0, 0},
         { 0.875f,  0.25f, 0, 0},
         { 0.125f,  0.25f, 0, 0},
      };
      struct vert_array ref_dyn;
      vert_array_init(&ref_dyn);
      ref_tri(&ref_dyn, -0.875f, -0.75f, -0.125f, -0.75f, -0.125f, 0.25f, 0);
      ref_quad(&ref_dyn, 0.125f, -0.75f, 0.875f, 0.25f, 0);
      struct case_desc c9 = {
         .name = "dynamic_cp_3_then_4",
         .tcs_idx = 6,
         .tes_idx = 0,
         .patch_cp = 4,
         .is_dyn = 1,
         .tess_verts = dyn_verts,
         .tess_vert_count = 7,
         .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .ref_verts = ref_dyn.data,
         .ref_vert_count = ref_dyn.count,
      };
      fails += run_case(&t, &c9);
      vert_array_free(&ref_dyn);
   }

   /* 10. indexed, indirect, instanced */
   struct vert_array ref_ab;
   vert_array_init(&ref_ab);
   ref_quad(&ref_ab, -0.75f, -0.75f, -0.125f, 0.25f, 0);
   ref_quad(&ref_ab,  0.125f, -0.75f,  0.75f, 0.25f, 1);

   struct vert idx_verts[13];
   for (int i = 0; i < 5; i++)
      idx_verts[i] = (struct vert){0.9f, 0.9f, 0, 0};
   idx_verts[5]  = (struct vert){ 0.75f,   0.25f, 0, 0}; /* B.BR */
   idx_verts[6]  = (struct vert){-0.75f,  -0.75f, 0, 0}; /* A.TL */
   idx_verts[7]  = (struct vert){ 0.125f, -0.75f, 0, 0}; /* B.TL */
   idx_verts[8]  = (struct vert){-0.125f,  0.25f, 0, 0}; /* A.BR */
   idx_verts[9]  = (struct vert){-0.125f, -0.75f, 0, 0}; /* A.TR */
   idx_verts[10] = (struct vert){ 0.125f,  0.25f, 0, 0}; /* B.BL */
   idx_verts[11] = (struct vert){-0.75f,   0.25f, 0, 0}; /* A.BL */
   idx_verts[12] = (struct vert){ 0.75f,  -0.75f, 0, 0}; /* B.TR */
   uint16_t indices[8] = {1, 4, 3, 6, 2, 7, 0, 5};

   struct case_desc c10a = {
      .name = "indexed_vertex_offset",
      .tcs_idx = 0,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = idx_verts,
      .tess_vert_count = 13,
      .indices = indices,
      .index_count = 8,
      .draw_count = 8,
      .instances = 1,
      .vertex_offset = 5,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_ab.data,
      .ref_vert_count = ref_ab.count,
   };
   fails += run_case(&t, &c10a);

   struct vert ind_verts[8] = {
      {-0.75f,  -0.75f, 0, 0}, /* A.TL */
      {-0.125f, -0.75f, 0, 0}, /* A.TR */
      {-0.125f,  0.25f, 0, 0}, /* A.BR */
      {-0.75f,   0.25f, 0, 0}, /* A.BL */
      { 0.125f, -0.75f, 0, 0}, /* B.TL */
      { 0.75f,  -0.75f, 0, 0}, /* B.TR */
      { 0.75f,   0.25f, 0, 0}, /* B.BR */
      { 0.125f,  0.25f, 0, 0}, /* B.BL */
   };
   struct case_desc c10b = {
      .name = "indirect",
      .tcs_idx = 0,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = ind_verts,
      .tess_vert_count = 8,
      .draw_count = 8,
      .instances = 1,
      .indirect = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_ab.data,
      .ref_vert_count = ref_ab.count,
   };
   fails += run_case(&t, &c10b);

   struct case_desc c10c = {
      .name = "indexed_indirect",
      .tcs_idx = 0,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = idx_verts,
      .tess_vert_count = 13,
      .indices = indices,
      .index_count = 8,
      .draw_count = 8,
      .instances = 1,
      .vertex_offset = 5,
      .indirect = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_ab.data,
      .ref_vert_count = ref_ab.count,
   };
   fails += run_case(&t, &c10c);
   vert_array_free(&ref_ab);

   struct vert inst_verts[4] = {
      {-0.75f,  -0.75f, 0, 0}, /* A.TL */
      {-0.125f, -0.75f, 0, 0}, /* A.TR */
      {-0.125f,  0.25f, 0, 0}, /* A.BR */
      {-0.75f,   0.25f, 0, 0}, /* A.BL */
   };
   struct vert_array ref_inst;
   vert_array_init(&ref_inst);
   ref_quad(&ref_inst, -0.75f, -0.75f, -0.125f, 0.25f, 0);
   ref_quad(&ref_inst,  0.0f,  -0.75f,  0.625f, 0.25f, 0);
   struct case_desc c10d = {
      .name = "instanced2",
      .tcs_idx = 0,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = inst_verts,
      .tess_vert_count = 4,
      .draw_count = 4,
      .instances = 2,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_inst.data,
      .ref_vert_count = ref_inst.count,
   };
   fails += run_case(&t, &c10d);
   vert_array_free(&ref_inst);

   /* 11. grid_64x48_l16 */
   struct vert *grid_tess = malloc(sizeof(struct vert) * 4 * 3072);
   if (!grid_tess) {
      printf("FAIL oom grid_tess\n");
      exit(1);
   }
   struct vert_array ref_grid;
   vert_array_init(&ref_grid);
   for (int j = 0; j < 48; j++) {
      for (int i = 0; i < 64; i++) {
         int p = j * 64 + i;
         float x0 = (float)i / 32.0f - 1.0f;
         float y0 = (float)j / 32.0f - 1.0f;
         float x1 = x0 + 1.0f / 32.0f;
         float y1 = y0 + 1.0f / 32.0f;
         grid_tess[p * 4 + 0] = (struct vert){x0, y0, 0, 0};
         grid_tess[p * 4 + 1] = (struct vert){x1, y0, 0, 0};
         grid_tess[p * 4 + 2] = (struct vert){x1, y1, 0, 0};
         grid_tess[p * 4 + 3] = (struct vert){x0, y1, 0, 0};
         ref_quad(&ref_grid, x0, y0, x1, y1, (float)p);
      }
   }
   struct case_desc c11 = {
      .name = "grid_64x48_l16",
      .tcs_idx = 7,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = grid_tess,
      .tess_vert_count = 4 * 3072,
      .draw_count = 4 * 3072,
      .instances = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_grid.data,
      .ref_vert_count = ref_grid.count,
   };
   fails += run_case(&t, &c11);
   free(grid_tess);
   vert_array_free(&ref_grid);

   /* quad_l64 */
   struct case_desc c_quad_l64 = {
      .name = "quad_l64",
      .tcs_idx = 8,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = q_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c_quad_l64);

   /* quad_unequal */
   struct case_desc c_quad_unequal = {
      .name = "quad_unequal",
      .tcs_idx = 9,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = q_verts,
      .tess_vert_count = 4,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_q.data,
      .ref_vert_count = ref_q.count,
   };
   fails += run_case(&t, &c_quad_unequal);

   /* 16x12 quad patch grid */
   struct vert *grid16x12_tess = malloc(sizeof(struct vert) * 4 * 192);
   if (!grid16x12_tess) {
      printf("FAIL oom grid16x12_tess\n");
      exit(1);
   }
   for (int j = 0; j < 12; j++) {
      for (int i = 0; i < 16; i++) {
         int p = j * 16 + i;
         float x0 = (float)i / 32.0f - 1.0f;
         float y0 = (float)j / 32.0f - 1.0f;
         float x1 = x0 + 1.0f / 32.0f;
         float y1 = y0 + 1.0f / 32.0f;
         grid16x12_tess[p * 4 + 0] = (struct vert){x0, y0, 0, 0};
         grid16x12_tess[p * 4 + 1] = (struct vert){x1, y0, 0, 0};
         grid16x12_tess[p * 4 + 2] = (struct vert){x1, y1, 0, 0};
         grid16x12_tess[p * 4 + 3] = (struct vert){x0, y1, 0, 0};
      }
   }

   /* grid_inst3_indirect_first1 */
   struct vert_array ref_inst3;
   vert_array_init(&ref_inst3);
   for (int inst = 1; inst <= 3; inst++) {
      float xshift = 0.75f * (float)inst;
      for (int j = 0; j < 12; j++) {
         for (int i = 0; i < 16; i++) {
            int p = j * 16 + i;
            float x0 = (float)i / 32.0f - 1.0f + xshift;
            float y0 = (float)j / 32.0f - 1.0f;
            float x1 = x0 + 1.0f / 32.0f;
            float y1 = y0 + 1.0f / 32.0f;
            ref_quad(&ref_inst3, x0, y0, x1, y1, (float)p);
         }
      }
   }
   struct case_desc c_grid_inst3 = {
      .name = "grid_inst3_indirect_first1",
      .tcs_idx = 7,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = grid16x12_tess,
      .tess_vert_count = 4 * 192,
      .draw_count = 4 * 192,
      .instances = 3,
      .first_instance = 1,
      .indirect = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_inst3.data,
      .ref_vert_count = ref_inst3.count,
   };
   fails += run_case(&t, &c_grid_inst3);
   vert_array_free(&ref_inst3);

   /* grid_indexed_inst2_direct_first1: 768 patches per instance, more than
    * one tessellation chunk holds, so chunks split instances. Indices walk
    * the vertices backwards.
    */
   static struct vert grid32x24_tess[4 * 768];
   static uint16_t grid32x24_idx[4 * 768];
   for (int j = 0; j < 24; j++) {
      for (int i = 0; i < 32; i++) {
         int p = j * 32 + i;
         int v = 4 * 768 - 4 - p * 4;
         float x0 = (float)i / 32.0f - 1.0f;
         float y0 = (float)j / 32.0f - 1.0f;
         float x1 = x0 + 1.0f / 32.0f;
         float y1 = y0 + 1.0f / 32.0f;
         grid32x24_tess[v + 0] = (struct vert){x0, y0, 0, 0};
         grid32x24_tess[v + 1] = (struct vert){x1, y0, 0, 0};
         grid32x24_tess[v + 2] = (struct vert){x1, y1, 0, 0};
         grid32x24_tess[v + 3] = (struct vert){x0, y1, 0, 0};
         for (int k = 0; k < 4; k++)
            grid32x24_idx[p * 4 + k] = (uint16_t)(v + k);
      }
   }

   struct vert_array ref_inst2;
   vert_array_init(&ref_inst2);
   for (int inst = 1; inst <= 2; inst++) {
      float xshift = 0.75f * (float)inst;
      for (int j = 0; j < 24; j++) {
         for (int i = 0; i < 32; i++) {
            int p = j * 32 + i;
            float x0 = (float)i / 32.0f - 1.0f + xshift;
            float y0 = (float)j / 32.0f - 1.0f;
            float x1 = x0 + 1.0f / 32.0f;
            float y1 = y0 + 1.0f / 32.0f;
            ref_quad(&ref_inst2, x0, y0, x1, y1, (float)p);
         }
      }
   }
   struct case_desc c_grid_indexed = {
      .name = "grid_indexed_inst2_direct_first1",
      .tcs_idx = 7,
      .tes_idx = 0,
      .patch_cp = 4,
      .tess_verts = grid32x24_tess,
      .tess_vert_count = 4 * 768,
      .indices = grid32x24_idx,
      .index_count = 4 * 768,
      .draw_count = 4 * 768,
      .instances = 2,
      .first_instance = 1,
      .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .ref_verts = ref_inst2.data,
      .ref_vert_count = ref_inst2.count,
   };
   fails += run_case(&t, &c_grid_indexed);
   c_grid_indexed.name = "grid_indexed_inst2_indirect_first1";
   c_grid_indexed.indirect = 1;
   fails += run_case(&t, &c_grid_indexed);
   vert_array_free(&ref_inst2);
   free(grid16x12_tess);

   /* grid_tes_to_gs_l16 */
   if (!enabled_features.geometryShader) {
      printf("SKIP grid_tes_to_gs_l16: geometryShader not exposed\n");
   } else {
      struct vert *grid_tess_gs = malloc(sizeof(struct vert) * 4 * 3072);
      if (!grid_tess_gs) {
         printf("FAIL oom grid_tess_gs\n");
         exit(1);
      }
      struct vert_array ref_grid_gs;
      vert_array_init(&ref_grid_gs);
      for (int j = 0; j < 48; j++) {
         for (int i = 0; i < 64; i++) {
            int p = j * 64 + i;
            float x0 = (float)i / 32.0f - 1.0f;
            float y0 = (float)j / 32.0f - 1.0f;
            float x1 = x0 + 1.0f / 32.0f;
            float y1 = y0 + 1.0f / 32.0f;
            grid_tess_gs[p * 4 + 0] = (struct vert){x0, y0, 0, 0};
            grid_tess_gs[p * 4 + 1] = (struct vert){x1, y0, 0, 0};
            grid_tess_gs[p * 4 + 2] = (struct vert){x1, y1, 0, 0};
            grid_tess_gs[p * 4 + 3] = (struct vert){x0, y1, 0, 0};
            ref_quad(&ref_grid_gs, x0, y0, x1, y1, (float)p);
         }
      }
      struct case_desc c_grid_gs = {
         .name = "grid_tes_to_gs_l16",
         .tcs_idx = 7,
         .tes_idx = 0,
         .gs = 1,
         .patch_cp = 4,
         .tess_verts = grid_tess_gs,
         .tess_vert_count = 4 * 3072,
         .draw_count = 4 * 3072,
         .instances = 1,
         .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .ref_verts = ref_grid_gs.data,
         .ref_vert_count = ref_grid_gs.count,
      };
      fails += run_case(&t, &c_grid_gs);
      free(grid_tess_gs);
      vert_array_free(&ref_grid_gs);
   }

   /* 12. tes_to_gs */
   if (!enabled_features.geometryShader) {
      printf("SKIP tes_to_gs: geometryShader not exposed\n");
   } else {
      struct case_desc c12 = {
         .name = "tes_to_gs",
         .tcs_idx = 0,
         .tes_idx = 0,
         .gs = 1,
         .patch_cp = 4,
         .tess_verts = q_verts,
         .tess_vert_count = 4,
         .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .ref_verts = ref_q.data,
         .ref_vert_count = ref_q.count,
      };
      fails += run_case(&t, &c12);
   }

   /* 13. stats_quad_equal_l4 */
   if (!enabled_features.pipelineStatisticsQuery) {
      printf("SKIP stats_quad_equal_l4: pipelineStatisticsQuery not exposed\n");
   } else {
      VkQueryPool pool;
      VkQueryPoolCreateInfo qpci = {
         .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
         .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
         .queryCount = 1,
         .pipelineStatistics =
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
      };
      CK(vkCreateQueryPool(t.dev, &qpci, NULL, &pool), "CreateQueryPool");
      struct case_desc c13 = {
         .name = "stats_quad_equal_l4",
         .tcs_idx = 0,
         .tes_idx = 0,
         .patch_cp = 4,
         .pool = pool,
         .tess_verts = q_verts,
         .tess_vert_count = 4,
         .ref_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .ref_verts = ref_q.data,
         .ref_vert_count = ref_q.count,
      };
      fails += run_case(&t, &c13);
   }

   vert_array_free(&ref_q);
   vert_array_free(&ref_t);
   vert_array_free(&ref_iso);
   vert_array_free(&ref_pts);

   printf("TESSELLATION_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
