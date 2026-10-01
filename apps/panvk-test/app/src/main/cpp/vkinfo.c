#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

#include "vkinfo_gen.h"

static void print_json_string(const char *s) {
    if (!s) {
        printf("null");
        return;
    }
    putchar('"');
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    printf("\\u%04x", (unsigned char)*p);
                } else {
                    putchar(*p);
                }
                break;
        }
    }
    putchar('"');
}

static bool dev_has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name) {
    if (!exts || !name) return false;
    for (uint32_t k = 0; k < count; k++) {
        if (strcmp(exts[k].extensionName, name) == 0) return true;
    }
    return false;
}

typedef struct {
    const VkFeatureStructDesc *desc;
    void *ptr;
} QueriedFeature;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <vulkan_icd.so> [out.json]\n", argv[0]);
        return 1;
    }
    /* Optional argv[2]: JSON output file, keeps driver stderr out of the JSON. */
    if (argc > 2 && !freopen(argv[2], "w", stdout)) {
        fprintf(stderr, "FAIL open %s\n", argv[2]);
        return 1;
    }

    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "FAIL dlopen: %s\n", dlerror());
        return 1;
    }

    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
    if (!gipa) {
        fprintf(stderr, "FAIL dlsym vk_icdGetInstanceProcAddr: %s\n", dlerror());
        return 1;
    }

    PFN_vkEnumerateInstanceExtensionProperties pfn_vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)gipa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    PFN_vkEnumerateInstanceVersion pfn_vkEnumerateInstanceVersion =
        (PFN_vkEnumerateInstanceVersion)gipa(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    PFN_vkCreateInstance pfn_vkCreateInstance =
        (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");

    if (!pfn_vkCreateInstance) {
        fprintf(stderr, "FAIL: vkCreateInstance not found\n");
        return 1;
    }

    uint32_t inst_ext_count = 0;
    if (pfn_vkEnumerateInstanceExtensionProperties) {
        pfn_vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, NULL);
    }
    VkExtensionProperties *inst_exts = inst_ext_count ? (VkExtensionProperties *)calloc(inst_ext_count, sizeof(VkExtensionProperties)) : NULL;
    if (inst_exts) {
        pfn_vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, inst_exts);
    }

    /* Start JSON output */
    printf("{\"instanceExtensions\":[");
    for (uint32_t i = 0; i < inst_ext_count; i++) {
        if (i > 0) printf(",");
        printf("{\"name\":\"%s\",\"specVersion\":%u}", inst_exts[i].extensionName, inst_exts[i].specVersion);
    }
    printf("],\"devices\":[");

    uint32_t inst_api_version = VK_MAKE_API_VERSION(0, 1, 0, 0);
    if (pfn_vkEnumerateInstanceVersion) {
        pfn_vkEnumerateInstanceVersion(&inst_api_version);
    }
    if (inst_api_version > VK_MAKE_API_VERSION(0, 1, 4, 0)) {
        inst_api_version = VK_MAKE_API_VERSION(0, 1, 4, 0);
    }
    if (inst_api_version < VK_MAKE_API_VERSION(0, 1, 0, 0)) {
        inst_api_version = VK_MAKE_API_VERSION(0, 1, 0, 0);
    }

    const char *enabled_inst_exts[1];
    uint32_t enabled_inst_ext_count = 0;
    for (uint32_t i = 0; i < inst_ext_count; i++) {
        if (strcmp(inst_exts[i].extensionName, "VK_KHR_get_physical_device_properties2") == 0) {
            enabled_inst_exts[enabled_inst_ext_count++] = "VK_KHR_get_physical_device_properties2";
            break;
        }
    }

    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = inst_api_version;

    VkInstanceCreateInfo inst_info;
    memset(&inst_info, 0, sizeof(inst_info));
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = enabled_inst_ext_count;
    inst_info.ppEnabledExtensionNames = enabled_inst_exts;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = pfn_vkCreateInstance(&inst_info, NULL, &instance);
    if (res != VK_SUCCESS) {
        /* Fallback: retry with Vulkan 1.0 and no extensions */
        app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        inst_info.enabledExtensionCount = 0;
        inst_info.ppEnabledExtensionNames = NULL;
        res = pfn_vkCreateInstance(&inst_info, NULL, &instance);
    }

    if (res != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        printf("]}\n");
        if (inst_exts) free(inst_exts);
        return 1;
    }

    PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)gipa(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkGetPhysicalDeviceProperties2 pfn_vkGetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2)gipa(instance, "vkGetPhysicalDeviceProperties2");
    if (!pfn_vkGetPhysicalDeviceProperties2) {
        pfn_vkGetPhysicalDeviceProperties2 =
            (PFN_vkGetPhysicalDeviceProperties2)gipa(instance, "vkGetPhysicalDeviceProperties2KHR");
    }
    PFN_vkGetPhysicalDeviceFeatures pfn_vkGetPhysicalDeviceFeatures =
        (PFN_vkGetPhysicalDeviceFeatures)gipa(instance, "vkGetPhysicalDeviceFeatures");
    PFN_vkGetPhysicalDeviceFeatures2 pfn_vkGetPhysicalDeviceFeatures2 =
        (PFN_vkGetPhysicalDeviceFeatures2)gipa(instance, "vkGetPhysicalDeviceFeatures2");
    if (!pfn_vkGetPhysicalDeviceFeatures2) {
        pfn_vkGetPhysicalDeviceFeatures2 =
            (PFN_vkGetPhysicalDeviceFeatures2)gipa(instance, "vkGetPhysicalDeviceFeatures2KHR");
    }
    PFN_vkEnumerateDeviceExtensionProperties pfn_vkEnumerateDeviceExtensionProperties =
        (PFN_vkEnumerateDeviceExtensionProperties)gipa(instance, "vkEnumerateDeviceExtensionProperties");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties =
        (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(instance, "vkGetPhysicalDeviceMemoryProperties");
    PFN_vkGetPhysicalDeviceFormatProperties pfn_vkGetPhysicalDeviceFormatProperties =
        (PFN_vkGetPhysicalDeviceFormatProperties)gipa(instance, "vkGetPhysicalDeviceFormatProperties");
    PFN_vkDestroyInstance pfn_vkDestroyInstance =
        (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");

    uint32_t phys_count = 0;
    if (pfn_vkEnumeratePhysicalDevices) {
        pfn_vkEnumeratePhysicalDevices(instance, &phys_count, NULL);
    }
    VkPhysicalDevice *phys_devices = phys_count ? (VkPhysicalDevice *)calloc(phys_count, sizeof(VkPhysicalDevice)) : NULL;
    if (phys_devices) {
        pfn_vkEnumeratePhysicalDevices(instance, &phys_count, phys_devices);
    }

    for (uint32_t i = 0; i < phys_count; i++) {
        if (i > 0) printf(",");
        printf("{");

        /* Properties */
        VkPhysicalDeviceProperties props;
        memset(&props, 0, sizeof(props));
        if (pfn_vkGetPhysicalDeviceProperties) {
            pfn_vkGetPhysicalDeviceProperties(phys_devices[i], &props);
        }

        /* Device extensions */
        uint32_t dev_ext_count = 0;
        if (pfn_vkEnumerateDeviceExtensionProperties) {
            pfn_vkEnumerateDeviceExtensionProperties(phys_devices[i], NULL, &dev_ext_count, NULL);
        }
        VkExtensionProperties *dev_exts = dev_ext_count ? (VkExtensionProperties *)calloc(dev_ext_count, sizeof(VkExtensionProperties)) : NULL;
        if (dev_exts) {
            pfn_vkEnumerateDeviceExtensionProperties(phys_devices[i], NULL, &dev_ext_count, dev_exts);
        }

        /* Driver properties via vkGetPhysicalDeviceProperties2 if available */
        VkPhysicalDeviceDriverProperties driver_props;
        memset(&driver_props, 0, sizeof(driver_props));
        driver_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        bool has_driver_props = false;

        if (pfn_vkGetPhysicalDeviceProperties2) {
            VkPhysicalDeviceProperties2 props2;
            memset(&props2, 0, sizeof(props2));
            props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props2.pNext = &driver_props;
            pfn_vkGetPhysicalDeviceProperties2(phys_devices[i], &props2);
            if (driver_props.driverName[0] != '\0' || driver_props.driverInfo[0] != '\0') {
                has_driver_props = true;
            }
        }

        printf("\"properties\":{");
        printf("\"deviceName\":"); print_json_string(props.deviceName);
        printf(",\"apiVersion\":\"%u.%u.%u\"",
               VK_API_VERSION_MAJOR(props.apiVersion),
               VK_API_VERSION_MINOR(props.apiVersion),
               VK_API_VERSION_PATCH(props.apiVersion));
        printf(",\"driverVersion\":%u", props.driverVersion);
        printf(",\"vendorID\":%u", props.vendorID);
        printf(",\"deviceID\":%u", props.deviceID);
        printf(",\"deviceType\":%u", props.deviceType);
        if (has_driver_props) {
            printf(",\"driverName\":"); print_json_string(driver_props.driverName);
            printf(",\"driverInfo\":"); print_json_string(driver_props.driverInfo);
            printf(",\"conformanceVersion\":\"%u.%u.%u.%u\"",
                   driver_props.conformanceVersion.major,
                   driver_props.conformanceVersion.minor,
                   driver_props.conformanceVersion.subminor,
                   driver_props.conformanceVersion.patch);
        } else {
            printf(",\"driverName\":null,\"driverInfo\":null,\"conformanceVersion\":null");
        }
        printf("}");

        /* Extensions */
        printf(",\"extensions\":[");
        for (uint32_t j = 0; j < dev_ext_count; j++) {
            if (j > 0) printf(",");
            printf("{\"name\":\"%s\",\"specVersion\":%u}", dev_exts[j].extensionName, dev_exts[j].specVersion);
        }
        printf("]");

        /* Features */
        VkPhysicalDeviceFeatures2 features2;
        memset(&features2, 0, sizeof(features2));
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        QueriedFeature queried[512];
        size_t queried_count = 0;
        void **pNext_tail = &features2.pNext;

        if (pfn_vkGetPhysicalDeviceFeatures2) {
            for (size_t k = 0; k < vkinfo_feature_struct_count; k++) {
                const VkFeatureStructDesc *desc = &vkinfo_feature_structs[k];
                bool include = false;
                if (desc->ext_name == NULL) {
                    if (desc->core_version != 0 && props.apiVersion >= desc->core_version) {
                        include = true;
                    }
                } else {
                    if (dev_has_extension(dev_exts, dev_ext_count, desc->ext_name)) {
                        include = true;
                    }
                }
                if (include) {
                    void *mem = calloc(1, desc->size);
                    if (mem) {
                        VkBaseOutStructure *header = (VkBaseOutStructure *)mem;
                        header->sType = desc->sType;
                        header->pNext = NULL;
                        *pNext_tail = header;
                        pNext_tail = (void **)&header->pNext;
                        queried[queried_count].desc = desc;
                        queried[queried_count].ptr = mem;
                        queried_count++;
                    }
                }
            }
            pfn_vkGetPhysicalDeviceFeatures2(phys_devices[i], &features2);
        } else if (pfn_vkGetPhysicalDeviceFeatures) {
            pfn_vkGetPhysicalDeviceFeatures(phys_devices[i], &features2.features);
        }

        printf(",\"features\":{");
        printf("\"VkPhysicalDeviceFeatures\":{");
        print_VkPhysicalDeviceFeatures_members(stdout, &features2.features);
        printf("}");
        for (size_t k = 0; k < queried_count; k++) {
            printf(",\"%s\":{", queried[k].desc->struct_name);
            queried[k].desc->print_members(stdout, queried[k].ptr);
            printf("}");
            free(queried[k].ptr);
        }
        printf("}");

        /* Limits */
        printf(",\"limits\":{");
        print_VkPhysicalDeviceLimits_json(stdout, &props.limits);
        printf("}");

        /* Sparse properties */
        printf(",\"sparseProperties\":{");
        print_VkPhysicalDeviceSparseProperties_json(stdout, &props.sparseProperties);
        printf("}");

        /* Memory */
        VkPhysicalDeviceMemoryProperties mem_props;
        memset(&mem_props, 0, sizeof(mem_props));
        if (pfn_vkGetPhysicalDeviceMemoryProperties) {
            pfn_vkGetPhysicalDeviceMemoryProperties(phys_devices[i], &mem_props);
        }
        printf(",\"memory\":{\"heaps\":[");
        for (uint32_t j = 0; j < mem_props.memoryHeapCount; j++) {
            if (j > 0) printf(",");
            printf("{\"size\":%" PRIu64 ",\"flags\":%u}", (uint64_t)mem_props.memoryHeaps[j].size, mem_props.memoryHeaps[j].flags);
        }
        printf("],\"types\":[");
        for (uint32_t j = 0; j < mem_props.memoryTypeCount; j++) {
            if (j > 0) printf(",");
            printf("{\"propertyFlags\":%u,\"heapIndex\":%u}", mem_props.memoryTypes[j].propertyFlags, mem_props.memoryTypes[j].heapIndex);
        }
        printf("]}");

        /* Queues */
        uint32_t qf_count = 0;
        if (pfn_vkGetPhysicalDeviceQueueFamilyProperties) {
            pfn_vkGetPhysicalDeviceQueueFamilyProperties(phys_devices[i], &qf_count, NULL);
        }
        VkQueueFamilyProperties *qf_props = qf_count ? (VkQueueFamilyProperties *)calloc(qf_count, sizeof(VkQueueFamilyProperties)) : NULL;
        if (qf_props) {
            pfn_vkGetPhysicalDeviceQueueFamilyProperties(phys_devices[i], &qf_count, qf_props);
        }
        printf(",\"queues\":[");
        for (uint32_t j = 0; j < qf_count; j++) {
            if (j > 0) printf(",");
            printf("{\"queueFlags\":%u,\"queueCount\":%u,\"timestampValidBits\":%u,\"minImageTransferGranularity\":{\"width\":%u,\"height\":%u,\"depth\":%u}}",
                   qf_props[j].queueFlags,
                   qf_props[j].queueCount,
                   qf_props[j].timestampValidBits,
                   qf_props[j].minImageTransferGranularity.width,
                   qf_props[j].minImageTransferGranularity.height,
                   qf_props[j].minImageTransferGranularity.depth);
        }
        printf("]");
        if (qf_props) free(qf_props);

        /* Formats */
        printf(",\"formats\":{");
        int first_fmt = 1;
        if (pfn_vkGetPhysicalDeviceFormatProperties) {
            for (size_t f = 0; f < vk_format_table_count; f++) {
                VkFormatProperties fmt_props;
                memset(&fmt_props, 0, sizeof(fmt_props));
                pfn_vkGetPhysicalDeviceFormatProperties(phys_devices[i], vk_format_table[f].format, &fmt_props);
                if ((fmt_props.linearTilingFeatures | fmt_props.optimalTilingFeatures | fmt_props.bufferFeatures) != 0) {
                    if (!first_fmt) printf(",");
                    first_fmt = 0;
                    printf("\"%s\":{\"linear\":\"0x%08x\",\"optimal\":\"0x%08x\",\"buffer\":\"0x%08x\"}",
                           vk_format_table[f].name,
                           fmt_props.linearTilingFeatures,
                           fmt_props.optimalTilingFeatures,
                           fmt_props.bufferFeatures);
                }
            }
        }
        printf("}");

        printf("}");
        if (dev_exts) free(dev_exts);
    }

    printf("]}\n");

    if (phys_devices) free(phys_devices);
    if (inst_exts) free(inst_exts);
    if (pfn_vkDestroyInstance && instance != VK_NULL_HANDLE) {
        pfn_vkDestroyInstance(instance, NULL);
    }

    return 0;
}
