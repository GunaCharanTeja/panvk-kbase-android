#include <jni.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JNIEXPORT jstring JNICALL
Java_dev_zenithblue_panvklauncher_Native_probe(JNIEnv *env, jclass clazz, jstring soPath) {
    (void)clazz;

    if (!soPath) {
        return (*env)->NewStringUTF(env, "FAIL path: null soPath");
    }

    const char *path = (*env)->GetStringUTFChars(env, soPath, NULL);
    if (!path) {
        return NULL;
    }

    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        char fail[512];
        snprintf(fail, sizeof(fail), "FAIL dlopen: %s", dlerror());
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, fail);
    }

    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
    if (!gipa) {
        char fail[512];
        const char *err = dlerror();
        snprintf(fail, sizeof(fail), "FAIL dlsym: vk_icdGetInstanceProcAddr (%s)", err ? err : "not found");
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, fail);
    }

    PFN_vkCreateInstance pfn_vkCreateInstance =
        (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
    if (!pfn_vkCreateInstance) {
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, "FAIL gipa: vkCreateInstance not found");
    }

    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

    VkInstanceCreateInfo inst_info;
    memset(&inst_info, 0, sizeof(inst_info));
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo = &app_info;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = pfn_vkCreateInstance(&inst_info, NULL, &instance);
    if (res != VK_SUCCESS) {
        // Fallback to Vulkan 1.0
        app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        res = pfn_vkCreateInstance(&inst_info, NULL, &instance);
    }

    if (res != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        char fail[256];
        snprintf(fail, sizeof(fail), "FAIL vkCreateInstance: %d", res);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, fail);
    }

    PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)gipa(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance pfn_vkDestroyInstance =
        (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");

    if (!pfn_vkEnumeratePhysicalDevices || !pfn_vkGetPhysicalDeviceProperties || !pfn_vkDestroyInstance) {
        if (pfn_vkDestroyInstance) {
            pfn_vkDestroyInstance(instance, NULL);
        }
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, "FAIL gipa: instance functions not found");
    }

    uint32_t dev_count = 0;
    res = pfn_vkEnumeratePhysicalDevices(instance, &dev_count, NULL);
    if (res != VK_SUCCESS) {
        pfn_vkDestroyInstance(instance, NULL);
        char fail[256];
        snprintf(fail, sizeof(fail), "FAIL vkEnumeratePhysicalDevices: %d", res);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, fail);
    }

    if (dev_count == 0) {
        pfn_vkDestroyInstance(instance, NULL);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, "FAIL vkEnumeratePhysicalDevices: 0 devices found");
    }

    VkPhysicalDevice *devices = (VkPhysicalDevice *)calloc(dev_count, sizeof(VkPhysicalDevice));
    if (!devices) {
        pfn_vkDestroyInstance(instance, NULL);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, "FAIL calloc: out of memory");
    }

    res = pfn_vkEnumeratePhysicalDevices(instance, &dev_count, devices);
    if (res != VK_SUCCESS) {
        free(devices);
        pfn_vkDestroyInstance(instance, NULL);
        char fail[256];
        snprintf(fail, sizeof(fail), "FAIL vkEnumeratePhysicalDevices: %d", res);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, fail);
    }

    size_t buf_capacity = (size_t)dev_count * 512 + 64;
    char *result_buf = (char *)malloc(buf_capacity);
    if (!result_buf) {
        free(devices);
        pfn_vkDestroyInstance(instance, NULL);
        (*env)->ReleaseStringUTFChars(env, soPath, path);
        return (*env)->NewStringUTF(env, "FAIL malloc: out of memory");
    }
    result_buf[0] = '\0';
    size_t offset = 0;

    for (uint32_t i = 0; i < dev_count; i++) {
        VkPhysicalDeviceProperties props;
        memset(&props, 0, sizeof(props));
        pfn_vkGetPhysicalDeviceProperties(devices[i], &props);

        uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
        uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
        uint32_t patch = VK_VERSION_PATCH(props.apiVersion);

        int written = snprintf(result_buf + offset, buf_capacity - offset,
            "%s%s api=%u.%u.%u driver=0x%x vendor=0x%x device=0x%x",
            (i > 0 ? "\n" : ""),
            props.deviceName,
            major, minor, patch,
            props.driverVersion,
            props.vendorID,
            props.deviceID);
        if (written < 0 || offset + (size_t)written >= buf_capacity) {
            offset = buf_capacity - 1;
            break;
        }
        offset += (size_t)written;
    }

    free(devices);
    pfn_vkDestroyInstance(instance, NULL);
    (*env)->ReleaseStringUTFChars(env, soPath, path);
    // Note: Do NOT dlclose (driver may keep threads)

    jstring result = (*env)->NewStringUTF(env, result_buf);
    free(result_buf);
    return result;
}
