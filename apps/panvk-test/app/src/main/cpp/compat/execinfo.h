#ifndef COMPAT_EXECINFO_H
#define COMPAT_EXECINFO_H

#include <stddef.h>

static inline int backtrace(void **buffer, int size) {
    (void)buffer;
    (void)size;
    return 0;
}

static inline char **backtrace_symbols(void *const *buffer, int size) {
    (void)buffer;
    (void)size;
    return NULL;
}

static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd) {
    (void)buffer;
    (void)size;
    (void)fd;
}

#endif /* COMPAT_EXECINFO_H */
