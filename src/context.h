#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdio.h>
#include "symtab.h"
#include "scanner.h"
#include "ast.h"
#include <stdint.h>

#ifndef JVM_MAX_CP_BYTES
#define JVM_MAX_CP_BYTES 65536
#endif

#ifndef JVM_MAX_CODE_BYTES
#define JVM_MAX_CODE_BYTES 65536
#endif

typedef struct CompilerContext {
    /* da main.c */
    Scope g_Scope;
    enumTipo1 g_t1;
    int g_label;
    int g_temp;

    /* da jvm.c */
    uint16_t g_ConstantPoolCount;
    uint8_t g_ArrayConstantPool[JVM_MAX_CP_BYTES];
    uint8_t g_ArrayCode[JVM_MAX_CODE_BYTES];
    int g_IndexConstantPool;
    int g_IndexBytecodeArray;
    int g_myType1;
    uint16_t g_max_stack;
    uint16_t g_max_locals;
    uint32_t g_code_length;
    int g_cur_stack;

    /* da parser.c */
    Token g_Token;
    int g_isCodeBlock;
    int g_isStmtList;
    int g_isREAD;
    int g_numErrors;
    int g_numWarnings;
    int g_currType;
    Valore g_currConstValue;
    int g_currConstType;
    nodeType *g_stack[4096];
    int g_top;
    int g_genTAC;
    int g_genJVM;
    int g_NextFrameIndex;

    /* da scanner.c */
    char *g_szInput;
    int g_pos;
    int g_line;
    int g_column;
    int g_depth;
} CompilerContext;

void ctx_init(CompilerContext *ctx);

#endif /* CONTEXT_H */
