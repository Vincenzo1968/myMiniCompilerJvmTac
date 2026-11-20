#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    int thread_id;
    const char *compiler_bin;
    const char *prg_file;
    const char *out_dir;
    int exit_code;
} WorkerArgs;

static int run_in_dir(const char *out_dir, const char *compiler_bin, const char *prg_file) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        if (mkdir(out_dir, 0777) != 0 && chdir(out_dir) != 0) {
            /* se esiste già, prova comunque chdir */
        }
        if (chdir(out_dir) != 0) {
            perror("chdir");
            _exit(127);
        }
        execl(compiler_bin, compiler_bin, prg_file, "jvm", (char *)NULL);
        perror("execl");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void *worker_fn(void *arg) {
    WorkerArgs *w = (WorkerArgs *)arg;
    w->exit_code = run_in_dir(w->out_dir, w->compiler_bin, w->prg_file);
    printf("[Thread %d] %s -> exit=%d (dir: %s)\n",
           w->thread_id, w->prg_file, w->exit_code, w->out_dir);
    return NULL;
}

int main(void) {
    const char *bin = "/home/vincenzo/AAA_DATI_21042026/MyGit/myMiniCompilerJvmTac/src/myCompiler";
    const char *root = "/home/vincenzo/AAA_DATI_21042026/MyGit/myMiniCompilerJvmTac";

    char prg1[512], prg2[512];
    snprintf(prg1, sizeof(prg1), "%s/prg_test/myprog_01.prg", root);
    snprintf(prg2, sizeof(prg2), "%s/prg_test/test_unroll.prg", root);

    WorkerArgs args[2] = {
        { 1, bin, prg1, "/tmp/pthread_t1", -1 },
        { 2, bin, prg2, "/tmp/pthread_t2", -1 },
    };

    pthread_t t[2];
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&t[i], NULL, worker_fn, &args[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(t[i], NULL);
    }

    int ok = (args[0].exit_code == 0 && args[1].exit_code == 0) ? 0 : 1;
    return ok;
}