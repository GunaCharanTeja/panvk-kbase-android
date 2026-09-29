/* Long-run queue submission stress: N submits, each a command buffer with
 * RPS render passes (clear + one triangle draw through the tiler) and a
 * readback copy, followed by a fence wait. Every 1000 submits the target is
 * checked (red triangle over blue clear). Reproduces the kbase tiler-heap
 * exhaustion hang (subqueue-0 queue timeout / VK_ERROR_DEVICE_LOST after a
 * fixed amount of tiler work) seen in long dEQP-VK.api.copy_and_blit runs.
 *
 * usage: submit_stress <icd.so> [submits=50000] [render-passes-per-submit=1]
 * build: gcc -O2 -Wall -o submit_stress submit_stress.c -ldl
 */
#include <time.h>
#include "../dx7_harness.h"
#include "../geometry/geometry_spv.h"

static double
now_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <icd> [submits] [rps]\n", argv[0]);
      return 2;
   }
   long n = argc > 2 ? atol(argv[2]) : 50000;
   int rps = argc > 3 ? atoi(argv[3]) : 1;
   struct dx7 t;
   VkPhysicalDeviceFeatures want = {0};
   dx7_init(&t, argv[1], &want);

   /* a.z = red channel; one triangle covering the lower-left half. */
   const float tri[3][4] = {{-1, -1, 1, 1}, {1, -1, 1, 1}, {-1, 1, 1, 1}};
   VkBuffer vb;
   VkDeviceMemory vmem;
   dx7_buffer(&t, sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, tri, &vb,
              &vmem);
   struct dx7_pipe_desc pd = {
      .vs = vs_geom, .vs_size = sizeof(vs_geom),
      .fs = fs_geom, .fs_size = sizeof(fs_geom),
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .polygon_mode = VK_POLYGON_MODE_FILL,
   };
   VkPipeline pipe = dx7_pipeline(&t, &pd);

   VkClearValue clear = {.color = {{0.0f, 0.0f, 1.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                 .renderPass = t.rp,
                                 .framebuffer = t.fb,
                                 .renderArea = {{0, 0}, {RT_W, RT_H}},
                                 .clearValueCount = 1,
                                 .pClearValues = &clear};
   VkBufferImageCopy region = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,
                                                    0, 0, 1},
                               .imageExtent = {RT_W, RT_H, 1}};
   VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                         .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &t.cmd};
   double t0 = now_s();

   for (long i = 0; i < n; i++) {
      CK(vkResetFences(t.dev, 1, &t.fence), "ResetFence");
      CK(vkResetCommandBuffer(t.cmd, 0), "ResetCmd");
      VkCommandBufferBeginInfo bbi = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
      CK(vkBeginCommandBuffer(t.cmd, &bbi), "Begin");
      for (int r = 0; r < rps; r++) {
         vkCmdBeginRenderPass(t.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
         vkCmdBindPipeline(t.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
         VkDeviceSize off = 0;
         vkCmdBindVertexBuffers(t.cmd, 0, 1, &vb, &off);
         vkCmdDraw(t.cmd, 3, 1, 0, 0);
         vkCmdEndRenderPass(t.cmd);
      }
      vkCmdPipelineBarrier(t.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, NULL,
                           0, NULL);
      vkCmdCopyImageToBuffer(t.cmd, t.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             t.rbuf, 1, &region);
      CK(vkEndCommandBuffer(t.cmd), "End");
      CK(vkQueueSubmit(t.queue, 1, &si, t.fence), "Submit");
      VkResult r = vkWaitForFences(t.dev, 1, &t.fence, VK_TRUE,
                                   30ull * 1000000000ull);
      if (r != VK_SUCCESS) {
         printf("FAIL submit=%ld render_passes=%ld wait r=%d\n", i,
                i * (long)rps, r);
         return 1;
      }
      if ((i + 1) % 1000 == 0 || i + 1 == n) {
         int ok = dx7_is_red(dx7_px(&t, 4, 4)) &&
                  dx7_is_blue(dx7_px(&t, RT_W - 4, RT_H - 4));
         printf("submit=%ld render_passes=%ld %.1fs %s\n", i + 1,
                (i + 1) * (long)rps, now_s() - t0, ok ? "ok" : "BADPIXELS");
         if (!ok)
            return 1;
      }
   }
   printf("SUBMIT_STRESS PASS submits=%ld rps=%d\n", n, rps);
   return 0;
}
