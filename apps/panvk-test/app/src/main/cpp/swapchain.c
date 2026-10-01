/* In-process Android swapchain lifecycle. JNI: Native.swapchainTest / swapchainPhase. */
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "clip_cull_spv.h"

#define FRAMES 2
#define MAX_IMG 16
#define WAIT_NS 2000000000ull
#define LOG_TAG "PanVKSwap"

/* Phase tracker (watchdog thread) + cached ICD. Nothing else is process-global. */
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static char g_phase[64] = "idle";
static struct timespec g_t0;
static volatile int g_done = 1;
static int g_t0_ready;
static void *g_icd;

static void
set_phase(const char *name)
{
    pthread_mutex_lock(&g_mu);
    snprintf(g_phase, sizeof g_phase, "%s", name ? name : "idle");
    clock_gettime(CLOCK_MONOTONIC, &g_t0);
    g_t0_ready = 1;
    pthread_mutex_unlock(&g_mu);
}

static void
set_done(int done)
{
    pthread_mutex_lock(&g_mu);
    g_done = done ? 1 : 0;
    pthread_mutex_unlock(&g_mu);
}

static int
phase_is(const char *name)
{
    char buf[64];
    pthread_mutex_lock(&g_mu);
    snprintf(buf, sizeof buf, "%s", g_phase);
    pthread_mutex_unlock(&g_mu);
    return strcmp(buf, name) == 0;
}

struct Images {
    VkSwapchainKHR swap;
    VkExtent2D extent;
    uint32_t n;
    VkImage img[MAX_IMG];
    VkImageView view[MAX_IMG];
    VkFramebuffer fb[MAX_IMG];
    VkSemaphore rend[MAX_IMG];
};

struct Ctx {
    FILE *log;
    ANativeWindow *win;
    int failed;
    int timed_out;
    int slot;
    VkFormat format;
    VkColorSpaceKHR color_space;
    uint32_t qf;
    VkInstance inst;
    VkPhysicalDevice phys;
    VkDevice dev;
    VkQueue queue;
    VkSurfaceKHR surf;
    VkRenderPass rp;
    VkPipelineLayout layout;
    VkPipeline pipe;
    VkCommandPool pool;
    VkCommandBuffer cmd[FRAMES];
    VkBuffer vbuf;
    VkDeviceMemory vmem;
    VkSemaphore acq[FRAMES];
    VkFence fence[FRAMES];
    VkFence img_fence[MAX_IMG];
    struct Images sc;

    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkResetCommandBuffer vkResetCommandBuffer;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkWaitForFences vkWaitForFences;
    PFN_vkResetFences vkResetFences;
    PFN_vkCreateSemaphore vkCreateSemaphore;
    PFN_vkDestroySemaphore vkDestroySemaphore;
    PFN_vkQueueSubmit vkQueueSubmit;
};

static void
lg(struct Ctx *c, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (c && c->log) {
        fputs(buf, c->log);
        fputc('\n', c->log);
        fflush(c->log);
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
}

static int
fail(struct Ctx *c, const char *what, VkResult res)
{
    char phase[64];
    if (res == VK_TIMEOUT || res == VK_NOT_READY)
        c->timed_out = 1;
    if (c->failed)
        return 0;
    c->failed = 1;
    pthread_mutex_lock(&g_mu);
    snprintf(phase, sizeof phase, "%s", g_phase);
    pthread_mutex_unlock(&g_mu);
    if (res == VK_TIMEOUT || res == VK_NOT_READY)
        lg(c, "FAIL hang in %s", phase);
    lg(c, "FAIL %s res=%d phase=%s", what, (int)res, phase);
    return 0;
}

static int
chk(struct Ctx *c, const char *what, VkResult res)
{
    return res == VK_SUCCESS ? 1 : fail(c, what, res);
}

/* Create outputs are undefined on failure. Zero the caller's 8-byte handle. */
static int
chk0(struct Ctx *c, const char *what, VkResult res, void *out)
{
    if (res == VK_SUCCESS)
        return 1;
    if (out) {
        uint64_t z = 0;
        memcpy(out, &z, sizeof z);
    }
    return fail(c, what, res);
}

static long
ms_since(const struct timespec *a)
{
    struct timespec b;
    double ms;
    clock_gettime(CLOCK_MONOTONIC, &b);
    ms = (b.tv_sec - a->tv_sec) * 1e3 + (b.tv_nsec - a->tv_nsec) / 1e6;
    if (ms < 0)
        ms = 0;
    return (long)(ms + 0.5);
}

static double
ms_exact(const struct timespec *a)
{
    struct timespec b;
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a->tv_sec) * 1e3 + (b.tv_nsec - a->tv_nsec) / 1e6;
}

