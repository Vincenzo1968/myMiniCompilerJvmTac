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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "context.h"
#include "ast.h"

#include <stddef.h>

nodeType *con(CompilerContext *ctx, enumTipo1 t1, char *lexeme, Valore value)
{
	(void)ctx;
    nodeType *p = NULL;
    size_t nodeSize;

    nodeSize = sizeof(nodeType);
    if ((p = malloc(nodeSize)) == NULL)
	{
		printf("Errore: memoria insufficiente.\n");
		return NULL;
	}

    p->type = typeConst;
	p->type0 = T_SIMPLE;
	p->type1 = t1;
	p->type2 = T_CONST;
	p->con.value = value;

	if ( lexeme != NULL )
		strcpy(p->con.lexeme, lexeme);
	else
		strcpy(p->con.lexeme, "");

    return p;
}

nodeType *id(CompilerContext *ctx, char *name, char *uniqueName, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore value, char bInizializzato)
{
	(void)ctx;
    nodeType *p = NULL;
    size_t nodeSize;

    nodeSize = sizeof(nodeType);
    if ((p = malloc(nodeSize)) == NULL)
	{
		printf("Errore: memoria insufficiente.\n");
		return NULL;
	}

    p->type = typeId;
	p->type0 = t0;
	p->type1 = t1;
	p->type2 = t2;
	p->id.name = (char*)malloc(sizeof(char)*strlen(name)+1);
	strcpy(p->id.name, name);
	p->id.uniqueName = (char*)malloc(sizeof(char)*strlen(uniqueName)+1);
	strcpy(p->id.uniqueName, uniqueName);
	p->id.value = value;
	p->id.bInizializzato = bInizializzato;

    return p;
}

nodeType *opr(CompilerContext *ctx, int oper, int nops, ...)
{
	(void)ctx;
    va_list ap;
    nodeType *p = NULL;
    size_t nodeSize;
    int i;

	size_t oprSize = offsetof(nodeType, opr.op) + (size_t)nops * sizeof(nodeType*);
	nodeSize = (oprSize > sizeof(nodeType)) ? oprSize : sizeof(nodeType);

    if ((p = malloc(nodeSize)) == NULL)
	{
		printf("Errore: memoria insufficiente.\n");
		return NULL;
	}
	
	p->type0 = 0;
	p->type1 = 0;
	p->type2 = 0;
    p->type = typeOperator;
    p->opr.oper = oper;
    p->opr.nops = nops;
    va_start(ap, nops);
    for (i = 0; i < nops; i++)
        p->opr.op[i] = va_arg(ap, nodeType*);
    va_end(ap);
    return p;
}

nodeType *oprN(CompilerContext *ctx, int oper, int nops, nodeType **ops)
{
	(void)ctx;
	nodeType *p = NULL;
	size_t nodeSize;
	int i;
	size_t oprSize = offsetof(nodeType, opr.op) + (size_t)nops * sizeof(nodeType*);
	nodeSize = (oprSize > sizeof(nodeType)) ? oprSize : sizeof(nodeType);

	if ((p = malloc(nodeSize)) == NULL)
	{
		printf("Errore: memoria insufficiente.\n");
		return NULL;
	}
	p->type0 = 0;
	p->type1 = 0;
	p->type2 = 0;
	p->type = typeOperator;
	p->opr.oper = oper;
	p->opr.nops = nops;
	p->opr.blockId = 0;
	for (i = 0; i < nops; i++)
		p->opr.op[i] = ops[i];
	return p;
}

void freeNode(CompilerContext *ctx, nodeType *p)
{
    int i;

    if (!p)
		return;

    if (p->type == typeOperator)
	{
        for (i = 0; i < p->opr.nops; i++)
            freeNode(ctx, p->opr.op[i]);
    }

	if ( p->type == typeId )
	{
		// name e uniqueName sono malloc'ate in id() (ast.c):
		// appartengono al nodo AST, quindi le libera freeNode. 
		free(p->id.name);
		free(p->id.uniqueName);

		if ( p->type0 == T_ARRAY && p->id.value.pArrayDef != NULL )
		{
			if ( p->id.value.pArrayDef->indexes != NULL )
			{
				for ( i = 0; i < p->id.value.pArrayDef->countIndexes; i++ )
				{
					freeNode(ctx, p->id.value.pArrayDef->indexes[i]);
				}
				free(p->id.value.pArrayDef->indexes);
				p->id.value.pArrayDef->indexes = NULL;
			}
			free(p->id.value.pArrayDef);
			p->id.value.pArrayDef = NULL;
		}
	}

	if ( p->type == typeConst && p->type1 == T_STRING )
	{
		// strVal della costante stringa e' malloc'ata in expr4():
		// appartiene all'AST. free(NULL) e' comunque sicuro.
		free(p->con.value.strVal);
	}

    free (p);
}
