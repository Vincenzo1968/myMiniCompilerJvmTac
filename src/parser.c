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

#define _POSIX_C_SOURCE 200809L

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
#include "structs.h"

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









/* Helper to create a wrapper copy of ArrayDef for an AST node */
static int allocArrayDefWrapper(CompilerContext *ctx, const HashTable *pHT, Valore *pOutVal)
{
    ArrayDef *pArr = (ArrayDef*)malloc(sizeof(ArrayDef));
    if ( !pArr )
    {
        printf("Error pArrayDef: out of memory.\n");
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
            printf("Error pArrayDef->indexes: out of memory.\n");
            ctx->g_numErrors++;
            free(pArr);
            return 0;
        }
    }

    pOutVal->pArrayDef = pArr;
    return 1;
}

/* Frees an ArrayDef wrapper (including any already-assigned indices) */
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

/* Removes nodes created during array index parsing from the stack */
static void popArrayIndexNodes(CompilerContext *ctx, int count)
{
    while ( count > 0 && ctx->g_top >= 0 )
    {
        freeNode(ctx, ctx->g_stack[ctx->g_top--]);
        count--;
    }
}

// Tokens that can start a statement (FIRST(stmt)). 
static int isStmtStart(CompilerContext *ctx, int t)
{
	(void)ctx;
    return t == T_KW_PRINT || t == T_KW_READ || t == T_KW_WHILE ||
           t == T_KW_DO    || t == T_KW_IF   || t == T_KW_BEGIN ||
           t == T_KW_RETURN || t == T_ID;
}

/* Handles the trailing ';' of a statement: if missing, checks current token
   e' gia' l'inizio di una nuova istruzione o la fine di un blocco,
   recupera senza mangiare il token successivo. */