#define LOAD_I(c, gipa, n)                                                     \
    do {                                                                       \
        (c)->vk##n = (PFN_vk##n)(gipa)((c)->inst, "vk" #n);                    \
        if (!(c)->vk##n)                                                       \
            return fail((c), "vk" #n, VK_ERROR_EXTENSION_NOT_PRESENT);         \
    } while (0)

#define LOAD_D(c, n)                                                           \
    do {                                                                       \
        (c)->vk##n = (PFN_vk##n)(c)->vkGetDeviceProcAddr((c)->dev, "vk" #n);   \
        if (!(c)->vk##n)                                                       \
            return fail((c), "vk" #n, VK_ERROR_EXTENSION_NOT_PRESENT);         \
    } while (0)

static int
load_instance(struct Ctx *c, PFN_vkGetInstanceProcAddr gipa)
{
    LOAD_I(c, gipa, DestroyInstance);
    LOAD_I(c, gipa, EnumeratePhysicalDevices);
    LOAD_I(c, gipa, GetPhysicalDeviceQueueFamilyProperties);
    LOAD_I(c, gipa, GetPhysicalDeviceMemoryProperties);
    LOAD_I(c, gipa, GetPhysicalDeviceSurfaceSupportKHR);
    LOAD_I(c, gipa, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_I(c, gipa, GetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_I(c, gipa, GetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_I(c, gipa, CreateAndroidSurfaceKHR);
    LOAD_I(c, gipa, DestroySurfaceKHR);
    LOAD_I(c, gipa, CreateDevice);
    LOAD_I(c, gipa, GetDeviceProcAddr);
    return 1;
}

static int
load_device(struct Ctx *c)
{
    LOAD_D(c, DestroyDevice);
    LOAD_D(c, GetDeviceQueue);
    LOAD_D(c, DeviceWaitIdle);
    LOAD_D(c, CreateSwapchainKHR);
    LOAD_D(c, DestroySwapchainKHR);
    LOAD_D(c, GetSwapchainImagesKHR);
    LOAD_D(c, AcquireNextImageKHR);
    LOAD_D(c, QueuePresentKHR);
    LOAD_D(c, CreateImageView);
    LOAD_D(c, DestroyImageView);
    LOAD_D(c, CreateRenderPass);
    LOAD_D(c, DestroyRenderPass);
    LOAD_D(c, CreateFramebuffer);
    LOAD_D(c, DestroyFramebuffer);
    LOAD_D(c, CreateShaderModule);
    LOAD_D(c, DestroyShaderModule);
    LOAD_D(c, CreatePipelineLayout);
    LOAD_D(c, DestroyPipelineLayout);
    LOAD_D(c, CreateGraphicsPipelines);
    LOAD_D(c, DestroyPipeline);
    LOAD_D(c, CreateCommandPool);
    LOAD_D(c, DestroyCommandPool);
    LOAD_D(c, AllocateCommandBuffers);
    LOAD_D(c, ResetCommandBuffer);
    LOAD_D(c, BeginCommandBuffer);
    LOAD_D(c, EndCommandBuffer);
    LOAD_D(c, CmdBeginRenderPass);
    LOAD_D(c, CmdEndRenderPass);
    LOAD_D(c, CmdBindPipeline);
    LOAD_D(c, CmdBindVertexBuffers);
    LOAD_D(c, CmdDraw);
    LOAD_D(c, CmdSetViewport);
    LOAD_D(c, CmdSetScissor);
    LOAD_D(c, CreateBuffer);
    LOAD_D(c, DestroyBuffer);
    LOAD_D(c, GetBufferMemoryRequirements);
    LOAD_D(c, AllocateMemory);
    LOAD_D(c, FreeMemory);
    LOAD_D(c, BindBufferMemory);
    LOAD_D(c, MapMemory);
    LOAD_D(c, UnmapMemory);
    LOAD_D(c, FlushMappedMemoryRanges);
    LOAD_D(c, CreateFence);
    LOAD_D(c, DestroyFence);
    LOAD_D(c, WaitForFences);
    LOAD_D(c, ResetFences);
    LOAD_D(c, CreateSemaphore);
    LOAD_D(c, DestroySemaphore);
    LOAD_D(c, QueueSubmit);
    return 1;
}

static void
images_destroy(struct Ctx *c, struct Images *im)
{
    uint32_t i;
    if (c->dev) {
        for (i = 0; i < im->n && i < MAX_IMG; i++) {
            if (im->fb[i] && c->vkDestroyFramebuffer)
                c->vkDestroyFramebuffer(c->dev, im->fb[i], NULL);
            if (im->view[i] && c->vkDestroyImageView)
                c->vkDestroyImageView(c->dev, im->view[i], NULL);
            if (im->rend[i] && c->vkDestroySemaphore)
                c->vkDestroySemaphore(c->dev, im->rend[i], NULL);
        }
        if (im->swap && c->vkDestroySwapchainKHR)
            c->vkDestroySwapchainKHR(c->dev, im->swap, NULL);
    }
    memset(im, 0, sizeof *im);
}

static int
shutdown(struct Ctx *c)
{
    uint32_t i;
    if (c->dev && c->vkDeviceWaitIdle && !c->timed_out) {
        VkResult r = c->vkDeviceWaitIdle(c->dev);
        if (r != VK_SUCCESS)
            fail(c, "vkDeviceWaitIdle", r);
    }
    images_destroy(c, &c->sc);
    memset(c->img_fence, 0, sizeof c->img_fence);
    if (c->dev) {
        for (i = 0; i < FRAMES; i++) {
            if (c->fence[i] && c->vkDestroyFence)
                c->vkDestroyFence(c->dev, c->fence[i], NULL);
            if (c->acq[i] && c->vkDestroySemaphore)
                c->vkDestroySemaphore(c->dev, c->acq[i], NULL);
            c->fence[i] = VK_NULL_HANDLE;
            c->acq[i] = VK_NULL_HANDLE;
        }
        if (c->pool && c->vkDestroyCommandPool)
            c->vkDestroyCommandPool(c->dev, c->pool, NULL);
        if (c->pipe && c->vkDestroyPipeline)
            c->vkDestroyPipeline(c->dev, c->pipe, NULL);
        if (c->layout && c->vkDestroyPipelineLayout)
            c->vkDestroyPipelineLayout(c->dev, c->layout, NULL);
        if (c->rp && c->vkDestroyRenderPass)
            c->vkDestroyRenderPass(c->dev, c->rp, NULL);
        if (c->vbuf && c->vkDestroyBuffer)
            c->vkDestroyBuffer(c->dev, c->vbuf, NULL);
        if (c->vmem && c->vkFreeMemory)
            c->vkFreeMemory(c->dev, c->vmem, NULL);
        c->pool = VK_NULL_HANDLE;
        c->pipe = VK_NULL_HANDLE;
        c->layout = VK_NULL_HANDLE;
        c->rp = VK_NULL_HANDLE;
        c->vbuf = VK_NULL_HANDLE;
        c->vmem = VK_NULL_HANDLE;
        if (c->vkDestroyDevice)
            c->vkDestroyDevice(c->dev, NULL);
        c->dev = VK_NULL_HANDLE;
    }
    if (c->win) {
        int ar = ANativeWindow_setBuffersGeometry(c->win, 0, 0, 0);
        if (ar != 0)
            fail(c, "ANativeWindow_setBuffersGeometry", ar);
    }
    if (c->surf && c->inst && c->vkDestroySurfaceKHR)
        c->vkDestroySurfaceKHR(c->inst, c->surf, NULL);
    c->surf = VK_NULL_HANDLE;
    if (c->inst && c->vkDestroyInstance)
        c->vkDestroyInstance(c->inst, NULL);
    c->inst = VK_NULL_HANDLE;
    if (c->win) {
        ANativeWindow_release(c->win);
        c->win = NULL;
    }
    return c->failed ? 0 : 1;
}

static int
open_icd(struct Ctx *c, const char *path)
{
    PFN_vkGetInstanceProcAddr gipa;
    if (!g_icd) {
        g_icd = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!g_icd)
            return fail(c, "dlopen", errno ? (VkResult)errno : -1);
    }
    gipa = (PFN_vkGetInstanceProcAddr)dlsym(g_icd, "vk_icdGetInstanceProcAddr");
    if (!gipa)
        gipa = (PFN_vkGetInstanceProcAddr)dlsym(g_icd, "vkGetInstanceProcAddr");
    if (!gipa)
        return fail(c, "dlsym", -1);
    {
        PFN_vkCreateInstance create =
            (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
        const char *exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                              VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                 .pApplicationName = "panvk-swapchain",
                                 .apiVersion = VK_API_VERSION_1_1};
        VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                    .pApplicationInfo = &app,
                                    .enabledExtensionCount = 2,
                                    .ppEnabledExtensionNames = exts};
        if (!create)
            return fail(c, "vkCreateInstance", VK_ERROR_EXTENSION_NOT_PRESENT);
        if (!chk0(c, "vkCreateInstance", create(&ici, NULL, &c->inst), &c->inst))
            return 0;
        if (!load_instance(c, gipa))
            return 0;
    }
    return 1;
}

static int
pick_gpu(struct Ctx *c)
{
    uint32_t nd = 0, d;
    VkPhysicalDevice *devs;
    int found = 0;
    if (!chk(c, "vkEnumeratePhysicalDevices",
             c->vkEnumeratePhysicalDevices(c->inst, &nd, NULL)))
        return 0;
    if (nd == 0)
        return fail(c, "vkEnumeratePhysicalDevices", VK_ERROR_INITIALIZATION_FAILED);
    devs = calloc(nd, sizeof *devs);
    if (!devs)
        return fail(c, "calloc", -12);
    if (!chk(c, "vkEnumeratePhysicalDevices",
             c->vkEnumeratePhysicalDevices(c->inst, &nd, devs))) {
        free(devs);
        return 0;
    }
    for (d = 0; d < nd && !found; d++) {
        uint32_t nq = 0, i;
        VkQueueFamilyProperties *qp;
        c->vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &nq, NULL);
        if (nq == 0)
            continue;
        qp = calloc(nq, sizeof *qp);
        if (!qp) {
            free(devs);
            return fail(c, "calloc", -12);
        }
        c->vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &nq, qp);
        for (i = 0; i < nq; i++) {
            VkBool32 sup = VK_FALSE;
            VkResult r;
            if (!(qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || qp[i].queueCount == 0)
                continue;
            r = c->vkGetPhysicalDeviceSurfaceSupportKHR(devs[d], i, c->surf, &sup);
            if (r != VK_SUCCESS) {
                free(qp);
                free(devs);
                return fail(c, "vkGetPhysicalDeviceSurfaceSupportKHR", r);
            }
            if (sup) {
                c->phys = devs[d];
                c->qf = i;
                found = 1;
                break;
            }
        }
        free(qp);
    }
    free(devs);
    if (!found)
        return fail(c, "queue", VK_ERROR_INITIALIZATION_FAILED);
    return 1;
}

static int
pick_format(struct Ctx *c)
{
    uint32_t n = 0, i;
    VkSurfaceFormatKHR *fmts;
    int unorm = -1, srgb = -1;
    if (!chk(c, "vkGetPhysicalDeviceSurfaceFormatsKHR",
             c->vkGetPhysicalDeviceSurfaceFormatsKHR(c->phys, c->surf, &n, NULL)))
        return 0;
    if (n == 0)
        return fail(c, "vkGetPhysicalDeviceSurfaceFormatsKHR",
                    VK_ERROR_FORMAT_NOT_SUPPORTED);
    fmts = calloc(n, sizeof *fmts);
    if (!fmts)
        return fail(c, "calloc", -12);
    if (!chk(c, "vkGetPhysicalDeviceSurfaceFormatsKHR",
             c->vkGetPhysicalDeviceSurfaceFormatsKHR(c->phys, c->surf, &n, fmts))) {
        free(fmts);
        return 0;
    }
    if (n == 1 && fmts[0].format == VK_FORMAT_UNDEFINED) {
        c->format = VK_FORMAT_R8G8B8A8_UNORM;
        c->color_space = fmts[0].colorSpace;
        free(fmts);
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (unorm < 0 && fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM)
            unorm = (int)i;
        if (srgb < 0 && fmts[i].format == VK_FORMAT_R8G8B8A8_SRGB)
            srgb = (int)i;
    }
    i = unorm >= 0 ? (uint32_t)unorm : srgb >= 0 ? (uint32_t)srgb : 0;
    c->format = fmts[i].format;
    c->color_space = fmts[i].colorSpace;
    free(fmts);
    return 1;
}

static int
make_rp(struct Ctx *c)
{
    VkAttachmentDescription att = {.format = c->format,
                                   .samples = VK_SAMPLE_COUNT_1_BIT,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                   .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                .colorAttachmentCount = 1,
                                .pColorAttachments = &ref};
    VkSubpassDependency dep = {.srcSubpass = VK_SUBPASS_EXTERNAL,
                               .dstSubpass = 0,
                               .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    VkRenderPassCreateInfo rp = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 1,
                                 .pAttachments = &att,
                                 .subpassCount = 1,
                                 .pSubpasses = &sub,
                                 .dependencyCount = 1,
                                 .pDependencies = &dep};
    return chk0(c, "vkCreateRenderPass", c->vkCreateRenderPass(c->dev, &rp, NULL, &c->rp), &c->rp);
}

static int
make_pipeline(struct Ctx *c)
{
    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo mi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo st[2];
    VkVertexInputBindingDescription bind = {0, 16, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bind,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attr};
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport vp = {0, 0, 1, 1, 0, 1};
    VkRect2D sc = {{0, 0}, {1, 1}};
    VkPipelineViewportStateCreateInfo vps = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &vp,
        .scissorCount = 1,
        .pScissors = &sc};
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f};
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState ba = {.colorWriteMask = 0xf};
    VkPipelineColorBlendStateCreateInfo cb = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &ba};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn};
    VkPipelineLayoutCreateInfo pl = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkGraphicsPipelineCreateInfo gp = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                       .stageCount = 2,
                                       .pStages = st,
                                       .pVertexInputState = &vi,
                                       .pInputAssemblyState = &ia,
                                       .pViewportState = &vps,
                                       .pRasterizationState = &rs,
                                       .pMultisampleState = &ms,
                                       .pColorBlendState = &cb,
                                       .pDynamicState = &ds,
                                       .renderPass = c->rp};
    int ok;
    mi.codeSize = sizeof vs_none;
    mi.pCode = vs_none;
    if (!chk(c, "vkCreateShaderModule", c->vkCreateShaderModule(c->dev, &mi, NULL, &vs)))
        return 0;
    mi.codeSize = sizeof fs_color;
    mi.pCode = fs_color;
    if (!chk(c, "vkCreateShaderModule", c->vkCreateShaderModule(c->dev, &mi, NULL, &fs))) {
        c->vkDestroyShaderModule(c->dev, vs, NULL);
        return 0;
    }
    memset(st, 0, sizeof st);
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[0].module = vs;
    st[1].module = fs;
    st[0].pName = st[1].pName = "main";
    if (!chk0(c, "vkCreatePipelineLayout",
              c->vkCreatePipelineLayout(c->dev, &pl, NULL, &c->layout), &c->layout)) {
        c->vkDestroyShaderModule(c->dev, vs, NULL);
        c->vkDestroyShaderModule(c->dev, fs, NULL);
        return 0;
    }
    gp.layout = c->layout;
    ok = chk0(c, "vkCreateGraphicsPipelines",
              c->vkCreateGraphicsPipelines(c->dev, VK_NULL_HANDLE, 1, &gp, NULL, &c->pipe),
              &c->pipe);
    c->vkDestroyShaderModule(c->dev, vs, NULL);
    c->vkDestroyShaderModule(c->dev, fs, NULL);
    return ok;
}

