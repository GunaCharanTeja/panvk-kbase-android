/* DX8 geometry shader draw test. Each case draws with a VS+GS+FS pipeline and
 * compares the RGBA readback pixel-exact against a reference drawn without a
 * GS: the primitives the GS is specified to emit, as a plain TRIANGLE_LIST or
 * LINE_LIST through the hardware IDVS path. */
#include "../dx7_harness.h"
#include "geometry_spv.h"

#define MAXV 64
#define MAXO 1024
#define RESTART 0xffff

enum mode {
   PASS_TRI, PASS_LINE, POINT_QUAD, TRI_WIRE, TWO_TRIS, STRIP4, INVOC3,
   PRIMID, ADJ_TRI, ADJ_LINE, LAYER, NMODES,
};

static const uint32_t *gs_code[NMODES] = {
   gs_mode0, gs_mode1, gs_mode2, gs_mode3, gs_mode4, gs_mode5,
   gs_mode6, gs_mode7, gs_mode8, gs_mode9, gs_mode10,
};
static const size_t gs_size[NMODES] = {
   sizeof(gs_mode0), sizeof(gs_mode1), sizeof(gs_mode2), sizeof(gs_mode3),
   sizeof(gs_mode4), sizeof(gs_mode5), sizeof(gs_mode6), sizeof(gs_mode7),
   sizeof(gs_mode8), sizeof(gs_mode9), sizeof(gs_mode10),
};

struct shape {
   VkPrimitiveTopology topology;
   int nverts;
   float xy[MAXV][2];
   int nidx;
   uint16_t idx[MAXV];
};

