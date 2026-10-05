#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <android/log.h>

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
    /* Compute the log directory before fork; child only calls chdir(). */
    const char *log_slash = strrchr(c_logPath, '/');
    size_t log_dir_len = log_slash ? (size_t)(log_slash - c_logPath) : 0;
    char *c_logDir = log_slash ? strndup(c_logPath, log_dir_len ? log_dir_len : 1) : strdup(".");

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

    /* Build runnerPath: dirname(c_libPath) + "/libpt_runner.so" */
    char *runner_path = NULL;
    const char *last_slash = strrchr(c_libPath, '/');
    if (last_slash) {
        size_t dir_len = (size_t)(last_slash - c_libPath);
        const char *runner_name = "/libpt_runner.so";
        runner_path = (char *)malloc(dir_len + strlen(runner_name) + 1);
        if (runner_path) {
            memcpy(runner_path, c_libPath, dir_len);
            strcpy(runner_path + dir_len, runner_name);
        }
    } else {
        runner_path = strdup("libpt_runner.so");
    }

    /* Build new_argv: {runnerPath, c_libPath, argv[0..argc-1], NULL} */
    char **new_argv = (char **)calloc(argc + 3, sizeof(char *));
    if (new_argv && runner_path) {
        new_argv[0] = runner_path;
        new_argv[1] = c_libPath;
        for (int i = 0; i < argc; i++) {
            new_argv[2 + i] = argv[i];
        }
        new_argv[2 + argc] = NULL;
    } else {
        free(runner_path);
        free(new_argv);
        free(c_libPath);
        free(c_logPath);
        free(c_logDir);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        for (int i = 0; i < env_count; i++) free(env_entries[i]);
        free(env_entries);
        return (*env)->NewStringUTF(env, "exit:-1");
    }

    /* envp built pre-fork (child must not allocate). Custom entries first:
     * getenv() returns the first match, so they override inherited ones. */
    extern char **environ;
    int inherited = 0;
    while (environ && environ[inherited]) inherited++;
    char **envp = (char **)calloc(env_count + inherited + 1, sizeof(char *));
    int envc = 0;
    if (envp) {
        for (int i = 0; i < env_count; i++)
            if (env_entries[i] && env_entries[i][0] != '\0') envp[envc++] = env_entries[i];
        for (int i = 0; i < inherited; i++) envp[envc++] = environ[i];
    }
    static const char exec_fail[] = "FAIL exec libpt_runner.so\n";

    /* 2. Fork */
    pid_t pid = envp ? fork() : -1;
    if (pid < 0) {
        /* fork failed */
        free(envp);
        free(runner_path);
        free(new_argv);
        free(c_libPath);
        free(c_logPath);
        free(c_logDir);
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
        if (c_logDir) (void)chdir(c_logDir);
        execve(runner_path, new_argv, envp);

        write(STDERR_FILENO, exec_fail, sizeof(exec_fail) - 1);
        _exit(127);
    }

    /* Parent process */
    free(envp);
    free(runner_path);
    free(new_argv);
    free(c_libPath);
    free(c_logPath);
    free(c_logDir);
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
