/* VK_LAYER_panvk_fbread
 * ponytail: CPU readback via a staging copy each present. Ceiling is a full-frame
 * synchronous map. Upgrade path: AHB import + SurfaceControl.
 */
#include <vulkan/vulkan.h>
#if __has_include(<vulkan/vk_layer.h>)
#include <vulkan/vk_layer.h>
#else
/* Khronos Vulkan-Loader vk_layer.h (NDK r29/r30). Field order matches. */
typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_GetPhysicalDeviceProcAddr)(VkInstance instance, const char *pName);
typedef enum VkNegotiateLayerStructType { LAYER_NEGOTIATE_UNINTIALIZED = 0, LAYER_NEGOTIATE_INTERFACE_STRUCT = 1 } VkNegotiateLayerStructType;
typedef struct VkNegotiateLayerInterface {
    VkNegotiateLayerStructType sType; void *pNext; uint32_t loaderLayerInterfaceVersion;
    PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr; PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr;
    PFN_GetPhysicalDeviceProcAddr pfnGetPhysicalDeviceProcAddr;
} VkNegotiateLayerInterface;
typedef enum VkLayerFunction_ { VK_LAYER_LINK_INFO = 0, VK_LOADER_DATA_CALLBACK = 1, VK_LOADER_LAYER_CREATE_DEVICE_CALLBACK = 2, VK_LOADER_FEATURES = 3 } VkLayerFunction;
typedef struct VkLayerInstanceLink_ { struct VkLayerInstanceLink_ *pNext; PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr; PFN_GetPhysicalDeviceProcAddr pfnNextGetPhysicalDeviceProcAddr; } VkLayerInstanceLink;
typedef VkResult (VKAPI_PTR *PFN_vkSetInstanceLoaderData)(VkInstance instance, void *object);
typedef VkResult (VKAPI_PTR *PFN_vkSetDeviceLoaderData)(VkDevice device, void *object);
typedef VkResult (VKAPI_PTR *PFN_vkLayerCreateDevice)(VkInstance instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice, PFN_vkGetInstanceProcAddr layerGIPA, PFN_vkGetDeviceProcAddr *nextGDPA);
typedef void (VKAPI_PTR *PFN_vkLayerDestroyDevice)(VkDevice physicalDevice, const VkAllocationCallbacks *pAllocator, PFN_vkDestroyDevice destroyFunction);
typedef enum VkLoaderFeastureFlagBits { VK_LOADER_FEATURE_PHYSICAL_DEVICE_SORTING = 1 } VkLoaderFlagBits;
typedef VkFlags VkLoaderFeatureFlags;
typedef struct { VkStructureType sType; const void *pNext; VkLayerFunction function; union { VkLayerInstanceLink *pLayerInfo; PFN_vkSetInstanceLoaderData pfnSetInstanceLoaderData; struct { PFN_vkLayerCreateDevice pfnLayerCreateDevice; PFN_vkLayerDestroyDevice pfnLayerDestroyDevice; } layerDevice; VkLoaderFeatureFlags loaderFeatures; } u; } VkLayerInstanceCreateInfo;
typedef struct VkLayerDeviceLink_ { struct VkLayerDeviceLink_ *pNext; PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr; PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr; } VkLayerDeviceLink;
typedef struct { VkStructureType sType; const void *pNext; VkLayerFunction function; union { VkLayerDeviceLink *pLayerInfo; PFN_vkSetDeviceLoaderData pfnSetDeviceLoaderData; } u; } VkLayerDeviceCreateInfo;
#endif
#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define EXPORT __attribute__((visibility("default")))
#define MAX_I 4
#define MAX_D 4
#define MAX_SC 8
#define MAX_Q 16
#define MAX_IMG 8
#define FB_MAGIC 0x31424650u
#define DEV_FNS(X) \
    X(DestroyDevice) X(GetDeviceQueue) X(GetDeviceQueue2) X(CreateSwapchainKHR) X(DestroySwapchainKHR) \
    X(GetSwapchainImagesKHR) X(QueuePresentKHR) X(QueueSubmit) X(QueueWaitIdle) X(DeviceWaitIdle) \
    X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandBuffer) \
    X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdPipelineBarrier) X(CmdCopyImageToBuffer) \
    X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) X(CreateSemaphore) X(DestroySemaphore) \
    X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) \
    X(BindBufferMemory) X(MapMemory) X(InvalidateMappedMemoryRanges)