static int
make_buffers(struct Ctx *c)
{
    static const float verts[12] = {
        -1.f, -1.f, 0.f, 1.f, 1.f, -1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f,
    };
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = sizeof verts,
                             .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements mr;
    VkPhysicalDeviceMemoryProperties mp;
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    uint32_t i, idx = ~0u;
    int coherent = 0;
    void *p;
    if (!chk0(c, "vkCreateBuffer", c->vkCreateBuffer(c->dev, &bi, NULL, &c->vbuf), &c->vbuf))
        return 0;
    c->vkGetBufferMemoryRequirements(c->dev, c->vbuf, &mr);
    c->vkGetPhysicalDeviceMemoryProperties(c->phys, &mp);
    for (i = 0; i < mp.memoryTypeCount; i++) {
        VkMemoryPropertyFlags f;
        if (!(mr.memoryTypeBits & (1u << i)))
            continue;
        f = mp.memoryTypes[i].propertyFlags;
        if (!(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
            continue;
        if (idx == ~0u)
            idx = i;
        if (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
            idx = i;
            coherent = 1;
            break;
        }
    }
    if (idx == ~0u)
        return fail(c, "memoryType", VK_ERROR_OUT_OF_DEVICE_MEMORY);
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = idx;
    if (!chk0(c, "vkAllocateMemory", c->vkAllocateMemory(c->dev, &ai, NULL, &c->vmem), &c->vmem))
        return 0;
    if (!chk(c, "vkBindBufferMemory", c->vkBindBufferMemory(c->dev, c->vbuf, c->vmem, 0)))
        return 0;
    if (!chk(c, "vkMapMemory", c->vkMapMemory(c->dev, c->vmem, 0, mr.size, 0, &p)))
        return 0;
    memcpy(p, verts, sizeof verts);
    if (!coherent) {
        VkMappedMemoryRange range = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                     .memory = c->vmem,
                                     .size = VK_WHOLE_SIZE};
        if (!chk(c, "vkFlushMappedMemoryRanges",
                 c->vkFlushMappedMemoryRanges(c->dev, 1, &range))) {
            c->vkUnmapMemory(c->dev, c->vmem);
            return 0;
        }
    }
    c->vkUnmapMemory(c->dev, c->vmem);
    return 1;
}

static int
make_sync(struct Ctx *c)
{
    VkCommandPoolCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  .queueFamilyIndex = c->qf};
    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                      .commandBufferCount = FRAMES};
    VkFenceCreateInfo fi = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                            .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    VkSemaphoreCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    int i;
    if (!chk0(c, "vkCreateCommandPool", c->vkCreateCommandPool(c->dev, &pi, NULL, &c->pool),
              &c->pool))
        return 0;
    ai.commandPool = c->pool;
    if (!chk(c, "vkAllocateCommandBuffers",
             c->vkAllocateCommandBuffers(c->dev, &ai, c->cmd)))
        return 0;
    for (i = 0; i < FRAMES; i++) {
        if (!chk0(c, "vkCreateFence", c->vkCreateFence(c->dev, &fi, NULL, &c->fence[i]),
                  &c->fence[i]))
            return 0;
        if (!chk0(c, "vkCreateSemaphore", c->vkCreateSemaphore(c->dev, &si, NULL, &c->acq[i]),
                  &c->acq[i]))
            return 0;
    }
    return 1;
}

