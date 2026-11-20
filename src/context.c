#include <string.h>
#include "context.h"

void ctx_init(CompilerContext *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(CompilerContext));

    /* Valori iniziali da main.c */
    ctx->g_t1 = T_INTEGER;
    ctx->g_label = 1;
    ctx->g_temp = 1;

    /* Valori iniziali da jvm.c */
    ctx->g_ConstantPoolCount = 68;
    ctx->g_IndexConstantPool = 0;
    ctx->g_IndexBytecodeArray = 0;
    ctx->g_myType1 = -1;
    ctx->g_max_stack = 0;
    ctx->g_max_locals = 1;
    ctx->g_code_length = 0;
    ctx->g_cur_stack = 0;

    /* Valori iniziali da parser.c */
    ctx->g_isCodeBlock = 0;
    ctx->g_isStmtList = 0;
    ctx->g_isREAD = 0;
    ctx->g_numErrors = 0;
    ctx->g_numWarnings = 0;
    ctx->g_currType = 0;
    ctx->g_currConstType = -1;
    ctx->g_top = -1;
    ctx->g_genTAC = 0;
    ctx->g_genJVM = 0;
    ctx->g_NextFrameIndex = 1;

    /* Valori iniziali da scanner.c */
    ctx->g_szInput = NULL;
    ctx->g_pos = 0;
    ctx->g_line = 1;
    ctx->g_column = 1;
    ctx->g_depth = 0;
}
