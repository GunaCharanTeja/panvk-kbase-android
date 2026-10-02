/* Gate 085 valid-API device regression.
 *
 * synchronization2 is enabled explicitly. One queue. Events never cross
 * queues. Every wait is legal:
 *  - host path: vkSetEvent after the host write and before submit
 *    (VUID-vkCmdWaitEvents2-pEvents-03840), srcStageMask includes HOST
 *    (03839/03841), compute consumes the written word. Reset and a second
 *    value run only after the first fence.
 *  - gpu path: CmdSetEvent2 then CmdWaitEvents2, both COMPUTE_SHADER, on
 *    q0. The set submit is fenced before the wait is submitted. Replay is
 *    CmdResetEvent2, a stage-only barrier, then the dispatch and
 *    CmdSetEvent2, after that completion.
 *  - ring: one SIMULTANEOUS_USE command buffer, 1700 color draws. v11
 *    packed render desc is 320 bytes. 1700 * 320 > 524288, so the pointer
 *    wraps inside the buffer. The counter is released per render, not per
 *    submit. Readback after each of two submits.
 *
 * Host-notify of an evicted group (KBASE_IOCTL_CS_EVENT_SIGNAL) is not
 * covered. A wait whose only stages are HOST or TOP_OF_PIPE emits no
 * SYNC_WAIT and is not a case here.
 *
 * Blocks on /tmp/085-go after printing READY so the runner can stamp
 * /proc/self/maps before any GPU work.
 *
 * usage: csf-event-regression <libvulkan_panfrost.so>
 *
 * Rebuild from the repo root (NDK r30, API 35). Triangle SPIR-V is the
 * existing tests/offscreen headers, included as-is; the copy shader is
 * inlined:
 *   aarch64-linux-android35-clang -std=c11 -O2 -Wall -Wextra -Werror \
 *     device/csf-event-regression.c -ldl -o csf-event-regression
 */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#include "../tests/offscreen/tri.frag.spv.h"
#include "../tests/offscreen/tri.vert.spv.h"

typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS) {                                                  \
         printf("FAIL %s r=%d line=%d\n", what, (int)_r, __LINE__);            \
         return 1;                                                             \
      }                                                                        \
   } while (0)

#define W 8
#define H 8
/* v11 packed: framebuffer 128 + render target 64 + tiler context 128 = 320.
 * cs_sync32_add of that size runs in the fragment finish, once per render,
 * not once per submit. Only draws inside one SIMULTANEOUS_USE command
 * buffer count. 1700 * 320 = 544000 > RENDER_DESC_RINGBUF_SIZE (524288):
 * one buffer wraps. */
#define RING_PASSES 1700
#define RING_SUBMITS 2
#define HOST_A 0x085000a1u
#define HOST_B 0x085000a2u
#define GPU_A 0x085000b1u
#define GPU_B 0x085000b2u

