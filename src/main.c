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

/*
Architettura :

    Unica pipeline di compilazione: AST -> TAC (-O0..-O3) -> JVM (.class),
    con -O3 attivo di default per il target jvm (e modificabile da riga
    di comando con -O0..-O3).

    Backend JVM aggiornato (src/jvm.c): allineato alle ottimizzazioni del TAC,
    con calcolo corretto dello stack massimo, gestione dei temporanei/label,
    supporto a ldc/ldc_w, confronti su interi/reali e array linearizzati.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
//#include "scanner.h"
#include "context.h"
#include "parser.h"
#include "symtab.h"
//#include "endianess.h"
#include "jvm.h"
#include "tac.h"

/*
expr:	term { (+|-) term }
term:	factor { (*|/) factor }
factor:	[-] expon
expon:	atom [^ expon]
atom:	'(' expr ')'
		| number
*/



char convertToBoolean(CompilerContext *ctx, enumTipo1 t1, Valore value)
{
	(void)ctx;
	char ret = 0;

	switch ( t1 )
	{
	case T_REAL:
		ret = value.dblVal ? 1 : 0;
		break;
	case T_INTEGER:
		ret = value.iVal ? 1 : 0;
		break;
	case T_BOOLEAN:
		ret = value.bVal ? 1 : 0;
		break;
	default:
		break;
	}

	return ret;
}

void AssignTypeChecking(CompilerContext *ctx, enumTipo1 lhsT1, Valore *lhsValue, enumTipo1 rhsT1, Valore rhsValue)
{
	(void)ctx;
	if ( lhsT1 == rhsT1 )
	{
		(*lhsValue) = rhsValue;
		return;
	}

	switch( lhsT1 )
	{
	case T_INTEGER:
		if ( rhsT1 == T_REAL )
			(*lhsValue).iVal = (int)rhsValue.dblVal;
		else if ( rhsT1 == T_BOOLEAN )
			(*lhsValue).iVal = rhsValue.bVal ? 1 : 0;
		break;
	case T_REAL:
		if ( rhsT1 == T_INTEGER )
			(*lhsValue).dblVal = (double)rhsValue.iVal;
		else if ( rhsT1 == T_BOOLEAN )
			(*lhsValue).dblVal = rhsValue.bVal ? 1.0 : 0.0;
		break;
	case T_BOOLEAN:
		if ( rhsT1 == T_INTEGER )
			(*lhsValue).bVal = rhsValue.iVal ? 1 : 0;
		if ( rhsT1 == T_REAL )
			(*lhsValue).bVal = rhsValue.dblVal ? 1 : 0;
		break;
	case T_STRING:
		break;
	default:
		break;
	}
}

