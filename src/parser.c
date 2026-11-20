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
#include <math.h>
#include <string.h>
#include "context.h"
#include "parser.h"
#include "symtab.h"
#include "ast.h"
#include "jvm.h"

/*
program : decl_list code_block;
decl_list : {decl};
decl : type var_list ';'
		| T_KW_ARRAY T_ID '[' T_INT_LITERAL ']' {'[' T_INT_LITERAL ']'} T_KW_OF type ';'
		;
type : T_KW_INT | T_KW_REAL | T_KW_BOOL | T_KW_STRING;
var_list : T_ID ['=' expr] {',' T_ID['=' expr]};
code_block : T_BEGIN stmt_list T_END;
stmt_list : {stmt};
stmt : T_KW_PRINT expr ';'
		| T_KW_READ expr ';'
		| T_KW_WHILE '(' expr ')' stmt
		| T_KW_DO stmt T_KW_WHILE '(' expr ')'
		| T_KW_IF '(' expr ')' stmt [T_KW_ELSE stmt]
		| T_BEGIN stmt_list T_END
		| T_ID {'[' expr ']'} '=' expr ';'
		;
expr  : expr1 {('&' | '|') expr1};
expr1 : expr2 {relop expr2};
expr2 : expr3 {addop expr3};
expr3 : expr4 {mulop expr4};
expr4 : T_INT_LITERAL
		| T_REAL_LITERAL
		| T_STRING_LITERAL
		| T_KW_TRUE
		| T_KW_FALSE
		| '(' expr ')'
		| '-' expr
		| '!' expr
		| T_ID {'[' expr ']'}
		;
*/









/* Helper per creare una copia wrapper di ArrayDef per un nodo AST */
static int allocArrayDefWrapper(CompilerContext *ctx, const HashTable *pHT, Valore *pOutVal)
{
    ArrayDef *pArr = (ArrayDef*)malloc(sizeof(ArrayDef));
    if ( !pArr )
    {
        printf("Errore pArrayDef: memoria insufficiente.\n");
        ctx->g_numErrors++;
        return 0;
    }

    pArr->countIndexes = pHT->Value.pArrayDef->countIndexes;
    pArr->pArray       = pHT->Value.pArrayDef->pArray;
    pArr->sizes        = pHT->Value.pArrayDef->sizes;
    pArr->indexes      = NULL;

    if ( pArr->countIndexes > 0 )
    {
        pArr->indexes = (nodeType**)calloc((size_t)pArr->countIndexes, sizeof(nodeType*));
        if ( !pArr->indexes )
        {
            printf("Errore pArrayDef->indexes: memoria insufficiente.\n");
            ctx->g_numErrors++;
            free(pArr);
            return 0;
        }
    }

    pOutVal->pArrayDef = pArr;
    return 1;
}

/* Libera un wrapper ArrayDef (inclusi eventuali indici gia' assegnati) */
static void freeArrayDefWrapper(CompilerContext *ctx, Valore *pVal)
{
    int i;
    if ( !pVal || !pVal->pArrayDef )
        return;

    if ( pVal->pArrayDef->indexes )
    {
        for ( i = 0; i < pVal->pArrayDef->countIndexes; i++ )
        {
            if ( pVal->pArrayDef->indexes[i] )
            {
                freeNode(ctx, pVal->pArrayDef->indexes[i]);
                pVal->pArrayDef->indexes[i] = NULL;
            }
        }
        free(pVal->pArrayDef->indexes);
        pVal->pArrayDef->indexes = NULL;
    }

    free(pVal->pArrayDef);
    pVal->pArrayDef = NULL;
}

/* Rimuove dallo stack i nodi creati durante l'analisi degli indici array */
static void popArrayIndexNodes(CompilerContext *ctx, int count)
{
    while ( count > 0 && ctx->g_top >= 0 )
    {
        freeNode(ctx, ctx->g_stack[ctx->g_top--]);
        count--;
    }
}

// Token che possono iniziare una istruzione (FIRST(stmt)). 
static int isStmtStart(CompilerContext *ctx, int t)
{
	(void)ctx;
    return t == T_KW_PRINT || t == T_KW_READ || t == T_KW_WHILE ||
           t == T_KW_DO    || t == T_KW_IF   || t == T_KW_BEGIN ||
           t == T_ID;
}

/* Gestisce il ';' finale di uno statement: se manca ma il token corrente
   e' gia' l'inizio di una nuova istruzione o la fine di un blocco,
   recupera senza mangiare il token successivo. */
static int expectStmtSemicolon(CompilerContext *ctx)
{
    if ( match(ctx, T_SEMICOLON) )
        return 1;

    printf("Linea %d Colonna %d: Errore stmt. Atteso semicolon ';'.\n",
           ctx->g_Token.line, ctx->g_Token.column);
    ctx->g_numErrors++;

    if ( isStmtStart(ctx, ctx->g_Token.Type) ||
         ctx->g_Token.Type == T_KW_END ||
         ctx->g_Token.Type == T_KW_ELSE ||
         ctx->g_Token.Type == T_EOF )
    {
        return 1;
    }

    return 0;
}

/* Gestisce ')' in intestazioni come if/while: se manca ma siamo gia'
   su un possibile inizio di istruzione o 'begin', recupera senza
   perdere il corpo dell'istruzione. */
static int expectCondRParen(CompilerContext *ctx)
{
    if ( match(ctx, T_CPAREN) )
        return 1;

    printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ')'.\n",
           ctx->g_Token.line, ctx->g_Token.column);
    ctx->g_numErrors++;

    if ( isStmtStart(ctx, ctx->g_Token.Type) || ctx->g_Token.Type == T_EOF )
        return 1;

    return 0;
}

void Panic(CompilerContext *ctx)
{
	ctx->g_numErrors++;

	while ( ctx->g_Token.Type != T_SEMICOLON && ctx->g_Token.Type != T_KW_END && ctx->g_Token.Type != T_EOF )
	{
		GetNextToken(ctx, &ctx->g_Token);
	}

	if ( ctx->g_Token.Type == T_SEMICOLON || ctx->g_Token.Type == T_KW_END )
	{
		GetNextToken(ctx, &ctx->g_Token);
		SetDepth(ctx, 0);
	}
}