static const uint32_t copy_spv[] = {
   0x07230203u, 0x00010000u, 0x0008000bu, 0x00000016u, 0x00000000u, 0x00020011u,
   0x00000001u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu,
   0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0005000fu, 0x00000005u,
   0x00000004u, 0x6e69616du, 0x00000000u, 0x00060010u, 0x00000004u, 0x00000011u,
   0x00000001u, 0x00000001u, 0x00000001u, 0x00030003u, 0x00000002u, 0x000001c2u,
   0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00030005u, 0x00000007u,
   0x0074754fu, 0x00040006u, 0x00000007u, 0x00000000u, 0x00000076u, 0x00040005u,
   0x00000009u, 0x7074756fu, 0x00000000u, 0x00030005u, 0x0000000cu, 0x00706e49u,
   0x00040006u, 0x0000000cu, 0x00000000u, 0x00000076u, 0x00030005u, 0x0000000eu,
   0x00706e69u, 0x00030047u, 0x00000007u, 0x00000003u, 0x00050048u, 0x00000007u,
   0x00000000u, 0x00000023u, 0x00000000u, 0x00040047u, 0x00000009u, 0x00000021u,
   0x00000001u, 0x00040047u, 0x00000009u, 0x00000022u, 0x00000000u, 0x00030047u,
   0x0000000cu, 0x00000003u, 0x00050048u, 0x0000000cu, 0x00000000u, 0x00000023u,
   0x00000000u, 0x00040047u, 0x0000000eu, 0x00000021u, 0x00000000u, 0x00040047u,
   0x0000000eu, 0x00000022u, 0x00000000u, 0x00040047u, 0x00000015u, 0x0000000bu,
   0x00000019u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u,
   0x00040015u, 0x00000006u, 0x00000020u, 0x00000000u, 0x0003001eu, 0x00000007u,
   0x00000006u, 0x00040020u, 0x00000008u, 0x00000002u, 0x00000007u, 0x0004003bu,
   0x00000008u, 0x00000009u, 0x00000002u, 0x00040015u, 0x0000000au, 0x00000020u,
   0x00000001u, 0x0004002bu, 0x0000000au, 0x0000000bu, 0x00000000u, 0x0003001eu,
   0x0000000cu, 0x00000006u, 0x00040020u, 0x0000000du, 0x00000002u, 0x0000000cu,
   0x0004003bu, 0x0000000du, 0x0000000eu, 0x00000002u, 0x00040020u, 0x0000000fu,
   0x00000002u, 0x00000006u, 0x00040017u, 0x00000013u, 0x00000006u, 0x00000003u,
   0x0004002bu, 0x00000006u, 0x00000014u, 0x00000001u, 0x0006002cu, 0x00000013u,
   0x00000015u, 0x00000014u, 0x00000014u, 0x00000014u, 0x00050036u, 0x00000002u,
   0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x00050041u,
   0x0000000fu, 0x00000010u, 0x0000000eu, 0x0000000bu, 0x0004003du, 0x00000006u,
   0x00000011u, 0x00000010u, 0x00050041u, 0x0000000fu, 0x00000012u, 0x00000009u,
   0x0000000bu, 0x0003003eu, 0x00000012u, 0x00000011u, 0x000100fdu, 0x00010038u,
};

static double
now_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void
stamp(const char *what)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   printf("T %ld.%09ld %s\n", (long)ts.tv_sec, ts.tv_nsec, what);
}

struct gpu
{
   icd_gipa_fn gipa;
   VkInstance inst;
   VkPhysicalDevice phys;
   VkDevice dev;
   VkQueue q0;
   uint32_t qi;
   uint32_t mem_host, mem_local;
   VkCommandPool pool;
   PFN_vkGetDeviceProcAddr gdpa;
   PFN_vkCreateEvent CreateEvent;
   PFN_vkDestroyEvent DestroyEvent;
   PFN_vkSetEvent SetEvent;
   PFN_vkResetEvent ResetEvent;
   PFN_vkGetEventStatus GetEventStatus;
   PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
   PFN_vkBeginCommandBuffer BeginCommandBuffer;
   PFN_vkEndCommandBuffer EndCommandBuffer;
   PFN_vkResetCommandBuffer ResetCommandBuffer;
   PFN_vkCmdWaitEvents2 CmdWaitEvents2;
   PFN_vkCmdSetEvent2 CmdSetEvent2;
   PFN_vkCmdResetEvent2 CmdResetEvent2;
   PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
   PFN_vkCmdPipelineBarrier2 CmdPipelineBarrier2;
   PFN_vkCmdBindPipeline CmdBindPipeline;
   PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
   PFN_vkCmdDispatch CmdDispatch;
   PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
   PFN_vkCmdEndRenderPass CmdEndRenderPass;
   PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
   PFN_vkCmdDraw CmdDraw;
   PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
   PFN_vkCreateFence CreateFence;
   PFN_vkWaitForFences WaitForFences;
   PFN_vkResetFences ResetFences;
   PFN_vkQueueSubmit QueueSubmit;
   PFN_vkCreateBuffer CreateBuffer;
   PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkBindBufferMemory BindBufferMemory;
   PFN_vkMapMemory MapMemory;
   PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
   PFN_vkCreateDescriptorPool CreateDescriptorPool;
   PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
   PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
   PFN_vkCreateShaderModule CreateShaderModule;
   PFN_vkCreatePipelineLayout CreatePipelineLayout;
   PFN_vkCreateComputePipelines CreateComputePipelines;
   PFN_vkCreateRenderPass CreateRenderPass;
   PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
   PFN_vkCreateImage CreateImage;
   PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
   PFN_vkBindImageMemory BindImageMemory;
   PFN_vkCreateImageView CreateImageView;
   PFN_vkCreateFramebuffer CreateFramebuffer;
};