Valore BinopValue(CompilerContext *ctx, int op, enumTipo1 lhsT1, Valore lhsValue, enumTipo1 rhsT1, Valore rhsValue)
{
	Valore retVal;
	enumTipo1 t1;

	if ( lhsT1 == T_INTEGER && rhsT1 == T_INTEGER )
	{
		t1 = T_INTEGER;
	}
	else if ( lhsT1 == T_INTEGER && rhsT1 == T_REAL )
	{
		t1 = T_REAL;
		lhsValue.dblVal = (double)lhsValue.iVal;
	}
	else if ( lhsT1 == T_REAL && rhsT1 == T_REAL )
	{
		t1 = T_REAL;
	}
	else if ( lhsT1 == T_REAL && rhsT1 == T_INTEGER )
	{
		t1 = T_REAL;
		rhsValue.dblVal = (double)rhsValue.iVal;
	}
	else if ( lhsT1 == T_STRING && rhsT1 == T_STRING )
	{
		t1 = T_STRING;
	}
	else
	{
	    t1 = T_INTEGER;
	}

	switch ( op )
	{
	case T_OP_LT:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal < rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal < rhsValue.dblVal;
		break;
	case T_OP_LE:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal <= rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal <= rhsValue.dblVal;
		break;
	case T_OP_EQ:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal == rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal == rhsValue.dblVal;
		break;
	case T_OP_NE:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal != rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal != rhsValue.dblVal;
		break;
	case T_OP_GT:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal > rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal > rhsValue.dblVal;
		break;
	case T_OP_GE:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal >= rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal >= rhsValue.dblVal;
		break;
	case T_OP_PLUS:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal + rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal + rhsValue.dblVal;
		else if ( t1 == T_STRING )
		{
			retVal.strVal = (char*)malloc(sizeof(char) * (strlen(lhsValue.strVal) + strlen(rhsValue.strVal)) + 1);
			if ( !retVal.strVal )
			{
				printf("Errore + string: memoria insufficiente.\n");
				return retVal;
			}
			strcpy(retVal.strVal, lhsValue.strVal);
			strcat(retVal.strVal, rhsValue.strVal);
		}
		break;
	case T_OP_MINUS:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal - rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal - rhsValue.dblVal;
		break;
	case T_OP_MULT:
		if ( t1 == T_INTEGER )
			retVal.iVal = lhsValue.iVal * rhsValue.iVal;
		else if ( t1 == T_REAL )
			retVal.dblVal = lhsValue.dblVal * rhsValue.dblVal;
		break;
	case T_OP_DIV:
		if ( t1 == T_INTEGER )
		{
			if ( rhsValue.iVal == 0 )
			{
				printf("Errore: divisione per zero.\n");
				retVal.iVal = 0;
			}
			else
			{
				retVal.iVal = lhsValue.iVal / rhsValue.iVal;
			}
		}
		else if ( t1 == T_REAL )
		{
			if ( rhsValue.dblVal == 0.0 )
			{
				printf("Errore: divisione per zero.\n");
				retVal.dblVal = 0;
			}
			else
			{
				retVal.dblVal = lhsValue.dblVal / rhsValue.dblVal;
			}
		}
		break;
	}

	ctx->g_t1 = t1;
	return retVal;
}