struct draw {
   VkBuffer vb, ib, indirect;
   uint32_t count, instances;
   int indexed;
};

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   const struct draw *d = data;
   VkDeviceSize off = 0;
   VkViewport vp = {0, 0, RT_W, RT_H, 0, 1};
   VkRect2D sc = {{0, 0}, {RT_W, RT_H}};
   vkCmdSetViewport(cmd, 0, 1, &vp);
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
      vkCmdDrawIndexed(cmd, d->count, d->instances, 0, 0, 0);
   } else {
      vkCmdDraw(cmd, d->count, d->instances, 0, 0);
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

/* GS input primitives in Vulkan vertex order (provoking vertex first),
 * restart splits segments, primitive IDs keep counting across restarts. */
static int
assemble(const struct shape *s, int prims[][6], int *nv)
{
   int seq[MAXV], n = 0, np = 0;
   int total = s->nidx ? s->nidx : s->nverts;
   for (int k = 0; k <= total; k++) {
      int v = k < total ? (s->nidx ? s->idx[k] : k) : RESTART;
      if (v != RESTART) {
         seq[n++] = v;
         continue;
      }
      switch (s->topology) {
      case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
         *nv = 1;
         for (int i = 0; i < n; i++)
            prims[np++][0] = seq[i];
         break;
      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: {
         int list = s->topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
         *nv = 2;
         for (int i = 0; i + 1 < n; i += list ? 2 : 1) {
            prims[np][0] = seq[i];
            prims[np++][1] = seq[i + 1];
         }
         break;
      }
      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: {
         int list = s->topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
         *nv = 4;
         for (int i = 0; i + 3 < n; i += list ? 4 : 1) {
            for (int j = 0; j < 4; j++)
               prims[np][j] = seq[i + j];
            np++;
         }
         break;
      }
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
         *nv = 6;
         for (int i = 0; i + 5 < n; i += 6) {
            for (int j = 0; j < 6; j++)
               prims[np][j] = seq[i + j];
            np++;
         }
         break;
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
         /* Only vertices 0, 2, 4 (the triangle) are read by the GS. */
         *nv = 6;
         for (int i = 0; 2 * i + 4 < n; i++) {
            int e = !(i & 1);
            prims[np][0] = seq[2 * i + (e ? 0 : 2)];
            prims[np][2] = seq[2 * i + (e ? 2 : 0)];
            prims[np][4] = seq[2 * i + 4];
            np++;
         }
         break;
      default:
         *nv = 3;
         for (int i = 0; i + 2 < n; i++) {
            int *o = prims[np];
            if (s->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
               if (i % 3)
                  continue;
               o[0] = seq[i], o[1] = seq[i + 1], o[2] = seq[i + 2];
            } else if (s->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
               o[0] = seq[i + (i & 1)], o[1] = seq[i + !(i & 1)];
               o[2] = seq[i + 2];
            } else {
               o[0] = seq[i + 1], o[1] = seq[i + 2], o[2] = seq[0];
            }
            np++;
         }
         break;
      }
      n = 0;
   }
   return np;
}

struct out {
   int n, lines;
   float v[MAXO][2];
};

static void
put(struct out *o, const float *p, float dx, float dy)
{
   o->v[o->n][0] = p[0] + dx;
   o->v[o->n][1] = p[1] + dy;
   o->n++;
}

/* The primitives each GS mode emits, as a list. */
static void
model(enum mode m, const struct shape *s, int instances, struct out *o)
{
   int prims[MAXV * 4][6] = {{0}}, nv;
   int np = assemble(s, prims, &nv);
   memset(o, 0, sizeof(*o));
   o->lines = m == PASS_LINE || m == TRI_WIRE || m == ADJ_LINE;
   for (int inst = 0; inst < instances; inst++) {
      float q[MAXV][2];
      for (int i = 0; i < s->nverts; i++) {
         q[i][0] = s->xy[i][0] + 0.25f * inst;
         q[i][1] = s->xy[i][1];
      }
      for (int p = 0; p < np; p++) {
         const float *a = q[prims[p][0]], *b = q[prims[p][1]],
                     *c = q[prims[p][2 % nv]];
         const float h = 3.0f / 32.0f;
         switch (m) {
         case PASS_TRI:
         case LAYER:
            put(o, a, 0, 0), put(o, b, 0, 0), put(o, c, 0, 0);
            break;
         case PASS_LINE:
            put(o, a, 0, 0), put(o, b, 0, 0);
            break;
         case POINT_QUAD:
            put(o, a, -h, -h), put(o, a, h, -h), put(o, a, -h, h);
            put(o, a, h, -h), put(o, a, -h, h), put(o, a, h, h);
            break;
         case TRI_WIRE:
            put(o, a, 0, 0), put(o, b, 0, 0), put(o, b, 0, 0);
            put(o, c, 0, 0), put(o, c, 0, 0), put(o, a, 0, 0);
            break;
         case TWO_TRIS:
            put(o, a, 0, 0), put(o, b, 0, 0), put(o, c, 0, 0);
            put(o, a, 0.125f, 0.125f), put(o, b, 0.125f, 0.125f);
            put(o, c, 0.125f, 0.125f);
            break;
         case STRIP4:
            put(o, a, 0, 0), put(o, b, 0, 0), put(o, c, 0, 0);
            put(o, b, 0, 0), put(o, c, 0, 0), put(o, c, 0.25f, 0);
            break;
         case INVOC3:
            for (int i = 0; i < 3; i++)
               put(o, a, 0, 0.25f * i), put(o, b, 0, 0.25f * i),
                  put(o, c, 0, 0.25f * i);
            break;
         case PRIMID:
            if (!(p & 1))
               put(o, a, 0, 0), put(o, b, 0, 0), put(o, c, 0, 0);
            break;
         case ADJ_TRI:
            put(o, q[prims[p][0]], 0, 0), put(o, q[prims[p][2]], 0, 0);
            put(o, q[prims[p][4]], 0, 0);
            break;
         case ADJ_LINE:
            put(o, q[prims[p][1]], 0, 0), put(o, q[prims[p][2]], 0, 0);
            break;
         default:
            break;
         }
      }
   }
}

static VkBuffer
vbuf(struct dx7 *t, const float (*xy)[2], int n)
{
   static float v[MAXO][4];
   for (int i = 0; i < n; i++) {
      v[i][0] = xy[i][0];
      v[i][1] = xy[i][1];
      v[i][2] = 1.0f;
      v[i][3] = 0.0f;
   }
   VkBuffer b;
   VkDeviceMemory m;
   dx7_buffer(t, sizeof(float) * 4 * (n ? n : 1),
              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, v, &b, &m);
   return b;
}

static VkPipeline
pipe_of(struct dx7 *t, VkPrimitiveTopology topo, int gs_mode, int restart)
{
   struct dx7_pipe_desc d = {
      .vs = vs_geom, .vs_size = sizeof(vs_geom),
      .fs = fs_geom, .fs_size = sizeof(fs_geom),
      .gs = gs_mode >= 0 ? gs_code[gs_mode] : NULL,
      .gs_size = gs_mode >= 0 ? gs_size[gs_mode] : 0,
      .topology = topo,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = VK_CULL_MODE_NONE,
      .dynamic_viewport = 1,
      .primitive_restart = restart,
   };
   return dx7_pipeline(t, &d);
}

static int
run_case(struct dx7 *t, const char *name, enum mode m, const struct shape *s,
         int indirect, int instances)
{
   static uint8_t ref[RT_W * RT_H * 4];
   static struct out o;
   model(m, s, instances, &o);

   VkPipeline rp = pipe_of(t,
                           o.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                   : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                           -1, 0);
   VkBuffer rb = vbuf(t, (const float (*)[2])o.v, o.n);
   struct draw rd = {.vb = rb, .count = o.n, .instances = 1};
   dx7_run(t, rp, rb, rec, &rd);
   memcpy(ref, t->px, sizeof(ref));
   int ref_red = count_red(t);
   vkDestroyPipeline(t->dev, rp, NULL);

   VkBuffer vb = vbuf(t, (const float (*)[2])s->xy, s->nverts);
   struct draw td = {.vb = vb, .count = s->nidx ? s->nidx : s->nverts,
                     .instances = instances};
   if (s->nidx) {
      VkDeviceMemory im;
      dx7_buffer(t, sizeof(uint16_t) * s->nidx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 s->idx, &td.ib, &im);
      td.indexed = 1;
   }
   if (indirect) {
      uint32_t cmdw[5] = {td.count, instances, 0, 0, 0};
      VkDeviceMemory am;
      dx7_buffer(t, sizeof(cmdw), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, cmdw,
                 &td.indirect, &am);
   }
   VkPipeline tp = pipe_of(t, s->topology, m, s->nidx != 0);
   dx7_run(t, tp, vb, rec, &td);
   vkDestroyPipeline(t->dev, tp, NULL);

   int bad = 0, fx = -1, fy = -1;
   for (int i = 0; i < RT_W * RT_H; i++)
      if (memcmp(ref + i * 4, t->px + i * 4, 4) && !bad++)
         fx = i % RT_W, fy = i / RT_W;
   int red = count_red(t);
   int fail = bad || !ref_red;
   printf("CASE %s outv=%d ref_red=%d red=%d mismatch=%d", name, o.n, ref_red,
          red, bad);
   if (bad) {
      const uint8_t *p = dx7_px(t, fx, fy), *q = ref + (fy * RT_W + fx) * 4;
      printf(" first=(%d,%d) got=%u %u %u want=%u %u %u", fx, fy, p[0], p[1],
             p[2], q[0], q[1], q[2]);
   }
   printf(" %s\n", fail ? "FAIL" : "PASS");
   return fail;
}

static const struct shape list3 = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 9,
   {{-0.83f, -0.71f}, {-0.12f, -0.77f}, {-0.46f, 0.07f},
    {0.11f, -0.62f}, {0.47f, 0.21f}, {0.86f, -0.66f},
    {-0.72f, 0.31f}, {0.69f, 0.36f}, {0.03f, 0.87f}},
};