static int
init_all(struct Ctx *c, const char *driver)
{
    float prio = 1.f;
    const char *ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkAndroidSurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                         .window = c->win};
    VkDeviceQueueCreateInfo q = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                 .queueCount = 1,
                                 .pQueuePriorities = &prio};
    VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &q,
                             .enabledExtensionCount = 1,
                             .ppEnabledExtensionNames = &ext};
    set_phase("init");
    if (!open_icd(c, driver))
        return 0;
    if (!chk0(c, "vkCreateAndroidSurfaceKHR",
              c->vkCreateAndroidSurfaceKHR(c->inst, &sci, NULL, &c->surf), &c->surf))
        return 0;
    if (!pick_gpu(c))
        return 0;
    q.queueFamilyIndex = c->qf;
    if (!chk0(c, "vkCreateDevice", c->vkCreateDevice(c->phys, &di, NULL, &c->dev), &c->dev))
        return 0;
    if (!load_device(c))
        return 0;
    c->vkGetDeviceQueue(c->dev, c->qf, 0, &c->queue);
    if (!pick_format(c) || !make_rp(c) || !make_pipeline(c) || !make_buffers(c) ||
        !make_sync(c))
        return 0;
    return 1;
}

static int
pick_extent(struct Ctx *c, const VkSurfaceCapabilitiesKHR *caps, VkExtent2D *out)
{
    if (caps->currentExtent.width != 0xffffffffu) {
        *out = caps->currentExtent;
    } else {
        int32_t w = ANativeWindow_getWidth(c->win);
        int32_t h = ANativeWindow_getHeight(c->win);
        uint32_t uw, uh;
        if (w <= 0 || h <= 0)
            return fail(c, "ANativeWindow_getWidth", w < 0 ? w : h);
        uw = (uint32_t)w;
        uh = (uint32_t)h;
        if (uw < caps->minImageExtent.width)
            uw = caps->minImageExtent.width;
        if (uh < caps->minImageExtent.height)
            uh = caps->minImageExtent.height;
        if (uw > caps->maxImageExtent.width)
            uw = caps->maxImageExtent.width;
        if (uh > caps->maxImageExtent.height)
            uh = caps->maxImageExtent.height;
        out->width = uw;
        out->height = uh;
    }
    if (out->width == 0 || out->height == 0)
        return fail(c, "extent", VK_ERROR_INITIALIZATION_FAILED);
    return 1;
}

