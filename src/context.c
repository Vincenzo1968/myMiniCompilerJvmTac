#include <string.h>
#include <stdlib.h>
#include "context.h"

void ctx_init(CompilerContext *ctx)
{
    if (!ctx)
		return;
		
    memset(ctx, 0, sizeof(CompilerContext));
    
	ctx->interpDecls.currentBlockId = -1;
    scopeInit(ctx, &ctx->interpScope);
    scopePush(ctx, &ctx->interpScope);

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

void ctx_interp_cleanup(CompilerContext *ctx)
{
    int i;
    if (!ctx)
        return;

    scopeFree(ctx, &ctx->interpScope);

    if (ctx->g_currConstType == T_STRING && ctx->g_currConstValue.strVal != NULL) {
        free(ctx->g_currConstValue.strVal);
        ctx->g_currConstValue.strVal = NULL;
        ctx->g_currConstType = -1;
    }

    for (i = 0; i < ctx->interpDecls.count; i++) {
        InterpVarDecl *d = &ctx->interpDecls.decls[i];
        free(d->name);
        if (d->t1 == T_STRING && d->bInizializzato && d->val.strVal != NULL) {
            free(d->val.strVal);
        }
    }
    free(ctx->interpDecls.decls);
    ctx->interpDecls.decls = NULL;
    ctx->interpDecls.count = 0;
    ctx->interpDecls.cap = 0;
    ctx->interpDecls.nextBlockId = 0;
    ctx->interpDecls.currentBlockId = -1;
    ctx->interpDecls.rootInitDone = 0;
    ctx->interpDecls.nextRuntimeBlock = 0;
}