#define CHAIN(fn, T, st) static T *fn(const void *pn, VkLayerFunction want) { \
    for (const VkBaseInStructure *p = pn; p; p = p->pNext) if (p->sType == st) { \
        T *c = (T *)p; if (c->function == want) return c; } return NULL; }

typedef struct {
    void *key; PFN_vkGetInstanceProcAddr gipa; PFN_vkDestroyInstance DestroyInstance;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetCaps;
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetCaps2;
    PFN_vkGetPhysicalDeviceMemoryProperties GetMemProps;
} Inst;
typedef struct {
    void *key; VkDevice device; PFN_vkGetDeviceProcAddr gdpa; PFN_vkSetDeviceLoaderData set_loader;
    uint32_t qfam; int mem_ok; VkPhysicalDeviceMemoryProperties mem;
#define DECL(n) PFN_vk##n n;
    DEV_FNS(DECL)
#undef DECL
} Dev;
typedef struct {
    int live, ready, busy, coherent; VkSwapchainKHR sc; void *devkey; VkFormat format;
    uint32_t w, h, nimg, qfam; VkImage imgs[MAX_IMG];
    VkCommandPool pool; VkCommandBuffer cmd; VkFence fence; VkSemaphore sem;
    VkBuffer buf; VkDeviceMemory mem; void *map; VkDeviceSize bytes;
} Sc;
typedef struct { VkQueue queue; void *devkey; uint32_t family; } QRec;

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static Inst g_in[MAX_I]; static Dev g_dev[MAX_D]; static Sc g_sc[MAX_SC]; static QRec g_q[MAX_Q];
static int g_fd = -1, g_errn, g_logged; static void *g_map; static size_t g_maplen;
static uint32_t g_fw, g_fh, g_frames;

