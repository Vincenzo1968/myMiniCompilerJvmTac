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
    ctx->g_curFunc = -1;

    /* Valori iniziali da scanner.c */
    ctx->g_szInput = NULL;
    ctx->g_pos = 0;
    ctx->g_line = 1;
    ctx->g_column = 1;
    ctx->g_depth = 0;    
}

void funcFreeParams(FuncParam *params, int nparams)
{
    int i;
    if (!params)
        return;
    for (i = 0; i < nparams; i++)
        free(params[i].name);
    free(params);
}

int funcFind(CompilerContext *ctx, const char *name)
{
    int i;
    for (i = 0; i < ctx->funcCount; i++)
        if (strcmp(ctx->funcs[i].name, name) == 0)
            return i;
    return -1;
}

int funcDeclare(CompilerContext *ctx, const char *name, int isVoid, enumTipo1 retType,
                int nparams, FuncParam *params, int line)
{
    FuncDef *f;
    if (ctx->funcCount >= ctx->funcCap)
    {
        int newCap = ctx->funcCap ? ctx->funcCap * 2 : 8;
        FuncDef *tmp = (FuncDef*)realloc(ctx->funcs, (size_t)newCap * sizeof(FuncDef));
        if (!tmp)
            return -1;
        ctx->funcs = tmp;
        ctx->funcCap = newCap;
    }
    f = &ctx->funcs[ctx->funcCount];
    memset(f, 0, sizeof(*f));
    f->name = strdup(name);
    if (!f->name)
        return -1;
    f->isVoid = isVoid;
    f->retType = retType;
    f->nparams = nparams;
    f->params = params;
    f->line = line;
    return ctx->funcCount++;
}

int funcSignatureEquals(const FuncDef *a, int isVoid, enumTipo1 retType, int nparams, const FuncParam *params)
{
    int i;
    if (a->isVoid != isVoid || (!isVoid && a->retType != retType) || a->nparams != nparams)
        return 0;
    for (i = 0; i < nparams; i++)
    {
        if (a->params[i].t0 != params[i].t0 || a->params[i].t1 != params[i].t1 ||
            a->params[i].isRef != params[i].isRef || a->params[i].dims != params[i].dims)
            return 0;
    }
    return 1;
}

void ctx_interp_cleanup(CompilerContext *ctx)
{
    int i;
    if (!ctx)
        return;

    for (i = 0; i < ctx->funcCount; i++)
    {
        free(ctx->funcs[i].name);
        funcFreeParams(ctx->funcs[i].params, ctx->funcs[i].nparams);
        freeNode(ctx, ctx->funcs[i].body);
    }
    free(ctx->funcs);
    ctx->funcs = NULL;
    ctx->funcCount = 0;
    ctx->funcCap = 0;

    scopeFree(ctx, &ctx->interpScope);

    /* nodi di inizializzazione mai agganciati all'albero (errori di parsing) */
    for (i = 0; i < ctx->g_initCount; i++)
        freeNode(ctx, ctx->g_initNodes[i]);
    free(ctx->g_initNodes);
    ctx->g_initNodes = NULL;
    ctx->g_initCount = 0;
    ctx->g_initCap = 0;

    if (ctx->g_currConstType == T_STRING && ctx->g_currConstValue.strVal != NULL) {
        free(ctx->g_currConstValue.strVal);
        ctx->g_currConstValue.strVal = NULL;
        ctx->g_currConstType = -1;
    }

    for (i = 0; i < ctx->interpDecls.count; i++) {
        InterpVarDecl *d = &ctx->interpDecls.decls[i];
        free(d->name);
        if (d->t0 == T_ARRAY) {
            arrayDefFreeEmpty(d->val.pArrayDef);
            d->val.pArrayDef = NULL;
        }
        if (d->t0 != T_ARRAY && d->t1 == T_STRING && d->bInizializzato && d->val.strVal != NULL) {
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

