/* DX7 clip/cull distance draw test. Expected image is computed from the
 * geometry; the GPU draws, the host only compares the final readback. */
#include "../dx7_harness.h"
#include "clip_cull_spv.h"

static const float full_quad[] = {
   -1, -1, 1, 0, 1, -1, 1, 0, -1, 1, 1, 0,
   1, -1, 1, 0, 1, 1, 1, 0, -1, 1, 1, 0,
};

/* Left half: two triangles, all cull values negative (must be culled).
 * Right half: one triangle with cull = -1, -1, +1 (must be drawn fully even
 * where the interpolated cull value is negative). */
static const float cull_geo[] = {
   -1, -1, -1, 0, 0, -1, -1, 0, -1, 1, -1, 0,
   0, -1, -1, 0, 0, 1, -1, 0, -1, 1, -1, 0,
   0, -1, -1, 0, 3, -1, -1, 0, 0, 5, 1, 0,
};

/* Horizontal lines through pixel rows 16 (cull -1, -1: culled) and 48
 * (cull -1, +1: drawn over its full length). */
#define ROW_NDC(r) (((r) + 0.5f) / (RT_H / 2) - 1.0f)
static const float line_geo[] = {
   -1, ROW_NDC(16), -1, 0, 1, ROW_NDC(16), -1, 0,
   -1, ROW_NDC(48), -1, 0, 1, ROW_NDC(48), 1, 0,
};

enum { E_ALL, E_RIGHT, E_BOTTOM, E_BR, E_LINE48, E_LINES_RIGHT };

static int
line_px(int x, int y, int row, int xmin)
{
   if (y != row || x < xmin)
      return 0;
   return (x < 2 || x > RT_W - 3) ? -1 : 1;
}

static int
expect(int x, int y, void *data)
{
   switch (*(int *)data) {
   case E_ALL: return 1;
   case E_RIGHT: return x >= RT_W / 2;
   case E_BOTTOM: return y >= RT_H / 2;
   case E_LINE48: return line_px(x, y, 48, 0);
   case E_LINES_RIGHT:
      return line_px(x, y, 48, RT_W / 2) | line_px(x, y, 16, RT_W / 2);
   default: return x >= RT_W / 2 && y >= RT_H / 2;
   }
}

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   vkCmdDraw(cmd, *(uint32_t *)data, 1, 0, 0);
}

#define SPV(n) n, sizeof(n)

struct cc_case {
   const char *name;
   const uint32_t *vs;
   size_t vs_size;
   const uint32_t *fs;
   size_t fs_size;
   int geo; /* 0 = quad, 1 = cull triangles, 2 = lines */
   int expect;
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
   want.shaderClipDistance = VK_TRUE;
   want.shaderCullDistance = VK_TRUE;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.shaderClipDistance || !t.feats.shaderCullDistance ||
       t.props.limits.maxClipDistances < 8 ||
       t.props.limits.maxCullDistances < 8 ||
       t.props.limits.maxCombinedClipAndCullDistances < 8) {
      printf("FAIL feature/limits not exposed\n");
      return 1;
   }

   VkBuffer qbuf, cbuf, lbuf;
   VkDeviceMemory qmem, cmem, lmem;
   dx7_buffer(&t, sizeof(full_quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              full_quad, &qbuf, &qmem);
   dx7_buffer(&t, sizeof(cull_geo), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              cull_geo, &cbuf, &cmem);
   dx7_buffer(&t, sizeof(line_geo), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              line_geo, &lbuf, &lmem);
   const VkBuffer bufs[3] = {qbuf, cbuf, lbuf};
   const uint32_t counts[3] = {6, 9, 4};

   const struct cc_case cases[] = {
      {"none", SPV(vs_none), SPV(fs_color), 0, E_ALL},
      {"none_cullgeo", SPV(vs_none), SPV(fs_color), 1, E_ALL},
      {"clip1_x", SPV(vs_clip1_x), SPV(fs_color), 0, E_RIGHT},
      {"clip2_xy", SPV(vs_clip2_xy), SPV(fs_color), 0, E_BR},
      {"clip6_x5", SPV(vs_clip6_x5), SPV(fs_color), 0, E_RIGHT},
      {"clip8_y7", SPV(vs_clip8_y7), SPV(fs_color), 0, E_BOTTOM},
      {"clip1_fs_read", SPV(vs_clip1_x), SPV(fs_clip_read), 0, E_RIGHT},
      {"cull1", SPV(vs_cull1), SPV(fs_color), 1, E_RIGHT},
      {"cull5_i4", SPV(vs_cull5_i4), SPV(fs_color), 1, E_RIGHT},
      {"cull8_i7", SPV(vs_cull8_i7), SPV(fs_color), 1, E_RIGHT},
      {"clip4y3_cull4i2", SPV(vs_clip4y3_cull4i2), SPV(fs_color), 1, E_BR},
      {"clip1_x_lines", SPV(vs_clip1_x), SPV(fs_color), 2, E_LINES_RIGHT},
      {"cull1_lines", SPV(vs_cull1), SPV(fs_color), 2, E_LINE48},
   };

   int fails = 0;
   for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      const struct cc_case *c = &cases[i];
      struct dx7_pipe_desc d = {
         .vs = c->vs, .vs_size = c->vs_size,
         .fs = c->fs, .fs_size = c->fs_size,
         .topology = c->geo == 2 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                 : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .polygon_mode = VK_POLYGON_MODE_FILL,
         .cull_mode = VK_CULL_MODE_NONE,
      };
      VkPipeline p = dx7_pipeline(&t, &d);
      uint32_t count = counts[c->geo];
      dx7_run(&t, p, bufs[c->geo], rec, &count);
      int e = c->expect;
      fails += dx7_check(&t, c->name, expect, &e);
      vkDestroyPipeline(t.dev, p, NULL);
   }
   printf("CLIP_CULL_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
