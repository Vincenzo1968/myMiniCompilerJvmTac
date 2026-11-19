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
    TAC_READ       // read res
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
void tacGenerate(nodeType *pTree, Scope *pScope, int optLevel);

/* Funzioni di utilita' per la IR (usate anche nei livelli -O2/-O3) */
void tacInit(TACList *list);
void tacFree(TACList *list);
void tacEmit(TACList *list, TACOp op, const char *arg1, const char *arg2, const char *res, enumTipo1 t1);
void tacPrint(const TACList *list);

nodeType *tacOptimizeAST(nodeType *p);
void tacOptimizeO1(TACList *list);
void tacOptimizeO2(TACList *list);
void tacOptimizeO3(TACList *list);

#endif /* TAC_H */