static int
proc(struct gpu *g, const char *name, void *dst)
{
   PFN_vkVoidFunction p = g->gdpa(g->dev, name);
   if (!p) {
      printf("FAIL missing %s\n", name);
      return 1;
   }
   memcpy(dst, &p, sizeof(p));
   return 0;
}

struct buf
{
   VkBuffer b;
   VkDeviceMemory m;
   void *p;
};

static int
make_buf(struct gpu *g, VkDeviceSize sz, VkBufferUsageFlags usage, int host,
         struct buf *o)
{
   VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size = sz,
                            .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
   CK(g->CreateBuffer(g->dev, &bi, NULL, &o->b), "CreateBuffer");
   VkMemoryRequirements mr;
   g->GetBufferMemoryRequirements(g->dev, o->b, &mr);
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex = host ? g->mem_host : g->mem_local};
   CK(g->AllocateMemory(g->dev, &mai, NULL, &o->m), "AllocateMemory");
   CK(g->BindBufferMemory(g->dev, o->b, o->m, 0), "BindBufferMemory");
   o->p = NULL;
   if (host)
      CK(g->MapMemory(g->dev, o->m, 0, sz, 0, &o->p), "MapMemory");
   return 0;
}

static int
wait_fence(struct gpu *g, VkFence f, double t0, const char *what)
{
   VkResult wr = VK_TIMEOUT;
   for (int k = 0; k < 15 && wr == VK_TIMEOUT; k++)
      wr = g->WaitForFences(g->dev, 1, &f, VK_TRUE, 200000000ull);
   printf("FENCE %s r=%d dt=%.3f\n", what, (int)wr, now_s() - t0);
   if (wr == VK_TIMEOUT)
      return 3;
   return wr == VK_SUCCESS ? 0 : 1;
}

static void
host_avail(struct gpu *g, VkCommandBuffer cmd, VkBuffer b)
{
   VkBufferMemoryBarrier bb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = 4};
   g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1,
                         &bb, 0, NULL);
}

static void
shader_to_host(struct gpu *g, VkCommandBuffer cmd, VkBuffer b)
{
   VkBufferMemoryBarrier bb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = 4};
   g->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0,
                         NULL);
}

static VkDependencyInfo
buf_dep(VkBufferMemoryBarrier2 *bb, VkPipelineStageFlags2 srcs,
        VkAccessFlags2 srca, VkPipelineStageFlags2 dsts, VkAccessFlags2 dsta,
        VkBuffer b)
{
   *bb = (VkBufferMemoryBarrier2){
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = srcs,
      .srcAccessMask = srca,
      .dstStageMask = dsts,
      .dstAccessMask = dsta,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = b,
      .size = 4};
   return (VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                             .bufferMemoryBarrierCount = 1,
                             .pBufferMemoryBarriers = bb};
}

/* Host write, then vkSetEvent, then a wait whose src is HOST. */
static int
case_host(struct gpu *g, VkPipeline pipe, VkPipelineLayout pl, VkDescriptorSet set,
          struct buf *in, struct buf *out, VkEvent ev, uint32_t magic, int replay)
{
   memset(out->p, 0, 4);
   memcpy(in->p, &magic, 4);
   if (replay)
      CK(g->ResetEvent(g->dev, ev), "ResetEvent");
   CK(g->SetEvent(g->dev, ev), "SetEvent");
   VkResult st = g->GetEventStatus(g->dev, ev);
   if (st != VK_EVENT_SET) {
      printf("FAIL host event not set before submit st=%d\n", (int)st);
      return 1;
   }
   stamp(replay ? "host-set-replay" : "host-set");

   VkCommandBuffer cmd;
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = g->pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(g->AllocateCommandBuffers(g->dev, &cai, &cmd), "Alloc");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
   CK(g->BeginCommandBuffer(cmd, &bbi), "Begin");
   VkBufferMemoryBarrier2 bb;
   VkDependencyInfo di =
      buf_dep(&bb, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
              in->b);
   g->CmdWaitEvents2(cmd, 1, &ev, &di);
   g->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   g->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &set,
                            0, NULL);
   g->CmdDispatch(cmd, 1, 1, 1);
   shader_to_host(g, cmd, out->b);
   CK(g->EndCommandBuffer(cmd), "End");

   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(g->CreateFence(g->dev, &fci, NULL, &fence), "Fence");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &cmd};
   double t0 = now_s();
   stamp(replay ? "host-submit-replay" : "host-submit");
   CK(g->QueueSubmit(g->q0, 1, &si, fence), "Submit");
   int wr = wait_fence(g, fence, t0, replay ? "host-replay" : "host");
   if (wr)
      return wr;
   uint32_t got;
   memcpy(&got, out->p, 4);
   printf("CASE host%s got=0x%08x expect=0x%08x\n", replay ? "-replay" : "",
          got, magic);
   return got == magic ? 0 : 1;
}

