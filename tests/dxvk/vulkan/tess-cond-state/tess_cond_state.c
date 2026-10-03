/* Tessellation draw skipped by conditional rendering must not eat the state
 * of the next draw (patch 093).
 *
 * Every case draws a full-screen patch three times in one render pass:
 *   1. unconditional, green
 *   2. state change to red, then the same draw inside conditional rendering
 *      that skips it
 *   3. unconditional again, no state change: must be red
 * The skipped draw is the first one that sees the red state. When its state
 * emission is predicated together with the launch (092), the dirty state is
 * consumed but never written, and draw 3 renders green.
 *
 *  push      FS color from push constants (FS push uniforms state)
 *  pipeline  pipeline change to a red-only FS (FS shader state)
 *  inverted  as push, inverted predicate
 *  run       as push, predicate passes: draw 2 runs (control)
 *
 * Output: CASE lines, RESULT PASS/FAIL. usage: tess_cond_state <icd>
 * SPIR-V regen: sh build_spv.sh
 */
#include "../dx7_harness.h"
#include "tess_cond_state_spv.h"

static PFN_vkCmdBeginConditionalRenderingEXT CmdBeginCond;
static PFN_vkCmdEndConditionalRenderingEXT CmdEndCond;

static VkPhysicalDeviceConditionalRenderingFeaturesEXT cond_feat = {
   .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,
   .conditionalRendering = VK_TRUE};
static const char *exts[] = {VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME};

static void
hook(struct dx7 *t, VkDeviceCreateInfo *dci)
{
   (void)t;
   dci->pNext = &cond_feat;
   dci->enabledExtensionCount = 1;
   dci->ppEnabledExtensionNames = exts;
}

struct rec {
   VkPipeline second; /* bound before draw 2, or VK_NULL_HANDLE */
   VkBuffer pred;
   int inverted;
};

static void
push(struct dx7 *t, VkCommandBuffer cmd, float r, float g)
{
   const float c[4] = {r, g, 0.0f, 1.0f};
   vkCmdPushConstants(cmd, t->layout,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(c), c);
}

static void
record(struct dx7 *t, VkCommandBuffer cmd, void *data)
{
   const struct rec *r = data;

   push(t, cmd, 0.0f, 1.0f);
   vkCmdDraw(cmd, 3, 1, 0, 0);

   if (r->second)
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r->second);
   push(t, cmd, 1.0f, 0.0f);
   VkConditionalRenderingBeginInfoEXT cri = {
      .sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
      .buffer = r->pred,
      .flags = r->inverted ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0};
   CmdBeginCond(cmd, &cri);
   vkCmdDraw(cmd, 3, 1, 0, 0);
   CmdEndCond(cmd);

   vkCmdDraw(cmd, 3, 1, 0, 0);
}

static int
all_red(int x, int y, void *d)
{
   (void)x, (void)y, (void)d;
   return 1;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <libvulkan_panfrost.so>\n", argv[0]);
      return 1;
   }
   struct dx7 t;
   VkPhysicalDeviceFeatures want = {.tessellationShader = VK_TRUE};
   dx7_device_hook = hook;
   dx7_init(&t, argv[1], &want);
   if (!t.feats.tessellationShader) {
      printf("SKIP no tessellationShader\nRESULT PASS\n");
      return 0;
   }

   void *h = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkGetDeviceProcAddr gdpa =
      (PFN_vkGetDeviceProcAddr)gipa(t.inst, "vkGetDeviceProcAddr");
   CmdBeginCond = (PFN_vkCmdBeginConditionalRenderingEXT)gdpa(
      t.dev, "vkCmdBeginConditionalRenderingEXT");
   CmdEndCond = (PFN_vkCmdEndConditionalRenderingEXT)gdpa(
      t.dev, "vkCmdEndConditionalRenderingEXT");
   if (!CmdBeginCond || !CmdEndCond) {
      printf("FAIL missing conditional rendering entry points\n");
      return 1;
   }

   const float tri[3][4] = {{-1, -1, 0, 1}, {3, -1, 0, 1}, {-1, 3, 0, 1}};
   VkBuffer vbuf;
   VkDeviceMemory vmem;
   dx7_buffer(&t, sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, tri, &vbuf,
              &vmem);
   const uint32_t zero = 0, one = 1;
   VkBuffer pred0, pred1;
   VkDeviceMemory pmem0, pmem1;
   dx7_buffer(&t, 4, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT, &zero,
              &pred0, &pmem0);
   dx7_buffer(&t, 4, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT, &one,
              &pred1, &pmem1);

   struct dx7_pipe_desc d = {
      .vs = vs_pos, .vs_size = sizeof(vs_pos),
      .fs = fs_push, .fs_size = sizeof(fs_push),
      .tcs = tcs_levels, .tcs_size = sizeof(tcs_levels),
      .tes = tes_tri, .tes_size = sizeof(tes_tri),
      .patch_control_points = 3,
      .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
      .polygon_mode = VK_POLYGON_MODE_FILL};
   VkPipeline p_push = dx7_pipeline(&t, &d);
   d.fs = fs_red;
   d.fs_size = sizeof(fs_red);
   VkPipeline p_red = dx7_pipeline(&t, &d);

   const struct {
      const char *name;
      struct rec r;
   } cases[] = {
      {"push", {VK_NULL_HANDLE, pred0, 0}},
      {"pipeline", {p_red, pred0, 0}},
      {"inverted", {VK_NULL_HANDLE, pred1, 1}},
      {"run", {VK_NULL_HANDLE, pred1, 0}},
   };
   int fails = 0;
   for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      struct rec r = cases[i].r;
      dx7_run(&t, p_push, vbuf, record, &r);
      fails += dx7_check(&t, cases[i].name, all_red, NULL);
   }
   printf("RESULT %s\n", fails ? "FAIL" : "PASS");
   return fails != 0;
}
