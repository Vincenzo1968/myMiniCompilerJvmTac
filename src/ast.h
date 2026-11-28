/*
Copyright (C) 2026 Vincenzo Lo Cicero

Author: Vincenzo Lo Cicero.
e-mail: vincenzolocicero68@gmail.com

This file is part of myMiniCompilerJvmTac.

myMiniCompilerJvmTac is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any side version.

myMiniCompilerJvmTac is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty
MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE. See the
GNU General Public License for details.

You should have received a copy of the GNU General Public License
along with mypdfsearch. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AST_H
#define AST_H


struct CompilerContext;
typedef struct CompilerContext CompilerContext;
#include "symtab.h"

typedef enum { typeConst, typeId, typeOperator } nodeEnum;

/* constants */
typedef struct
{
	char lexeme[128]; // used only for numeric constants.
	Valore value;
} constNodeType;

/* identifiers */
typedef struct
{
	char *name;
	char *uniqueName;
	Valore value;
	char bInizializzato;
} idNodeType;

/* operators */
typedef struct
{
    int oper;                   /* operator */
    int nops;  
    int blockId;                /* number of operands */
    struct nodeTypeTag *op[1];  /* operands (expandable) */
} operatorNodeType;

typedef struct nodeTypeTag
{
    nodeEnum type;
	enumTipo0 type0;			/* T_SIMPLE, T_ARRAY */
	enumTipo1 type1;			/* T_INTEGER, T_REAL, T_STRING, T_BOOLEAN */
	enumTipo2 type2;			/* T_CONST, T_VAR */

	// union must be the last entry in nodeType 
	// because operNodeType can grow dynamically 
    union
	{
        constNodeType con;			/* constants */
        idNodeType id;				/* identifiers */
        operatorNodeType opr;		/* operators */
    };
} nodeType;

/* prototypes */
nodeType *opr(CompilerContext *ctx, int oper, int nops, ...);
/* Like opr, but with operands in an array (variable count, e.g., call arguments). */
nodeType *oprN(CompilerContext *ctx, int oper, int nops, nodeType **ops);
nodeType *id(CompilerContext *ctx, char *name, char *uniqueName, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore value, char bInizializzato);
nodeType *con(CompilerContext *ctx, enumTipo1 t1, char *lexeme, Valore value);
void freeNode(CompilerContext *ctx, nodeType *p);

#endif // AST_H