int Parse(CompilerContext *ctx, char *szFileName, nodeType **pTree, int genTAC, int genJVM)
{
    int ret;

    ctx->g_genTAC = genTAC;
    ctx->g_genJVM = genJVM;
    ctx->g_Token.Value.vString = NULL;

    if ( !LeggiFile(ctx, szFileName) )
        return 0;

    GetNextToken(ctx, &ctx->g_Token);

    /* Un programma = dichiarazioni + un solo blocco begin...end. */
    ret = program(ctx);

    /* Parsing riuscito ma token residui: codice fuori dal programma. */
    if ( ret && ctx->g_Token.Type != T_EOF )
    {
        printf("Linea %d Colonna %d: Errore. Token inattesi dopo la fine del programma.\n",
               ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        ret = 0;
    }

    /* Svuota il buffer fino a EOF (mai oltre: dopo EOF lo scanner
       restituirebbe T_ERROR all'infinito). */
    while ( ctx->g_Token.Type != T_EOF )
        GetNextToken(ctx, &ctx->g_Token);

    FreeInputbuffer(ctx);

    /* Se program() ha fallito, garantisce almeno un errore conteggiato:
       evita ret=1 con stack vuoto (lettura di g_stack[-1]). */
    if ( !ret && ctx->g_numErrors == 0 )
        ctx->g_numErrors++;

    ret = ctx->g_numErrors ? 0 : 1;

    if ( ctx->g_numErrors || ctx->g_numWarnings )
        printf("Errori: %d\nWarnings: %d\n", ctx->g_numErrors, ctx->g_numWarnings);

    if ( !ret )
    {
        (*pTree) = NULL;
        while ( ctx->g_top >= 0 )
        {
            freeNode(ctx, ctx->g_stack[ctx->g_top]);   // freeNode(NULL) e' sicuro 
            ctx->g_top--;                    // decremento SEMPRE, anche se NULL 
        }
    }
    else
    {
        (*pTree) = ctx->g_stack[ctx->g_top];
    }
    
    return ret;
}

int match(CompilerContext *ctx, TokenTypeEnum ExpectedToken)
{
	if ( ctx->g_Token.Type == ExpectedToken )
	{
		GetNextToken(ctx, &ctx->g_Token);
		return 1;
	}
	//else
	//{
	//	printf("Errore di sintassi\n");
	//	exit(1);
	//}

	//printf("Linea %d Colonna %d: Errore di sintassi\n", g_Token.line, g_Token.column);

	return 0;
}

int TypeCheck(CompilerContext *ctx, Valore *value, const Token *pToken)
{
	if ( ctx->g_currType == ctx->g_currConstType )
	{
		(*value) = ctx->g_currConstValue;

		//if ( g_currType == T_BOOLEAN )
		//{
		//	printf("Valore   -> %d\n", g_currConstValue.bVal);
		//	printf("Valore 2 -> %d\n", g_currConstValue.iVal);
		//}
	}
	else
	{
		switch ( ctx->g_currType )
		{
		case T_INTEGER:
			if ( ctx->g_currConstType == T_REAL )
			{
				printf("Linea %d Colonna %d: "
						"Warning var_list. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' "
						"di tipo int. Potrebbe verificarsi una perdita dei dati.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				ctx->g_numWarnings++;
				(*value).iVal = (int)ctx->g_currConstValue.dblVal;
			}
			else if ( ctx->g_currConstType == T_BOOLEAN )
			{
				if ( ctx->g_currConstValue.bVal )
					(*value).iVal = 1;
				else
					(*value).iVal = 0;
			}
			else if ( ctx->g_currConstType == T_STRING )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"si sta assegnando un valore di tipo string alla variabile '%s' "
						"di tipo int.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		case T_REAL:
			if ( ctx->g_currConstType == T_INTEGER )
			{
				(*value).dblVal = (double)ctx->g_currConstValue.iVal;
			}
			else if ( ctx->g_currConstType == T_BOOLEAN )
			{
				if ( ctx->g_currConstValue.bVal )
					(*value).dblVal = 1.0;
					//(*value).iVal = 1;
				else
					//(*value).iVal = 0;
					(*value).dblVal = 0.0;
			}
			else if ( ctx->g_currConstType == T_STRING )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"si sta assegnando un valore di tipo string alla variabile '%s' "
						"di tipo real.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		case T_BOOLEAN:
			if ( ctx->g_currConstType == T_INTEGER )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"Si sta assegnando un valore di tipo int alla variabile '%s' "
						"di tipo bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_REAL )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' "
						"di tipo bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_STRING )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"si sta assegnando un valore di tipo string alla variabile '%s' "
						"di tipo bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		case T_STRING:
			if ( ctx->g_currConstType == T_INTEGER )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"Si sta assegnando un valore di tipo int alla variabile '%s' "
						"di tipo string.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_REAL )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' "
						"di tipo string.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: "
						"Errore var_list. "
						"si sta assegnando un valore di tipo bool alla variabile '%s' "
						"di tipo string.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		}
	}

	return 1;
}


// program : decl_list code_block;
int program(CompilerContext *ctx)
{
	if ( !decl_list(ctx) )
		return 0;

	if ( !code_block(ctx) )
		return 0;

	return 1;
}

//decl_list : {decl};
int decl_list(CompilerContext *ctx)
{
	while ( ctx->g_Token.Type == T_KW_ARRAY ||
		ctx->g_Token.Type == T_KW_INT ||
		ctx->g_Token.Type == T_KW_REAL ||
		ctx->g_Token.Type == T_KW_BOOL ||
		ctx->g_Token.Type == T_KW_STRING )
		if ( !decl(ctx) )
			return 0;

	return 1;
}

//type : T_KW_INT | T_KW_REAL | T_KW_BOOL | T_KW_STRING;
int type(CompilerContext *ctx)
{
	switch ( ctx->g_Token.Type )
	{
	case T_KW_INT:
		ctx->g_currType = T_INTEGER;
		GetNextToken(ctx, &ctx->g_Token);
		return 1;
	case T_KW_REAL:
		ctx->g_currType = T_REAL;
		GetNextToken(ctx, &ctx->g_Token);
		return 1;
	case T_KW_BOOL:
		ctx->g_currType = T_BOOLEAN;
		GetNextToken(ctx, &ctx->g_Token);
		return 1;
	case T_KW_STRING:
		ctx->g_currType = T_STRING;
		GetNextToken(ctx, &ctx->g_Token);
		return 1;
	default:
		printf("Linea %d Colonna %d: Errore type. Attesa keyword 'int' o 'real' o 'bool' o 'string'.\n",
		       ctx->g_Token.line, ctx->g_Token.column);
		ctx->g_numErrors++;
		return 0;
	}
}

//decl : type var_list ';'
//		| T_KW_ARRAY T_ID '[' T_INT_LITERAL ']' {'[' T_INT_LITERAL ']'} T_KW_OF type ';'
//		;
int decl(CompilerContext *ctx)
{
	if ( ctx->g_Token.Type == T_KW_ARRAY )
	{
		Valore value;
		Token token;
		HashTable hashTable;

		char szIdName[256] = "";

		int indici[89];
		int currIndex = 0;
		int array_size = 0;

		value.pArrayDef = (ArrayDef*)malloc(sizeof(ArrayDef));
		if ( !value.pArrayDef )
		{
			printf("Errore: memoria insufficiente.\n");
			ctx->g_numErrors++;
			return 0;
		}

		value.pArrayDef->countIndexes = 0;
		value.pArrayDef->pArray = NULL;
		value.pArrayDef->sizes = NULL;
		value.pArrayDef->indexes = NULL;

		GetNextToken(ctx, &ctx->g_Token);

		token = ctx->g_Token;
		if ( ctx->g_Token.Type == T_ID )
		{
			if ( strlen(ctx->g_Token.Value.vString) >= sizeof(szIdName) )
			{
				printf("Linea %d Colonna %d: Errore decl. Identificatore troppo lungo.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
			strcpy(szIdName, ctx->g_Token.Value.vString);
		}
		token.Value.vString = szIdName;

		if ( !match(ctx, T_ID) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso identificatore.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		if ( scopeFind(ctx, &ctx->g_Scope, token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
					token.line, token.column, token.Value.vString);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( !match(ctx, T_QOPAREN) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa parentesi '['.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( ctx->g_Token.Type == T_INT_LITERAL )
		{
			if ( ctx->g_Token.Value.vInt <= 0 )
			{
				printf("Linea %d Colonna %d: Errore decl. La dimensione dell'array deve essere positiva.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
			indici[value.pArrayDef->countIndexes++] = ctx->g_Token.Value.vInt;
			array_size = ctx->g_Token.Value.vInt;
		}

		if ( !match(ctx, T_INT_LITERAL) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso int literal.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		if ( !match(ctx, T_QCPAREN) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa parentesi ']'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		while ( ctx->g_Token.Type == T_QOPAREN )
		{
			if ( !match(ctx, T_QOPAREN) )
			{
				printf("Linea %d Colonna %d: Errore decl. Attesa parentesi '['.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}

			if ( ctx->g_Token.Type == T_INT_LITERAL )
			{
				if ( value.pArrayDef->countIndexes >= 89 )
				{
					printf("Linea %d Colonna %d: Errore decl. Troppe dimensioni per l'array.\n",
							ctx->g_Token.line, ctx->g_Token.column);
					ctx->g_numErrors++;
					free(value.pArrayDef);
					return 0;
				}
				if ( ctx->g_Token.Value.vInt <= 0 )
				{
					printf("Linea %d Colonna %d: Errore decl. La dimensione dell'array deve essere positiva.\n",
							ctx->g_Token.line, ctx->g_Token.column);
					ctx->g_numErrors++;
					free(value.pArrayDef);
					return 0;
				}
				indici[value.pArrayDef->countIndexes++] = ctx->g_Token.Value.vInt;
				array_size *= ctx->g_Token.Value.vInt;
			}

			if ( !match(ctx, T_INT_LITERAL) )
			{
				printf("Linea %d Colonna %d: Errore decl. Atteso int literal.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore decl. Attesa parentesi ']'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
		}
		if ( !match(ctx, T_KW_OF) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'of'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		if ( !type(ctx) )
		{
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( array_size <= 0 )
		{
			printf("Linea %d Colonna %d: Errore decl. Dimensione totale array non valida.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( ctx->g_currType == T_INTEGER || ctx->g_currType == T_BOOLEAN )
		{
			value.pArrayDef->pArray = (int*)calloc((size_t)array_size, sizeof(int));
		}
		else if ( ctx->g_currType == T_REAL )
		{
			value.pArrayDef->pArray = (double*)calloc((size_t)array_size, sizeof(double));
		}
		else if ( ctx->g_currType == T_STRING )
		{
			value.pArrayDef->pArray = (char**)calloc((size_t)array_size, sizeof(char*));
		}

		if ( !value.pArrayDef->pArray )
		{
			printf("Errore pArrayDef->pArray: memoria insufficiente.\n");
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( !match(ctx, T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso semicolon ';'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef->pArray);
			free(value.pArrayDef);
			return 0;
		}

		value.pArrayDef->sizes = (int*)calloc((size_t)value.pArrayDef->countIndexes, sizeof(int));
		if ( !value.pArrayDef->sizes )
		{
			printf("Errore pArrayDef->sizes: memoria insufficiente.\n");
			ctx->g_numErrors++;
			free(value.pArrayDef->pArray);
			free(value.pArrayDef);
			return 0;
		}

		for ( currIndex = 0; currIndex < value.pArrayDef->countIndexes; currIndex++ )
			value.pArrayDef->sizes[currIndex] = indici[currIndex];

		value.pArrayDef->indexes = NULL;
		scopeInsert(ctx, &ctx->g_Scope, token.Value.vString, T_ARRAY, ctx->g_currType, T_VAR, value, 0, ctx->g_NextFrameIndex);
		if ( ctx->g_genJVM )
		{
			ctx->g_NextFrameIndex++;
			jvmSetNumberLocals(ctx, ctx->g_NextFrameIndex);
		}
	}
	else
	{
		if ( !type(ctx) )
		{
			ctx->g_numErrors++;
			return 0;
		}
		if ( !var_list(ctx) )
			return 0;
		if ( !match(ctx, T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso semicolon ';'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			return 0;
		}
	}

	return 1;
}

static void reset_var_value(CompilerContext *ctx, Valore *val, enumTipo1 tipo)
{
	(void)ctx;
	memset(val, 0, sizeof(*val));
	if ( tipo == T_INTEGER )
		val->iVal = 0;
	else if ( tipo == T_REAL )
		val->dblVal = 0.0;
	else if ( tipo == T_STRING )
		val->strVal = NULL;
	else
		val->bVal = 0;
}

static int parse_var_item(CompilerContext *ctx)
{
	Token token = ctx->g_Token;
	Valore value;
	HashTable hashTable;
	char szVarName[256] = "";
	char szLexeme[256] = "";

	reset_var_value(ctx, &value, (enumTipo1)ctx->g_currType);

	if ( ctx->g_Token.Type == T_ID && ctx->g_Token.Value.vString != NULL )
	{
		snprintf(szVarName, sizeof(szVarName), "%s", ctx->g_Token.Value.vString);
	}
	token.Value.vString = szVarName;

	if ( !match(ctx, T_ID) )
	{
		printf("Linea %d Colonna %d: Errore var_list. Atteso identificatore.\n",
		       ctx->g_Token.line, ctx->g_Token.column);
		ctx->g_numErrors++;
		return 0;
	}

	if ( scopeFind(ctx, &ctx->g_Scope, token.Value.vString, &hashTable) )
	{
		printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
		       token.line, token.column, token.Value.vString);
		ctx->g_numErrors++;
		return 0;
	}

	scopeInsert(ctx, &ctx->g_Scope, token.Value.vString, T_SIMPLE, (enumTipo1)ctx->g_currType,
	            T_VAR, value, 0, ctx->g_NextFrameIndex);
	if ( ctx->g_genJVM )
	{
		if ( ctx->g_currType == T_REAL )
			ctx->g_NextFrameIndex += 2;
		else
			ctx->g_NextFrameIndex++;
		jvmSetNumberLocals(ctx, ctx->g_NextFrameIndex);
	}

	if ( ctx->g_Token.Type == T_OP_ASSIGN )
	{
		GetNextToken(ctx, &ctx->g_Token);
		snprintf(szLexeme, sizeof(szLexeme), "%s", ctx->g_Token.lexeme);

		if ( !expr(ctx) )
		{
			ctx->g_numErrors++;
			return 0;
		}

		if ( ctx->g_currConstType != -1 )
		{
			if ( !TypeCheck(ctx, &value, &token) )
			{
				ctx->g_numErrors++;
				return 0;
			}

			scopeUpdateValue(ctx, &ctx->g_Scope, szVarName, T_SIMPLE, (enumTipo1)ctx->g_currType,
			                 T_VAR, value, 1, 1);

			if ( ctx->g_genTAC )
			{
				printf("%s = %s\n", szVarName, szLexeme);
			}
		}
		else
		{
			printf("Linea %d Colonna %d: Errore var_list. In fase di dichiarazione è possibile assegnare solo valori costanti.\n",
			       token.line, token.column);
			ctx->g_numErrors++;
			return 0;
		}
	}

	return 1;
}

//var_list : T_ID ['=' expr] {',' T_ID['=' expr]};
int var_list(CompilerContext *ctx)
{
	if ( !parse_var_item(ctx) )
	{
		while ( ctx->g_Token.Type != T_EOF && ctx->g_Token.Type != T_COMMA && ctx->g_Token.Type != T_SEMICOLON )
			GetNextToken(ctx, &ctx->g_Token);
		if ( ctx->g_Token.Type != T_COMMA )
			return 0;
	}

	while ( ctx->g_Token.Type == T_COMMA )
	{
		GetNextToken(ctx, &ctx->g_Token);
		if ( !parse_var_item(ctx) )
		{
			while ( ctx->g_Token.Type != T_EOF && ctx->g_Token.Type != T_COMMA && ctx->g_Token.Type != T_SEMICOLON )
				GetNextToken(ctx, &ctx->g_Token);
			if ( ctx->g_Token.Type != T_COMMA )
				return 0;
		}
	}

	return 1;
}

//code_block : T_KW_BEGIN stmt_list T_KW_END;
int code_block(CompilerContext *ctx)
{
    nodeType *left = NULL;

    ctx->g_isCodeBlock = 1;

    if ( !match(ctx, T_KW_BEGIN) )
    {
        printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'begin'.\n",
               ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        return 0;
    }

    stmt_list(ctx);   /* gestisce i propri errori e ritorna sempre 1 */

    if ( !match(ctx, T_KW_END) )
    {
        printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'end'.\n",
               ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        return 0;
    }

    left = ctx->g_stack[ctx->g_top--];
    ctx->g_stack[++ctx->g_top] = opr(ctx, T_CODE_BLOCK, 1, left);

    return 1;
}

//stmt_list : {stmt};
int stmt_list(CompilerContext *ctx)
{
    nodeType *left = NULL, *right = NULL, *root = NULL;
    int listDepth;
    int startLine, startColumn;
 
    TokenTypeEnum prevTokType;

    ctx->g_isStmtList = 1;

    root = opr(ctx, T_STMT_LIST, 2, NULL, NULL);
    ctx->g_stack[++ctx->g_top] = root;
    listDepth = ctx->g_top;

    while ( isStmtStart(ctx, ctx->g_Token.Type) )
    {
        startLine   = ctx->g_Token.line;
        startColumn = ctx->g_Token.column;
        prevTokType = ctx->g_Token.Type;

        if ( !stmt(ctx) )
        {
            while ( ctx->g_top > listDepth )
                freeNode(ctx, ctx->g_stack[ctx->g_top--]);

            if ( ctx->g_Token.Type == T_KW_END ||
                 ctx->g_Token.Type == T_KW_ELSE ||
                 ctx->g_Token.Type == T_EOF )
                break;

            /* Se stmt() non ha consumato nulla (es. identificatore non dichiarato
               o atteso '='), consumiamo il token errato se non e' gia' un nuovo
               stmtStart diverso da quello iniziale. */
            if ( ctx->g_Token.line == startLine && ctx->g_Token.column == startColumn )
                GetNextToken(ctx, &ctx->g_Token);

            while ( ctx->g_Token.Type != T_SEMICOLON &&
                    ctx->g_Token.Type != T_KW_END &&
                    ctx->g_Token.Type != T_KW_ELSE &&
                    ctx->g_Token.Type != T_EOF &&
                    !(isStmtStart(ctx, ctx->g_Token.Type) && ctx->g_Token.Type != prevTokType) )
            {
                GetNextToken(ctx, &ctx->g_Token);
            }

            if ( ctx->g_Token.Type == T_SEMICOLON )
                GetNextToken(ctx, &ctx->g_Token);

            if ( ctx->g_Token.Type == T_KW_END ||
                 ctx->g_Token.Type == T_KW_ELSE ||
                 ctx->g_Token.Type == T_EOF )
                break;
        }
        else
        {
            if ( ctx->g_top >= listDepth + 1 )
            {
                right = ctx->g_stack[ctx->g_top--];
                left  = ctx->g_stack[ctx->g_top--];
                ctx->g_stack[++ctx->g_top] = opr(ctx, T_STMT_LIST, 2, left, right);
            }
        }
    }

    ctx->g_isStmtList = 0;
    return 1;
}

//stmt : T_KW_PRINT expr ';'
//		| T_KW_READ expr ';'
//		| T_KW_WHILE '(' expr ')' stmt
//		| T_KW_DO stmt T_KW_WHILE '(' expr ')'
//		| T_KW_IF '(' expr ')' stmt [T_KW_ELSE stmt]
//		| T_KW_BEGIN stmt_list T_END
//		| T_ID {'[' expr ']'} '=' expr ';'
//		;
int stmt(CompilerContext *ctx)
{
	nodeType *left, *right, *elseNode;
	HashTable hashTable;
	Valore value;
	char szIdName[256];
	int line, column;
	int currArrayIndex;

	left = right = elseNode = NULL;

	switch ( ctx->g_Token.Type )
	{
	case T_KW_PRINT:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( !expectStmtSemicolon(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		left = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_PRINT, 1, left);
		break;

	case T_KW_READ:
		GetNextToken(ctx, &ctx->g_Token);
		ctx->g_isREAD = 1;
		if ( !expr(ctx) )
		{
			ctx->g_isREAD = 0;
			return 0;
		}
		ctx->g_isREAD = 0;
		if ( ctx->g_top < 0 || ctx->g_stack[ctx->g_top]->type != typeId )
		{
			printf("Linea %d Colonna %d: Errore stmt. 'read' richiede un identificatore o elemento di array.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !expectStmtSemicolon(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		left = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_READ, 1, left);
		break;

	case T_KW_WHILE:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !match(ctx, T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			return 0;
		}
		if ( !expr(ctx) )
			return 0;
		if ( !expectCondRParen(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( ctx->g_top < 0 || ctx->g_stack[ctx->g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !stmt(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		right = ctx->g_stack[ctx->g_top--];
		left = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_WHILE, 2, left, right);
		break;

	case T_KW_DO:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !stmt(ctx) )
			return 0;
		if ( !match(ctx, T_KW_WHILE) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa keyword 'while'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !match(ctx, T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !expr(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !expectCondRParen(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		//if ( g_top < 1 || g_stack[g_top]->type1 == T_STRING )
		if ( ctx->g_top < 0 || ctx->g_stack[ctx->g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 ) freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			if ( ctx->g_top >= 0 ) freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		right = ctx->g_stack[ctx->g_top--];
		left = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_DO, 2, left, right);
		break;

	case T_KW_IF:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !match(ctx, T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			return 0;
		}
		if ( !expr(ctx) )
			return 0;
		if ( !expectCondRParen(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( ctx->g_top < 0 || ctx->g_stack[ctx->g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !stmt(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		elseNode = NULL;
		if ( ctx->g_Token.Type == T_KW_ELSE )
		{
			GetNextToken(ctx, &ctx->g_Token);
			if ( !stmt(ctx) )
			{
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
				return 0;
			}
			elseNode = ctx->g_stack[ctx->g_top--];
		}
		right = ctx->g_stack[ctx->g_top--];
		left = ctx->g_stack[ctx->g_top--];
		if ( elseNode )
			ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_IF, 3, left, right, elseNode);
		else
			ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_IF, 2, left, right);
		break;

	case T_KW_BEGIN:
		GetNextToken(ctx, &ctx->g_Token);
		stmt_list(ctx);
		if ( !match(ctx, T_KW_END) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa keyword 'end'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		left = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_STMT_LIST, 1, left);
		break;

	case T_ID:
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Identificatore '%s' non dichiarato.\n",
					ctx->g_Token.line, ctx->g_Token.column, ctx->g_Token.Value.vString);
			ctx->g_numErrors++;
			return 0;
		}
		if ( strlen(ctx->g_Token.Value.vString) >= sizeof(szIdName) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Identificatore troppo lungo.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			return 0;
		}
		strcpy(szIdName, ctx->g_Token.Value.vString);

		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		GetNextToken(ctx, &ctx->g_Token);

		if ( hashTable.Tipo0 == T_SIMPLE && ctx->g_Token.Type == T_QOPAREN )
		{
			printf("Linea %d Colonna %d: Errore stmt. L'identificatore '%s' non e' un array. Parentesi quadra non valida.\n",
					line, column, szIdName);
			ctx->g_numErrors++;
			return 0;
		}
		else if ( hashTable.Tipo0 == T_ARRAY && ctx->g_Token.Type != T_QOPAREN )
		{
			printf("Linea %d Colonna %d: Errore stmt. L'identificatore '%s' e' un array. Attesa parentesi quadra.\n",
					line, column, szIdName);
			ctx->g_numErrors++;
			return 0;
		}

		if ( hashTable.Tipo0 == T_ARRAY )
		{
			if ( !allocArrayDefWrapper(ctx, &hashTable, &value) )
				return 0;
		}
		else
		{
			value = hashTable.Value;
		}

		currArrayIndex = 0;
		while ( ctx->g_Token.Type == T_QOPAREN )
		{
			if ( currArrayIndex >= hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				popArrayIndexNodes(ctx, currArrayIndex);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}

			GetNextToken(ctx, &ctx->g_Token);
			if ( !expr(ctx) )
			{
				popArrayIndexNodes(ctx, currArrayIndex);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}

			if ( ctx->g_top < 0 || ctx->g_stack[ctx->g_top]->type1 != T_INTEGER )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Gli array vanno indicizzati con espressioni di tipo int.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				popArrayIndexNodes(ctx, currArrayIndex + 1);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}
			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ']'.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				popArrayIndexNodes(ctx, currArrayIndex + 1);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}
			currArrayIndex++;
		}

		if ( hashTable.Tipo0 == T_ARRAY )
		{
			if ( currArrayIndex < hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				popArrayIndexNodes(ctx, currArrayIndex);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}

			for ( currArrayIndex = hashTable.Value.pArrayDef->countIndexes - 1; currArrayIndex >= 0 ; currArrayIndex-- )
			{
				value.pArrayDef->indexes[currArrayIndex] = ctx->g_stack[ctx->g_top--];
			}
		}

		if ( !match(ctx, T_OP_ASSIGN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Atteso operatore '='.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( hashTable.Tipo0 == T_ARRAY )
				freeArrayDefWrapper(ctx, &value);
			return 0;
		}
		if ( !expr(ctx) )
		{
			if ( hashTable.Tipo0 == T_ARRAY )
				freeArrayDefWrapper(ctx, &value);
			return 0;
		}
		if ( !expectStmtSemicolon(ctx) )
		{
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			if ( hashTable.Tipo0 == T_ARRAY )
				freeArrayDefWrapper(ctx, &value);
			return 0;
		}

		right = ctx->g_stack[ctx->g_top--];
		left = id(ctx, szIdName, "a", hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, 1);

		if ( left->type1 != right->type1 )
		{
			if ( left->type1 == T_INTEGER && right->type1 == T_REAL )
			{
				printf("Linea %d Colonna %d: Warning stmt. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' di tipo int. "
						"Potrebbe verificarsi una perdita di dati.\n",
						line, column, szIdName);
				ctx->g_numWarnings++;
			}
			else
			{
				if ( left->type1 == T_INTEGER && right->type1 == T_BOOLEAN )
					printf("Linea %d Colonna %d: Errore stmt. "
							"Non si puo' assegnare una valore booleano ad una variabile di tipo int.\n",
							line, column);
				else if ( left->type1 == T_INTEGER && right->type1 == T_STRING )
					printf("Linea %d Colonna %d: Errore stmt. "
							"Non si puo' assegnare una valore di tipo string ad una variabile di tipo int.\n",
							line, column);
				else if ( left->type1 == T_REAL && right->type1 == T_BOOLEAN )
					printf("Linea %d Colonna %d: Errore stmt. "
							"Non si puo' assegnare una valore booleano ad una variabile di tipo real.\n",
							line, column);
				else if ( left->type1 == T_REAL && right->type1 == T_STRING )
					printf("Linea %d Colonna %d: Errore stmt. "
							"Non si puo' assegnare una valore di tipo string ad una variabile di tipo real.\n",
							line, column);
				else if ( left->type1 == T_BOOLEAN && right->type1 == T_STRING )
					printf("Linea %d Colonna %d: Errore stmt. "
							"Non si puo' assegnare una valore di tipo string ad una variabile di tipo bool.\n",
							line, column);
				else
					printf("Linea %d Colonna %d: Errore stmt. "
							"Alle variabili di tipo string e' possibile assegnare soltanto valori di tipo string.\n",
							line, column);

				ctx->g_numErrors++;
				freeNode(ctx, left);
				freeNode(ctx, right);
				return 0;
			}
		}

		if ( hashTable.Tipo0 != T_ARRAY )
			scopeUpdateValue(ctx, &ctx->g_Scope, szIdName, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, hashTable.Value, 1, -1);

		ctx->g_stack[++ctx->g_top] = opr(ctx, T_OP_ASSIGN, 2, left, right);
		break;

	default:
		printf("Linea %d Colonna %d: Errore stmt. Attesa 'print'."
				" o 'read' o 'while' o 'do' o 'if' o 'begin' o identificatore.\n",
				ctx->g_Token.line, ctx->g_Token.column);
		ctx->g_numErrors++;
		return 0;
	}

	return 1;
}

//expr  : expr1 {('&' | '|') expr1};
int expr(CompilerContext *ctx)
{
	ctx->g_currConstType = -1;

	if ( !expr1(ctx) )
		return 0;

	while ( ctx->g_Token.Type == T_OP_AND || ctx->g_Token.Type == T_OP_OR )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		oper = ctx->g_Token.Type;

		GetNextToken(ctx, &ctx->g_Token);

		if ( !expr1(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}

		right = ctx->g_stack[ctx->g_top--];
		left  = ctx->g_stack[ctx->g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr. Gli operatori booleani "
					"non accettano operandi di tipo stringa.\n",
					line, column);
			ctx->g_numErrors++;
			freeNode(ctx, left);
			freeNode(ctx, right);
			return 0;
		}
		ctx->g_stack[++ctx->g_top] = opr(ctx, oper, 2, left, right);
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			ctx->g_stack[ctx->g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			ctx->g_stack[ctx->g_top]->type1 = T_INTEGER;
		else
			ctx->g_stack[ctx->g_top]->type1 = T_BOOLEAN;
	}

	return 1;
}

//expr1 : expr2 {relop expr2};
int expr1(CompilerContext *ctx)
{
	if ( !expr2(ctx) )
		return 0;

	while ( ctx->g_Token.Type == T_OP_LT || ctx->g_Token.Type == T_OP_LE ||
			ctx->g_Token.Type == T_OP_EQ || ctx->g_Token.Type == T_OP_NE ||
			ctx->g_Token.Type == T_OP_GT || ctx->g_Token.Type == T_OP_GE )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		oper = ctx->g_Token.Type;

		GetNextToken(ctx, &ctx->g_Token);

		if ( !expr2(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}

		right = ctx->g_stack[ctx->g_top--];
		left  = ctx->g_stack[ctx->g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr1. Gli operatori di confronto "
					"non accettano espressioni di tipo stringa.\n",
					line, column);
			ctx->g_numErrors++;
			freeNode(ctx, left);
			freeNode(ctx, right);
			return 0;
		}
		ctx->g_stack[++ctx->g_top] = opr(ctx, oper, 2, left, right);
		ctx->g_stack[ctx->g_top]->type0 = T_SIMPLE;
		ctx->g_stack[ctx->g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			ctx->g_stack[ctx->g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			ctx->g_stack[ctx->g_top]->type1 = T_INTEGER;
		else
			ctx->g_stack[ctx->g_top]->type1 = T_BOOLEAN;
	}

	return 1;
}

//expr2 : expr3 {addop expr3};
int expr2(CompilerContext *ctx)
{
	if ( !expr3(ctx) )
		return 0;

	while ( ctx->g_Token.Type == T_OP_PLUS || ctx->g_Token.Type == T_OP_MINUS )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		oper = ctx->g_Token.Type;

		GetNextToken(ctx, &ctx->g_Token);

		if ( !expr3(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}

		right = ctx->g_stack[ctx->g_top--];
		left  = ctx->g_stack[ctx->g_top--];

		if ( oper == T_OP_MINUS )
		{
			if ( left->type1 == T_STRING || right->type1 == T_STRING ||
				left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore expr2. L'operatore di sottrazione "
						"accetta solo operandi di tipo integer o real.\n",
						line, column);
				ctx->g_numErrors++;
				freeNode(ctx, left);
				freeNode(ctx, right);
				return 0;
			}
		}
		else // T_PLUS
		{
			if ( (left->type1 == T_STRING && right->type1 != T_STRING) ||
				 (left->type1 != T_STRING && right->type1 == T_STRING) )
			{
				printf("Linea %d Colonna %d: Errore expr2. Gli operandi dell'operatore di addizione "
						"devono essere o entrambi di tipo numerico o entrambi di tipo stringa.\n",
						line, column);
				ctx->g_numErrors++;
				freeNode(ctx, left);
				freeNode(ctx, right);
				return 0;
			}
			else if ( left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore expr2. L'operatore di addizione "
						"non accetta operandi di tipo boolean.\n",
						line, column);
				ctx->g_numErrors++;
				freeNode(ctx, left);
				freeNode(ctx, right);
				return 0;
			}
		}
		ctx->g_stack[++ctx->g_top] = opr(ctx, oper, 2, left, right);
		ctx->g_stack[ctx->g_top]->type0 = T_SIMPLE;
		ctx->g_stack[ctx->g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			ctx->g_stack[ctx->g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			ctx->g_stack[ctx->g_top]->type1 = T_INTEGER;
		else
			ctx->g_stack[ctx->g_top]->type1 = T_STRING;
	}

	return 1;
}

//expr3 : expr4 {mulop expr4};
int expr3(CompilerContext *ctx)
{
	if ( !expr4(ctx) )
		return 0;

	while ( ctx->g_Token.Type == T_OP_MULT || ctx->g_Token.Type == T_OP_DIV )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		oper = ctx->g_Token.Type;

		GetNextToken(ctx, &ctx->g_Token);

		if ( !expr4(ctx) )
		{
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}

		right = ctx->g_stack[ctx->g_top--];
		left  = ctx->g_stack[ctx->g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING ||
			left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
		{
			printf("Linea %d Colonna %d: Errore expr3. Gli operatori di moltiplicazione/divisione "
					"accettano solo operandi di tipo integer o real.\n",
					line, column);
			ctx->g_numErrors++;
			freeNode(ctx, left);
			freeNode(ctx, right);
			return 0;
		}
		ctx->g_stack[++ctx->g_top] = opr(ctx, oper, 2, left, right);
		ctx->g_stack[ctx->g_top]->type0 = T_SIMPLE;
		ctx->g_stack[ctx->g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			ctx->g_stack[ctx->g_top]->type1 = T_REAL;
		else
			ctx->g_stack[ctx->g_top]->type1 = T_INTEGER;
	}

	return 1;
}

static void cleanupArrayExprError(CompilerContext *ctx, int currArrayIndex, Valore *val, nodeType *pNode)
{
	int i = currArrayIndex - 1;

	while ( i >= 0 )
	{
		if ( ctx->g_top >= 0 )
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
		i--;
	}

	if ( val != NULL && val->pArrayDef != NULL )
	{
		free(val->pArrayDef->indexes);
		val->pArrayDef->indexes = NULL;
		free(val->pArrayDef);
		val->pArrayDef = NULL;
	}

	if ( pNode != NULL )
		freeNode(ctx, pNode);
}

//expr4 : T_INT_LITERAL
//		| T_REAL_LITERAL
//		| T_STRING_LITERAL
//		| T_KW_TRUE
//		| T_KW_FALSE
//		| '(' expr ')'
//		| '-' expr
//		| '!' expr
//		| T_ID {'[' expr ']'}
//		;
int expr4(CompilerContext *ctx)
{
	HashTable hashTable;
	Valore value;
	nodeType *pNode = NULL;
	int currArrayIndex;

	switch( ctx->g_Token.Type )
	{
	case T_INT_LITERAL:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.iVal = ctx->g_Token.Value.vInt;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_INTEGER, ctx->g_Token.lexeme, value);
		}
		else
		{
			ctx->g_currConstType = T_INTEGER;
			ctx->g_currConstValue.iVal = ctx->g_Token.Value.vInt;
		}

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_REAL_LITERAL:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.dblVal = ctx->g_Token.Value.vDouble;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_REAL, ctx->g_Token.lexeme, value);
		}
		else
		{
			ctx->g_currConstType = T_REAL;
			ctx->g_currConstValue.dblVal = ctx->g_Token.Value.vDouble;
		}

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_STRING_LITERAL:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.strVal = (char*)malloc(sizeof(char)*strlen(ctx->g_Token.Value.vString)+1);
			if ( !value.strVal )
			{
				printf("Errore: memoria non sufficiente.\n");
				return 0;
			}
			strcpy(value.strVal, ctx->g_Token.Value.vString);
			ctx->g_stack[++ctx->g_top] = con(ctx, T_STRING, NULL, value);
		}
		else
		{
			ctx->g_currConstType = T_STRING;
			ctx->g_currConstValue.strVal = (char*)malloc(sizeof(char)*strlen(ctx->g_Token.Value.vString)+1);
			if ( !ctx->g_currConstValue.strVal )
			{
				printf("Errore: memoria non sufficiente.\n");
				return 0;
			}
			strcpy(ctx->g_currConstValue.strVal, ctx->g_Token.Value.vString);
		}

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_KW_TRUE:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.bVal = 1;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_BOOLEAN, "true", value);
		}
		else
		{
			ctx->g_currConstType = T_BOOLEAN;
			ctx->g_currConstValue.bVal = 1;
		}

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_KW_FALSE:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.bVal = 0;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_BOOLEAN, "false", value);
		}
		else
		{
			ctx->g_currConstType = T_BOOLEAN;
			ctx->g_currConstValue.bVal = 0;
		}

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_OPAREN:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( !match(ctx, T_CPAREN) )
		{
			printf("Linea %d Colonna %d: Errore expr4. Attesa parentesi ')'.\n",
					ctx->g_Token.line,
					ctx->g_Token.column);
			return 0;
		}
		break;
	case T_OP_MINUS:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( ctx->g_stack[ctx->g_top]->type1 != T_INTEGER && ctx->g_stack[ctx->g_top]->type1 != T_REAL )
		{
			printf("Linea %d Colonna %d: Errore expr4. L'operatore meno unario "
					"accetta solo operandi di tipo integer o real.\n",
					ctx->g_Token.line,
					ctx->g_Token.column);
			return 0;
		}
		pNode = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_OP_UMINUS, 1, pNode);
		ctx->g_stack[ctx->g_top]->type0 = T_SIMPLE;
		ctx->g_stack[ctx->g_top]->type1 = pNode->type1;
		ctx->g_stack[ctx->g_top]->type2 = T_VAR;
		break;
	case T_OP_NOT:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( ctx->g_stack[ctx->g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr4. L'operatore not "
					"accetta solo espressioni di tipo boolean.\n",
					ctx->g_Token.line,
					ctx->g_Token.column);
			return 0;
		}
		pNode = ctx->g_stack[ctx->g_top--];
		ctx->g_stack[++ctx->g_top] = opr(ctx, T_OP_NOT, 1, pNode);
		ctx->g_stack[ctx->g_top]->type0 = T_SIMPLE;
		ctx->g_stack[ctx->g_top]->type1 = T_BOOLEAN;
		ctx->g_stack[ctx->g_top]->type2 = T_VAR;
		break;
	case T_ID:
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore expr4. Identificatore '%s' non dichiarato.\n",
					ctx->g_Token.line,
					ctx->g_Token.column,
					ctx->g_Token.Value.vString);
			return 0;
		}
		if ( hashTable.Tipo0 == T_SIMPLE && !hashTable.bInizializzato )
		{
			if ( !ctx->g_isREAD )
			{
				printf("Linea %d Colonna %d: Errore expr4. Identificatore '%s' non inizializzato.\n",
						ctx->g_Token.line,
						ctx->g_Token.column,
						ctx->g_Token.Value.vString);
				return 0;
			}
			else
			{
				hashTable.bInizializzato = 1;
				scopeUpdateValue(ctx, &ctx->g_Scope, hashTable.Key, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, hashTable.Value, 1, -1);
			}
		}

		if ( hashTable.Tipo0 == T_ARRAY )
		{
			value.pArrayDef = (ArrayDef*)malloc(sizeof(ArrayDef));
			if ( !value.pArrayDef )
			{
				printf("Errore pArrayDef: memoria insufficiente.\n");
				return 0;
			}
			value.pArrayDef->countIndexes = hashTable.Value.pArrayDef->countIndexes;
			value.pArrayDef->pArray = hashTable.Value.pArrayDef->pArray;
			value.pArrayDef->sizes = hashTable.Value.pArrayDef->sizes;
			value.pArrayDef->indexes = (nodeType**)malloc(sizeof(nodeType*)*(size_t)hashTable.Value.pArrayDef->countIndexes);
			if ( !value.pArrayDef->indexes )
			{
				printf("Errore pArrayDef->indexes: memoria insufficiente.\n");
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
			memset(value.pArrayDef->indexes, 0, sizeof(nodeType*)*(size_t)hashTable.Value.pArrayDef->countIndexes);
		}
		else
		{
			value = hashTable.Value;
		}

		ctx->g_stack[++ctx->g_top] = id(ctx, ctx->g_Token.Value.vString, "a", hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, hashTable.bInizializzato);

		GetNextToken(ctx, &ctx->g_Token);

		pNode = NULL;
		currArrayIndex = 0;
		if ( ctx->g_Token.Type == T_QOPAREN )
		{
			pNode = ctx->g_stack[ctx->g_top--];
		}
		while ( ctx->g_Token.Type == T_QOPAREN )
		{
			if ( currArrayIndex >= hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						ctx->g_Token.line,
						ctx->g_Token.column);
				cleanupArrayExprError(ctx, currArrayIndex, &value, pNode);
				return 0;
			}

			GetNextToken(ctx, &ctx->g_Token);
			if ( !expr(ctx) )
			{
				cleanupArrayExprError(ctx, currArrayIndex, &value, pNode);
				return 0;
			}

			if ( ctx->g_stack[ctx->g_top]->type1 != T_INTEGER )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Gli array vanno indicizzati con espressioni di tipo int.\n",
						ctx->g_Token.line,
						ctx->g_Token.column);
				cleanupArrayExprError(ctx, currArrayIndex + 1, &value, pNode);
				return 0;
			}

			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore expr4. Attesa parentesi ']'.\n",
						ctx->g_Token.line,
						ctx->g_Token.column);
				cleanupArrayExprError(ctx, currArrayIndex + 1, &value, pNode);
				return 0;
			}

			currArrayIndex++;
		}
		if ( hashTable.Tipo0 == T_ARRAY )
		{
			if ( currArrayIndex < hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						ctx->g_Token.line,
						ctx->g_Token.column);
				cleanupArrayExprError(ctx, currArrayIndex, &value, pNode);
				return 0;
			}

			for ( currArrayIndex = hashTable.Value.pArrayDef->countIndexes - 1; currArrayIndex >= 0 ; currArrayIndex-- )
			{
				pNode->id.value.pArrayDef->indexes[currArrayIndex] = ctx->g_stack[ctx->g_top--];
			}
			ctx->g_stack[++ctx->g_top] = pNode;
		}

		break;
	default:
		printf("Linea %d Colonna %d: Errore stmt. Atteso 'int literal' o 'real literal'"
				" o 'string literal' o 'true' o 'false' o '(' o '-' o '!' o identificatore.\n",
				ctx->g_Token.line,
				ctx->g_Token.column);
		return 0;
	}

	return 1;
}
