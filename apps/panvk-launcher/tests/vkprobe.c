/*
 * Build line:
 *   /var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin/<triple>-clang -O2 vkprobe.c -o vkprobe.exe
 *
 * where <triple> is one of:
 *   - aarch64-w64-mingw32
 *   - arm64ec-w64-mingw32
 *   - x86_64-w64-mingw32
 *
 * Note: Uses self-contained minimal Vulkan definitions and runtime dynamic loading
 * of vulkan-1.dll, so no external Vulkan SDK or headers are required.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VKAPI_PTR WINAPI

typedef int32_t VkResult;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;

#define VK_SUCCESS 0

#define VK_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))
#define VK_API_VERSION_1_1 VK_MAKE_VERSION(1, 1, 0)

#define VK_VERSION_MAJOR(version) ((uint32_t)(version) >> 22)
#define VK_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3ff)
#define VK_VERSION_PATCH(version) ((uint32_t)(version) & 0xfff)

#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1

#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256
#define VK_UUID_SIZE 16

typedef enum VkStructureType {
    VK_STRUCTURE_TYPE_APPLICATION_INFO_ENUM = 0,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO_ENUM = 1
} VkStructureType;

typedef enum VkPhysicalDeviceType {
    VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
    VK_PHYSICAL_DEVICE_TYPE_CPU = 4
} VkPhysicalDeviceType;

typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;

typedef struct VkApplicationInfo {
    uint32_t                    sType;
    const void*                 pNext;
    const char*                 pApplicationName;
    uint32_t                    applicationVersion;
    const char*                 pEngineName;
    uint32_t                    engineVersion;
    uint32_t                    apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
    uint32_t                    sType;
    const void*                 pNext;
    VkFlags                     flags;
    const VkApplicationInfo*    pApplicationInfo;
    uint32_t                    enabledLayerCount;
    const char* const*          ppEnabledLayerNames;
    uint32_t                    enabledExtensionCount;
    const char* const*          ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkPhysicalDeviceProperties {
    uint32_t             apiVersion;
    uint32_t             driverVersion;
    uint32_t             vendorID;
    uint32_t             deviceID;
    VkPhysicalDeviceType deviceType;
    char                 deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    uint8_t              pipelineCacheUUID[VK_UUID_SIZE];
    uint64_t             limitsAndSparseReserved[256];
} VkPhysicalDeviceProperties;

typedef void* (VKAPI_PTR *PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const void* pAllocator, VkInstance* pInstance);
typedef void (VKAPI_PTR *PFN_vkDestroyInstance)(VkInstance instance, const void* pAllocator);
typedef VkResult (VKAPI_PTR *PFN_vkEnumeratePhysicalDevices)(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
typedef void (VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);

int main(void)
{
    HMODULE hVulkan = LoadLibraryA("vulkan-1.dll");
    if (!hVulkan) {
        printf("VKPROBE: failed to load vulkan-1.dll (error=%lu)\n", (unsigned long)GetLastError());
        fflush(stdout);
        return 1;
    }

    PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)GetProcAddress(hVulkan, "vkGetInstanceProcAddr");
    if (!pfn_vkGetInstanceProcAddr) {
        printf("VKPROBE: failed to get vkGetInstanceProcAddr\n");
        fflush(stdout);
        FreeLibrary(hVulkan);
        return 1;
    }

    PFN_vkCreateInstance pfn_vkCreateInstance =
        (PFN_vkCreateInstance)pfn_vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    if (!pfn_vkCreateInstance) {
        printf("VKPROBE: failed to get vkCreateInstance\n");
        fflush(stdout);
        FreeLibrary(hVulkan);
        return 1;
    }

    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vkprobe";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "vkprobe";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = NULL;

    VkInstance instance = NULL;
    VkResult res = pfn_vkCreateInstance(&createInfo, NULL, &instance);
    if (res != VK_SUCCESS || !instance) {
        printf("VKPROBE: vkCreateInstance failed (VkResult=%d)\n", (int)res);
        fflush(stdout);
        FreeLibrary(hVulkan);
        return 1;
    }

    PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)pfn_vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance pfn_vkDestroyInstance =
        (PFN_vkDestroyInstance)pfn_vkGetInstanceProcAddr(instance, "vkDestroyInstance");

    if (!pfn_vkEnumeratePhysicalDevices || !pfn_vkGetPhysicalDeviceProperties || !pfn_vkDestroyInstance) {
        printf("VKPROBE: failed to get instance function pointers\n");
        fflush(stdout);
        if (pfn_vkDestroyInstance && instance) {
            pfn_vkDestroyInstance(instance, NULL);
        }
        FreeLibrary(hVulkan);
        return 1;
    }

    uint32_t deviceCount = 0;
    res = pfn_vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    if (res != VK_SUCCESS) {
        printf("VKPROBE: vkEnumeratePhysicalDevices failed (VkResult=%d)\n", (int)res);
        fflush(stdout);
        pfn_vkDestroyInstance(instance, NULL);
        FreeLibrary(hVulkan);
        return 1;
    }

    if (deviceCount == 0) {
        printf("VKPROBE: no devices\n");
        fflush(stdout);
        pfn_vkDestroyInstance(instance, NULL);
        FreeLibrary(hVulkan);
        return 1;
    }

    VkPhysicalDevice *devices = (VkPhysicalDevice *)calloc(deviceCount, sizeof(VkPhysicalDevice));
    if (!devices) {
        printf("VKPROBE: failed to allocate memory for %u devices\n", deviceCount);
        fflush(stdout);
        pfn_vkDestroyInstance(instance, NULL);
        FreeLibrary(hVulkan);
        return 1;
    }

    res = pfn_vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
    if (res != VK_SUCCESS) {
        printf("VKPROBE: vkEnumeratePhysicalDevices failed (VkResult=%d)\n", (int)res);
        fflush(stdout);
        free(devices);
        pfn_vkDestroyInstance(instance, NULL);
        FreeLibrary(hVulkan);
        return 1;
    }

    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties props;
        memset(&props, 0, sizeof(props));
        pfn_vkGetPhysicalDeviceProperties(devices[i], &props);

        printf("device[%u]: name=%s apiVersion=%u.%u.%u driverVersion=0x%x vendorID=0x%x deviceID=0x%x type=%d\n",
               i,
               props.deviceName,
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion),
               VK_VERSION_PATCH(props.apiVersion),
               props.driverVersion,
               props.vendorID,
               props.deviceID,
               (int)props.deviceType);
        fflush(stdout);
    }

    printf("VKPROBE: OK count=%u\n", deviceCount);
    fflush(stdout);

    free(devices);
    pfn_vkDestroyInstance(instance, NULL);
    FreeLibrary(hVulkan);

    return 0;
}