/* Same queue. Producer submit completes before the consumer is submitted,
 * so the event is already signaled and the consumer cannot deadlock.
 * Replay runs only after that completion, then resets and repeats. */
static int
case_gpu(struct gpu *g, VkPipeline pipe, VkPipelineLayout pl, VkDescriptorSet prod,
         VkDescriptorSet cons, struct buf *src, struct buf *mid, struct buf *out,
         VkEvent ev, uint32_t magic, int replay)
{
   memset(mid->p, 0, 4);
   memset(out->p, 0, 4);
   memcpy(src->p, &magic, 4);
   stamp(replay ? "gpu-write-replay" : "gpu-write");

   VkCommandBuffer cset, cwait;
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = g->pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(g->AllocateCommandBuffers(g->dev, &cai, &cset), "AllocSet");
   CK(g->AllocateCommandBuffers(g->dev, &cai, &cwait), "AllocWait");
   VkCommandBufferBeginInfo bbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

   CK(g->BeginCommandBuffer(cset, &bbi), "BeginSet");
   if (replay) {
      g->CmdResetEvent2(cset, ev, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
      /* Reset's stage mask does not order the following SetEvent2. */
      VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                             .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
      VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                             .memoryBarrierCount = 1,
                             .pMemoryBarriers = &mb};
      g->CmdPipelineBarrier2(cset, &di);
   }
   host_avail(g, cset, src->b);
   g->CmdBindPipeline(cset, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   g->CmdBindDescriptorSets(cset, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1,
                            &prod, 0, NULL);
   g->CmdDispatch(cset, 1, 1, 1);
   VkBufferMemoryBarrier2 sb;
   VkDependencyInfo sdi =
      buf_dep(&sb, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_READ_BIT, mid->b);
   g->CmdSetEvent2(cset, ev, &sdi);
   CK(g->EndCommandBuffer(cset), "EndSet");

   CK(g->BeginCommandBuffer(cwait, &bbi), "BeginWait");
   VkBufferMemoryBarrier2 wb;
   VkDependencyInfo wdi =
      buf_dep(&wb, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_READ_BIT, mid->b);
   g->CmdWaitEvents2(cwait, 1, &ev, &wdi);
   g->CmdBindPipeline(cwait, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   g->CmdBindDescriptorSets(cwait, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1,
                            &cons, 0, NULL);
   g->CmdDispatch(cwait, 1, 1, 1);
   shader_to_host(g, cwait, out->b);
   CK(g->EndCommandBuffer(cwait), "EndWait");

   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(g->CreateFence(g->dev, &fci, NULL, &fence), "Fence");
   VkSubmitInfo sis = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &cset};
   double t0 = now_s();
   stamp(replay ? "gpu-submit-set-replay" : "gpu-submit-set");
   CK(g->QueueSubmit(g->q0, 1, &sis, fence), "SubmitSet");
   int wr = wait_fence(g, fence, t0, replay ? "gpu-set-replay" : "gpu-set");
   if (wr)
      return wr;
   VkResult st = g->GetEventStatus(g->dev, ev);
   printf("GPU_EVENT_STATUS %d\n", (int)st);
   if (st != VK_EVENT_SET)
      return 1;
   CK(g->ResetFences(g->dev, 1, &fence), "ResetF");
   VkSubmitInfo siw = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &cwait};
   stamp(replay ? "gpu-submit-wait-replay" : "gpu-submit-wait");
   CK(g->QueueSubmit(g->q0, 1, &siw, fence), "SubmitWait");
   wr = wait_fence(g, fence, t0, replay ? "gpu-replay" : "gpu");
   if (wr)
      return wr;
   uint32_t got;
   memcpy(&got, out->p, 4);
   printf("CASE gpu%s got=0x%08x expect=0x%08x\n", replay ? "-replay" : "", got,
          magic);
   return got == magic ? 0 : 1;
}