static int expectStmtSemicolon(CompilerContext *ctx)
{
    if ( match(ctx, T_SEMICOLON) )
        return 1;

    printf("Line %d Column %d: Statement error. Expected semicolon ';'.\n",
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

    printf("Line %d Column %d: Statement error. Expected ')'.\n",
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

    /* structs are eliminated by rewriting the source (see structs.c) */
    if ( !structsLower(ctx) )
    {
        FreeInputbuffer(ctx);
        printf("Errors: %d\nWarnings: %d\n", ctx->g_numErrors, ctx->g_numWarnings);
        (*pTree) = NULL;
        return 0;
    }

    GetNextToken(ctx, &ctx->g_Token);

    /* A program = declarations + a single begin...end block. */
    ret = program(ctx);

    /* Parsing succeeded with leftover tokens: code outside the program. */
    if ( ret && ctx->g_Token.Type != T_EOF )
    {
        printf("Line %d Column %d: Error. Unexpected tokens after end of program.\n",
               ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        ret = 0;
    }

    /* Flushes the buffer up to EOF (never past it: after EOF the scanner
       restituirebbe T_ERROR all'infinito). */
    while ( ctx->g_Token.Type != T_EOF )
        GetNextToken(ctx, &ctx->g_Token);

    FreeInputbuffer(ctx);

    /* If program() failed, ensures at least one counted error:
       evita ret=1 con stack vuoto (lettura di g_stack[-1]). */
    if ( !ret && ctx->g_numErrors == 0 )
        ctx->g_numErrors++;

    ret = ctx->g_numErrors ? 0 : 1;

    if ( ctx->g_numErrors || ctx->g_numWarnings )
        printf("Errors: %d\nWarnings: %d\n", ctx->g_numErrors, ctx->g_numWarnings);

    if ( !ret )
    {
        (*pTree) = NULL;
        while ( ctx->g_top >= 0 )
        {
            freeNode(ctx, ctx->g_stack[ctx->g_top]);   // freeNode(NULL) e' sicuro 
            ctx->g_top--;                    // ALWAYS decrement, even if NULL 
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
	//	printf("Syntax error\n");
	//	exit(1);
	//}

	//printf("Line %d Column %d: Syntax error\n", g_Token.line, g_Token.column);

	return 0;
}

int TypeCheck(CompilerContext *ctx, Valore *value, const Token *pToken)
{
	if ( ctx->g_currType == ctx->g_currConstType )
	{
		(*value) = ctx->g_currConstValue;

		//if ( g_currType == T_BOOLEAN )
		//{
		//	printf("Value    -> %d\n", g_currConstValue.bVal);
		//	printf("Value 2  -> %d\n", g_currConstValue.iVal);
		//}
	}
	else
	{
		switch ( ctx->g_currType )
		{
		case T_INTEGER:
			if ( ctx->g_currConstType == T_REAL )
			{
				printf("Line %d Column %d: "
						"Warning var_list. "
						"Assigning a value of type real to variable '%s' "
						"of type int. Data loss may occur.\n",
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
				printf("Line %d Column %d: "
						"var_list error. "
						"assigning a value of type string to variable '%s' "
						"of type int.\n",
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
				printf("Line %d Column %d: "
						"var_list error. "
						"assigning a value of type string to variable '%s' "
						"of type real.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		case T_BOOLEAN:
			if ( ctx->g_currConstType == T_INTEGER )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"Assigning a value of type int to variable '%s' "
						"of type bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_REAL )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"Assigning a value of type real to variable '%s' "
						"of type bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_STRING )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"assigning a value of type string to variable '%s' "
						"of type bool.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			break;
		case T_STRING:
			if ( ctx->g_currConstType == T_INTEGER )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"Assigning a value of type int to variable '%s' "
						"of type string.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_REAL )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"Assigning a value of type real to variable '%s' "
						"of type string.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				return 0;
			}
			else if ( ctx->g_currConstType == T_BOOLEAN )
			{
				printf("Line %d Column %d: "
						"var_list error. "
						"assigning a value of type bool to variable '%s' "
						"of type string.\n",
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

/* ===================================================================== */
/*  Funzioni                                                             */
/* ===================================================================== */

#define MAX_FUNC_PARAMS 32

static int argCompat(enumTipo1 param, enumTipo1 arg)
{
	/* 0 = error, 1 = ok, 2 = ok with warning (data loss) */
	if ( param == arg )
		return 1;
	if ( param == T_REAL && arg == T_INTEGER )
		return 1;
	if ( param == T_INTEGER && arg == T_REAL )
		return 2;
	if ( param == T_BOOLEAN && (arg == T_INTEGER || arg == T_REAL) )
		return 1;
	return 0;
}

static int funcParseError(CompilerContext *ctx, const char *msg)
{
	printf("Line %d Column %d: Function error. %s\n", ctx->g_Token.line, ctx->g_Token.column, msg);
	ctx->g_numErrors++;
	return 0;
}

/* header : (type | 'void') T_ID '(' [param {',' param}] ')'
   param  : ['ref'] type T_ID | 'array' T_ID '[' ']' {'[' ']'} 'of' type */
static int parseFuncHeader(CompilerContext *ctx, char *name, size_t nameSize, int *pIsVoid, enumTipo1 *pRet,
                           int *pLine, FuncParam **pParams, int *pCount)
{
	FuncParam *params = NULL;
	int n = 0, i;

	*pParams = NULL;
	*pCount = 0;

	if ( ctx->g_Token.Type == T_KW_VOID )
	{
		*pIsVoid = 1;
		*pRet = T_INTEGER;
		GetNextToken(ctx, &ctx->g_Token);
	}
	else
	{
		*pIsVoid = 0;
		if ( !type(ctx) )
			return 0;
		*pRet = (enumTipo1)ctx->g_currType;
	}

	if ( ctx->g_Token.Type != T_ID )
		return funcParseError(ctx, "Expected function name.");
	snprintf(name, nameSize, "%s", ctx->g_Token.Value.vString);
	*pLine = ctx->g_Token.line;
	GetNextToken(ctx, &ctx->g_Token);

	if ( ctx->g_Token.Type != T_OPAREN )
		return funcParseError(ctx, "Expected '('.");
	GetNextToken(ctx, &ctx->g_Token);

	params = (FuncParam*)calloc(MAX_FUNC_PARAMS, sizeof(FuncParam));
	if ( !params )
		return funcParseError(ctx, "Out of memory.");

	if ( ctx->g_Token.Type != T_CPAREN )
	{
		for (;;)
		{
			FuncParam *fp;
			if ( n >= MAX_FUNC_PARAMS )
			{
				funcParseError(ctx, "Too many parameters.");
				goto fail;
			}
			fp = &params[n];
			fp->t0 = T_SIMPLE;

			if ( ctx->g_Token.Type == T_KW_REF )
			{
				fp->isRef = 1;
				GetNextToken(ctx, &ctx->g_Token);
			}

			if ( ctx->g_Token.Type == T_KW_ARRAY )
			{
				if ( fp->isRef )
				{
					funcParseError(ctx, "Arrays are always passed by reference: 'ref' is not allowed.");
					goto fail;
				}
				GetNextToken(ctx, &ctx->g_Token);
				if ( ctx->g_Token.Type != T_ID )
				{
					funcParseError(ctx, "Expected array parameter name.");
					goto fail;
				}
				fp->name = strdup(ctx->g_Token.Value.vString);
				GetNextToken(ctx, &ctx->g_Token);
				while ( ctx->g_Token.Type == T_QOPAREN )
				{
					GetNextToken(ctx, &ctx->g_Token);
					if ( ctx->g_Token.Type != T_QCPAREN )
					{
						funcParseError(ctx, "Expected ']' (array parameter dimensions must not be specified).");
						n++;
						goto fail;
					}
					fp->dims++;
					GetNextToken(ctx, &ctx->g_Token);
				}
				if ( fp->dims == 0 )
				{
					funcParseError(ctx, "An array parameter requires at least '[]'.");
					n++;
					goto fail;
				}
				if ( ctx->g_Token.Type != T_KW_OF )
				{
					funcParseError(ctx, "Expected keyword 'of'.");
					n++;
					goto fail;
				}
				GetNextToken(ctx, &ctx->g_Token);
				if ( !type(ctx) )
				{
					n++;
					goto fail;
				}
				fp->t0 = T_ARRAY;
				fp->t1 = (enumTipo1)ctx->g_currType;
			}
			else
			{
				if ( !type(ctx) )
					goto fail;
				fp->t1 = (enumTipo1)ctx->g_currType;
				if ( ctx->g_Token.Type != T_ID )
				{
					funcParseError(ctx, "Expected parameter name.");
					goto fail;
				}
				fp->name = strdup(ctx->g_Token.Value.vString);
				GetNextToken(ctx, &ctx->g_Token);
			}

			for ( i = 0; i < n; i++ )
			{
				if ( strcmp(params[i].name, fp->name) == 0 )
				{
					printf("Line %d Column %d: Function error. Duplicate parameter '%s'.\n",
					       ctx->g_Token.line, ctx->g_Token.column, fp->name);
					ctx->g_numErrors++;
					n++;
					goto fail;
				}
			}
			n++;

			if ( ctx->g_Token.Type == T_COMMA )
			{
				GetNextToken(ctx, &ctx->g_Token);
				continue;
			}
			break;
		}
	}

	if ( ctx->g_Token.Type != T_CPAREN )
	{
		funcParseError(ctx, "Expected ')'. ");
		goto fail;
	}
	GetNextToken(ctx, &ctx->g_Token);

	*pParams = params;
	*pCount = n;
	return 1;

fail:
	funcFreeParams(params, n);
	return 0;
}

/* prototype : header ';'   (allowed only before the main block) */
static int parseFuncPrototype(CompilerContext *ctx)
{
	char name[256];
	int isVoid, line, n, idx;
	enumTipo1 ret;
	FuncParam *params;

	if ( !parseFuncHeader(ctx, name, sizeof(name), &isVoid, &ret, &line, &params, &n) )
		return 0;

	if ( ctx->g_Token.Type != T_SEMICOLON )
	{
		funcFreeParams(params, n);
		return funcParseError(ctx, "Expected ';' after prototype (definitions must appear after the main block).");
	}
	GetNextToken(ctx, &ctx->g_Token);

	idx = funcFind(ctx, name);
	if ( idx >= 0 )
	{
		int same = funcSignatureEquals(&ctx->funcs[idx], isVoid, ret, n, params);
		funcFreeParams(params, n);
		if ( !same )
		{
			printf("Line %d: Function error. Prototype of '%s' conflicts with a previous declaration.\n", line, name);
			ctx->g_numErrors++;
			return 0;
		}
		return 1;
	}

	if ( scopeFind(ctx, &ctx->g_Scope, name, &(HashTable){0}) )
	{
		funcFreeParams(params, n);
		printf("Line %d: Function error. '%s' is already declared as a variable.\n", line, name);
		ctx->g_numErrors++;
		return 0;
	}

	if ( funcDeclare(ctx, name, isVoid, ret, n, params, line) < 0 )
	{
		funcFreeParams(params, n);
		return funcParseError(ctx, "Out of memory.");
	}
	return 1;
}

/* definition : header code_block   (after the main block) */
static int parseFuncDefinition(CompilerContext *ctx)
{
	char name[256];
	int isVoid, line, n, idx, i, ok;
	enumTipo1 ret;
	FuncParam *params;

	if ( !parseFuncHeader(ctx, name, sizeof(name), &isVoid, &ret, &line, &params, &n) )
		return 0;

	if ( ctx->g_Token.Type == T_SEMICOLON )
	{
		funcFreeParams(params, n);
		return funcParseError(ctx, "Prototypes must be declared at the top of the file, before the main block.");
	}

	idx = funcFind(ctx, name);
	if ( idx >= 0 )
	{
		int same = funcSignatureEquals(&ctx->funcs[idx], isVoid, ret, n, params);
		funcFreeParams(params, n);
		if ( !same )
		{
			printf("Line %d: Function error. Definition of '%s' does not match prototype.\n", line, name);
			ctx->g_numErrors++;
			return 0;
		}
		if ( ctx->funcs[idx].defined )
		{
			printf("Line %d: Function error. '%s' has already been defined.\n", line, name);
			ctx->g_numErrors++;
			return 0;
		}
		/* parameters are taken from the prototype (same signature) */
	}
	else
	{
		if ( scopeFind(ctx, &ctx->g_Scope, name, &(HashTable){0}) )
		{
			funcFreeParams(params, n);
			printf("Line %d: Function error. '%s' is already declared as a variable.\n", line, name);
			ctx->g_numErrors++;
			return 0;
		}
		idx = funcDeclare(ctx, name, isVoid, ret, n, params, line);
		if ( idx < 0 )
		{
			funcFreeParams(params, n);
			return funcParseError(ctx, "Out of memory.");
		}
	}

	/* parameter scope; the body (code_block) opens its own scope for local variables */
	ctx->g_curFunc = idx;
	scopePush(ctx, &ctx->g_Scope);
	for ( i = 0; i < ctx->funcs[idx].nparams; i++ )
	{
		FuncParam *fp = &ctx->funcs[idx].params[i];
		Valore v;
		memset(&v, 0, sizeof(v));
		if ( fp->t0 == T_ARRAY )
		{
			v.pArrayDef = (ArrayDef*)calloc(1, sizeof(ArrayDef));
			if ( v.pArrayDef )
			{
				v.pArrayDef->countIndexes = fp->dims;
				v.pArrayDef->sizes = (int*)calloc((size_t)fp->dims, sizeof(int));
			}
		}
		scopeInsert(ctx, &ctx->g_Scope, fp->name, fp->t0, fp->t1, T_VAR, v, 1, 0);
		scopeAssignUniqueName(ctx, &ctx->g_Scope, fp->name);
		{
			HashTable ht;
			free(fp->uniqueName);
			fp->uniqueName = NULL;
			if ( scopeFind(ctx, &ctx->g_Scope, fp->name, &ht) )
				fp->uniqueName = strdup(ht.uniqueName);
		}
	}

	ok = code_block(ctx);

	scopePopKeep(ctx, &ctx->g_Scope);
	ctx->g_curFunc = -1;

	if ( !ok )
		return 0;

	ctx->funcs[idx].body = ctx->g_stack[ctx->g_top--];
	ctx->funcs[idx].defined = 1;
	return 1;
}

/* Call: T_ID '(' [arg {',' arg}] ')'. Leaves the T_FUNC_CALL node on top of g_stack.
   op[0] = id con il nome (type1 = tipo di ritorno), op[1..] = argomenti.
   Per un parametro 'ref' l'argomento e' un id scalare; per un array e' un id array senza indici. */
static int parseCall(CompilerContext *ctx)
{
	char name[256];
	int idx, i, nops = 0;
	int line = ctx->g_Token.line, column = ctx->g_Token.column;
	nodeType *ops[MAX_FUNC_PARAMS + 1];
	FuncDef *fd;
	Valore zero;
	nodeType *call;

	memset(&zero, 0, sizeof(zero));
	snprintf(name, sizeof(name), "%s", ctx->g_Token.Value.vString);
	idx = funcFind(ctx, name);
	fd = &ctx->funcs[idx];

	if ( !ctx->g_isCodeBlock )
	{
		printf("Line %d Column %d: Call error. Function calls are not allowed in declarations.\n", line, column);
		ctx->g_numErrors++;
		return 0;
	}

	GetNextToken(ctx, &ctx->g_Token);
	if ( ctx->g_Token.Type != T_OPAREN )
	{
		printf("Line %d Column %d: Call error. Expected '(' after '%s'.\n", ctx->g_Token.line, ctx->g_Token.column, name);
		ctx->g_numErrors++;
		return 0;
	}
	GetNextToken(ctx, &ctx->g_Token);

	ops[nops++] = id(ctx, name, name, T_SIMPLE, fd->isVoid ? T_INTEGER : fd->retType, T_VAR, zero, 1);

	for ( i = 0; i < fd->nparams; i++ )
	{
		FuncParam *fp = &fd->params[i];

		if ( i > 0 )
		{
			if ( ctx->g_Token.Type != T_COMMA )
			{
				printf("Line %d Column %d: Call error. '%s' requires %d arguments: missing ','.\n",
				       ctx->g_Token.line, ctx->g_Token.column, name, fd->nparams);
				ctx->g_numErrors++;
				goto fail;
			}
			GetNextToken(ctx, &ctx->g_Token);
		}

		if ( fp->t0 == T_ARRAY || fp->isRef )
		{
			HashTable ht;
			Valore av;

			if ( ctx->g_Token.Type != T_ID || !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &ht) )
			{
				printf("Line %d Column %d: Call error. Argument %d of '%s' must be %s.\n",
				       ctx->g_Token.line, ctx->g_Token.column, i + 1, name,
				       fp->t0 == T_ARRAY ? "an array" : "a variable ('ref' parameter)");
				ctx->g_numErrors++;
				goto fail;
			}
			if ( fp->t0 == T_ARRAY )
			{
				if ( ht.Tipo0 != T_ARRAY || ht.Tipo1 != fp->t1 || ht.Value.pArrayDef->countIndexes != fp->dims )
				{
					printf("Line %d Column %d: Call error. Argument %d of '%s' must be a %d-dimensional array of the same type as the parameter.\n",
					       ctx->g_Token.line, ctx->g_Token.column, i + 1, name, fp->dims);
					ctx->g_numErrors++;
					goto fail;
				}
				if ( !allocArrayDefWrapper(ctx, &ht, &av) )
					goto fail;
				ops[nops++] = id(ctx, ctx->g_Token.Value.vString, ht.uniqueName, T_ARRAY, ht.Tipo1, ht.Tipo2, av, 1);
			}
			else
			{
				if ( ht.Tipo0 != T_SIMPLE || ht.Tipo2 != T_VAR || ht.Tipo1 != fp->t1 )
				{
					printf("Line %d Column %d: Call error. Argument %d of '%s' ('ref' parameter) must be a scalar variable of the same type.\n",
					       ctx->g_Token.line, ctx->g_Token.column, i + 1, name);
					ctx->g_numErrors++;
					goto fail;
				}
				/* a 'ref' parameter may also be write-only: the variable is marked as initialized */
				scopeUpdateValue(ctx, &ctx->g_Scope, ht.Key, ht.Tipo0, ht.Tipo1, ht.Tipo2, ht.Value, 1, -1);
				ops[nops++] = id(ctx, ctx->g_Token.Value.vString, ht.uniqueName, T_SIMPLE, ht.Tipo1, ht.Tipo2, ht.Value, 1);
			}
			GetNextToken(ctx, &ctx->g_Token);
			if ( ctx->g_Token.Type == T_QOPAREN )
			{
				printf("Line %d Column %d: Call error. Argument %d of '%s' cannot be an array element.\n",
				       ctx->g_Token.line, ctx->g_Token.column, i + 1, name);
				ctx->g_numErrors++;
				goto fail;
			}
		}
		else
		{
			int c;
			if ( !expr(ctx) )
				goto fail;
			ops[nops++] = ctx->g_stack[ctx->g_top--];
			c = argCompat(fp->t1, ops[nops - 1]->type1);
			if ( ops[nops - 1]->type0 == T_ARRAY || c == 0 )
			{
				printf("Line %d Column %d: Call error. Type of argument %d of '%s' is incompatible with parameter.\n",
				       ctx->g_Token.line, ctx->g_Token.column, i + 1, name);
				ctx->g_numErrors++;
				goto fail;
			}
			if ( c == 2 )
			{
				printf("Line %d Column %d: Call warning. Argument %d of '%s': real -> int conversion, data loss may occur.\n",
				       line, column, i + 1, name);
				ctx->g_numWarnings++;
			}
		}
	}

	if ( ctx->g_Token.Type != T_CPAREN )
	{
		printf("Line %d Column %d: Call error. '%s' accepts %d arguments: expected ')'.\n",
		       ctx->g_Token.line, ctx->g_Token.column, name, fd->nparams);
		ctx->g_numErrors++;
		goto fail;
	}
	GetNextToken(ctx, &ctx->g_Token);

	call = oprN(ctx, T_FUNC_CALL, nops, ops);
	call->type0 = T_SIMPLE;
	call->type1 = fd->isVoid ? T_INTEGER : fd->retType;
	call->type2 = T_VAR;
	ctx->g_stack[++ctx->g_top] = call;
	return 1;

fail:
	for ( i = 0; i < nops; i++ )
		freeNode(ctx, ops[i]);
	return 0;
}

static void checkAllFunctionsDefined(CompilerContext *ctx)
{
	int i;
	for ( i = 0; i < ctx->funcCount; i++ )
	{
		if ( !ctx->funcs[i].defined )
		{
			printf("Line %d: Function error. '%s' is declared but never defined.\n", ctx->funcs[i].line, ctx->funcs[i].name);
			ctx->g_numErrors++;
		}
	}
}

//program : {decl | prototipo} code_block {definizione};
int program(CompilerContext *ctx)
{
	if ( !decl_list(ctx) )
		return 0;

	if ( !code_block(ctx) )
		return 0;

	while ( ctx->g_Token.Type == T_KW_VOID || ctx->g_Token.Type == T_KW_INT || ctx->g_Token.Type == T_KW_REAL ||
	        ctx->g_Token.Type == T_KW_BOOL || ctx->g_Token.Type == T_KW_STRING )
	{
		if ( !parseFuncDefinition(ctx) )
			return 0;
	}

	checkAllFunctionsDefined(ctx);

	return 1;
}

//decl_list : {decl};
static void interpRecordDecl(CompilerContext *ctx, const char *name, enumTipo0 t0, enumTipo1 t1,
                             enumTipo2 t2, Valore val, char bInizializzato);

int decl_list(CompilerContext *ctx)
{
	/* prototypes are allowed only at the outermost level, before the main block */
	while ( ctx->g_Token.Type == T_KW_ARRAY ||
		ctx->g_Token.Type == T_KW_INT ||
		ctx->g_Token.Type == T_KW_REAL ||
		ctx->g_Token.Type == T_KW_BOOL ||
		ctx->g_Token.Type == T_KW_STRING ||
		(ctx->g_Token.Type == T_KW_VOID && !ctx->g_isCodeBlock) )
	{
		if ( !ctx->g_isCodeBlock &&
		     (ctx->g_Token.Type == T_KW_VOID ||
		      (ctx->g_Token.Type != T_KW_ARRAY && PeekIsFunctionDecl(ctx))) )
		{
			if ( !parseFuncPrototype(ctx) )
				return 0;
			continue;
		}
		if ( !decl(ctx) )
			return 0;
	}

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
		printf("Line %d Column %d: Type error. Expected keyword 'int', 'real', 'bool', or 'string'.\n",
		       ctx->g_Token.line, ctx->g_Token.column);
		ctx->g_numErrors++;
		return 0;
	}
}

int isDeclaredInCurrentScope(CompilerContext *ctx, Scope *pScope, char *key, HashTable *pDati)
{
    if ( !pScope || pScope->top < 0 || !pScope->stack[pScope->top] )
        return 0;
    return htFind(ctx, pScope->stack[pScope->top], key, pDati) >= 0;
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
			printf("Error: out of memory.\n");
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
				printf("Line %d Column %d: Declaration error. Identifier too long.\n",
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
			printf("Line %d Column %d: Declaration error. Expected identifier.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		//if ( scopeFind(ctx, &ctx->g_Scope, token.Value.vString, &hashTable) )
		if ( isDeclaredInCurrentScope(ctx, &ctx->g_Scope, token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
					token.line, token.column, token.Value.vString);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( !match(ctx, T_QOPAREN) )
		{
			printf("Line %d Column %d: Declaration error. Expected '['.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( ctx->g_Token.Type == T_INT_LITERAL )
		{
			if ( ctx->g_Token.Value.vInt <= 0 )
			{
				printf("Line %d Column %d: Declaration error. Array size must be positive.\n",
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
			printf("Line %d Column %d: Declaration error. Expected integer literal.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		if ( !match(ctx, T_QCPAREN) )
		{
			printf("Line %d Column %d: Declaration error. Expected ']'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}
		while ( ctx->g_Token.Type == T_QOPAREN )
		{
			if ( !match(ctx, T_QOPAREN) )
			{
				printf("Line %d Column %d: Declaration error. Expected '['.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}

			if ( ctx->g_Token.Type == T_INT_LITERAL )
			{
				if ( value.pArrayDef->countIndexes >= 89 )
				{
					printf("Line %d Column %d: Declaration error. Too many array dimensions.\n",
							ctx->g_Token.line, ctx->g_Token.column);
					ctx->g_numErrors++;
					free(value.pArrayDef);
					return 0;
				}
				if ( ctx->g_Token.Value.vInt <= 0 )
				{
					printf("Line %d Column %d: Declaration error. Array size must be positive.\n",
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
				printf("Line %d Column %d: Declaration error. Expected integer literal.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Line %d Column %d: Declaration error. Expected ']'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				free(value.pArrayDef);
				return 0;
			}
		}
		if ( !match(ctx, T_KW_OF) )
		{
			printf("Line %d Column %d: Declaration error. Expected keyword 'of'.\n",
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
			printf("Line %d Column %d: Declaration error. Invalid total array size.\n",
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
			printf("Error pArrayDef->pArray: out of memory.\n");
			ctx->g_numErrors++;
			free(value.pArrayDef);
			return 0;
		}

		if ( !match(ctx, T_SEMICOLON) )
		{
			printf("Line %d Column %d: Declaration error. Expected semicolon ';'.\n",
				ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			free(value.pArrayDef->pArray);
			free(value.pArrayDef);
			return 0;
		}

		value.pArrayDef->sizes = (int*)calloc((size_t)value.pArrayDef->countIndexes, sizeof(int));
		if ( !value.pArrayDef->sizes )
		{
			printf("Error pArrayDef->sizes: out of memory.\n");
			ctx->g_numErrors++;
			free(value.pArrayDef->pArray);
			free(value.pArrayDef);
			return 0;
		}

		for ( currIndex = 0; currIndex < value.pArrayDef->countIndexes; currIndex++ )
			value.pArrayDef->sizes[currIndex] = indici[currIndex];

		value.pArrayDef->indexes = NULL;
		scopeInsert(ctx, &ctx->g_Scope, token.Value.vString, T_ARRAY, ctx->g_currType, T_VAR, value, 0, ctx->g_NextFrameIndex);
		scopeAssignUniqueName(ctx, &ctx->g_Scope, token.Value.vString);
		{
			/* the interpreter has its own scope: register a copy of the array for it */
			Valore rv = value;
			rv.pArrayDef = arrayDefClone(value.pArrayDef, (enumTipo1)ctx->g_currType);
			interpRecordDecl(ctx, token.Value.vString, T_ARRAY, (enumTipo1)ctx->g_currType, T_VAR, rv, 0);
		}
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
			printf("Line %d Column %d: Declaration error. Expected semicolon ';'.\n",
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

static int interpBeginBlockRecord(CompilerContext *ctx)
{
	int id = ctx->interpDecls.nextBlockId++;
	ctx->interpDecls.currentBlockId = id;
	return id;
}

static void interpEndBlockRecord(CompilerContext *ctx, int prevBlockId)
{
	ctx->interpDecls.currentBlockId = prevBlockId;
}

static void interpRecordDecl(CompilerContext *ctx,
                             const char *name,
                             enumTipo0 t0,
                             enumTipo1 t1,
                             enumTipo2 t2,
                             Valore val,
                             char bInizializzato)
{
	if (!ctx)
		return;

	if (ctx->interpDecls.count >= ctx->interpDecls.cap)
	{
		int newCap = (ctx->interpDecls.cap == 0) ? 64 : ctx->interpDecls.cap * 2;
		InterpVarDecl *tmp = (InterpVarDecl *)realloc(
			ctx->interpDecls.decls, (size_t)newCap * sizeof(InterpVarDecl));
		if (!tmp)
			return;
		ctx->interpDecls.decls = tmp;
		ctx->interpDecls.cap = newCap;
	}

	InterpVarDecl *d = &ctx->interpDecls.decls[ctx->interpDecls.count++];
	memset(d, 0, sizeof(*d));
	d->blockId = ctx->interpDecls.currentBlockId;
	d->name = strdup(name ? name : "");
	d->t0 = t0;
	d->t1 = t1;
	d->t2 = t2;
	d->val = val;
	d->bInizializzato = bInizializzato;

	if (t1 == T_STRING && bInizializzato && val.strVal != NULL)
	{
		d->val.strVal = strdup(val.strVal);
	}
}

/* A declaration initializer ("int x = 8;") becomes a normal AST node
   di assegnazione nell'AST: cosi' finisce nel TAC al punto giusto (anche dentro
   un ciclo) ed e' soggetto agli stessi passi di ottimizzazione, invece di essere
   stampato a parse-time o letto dalla symtab dai singoli backend. */
static void addDeclInitNode(CompilerContext *ctx, const char *name, enumTipo1 t1, Valore value)
{
	HashTable hashTable;
	Valore zero;
	Valore v = value;
	char lexeme[64] = "";
	nodeType *lhs, *rhs, *assign;

	if ( !scopeFind(ctx, &ctx->g_Scope, (char*)name, &hashTable) )
		return;

	memset(&zero, 0, sizeof(zero));

	switch ( t1 )
	{
	case T_INTEGER:
		snprintf(lexeme, sizeof(lexeme), "%d", value.iVal);
		break;
	case T_REAL:
		{
			char *e;
			snprintf(lexeme, sizeof(lexeme), "%.15g", value.dblVal);
			if ( strchr(lexeme, '.') == NULL )
			{
				e = strchr(lexeme, 'e');
				if ( e == NULL )
					strcat(lexeme, ".0");
				else
				{
					char tail[32];
					snprintf(tail, sizeof(tail), "%s", e);
					strcpy(e, ".0");
					strcat(lexeme, tail);
				}
			}
		}
		break;
	case T_BOOLEAN:
		snprintf(lexeme, sizeof(lexeme), "%s", value.bVal ? "true" : "false");
		break;
	case T_STRING:
		v.strVal = strdup(value.strVal ? value.strVal : "");
		if ( v.strVal == NULL )
			return;
		break;
	}

	lhs = id(ctx, (char*)name, hashTable.uniqueName, T_SIMPLE, t1, T_VAR, zero, 1);
	rhs = con(ctx, t1, (t1 == T_STRING) ? NULL : lexeme, v);
	assign = opr(ctx, T_OP_ASSIGN, 2, lhs, rhs);

	if ( ctx->g_initCount >= ctx->g_initCap )
	{
		int newCap = (ctx->g_initCap == 0) ? 16 : ctx->g_initCap * 2;
		nodeType **tmp = (nodeType**)realloc(ctx->g_initNodes, (size_t)newCap * sizeof(nodeType*));
		if ( tmp == NULL )
		{
			freeNode(ctx, assign);
			return;
		}
		ctx->g_initNodes = tmp;
		ctx->g_initCap = newCap;
	}
	ctx->g_initNodes[ctx->g_initCount++] = assign;
}

/* Prepends assignments from declaration initializers to the newly parsed stmt_list (top of g_stack)
   inizializzatori dichiarati da startIdx in poi. */
static void attachDeclInits(CompilerContext *ctx, int startIdx)
{
	int k;

	if ( ctx->g_initCount <= startIdx )
		return;

	if ( ctx->g_top < 0 )
	{
		for ( k = startIdx; k < ctx->g_initCount; k++ )
			freeNode(ctx, ctx->g_initNodes[k]);
		ctx->g_initCount = startIdx;
		return;
	}

	for ( k = ctx->g_initCount - 1; k >= startIdx; k-- )
		ctx->g_stack[ctx->g_top] = opr(ctx, T_STMT_LIST, 2, ctx->g_initNodes[k], ctx->g_stack[ctx->g_top]);

	ctx->g_initCount = startIdx;
}

static int parse_var_item(CompilerContext *ctx)
{
	Token token = ctx->g_Token;
	Valore value;
	HashTable hashTable;
	char szVarName[256] = "";
	char bInit = 0;

	reset_var_value(ctx, &value, (enumTipo1)ctx->g_currType);

	if (ctx->g_Token.Type == T_ID && ctx->g_Token.Value.vString != NULL)
	{
		snprintf(szVarName, sizeof(szVarName), "%s", ctx->g_Token.Value.vString);
	}
	token.Value.vString = szVarName;

	if (!match(ctx, T_ID))
	{
		printf("Line %d Column %d: var_list error. Expected identifier.\n",
		       ctx->g_Token.line, ctx->g_Token.column);
		ctx->g_numErrors++;
		return 0;
	}

	if (isDeclaredInCurrentScope(ctx, &ctx->g_Scope, token.Value.vString, &hashTable))
	{
		printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
		       token.line, token.column, token.Value.vString);
		ctx->g_numErrors++;
		return 0;
	}

	scopeInsert(ctx, &ctx->g_Scope, token.Value.vString, T_SIMPLE, (enumTipo1)ctx->g_currType,
	            T_VAR, value, 0, ctx->g_NextFrameIndex);
	scopeAssignUniqueName(ctx, &ctx->g_Scope, token.Value.vString);
	if (ctx->g_genJVM)
	{
		if (ctx->g_currType == T_REAL)
			ctx->g_NextFrameIndex += 2;
		else
			ctx->g_NextFrameIndex++;
		jvmSetNumberLocals(ctx, ctx->g_NextFrameIndex);
	}

	if (ctx->g_Token.Type == T_OP_ASSIGN)
	{
		int savedIsCodeBlock = ctx->g_isCodeBlock;
		ctx->g_isCodeBlock = 0;

		GetNextToken(ctx, &ctx->g_Token);

		ctx->g_currConstType = -1;
		if (!expr(ctx))
		{
			ctx->g_isCodeBlock = savedIsCodeBlock;
			ctx->g_numErrors++;
			return 0;
		}
		ctx->g_isCodeBlock = savedIsCodeBlock;

		if (ctx->g_currConstType != -1)
		{
			if (!TypeCheck(ctx, &value, &token))
			{
				ctx->g_numErrors++;
				return 0;
			}

			bInit = 1;
			scopeUpdateValue(ctx, &ctx->g_Scope, szVarName, T_SIMPLE, (enumTipo1)ctx->g_currType,
			                 T_VAR, value, 1, 1);

			addDeclInitNode(ctx, szVarName, (enumTipo1)ctx->g_currType, value);
		}
		else
		{
			printf("Linea %d Colonna %d: Errore var_list. In fase di dichiarazione è possibile assegnare solo valori costanti.\n",
			       token.line, token.column);
			ctx->g_numErrors++;
			return 0;
		}
	}

	interpRecordDecl(ctx, szVarName, T_SIMPLE, (enumTipo1)ctx->g_currType, T_VAR, value, bInit);

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

// code_block : T_KW_BEGIN decl_list stmt_list T_KW_END;
int code_block(CompilerContext *ctx)
{
    nodeType *left = NULL;
    int prevBlock;
    int myBlockId;
    int initStart;

    ctx->g_isCodeBlock = 1;

    if ( !match(ctx, T_KW_BEGIN) )
    {
        printf("Line %d Column %d: Declaration error. Expected keyword 'begin'.\n", ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        return 0;
    }

    prevBlock = ctx->interpDecls.currentBlockId;
    myBlockId = interpBeginBlockRecord(ctx);

    /* outermost block (prevBlock == -1): also includes initializers
       delle dichiarazioni globali, parsate prima di entrare qui. */
    initStart = (prevBlock == -1) ? 0 : ctx->g_initCount;

    scopePush(ctx, &ctx->g_Scope);
    decl_list(ctx);
    stmt_list(ctx);   // handles its own errors and always returns 1
    attachDeclInits(ctx, initStart);
    scopePopKeep(ctx, &ctx->g_Scope);

    interpEndBlockRecord(ctx, prevBlock);

    if ( !match(ctx, T_KW_END) )
    {
        printf("Line %d Column %d: Declaration error. Expected keyword 'end'.\n",
               ctx->g_Token.line, ctx->g_Token.column);
        ctx->g_numErrors++;
        return 0;
    }

	left = ctx->g_stack[ctx->g_top--];
	nodeType *blkNode = opr(ctx, T_CODE_BLOCK, 1, left);
	blkNode->opr.blockId = myBlockId;
	ctx->g_stack[++ctx->g_top] = blkNode;

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

            /* If stmt() consumed nothing (e.g., undeclared identifier
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
	case T_KW_RETURN:
		line = ctx->g_Token.line;
		column = ctx->g_Token.column;
		if ( ctx->g_curFunc < 0 )
		{
			printf("Line %d Column %d: Statement error. 'return' is only allowed inside a function.\n", line, column);
			ctx->g_numErrors++;
			return 0;
		}
		GetNextToken(ctx, &ctx->g_Token);
		if ( ctx->funcs[ctx->g_curFunc].isVoid )
		{
			if ( ctx->g_Token.Type != T_SEMICOLON )
			{
				printf("Line %d Column %d: Statement error. A void function cannot return a value.\n", line, column);
				ctx->g_numErrors++;
				return 0;
			}
			GetNextToken(ctx, &ctx->g_Token);
			ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_RETURN, 1, NULL);
		}
		else
		{
			int c;
			if ( ctx->g_Token.Type == T_SEMICOLON )
			{
				printf("Line %d Column %d: Statement error. Function '%s' must return a value.\n", line, column, ctx->funcs[ctx->g_curFunc].name);
				ctx->g_numErrors++;
				return 0;
			}
			if ( !expr(ctx) )
				return 0;
			c = argCompat(ctx->funcs[ctx->g_curFunc].retType, ctx->g_stack[ctx->g_top]->type1);
			if ( ctx->g_stack[ctx->g_top]->type0 == T_ARRAY || c == 0 )
			{
				printf("Line %d Column %d: Statement error. Return value is incompatible with return type of '%s'.\n",
				       line, column, ctx->funcs[ctx->g_curFunc].name);
				ctx->g_numErrors++;
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
				return 0;
			}
			if ( c == 2 )
			{
				printf("Line %d Column %d: Statement warning. 'return': real -> int conversion, data loss may occur.\n", line, column);
				ctx->g_numWarnings++;
			}
			if ( !expectStmtSemicolon(ctx) )
			{
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
				return 0;
			}
			left = ctx->g_stack[ctx->g_top--];
			ctx->g_stack[++ctx->g_top] = opr(ctx, T_KW_RETURN, 1, left);
		}
		break;

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
			printf("Line %d Column %d: Statement error. 'read' requires an identifier or array element.\n",
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
			printf("Line %d Column %d: Statement error. Expected '('.\n",
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
			printf("Line %d Column %d: Statement error. The expression inside "
					"parentheses must evaluate to a boolean value.\n",
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
			printf("Line %d Column %d: Statement error. Expected keyword 'while'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		if ( !match(ctx, T_OPAREN) )
		{
			printf("Line %d Column %d: Statement error. Expected '('.\n",
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
			printf("Line %d Column %d: Statement error. The expression inside "
					"parentheses must evaluate to a boolean value.\n",
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
			printf("Line %d Column %d: Statement error. Expected '('.\n",
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
			printf("Line %d Column %d: Statement error. The expression inside "
					"parentheses must evaluate to a boolean value.\n",
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
	{
		GetNextToken(ctx, &ctx->g_Token);

		int prevBlock = ctx->interpDecls.currentBlockId;
		int myBlockId = interpBeginBlockRecord(ctx);

		int initStart = ctx->g_initCount;

		scopePush(ctx, &ctx->g_Scope);
		decl_list(ctx);
		stmt_list(ctx);
		attachDeclInits(ctx, initStart);
		scopePopKeep(ctx, &ctx->g_Scope);

		interpEndBlockRecord(ctx, prevBlock);

		if ( !match(ctx, T_KW_END) )
		{
			printf("Line %d Column %d: Statement error. Expected keyword 'end'.\n",
					ctx->g_Token.line, ctx->g_Token.column);
			ctx->g_numErrors++;
			if ( ctx->g_top >= 0 )
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
			return 0;
		}
		left = ctx->g_stack[ctx->g_top--];
		nodeType *blkNode = opr(ctx, T_CODE_BLOCK, 1, left);
		blkNode->opr.blockId = myBlockId;
		ctx->g_stack[++ctx->g_top] = blkNode;
		break;
	}

	case T_ID:
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) &&
		     funcFind(ctx, ctx->g_Token.Value.vString) >= 0 )
		{
			/* function call used as a statement */
			if ( !parseCall(ctx) )
				return 0;
			if ( !expectStmtSemicolon(ctx) )
			{
				freeNode(ctx, ctx->g_stack[ctx->g_top--]);
				return 0;
			}
			break;
		}
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) )
		{
			printf("Line %d Column %d: Statement error. Identifier '%s' not declared.\n",
					ctx->g_Token.line, ctx->g_Token.column, ctx->g_Token.Value.vString);
			ctx->g_numErrors++;
			return 0;
		}
		if ( strlen(ctx->g_Token.Value.vString) >= sizeof(szIdName) )
		{
			printf("Line %d Column %d: Statement error. Identifier too long.\n",
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
			printf("Line %d Column %d: Statement error. Identifier '%s' is not an array. Invalid bracket.\n",
					line, column, szIdName);
			ctx->g_numErrors++;
			return 0;
		}
		else if ( hashTable.Tipo0 == T_ARRAY && ctx->g_Token.Type != T_QOPAREN )
		{
			printf("Line %d Column %d: Statement error. Identifier '%s' is an array. Expected bracket.\n",
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
				printf("Line %d Column %d: Statement error. "
						"Mismatched number of indices.\n",
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
				printf("Line %d Column %d: Statement error. "
						"Arrays must be indexed with expressions of type int.\n",
						ctx->g_Token.line, ctx->g_Token.column);
				ctx->g_numErrors++;
				popArrayIndexNodes(ctx, currArrayIndex + 1);
				freeArrayDefWrapper(ctx, &value);
				return 0;
			}
			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Line %d Column %d: Statement error. Expected ']'.\n",
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
				printf("Line %d Column %d: Statement error. "
						"Mismatched number of indices.\n",
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
			printf("Line %d Column %d: Statement error. Expected '=' operator.\n",
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
		left = id(ctx, szIdName, hashTable.uniqueName, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, 1);

		if ( left->type1 != right->type1 )
		{
			if ( left->type1 == T_INTEGER && right->type1 == T_REAL )
			{
				printf("Line %d Column %d: Statement warning. "
						"Assigning a value of type real to variable '%s' of type int. "
						"Data loss may occur.\n",
						line, column, szIdName);
				ctx->g_numWarnings++;
			}
			else if ( left->type1 == T_REAL && right->type1 == T_INTEGER )
			{
				/* Promozione implicita int -> real consentita */
			}
			else if ( left->type1 == T_BOOLEAN && (right->type1 == T_INTEGER || right->type1 == T_REAL) )
			{
				/* Conversione implicita numerico -> bool consentita */
			}
			else
			{
				if ( left->type1 == T_INTEGER && right->type1 == T_BOOLEAN )
					printf("Line %d Column %d: Statement error. "
							"Cannot assign a boolean value to a variable of type int.\n",
							line, column);
				else if ( left->type1 == T_INTEGER && right->type1 == T_STRING )
					printf("Line %d Column %d: Statement error. "
							"Cannot assign a string value to a variable of type int.\n",
							line, column);
				else if ( left->type1 == T_REAL && right->type1 == T_BOOLEAN )
					printf("Line %d Column %d: Statement error. "
							"Cannot assign a boolean value to a variable of type real.\n",
							line, column);
				else if ( left->type1 == T_REAL && right->type1 == T_STRING )
					printf("Line %d Column %d: Statement error. "
							"Cannot assign a string value to a variable of type real.\n",
							line, column);
				else if ( left->type1 == T_BOOLEAN && right->type1 == T_STRING )
					printf("Line %d Column %d: Statement error. "
							"Cannot assign a string value to a variable of type bool.\n",
							line, column);
				else
					printf("Line %d Column %d: Statement error. "
							"Variables of type string can only be assigned values of type string.\n",
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
		printf("Line %d Column %d: Statement error. Expected 'print'"
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
			printf("Line %d Column %d: Expression error. Boolean operators "
					"do not accept string operands.\n",
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
			printf("Line %d Column %d: Expression error. Comparison operators "
					"do not accept string expressions.\n",
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
				printf("Line %d Column %d: Expression error. Subtraction operator "
						"accepts only integer or real operands.\n",
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
				printf("Line %d Column %d: Expression error. Operands of addition operator "
						"must be either both numeric or both string.\n",
						line, column);
				ctx->g_numErrors++;
				freeNode(ctx, left);
				freeNode(ctx, right);
				return 0;
			}
			else if ( left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
			{
				printf("Line %d Column %d: Expression error. Addition operator "
						"does not accept boolean operands.\n",
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
			printf("Line %d Column %d: Expression error. Multiplication/division operators "
					"accept only integer or real operands.\n",
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

static void freeCurrConstString(CompilerContext *ctx)
{
	if ( ctx->g_currConstType == T_STRING && ctx->g_currConstValue.strVal != NULL )
	{
		free(ctx->g_currConstValue.strVal);
		ctx->g_currConstValue.strVal = NULL;
	}
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
		freeCurrConstString(ctx);
		ctx->g_currConstType = T_INTEGER;
		ctx->g_currConstValue.iVal = ctx->g_Token.Value.vInt;

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_REAL_LITERAL:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.dblVal = ctx->g_Token.Value.vDouble;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_REAL, ctx->g_Token.lexeme, value);
		}
		freeCurrConstString(ctx);
		ctx->g_currConstType = T_REAL;
		ctx->g_currConstValue.dblVal = ctx->g_Token.Value.vDouble;

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_STRING_LITERAL:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.strVal = strdup(ctx->g_Token.Value.vString);
			if ( !value.strVal )
			{
				printf("Error: out of memory.\n");
				return 0;
			}
			ctx->g_stack[++ctx->g_top] = con(ctx, T_STRING, NULL, value);
		}
		else
		{
			freeCurrConstString(ctx);
			ctx->g_currConstType = T_STRING;
			ctx->g_currConstValue.strVal = strdup(ctx->g_Token.Value.vString);
			if ( !ctx->g_currConstValue.strVal )
			{
				printf("Error: out of memory.\n");
				return 0;
			}
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
		freeCurrConstString(ctx);
		ctx->g_currConstType = T_BOOLEAN;
		ctx->g_currConstValue.bVal = 1;

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_KW_FALSE:
		if ( ctx->g_isCodeBlock )
		{
			Valore value;
			value.bVal = 0;
			ctx->g_stack[++ctx->g_top] = con(ctx, T_BOOLEAN, "false", value);
		}
		freeCurrConstString(ctx);
		ctx->g_currConstType = T_BOOLEAN;
		ctx->g_currConstValue.bVal = 0;

		GetNextToken(ctx, &ctx->g_Token);
		break;
	case T_OPAREN:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( !match(ctx, T_CPAREN) )
		{
			printf("Line %d Column %d: Expression error. Expected ')'.\n",
					ctx->g_Token.line,
					ctx->g_Token.column);
			return 0;
		}
		break;
	case T_OP_MINUS:
		GetNextToken(ctx, &ctx->g_Token);
		if ( !expr(ctx) )
			return 0;
		if ( !ctx->g_isCodeBlock )
		{
			if ( ctx->g_currConstType == T_INTEGER )
			{
				ctx->g_currConstValue.iVal = -ctx->g_currConstValue.iVal;
				break;
			}
			else if ( ctx->g_currConstType == T_REAL )
			{
				ctx->g_currConstValue.dblVal = -ctx->g_currConstValue.dblVal;
				break;
			}
		}
		if ( ctx->g_stack[ctx->g_top]->type1 != T_INTEGER && ctx->g_stack[ctx->g_top]->type1 != T_REAL )
		{
			printf("Line %d Column %d: Expression error. Unary minus operator "
					"accepts only integer or real operands.\n",
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
			printf("Line %d Column %d: Expression error. Operator 'not' "
					"accepts only boolean expressions.\n",
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
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) &&
		     funcFind(ctx, ctx->g_Token.Value.vString) >= 0 )
		{
			int fIdx = funcFind(ctx, ctx->g_Token.Value.vString);
			if ( ctx->funcs[fIdx].isVoid )
			{
				printf("Line %d Column %d: Expression error. Void function '%s' does not return a value.\n",
						ctx->g_Token.line, ctx->g_Token.column, ctx->g_Token.Value.vString);
				ctx->g_numErrors++;
				return 0;
			}
			return parseCall(ctx);
		}
		if ( !scopeFind(ctx, &ctx->g_Scope, ctx->g_Token.Value.vString, &hashTable) )
		{
			printf("Line %d Column %d: Expression error. Identifier '%s' not declared.\n",
					ctx->g_Token.line,
					ctx->g_Token.column,
					ctx->g_Token.Value.vString);
			return 0;
		}
		if ( hashTable.Tipo0 == T_SIMPLE && !hashTable.bInizializzato )
		{
			if ( !ctx->g_isREAD )
			{
				printf("Line %d Column %d: Expression error. Identifier '%s' not initialized.\n",
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
				printf("Error pArrayDef: out of memory.\n");
				return 0;
			}
			value.pArrayDef->countIndexes = hashTable.Value.pArrayDef->countIndexes;
			value.pArrayDef->pArray = hashTable.Value.pArrayDef->pArray;
			value.pArrayDef->sizes = hashTable.Value.pArrayDef->sizes;
			value.pArrayDef->indexes = (nodeType**)malloc(sizeof(nodeType*)*(size_t)hashTable.Value.pArrayDef->countIndexes);
			if ( !value.pArrayDef->indexes )
			{
				printf("Error pArrayDef->indexes: out of memory.\n");
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

		ctx->g_stack[++ctx->g_top] = id(ctx, ctx->g_Token.Value.vString, hashTable.uniqueName, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, hashTable.bInizializzato);

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
				printf("Line %d Column %d: Statement error. "
						"Mismatched number of indices.\n",
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
				printf("Line %d Column %d: Statement error. "
						"Arrays must be indexed with expressions of type int.\n",
						ctx->g_Token.line,
						ctx->g_Token.column);
				cleanupArrayExprError(ctx, currArrayIndex + 1, &value, pNode);
				return 0;
			}

			if ( !match(ctx, T_QCPAREN) )
			{
				printf("Line %d Column %d: Expression error. Expected ']'.\n",
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
				printf("Line %d Column %d: Statement error. "
						"Mismatched number of indices.\n",
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
		printf("Line %d Column %d: Statement error. Expected 'int literal' or 'real literal'"
				", 'string literal', 'true', 'false', '(', '-', '!', or identifier.\n",
				ctx->g_Token.line,
				ctx->g_Token.column);
		return 0;
	}

	return 1;
}

