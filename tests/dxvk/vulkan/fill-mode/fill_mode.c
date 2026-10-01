/* DX7 fillModeNonSolid draw test. Each case draws triangles with
 * VK_POLYGON_MODE_LINE or _POINT and compares the RGBA readback pixel-exact
 * against a GPU reference: the same edges (or vertices) drawn as a filled
 * LINE_LIST (or POINT_LIST). Which triangles survive culling is decided by
 * drawing each triangle filled with the same cull mode and viewport on the
 * hardware rasterizer, so facing is never computed on the CPU. */
#include "../dx7_harness.h"
#include "../clip-cull/clip_cull_spv.h"

#define MAXV 64
#define RESTART 0xffff

struct shape {
   const char *name;
   VkPrimitiveTopology topology;
   int nverts;
   float xy[MAXV][2];
   int nidx;
   uint16_t idx[MAXV];
};

struct draw {
   VkBuffer vb, ib, indirect;
   uint32_t count;
   int indexed;
   VkViewport vp;
};

static int use_indirect;

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   const struct draw *d = data;
   VkDeviceSize off = 0;
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   vkCmdSetViewport(cmd, 0, 1, &d->vp);
   vkCmdSetScissor(cmd, 0, 1, &sc);
   vkCmdBindVertexBuffers(cmd, 0, 1, &d->vb, &off);
   if (d->indexed)
      vkCmdBindIndexBuffer(cmd, d->ib, 0, VK_INDEX_TYPE_UINT16);
   if (d->indirect) {
      if (d->indexed)
         vkCmdDrawIndexedIndirect(cmd, d->indirect, 0, 1, 0);
      else
         vkCmdDrawIndirect(cmd, d->indirect, 0, 1, 0);
   } else if (d->indexed) {
      vkCmdDrawIndexed(cmd, d->count, 1, 0, 0, 0);
   } else {
      vkCmdDraw(cmd, d->count, 1, 0, 0);
   }
}

static int
count_red(struct dx7 *t)
{
   int n = 0;
   for (int i = 0; i < RT_W * RT_H; i++)
      n += dx7_is_red(t->px + i * 4);
   return n;
}

/* Expand the shape into triangles in the order the driver assembles them:
 * list (3i, 3i+1, 3i+2), strip even (i, i+1, i+2) / odd (i+1, i, i+2),
 * fan (i+1, i+2, 0), segments split at the restart index. */
static int
expand(const struct shape *s, int tris[][3])
{
   int seq[MAXV], n = 0, nt = 0;
   int total = s->nidx ? s->nidx : s->nverts;
   for (int k = 0; k <= total; k++) {
      int v = k < total ? (s->nidx ? s->idx[k] : k) : RESTART;
      if (v != RESTART) {
         seq[n++] = v;
         continue;
      }
      for (int i = 0; i + 2 < n; i++) {
         int *o = tris[nt];
         if (s->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
            if (i % 3)
               continue;
            o[0] = seq[i], o[1] = seq[i + 1], o[2] = seq[i + 2];
         } else if (s->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
            o[0] = seq[i + (i & 1)], o[1] = seq[i + !(i & 1)], o[2] = seq[i + 2];
         } else {
            o[0] = seq[i + 1], o[1] = seq[i + 2], o[2] = seq[0];
         }
         nt++;
      }
      n = 0;
   }
   return nt;
}

static VkBuffer
vbuf_of(struct dx7 *t, const float (*xy)[2], const int *ids, int n)
{
   float v[MAXV * 6][4];
   for (int i = 0; i < n; i++) {
      v[i][0] = xy[ids[i]][0];
      v[i][1] = xy[ids[i]][1];
      v[i][2] = 1.0f;
      v[i][3] = 0.0f;
   }
   VkBuffer b;
   VkDeviceMemory m;
   dx7_buffer(t, sizeof(float) * 4 * (n ? n : 1), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              v, &b, &m);
   return b;
}

static VkPipeline
pipe_of(struct dx7 *t, VkPrimitiveTopology topo, VkPolygonMode mode,
        VkCullModeFlags cull, int restart, int point)
{
   struct dx7_pipe_desc d = {
      .vs = point ? vs_psize1 : vs_none,
      .vs_size = point ? sizeof(vs_psize1) : sizeof(vs_none),
      .fs = fs_color, .fs_size = sizeof(fs_color),
      .topology = topo,
      .polygon_mode = mode,
      .cull_mode = cull,
      .dynamic_viewport = 1,
      .primitive_restart = restart,
   };
   return dx7_pipeline(t, &d);
}

