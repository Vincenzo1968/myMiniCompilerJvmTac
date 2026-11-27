/*    Copyright (C) 2026 Vincenzo Lo Cicero

Author: Vincenzo Lo Cicero.
e-mail: vincenzolocicero68@gmail.com

This file is part of myMiniCompilerJvmTac.

mypdfsearch is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any side version.

mypdfsearch is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty
MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE. See the
GNU General Public License for details.

You should have received a copy of the GNU General Public License
along with mypdfsearch. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TAC_H
#define TAC_H


struct CompilerContext;
typedef struct CompilerContext CompilerContext;
#include "ast.h"
#include "scanner.h"

typedef enum {
    TAC_ASSIGN,    // res = arg1
    TAC_NEG,       // res = neg arg1
    TAC_NOT,       // res = not arg1
    TAC_ADD,       // res = arg1 + arg2
    TAC_SUB,       // res = arg1 - arg2
    TAC_MUL,       // res = arg1 * arg2
    TAC_DIV,       // res = arg1 / arg2
    TAC_LT,        // res = arg1 < arg2
    TAC_LE,        // res = arg1 <= arg2
    TAC_GT,        // res = arg1 > arg2
    TAC_GE,        // res = arg1 >= arg2
    TAC_EQ,        // res = arg1 == arg2
    TAC_NE,        // res = arg1 != arg2
    TAC_AND,       // res = arg1 and arg2
    TAC_OR,        // res = arg1 or arg2
    TAC_LOAD_IDX,  // res = arg1[arg2]
    TAC_STORE_IDX, // res[arg1] = arg2
    TAC_LABEL,     // res:
    TAC_GOTO,      // goto res
    TAC_IF_FALSE,  // ifFalse arg1 goto res
    TAC_IF_TRUE,   // if arg1 goto res
    TAC_PRINT,     // print arg1
    TAC_READ,      // read res
    /* functions (appended at the end to preserve existing enum values) */
    TAC_FUNC,      // func res(arg1) : function start; arg1 = parameters, arg2 = return type
    TAC_ENDFUNC,   // endfunc res : function end (res = name; "main" closes the main block)
    TAC_PARAM,     // param arg1 : pass-by-value argument
    TAC_PARAM_REF, // param ref arg1 : scalar variable passed by reference
    TAC_PARAM_ARR, // param array arg1 : array passed by reference
    TAC_CALL,      // [res =] call arg1, arg2 : arg1 = function, arg2 = argument count
    TAC_RETURN     // return [arg1]
} TACOp;

typedef struct {
    TACOp op;
    char *arg1;
    char *arg2;
    char *res;
    enumTipo1 type1;
    int dead;
} TACInstr;

typedef struct {
    TACInstr *items;
    int count;
    int capacity;
} TACList;

/* API principale chiamata da main.c */
void tacGenerate(CompilerContext *ctx, nodeType *pTree, Scope *pScope, int optLevel);

/* Utility functions for IR (also used in -O2/-O3 levels) */
void tacInit(CompilerContext *ctx, TACList *list);
void tacFree(CompilerContext *ctx, TACList *list);
void tacEmit(CompilerContext *ctx, TACList *list, TACOp op, const char *arg1, const char *arg2, const char *res, enumTipo1 t1);
void tacPrint(CompilerContext *ctx, const TACList *list);

nodeType *tacOptimizeAST(CompilerContext *ctx, nodeType *p);
void tacOptimizeO1(CompilerContext *ctx, TACList *list);
void tacOptimizeO2(CompilerContext *ctx, TACList *list);
void tacOptimizeO3(CompilerContext *ctx, TACList *list);

void tacBuild(CompilerContext *ctx, nodeType **ppTree, Scope *pScope, int optLevel, TACList *outTac);

#endif /* TAC_H */