Valore interpret(CompilerContext *ctx, nodeType *p)
{
	char szInput[1024];

	static Valore valZero;
	valZero.iVal = 0;

    if (!p)
		return valZero;

    switch(p->type)
	{
    case typeConst:
		ctx->g_t1 = p->type1;
		return p->con.value;
    case typeId:
		{
			HashTable Dati;
			Valore retVal;
			if ( scopeFind(ctx, &ctx->g_Scope, p->id.name, &Dati) )
			{
				ctx->g_t1 = Dati.Tipo1;
				retVal = Dati.Value;

				if ( Dati.Tipo0 == T_ARRAY )
				{
					int k;
					int loc;
					if ( Dati.Value.pArrayDef->countIndexes == 1 )
					{
						loc = interpret(ctx, p->id.value.pArrayDef->indexes[0]).iVal;
					}
					else
					{
						loc = interpret(ctx, p->id.value.pArrayDef->indexes[0]).iVal * Dati.Value.pArrayDef->sizes[1] + interpret(ctx, p->id.value.pArrayDef->indexes[1]).iVal;
						for ( k = 1; k < Dati.Value.pArrayDef->countIndexes - 1; k++ )
						{
							loc *= Dati.Value.pArrayDef->sizes[k+1];
							loc += interpret(ctx, p->id.value.pArrayDef->indexes[k+1]).iVal;
						}
					}

					if ( Dati.Tipo1 == T_INTEGER )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						retVal.iVal = base[loc];
					}
					else if ( Dati.Tipo1 == T_REAL )
					{
						double *base = (double*)Dati.Value.pArrayDef->pArray;
						retVal.dblVal = base[loc];
					}
					else if ( Dati.Tipo1 == T_BOOLEAN )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						retVal.bVal = base[loc];
					}
					else if ( Dati.Tipo1 == T_STRING )
					{
						char **base = (char**)Dati.Value.pArrayDef->pArray;
						retVal.strVal = base[loc];
					}
				}
			}
			else
			{
				printf("Errore: X identificatore '%s' non dichiarato\n", p->id.name);
				ctx->g_t1 = T_INTEGER;
				retVal = valZero;
			}
			return retVal;
		}
    case typeOperator:
		{
			Valore retVal;
			Valore leftVal, rightVal;
			switch(p->opr.oper)
			{
			case T_KW_WHILE:
				while( convertToBoolean(ctx, p->type1, interpret(ctx, p->opr.op[0])) )
					interpret(ctx, p->opr.op[1]);
				ctx->g_t1 = T_INTEGER;
				return valZero;
			case T_KW_DO: // do-while
				do
				{
					interpret(ctx, p->opr.op[0]);
				} while( convertToBoolean(ctx, p->type1, interpret(ctx, p->opr.op[1])) );
				ctx->g_t1 = T_INTEGER;
				return valZero;
			case T_KW_IF:
				if ( convertToBoolean(ctx, p->type1, interpret(ctx, p->opr.op[0])) )
					interpret(ctx, p->opr.op[1]);
				else if ( p->opr.nops > 2 )
					interpret(ctx, p->opr.op[2]);
				ctx->g_t1 = T_INTEGER;
				return valZero;
			case T_KW_PRINT:
				retVal = interpret(ctx, p->opr.op[0]);
				if ( p->opr.op[0]->type1 == T_INTEGER )
					printf("%d", retVal.iVal);
				else if ( p->opr.op[0]->type1 == T_REAL )
					printf("%f", retVal.dblVal);
				else if ( p->opr.op[0]->type1 == T_BOOLEAN )
					printf("%s", retVal.bVal ? "true" : "false");
				else if (p->opr.op[0]->type1 == T_STRING)
					printf("%s", retVal.strVal);
				else
				{
					if ( ctx->g_t1 == T_INTEGER)
						printf("%d", retVal.iVal);
					else if (ctx->g_t1 == T_REAL)
						printf("%f", retVal.dblVal);
				}
				ctx->g_t1 = T_INTEGER;
				return valZero;
			case T_KW_READ:
				if (fgets(szInput, 1023, stdin) == NULL)
					szInput[0] = '\0';
				else if (strlen(szInput) > 0 && szInput[strlen(szInput) - 1] == '\n')
					szInput[strlen(szInput) - 1] = '\0';
				
				if ( szInput[strlen(szInput) - 1] == '\n' )
					szInput[strlen(szInput) - 1] = '\0';
					
				if ( p->opr.op[0]->type1 == T_INTEGER )
				{
					rightVal.iVal = atoi(szInput);
				}
				else if ( p->opr.op[0]->type1 == T_REAL )
				{
					rightVal.dblVal = atof(szInput);
				}
				else if ( p->opr.op[0]->type1 == T_BOOLEAN )
				{
					rightVal.bVal = atoi(szInput) ? 1 : 0;
				}
				else if ( p->opr.op[0]->type1 == T_STRING )
				{
					rightVal.strVal = szInput;
				}

				if ( p->opr.op[0]->type0 == T_ARRAY )
				{
					int k;
					int loc;
					HashTable Dati;

					retVal.pArrayDef = p->opr.op[0]->id.value.pArrayDef;

					scopeFind(ctx, &ctx->g_Scope, p->opr.op[0]->id.name, &Dati);

					if ( Dati.Value.pArrayDef->countIndexes == 1 )
					{
						loc = interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[0]).iVal;
					}
					else
					{
						loc = interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[0]).iVal * Dati.Value.pArrayDef->sizes[1] + interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[1]).iVal;
						for ( k = 1; k < Dati.Value.pArrayDef->countIndexes - 1; k++ )
						{
							loc *= Dati.Value.pArrayDef->sizes[k+1];
							loc += interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[k+1]).iVal;
						}
					}

					if ( p->opr.op[0]->type1 == T_INTEGER )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						base[loc] = rightVal.iVal;
					}
					else if ( p->opr.op[0]->type1 == T_REAL )
					{
						double *base = (double*)Dati.Value.pArrayDef->pArray;
						base[loc] = rightVal.dblVal;
					}
					else if ( p->opr.op[0]->type1 == T_BOOLEAN )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						base[loc] = rightVal.bVal;
					}
					else if ( p->opr.op[0]->type1 == T_STRING )
					{
						char **base = (char**)Dati.Value.pArrayDef->pArray;
						if ( base[loc] != NULL )
							free(base[loc]);
						base[loc] = (char*)malloc(sizeof(char) * strlen(rightVal.strVal) + 1);
						if ( !base[loc] )
						{
							printf("Errore: memoria insufficiente.\n");
							return valZero;
						}
						strcpy(base[loc], rightVal.strVal);
					}
				}
				else
				{
					scopeUpdateValue(ctx, &ctx->g_Scope,
									p->opr.op[0]->id.name,
									p->opr.op[0]->type0,
									p->opr.op[0]->type1,
									p->opr.op[0]->type2,
									rightVal,
									1,
									0);
				}

				ctx->g_t1 = T_INTEGER;
				return valZero;
			case T_STMT_LIST:
				interpret(ctx, p->opr.op[0]);
				return interpret(ctx, p->opr.op[1]);
			case T_CODE_BLOCK:
				return interpret(ctx, p->opr.op[0]);
			case T_OP_ASSIGN:
				AssignTypeChecking(ctx, p->opr.op[0]->type1, &leftVal, ctx->g_t1, interpret(ctx, p->opr.op[1]));
				if ( p->opr.op[0]->type0 == T_ARRAY )
				{
					int k;
					int loc;
					HashTable Dati;

					retVal.pArrayDef = p->opr.op[0]->id.value.pArrayDef;

					scopeFind(ctx, &ctx->g_Scope, p->opr.op[0]->id.name, &Dati);

					if ( Dati.Value.pArrayDef->countIndexes == 1 )
					{
						loc = interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[0]).iVal;
					}
					else
					{
						loc = interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[0]).iVal * Dati.Value.pArrayDef->sizes[1] + interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[1]).iVal;
						for ( k = 1; k < Dati.Value.pArrayDef->countIndexes - 1; k++ )
						{
							loc *= Dati.Value.pArrayDef->sizes[k+1];
							loc += interpret(ctx, p->opr.op[0]->id.value.pArrayDef->indexes[k+1]).iVal;
						}
					}

					if ( p->opr.op[0]->type1 == T_INTEGER )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						base[loc] = leftVal.iVal;
					}
					else if ( p->opr.op[0]->type1 == T_REAL )
					{
						double *base = (double*)Dati.Value.pArrayDef->pArray;
						base[loc] = leftVal.dblVal;
					}
					else if ( p->opr.op[0]->type1 == T_BOOLEAN )
					{
						int *base = (int*)Dati.Value.pArrayDef->pArray;
						base[loc] = leftVal.bVal;
					}
					else if ( p->opr.op[0]->type1 == T_STRING )
					{
						char **base = (char**)Dati.Value.pArrayDef->pArray;
						if ( base[loc] != NULL )
						{
							free(base[loc]);
							base[loc] = NULL;
						}
						base[loc] = (char*)malloc(sizeof(char) * strlen(leftVal.strVal) + 1);
						if ( !base[loc] )
						{
							printf("Errore: memoria insufficiente.\n");
							return valZero;
						}
						strcpy(base[loc], leftVal.strVal);
					}
				}
				else
				{
					retVal = leftVal;
				}
				scopeUpdateValue(ctx, &ctx->g_Scope,
								p->opr.op[0]->id.name,
								p->opr.op[0]->type0,
								p->opr.op[0]->type1,
								p->opr.op[0]->type2,
								retVal,
								1,
								0);

				return retVal;
			case T_OP_UMINUS:
				retVal = interpret(ctx, p->opr.op[0]);
				if ( p->opr.op[0]->type1 == T_INTEGER )
				{
					ctx->g_t1 = T_INTEGER;
					retVal.iVal = -retVal.iVal;
				}
				else if ( p->opr.op[0]->type1 == T_REAL )
				{
					ctx->g_t1 = T_REAL;
					retVal.dblVal = -retVal.dblVal;
				}
				return retVal;
			case T_OP_PLUS:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_PLUS, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_MINUS:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_MINUS, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_MULT:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_MULT, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_DIV:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_DIV, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_LT:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_LT, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_GT:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_GT, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_GE:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_GE, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_LE:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_LE, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_NE:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_NE, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_EQ:
				leftVal = interpret(ctx, p->opr.op[0]);
				rightVal = interpret(ctx, p->opr.op[1]);
				retVal = BinopValue(ctx, T_OP_EQ, p->opr.op[0]->type1, leftVal, p->opr.op[1]->type1, rightVal);
				return retVal;
			case T_OP_AND:
				leftVal.bVal = convertToBoolean(ctx, p->opr.op[0]->type1, interpret(ctx, p->opr.op[0]));
				rightVal.bVal = convertToBoolean(ctx, p->opr.op[1]->type1, interpret(ctx, p->opr.op[1]));
				retVal.bVal = leftVal.bVal && rightVal.bVal;
				return retVal;
			case T_OP_OR:
				leftVal.bVal = convertToBoolean(ctx, p->opr.op[0]->type1, interpret(ctx, p->opr.op[0]));
				rightVal.bVal = convertToBoolean(ctx, p->opr.op[1]->type1, interpret(ctx, p->opr.op[1]));
				retVal.bVal = leftVal.bVal || rightVal.bVal;
				return retVal;
			case T_OP_NOT:
				retVal.bVal = convertToBoolean(ctx, p->opr.op[0]->type1, interpret(ctx, p->opr.op[0]));
				retVal.bVal = !retVal.bVal;
				return retVal;
			}
		}
    }
    return valZero;
}