static int
run_case(struct dx7 *t, const char *name, const struct shape *s,
         VkPolygonMode mode, VkCullModeFlags cull, int flip)
{
   static uint8_t ref[RT_W * RT_H * 4];
   int point = mode == VK_POLYGON_MODE_POINT;
   VkViewport vp = flip ? (VkViewport){0, RT_H, RT_W, -RT_H, 0, 1}
                        : (VkViewport){0, 0, RT_W, RT_H, 0, 1};
   int tris[MAXV][3];
   int nt = expand(s, tris);

   VkPipeline fill = pipe_of(t, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                             VK_POLYGON_MODE_FILL, cull, 0, 0);
   int kept_ids[MAXV * 6], nk = 0, kept = 0;
   for (int i = 0; i < nt; i++) {
      VkBuffer b = vbuf_of(t, s->xy, tris[i], 3);
      struct draw d = {.vb = b, .count = 3, .vp = vp};
      dx7_run(t, fill, b, rec, &d);
      if (!count_red(t))
         continue;
      kept++;
      int *o = tris[i];
      if (point) {
         kept_ids[nk++] = o[0], kept_ids[nk++] = o[1], kept_ids[nk++] = o[2];
      } else {
         int e[6] = {o[0], o[1], o[1], o[2], o[2], o[0]};
         memcpy(kept_ids + nk, e, sizeof(e));
         nk += 6;
      }
   }
   vkDestroyPipeline(t->dev, fill, NULL);

   VkPipeline rp = pipe_of(t,
                           point ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                                 : VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                           VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, 0, point);
   VkBuffer rb = vbuf_of(t, s->xy, kept_ids, nk);
   struct draw rd = {.vb = rb, .count = nk, .vp = vp};
   dx7_run(t, rp, rb, rec, &rd);
   memcpy(ref, t->px, sizeof(ref));
   int ref_red = count_red(t);
   vkDestroyPipeline(t->dev, rp, NULL);

   int all[MAXV];
   for (int i = 0; i < s->nverts; i++)
      all[i] = i;
   VkBuffer vb = vbuf_of(t, s->xy, all, s->nverts);
   struct draw td = {.vb = vb, .count = s->nidx ? s->nidx : s->nverts,
                     .vp = vp};
   if (s->nidx) {
      VkBuffer ib;
      VkDeviceMemory im;
      dx7_buffer(t, sizeof(uint16_t) * s->nidx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 s->idx, &ib, &im);
      td.ib = ib;
      td.indexed = 1;
   }
   if (use_indirect) {
      uint32_t cmdw[5] = {td.count, 1, 0, 0, 0};
      VkDeviceMemory am;
      dx7_buffer(t, sizeof(cmdw), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, cmdw,
                 &td.indirect, &am);
   }
   VkPipeline tp = pipe_of(t, s->topology, mode, cull, s->nidx != 0, point);
   dx7_run(t, tp, vb, rec, &td);
   vkDestroyPipeline(t->dev, tp, NULL);

   int bad = 0, fx = -1, fy = -1;
   for (int i = 0; i < RT_W * RT_H; i++)
      if (memcmp(ref + i * 4, t->px + i * 4, 4) && !bad++)
         fx = i % RT_W, fy = i / RT_W;
   int red = count_red(t);
   int fail = bad || (kept && !ref_red);
   printf("CASE %s tris=%d kept=%d ref_red=%d red=%d mismatch=%d", name, nt,
          kept, ref_red, red, bad);
   if (bad) {
      const uint8_t *p = dx7_px(t, fx, fy), *q = ref + (fy * RT_W + fx) * 4;
      printf(" first=(%d,%d) got=%u %u %u want=%u %u %u", fx, fy, p[0], p[1],
             p[2], q[0], q[1], q[2]);
   }
   printf(" %s\n", fail ? "FAIL" : "PASS");
   return fail;
}

static const struct shape list3 = {
   "list", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 9,
   {{-0.83f, -0.71f}, {-0.12f, -0.77f}, {-0.46f, 0.07f},
    {0.11f, -0.62f}, {0.47f, 0.21f}, {0.86f, -0.66f},
    {-0.72f, 0.31f}, {0.69f, 0.36f}, {0.03f, 0.87f}},
};