static const struct shape small_list = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 6,
   {{-0.83f, -0.71f}, {-0.42f, -0.77f}, {-0.66f, -0.27f},
    {-0.61f, 0.12f}, {-0.23f, 0.21f}, {-0.54f, 0.46f}},
};

static const struct shape strip5 = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 5,
   {{-0.81f, 0.17f}, {-0.68f, 0.79f}, {-0.29f, 0.23f}, {0.02f, 0.83f},
    {0.41f, 0.13f}},
};

static const struct shape fan6 = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, 7,
   {{0.01f, -0.03f}, {0.71f, 0.09f}, {0.52f, 0.61f}, {-0.11f, 0.74f},
    {-0.63f, 0.38f}, {-0.69f, -0.31f}, {-0.21f, 0.52f}},
};

static const struct shape strip_restart = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 8,
   {{-0.87f, -0.83f}, {-0.79f, -0.13f}, {-0.31f, -0.77f}, {-0.23f, -0.09f},
    {0.13f, 0.19f}, {0.21f, 0.88f}, {0.67f, 0.23f}, {0.83f, 0.81f}},
   9,
   {0, 1, 2, 3, RESTART, 4, 5, 6, 7},
};

static const struct shape strip7_restart = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 9,
   {{-0.91f, -0.83f}, {-0.79f, -0.33f}, {-0.51f, -0.77f}, {-0.43f, -0.29f},
    {-0.13f, -0.81f}, {0.13f, 0.19f}, {0.21f, 0.88f}, {0.67f, 0.23f},
    {0.83f, 0.81f}},
   10,
   {0, 1, 2, 3, 4, RESTART, 5, 6, 7, 8},
};

static const struct shape lines4 = {
   VK_PRIMITIVE_TOPOLOGY_LINE_LIST, 8,
   {{-0.83f, -0.71f}, {0.72f, -0.52f}, {-0.46f, 0.07f}, {0.41f, 0.83f},
    {0.11f, -0.62f}, {-0.37f, 0.61f}, {0.86f, 0.66f}, {-0.72f, 0.31f}},
};

static const struct shape linestrip6 = {
   VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 6,
   {{-0.83f, -0.71f}, {0.72f, -0.52f}, {-0.46f, 0.07f}, {0.41f, 0.83f},
    {0.11f, -0.62f}, {-0.37f, 0.61f}},
};

