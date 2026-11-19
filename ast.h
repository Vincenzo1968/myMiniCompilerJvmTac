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

#ifndef AST_H
#define AST_H

#include "symtab.h"

typedef enum { typeConst, typeId, typeOperator } nodeEnum;

/* costanti */
typedef struct
{
	char lexeme[128]; // usato solo per le costanti numeriche.
	Valore value;
} constNodeType;

/* identificatori */
typedef struct
{
	char *name;
	char *uniqueName;
	Valore value;
	char bInizializzato;
} idNodeType;

/* operatori */
typedef struct
{
    int oper;                   /* operatore */
    int nops;                   /* numero di operandi */
    struct nodeTypeTag *op[1];  /* operandi (espandibile) */
} operatorNodeType;

typedef struct nodeTypeTag
{
    nodeEnum type;
	enumTipo0 type0;			/* T_SIMPLE, T_ARRAY */
	enumTipo1 type1;			/* T_INTEGER, T_REAL, T_STRING, T_BOOLEAN */
	enumTipo2 type2;			/* T_CONST, T_VAR */

    /* union deve essere l'ultima voce in nodeType */
    /* perché operNodeType può crescere dinamicamente */
    union
	{
        constNodeType con;			/* costanti */
        idNodeType id;				/* identificatori */
        operatorNodeType opr;		/* operatori */
    };
} nodeType;

/* prototypes */
nodeType *opr(int oper, int nops, ...);
nodeType *id(char *name, char *uniqueName, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore value, char bInizializzato);
nodeType *con(enumTipo1 t1, char *lexeme, Valore value);
void freeNode(nodeType *p);

#endif // AST_H