Valore GenThreeAddressCode(CompilerContext *ctx, nodeType *p)
{
	char szTemp[128];
	char szTemp2[128];

	int label1, label2;

	Valore retVal;
	Valore leftVal, rightVal;

	static Valore valZero;
	valZero.iVal = 0;
	valZero.dblVal = 0.0;
	valZero.bVal = FALSE;
	valZero.strVal = NULL;
	valZero.pArrayDef = NULL;

	retVal.iVal = 0;
	retVal.dblVal = 0.0;
	retVal.bVal = FALSE;
	retVal.strVal = NULL;
	retVal.pArrayDef = NULL;

	leftVal.iVal = 0;
	leftVal.dblVal = 0.0;
	leftVal.bVal = FALSE;
	leftVal.strVal = NULL;
	leftVal.pArrayDef = NULL;

	rightVal.iVal = 0;
	rightVal.dblVal = 0.0;
	rightVal.bVal = FALSE;
	rightVal.strVal = NULL;
	rightVal.pArrayDef = NULL;

    if (!p)
	{
		return valZero;
	}

    switch(p->type)
	{
    case typeConst:
		{
			ctx->g_t1 = p->type1;

			switch( p->type1 )
			{
			case T_INTEGER:
			case T_REAL:
			case T_BOOLEAN:
				retVal.strVal = (char*)malloc(sizeof(char)*strlen(p->con.lexeme)+1);
				if ( !retVal.strVal )
				{
					printf("Errore. Memoria insufficiente.\n");
					return valZero;
				}
				strcpy(retVal.strVal, p->con.lexeme);
				break;
			case T_STRING:
				retVal.strVal = (char*)malloc(sizeof(char)*strlen(p->con.value.strVal)*2+3);
				if ( !retVal.strVal )
				{
					printf("Errore. Memoria insufficiente.\n");
					return valZero;
				}
				strcpy(retVal.strVal, "\"");
				strcat(retVal.strVal, p->con.value.strVal);
				strcat(retVal.strVal, "\"");
				break;
			}

			return retVal;
		}
		break;
    case typeId:
		{
			HashTable Dati;
			if ( scopeFind(ctx, &ctx->g_Scope, p->id.name, &Dati) )
			{
				ctx->g_t1 = Dati.Tipo1;
				retVal = Dati.Value;

				if ( Dati.Tipo0 == T_ARRAY )
				{
					int k;
					int loc;
					int size = 0;

					if ( Dati.Tipo1 == T_INTEGER )
						size = sizeof(int);
					else if ( Dati.Tipo1 == T_REAL )
						size = sizeof(double);
					else if ( Dati.Tipo1 == T_BOOLEAN )
						size = sizeof(char);
					else if ( Dati.Tipo1 == T_STRING )
						size = sizeof(char*);

					retVal.strVal = (char*)malloc(sizeof(char)*strlen(p->id.name)+256);
					if ( !retVal.strVal )
					{
						printf("Errore. Memoria insufficiente.\n");
						return valZero;
					}
					strcpy(retVal.strVal, p->id.name);

					if ( Dati.Value.pArrayDef->countIndexes == 1 )
					{
						leftVal = GenThreeAddressCode(ctx, p->id.value.pArrayDef->indexes[0]);
						sprintf(szTemp, "t%d", ctx->g_temp++);
						printf("%s = %s * %d\n", szTemp, leftVal.strVal, size);
						strcpy(retVal.strVal, szTemp);
						sprintf(szTemp, "t%d", ctx->g_temp++);
						printf("%s = %s[%s]\n", szTemp, p->id.name, retVal.strVal);
						strcpy(retVal.strVal, szTemp);
					}
					else
					{
						loc = size * Dati.Value.pArrayDef->sizes[1];

						leftVal = GenThreeAddressCode(ctx, p->id.value.pArrayDef->indexes[0]);
						sprintf(szTemp, "t%d", ctx->g_temp++);
						printf("%s = %s * %d\n", szTemp, leftVal.strVal, loc);
						strcpy(retVal.strVal, szTemp);

						for ( k = 1; k < Dati.Value.pArrayDef->countIndexes; k++ )
						{
							if ( k < Dati.Value.pArrayDef->countIndexes - 1 )
								loc = size * Dati.Value.pArrayDef->sizes[k+1];
							else
								loc = size;

							leftVal = GenThreeAddressCode(ctx, p->id.value.pArrayDef->indexes[k]);
							sprintf(szTemp, "t%d", ctx->g_temp++);
							strcpy(szTemp2, szTemp);
							printf("%s = %s * %d\n", szTemp, leftVal.strVal, loc);
							sprintf(szTemp, "t%d", ctx->g_temp++);
							printf("%s = %s + %s\n", szTemp, retVal.strVal, szTemp2);
						}

						strcpy(retVal.strVal, szTemp);
						sprintf(szTemp, "t%d", ctx->g_temp++);

						printf("%s = %s[%s]\n", szTemp, p->id.name, retVal.strVal);
						strcpy(retVal.strVal, szTemp);
					}
				}
				else // Dati.Tipo0 == T_SIMPLE
				{
					retVal.strVal = (char*)malloc(sizeof(char)*strlen(p->id.name)+1);
					if ( !retVal.strVal )
					{
						printf("Errore. Memoria insufficiente.\n");
						return valZero;
					}
					strcpy(retVal.strVal, p->id.name);
				}
			}
			else
			{
				printf("Errore: X identificatore '%s' non dichiarato\n", p->id.name);
				ctx->g_t1 = T_INTEGER;
				retVal = valZero;
			}

			//printf("array ref: %s\n", retVal.strVal);

			return retVal;
		}
		break;
    case typeOperator:
		{
			switch(p->opr.oper)
			{
			case T_KW_WHILE:
				printf("L%03d:\n", label1 = ctx->g_label++);
				retVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				printf("if not %s goto L%03d\n", retVal.strVal, label2 = ctx->g_label++);
				retVal = GenThreeAddressCode(ctx, p->opr.op[1]);
				printf("goto L%03d\n", label1);
				printf("L%03d:\n", label2);
				return retVal;
			case T_KW_DO: // do-while
				printf("L%03d:\n", label1 = ctx->g_label++);
				retVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				retVal = GenThreeAddressCode(ctx, p->opr.op[1]);
				printf("if %s goto L%03d\n", retVal.strVal, label1);
				return retVal;
			case T_KW_IF:
				retVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				if (p->opr.nops > 2)
				{
					// if else
					printf("ifFalse %s goto L%03d\n", retVal.strVal, label1 = ctx->g_label++);
					GenThreeAddressCode(ctx, p->opr.op[1]);
					printf("goto L%03d\n", label2 = ctx->g_label++);
					printf("L%03d:\n", label1);
					retVal = GenThreeAddressCode(ctx, p->opr.op[2]);
					printf("L%03d:\n", label2);
				}
				else
				{
					// if
					printf("ifFalse %s goto L%03d\n", retVal.strVal, label1 = ctx->g_label++);
					retVal = GenThreeAddressCode(ctx, p->opr.op[1]);
					printf("L%03d:\n", label1);
				}
				return retVal;
			case T_KW_PRINT:
				retVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				printf("print %s\n", retVal.strVal);
				return retVal;
			case T_KW_READ:
				retVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				printf("read %s\n", retVal.strVal);
				return retVal;
			case T_STMT_LIST:
				GenThreeAddressCode(ctx, p->opr.op[0]);
				return GenThreeAddressCode(ctx, p->opr.op[1]);
			case T_CODE_BLOCK:
				return GenThreeAddressCode(ctx, p->opr.op[0]);
			case T_OP_ASSIGN:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				printf("%s = %s\n", leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_UMINUS:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = neg %s\n", szTemp, leftVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_PLUS:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s + %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_MINUS:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s - %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_MULT:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s * %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_DIV:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s / %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_LT:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s < %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_GT:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s > %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_GE:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s >= %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_LE:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s <= %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_NE:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s != %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_EQ:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s == %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_AND:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s and %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_OR:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				rightVal = GenThreeAddressCode(ctx, p->opr.op[1]);

				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = %s or %s\n", szTemp, leftVal.strVal, rightVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			case T_OP_NOT:
				leftVal = GenThreeAddressCode(ctx, p->opr.op[0]);
				sprintf(szTemp, "t%d", ctx->g_temp++);
				printf("%s = not %s\n", szTemp, leftVal.strVal);
				retVal.strVal = szTemp;
				return retVal;
			}
		}
		break;
    }
    return valZero;
}

/*
gcc -Wall -W -pedantic -O2 ast.c endianess.c jvm.c main.c parser.c scanner.c symtab.c tac.c -o myCompiler

./myCompiler prg_test/myprog_01.prg tac -O0

./myCompiler prg_test/myprog_01.prg tac -O1

./myCompiler prg_test/myprog_01.prg tac -O2

./myCompiler prg_test/myprog_01.prg tac -O3

------------------------------------------------------------------------

./myCompiler prg_test/myprog_02.prg tac -O0

./myCompiler prg_test/myprog_02.prg tac -O1

./myCompiler prg_test/myprog_02.prg tac -O2

./myCompiler prg_test/myprog_02.prg tac -O3

------------------------------------------------------------------------

./myCompiler prg_test/test_licm.prg tac -O0

./myCompiler prg_test/test_licm.prg tac -O1

./myCompiler prg_test/test_licm.prg tac -O2

./myCompiler prg_test/test_licm.prg tac -O3

------------------------------------------------------------------------

./myCompiler prg_test/test_unroll.prg tac -O0

./myCompiler prg_test/test_unroll.prg tac -O1

./myCompiler prg_test/test_unroll.prg tac -O2

./myCompiler prg_test/test_unroll.prg tac -O3

------------------------------------------------------------------------

./myCompiler prg_test/myprog_01.prg jvm

java myprog_01
*/

int main(int argc, char **argv)
{
	CompilerContext ctx;
	ctx_init(&ctx);

	//char *szInput = "expr:	term { ('+'|'-') term };\n"
	//				"term:	factor { ('*'|'/') factor };\n"
	//				"factor:	['-'] expon;\n"
	//				"expon:	atom { '^' expon };\n"
	//				"atom:	'(' expr ')'\n"
	//				"		| number;\n";

	int ret;
	int genTAC = 0;
	int genJVM = 0;
	
	int i;
	int optLevel = -1;

	nodeType *pTree = NULL;

	if ( argc < 2 )
	{
		printf("Specificare il nome di un file, prego\n");
		return -1;
	}
	
	if (argc == 2 && strcmp(argv[1], "--version") == 0)
	{	
		printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		return 0;
	}
	
	for (i = 2; i < argc; i++)
	{
		if (strcmp(argv[i], "tac") == 0)
			genTAC = 1;
		else if (strcmp(argv[i], "jvm") == 0)
			genJVM = 1;
		else if (strcmp(argv[i], "-O0") == 0)
			optLevel = 0;
		else if (strcmp(argv[i], "-O1") == 0)
			optLevel = 1;
		else if (strcmp(argv[i], "-O2") == 0)
			optLevel = 2;
		else if (strcmp(argv[i], "-O3") == 0)
			optLevel = 3;
	}

	if (optLevel < 0)
		optLevel = genJVM ? 3 : 0;
	
	scopeInit(&ctx, &ctx.g_Scope);

	//printf("\n");
	ret = Parse(&ctx, argv[1], &pTree, genTAC, genJVM);
	printf("\n");
	
	if ( ret && pTree )
	{
		if ( genTAC )
		{
			//GenThreeAddressCode(pTree);
			tacGenerate(&ctx, pTree, &ctx.g_Scope, optLevel);
		}
		else if ( genJVM )
		{
			jvmStart(&ctx, argv[1], &pTree, &ctx.g_Scope, optLevel);
		}
		else
		{
			interpret(&ctx, pTree);
		}
	}

	//printf("\n");

	scopeFree(&ctx, &ctx.g_Scope);
	freeNode(&ctx, pTree);
	
	return 0;
}


/*
int main(int argc, char **argv)
{
	Token token;

	if ( argc < 2 )
	{
		printf("Specificare il nome di un file, prego\n");
		return -1;
	}

	if ( !LeggiFile(argv[1]) )
		return -1;

	printf("\n");

	token.Value.vString = NULL;
	GetNextToken(&token);
	while ( token.Type != T_EOF )
	{
		switch( token.Type )
		{
		case T_ERROR:
			break;
		case T_UNKNOWN:
			printf("carattere non riconosciuto: %c\n", token.Value.vChar);
			break;
		case T_ID:
			if ( token.Value.vString != NULL )
				printf("ID: %s\n", token.Value.vString);
			break;
		//case T_CHAR:
		//	printf("carattere: %c\n", token.Value.vChar);
		//	break;
		case T_OPAREN:
			printf("parentesi tonda aperta: (\n");
			break;
		case T_CPAREN:
			printf("parentesi tonda chiusa: )\n");
			break;
		case T_QOPAREN:
			printf("parentesi quadra aperta: [\n");
			break;
		case T_QCPAREN:
			printf("parentesi quadra chiusa: ]\n");
			break;
		case T_SEMICOLON:
			printf("punto e virgola: ;\n");
			break;
		case T_STRING_LITERAL:
			printf("letterale stringa: %s\n", token.Value.vString);
			break;
		case T_INT_LITERAL:
			printf("letterale intero: %s\n", token.lexeme);
			break;
		case T_REAL_LITERAL:
			printf("letterale reale: %s\n", token.lexeme);
			break;
		case T_COMMA:
			printf("virgola: ,\n");
			break;
		case T_KW_IF:
			printf("T_KW_IF: if\n");
			break;
		case T_KW_ELSE:
			printf("T_KW_ELSE: else\n");
			break;
		case T_KW_WHILE:
			printf("T_KW_WHILE: while\n");
			break;
		case T_KW_DO:
			printf("T_KW_DO: do\n");
			break;
		case T_KW_BEGIN:
			printf("T_KW_BEGIN: begin\n");
			break;
		case T_KW_END:
			printf("T_KW_END: end\n");
			break;
		case T_KW_ARRAY:
			printf("T_KW_ARRAY: array\n");
			break;
		case T_KW_OF:
			printf("T_KW_OF: of\n");
			break;
		case T_KW_STRING:
			printf("T_KW_STRING: string\n");
			break;
		case T_KW_INT:
			printf("T_KW_INT: int\n");
			break;
		case T_KW_REAL:
			printf("T_KW_REAL: real\n");
			break;
		case T_KW_BOOL:
			printf("T_KW_BOOL: bool\n");
			break;
		case T_KW_FALSE:
			printf("T_KW_FALSE: false\n");
			break;
		case T_KW_TRUE:
			printf("T_KW_TRUE: true\n");
			break;
		case T_KW_PRINT:
			printf("T_KW_PRINT: print\n");
			break;
		case T_KW_READ:
			printf("T_KW_READ: read\n");
			break;
		case T_OP_ASSIGN:
			printf("T_OP_ASSIGN: =\n");
			break;
		case T_OP_AND:
			printf("T_OP_AND: &\n");
			break;
		case T_OP_OR:
			printf("T_OP_OR: |\n");
			break;
		case T_OP_NOT:
			printf("T_OP_NOT: !\n");
			break;
		case T_OP_LT:
			printf("T_OP_LT: <\n");
			break;
		case T_OP_LE:
			printf("T_OP_LE: <=\n");
			break;
		case T_OP_EQ:
			printf("T_OP_EQ: ==\n");
			break;
		case T_OP_NE:
			printf("T_OP_NE: !=\n");
			break;
		case T_OP_GT:
			printf("T_OP_GT: >\n");
			break;
		case T_OP_GE:
			printf("T_OP_GE: >=\n");
			break;
		case T_OP_PLUS:
			printf("T_OP_PLUS: +\n");
			break;
		case T_OP_MINUS:
			printf("T_OP_MINUS: -\n");
			break;
		case T_OP_MULT:
			printf("T_OP_MULT: *\n");
			break;
		case T_OP_DIV:
			printf("T_OP_DIV: /\n");
			break;
		}

		GetNextToken(&token);
	}

	FreeInputbuffer();

	return 0;
}
*/
