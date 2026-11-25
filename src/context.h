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

/* Pool semplice per costanti reali (float/double) che richiedono un indirizzo in memoria */
typedef struct {
    char *label;
    double value;
} RealConst;

typedef struct {
    int blockId;          /* -1 = scope globale, >= 0 = id blocco */
    char *name;
    enumTipo0 t0;
    enumTipo1 t1;
    enumTipo2 t2;
    Valore val;
    char bInizializzato;
} InterpVarDecl;

/* Funzioni (prototipi e definizioni) */
typedef struct {
    char *name;
    char *uniqueName;   /* nome univoco nell'intero programma (usato da TAC e backend) */
    enumTipo0 t0;       /* T_SIMPLE oppure T_ARRAY (sempre per riferimento) */
    enumTipo1 t1;
    int isRef;          /* parametro scalare passato per riferimento ('ref') */
    int dims;           /* numero di dimensioni se array */
} FuncParam;

typedef struct {
    char *name;
    int isVoid;
    enumTipo1 retType;  /* significativo solo se !isVoid */
    int nparams;
    FuncParam *params;
    int defined;        /* 1 dopo la definizione (corpo) */
    int line;
    nodeType *body;     /* T_CODE_BLOCK del corpo (NULL finche' non definita) */
} FuncDef;

typedef struct {
    InterpVarDecl *decls;
    int count;
    int cap;
    int nextBlockId;
    int currentBlockId;
    int rootInitDone;
    int nextRuntimeBlock;
} InterpDeclTable;

typedef struct CompilerContext {
    /* da main.c */
    Scope g_Scope;
    Scope interpScope;
    InterpDeclTable interpDecls;
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
    //nodeType *g_stack;
    nodeType *g_stack[4096];
    int g_top;
    int g_genTAC;
    int g_genJVM;
    int g_NextFrameIndex;
    /* Assegnazioni generate dagli inizializzatori delle dichiarazioni
       ("int x = 8;"): vengono accodate durante decl_list e poi anteposte
       allo stmt_list del blocco che le contiene. */
    nodeType **g_initNodes;
    int g_initCount;
    int g_initCap;

    /* funzioni */
    FuncDef *funcs;
    int funcCount;
    int funcCap;
    int g_curFunc;          /* indice della funzione in corso di parsing, -1 nel programma principale */
    /* interprete: stato del 'return' in corso */
    int interpReturning;
    Valore interpRetVal;
    enumTipo1 interpRetType;

    /* da scanner.c */
    char *g_szInput;
    int g_pos;
    int g_line;
    int g_column;
    int g_depth;

    /* da backend_x86_64.c */
    RealConst *real_consts;
    int real_const_count;
} CompilerContext;

void ctx_init(CompilerContext *ctx);
void ctx_interp_cleanup(CompilerContext *ctx);

/* Tabella delle funzioni. funcFind: indice o -1. funcDeclare: aggiunge (prende possesso
   di params) e restituisce l'indice. funcSignatureEquals: stesso tipo di ritorno e stessi parametri. */
int funcFind(CompilerContext *ctx, const char *name);
int funcDeclare(CompilerContext *ctx, const char *name, int isVoid, enumTipo1 retType,
                int nparams, FuncParam *params, int line);
int funcSignatureEquals(const FuncDef *a, int isVoid, enumTipo1 retType, int nparams, const FuncParam *params);
void funcFreeParams(FuncParam *params, int nparams);

#endif /* CONTEXT_H */
