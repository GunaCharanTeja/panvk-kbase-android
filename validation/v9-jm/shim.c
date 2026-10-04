#include <dlfcn.h>
#include <stddef.h>

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(void* instance, const char* pName);

static void* handle = NULL;
static PFN_vkGetInstanceProcAddr gipa = NULL;

__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetInstanceProcAddr(void* instance, const char* pName) {
    if (!handle) {
        handle = dlopen("/data/local/tmp/v9cts/libvulkan_panfrost.so", RTLD_NOW | RTLD_LOCAL);
        if (handle) {
            gipa = (PFN_vkGetInstanceProcAddr)dlsym(handle, "vk_icdGetInstanceProcAddr");
        }
    }
    if (gipa) {
        return gipa(instance, pName);
    }
    return NULL;
}