static int
case_ring(struct gpu *g)
{
   VkShaderModule vs, fs;
   VkShaderModuleCreateInfo sm = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                  .codeSize = sizeof(tri_vert_spv),
                                  .pCode = tri_vert_spv};
   CK(g->CreateShaderModule(g->dev, &sm, NULL, &vs), "VS");
   sm.codeSize = sizeof(tri_frag_spv);
   sm.pCode = tri_frag_spv;
   CK(g->CreateShaderModule(g->dev, &sm, NULL, &fs), "FS");

   VkPipelineLayout pl;
   VkPipelineLayoutCreateInfo plci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
   CK(g->CreatePipelineLayout(g->dev, &plci, NULL, &pl), "PLL");

   VkAttachmentDescription att = {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   VkAttachmentReference ref = {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &ref};
   VkSubpassDependency dep = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                  .attachmentCount = 1,
                                  .pAttachments = &att,
                                  .subpassCount = 1,
                                  .pSubpasses = &sub,
                                  .dependencyCount = 1,
                                  .pDependencies = &dep};
   VkRenderPass rp;
   CK(g->CreateRenderPass(g->dev, &rpci, NULL, &rp), "RP");

   VkImage img;
   VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = VK_FORMAT_R8G8B8A8_UNORM,
                           .extent = {W, H, 1},
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   CK(g->CreateImage(g->dev, &ii, NULL, &img), "Image");
   VkMemoryRequirements mr;
   g->GetImageMemoryRequirements(g->dev, img, &mr);
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size,
                               .memoryTypeIndex = g->mem_local};
   VkDeviceMemory imem;
   CK(g->AllocateMemory(g->dev, &mai, NULL, &imem), "ImgMem");
   CK(g->BindImageMemory(g->dev, img, imem, 0), "BindImg");
   VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                               .image = img,
                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                               .format = VK_FORMAT_R8G8B8A8_UNORM,
                               .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   VkImageView view;
   CK(g->CreateImageView(g->dev, &vi, NULL, &view), "View");
   VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                   .renderPass = rp,
                                   .attachmentCount = 1,
                                   .pAttachments = &view,
                                   .width = W,
                                   .height = H,
                                   .layers = 1};
   VkFramebuffer fb;
   CK(g->CreateFramebuffer(g->dev, &fbci, NULL, &fb), "FB");

   VkVertexInputBindingDescription vb = {0, 8, VK_VERTEX_INPUT_RATE_VERTEX};
   VkVertexInputAttributeDescription va = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
   VkPipelineShaderStageCreateInfo st[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fs,
       .pName = "main"}};
   VkPipelineVertexInputStateCreateInfo vis = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vb,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &va};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkViewport viewport = {0, 0, W, H, 0, 1};
   VkRect2D scissor = {{0, 0}, {W, H}};
   VkPipelineViewportStateCreateInfo vp = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor};
   VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .lineWidth = 1.0f};
   VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &cba};
   VkGraphicsPipelineCreateInfo gp = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = st,
      .pVertexInputState = &vis,
      .pInputAssemblyState = &ia,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .layout = pl,
      .renderPass = rp,
      .subpass = 0};
   VkPipeline pipe;
   CK(g->CreateGraphicsPipelines(g->dev, VK_NULL_HANDLE, 1, &gp, NULL, &pipe),
      "GfxPipe");

   /* Covers the whole 8x8. */
   const float tri[6] = {-1, -1, 3, -1, -1, 3};
   struct buf vbuf;
   if (make_buf(g, sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1, &vbuf))
      return 1;
   memcpy(vbuf.p, tri, sizeof(tri));
   struct buf rb;
   if (make_buf(g, W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, &rb))
      return 1;

   VkCommandBuffer draw, copy;
   VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = g->pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
   CK(g->AllocateCommandBuffers(g->dev, &cai, &draw), "AllocDraw");
   CK(g->AllocateCommandBuffers(g->dev, &cai, &copy), "AllocCopy");
   VkCommandBufferBeginInfo cbi0 = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(g->BeginCommandBuffer(copy, &cbi0), "BeginCopy0");
   CK(g->EndCommandBuffer(copy), "EndCopy0");
   VkCommandBufferBeginInfo dbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
   CK(g->BeginCommandBuffer(draw, &dbi), "BeginDraw");
   VkClearValue clear = {.color = {{0.0f, 0.0f, 1.0f, 1.0f}}};
   VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                 .renderPass = rp,
                                 .framebuffer = fb,
                                 .renderArea = {{0, 0}, {W, H}},
                                 .clearValueCount = 1,
                                 .pClearValues = &clear};
   for (int i = 0; i < RING_PASSES; i++) {
      g->CmdBeginRenderPass(draw, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
      g->CmdBindPipeline(draw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      VkDeviceSize off = 0;
      g->CmdBindVertexBuffers(draw, 0, 1, &vbuf.b, &off);
      g->CmdDraw(draw, 3, 1, 0, 0);
      g->CmdEndRenderPass(draw);
   }
   CK(g->EndCommandBuffer(draw), "EndDraw");

   VkFence fence;
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   CK(g->CreateFence(g->dev, &fci, NULL, &fence), "Fence");
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &draw};
   VkCommandBufferBeginInfo cbi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
   VkBufferImageCopy region = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                               .imageExtent = {W, H, 1}};
   VkBufferMemoryBarrier hb = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = rb.b,
      .size = W * H * 4};
   stamp("ring-begin");
   for (int s = 0; s < RING_SUBMITS; s++) {
      CK(g->ResetFences(g->dev, 1, &fence), "ResetF");
      double t0 = now_s();
      CK(g->QueueSubmit(g->q0, 1, &si, fence), "SubmitDraw");
      int wr = wait_fence(g, fence, t0, "ring");
      if (wr) {
         printf("FAIL ring submit %d\n", s);
         return wr;
      }
      CK(g->ResetCommandBuffer(copy, 0), "ResetCopy");
      CK(g->BeginCommandBuffer(copy, &cbi), "BeginCopy");
      g->CmdCopyImageToBuffer(copy, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              rb.b, 1, &region);
      g->CmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &hb, 0,
                            NULL);
      CK(g->EndCommandBuffer(copy), "EndCopy");
      CK(g->ResetFences(g->dev, 1, &fence), "ResetF2");
      VkSubmitInfo sc = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &copy};
      double t1 = now_s();
      CK(g->QueueSubmit(g->q0, 1, &sc, fence), "SubmitCopy");
      wr = wait_fence(g, fence, t1, "ring-copy");
      if (wr)
         return wr;
      const uint8_t *px = rb.p;
      int red = 0;
      for (int i = 0; i < W * H; i++) {
         const uint8_t *p = px + i * 4;
         if (p[0] > 200 && p[1] < 30 && p[2] < 30)
            red++;
         else if (red == i)
            printf("PIXEL s=%d i=%d %u %u %u %u\n", s, i, p[0], p[1], p[2], p[3]);
      }
      printf("CASE ring submit=%d red=%d/%d passes=%d\n", s, red, W * H,
             RING_PASSES);
      if (red != W * H)
         return 1;
   }
   stamp("ring-end");
   return 0;
}