static void *key_of(const void *o) { return o ? *(void *const *)o : NULL; }
static int enabled(void) { const char *p = getenv("PANVK_FB_PATH"); return p && p[0]; }
static void loge(const char *s) {
    if (__atomic_fetch_add(&g_errn, 1, __ATOMIC_RELAXED) >= 5) return;
    __android_log_print(ANDROID_LOG_ERROR, "fbread", "%s", s); fprintf(stderr, "fbread: %s\n", s);
}
static void logef(const char *fmt, int code) { char b[160]; snprintf(b, sizeof b, fmt, code); loge(b); }
static void log_sc_once(uint32_t w, uint32_t h, unsigned fmt) {
    if (g_logged) return; g_logged = 1;
    __android_log_print(ANDROID_LOG_INFO, "fbread", "swapchain %u x %u format %u", w, h, fmt);
    fprintf(stderr, "fbread: swapchain %u x %u format %u\n", w, h, fmt);
}
static int fmt_ok(VkFormat f) {
    return f == VK_FORMAT_B8G8R8A8_UNORM || f == VK_FORMAT_B8G8R8A8_SRGB
        || f == VK_FORMAT_R8G8B8A8_UNORM || f == VK_FORMAT_R8G8B8A8_SRGB;
}
static int fmt_swap(VkFormat f) { return f == VK_FORMAT_B8G8R8A8_UNORM || f == VK_FORMAT_B8G8R8A8_SRGB; }
static Inst *inst_of(void *k) { for (int i = 0; i < MAX_I; i++) if (g_in[i].key == k) return &g_in[i]; return NULL; }
static Dev *dev_of(void *k) { for (int i = 0; i < MAX_D; i++) if (g_dev[i].key == k) return &g_dev[i]; return NULL; }
static Sc *sc_of(VkSwapchainKHR sc) { for (int i = 0; i < MAX_SC; i++) if (g_sc[i].live && g_sc[i].sc == sc) return &g_sc[i]; return NULL; }
static void q_add(VkQueue q, void *devkey, uint32_t fam) {
    int free_i = -1;
    for (int i = 0; i < MAX_Q; i++) {
        if (g_q[i].queue == q) { g_q[i].devkey = devkey; g_q[i].family = fam; return; }
        if (free_i < 0 && !g_q[i].queue) free_i = i;
    }
    if (free_i >= 0) { g_q[free_i].queue = q; g_q[free_i].devkey = devkey; g_q[free_i].family = fam; }
}
static uint32_t q_family(VkQueue q, uint32_t fb) { for (int i = 0; i < MAX_Q; i++) if (g_q[i].queue == q) return g_q[i].family; return fb; }
CHAIN(inst_chain, VkLayerInstanceCreateInfo, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO)
CHAIN(dev_chain, VkLayerDeviceCreateInfo, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
static int pick_mem(const Dev *d, uint32_t bits, VkMemoryPropertyFlags need) {
    for (uint32_t i = 0; i < d->mem.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (d->mem.memoryTypes[i].propertyFlags & need) == need) return (int)i;
    return -1;
}
static void sc_free(Dev *d, Sc *s) {
    if (s->busy && d->DeviceWaitIdle) d->DeviceWaitIdle(d->device);
    if (s->pool && d->DestroyCommandPool) d->DestroyCommandPool(d->device, s->pool, NULL);
    if (s->fence && d->DestroyFence) d->DestroyFence(d->device, s->fence, NULL);
    if (s->sem && d->DestroySemaphore) d->DestroySemaphore(d->device, s->sem, NULL);
    if (s->buf && d->DestroyBuffer) d->DestroyBuffer(d->device, s->buf, NULL);
    if (s->mem && d->FreeMemory) d->FreeMemory(d->device, s->mem, NULL);
    s->pool = VK_NULL_HANDLE; s->cmd = VK_NULL_HANDLE; s->fence = VK_NULL_HANDLE; s->sem = VK_NULL_HANDLE;
    s->buf = VK_NULL_HANDLE; s->mem = VK_NULL_HANDLE; s->map = NULL; s->ready = s->busy = 0; s->bytes = 0;
}
static int sc_ensure(Dev *d, Sc *s, uint32_t fam) {
    if (!s->w || !s->h || s->h > (UINT32_MAX / 4u) / s->w) return -1;
    VkDeviceSize bytes = (VkDeviceSize)s->w * s->h * 4u;
    if (s->ready && s->qfam == fam && s->bytes == bytes) return 0;
    if (s->pool || s->buf || s->ready) sc_free(d, s);
    if (!d->mem_ok || !d->CreateCommandPool || !d->AllocateCommandBuffers || !d->CreateFence || !d->CreateSemaphore
        || !d->CreateBuffer || !d->GetBufferMemoryRequirements || !d->AllocateMemory || !d->BindBufferMemory || !d->MapMemory)
        return -1;
    VkCommandPoolCreateInfo pci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = fam };
    if (d->CreateCommandPool(d->device, &pci, NULL, &s->pool) != VK_SUCCESS) return -1;
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = s->pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    if (d->AllocateCommandBuffers(d->device, &ai, &s->cmd) != VK_SUCCESS) return -1;
    if (!d->set_loader || d->set_loader(d->device, s->cmd) != VK_SUCCESS) *(void **)s->cmd = *(void **)d->device;
    VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkSemaphoreCreateInfo sei = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (d->CreateFence(d->device, &fi, NULL, &s->fence) != VK_SUCCESS) return -1;
    if (d->CreateSemaphore(d->device, &sei, NULL, &s->sem) != VK_SUCCESS) return -1;
    VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (d->CreateBuffer(d->device, &bi, NULL, &s->buf) != VK_SUCCESS) return -1;
    VkMemoryRequirements req; d->GetBufferMemoryRequirements(d->device, s->buf, &req);
    int mt = pick_mem(d, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    s->coherent = mt >= 0;
    if (mt < 0) mt = pick_mem(d, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (mt < 0 || (!s->coherent && !d->InvalidateMappedMemoryRanges)) return -1;
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size, .memoryTypeIndex = (uint32_t)mt };
    if (d->AllocateMemory(d->device, &mai, NULL, &s->mem) != VK_SUCCESS) return -1;
    if (d->BindBufferMemory(d->device, s->buf, s->mem, 0) != VK_SUCCESS) return -1;
    if (d->MapMemory(d->device, s->mem, 0, VK_WHOLE_SIZE, 0, &s->map) != VK_SUCCESS) return -1;
    s->bytes = bytes; s->qfam = fam; s->ready = 1; return 0;
}
static void barrier_img(Dev *d, VkCommandBuffer cmd, VkImage image, VkImageLayout ol, VkImageLayout nl,
    VkAccessFlags sa, VkAccessFlags da, VkPipelineStageFlags ss, VkPipelineStageFlags ds) {
    VkImageMemoryBarrier ib = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = sa, .dstAccessMask = da,
        .oldLayout = ol, .newLayout = nl, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
    d->CmdPipelineBarrier(cmd, ss, ds, 0, 0, NULL, 0, NULL, 1, &ib);
}
static VkResult record_copy(Dev *d, Sc *s, VkImage image) {
    if (!d->ResetCommandBuffer || !d->BeginCommandBuffer || !d->EndCommandBuffer || !d->CmdPipelineBarrier || !d->CmdCopyImageToBuffer)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = d->ResetCommandBuffer(s->cmd, 0); if (r != VK_SUCCESS) return r;
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    r = d->BeginCommandBuffer(s->cmd, &bi); if (r != VK_SUCCESS) return r;
    barrier_img(d, s->cmd, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy c = { .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
        .imageExtent = { s->w, s->h, 1 } };
    d->CmdCopyImageToBuffer(s->cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s->buf, 1, &c);
    barrier_img(d, s->cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_READ_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    VkBufferMemoryBarrier bb = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = s->buf, .size = VK_WHOLE_SIZE };
    d->CmdPipelineBarrier(s->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0, NULL);
    return d->EndCommandBuffer(s->cmd);
}
static int fb_map(uint32_t w, uint32_t h) {
    const char *path = getenv("PANVK_FB_PATH");
    if (!path || !path[0] || !w || !h || h > (UINT32_MAX / 4u) / w) return -1;
    size_t need = 32u + (size_t)w * h * 4u;
    if (g_map && g_fw == w && g_fh == h && g_maplen == need) return 0;
    if (g_fd < 0 && (g_fd = open(path, O_RDWR | O_CREAT, 0600)) < 0) { loge(strerror(errno)); return -1; }
    if (ftruncate(g_fd, (off_t)need) != 0) { loge("ftruncate failed"); return -1; }
    if (g_map) { munmap(g_map, g_maplen); g_map = NULL; }
    void *p = mmap(NULL, need, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (p == MAP_FAILED) { loge(strerror(errno)); return -1; }
    g_map = p; g_maplen = need; g_fw = w; g_fh = h;
    uint32_t *hdr = p; hdr[0] = FB_MAGIC; hdr[7] = 0; __atomic_store_n(&hdr[1], 0u, __ATOMIC_RELAXED);
    return 0;
}
static void publish(uint32_t w, uint32_t h, VkFormat fmt, const uint8_t *src) {
    uint32_t *hdr = g_map; uint8_t *dst = (uint8_t *)g_map + 32; size_t n = (size_t)w * h;
    __atomic_fetch_add(&hdr[1], 1u, __ATOMIC_RELEASE); __atomic_thread_fence(__ATOMIC_RELEASE);
    hdr[0] = FB_MAGIC; hdr[2] = w; hdr[3] = h; hdr[4] = (uint32_t)fmt; hdr[5] = w * 4u; hdr[6] = ++g_frames; hdr[7] = 0;
    if (!fmt_swap(fmt)) memcpy(dst, src, n * 4u);
    else for (size_t i = 0; i < n; i++, dst += 4, src += 4) { dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3]; }
    __atomic_thread_fence(__ATOMIC_RELEASE); __atomic_fetch_add(&hdr[1], 1u, __ATOMIC_RELEASE);
}
static VkResult VKAPI_CALL fb_CreateInstance(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *ac, VkInstance *out) {
    VkLayerInstanceCreateInfo *link = inst_chain(ci ? ci->pNext : NULL, VK_LAYER_LINK_INFO);
    if (!link || !link->u.pLayerInfo || !out) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr ngipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    PFN_vkCreateInstance nxt = (PFN_vkCreateInstance)ngipa(NULL, "vkCreateInstance");
    if (!nxt) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = nxt(ci, ac, out); if (r != VK_SUCCESS) return r;
    pthread_mutex_lock(&g_mu);
    Inst *in = NULL; for (int i = 0; i < MAX_I; i++) if (!g_in[i].key) { in = &g_in[i]; break; }
    if (!in) loge("instance table full");
    else {
        memset(in, 0, sizeof *in); in->key = key_of(*out); in->gipa = ngipa;
        in->DestroyInstance = (PFN_vkDestroyInstance)ngipa(*out, "vkDestroyInstance");
        in->GetCaps = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)ngipa(*out, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        in->GetCaps2 = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)ngipa(*out, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
        in->GetMemProps = (PFN_vkGetPhysicalDeviceMemoryProperties)ngipa(*out, "vkGetPhysicalDeviceMemoryProperties");
    }
    pthread_mutex_unlock(&g_mu); return r;
}
static void VKAPI_CALL fb_DestroyInstance(VkInstance instance, const VkAllocationCallbacks *ac) {
    pthread_mutex_lock(&g_mu);
    Inst *in = inst_of(key_of(instance)); PFN_vkDestroyInstance fn = in ? in->DestroyInstance : NULL;
    if (in) memset(in, 0, sizeof *in);
    pthread_mutex_unlock(&g_mu); if (fn) fn(instance, ac);
}
static VkResult VKAPI_CALL fb_CreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *ac, VkDevice *out) {
    VkLayerDeviceCreateInfo *link = dev_chain(ci ? ci->pNext : NULL, VK_LAYER_LINK_INFO);
    VkLayerDeviceCreateInfo *ld = dev_chain(ci ? ci->pNext : NULL, VK_LOADER_DATA_CALLBACK);
    if (!link || !link->u.pLayerInfo || !out) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr ngipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr ngdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    PFN_vkCreateDevice nxt = (PFN_vkCreateDevice)ngipa(NULL, "vkCreateDevice");
    if (!nxt) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = nxt(pd, ci, ac, out); if (r != VK_SUCCESS) return r;
    pthread_mutex_lock(&g_mu);
    Dev *d = NULL; for (int i = 0; i < MAX_D; i++) if (!g_dev[i].key) { d = &g_dev[i]; break; }
    if (!d) { loge("device table full"); pthread_mutex_unlock(&g_mu); return r; }
    memset(d, 0, sizeof *d);
    d->key = key_of(*out); d->device = *out; d->gdpa = ngdpa; d->set_loader = ld ? ld->u.pfnSetDeviceLoaderData : NULL;
    if (ci && ci->queueCreateInfoCount && ci->pQueueCreateInfos) d->qfam = ci->pQueueCreateInfos[0].queueFamilyIndex;
    Inst *in = inst_of(key_of(pd));
    PFN_vkGetPhysicalDeviceMemoryProperties gp = (in && in->GetMemProps) ? in->GetMemProps
        : (PFN_vkGetPhysicalDeviceMemoryProperties)ngipa(NULL, "vkGetPhysicalDeviceMemoryProperties");
    if (gp) { gp(pd, &d->mem); d->mem_ok = 1; }
#define LD(n) d->n = (PFN_vk##n)ngdpa(*out, "vk" #n);
    DEV_FNS(LD)
#undef LD
    pthread_mutex_unlock(&g_mu); return r;
}
static void VKAPI_CALL fb_DestroyDevice(VkDevice device, const VkAllocationCallbacks *ac) {
    pthread_mutex_lock(&g_mu);
    Dev *d = dev_of(key_of(device)); PFN_vkDestroyDevice fn = d ? d->DestroyDevice : NULL;
    if (d) {
        for (int i = 0; i < MAX_SC; i++) if (g_sc[i].live && g_sc[i].devkey == d->key) { sc_free(d, &g_sc[i]); g_sc[i].live = 0; }
        for (int i = 0; i < MAX_Q; i++) if (g_q[i].devkey == d->key) g_q[i].queue = VK_NULL_HANDLE;
        memset(d, 0, sizeof *d);
    }
    pthread_mutex_unlock(&g_mu); if (fn) fn(device, ac);
}
static void VKAPI_CALL fb_GetDeviceQueue(VkDevice device, uint32_t family, uint32_t index, VkQueue *pq) {
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(device));
    PFN_vkGetDeviceQueue fn = d ? d->GetDeviceQueue : NULL; void *k = d ? d->key : NULL; pthread_mutex_unlock(&g_mu);
    if (!fn) { if (pq) *pq = VK_NULL_HANDLE; return; }
    fn(device, family, index, pq); if (!pq || !*pq) return;
    pthread_mutex_lock(&g_mu); q_add(*pq, k, family); pthread_mutex_unlock(&g_mu);
}
static void VKAPI_CALL fb_GetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *pq) {
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(device));
    PFN_vkGetDeviceQueue2 fn = d ? d->GetDeviceQueue2 : NULL; void *k = d ? d->key : NULL; pthread_mutex_unlock(&g_mu);
    if (!fn) { if (pq) *pq = VK_NULL_HANDLE; return; }
    fn(device, info, pq); if (!pq || !*pq || !info) return;
    pthread_mutex_lock(&g_mu); q_add(*pq, k, info->queueFamilyIndex); pthread_mutex_unlock(&g_mu);
}
static VkResult VKAPI_CALL fb_GetCaps(VkPhysicalDevice pd, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *caps) {
    pthread_mutex_lock(&g_mu); Inst *in = inst_of(key_of(pd));
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fn = in ? in->GetCaps : NULL; pthread_mutex_unlock(&g_mu);
    if (!fn) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = fn(pd, surface, caps);
    if (r == VK_SUCCESS && caps && enabled()) caps->supportedUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return r;
}
static VkResult VKAPI_CALL fb_GetCaps2(VkPhysicalDevice pd, const VkPhysicalDeviceSurfaceInfo2KHR *info, VkSurfaceCapabilities2KHR *caps) {
    pthread_mutex_lock(&g_mu); Inst *in = inst_of(key_of(pd));
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR fn = in ? in->GetCaps2 : NULL; pthread_mutex_unlock(&g_mu);
    if (!fn) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = fn(pd, info, caps);
    if (r == VK_SUCCESS && caps && enabled()) caps->surfaceCapabilities.supportedUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return r;
}
static void sc_take_images(Dev *d, VkDevice device, Sc *s) {
    uint32_t n = 0; if (!d->GetSwapchainImagesKHR || d->GetSwapchainImagesKHR(device, s->sc, &n, NULL) < 0) return;
    VkImage tmp[MAX_IMG]; uint32_t c = n > MAX_IMG ? MAX_IMG : n;
    if (c && d->GetSwapchainImagesKHR(device, s->sc, &c, tmp) >= 0) { s->nimg = c; memcpy(s->imgs, tmp, c * sizeof(VkImage)); }
}
static VkResult VKAPI_CALL fb_CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *ac, VkSwapchainKHR *out) {
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(device));
    PFN_vkCreateSwapchainKHR fn = d ? d->CreateSwapchainKHR : NULL; int on = enabled(); pthread_mutex_unlock(&g_mu);
    if (!fn || !ci || !out) return VK_ERROR_INITIALIZATION_FAILED;
    VkSwapchainCreateInfoKHR info = *ci; if (on) info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkResult r = fn(device, on ? &info : ci, ac, out); if (r != VK_SUCCESS || !on) return r;
    pthread_mutex_lock(&g_mu); d = dev_of(key_of(device)); Sc *s = NULL;
    for (int i = 0; i < MAX_SC; i++) if (!g_sc[i].live) { s = &g_sc[i]; break; }
    if (d && s) {
        memset(s, 0, sizeof *s); s->live = 1; s->sc = *out; s->devkey = d->key; s->format = ci->imageFormat;
        s->w = ci->imageExtent.width; s->h = ci->imageExtent.height; s->qfam = d->qfam;
        sc_take_images(d, device, s); log_sc_once(s->w, s->h, (unsigned)s->format);
    } else if (!s) loge("swapchain table full");
    pthread_mutex_unlock(&g_mu); return r;
}
static void VKAPI_CALL fb_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR sc, const VkAllocationCallbacks *ac) {
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(device)); PFN_vkDestroySwapchainKHR fn = d ? d->DestroySwapchainKHR : NULL;
    for (int i = 0; i < MAX_SC; i++) if (g_sc[i].live && g_sc[i].sc == sc) { if (d) sc_free(d, &g_sc[i]); g_sc[i].live = 0; }
    pthread_mutex_unlock(&g_mu); if (fn) fn(device, sc, ac);
}
/* VK_SUCCESS: our submit consumed pPresentInfo wait semaphores; caller presents s->sem.
 * Any other result: caller presents the original info. */