static int
sc_fill(struct Ctx *c, VkSwapchainKHR old, struct Images *out, int banner)
{
    VkSurfaceCapabilitiesKHR caps;
    VkExtent2D ext;
    VkPresentModeKHR stack[16], *modes = stack, *heap = NULL;
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkSurfaceTransformFlagBitsKHR pre;
    VkSwapchainCreateInfoKHR sci;
    struct Images im;
    uint32_t npm = 0, want, i;
    int fifo = 0;
    static const VkCompositeAlphaFlagBitsKHR order[] = {
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    memset(&im, 0, sizeof im);
    if (!chk(c, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
             c->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c->phys, c->surf, &caps)))
        return 0;
    if (!pick_extent(c, &caps, &ext))
        return 0;
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
        return fail(c, "imageUsage", VK_ERROR_FORMAT_NOT_SUPPORTED);
    if (!chk(c, "vkGetPhysicalDeviceSurfacePresentModesKHR",
             c->vkGetPhysicalDeviceSurfacePresentModesKHR(c->phys, c->surf, &npm, NULL)))
        return 0;
    if (npm == 0)
        return fail(c, "vkGetPhysicalDeviceSurfacePresentModesKHR",
                    VK_ERROR_FORMAT_NOT_SUPPORTED);
    if (npm > 16) {
        heap = calloc(npm, sizeof *heap);
        if (!heap)
            return fail(c, "calloc", -12);
        modes = heap;
    }
    if (!chk(c, "vkGetPhysicalDeviceSurfacePresentModesKHR",
             c->vkGetPhysicalDeviceSurfacePresentModesKHR(c->phys, c->surf, &npm, modes))) {
        free(heap);
        return 0;
    }
    for (i = 0; i < npm; i++)
        if (modes[i] == VK_PRESENT_MODE_FIFO_KHR)
            fifo = 1;
    free(heap);
    if (!fifo)
        return fail(c, "FIFO", VK_ERROR_FORMAT_NOT_SUPPORTED);
    for (i = 0; i < 4; i++)
        if (caps.supportedCompositeAlpha & order[i]) {
            alpha = order[i];
            break;
        }
    if (i == 4)
        return fail(c, "compositeAlpha", VK_ERROR_FORMAT_NOT_SUPPORTED);
    pre = caps.currentTransform;
    if (!pre) {
        if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            pre = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            return fail(c, "preTransform", VK_ERROR_FORMAT_NOT_SUPPORTED);
    }
    want = caps.minImageCount < 3 ? 3 : caps.minImageCount;
    if (caps.maxImageCount && want > caps.maxImageCount)
        want = caps.maxImageCount;
    memset(&sci, 0, sizeof sci);
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = c->surf;
    sci.minImageCount = want;
    sci.imageFormat = c->format;
    sci.imageColorSpace = c->color_space;
    sci.imageExtent = ext;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = pre;
    sci.compositeAlpha = alpha;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old;
    if (!chk0(c, "vkCreateSwapchainKHR",
              c->vkCreateSwapchainKHR(c->dev, &sci, NULL, &im.swap), &im.swap))
        return 0;
    if (!chk(c, "vkGetSwapchainImagesKHR",
             c->vkGetSwapchainImagesKHR(c->dev, im.swap, &im.n, NULL)) ||
        im.n == 0 || im.n > MAX_IMG) {
        if (im.n == 0 || im.n > MAX_IMG)
            fail(c, "vkGetSwapchainImagesKHR", VK_ERROR_TOO_MANY_OBJECTS);
        images_destroy(c, &im);
        return 0;
    }
    if (!chk(c, "vkGetSwapchainImagesKHR",
             c->vkGetSwapchainImagesKHR(c->dev, im.swap, &im.n, im.img))) {
        images_destroy(c, &im);
        return 0;
    }
    im.extent = ext;
    for (i = 0; i < im.n; i++) {
        VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                    .image = im.img[i],
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = c->format,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VkFramebufferCreateInfo fi = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                      .renderPass = c->rp,
                                      .attachmentCount = 1,
                                      .pAttachments = &im.view[i],
                                      .width = ext.width,
                                      .height = ext.height,
                                      .layers = 1};
        VkSemaphoreCreateInfo se = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (!chk0(c, "vkCreateImageView",
                  c->vkCreateImageView(c->dev, &vi, NULL, &im.view[i]), &im.view[i]) ||
            !chk0(c, "vkCreateFramebuffer",
                  c->vkCreateFramebuffer(c->dev, &fi, NULL, &im.fb[i]), &im.fb[i]) ||
            !chk0(c, "vkCreateSemaphore",
                  c->vkCreateSemaphore(c->dev, &se, NULL, &im.rend[i]), &im.rend[i])) {
            images_destroy(c, &im);
            return 0;
        }
    }
    if (banner)
        lg(c, "SWAPCHAIN %ux%u images=%u format=%u", ext.width, ext.height, im.n,
           (unsigned)c->format);
    *out = im;
    return 1;
}

