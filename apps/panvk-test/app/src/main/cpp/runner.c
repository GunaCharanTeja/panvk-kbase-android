#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unwind.h>
#include <stdint.h>

static const char *get_signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGABRT: return "SIGABRT";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGTRAP: return "SIGTRAP";
        default:      return "UNKNOWN";
    }
}

struct BacktraceState {
    void **current;
    void **end;
};

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context *context, void *arg) {
    struct BacktraceState *state = (struct BacktraceState *)arg;
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current < state->end) {
            *state->current++ = (void *)pc;
        } else {
            return _URC_END_OF_STACK;
        }
    }
    return _URC_NO_REASON;
}

static void crash_handler(int sig, siginfo_t *si, void *unused) {
    (void)unused;
    char buf[512];
    const char *sig_name = get_signal_name(sig);
    void *addr = si ? si->si_addr : NULL;
    int len = snprintf(buf, sizeof(buf), "CRASH signal=%d (%s) addr=%p\n", sig, sig_name, addr);
    if (len > 0) {
        write(STDERR_FILENO, buf, (size_t)len);
    }

    void *stack[64];
    struct BacktraceState state = { stack, stack + 64 };
    _Unwind_Backtrace(unwind_callback, &state);
    int count = (int)(state.current - stack);

    for (int i = 0; i < count; i++) {
        Dl_info info;
        if (dladdr(stack[i], &info) && info.dli_fname) {
            const char *slash = strrchr(info.dli_fname, '/');
            const char *lib = slash ? slash + 1 : info.dli_fname;
            const char *sym = info.dli_sname ? info.dli_sname : "???";
            uintptr_t offset = info.dli_saddr ? ((uintptr_t)stack[i] - (uintptr_t)info.dli_saddr)
                                              : ((uintptr_t)stack[i] - (uintptr_t)info.dli_fbase);
            len = snprintf(buf, sizeof(buf), "  #%02d: %s (%s+0x%lx)\n", i, lib, sym, (unsigned long)offset);
        } else {
            len = snprintf(buf, sizeof(buf), "  #%02d: %p\n", i, stack[i]);
        }
        if (len > 0) {
            write(STDERR_FILENO, buf, (size_t)len);
        }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char **argv) {
    /* Unbuffer stdout and stderr */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* Set up alternate signal stack */
    size_t stack_size = SIGSTKSZ < 65536 ? 65536 : SIGSTKSZ;
    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = malloc(stack_size);
    ss.ss_size = stack_size;
    ss.ss_flags = 0;
    if (ss.ss_sp) {
        sigaltstack(&ss, NULL);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);

    if (argc < 2) {
        fprintf(stderr, "FAIL runner: missing test lib path\n");
        fflush(stderr);
        _exit(127);
    }

    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "FAIL dlopen %s\n", dlerror());
        fflush(stderr);
        _exit(127);
    }

    typedef int (*main_fn)(int, char **);
    main_fn pfn_main = (main_fn)dlsym(h, "main");
    if (!pfn_main) {
        fprintf(stderr, "FAIL dlsym main: %s\n", dlerror());
        fflush(stderr);
        _exit(127);
    }

    int rc = pfn_main(argc - 2, argv + 2);
    fflush(NULL);
    _exit(rc);
}