static VkResult present_copy(Dev *d, Sc *s, VkQueue queue, VkImage image, const VkPresentInfoKHR *pi) {
    if (sc_ensure(d, s, q_family(queue, d->qfam)) != 0) { sc_free(d, s); loge("readback setup failed"); return VK_NOT_READY; }
    if (s->busy) { /* previous present still owns s->sem */
        if (!d->QueueWaitIdle || d->QueueWaitIdle(queue) != VK_SUCCESS) { loge("QueueWaitIdle failed"); return VK_NOT_READY; }
        if (d->ResetFences) d->ResetFences(d->device, 1, &s->fence); s->busy = 0;
    }
    VkResult r = record_copy(d, s, image); if (r != VK_SUCCESS) { logef("record_copy %d", (int)r); return VK_NOT_READY; }
    uint32_t wc = pi->waitSemaphoreCount;
    if (wc > 64u || !d->QueueSubmit || !d->WaitForFences) { loge("present wait list unsupported"); return VK_NOT_READY; }
    VkPipelineStageFlags stages[64]; for (uint32_t i = 0; i < wc; i++) stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = wc,
        .pWaitSemaphores = wc ? pi->pWaitSemaphores : NULL, .pWaitDstStageMask = wc ? stages : NULL,
        .commandBufferCount = 1, .pCommandBuffers = &s->cmd, .signalSemaphoreCount = 1, .pSignalSemaphores = &s->sem };
    r = d->QueueSubmit(queue, 1, &si, s->fence);
    if (r != VK_SUCCESS) { logef("QueueSubmit %d", (int)r); return VK_NOT_READY; }
    s->busy = 1;
    r = d->WaitForFences(d->device, 1, &s->fence, VK_TRUE, 1000000000ull);
    if (r != VK_SUCCESS) { logef("WaitForFences %d", (int)r); return VK_SUCCESS; }
    if (d->ResetFences) d->ResetFences(d->device, 1, &s->fence);
    if (!s->coherent) {
        VkMappedMemoryRange mr = { .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = s->mem, .size = VK_WHOLE_SIZE };
        d->InvalidateMappedMemoryRanges(d->device, 1, &mr);
    }
    if (fb_map(s->w, s->h) == 0 && s->map) publish(s->w, s->h, s->format, s->map);
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL fb_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pi) {
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(queue)); PFN_vkQueuePresentKHR down = d ? d->QueuePresentKHR : NULL;
    if (!down) { pthread_mutex_unlock(&g_mu); return VK_ERROR_INITIALIZATION_FAILED; }
    Sc *s = NULL; uint32_t si = 0;
    if (enabled() && pi) for (uint32_t i = 0; i < pi->swapchainCount; i++) {
        Sc *c = sc_of(pi->pSwapchains[i]); if (c && fmt_ok(c->format)) { s = c; si = i; break; }
    }
    VkImage image = VK_NULL_HANDLE;
    if (!(s && pi->pImageIndices && pi->pImageIndices[si] < s->nimg)) s = NULL;
    else image = s->imgs[pi->pImageIndices[si]];
    VkSemaphore sem = VK_NULL_HANDLE; int stole = 0;
    if (s && image && present_copy(d, s, queue, image, pi) == VK_SUCCESS) { sem = s->sem; stole = 1; }
    pthread_mutex_unlock(&g_mu);
    if (!stole) return down(queue, pi);
    VkPresentInfoKHR cpy = *pi; cpy.waitSemaphoreCount = 1; cpy.pWaitSemaphores = &sem;
    return down(queue, &cpy);
}
#define H(s, f) if (!strcmp(n, s)) return (PFN_vkVoidFunction)(f);
static int dev_fn(const char *n) {
    return !strcmp(n, "vkDestroyDevice") || !strcmp(n, "vkGetDeviceQueue") || !strcmp(n, "vkGetDeviceQueue2")
        || !strcmp(n, "vkCreateSwapchainKHR") || !strcmp(n, "vkDestroySwapchainKHR") || !strcmp(n, "vkQueuePresentKHR");
}
static PFN_vkVoidFunction layer_proc(const char *n) {
    H("vkCreateInstance", fb_CreateInstance) H("vkDestroyInstance", fb_DestroyInstance)
    H("vkCreateDevice", fb_CreateDevice) H("vkDestroyDevice", fb_DestroyDevice)
    H("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", fb_GetCaps) H("vkGetPhysicalDeviceSurfaceCapabilities2KHR", fb_GetCaps2)
    H("vkCreateSwapchainKHR", fb_CreateSwapchainKHR) H("vkDestroySwapchainKHR", fb_DestroySwapchainKHR)
    H("vkQueuePresentKHR", fb_QueuePresentKHR) H("vkGetDeviceQueue", fb_GetDeviceQueue) H("vkGetDeviceQueue2", fb_GetDeviceQueue2)
    return NULL;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName) {
    if (!pName) return NULL;
    if (!strcmp(pName, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if (!strcmp(pName, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!instance) return !strcmp(pName, "vkCreateInstance") ? (PFN_vkVoidFunction)fb_CreateInstance : NULL;
    PFN_vkVoidFunction f = layer_proc(pName); if (f) return f;
    pthread_mutex_lock(&g_mu); Inst *in = inst_of(key_of(instance));
    PFN_vkGetInstanceProcAddr nxt = in ? in->gipa : NULL; pthread_mutex_unlock(&g_mu);
    return nxt ? nxt(instance, pName) : NULL;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName) {
    if (!pName) return NULL;
    if (!strcmp(pName, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (dev_fn(pName)) { PFN_vkVoidFunction f = layer_proc(pName); if (f) return f; }
    if (!device) return NULL;
    pthread_mutex_lock(&g_mu); Dev *d = dev_of(key_of(device));
    PFN_vkGetDeviceProcAddr nxt = d ? d->gdpa : NULL; pthread_mutex_unlock(&g_mu);
    return nxt ? nxt(device, pName) : NULL;
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *p) {
    if (!p || p->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT || p->loaderLayerInterfaceVersion < 2)
        return VK_ERROR_INITIALIZATION_FAILED;
    p->loaderLayerInterfaceVersion = 2;
    p->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    p->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    p->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
