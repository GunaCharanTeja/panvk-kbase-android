#ifndef PT_COMPAT_H
#define PT_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>

static inline void pt_exit(int c) {
    fflush(NULL);
    _exit(c);
}

static inline void *pt_real_dlsym(void *handle, const char *symbol) {
    return dlsym(handle, symbol);
}

static inline void *pt_dlsym(void *handle, const char *symbol) {
    void *ptr = pt_real_dlsym(handle, symbol);
    if (!ptr && symbol && strcmp(symbol, "vk_icdGetInstanceProcAddr") == 0) {
        ptr = pt_real_dlsym(handle, "vkGetInstanceProcAddr");
    }
    return ptr;
}

#define exit(c) pt_exit(c)
#define dlsym pt_dlsym

#endif /* PT_COMPAT_H */