static int
wait_frame_fences(struct Ctx *c)
{
    int i;
    for (i = 0; i < FRAMES; i++) {
        VkResult r;
        if (!c->fence[i])
            continue;
        r = c->vkWaitForFences(c->dev, 1, &c->fence[i], VK_TRUE, WAIT_NS);
        if (r != VK_SUCCESS)
            return fail(c, "vkWaitForFences", r);
    }
    return 1;
}

static int
sc_recreate(struct Ctx *c)
{
    struct Images neu;
    VkExtent2D prev = c->sc.extent;
    memset(&neu, 0, sizeof neu);
    if (!wait_frame_fences(c))
        return 0;
    if (!sc_fill(c, c->sc.swap, &neu, 0))
        return 0;
    if (!c->timed_out && c->vkDeviceWaitIdle) {
        VkResult r = c->vkDeviceWaitIdle(c->dev);
        if (r != VK_SUCCESS) {
            images_destroy(c, &neu);
            return fail(c, "vkDeviceWaitIdle", r);
        }
    }
    images_destroy(c, &c->sc);
    c->sc = neu;
    memset(c->img_fence, 0, sizeof c->img_fence);
    if (neu.extent.width == prev.width && neu.extent.height == prev.height)
        lg(c, "RESIZE extent unchanged, recreated with oldSwapchain");
    return 1;
}