static const struct shape points5 = {
   VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 5,
   {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.0f}, {-0.5f, 0.5f},
    {0.5f, 0.5f}},
};

static const struct shape tri_adj = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, 12,
   {{-0.83f, -0.71f}, {0.9f, 0.9f}, {-0.12f, -0.77f}, {0.9f, 0.9f},
    {-0.46f, 0.07f}, {0.9f, 0.9f},
    {0.11f, -0.62f}, {-0.9f, 0.9f}, {0.47f, 0.21f}, {-0.9f, 0.9f},
    {0.86f, -0.66f}, {-0.9f, 0.9f}},
};

static const struct shape tri_strip_adj = {
   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, 10,
   {{-0.81f, 0.17f}, {0.9f, -0.9f}, {-0.68f, 0.79f}, {0.9f, -0.9f},
    {-0.29f, 0.23f}, {0.9f, -0.9f}, {0.02f, 0.83f}, {0.9f, -0.9f},
    {0.41f, 0.13f}, {0.9f, -0.9f}},
};

static const struct shape line_adj = {
   VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, 8,
   {{0.9f, 0.9f}, {-0.83f, -0.71f}, {0.72f, -0.52f}, {0.9f, 0.9f},
    {0.9f, 0.9f}, {-0.46f, 0.07f}, {0.41f, 0.83f}, {0.9f, 0.9f}},
};

static const struct shape line_strip_adj = {
   VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, 6,
   {{0.9f, 0.9f}, {-0.83f, -0.71f}, {0.72f, -0.52f}, {-0.46f, 0.07f},
    {0.41f, 0.83f}, {0.9f, 0.9f}},
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
   want.geometryShader = VK_TRUE;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.geometryShader) {
      printf("FAIL feature not exposed\n");
      return 1;
   }
   printf("limits invocations=%u in=%u out=%u outv=%u total=%u\n",
          t.props.limits.maxGeometryShaderInvocations,
          t.props.limits.maxGeometryInputComponents,
          t.props.limits.maxGeometryOutputComponents,
          t.props.limits.maxGeometryOutputVertices,
          t.props.limits.maxGeometryTotalOutputComponents);

   int fails = 0;
   fails += run_case(&t, "tri_list_pass", PASS_TRI, &list3, 0, 1);
   fails += run_case(&t, "tri_strip_pass", PASS_TRI, &strip5, 0, 1);
   fails += run_case(&t, "tri_fan_pass", PASS_TRI, &fan6, 0, 1);
   fails += run_case(&t, "tri_strip_restart_pass", PASS_TRI, &strip_restart, 0, 1);
   fails += run_case(&t, "tri_list_indirect", PASS_TRI, &list3, 1, 1);
   fails += run_case(&t, "tri_strip_restart_indexed_indirect", PASS_TRI,
                     &strip_restart, 1, 1);
   fails += run_case(&t, "tri_list_instanced3", PASS_TRI, &small_list, 0, 3);
   fails += run_case(&t, "tri_list_instanced3_indirect", PASS_TRI, &small_list,
                     1, 3);
   fails += run_case(&t, "line_list_pass", PASS_LINE, &lines4, 0, 1);
   fails += run_case(&t, "line_strip_pass", PASS_LINE, &linestrip6, 0, 1);
   fails += run_case(&t, "point_to_quad", POINT_QUAD, &points5, 0, 1);
   fails += run_case(&t, "tri_to_lines_endprim", TRI_WIRE, &list3, 0, 1);
   fails += run_case(&t, "two_tris_endprim", TWO_TRIS, &list3, 0, 1);
   fails += run_case(&t, "strip_out4", STRIP4, &list3, 0, 1);
   fails += run_case(&t, "invocations3", INVOC3, &small_list, 0, 1);
   fails += run_case(&t, "primid_list", PRIMID, &list3, 0, 1);
   fails += run_case(&t, "primid_strip_restart", PRIMID, &strip7_restart, 0, 1);
   fails += run_case(&t, "primid_instanced", PRIMID, &small_list, 0, 2);
   fails += run_case(&t, "adj_tri_list", ADJ_TRI, &tri_adj, 0, 1);
   fails += run_case(&t, "adj_tri_strip", ADJ_TRI, &tri_strip_adj, 0, 1);
   fails += run_case(&t, "adj_line_list", ADJ_LINE, &line_adj, 0, 1);
   fails += run_case(&t, "adj_line_strip", ADJ_LINE, &line_strip_adj, 0, 1);
   fails += run_case(&t, "layer_write", LAYER, &list3, 0, 1);

   printf("GEOMETRY_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