static const struct shape strip5 = {
   "strip", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 5,
   {{-0.81f, 0.17f}, {-0.68f, 0.79f}, {-0.29f, 0.23f}, {0.02f, 0.83f},
    {0.41f, 0.13f}},
};

static const struct shape fan6 = {
   "fan", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, 7,
   {{0.01f, -0.03f}, {0.71f, 0.09f}, {0.52f, 0.61f}, {-0.11f, 0.74f},
    {-0.63f, 0.38f}, {-0.69f, -0.31f}, {-0.21f, 0.52f}},
};

static const struct shape strip_restart = {
   "strip_restart", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 8,
   {{-0.87f, -0.83f}, {-0.79f, -0.13f}, {-0.31f, -0.77f}, {-0.23f, -0.09f},
    {0.13f, 0.19f}, {0.21f, 0.88f}, {0.67f, 0.23f}, {0.83f, 0.81f}},
   9,
   {0, 1, 2, 3, RESTART, 4, 5, 6, 7},
};

static const struct shape fan_restart = {
   "fan_restart", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, 8,
   {{-0.51f, -0.49f}, {-0.09f, -0.53f}, {-0.27f, -0.07f}, {-0.83f, -0.21f},
    {0.49f, 0.51f}, {0.91f, 0.47f}, {0.73f, 0.93f}, {0.17f, 0.79f}},
   9,
   {0, 1, 2, 3, RESTART, 4, 5, 6, 7},
};

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }
   struct dx7 t;
   VkPhysicalDeviceFeatures want = {0};
   want.fillModeNonSolid = VK_TRUE;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.fillModeNonSolid) {
      printf("FAIL feature not exposed\n");
      return 1;
   }

   const VkPolygonMode L = VK_POLYGON_MODE_LINE, P = VK_POLYGON_MODE_POINT;
   int fails = 0;
   fails += run_case(&t, "list_line_cull_none", &list3, L, VK_CULL_MODE_NONE, 0);
   fails += run_case(&t, "list_line_cull_back", &list3, L, VK_CULL_MODE_BACK_BIT, 0);
   fails += run_case(&t, "list_line_cull_front", &list3, L, VK_CULL_MODE_FRONT_BIT, 0);
   fails += run_case(&t, "list_line_cull_both", &list3, L,
                     VK_CULL_MODE_FRONT_AND_BACK, 0);
   fails += run_case(&t, "list_line_cull_back_flipy", &list3, L,
                     VK_CULL_MODE_BACK_BIT, 1);
   fails += run_case(&t, "strip_line_cull_none", &strip5, L, VK_CULL_MODE_NONE, 0);
   fails += run_case(&t, "strip_line_cull_back", &strip5, L, VK_CULL_MODE_BACK_BIT, 0);
   fails += run_case(&t, "fan_line_cull_none", &fan6, L, VK_CULL_MODE_NONE, 0);
   fails += run_case(&t, "fan_line_cull_front", &fan6, L, VK_CULL_MODE_FRONT_BIT, 0);
   fails += run_case(&t, "strip_restart_line", &strip_restart, L,
                     VK_CULL_MODE_NONE, 0);
   fails += run_case(&t, "fan_restart_line_cull_front", &fan_restart, L,
                     VK_CULL_MODE_FRONT_BIT, 0);
   fails += run_case(&t, "fan_restart_line_cull_back", &fan_restart, L,
                     VK_CULL_MODE_BACK_BIT, 0);
   fails += run_case(&t, "list_point_cull_none", &list3, P, VK_CULL_MODE_NONE, 0);
   fails += run_case(&t, "fan_point_cull_back", &fan6, P, VK_CULL_MODE_BACK_BIT, 0);
   fails += run_case(&t, "strip_point_flipy_cull_front", &strip5, P,
                     VK_CULL_MODE_FRONT_BIT, 1);
   use_indirect = 1;
   fails += run_case(&t, "indirect_list_line_cull_back", &list3, L,
                     VK_CULL_MODE_BACK_BIT, 0);
   fails += run_case(&t, "indirect_indexed_strip_restart_line", &strip_restart,
                     L, VK_CULL_MODE_NONE, 0);
   use_indirect = 0;

   printf("FILL_MODE_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
