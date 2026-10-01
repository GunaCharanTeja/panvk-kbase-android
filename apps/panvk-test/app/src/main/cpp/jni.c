#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unwind.h>
#include <android/log.h>

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

JNIEXPORT jstring JNICALL Java_dev_zenithblue_panvktest_Native_run(
    JNIEnv *env,
    jclass clazz,
    jstring jLibPath,
    jobjectArray jArgs,
    jobjectArray jEnv,
    jstring jLogPath,
    jint timeoutMs)
{
    (void)clazz;

    /* 1. Copy all jstrings to C heap before fork() */
    const char *raw_libPath = (*env)->GetStringUTFChars(env, jLibPath, NULL);
    char *c_libPath = raw_libPath ? strdup(raw_libPath) : strdup("");
    if (raw_libPath) {
        (*env)->ReleaseStringUTFChars(env, jLibPath, raw_libPath);
    }

    const char *raw_logPath = (*env)->GetStringUTFChars(env, jLogPath, NULL);
    char *c_logPath = raw_logPath ? strdup(raw_logPath) : strdup("/dev/null");
    if (raw_logPath) {
        (*env)->ReleaseStringUTFChars(env, jLogPath, raw_logPath);
    }

    int args_count = jArgs ? (*env)->GetArrayLength(env, jArgs) : 0;
    int argc = 1 + args_count;
    char **argv = (char **)calloc(argc + 1, sizeof(char *));

    /* argv[0] = basename of libPath */
    const char *slash = strrchr(c_libPath, '/');
    argv[0] = strdup(slash ? slash + 1 : c_libPath);

    for (int i = 0; i < args_count; i++) {
        jstring js = (jstring)(*env)->GetObjectArrayElement(env, jArgs, i);
        if (js) {
            const char *str = (*env)->GetStringUTFChars(env, js, NULL);
            argv[1 + i] = str ? strdup(str) : strdup("");
            if (str) {
                (*env)->ReleaseStringUTFChars(env, js, str);
            }
            (*env)->DeleteLocalRef(env, js);
        } else {
            argv[1 + i] = strdup("");
        }
    }
    argv[argc] = NULL;

    int env_count = jEnv ? (*env)->GetArrayLength(env, jEnv) : 0;
    char **env_entries = (char **)calloc(env_count + 1, sizeof(char *));
    for (int i = 0; i < env_count; i++) {
        jstring js = (jstring)(*env)->GetObjectArrayElement(env, jEnv, i);
        if (js) {
            const char *str = (*env)->GetStringUTFChars(env, js, NULL);
            env_entries[i] = str ? strdup(str) : strdup("");
            if (str) {
                (*env)->ReleaseStringUTFChars(env, js, str);
            }
            (*env)->DeleteLocalRef(env, js);
        } else {
            env_entries[i] = strdup("");
        }
    }
    env_entries[env_count] = NULL;

    /* 2. Fork */
    pid_t pid = fork();
    if (pid < 0) {
        /* fork failed */
        free(c_libPath);
        free(c_logPath);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        for (int i = 0; i < env_count; i++) free(env_entries[i]);
        free(env_entries);
        return (*env)->NewStringUTF(env, "exit:-1");
    }

    if (pid == 0) {
        /* Child process */
        int fd = open(c_logPath, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

        for (int i = 0; i < env_count; i++) {
            if (env_entries[i] && env_entries[i][0] != '\0') {
                putenv(env_entries[i]);
            }
        }

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

        void *h = dlopen(c_libPath, RTLD_NOW);
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

        int rc = pfn_main(argc, argv);
        fflush(NULL);
        _exit(rc);
    }

    /* Parent process */
    free(c_libPath);
    free(c_logPath);
    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    for (int i = 0; i < env_count; i++) free(env_entries[i]);
    free(env_entries);

    int status = 0;
    int elapsed_ms = 0;
    int pid_res = 0;

    while (elapsed_ms < timeoutMs) {
        pid_res = waitpid(pid, &status, WNOHANG);
        if (pid_res != 0) {
            break;
        }
        usleep(10000); /* 10 ms */
        elapsed_ms += 10;
    }

    if (pid_res == 0) {
        pid_res = waitpid(pid, &status, WNOHANG);
    }

    char retbuf[64];
    if (pid_res == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        snprintf(retbuf, sizeof(retbuf), "timeout");
    } else if (WIFEXITED(status)) {
        snprintf(retbuf, sizeof(retbuf), "exit:%d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        snprintf(retbuf, sizeof(retbuf), "signal:%d", WTERMSIG(status));
    } else {
        snprintf(retbuf, sizeof(retbuf), "exit:1");
    }

    return (*env)->NewStringUTF(env, retbuf);
}
