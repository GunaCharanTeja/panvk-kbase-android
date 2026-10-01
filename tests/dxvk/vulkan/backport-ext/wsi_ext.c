/* Backport device probe: VK_KHR_incremental_present + VK_EXT_swapchain_colorspace
 * on a headless surface. Direct ICD load. Prints advertisement, surface
 * formats/colorspaces, and presents N frames with VkPresentRegionsKHR.
 * usage: wsi_ext <icd.so> */
#define VK_NO_PROTOTYPES
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CK(expr, what)                                                         \
   do {                                                                        \
      VkResult _r = (expr);                                                    \
      if (_r != VK_SUCCESS && _r != VK_SUBOPTIMAL_KHR) {                       \
         printf("FAIL %s r=%d line=%d\n", what, _r, __LINE__);                 \
         exit(1);                                                              \
      }                                                                        \
   } while (0)

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define L(n) PFN_vk##n vk##n = (PFN_vk##n)gipa(inst, "vk" #n)

static int
has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
   for (uint32_t i = 0; i < n; i++)
      if (!strcmp(e[i].extensionName, name))
         return 1;
   return 0;
}

int
main(int argc, char **argv)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argc > 1 ? argv[1] : "libvulkan_panfrost.so", RTLD_NOW);
   if (!h) {
      printf("FAIL dlopen %s\n", dlerror());
      return 1;
   }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkEnumerateInstanceExtensionProperties eie =
      (PFN_vkEnumerateInstanceExtensionProperties)gipa(
         NULL, "vkEnumerateInstanceExtensionProperties");
   PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   uint32_t ni = 0;
   eie(NULL, &ni, NULL);
   VkExtensionProperties *ie = calloc(ni, sizeof(*ie));
   eie(NULL, &ni, ie);
   int has_cs = has_ext(ie, ni, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
   int has_hl = has_ext(ie, ni, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
   printf("INST EXT_swapchain_colorspace=%d EXT_headless_surface=%d\n", has_cs,
          has_hl);
   if (!has_hl) {
      printf("FAIL no headless surface\n");
      return 1;
   }

   const char *iexts[3] = {VK_KHR_SURFACE_EXTENSION_NAME,
                           VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
                           VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME};
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app,
                               .enabledExtensionCount = has_cs ? 3 : 2,
                               .ppEnabledExtensionNames = iexts};
   CK(ci(&ici, NULL, &inst), "CreateInstance");

   L(EnumeratePhysicalDevices);
   L(GetPhysicalDeviceProperties);
   L(EnumerateDeviceExtensionProperties);
   L(GetPhysicalDeviceQueueFamilyProperties);
   L(CreateDevice);
   L(GetDeviceQueue);
   L(CreateHeadlessSurfaceEXT);
   L(GetPhysicalDeviceSurfaceSupportKHR);
   L(GetPhysicalDeviceSurfaceFormatsKHR);
   L(GetPhysicalDeviceSurfaceCapabilitiesKHR);

   uint32_t nd = 8;
   VkPhysicalDevice pds[8], pd = VK_NULL_HANDLE;
   CK(vkEnumeratePhysicalDevices(inst, &nd, pds), "enum");
   VkPhysicalDeviceProperties props;
   for (uint32_t i = 0; i < nd; i++) {
      vkGetPhysicalDeviceProperties(pds[i], &props);
      if (strstr(props.deviceName, "Mali")) {
         pd = pds[i];
         break;
      }
   }
   if (!pd) {
      printf("FAIL no Mali\n");
      return 1;
   }
   printf("ICD device=%s deviceID=0x%x driverVersion=0x%x\n", props.deviceName,
          props.deviceID, props.driverVersion);

   uint32_t ne = 0;
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
   VkExtensionProperties *de = calloc(ne, sizeof(*de));
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, de);
   int has_ip = has_ext(de, ne, VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
   printf("DEV KHR_incremental_present=%d KHR_swapchain=%d\n", has_ip,
          has_ext(de, ne, VK_KHR_SWAPCHAIN_EXTENSION_NAME));

   VkHeadlessSurfaceCreateInfoEXT hci = {
      .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
   VkSurfaceKHR surf;
   CK(vkCreateHeadlessSurfaceEXT(inst, &hci, NULL, &surf), "HeadlessSurface");

   uint32_t nf = 0;
   CK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, NULL), "fmts");
   VkSurfaceFormatKHR *sf = calloc(nf, sizeof(*sf));
   CK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, sf), "fmts2");
   int non_srgb_cs = 0;
   for (uint32_t i = 0; i < nf; i++) {
      printf("SURF_FMT format=%d colorSpace=%d\n", sf[i].format,
             sf[i].colorSpace);
      if (sf[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
         non_srgb_cs++;
   }
   printf("SURF_FMT_COUNT=%u non_srgb_nonlinear=%d\n", nf, non_srgb_cs);

   uint32_t qn = 8;
   VkQueueFamilyProperties qp[8];
   vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qp);
   uint32_t qi = 0;
   while (qi < qn && !(qp[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      qi++;
   VkBool32 sup = 0;
   CK(vkGetPhysicalDeviceSurfaceSupportKHR(pd, qi, surf, &sup), "support");
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = qi,
      .queueCount = 1,
      .pQueuePriorities = &prio};
   const char *dexts[2] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                           VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .enabledExtensionCount = has_ip ? 2 : 1,
                             .ppEnabledExtensionNames = dexts};
   VkDevice dev;
   CK(vkCreateDevice(pd, &dci, NULL, &dev), "CreateDevice");
   PFN_vkGetDeviceProcAddr gdpa =
      (PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr");
#define DL(n) PFN_vk##n vk##n = (PFN_vk##n)gdpa(dev, "vk" #n)
   DL(CreateSwapchainKHR);
   DL(GetSwapchainImagesKHR);
   DL(AcquireNextImageKHR);
   DL(QueuePresentKHR);
   DL(CreateCommandPool);
   DL(AllocateCommandBuffers);
   DL(BeginCommandBuffer);
   DL(EndCommandBuffer);
   DL(CmdPipelineBarrier);
   DL(CmdClearColorImage);
   DL(QueueSubmit);
   DL(CreateFence);
   DL(WaitForFences);
   DL(ResetFences);
   DL(DeviceWaitIdle);
   VkQueue q;
   vkGetDeviceQueue(dev, qi, 0, &q);

   VkSurfaceCapabilitiesKHR caps;
   CK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &caps), "caps");
   VkExtent2D ext = caps.currentExtent.width == 0xffffffff
                       ? (VkExtent2D){64, 64}
                       : caps.currentExtent;
   int fails = 0;
   /* one swapchain per advertised (format, colorspace) pair */
   for (uint32_t f = 0; f < nf; f++) {
      VkSwapchainCreateInfoKHR sci = {
         .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
         .surface = surf,
         .minImageCount = caps.minImageCount,
         .imageFormat = sf[f].format,
         .imageColorSpace = sf[f].colorSpace,
         .imageExtent = ext,
         .imageArrayLayers = 1,
         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT,
         .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
         .preTransform = caps.currentTransform,
         .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
         .presentMode = VK_PRESENT_MODE_FIFO_KHR,
         .clipped = VK_TRUE};
      VkSwapchainKHR sc;
      VkResult r = vkCreateSwapchainKHR(dev, &sci, NULL, &sc);
      if (r != VK_SUCCESS) {
         printf("CASE swapchain fmt=%d cs=%d create r=%d FAIL\n", sf[f].format,
                sf[f].colorSpace, r);
         fails++;
         continue;
      }
      uint32_t nimg = 0;
      vkGetSwapchainImagesKHR(dev, sc, &nimg, NULL);
      VkImage imgs[8];
      if (nimg > 8)
         nimg = 8;
      vkGetSwapchainImagesKHR(dev, sc, &nimg, imgs);

      VkCommandPoolCreateInfo pci = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
         .queueFamilyIndex = qi};
      VkCommandPool pool;
      CK(vkCreateCommandPool(dev, &pci, NULL, &pool), "pool");
      VkCommandBufferAllocateInfo cai = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .commandPool = pool,
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = 1};
      VkCommandBuffer cmd;
      CK(vkAllocateCommandBuffers(dev, &cai, &cmd), "cmdbuf");
      VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
      VkFence acq, done;
      CK(vkCreateFence(dev, &fci, NULL, &acq), "fence");
      CK(vkCreateFence(dev, &fci, NULL, &done), "fence2");

      int ok = 1;
      for (int frame = 0; frame < 6 && ok; frame++) {
         uint32_t idx;
         r = vkAcquireNextImageKHR(dev, sc, 1000000000ull, VK_NULL_HANDLE, acq,
                                   &idx);
         if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            printf("CASE fmt=%d cs=%d acquire r=%d FAIL\n", sf[f].format,
                   sf[f].colorSpace, r);
            ok = 0;
            break;
         }
         vkWaitForFences(dev, 1, &acq, VK_TRUE, ~0ull);
         vkResetFences(dev, 1, &acq);
         VkCommandBufferBeginInfo bi = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
         vkBeginCommandBuffer(cmd, &bi);
         VkImageSubresourceRange rng = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
         VkImageMemoryBarrier b = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = imgs[idx],
            .subresourceRange = rng};
         vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                              NULL, 1, &b);
         VkClearColorValue cc = {.float32 = {frame & 1, 0.5f, 0.25f, 1.0f}};
         vkCmdClearColorImage(cmd, imgs[idx],
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cc, 1,
                              &rng);
         b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
         b.dstAccessMask = 0;
         b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
         b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
         vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL,
                              0, NULL, 1, &b);
         vkEndCommandBuffer(cmd);
         VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &cmd};
         CK(vkQueueSubmit(q, 1, &si, done), "submit");
         vkWaitForFences(dev, 1, &done, VK_TRUE, ~0ull);
         vkResetFences(dev, 1, &done);

         /* incremental present: damage one rectangle that changes per frame */
         VkRectLayerKHR rect = {
            .offset = {(int32_t)(frame * 4) % (int32_t)ext.width, 0},
            .extent = {4, 4},
            .layer = 0};
         VkPresentRegionKHR reg = {.rectangleCount = 1, .pRectangles = &rect};
         VkPresentRegionsKHR regs = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR,
            .swapchainCount = 1,
            .pRegions = &reg};
         VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .pNext = has_ip ? &regs : NULL,
                                .swapchainCount = 1,
                                .pSwapchains = &sc,
                                .pImageIndices = &idx};
         r = vkQueuePresentKHR(q, &pi);
         if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            printf("CASE fmt=%d cs=%d present r=%d FAIL\n", sf[f].format,
                   sf[f].colorSpace, r);
            ok = 0;
         }
      }
      vkDeviceWaitIdle(dev);
      printf("CASE swapchain fmt=%d cs=%d images=%u frames=6 regions=%d %s\n",
             sf[f].format, sf[f].colorSpace, nimg, has_ip,
             ok ? "PASS" : "FAIL");
      if (!ok)
         fails++;
      PFN_vkDestroySwapchainKHR ds =
         (PFN_vkDestroySwapchainKHR)gdpa(dev, "vkDestroySwapchainKHR");
      ds(dev, sc, NULL);
   }
   if (!has_ip || !has_cs)
      fails++;
   printf("WSI_EXT_FAILS=%d\n", fails);
   return fails != 0;
}
