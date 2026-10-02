/* nclear: native Xlib WSI, 320x240 BGRA, layer off.
 *   nclear rp|xfer|sample|sample-copy [frames]
 * rp   = empty render pass, loadOp CLEAR, no draws
 * xfer = vkCmdClearColorImage (same path as tmp/m4-wsi/wsitest.c)
 * Orange is float {1, 0.5, 0.25, 1}. After each present, XGetImage the
 * window before the next frame. Dump raw BGRA + a timestamp line.
 * sample = offscreen render-pass clear -> texture draw, no destination copy.
 * sample-copy = same, producer copy before sampling. */
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FAIL(...) do { printf("FAIL: " __VA_ARGS__); printf("\n"); exit(1); } while (0)
#define CK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) FAIL("%s = %d", #x, (int)r_); } while (0)

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkDevice dev;
#define IF(n) static PFN_##n n
IF(vkCreateInstance); IF(vkEnumeratePhysicalDevices); IF(vkGetPhysicalDeviceProperties);
IF(vkCreateDevice); IF(vkGetDeviceQueue); IF(vkCreateXlibSurfaceKHR);
IF(vkGetPhysicalDeviceSurfaceSupportKHR); IF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
IF(vkGetPhysicalDeviceSurfaceFormatsKHR); IF(vkDestroySurfaceKHR); IF(vkDestroyInstance);
IF(vkCreateSwapchainKHR); IF(vkDestroySwapchainKHR); IF(vkGetSwapchainImagesKHR);
IF(vkAcquireNextImageKHR); IF(vkQueuePresentKHR); IF(vkCreateCommandPool);
IF(vkAllocateCommandBuffers); IF(vkBeginCommandBuffer); IF(vkEndCommandBuffer);
IF(vkCmdPipelineBarrier); IF(vkCmdClearColorImage); IF(vkQueueSubmit);
IF(vkCreateSemaphore); IF(vkCreateFence); IF(vkWaitForFences); IF(vkResetFences);
IF(vkDeviceWaitIdle); IF(vkDestroyDevice); IF(vkDestroySemaphore); IF(vkDestroyFence);
IF(vkDestroyCommandPool); IF(vkResetCommandBuffer); IF(vkCreateRenderPass);
IF(vkDestroyRenderPass); IF(vkCreateImageView); IF(vkDestroyImageView);
IF(vkCreateFramebuffer); IF(vkDestroyFramebuffer); IF(vkCmdBeginRenderPass);
IF(vkCmdEndRenderPass);
IF(vkCreateBuffer); IF(vkDestroyBuffer); IF(vkGetBufferMemoryRequirements);
IF(vkAllocateMemory); IF(vkFreeMemory); IF(vkBindBufferMemory);
IF(vkMapMemory); IF(vkUnmapMemory); IF(vkInvalidateMappedMemoryRanges);
IF(vkCmdCopyImageToBuffer); IF(vkGetPhysicalDeviceMemoryProperties);
#define LI(n) n = (PFN_##n)gipa(inst, #n)

static unsigned W = 320, H = 240;
enum { MAXI = 4 };

static double mono(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec + t.tv_nsec * 1e-9;
}

static void stamp(const char *ev)
{
   struct timespec r, m;
   clock_gettime(CLOCK_REALTIME, &r);
   clock_gettime(CLOCK_MONOTONIC, &m);
   printf("TS %s realtime=%ld.%09ld mono=%ld.%09ld\n", ev,
          (long)r.tv_sec, r.tv_nsec, (long)m.tv_sec, m.tv_nsec);
}

static unsigned chan(unsigned long p, unsigned long mask)
{
   int s = 0;
   unsigned long m = mask;
   if (!m) return 0;
   while ((m & 1ul) == 0) { m >>= 1; s++; }
   return (unsigned)((p & mask) >> s);
}

/* X visuals here are 24-bit. Byte 3 is not alpha. Orange is the RGB mask match. */
static void count_img(XImage *im, int *orange, int *zero, int *other)
{
   int x, y;
   *orange = *zero = *other = 0;
   for (y = 0; y < im->height; y++) {
      for (x = 0; x < im->width; x++) {
         unsigned long p = XGetPixel(im, x, y);
         unsigned r = chan(p, im->red_mask), g = chan(p, im->green_mask), b = chan(p, im->blue_mask);
         int hit = abs((int)r - 255) <= 1 && abs((int)g - 128) <= 1 && abs((int)b - 64) <= 1;
         if (hit) (*orange)++;
         else if (!r && !g && !b) (*zero)++;
         else (*other)++;
      }
   }
}