static int
bind_pair(struct gpu *g, VkDescriptorSetLayout dsl, VkDescriptorPool pool,
          struct buf *a, struct buf *b, VkDescriptorSet *set)
{
   VkDescriptorSetAllocateInfo dsai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &dsl};
   CK(g->AllocateDescriptorSets(g->dev, &dsai, set), "AllocDS");
   VkDescriptorBufferInfo bi[2] = {{a->b, 0, 4}, {b->b, 0, 4}};
   VkWriteDescriptorSet wr[2] = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = *set,
       .dstBinding = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pBufferInfo = &bi[0]},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = *set,
       .dstBinding = 1,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pBufferInfo = &bi[1]}};
   g->UpdateDescriptorSets(g->dev, 2, wr, 0, NULL);
   return 0;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("usage: %s <libvulkan_panfrost.so>\n", argv[0]);
      return 2;
   }
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      return 1;
   }
   printf("PID %d\n", (int)getpid());
   FILE *stf = fopen("/proc/self/status", "r");
   if (stf) {
      char line[256];
      while (fgets(line, sizeof(line), stf))
         if (!strncmp(line, "Tgid:", 5) || !strncmp(line, "Pid:", 4) ||
             !strncmp(line, "PPid:", 5))
            fputs(line, stdout);
      fclose(stf);
   }
   printf("READY\n");
   /* Real fifo (mknod p). Blocks until the runner has stamped maps. */
   int go = open("/tmp/085-go", O_RDONLY);
   if (go < 0) {
      printf("FAIL open /tmp/085-go: %s\n", strerror(errno));
      return 1;
   }
   char ack;
   if (read(go, &ack, 1) != 1) {
      printf("FAIL go pipe\n");
      return 1;
   }
   close(go);
   stamp("go");

   icd_gipa_fn gipa = (icd_gipa_fn)dlsym(h, "vk_icdGetInstanceProcAddr");
   if (!gipa) {
      printf("FAIL gipa\n");
      return 1;
   }
   PFN_vkCreateInstance CreateInstance =
      (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   struct gpu g = {.gipa = gipa};
   CK(CreateInstance(&ici, NULL, &g.inst), "CreateInstance");

#define GI(n)                                                                  \
   PFN_vk##n n = (PFN_vk##n)gipa(g.inst, "vk" #n);                             \
   if (!n) {                                                                   \
      printf("FAIL missing vk" #n "\n");                                       \
      return 1;                                                                \
   }
   GI(EnumeratePhysicalDevices)
   GI(GetPhysicalDeviceProperties)
   GI(GetPhysicalDeviceFeatures2)
   GI(GetPhysicalDeviceQueueFamilyProperties)
   GI(GetPhysicalDeviceMemoryProperties)
   GI(CreateDevice)
   GI(GetDeviceQueue)
   GI(GetDeviceProcAddr)
   GI(CreateCommandPool)

   uint32_t n = 0;
   CK(EnumeratePhysicalDevices(g.inst, &n, NULL), "count");
   VkPhysicalDevice *devs = calloc(n, sizeof(*devs));
   CK(EnumeratePhysicalDevices(g.inst, &n, devs), "enum");
   for (uint32_t i = 0; i < n; i++) {
      VkPhysicalDeviceProperties p;
      GetPhysicalDeviceProperties(devs[i], &p);
      printf("PHYS %u %s vendor=0x%x device=0x%x\n", i, p.deviceName, p.vendorID,
             p.deviceID);
      if (strstr(p.deviceName, "Mali"))
         g.phys = devs[i];
   }
   free(devs);
   if (!g.phys) {
      printf("FAIL no Mali\n");
      return 1;
   }

   VkPhysicalDeviceSynchronization2Features got = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
   VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   .pNext = &got};
   GetPhysicalDeviceFeatures2(g.phys, &f2);
   printf("FEATURE synchronization2=%u\n", got.synchronization2);
   if (!got.synchronization2) {
      printf("FAIL synchronization2 unsupported\n");
      return 1;
   }

   uint32_t qn = 0;
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, NULL);
   VkQueueFamilyProperties *qp = calloc(qn, sizeof(*qp));
   GetPhysicalDeviceQueueFamilyProperties(g.phys, &qn, qp);
   g.qi = ~0u;
   uint32_t qc = 0;
   for (uint32_t i = 0; i < qn; i++)
      if ((qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
          (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
         g.qi = i;
         qc = qp[i].queueCount;
         printf("QFAM %u flags=0x%x count=%u\n", i, qp[i].queueFlags, qc);
         break;
      }
   free(qp);
    if (g.qi == ~0u) {
      printf("FAIL need graphics+compute queue\n");
      return 1;
   }
   (void)qc;

   VkPhysicalDeviceMemoryProperties mp;
   GetPhysicalDeviceMemoryProperties(g.phys, &mp);
   g.mem_host = g.mem_local = ~0u;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
      if (g.mem_host == ~0u &&
          (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
          (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         g.mem_host = i;
      if (g.mem_local == ~0u && (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
         g.mem_local = i;
   }
   if (g.mem_host == ~0u) {
      printf("FAIL no host-coherent memory\n");
      return 1;
   }
   if (g.mem_local == ~0u)
      g.mem_local = g.mem_host;

    float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = g.qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceSynchronization2Features en = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
      .synchronization2 = VK_TRUE};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .pNext = &en,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci};
   CK(CreateDevice(g.phys, &dci, NULL, &g.dev), "CreateDevice");
   GetDeviceQueue(g.dev, g.qi, 0, &g.q0);
   g.gdpa = GetDeviceProcAddr;
   printf("SYNC2 enabled\n");

#define LP(n)                                                                  \
   if (proc(&g, "vk" #n, &g.n))                                                \
      return 1;
   LP(CreateEvent) LP(DestroyEvent) LP(SetEvent) LP(ResetEvent) LP(GetEventStatus)
   LP(AllocateCommandBuffers) LP(BeginCommandBuffer) LP(EndCommandBuffer)
   LP(ResetCommandBuffer) LP(CmdWaitEvents2) LP(CmdSetEvent2) LP(CmdResetEvent2)
   LP(CmdPipelineBarrier) LP(CmdPipelineBarrier2) LP(CmdBindPipeline) LP(CmdBindDescriptorSets)
   LP(CmdDispatch) LP(CmdBeginRenderPass) LP(CmdEndRenderPass)
   LP(CmdBindVertexBuffers) LP(CmdDraw) LP(CmdCopyImageToBuffer) LP(CreateFence)
   LP(WaitForFences) LP(ResetFences) LP(QueueSubmit) LP(CreateBuffer)
   LP(GetBufferMemoryRequirements) LP(AllocateMemory) LP(BindBufferMemory)
   LP(MapMemory) LP(CreateDescriptorSetLayout) LP(CreateDescriptorPool)
   LP(AllocateDescriptorSets) LP(UpdateDescriptorSets) LP(CreateShaderModule)
   LP(CreatePipelineLayout) LP(CreateComputePipelines) LP(CreateRenderPass)
   LP(CreateGraphicsPipelines) LP(CreateImage) LP(GetImageMemoryRequirements)
   LP(BindImageMemory) LP(CreateImageView) LP(CreateFramebuffer)
#undef LP

   VkCommandPoolCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = g.qi};
   CK(CreateCommandPool(g.dev, &pci, NULL, &g.pool), "Pool");

   VkDescriptorSetLayoutBinding binds[2] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}};
   VkDescriptorSetLayoutCreateInfo dlci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = binds};
   VkDescriptorSetLayout dsl;
   CK(g.CreateDescriptorSetLayout(g.dev, &dlci, NULL, &dsl), "DSL");
   VkDescriptorPoolSize psz = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};
   VkDescriptorPoolCreateInfo dpci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 4,
      .poolSizeCount = 1,
      .pPoolSizes = &psz};
   VkDescriptorPool dpool;
   CK(g.CreateDescriptorPool(g.dev, &dpci, NULL, &dpool), "DPool");

   VkShaderModule cs;
   VkShaderModuleCreateInfo sm = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                  .codeSize = sizeof(copy_spv),
                                  .pCode = copy_spv};
   CK(g.CreateShaderModule(g.dev, &sm, NULL, &cs), "CS");
   VkPipelineLayout pl;
   VkPipelineLayoutCreateInfo plci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount = 1,
                                      .pSetLayouts = &dsl};
   CK(g.CreatePipelineLayout(g.dev, &plci, NULL, &pl), "CPL");
   VkPipelineShaderStageCreateInfo sst = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = cs,
      .pName = "main"};
   VkComputePipelineCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = sst,
                                      .layout = pl};
   VkPipeline cp;
   CK(g.CreateComputePipelines(g.dev, VK_NULL_HANDLE, 1, &cpi, NULL, &cp), "CPipe");

   struct buf hin, hout, gsrc, gmid, gout;
   if (make_buf(&g, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1, &hin) ||
       make_buf(&g, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1, &hout) ||
       make_buf(&g, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1, &gsrc) ||
       make_buf(&g, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1, &gmid) ||
       make_buf(&g, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1, &gout))
      return 1;
   VkDescriptorSet hs, prod, cons;
   if (bind_pair(&g, dsl, dpool, &hin, &hout, &hs) ||
       bind_pair(&g, dsl, dpool, &gsrc, &gmid, &prod) ||
       bind_pair(&g, dsl, dpool, &gmid, &gout, &cons))
      return 1;

   VkEvent he, ge;
   VkEventCreateInfo eci = {.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
   CK(g.CreateEvent(g.dev, &eci, NULL, &he), "HostEvent");
   CK(g.CreateEvent(g.dev, &eci, NULL, &ge), "GpuEvent");

   int r;
   if ((r = case_host(&g, cp, pl, hs, &hin, &hout, he, HOST_A, 0)) ||
       (r = case_host(&g, cp, pl, hs, &hin, &hout, he, HOST_B, 1)) ||
       (r = case_gpu(&g, cp, pl, prod, cons, &gsrc, &gmid, &gout, ge, GPU_A, 0)) ||
       (r = case_gpu(&g, cp, pl, prod, cons, &gsrc, &gmid, &gout, ge, GPU_B, 1)) ||
       (r = case_ring(&g))) {
      printf("RESULT FAIL %d\n", r);
      return r;
   }
   printf("RESULT PASS\n");
   return 0;
}