static int
record_frame(struct Ctx *c, int slot, uint32_t img, int index)
{
    VkCommandBuffer cmd = c->cmd[slot];
    VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkViewport vp = {0, 0, (float)c->sc.extent.width, (float)c->sc.extent.height, 0.f, 1.f};
    VkRect2D sc = {{0, 0}, c->sc.extent};
    float t = (float)(index % 256) / 255.f;
    VkClearValue clear = {.color = {{t, 0.15f, 1.f - t, 1.f}}};
    VkRenderPassBeginInfo rp = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = c->rp,
                                .framebuffer = c->sc.fb[img],
                                .renderArea = sc,
                                .clearValueCount = 1,
                                .pClearValues = &clear};
    VkDeviceSize off = 0;
    if (!chk(c, "vkResetCommandBuffer", c->vkResetCommandBuffer(cmd, 0)))
        return 0;
    if (!chk(c, "vkBeginCommandBuffer", c->vkBeginCommandBuffer(cmd, &bi)))
        return 0;
    c->vkCmdSetViewport(cmd, 0, 1, &vp);
    c->vkCmdSetScissor(cmd, 0, 1, &sc);
    c->vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    c->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, c->pipe);
    c->vkCmdBindVertexBuffers(cmd, 0, 1, &c->vbuf, &off);
    c->vkCmdDraw(cmd, 3, 1, 0, 0);
    c->vkCmdEndRenderPass(cmd);
    return chk(c, "vkEndCommandBuffer", c->vkEndCommandBuffer(cmd));
}

static int
draw_frame(struct Ctx *c, int index)
{
    int slot = c->slot;
    uint32_t img = 0;
    VkResult r;
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si;
    VkPresentInfoKHR pi;
    r = c->vkWaitForFences(c->dev, 1, &c->fence[slot], VK_TRUE, WAIT_NS);
    if (r != VK_SUCCESS)
        return fail(c, "vkWaitForFences", r);
    r = c->vkAcquireNextImageKHR(c->dev, c->sc.swap, WAIT_NS, c->acq[slot],
                                 VK_NULL_HANDLE, &img);
    if (r == VK_ERROR_OUT_OF_DATE_KHR)
        return phase_is("resize") ? sc_recreate(c) : fail(c, "vkAcquireNextImageKHR", r);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        return fail(c, "vkAcquireNextImageKHR", r);
    if (img >= c->sc.n)
        return fail(c, "vkAcquireNextImageKHR", VK_ERROR_DEVICE_LOST);
    if (c->img_fence[img]) {
        r = c->vkWaitForFences(c->dev, 1, &c->img_fence[img], VK_TRUE, WAIT_NS);
        if (r != VK_SUCCESS)
            return fail(c, "vkWaitForFences", r);
    }
    if (!chk(c, "vkResetFences", c->vkResetFences(c->dev, 1, &c->fence[slot])))
        return 0;
    c->img_fence[img] = c->fence[slot];
    if (!record_frame(c, slot, img, index))
        return 0;
    memset(&si, 0, sizeof si);
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &c->acq[slot];
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &c->cmd[slot];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &c->sc.rend[img];
    if (!chk(c, "vkQueueSubmit", c->vkQueueSubmit(c->queue, 1, &si, c->fence[slot])))
        return 0;
    memset(&pi, 0, sizeof pi);
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &c->sc.rend[img];
    pi.swapchainCount = 1;
    pi.pSwapchains = &c->sc.swap;
    pi.pImageIndices = &img;
    r = c->vkQueuePresentKHR(c->queue, &pi);
    c->slot = (slot + 1) % FRAMES;
    if (r == VK_ERROR_OUT_OF_DATE_KHR)
        return phase_is("resize") ? sc_recreate(c) : fail(c, "vkQueuePresentKHR", r);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        return fail(c, "vkQueuePresentKHR", r);
    return 1;
}

static int
present_n(struct Ctx *c, int frames, int kind)
{
    struct timespec a;
    int i;
    double ms;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < frames; i++)
        if (!draw_frame(c, i))
            return 0;
    if (kind < 0)
        return 1;
    ms = ms_exact(&a);
    if (ms < 0.001)
        ms = 0.001;
    lg(c, "%s %.1f", kind == 0 ? "FPS" : "FPS_RESIZED", frames * 1000.0 / ms);
    if (kind == 0)
        lg(c, "PHASE present_main ms=%ld frames=%d", (long)(ms + 0.5), frames);
    else
        lg(c, "PHASE present_resized ms=%ld", (long)(ms + 0.5));
    return 1;
}

static int
idle_destroy_swap(struct Ctx *c)
{
    if (c->dev && c->vkDeviceWaitIdle && !c->timed_out) {
        VkResult r = c->vkDeviceWaitIdle(c->dev);
        if (r != VK_SUCCESS) {
            images_destroy(c, &c->sc);
            memset(c->img_fence, 0, sizeof c->img_fence);
            return fail(c, "vkDeviceWaitIdle", r);
        }
    }
    images_destroy(c, &c->sc);
    memset(c->img_fence, 0, sizeof c->img_fence);
    return 1;
}