static void dump_raw(const char *path, XImage *im)
{
   FILE *f = fopen(path, "wb");
   int y, bpp = im->bits_per_pixel / 8;
   if (!f) FAIL("fopen %s", path);
   for (y = 0; y < im->height; y++)
      if ((int)fwrite(im->data + (size_t)y * im->bytes_per_line, 1, (size_t)im->width * bpp, f) != im->width * bpp)
         FAIL("short write %s", path);
   fclose(f);
}

#include "sample-clear.h"

int main(int argc, char **argv)
{
   int sampled = argc > 1 && (!strcmp(argv[1], "sample") || !strcmp(argv[1], "sample-copy"));
   int forced_copy = argc > 1 && !strcmp(argv[1], "sample-copy");
   int rp = sampled || (argc > 1 && !strcmp(argv[1], "rp"));
   int frames = argc > 2 ? atoi(argv[2]) : 4;
   if (argc > 3) W = (unsigned)atoi(argv[3]);
   if (argc > 4) H = (unsigned)atoi(argv[4]);
   const char *mode = sampled ? argv[1] : (rp ? "rp" : "xfer");
   void *lib;
   Display *dpy;
   Window win;
   VkSurfaceKHR surf;
   VkPhysicalDevice pd;
   VkQueue q;
   VkFormat fmt = VK_FORMAT_B8G8R8A8_UNORM;
   VkSwapchainKHR sc;
   VkImage imgs[MAXI];
   VkImageView views[MAXI];
   VkFramebuffer fbs[MAXI];
   VkRenderPass pass = VK_NULL_HANDLE;
   VkCommandPool pool;
   VkCommandBuffer cb;
    VkSemaphore acq, done[MAXI];
    VkFence fence;
    VkBuffer rbuf = VK_NULL_HANDLE;
    VkDeviceMemory rmem = VK_NULL_HANDLE;
    void *rmap = NULL;
    VkDeviceSize rstride = (VkDeviceSize)W * 4;
   uint32_t npd = 1, nf = 0, nimg = 0, i;
   VkSurfaceFormatKHR fmts[16];
   VkClearColorValue orange = { .float32 = { 1.f, 0.5f, 0.25f, 1.f } };
   int f;
   unsigned src_orange = 0, src_zero = 0, src_other = 0;
   int xi_orange = 0, xi_zero = 0, xi_other = 0;

   setvbuf(stdout, NULL, _IONBF, 0);
   if (argc > 1 && !sampled && strcmp(argv[1], "rp") && strcmp(argv[1], "xfer"))
      FAIL("usage: nclear rp|xfer|sample|sample-copy [frames] [w] [h]");
   if (frames < 1 || frames > 16) FAIL("frames 1..16");
   if (W < 32 || H < 32 || W > 1280 || H > 1280) FAIL("size");
   if (getenv("VK_INSTANCE_LAYERS") && getenv("VK_INSTANCE_LAYERS")[0])
      FAIL("VK_INSTANCE_LAYERS set; this run must be layer off");

   lib = dlopen("libvulkan.so.1", RTLD_NOW);
   if (!lib) FAIL("dlopen libvulkan.so.1: %s", dlerror());
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
   LI(vkCreateInstance);

   {
      const char *iext[] = { "VK_KHR_surface", "VK_KHR_xlib_surface" };
      VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3 };
      VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
                                   .enabledExtensionCount = 2, .ppEnabledExtensionNames = iext };
      CK(vkCreateInstance(&ici, NULL, &inst));
   }
   LI(vkEnumeratePhysicalDevices); LI(vkGetPhysicalDeviceProperties); LI(vkCreateDevice);
   LI(vkGetDeviceQueue); LI(vkCreateXlibSurfaceKHR); LI(vkGetPhysicalDeviceSurfaceSupportKHR);
   LI(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); LI(vkGetPhysicalDeviceSurfaceFormatsKHR);
   LI(vkDestroySurfaceKHR); LI(vkDestroyInstance); LI(vkCreateSwapchainKHR); LI(vkDestroySwapchainKHR);
   LI(vkGetSwapchainImagesKHR); LI(vkAcquireNextImageKHR); LI(vkQueuePresentKHR);
   LI(vkCreateCommandPool); LI(vkAllocateCommandBuffers); LI(vkBeginCommandBuffer);
   LI(vkEndCommandBuffer); LI(vkCmdPipelineBarrier); LI(vkCmdClearColorImage); LI(vkQueueSubmit);
   LI(vkCreateSemaphore); LI(vkCreateFence); LI(vkWaitForFences); LI(vkResetFences);
   LI(vkDeviceWaitIdle); LI(vkDestroyDevice); LI(vkDestroySemaphore); LI(vkDestroyFence);
   LI(vkDestroyCommandPool); LI(vkResetCommandBuffer); LI(vkCreateRenderPass); LI(vkDestroyRenderPass);
   LI(vkCreateImageView); LI(vkDestroyImageView); LI(vkCreateFramebuffer); LI(vkDestroyFramebuffer);
   LI(vkCmdBeginRenderPass); LI(vkCmdEndRenderPass);
   LI(vkCreateBuffer); LI(vkDestroyBuffer); LI(vkGetBufferMemoryRequirements);
   LI(vkAllocateMemory); LI(vkFreeMemory); LI(vkBindBufferMemory);
   LI(vkMapMemory); LI(vkUnmapMemory); LI(vkInvalidateMappedMemoryRanges);
   LI(vkCmdCopyImageToBuffer); LI(vkGetPhysicalDeviceMemoryProperties);

   vkEnumeratePhysicalDevices(inst, &npd, &pd);
   if (!npd) FAIL("no physical device");
   {
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(pd, &props);
      printf("device: %s api %u.%u.%u driver 0x%x mode=%s\n", props.deviceName,
             VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
             VK_API_VERSION_PATCH(props.apiVersion), props.driverVersion, mode);
   }

   XInitThreads();
   dpy = XOpenDisplay(NULL);
   if (!dpy) FAIL("XOpenDisplay");
   win = XCreateSimpleWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, W, H, 0, 0, 0);
   XStoreName(dpy, win, mode);
   XMapWindow(dpy, win);
   XSync(dpy, False);
   {
      VkXlibSurfaceCreateInfoKHR ci = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, .dpy = dpy, .window = win };
      VkBool32 sup = 0;
      CK(vkCreateXlibSurfaceKHR(inst, &ci, NULL, &surf));
      CK(vkGetPhysicalDeviceSurfaceSupportKHR(pd, 0, surf, &sup));
      printf("surface-support=%u window=0x%lx\n", sup, win);
      if (!sup) FAIL("surface not supported");
   }

   vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, NULL);
   if (!nf || nf > 16) FAIL("formats %u", nf);
   vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, fmts);
   fmt = fmts[0].format;
   for (i = 0; i < nf; i++)
      if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM) fmt = fmts[i].format;
   printf("chosen-format=%d (BGRA8=%d)\n", fmt, VK_FORMAT_B8G8R8A8_UNORM);
   if (fmt != VK_FORMAT_B8G8R8A8_UNORM) FAIL("BGRA8 UNORM not offered");

   {
      float prio = 1;
      const char *dext[] = { "VK_KHR_swapchain" };
      VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &prio };
      VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                                 .pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dext };
      CK(vkCreateDevice(pd, &dci, NULL, &dev));
      vkGetDeviceQueue(dev, 0, 0, &q);
   }

   {
      VkSurfaceCapabilitiesKHR caps;
      VkExtent2D ext = { W, H };
      VkSwapchainCreateInfoKHR sw;
      CK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &caps));
      printf("caps usage=0x%x extent=%ux%u min=%u max=%u\n", caps.supportedUsageFlags,
             caps.currentExtent.width, caps.currentExtent.height, caps.minImageCount, caps.maxImageCount);
      if (caps.currentExtent.width != 0xFFFFFFFFu) {
         ext = caps.currentExtent;
         if (ext.width != W || ext.height != H)
            FAIL("extent %ux%u != %ux%u", ext.width, ext.height, W, H);
      }
      if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
         FAIL("COLOR_ATTACHMENT usage unsupported");
       if (!rp && !(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
          FAIL("TRANSFER_DST usage unsupported");
       printf("caps TRANSFER_SRC=%d TRANSFER_DST=%d COLOR=%d\n",
              !!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
              !!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT),
              !!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
      memset(&sw, 0, sizeof(sw));
      sw.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      sw.surface = surf;
      sw.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
      if (caps.maxImageCount && sw.minImageCount > caps.maxImageCount) sw.minImageCount = caps.maxImageCount;
      sw.imageFormat = fmt;
      sw.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      sw.imageExtent = ext;
      sw.imageArrayLayers = 1;
       sw.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
       if (!rp)
          sw.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
       if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
          FAIL("TRANSFER_SRC usage unsupported; refusing a copy that the surface cannot do");
      sw.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
      sw.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                             ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
      sw.presentMode = VK_PRESENT_MODE_FIFO_KHR;
      sw.clipped = VK_TRUE;
      CK(vkCreateSwapchainKHR(dev, &sw, NULL, &sc));
      nimg = MAXI;
      CK(vkGetSwapchainImagesKHR(dev, sc, &nimg, imgs));
      if (!nimg || nimg > MAXI) FAIL("images %u", nimg);
      printf("swapchain %ux%u images=%u usage=0x%x\n", ext.width, ext.height, nimg, sw.imageUsage);
   }

   if (rp) {
      VkAttachmentDescription att = {
         .format = fmt, .samples = VK_SAMPLE_COUNT_1_BIT,
         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
         .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
      VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
      VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   .colorAttachmentCount = 1, .pColorAttachments = &ref };
      VkSubpassDependency dep = {
         .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
         .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
      VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &att,
                                      .subpassCount = 1, .pSubpasses = &sub, .dependencyCount = 1, .pDependencies = &dep };
      CK(vkCreateRenderPass(dev, &rpci, NULL, &pass));
      for (i = 0; i < nimg; i++) {
         VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = imgs[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmt,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
         VkFramebufferCreateInfo fi;
         CK(vkCreateImageView(dev, &vi, NULL, &views[i]));
         memset(&fi, 0, sizeof(fi));
         fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         fi.renderPass = pass; fi.attachmentCount = 1; fi.pAttachments = &views[i];
         fi.width = W; fi.height = H; fi.layers = 1;
         CK(vkCreateFramebuffer(dev, &fi, NULL, &fbs[i]));
      }
       printf("renderpass loadOp=CLEAR attachments=1 draws=0\n");
       if (sampled) sample_init(pd, fmt, pass);
   }

   {
      VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
      VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandBufferCount = 1 };
      VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
      CK(vkCreateCommandPool(dev, &cpci, NULL, &pool));
      cbai.commandPool = pool;
      CK(vkAllocateCommandBuffers(dev, &cbai, &cb));
       CK(vkCreateSemaphore(dev, &sci, NULL, &acq));
       for (i = 0; i < nimg; i++)
          CK(vkCreateSemaphore(dev, &sci, NULL, &done[i]));
       CK(vkCreateFence(dev, &fci, NULL, &fence));
    }

    {
       VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
       VkMemoryRequirements mr;
       VkPhysicalDeviceMemoryProperties mp;
       VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
       uint32_t t;
       bci.size = rstride * H;
       bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
       CK(vkCreateBuffer(dev, &bci, NULL, &rbuf));
       vkGetBufferMemoryRequirements(dev, rbuf, &mr);
       vkGetPhysicalDeviceMemoryProperties(pd, &mp);
       mai.allocationSize = mr.size;
       for (t = 0; t < mp.memoryTypeCount; t++) {
          if ((mr.memoryTypeBits & (1u << t)) &&
              (mp.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
              (mp.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
             mai.memoryTypeIndex = t;
             break;
          }
       }
       if (t == mp.memoryTypeCount) FAIL("no host-cached memory for readback");
       CK(vkAllocateMemory(dev, &mai, NULL, &rmem));
       CK(vkBindBufferMemory(dev, rbuf, rmem, 0));
       CK(vkMapMemory(dev, rmem, 0, mr.size, 0, &rmap));
       if (!rmap) FAIL("map returned NULL");
       printf("readback buffer bytes=%llu host-cached type=%u map=%p\n",
              (unsigned long long)mr.size, t, rmap);
    }

    for (f = 0; f < frames; f++) {
       uint32_t idx;
       VkResult r;
       CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));
      r = vkAcquireNextImageKHR(dev, sc, 2000000000ull, acq, VK_NULL_HANDLE, &idx);
      if (r != VK_SUCCESS) FAIL("acquire=%d frame=%d", (int)r, f);
      CK(vkResetFences(dev, 1, &fence));
      CK(vkResetCommandBuffer(cb, 0));
      {
         VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
         CK(vkBeginCommandBuffer(cb, &bi));
      }
       if (rp) {
          if (sampled) sample_record(cb, orange, forced_copy, rbuf);
          VkClearValue cv = { .color = orange };
          if (sampled) memset(&cv, 0, sizeof(cv));
         VkRenderPassBeginInfo rbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = pass,
            .framebuffer = fbs[idx], .renderArea = { {0, 0}, {W, H} }, .clearValueCount = 1, .pClearValues = &cv };
          vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);
          if (sampled) {
             vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, sample_pipeline);
             vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, sample_layout, 0, 1, &sample_set, 0, NULL);
             vkCmdDraw(cb, 3, 1, 0, 0);
          }
         vkCmdEndRenderPass(cb);
      } else {
         VkImageSubresourceRange rng = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
         VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = imgs[idx], .subresourceRange = rng };
         vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
         vkCmdClearColorImage(cb, imgs[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &orange, 1, &rng);
         b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask = 0;
         b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
          vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &b);
       }
       /* Read the image that will be presented. Src usage was required at swapchain create. */
        if (!sampled) {
          VkImageSubresourceRange rng = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
          VkBufferImageCopy reg = { 0 };
          VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
          VkBufferMemoryBarrier bb = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
          b.srcAccessMask = rp ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
          b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
          b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
          b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          b.image = imgs[idx];
          b.subresourceRange = rng;
          vkCmdPipelineBarrier(cb,
             rp ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
          reg.bufferRowLength = W;
          reg.bufferImageHeight = H;
          reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          reg.imageSubresource.layerCount = 1;
          reg.imageExtent.width = W;
          reg.imageExtent.height = H;
          reg.imageExtent.depth = 1;
          vkCmdCopyImageToBuffer(cb, imgs[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rbuf, 1, &reg);
          b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
          b.dstAccessMask = 0;
          b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
          vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               0, 0, NULL, 0, NULL, 1, &b);
          bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
          bb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          bb.buffer = rbuf;
          bb.size = VK_WHOLE_SIZE;
          vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                               0, 0, NULL, 1, &bb, 0, NULL);
       }
      CK(vkEndCommandBuffer(cb));
      {
         VkPipelineStageFlags ws = rp ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
         VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &acq,
            .pWaitDstStageMask = &ws, .commandBufferCount = 1, .pCommandBuffers = &cb,
             .signalSemaphoreCount = 1, .pSignalSemaphores = &done[idx] };
          CK(vkQueueSubmit(q, 1, &si, fence));
       }
        if (f == frames - 1 && (!sampled || forced_copy)) {
          const unsigned char *p;
          VkMappedMemoryRange rng = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = rmem, .size = VK_WHOLE_SIZE };
          unsigned y, x;
          FILE *raw;
          char path[64];
          CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));
          CK(vkInvalidateMappedMemoryRanges(dev, 1, &rng));
          p = (const unsigned char *)rmap + ((size_t)H / 2) * (size_t)rstride + ((size_t)W / 2) * 4;
          src_orange = src_zero = src_other = 0;
          for (y = 0; y < H; y++) {
             const unsigned char *row = (const unsigned char *)rmap + (size_t)y * (size_t)rstride;
             for (x = 0; x < W; x++) {
                const unsigned char *px = row + x * 4;
                int hit = abs((int)px[2] - 255) <= 1 && abs((int)px[1] - 128) <= 1 && abs((int)px[0] - 64) <= 1 && px[3] == 255;
                if (hit) src_orange++;
                else if (!px[0] && !px[1] && !px[2]) src_zero++;
                else src_other++;
             }
          }
          snprintf(path, sizeof(path), "nclear-%s-src.raw", mode);
          raw = fopen(path, "wb");
          if (!raw) FAIL("fopen %s", path);
          if (fwrite(rmap, 1, (size_t)rstride * H, raw) != (size_t)rstride * H) FAIL("short src write");
          fclose(raw);
          printf("src mode=%s BGRA center=%02x %02x %02x %02x orange=%u zero=%u other=%u need=%u\n",
                 mode, p[0], p[1], p[2], p[3], src_orange, src_zero, src_other, W * H);
          stamp("src_readback");
       }
      {
         VkResult swapchain_result = VK_NOT_READY;
         VkPresentInfoKHR pi = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &done[idx],
            .swapchainCount = 1,
            .pSwapchains = &sc,
            .pImageIndices = &idx,
            .pResults = &swapchain_result,
         };
         stamp("before_present");
         r = vkQueuePresentKHR(q, &pi);
         stamp("after_present");
         if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
            FAIL("present queue result=%d frame=%d", (int)r, f);
         if (swapchain_result != VK_SUCCESS && swapchain_result != VK_SUBOPTIMAL_KHR)
            FAIL("present swapchain result=%d frame=%d", (int)swapchain_result, f);
         printf("present frame=%d result=%d swapchain_result=%d image=%u\n",
                f, (int)r, (int)swapchain_result, idx);
      }
   }
   /* ponytail: one grab after the last present, same moment as wsitest.
    * Per-frame XGetImage on this Display read 0 while the phone stayed black;
    * wsitest (no mid-loop XGetImage) showed the clear. Upgrade: present-wait. */
   {
      char path[64];
      XImage *xi;
      CK(vkDeviceWaitIdle(dev));
      stamp("hold");
      usleep(1200000);
      XSync(dpy, False);
      stamp("before_xgetimage");
      xi = XGetImage(dpy, win, 0, 0, W, H, AllPlanes, ZPixmap);
      stamp("after_xgetimage");
      if (!xi) FAIL("XGetImage");
      snprintf(path, sizeof(path), "nclear-%s.raw", mode);
      dump_raw(path, xi);
      count_img(xi, &xi_orange, &xi_zero, &xi_other);
      printf("final mode=%s %dx%d depth=%d bpp=%d stride=%d masks=%lx/%lx/%lx orange=%d zero=%d other=%d need=%u center=0x%06lx\n",
             mode, xi->width, xi->height, xi->depth, xi->bits_per_pixel, xi->bytes_per_line,
             xi->red_mask, xi->green_mask, xi->blue_mask, xi_orange, xi_zero, xi_other, W * H,
             XGetPixel(xi, (int)W / 2, (int)H / 2) & 0xffffff);
       XDestroyImage(xi);
       if (sampled) usleep(12000000);
   }
   stamp("before_exit");
   if (sampled) {
      /* ponytail: test-process resource lifetime; add teardown for embedding. */
      if ((forced_copy && src_orange != W * H) || (unsigned)xi_orange != W * H || xi_zero || xi_other)
         FAIL("sample pixel mismatch");
      printf("API=PASS mode=%s frames=%d source=%s drawable=ORANGE_PASS\n",
             mode, frames, forced_copy ? "ORANGE_PASS" : "UNMEASURED");
      return 0;
   }
   if (rp) {
      for (i = 0; i < nimg; i++) {
         vkDestroyFramebuffer(dev, fbs[i], NULL);
         vkDestroyImageView(dev, views[i], NULL);
      }
      vkDestroyRenderPass(dev, pass, NULL);
   }
   vkDestroySwapchainKHR(dev, sc, NULL);
    if (rmap) vkUnmapMemory(dev, rmem);
    if (rbuf) vkDestroyBuffer(dev, rbuf, NULL);
    if (rmem) vkFreeMemory(dev, rmem, NULL);
    vkDestroySemaphore(dev, acq, NULL);
    for (i = 0; i < nimg; i++) vkDestroySemaphore(dev, done[i], NULL);
   vkDestroyFence(dev, fence, NULL); vkDestroyCommandPool(dev, pool, NULL);
   vkDestroyDevice(dev, NULL);
   vkDestroySurfaceKHR(inst, surf, NULL);
   vkDestroyInstance(inst, NULL);
   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);

   unsigned total_px = W * H;
   printf("EXITGATE: src(orange=%u zero=%u other=%u need=%u) drawable(orange=%d zero=%d other=%d need=%u)\n",
          src_orange, src_zero, src_other, total_px,
          xi_orange, xi_zero, xi_other, total_px);
   if ((!sampled || forced_copy) && (src_orange != total_px || src_zero != 0 || src_other != 0))
      FAIL("source pixel count mismatch: orange=%u zero=%u other=%u need=%u",
           src_orange, src_zero, src_other, total_px);
   if ((unsigned)xi_orange != total_px || xi_zero != 0 || xi_other != 0)
      FAIL("drawable pixel count mismatch: orange=%d zero=%d other=%d need=%u",
           xi_orange, xi_zero, xi_other, total_px);

   printf("API=PASS mode=%s frames=%d\n", mode, frames);
   return 0;
}
