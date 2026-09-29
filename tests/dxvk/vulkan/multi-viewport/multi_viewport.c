/* DX7 multiViewport draw test. No stage can write ViewportIndex here
 * (geometryShader and shaderOutputViewportIndex are not exposed), so every
 * primitive must use viewport 0 and scissor 0 while the other array entries
 * are accepted and ignored. */
#include "../dx7_harness.h"
#include "../clip-cull/clip_cull_spv.h"

static const float full_quad[] = {
   -1, -1, 1, 0, 1, -1, 1, 0, -1, 1, 1, 0,
   1, -1, 1, 0, 1, 1, 1, 0, -1, 1, 1, 0,
};

struct rect {
   int x0, y0, x1, y1;
};

static int
expect(int x, int y, void *data)
{
   const struct rect *r = data;
   return x >= r->x0 && x < r->x1 && y >= r->y0 && y < r->y1;
}

struct dyn {
   int count;
   VkViewport vp[4];
   VkRect2D sc[4];
   int second_first;
   VkViewport vp2[3];
   VkRect2D sc2[3];
};

static void
rec(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   const struct dyn *d = data;
   if (d && d->count) {
      vkCmdSetViewport(cmd, 0, d->count, d->vp);
      vkCmdSetScissor(cmd, 0, d->count, d->sc);
      if (d->second_first) {
         vkCmdSetViewport(cmd, d->second_first, 3, d->vp2);
         vkCmdSetScissor(cmd, d->second_first, 3, d->sc2);
      }
   }
   vkCmdDraw(cmd, 6, 1, 0, 0);
}

#define VP(x, y, w, h) {x, y, w, h, 0.0f, 1.0f}
#define SC(x, y, w, h) {{x, y}, {w, h}}
#define FULL_VP VP(0, 0, RT_W, RT_H)
#define FULL_SC SC(0, 0, RT_W, RT_H)

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd>\n", argv[0]);
      return 2;
   }
   struct dx7 t;
   VkPhysicalDeviceFeatures want = {0};
   want.multiViewport = VK_TRUE;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.multiViewport || t.props.limits.maxViewports < 16) {
      printf("FAIL feature/limits not exposed\n");
      return 1;
   }

   VkBuffer qbuf;
   VkDeviceMemory qmem;
   dx7_buffer(&t, sizeof(full_quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              full_quad, &qbuf, &qmem);

   int fails = 0;
   struct dx7_pipe_desc d = {
      .vs = vs_none, .vs_size = sizeof(vs_none),
      .fs = fs_color, .fs_size = sizeof(fs_color),
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = VK_CULL_MODE_NONE,
   };

   /* static state, 16 viewports: viewport 0 = left half */
   {
      VkViewport vps[16];
      VkRect2D scs[16];
      for (int i = 0; i < 16; i++) {
         vps[i] = (VkViewport)FULL_VP;
         scs[i] = (VkRect2D)FULL_SC;
      }
      vps[0] = (VkViewport)VP(0, 0, RT_W / 2, RT_H);
      d.viewport_count = 16;
      d.viewports = vps;
      d.scissors = scs;
      VkPipeline p = dx7_pipeline(&t, &d);
      dx7_run(&t, p, qbuf, rec, NULL);
      struct rect r = {0, 0, RT_W / 2, RT_H};
      fails += dx7_check(&t, "static_vp16_vp0_left", expect, &r);
      vkDestroyPipeline(t.dev, p, NULL);

      /* scissor 0 = top half, viewport 0 full */
      vps[0] = (VkViewport)FULL_VP;
      scs[0] = (VkRect2D)SC(0, 0, RT_W, RT_H / 2);
      scs[1] = (VkRect2D)SC(0, 0, 1, 1);
      p = dx7_pipeline(&t, &d);
      dx7_run(&t, p, qbuf, rec, NULL);
      struct rect r2 = {0, 0, RT_W, RT_H / 2};
      fails += dx7_check(&t, "static_sc16_sc0_top", expect, &r2);
      vkDestroyPipeline(t.dev, p, NULL);
   }

   /* dynamic state, 4 viewports */
   d.viewport_count = 4;
   d.viewports = NULL;
   d.scissors = NULL;
   d.dynamic_viewport = 1;
   VkPipeline p = dx7_pipeline(&t, &d);
   {
      struct dyn s = {.count = 4,
                      .vp = {VP(RT_W / 2, RT_H / 2, RT_W / 2, RT_H / 2),
                             VP(0, 0, 8, 8), VP(0, 0, 8, 8), VP(0, 0, 8, 8)},
                      .sc = {FULL_SC, SC(0, 0, 8, 8), SC(0, 0, 8, 8),
                             SC(0, 0, 8, 8)}};
      dx7_run(&t, p, qbuf, rec, &s);
      struct rect r = {RT_W / 2, RT_H / 2, RT_W, RT_H};
      fails += dx7_check(&t, "dynamic_vp4_vp0_bottom_right", expect, &r);
   }
   {
      /* a later update of entries 1..3 must leave entry 0 in place */
      struct dyn s = {.count = 4,
                      .vp = {VP(0, RT_H / 2, RT_W / 2, RT_H / 2), FULL_VP,
                             FULL_VP, FULL_VP},
                      .sc = {FULL_SC, FULL_SC, FULL_SC, FULL_SC},
                      .second_first = 1,
                      .vp2 = {FULL_VP, FULL_VP, FULL_VP},
                      .sc2 = {SC(0, 0, 1, 1), SC(0, 0, 1, 1), SC(0, 0, 1, 1)}};
      dx7_run(&t, p, qbuf, rec, &s);
      struct rect r = {0, RT_H / 2, RT_W / 2, RT_H};
      fails += dx7_check(&t, "dynamic_first1_keeps_vp0", expect, &r);
   }
   {
      struct dyn s = {.count = 4,
                      .vp = {FULL_VP, VP(0, 0, 8, 8), VP(0, 0, 8, 8),
                             VP(0, 0, 8, 8)},
                      .sc = {SC(16, 16, 32, 32), SC(0, 0, 8, 8),
                             SC(0, 0, 8, 8), SC(0, 0, 8, 8)}};
      dx7_run(&t, p, qbuf, rec, &s);
      struct rect r = {16, 16, 48, 48};
      fails += dx7_check(&t, "dynamic_sc4_sc0_center", expect, &r);
   }
   vkDestroyPipeline(t.dev, p, NULL);

   printf("MULTI_VIEWPORT_FAILS=%d\n", fails);
   return fails ? 1 : 0;
}