static int
do_resize(struct Ctx *c)
{
    struct timespec a;
    int32_t W, H, ar;
    clock_gettime(CLOCK_MONOTONIC, &a);
    W = ANativeWindow_getWidth(c->win);
    H = ANativeWindow_getHeight(c->win);
    if (W <= 0 || H <= 0)
        return fail(c, "ANativeWindow_getWidth", W < 0 ? W : H);
    ar = ANativeWindow_setBuffersGeometry(c->win, W / 2, H / 2, 0);
    if (ar != 0)
        return fail(c, "ANativeWindow_setBuffersGeometry", ar);
    if (!sc_recreate(c))
        return 0;
    lg(c, "PHASE resize ms=%ld", ms_since(&a));
    return 1;
}

static int
phase_cycle(struct Ctx *c)
{
    struct timespec a;
    int n;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (n = 0; n < 10; n++) {
        char name[32];
        snprintf(name, sizeof name, "cycle_%d_create", n);
        set_phase(name);
        if (!sc_fill(c, VK_NULL_HANDLE, &c->sc, 0))
            return 0;
        snprintf(name, sizeof name, "cycle_%d_present", n);
        set_phase(name);
        if (!present_n(c, 10, -1))
            return 0;
        snprintf(name, sizeof name, "cycle_%d_destroy", n);
        set_phase(name);
        if (!idle_destroy_swap(c))
            return 0;
    }
    lg(c, "PHASE cycle ms=%ld", ms_since(&a));
    return 1;
}

static int
run(struct Ctx *c, const char *driver)
{
    struct timespec a;
    if (!init_all(c, driver))
        return 0;
    set_phase("create_swapchain");
    if (!sc_fill(c, VK_NULL_HANDLE, &c->sc, 1))
        return 0;
    set_phase("present_main");
    if (!present_n(c, 300, 0))
        return 0;
    set_phase("resize");
    if (!do_resize(c))
        return 0;
    set_phase("present_resized");
    if (!present_n(c, 120, 1))
        return 0;
    set_phase("destroy");
    clock_gettime(CLOCK_MONOTONIC, &a);
    if (!idle_destroy_swap(c))
        return 0;
    lg(c, "PHASE destroy ms=%ld", ms_since(&a));
    if (!phase_cycle(c))
        return 0;
    set_phase("teardown");
    clock_gettime(CLOCK_MONOTONIC, &a);
    if (!shutdown(c))
        return 0;
    lg(c, "PHASE teardown ms=%ld", ms_since(&a));
    lg(c, "PASS swapchain_lifecycle");
    return 1;
}

JNIEXPORT jstring JNICALL
Java_dev_zenithblue_panvktest_Native_swapchainPhase(JNIEnv *env, jclass cls)
{
    char phase[64], buf[96];
    struct timespec t0, now;
    int done;
    long ms;
    (void)cls;
    pthread_mutex_lock(&g_mu);
    if (!g_t0_ready) {
        clock_gettime(CLOCK_MONOTONIC, &g_t0);
        g_t0_ready = 1;
    }
    snprintf(phase, sizeof phase, "%s", g_phase);
    t0 = g_t0;
    done = g_done;
    pthread_mutex_unlock(&g_mu);
    clock_gettime(CLOCK_MONOTONIC, &now);
    ms = (long)(now.tv_sec - t0.tv_sec) * 1000L +
         (long)((now.tv_nsec - t0.tv_nsec) / 1000000L);
    if (ms < 0)
        ms = 0;
    snprintf(buf, sizeof buf, "%s %ld %d", phase[0] ? phase : "idle", ms, done ? 1 : 0);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_dev_zenithblue_panvktest_Native_swapchainTest(JNIEnv *env, jclass cls,
                                                   jstring j_driver, jobject surface,
                                                   jstring j_log)
{
    struct Ctx c;
    const char *driver = NULL, *log_path = NULL;
    int passed = 0;
    (void)cls;
    memset(&c, 0, sizeof c);
    set_done(0);
    set_phase("init");
    if (j_driver)
        driver = (*env)->GetStringUTFChars(env, j_driver, NULL);
    if (j_log)
        log_path = (*env)->GetStringUTFChars(env, j_log, NULL);
    if (!driver || !log_path || !surface) {
        fail(&c, "jni", -1);
        goto out;
    }
    c.log = fopen(log_path, "w");
    if (!c.log) {
        fail(&c, "fopen", errno ? (VkResult)errno : -1);
        goto out;
    }
    setvbuf(c.log, NULL, _IOLBF, 0);
    c.win = ANativeWindow_fromSurface(env, surface);
    if (!c.win) {
        fail(&c, "ANativeWindow_fromSurface", -1);
        goto out;
    }
    passed = run(&c, driver);
out:
    if (!passed)
        shutdown(&c);
    if (c.log) {
        fclose(c.log);
        c.log = NULL;
    }
    if (driver)
        (*env)->ReleaseStringUTFChars(env, j_driver, driver);
    if (log_path)
        (*env)->ReleaseStringUTFChars(env, j_log, log_path);
    set_done(1);
    return (*env)->NewStringUTF(env, passed && !c.failed ? "exit:0" : "exit:1");
}
