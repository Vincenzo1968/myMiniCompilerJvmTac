#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "parser.h"
#include "symtab.h"
#include "jvm.h"
#include "tac.h"

typedef struct {
    int thread_id;
    const char *prg_path;
    int gen_tac;
    int gen_jvm;
    int opt_level;
    int expect_ok;
    int ok;
} ThreadJob;

static void *worker_compile(void *arg)
{
    ThreadJob *job = (ThreadJob *)arg;
    CompilerContext *ctx = (CompilerContext *)malloc(sizeof(CompilerContext));
    nodeType *pTree = NULL;
    int ret = 0;

    if (!ctx) {
        fprintf(stderr, "[Thread %d] Errore allocazione CompilerContext\n", job->thread_id);
        job->ok = 0;
        return NULL;
    }

    ctx_init(ctx);
    scopeInit(ctx, &ctx->g_Scope);

    ret = Parse(ctx, (char *)job->prg_path, &pTree, job->gen_tac, job->gen_jvm);

    if (ret && pTree) {
        if (job->gen_tac) {
            tacGenerate(ctx, pTree, &ctx->g_Scope, job->opt_level);
        } else if (job->gen_jvm) {
            jvmStart(ctx, (char *)job->prg_path, &pTree, &ctx->g_Scope, job->opt_level);
        }
    }

    scopeFree(ctx, &ctx->g_Scope);
    freeNode(ctx, pTree);
    free(ctx);

    job->ok = (ret == job->expect_ok) ? 1 : 0;
    if (!job->ok) {
        fprintf(stderr, "[Thread %d] Fallito %s (ret=%d, atteso=%d)\n",
                job->thread_id, job->prg_path, ret, job->expect_ok);
    }

    return NULL;
}

typedef struct {
    const char *filename;
    int gen_tac;
    int gen_jvm;
    int opt_level;
    int expect_ok;
} JobSpec;

int main(int argc, char **argv)
{
    const char *prg_dir = (argc >= 2) ? argv[1] : "./prg_test";

    static const JobSpec specs[] = {
        /* Thread in modalita' TAC (vari livelli -O0..-O3 + error recovery) */
        { "myprog_01.prg",           1, 0, 0, 1 },
        { "myprog_02.prg",           1, 0, 2, 1 },
        { "myprog_03.prg",           1, 0, 1, 1 },
        { "test_licm.prg",           1, 0, 3, 1 },
        { "test_unroll.prg",         1, 0, 3, 1 },
        { "test_gcp.prg",            1, 0, 2, 1 },
        { "myprog_err.prg",          1, 0, 0, 0 },
        { "test_error_recovery.prg", 1, 0, 0, 0 },

        /* Thread in modalita' JVM (file distinti per non sovrascrivere lo stesso .class) */
        { "myprog_01.prg",           0, 1, 3, 1 },
        { "myprog_02.prg",           0, 1, 3, 1 },
        { "myprog_03.prg",           0, 1, 3, 1 },
        { "test_licm.prg",           0, 1, 3, 1 },
        { "test_unroll.prg",           0, 1, 3, 1 },
        { "test_gcp.prg",            0, 1, 2, 1 }
    };

    const int n = (int)(sizeof(specs) / sizeof(specs[0]));
    pthread_t *threads = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    ThreadJob *jobs = (ThreadJob *)calloc((size_t)n, sizeof(ThreadJob));
    char (*paths)[1024] = malloc((size_t)n * sizeof(*paths));
    int i;
    int all_ok = 1;

    if (!threads || !jobs || !paths) {
        free(threads);
        free(jobs);
        free(paths);
        return 1;
    }

    for (i = 0; i < n; i++) {
        snprintf(paths[i], sizeof(paths[i]), "%s/%s", prg_dir, specs[i].filename);
        jobs[i].thread_id = i + 1;
        jobs[i].prg_path  = paths[i];
        jobs[i].gen_tac   = specs[i].gen_tac;
        jobs[i].gen_jvm   = specs[i].gen_jvm;
        jobs[i].opt_level = specs[i].opt_level;
        jobs[i].expect_ok = specs[i].expect_ok;
        jobs[i].ok        = 0;

        if (pthread_create(&threads[i], NULL, worker_compile, &jobs[i]) != 0) {
            perror("pthread_create");
            all_ok = 0;
            break;
        }
    }

    for (i = 0; i < n; i++) {
        if (threads[i]) {
            pthread_join(threads[i], NULL);
            if (!jobs[i].ok)
                all_ok = 0;
        }
    }

    free(threads);
    free(jobs);
    free(paths);

    return all_ok ? 0 : 1;
}
