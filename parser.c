#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
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

static Token g_Token;

static int g_isCodeBlock = 0;
static int g_isStmtList = 0;
static int g_isREAD = 0;

static int g_numErrors = 0;
static int g_numWarnings = 0;

extern Scope g_Scope;
static int g_currType = 0;

static Valore g_currConstValue;
static int g_currConstType = -1;

static nodeType *g_stack[4096];
static int g_top = -1;

static int g_genTAC = 0;
static int g_genJVM = 0;

static int g_NextFrameIndex = 1;

void Panic()
{
	g_numErrors++;

	while ( g_Token.Type != T_SEMICOLON && g_Token.Type != T_KW_END && g_Token.Type != T_EOF )
	{
		GetNextToken(&g_Token);
	}

	if ( g_Token.Type == T_SEMICOLON || g_Token.Type == T_KW_END )
	{
		GetNextToken(&g_Token);
		SetDepth(0);
	}
}

int Parse(char *szFileName, nodeType **pTree, int genTAC, int genJVM)
{
	int ret;

	g_genTAC = genTAC;
	g_genJVM = genJVM;

	g_Token.Value.vString = NULL;

	if ( !LeggiFile(szFileName) )
		return 0;

	GetNextToken(&g_Token);

	while ( g_Token.Type != T_EOF )
	{
		if ( !g_isCodeBlock )
			ret = program();
		else
			ret = code_block();
			//ret = stmt_list();
		if ( !ret )
			Panic();
	}

	FreeInputbuffer();

	ret = g_numErrors ? 0 : 1;

	if ( g_numErrors || g_numWarnings )
		printf("Errori: %d\nWarnings: %d\n", g_numErrors, g_numWarnings);

	if ( !ret )
	{
		(*pTree) = NULL;
		while ( g_top >= 0 )
		{
			if ( g_stack[g_top] != NULL )
				freeNode(g_stack[g_top--]);
		}
	}
	else
	{
		(*pTree) = g_stack[g_top];
	}

	return ret;
}

/*
int Parse()
{
	GetNextToken(&g_Token);

	return syntax();

	//if ( g_Token.Type != T_EOF )
	//{
	//	printf("Errore\n");
	//	return;
	//}
}
*/

int match(TokenTypeEnum ExpectedToken)
{
	if ( g_Token.Type == ExpectedToken )
	{
		GetNextToken(&g_Token);
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


int TypeCheck(Valore *value, const Token *pToken)
{
	if ( g_currType == g_currConstType )
	{
		(*value) = g_currConstValue;

		//if ( g_currType == T_BOOLEAN )
		//{
		//	printf("Valore   -> %d\n", g_currConstValue.bVal);
		//	printf("Valore 2 -> %d\n", g_currConstValue.iVal);
		//}
	}
	else
	{
		switch ( g_currType )
		{
		case T_INTEGER:
			if ( g_currConstType == T_REAL )
			{
				printf("Linea %d Colonna %d: "
						"Warning var_list. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' "
						"di tipo int. Potrebbe verificarsi una perdita dei dati.\n",
						pToken->line,
						pToken->column,
						pToken->Value.vString);
				g_numWarnings++;
				(*value).iVal = (int)g_currConstValue.dblVal;
			}
			else if ( g_currConstType == T_BOOLEAN )
			{
				if ( g_currConstValue.bVal )
					(*value).iVal = 1;
				else
					(*value).iVal = 0;
			}
			else if ( g_currConstType == T_STRING )
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
			if ( g_currConstType == T_INTEGER )
			{
				(*value).dblVal = (double)g_currConstValue.iVal;
			}
			else if ( g_currConstType == T_BOOLEAN )
			{
				if ( g_currConstValue.bVal )
					(*value).iVal = 1;
				else
					(*value).iVal = 0;
			}
			else if ( g_currConstType == T_STRING )
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
			if ( g_currConstType == T_INTEGER )
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
			else if ( g_currConstType == T_REAL )
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
			else if ( g_currConstType == T_STRING )
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
			if ( g_currConstType == T_INTEGER )
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
			else if ( g_currConstType == T_REAL )
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
			else if ( g_currConstType == T_BOOLEAN )
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
int program()
{
	if ( !decl_list() )
		return 0;

	if ( !code_block() )
		return 0;

	return 1;
}

//decl_list : {decl};
int decl_list()
{
	while ( g_Token.Type == T_KW_ARRAY ||
		g_Token.Type == T_KW_INT ||
		g_Token.Type == T_KW_REAL ||
		g_Token.Type == T_KW_BOOL ||
		g_Token.Type == T_KW_STRING )
		if ( !decl() )
			return 0;

	return 1;
}

//decl : type var_list ';'
//		| T_KW_ARRAY T_ID '[' T_INT_LITERAL ']' {'[' T_INT_LITERAL ']'} T_KW_OF type ';'
//		;
int decl()
{
	if ( g_Token.Type == T_KW_ARRAY )
	{
		Valore value;
		Token token;
		HashTable hashTable;
		int indici[89];
		int currIndex = 0;
		int array_size = 0;

		//int *pTemp;

		value.pArrayDef = (ArrayDef*)malloc(sizeof(ArrayDef));
		if ( !value.pArrayDef )
		{
			printf("Errore: memoria insufficiente.\n");
			return 0;
		}

		value.pArrayDef->countIndexes = 0;

		//if ( !match(T_KW_ARRAY) )
		//	return 0;
		GetNextToken(&g_Token);

		token = g_Token;

		if ( !match(T_ID) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso identificatore.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}
		if ( scopeFind(&g_Scope,token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
					token.line,
					token.column,
					token.Value.vString);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}

		if ( !match(T_QOPAREN) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa parentesi '['.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}

		if ( g_Token.Type == T_INT_LITERAL )
		{
			indici[value.pArrayDef->countIndexes++] = g_Token.Value.vInt;
			array_size = g_Token.Value.vInt;
		}

		if ( !match(T_INT_LITERAL) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso int literal.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}
		if ( !match(T_QCPAREN) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa parentesi ']'.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}
		while ( g_Token.Type == T_QOPAREN )
		{
			if ( !match(T_QOPAREN) )
			{
				printf("Linea %d Colonna %d: Errore decl. Attesa parentesi '['.\n",
					g_Token.line,
					g_Token.column);
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}

			if ( g_Token.Type == T_INT_LITERAL )
			{
				indici[value.pArrayDef->countIndexes++] = g_Token.Value.vInt;
				array_size *= g_Token.Value.vInt;
			}

			if ( !match(T_INT_LITERAL) )
			{
				printf("Linea %d Colonna %d: Errore decl. Atteso int literal.\n",
					g_Token.line,
					g_Token.column);
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
			if ( !match(T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore decl. Attesa parentesi ']'.\n",
					g_Token.line,
					g_Token.column);
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
		}
		if ( !match(T_KW_OF) )
		{
			printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'of'.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}
		if ( !type() )
			return 0;

		if ( g_currType == T_INTEGER || g_currType == T_BOOLEAN )
		{
			value.pArrayDef->pArray = (int*)malloc(sizeof(int)*array_size);
			if ( !value.pArrayDef->pArray )
			{
				printf("Errore pArrayDef->pArray: memoria insufficiente.\n");
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
		}
		else if ( g_currType == T_REAL )
		{
			value.pArrayDef->pArray = (double*)malloc(sizeof(double)*array_size);
			if ( !value.pArrayDef->pArray )
			{
				printf("Errore pArrayDef->pArray: memoria insufficiente.\n");
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
		}
		else if ( g_currType == T_STRING )
		{
			value.pArrayDef->pArray = (char**)malloc(sizeof(char*)*array_size);
			if ( !value.pArrayDef->pArray )
			{
				printf("Errore pArrayDef->pArray: memoria insufficiente.\n");
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}
		}
		memset(value.pArrayDef->pArray, 0, (size_t)array_size);

		if ( !match(T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso semicolon ';'.\n",
				g_Token.line,
				g_Token.column);
			free(value.pArrayDef->pArray);
			value.pArrayDef->pArray = NULL;
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}

		value.pArrayDef->sizes = (int*)malloc(sizeof(int)*value.pArrayDef->countIndexes);
		if ( !value.pArrayDef->sizes )
		{
			printf("Errore pArrayDef->indexes: memoria insufficiente.\n");
			free(value.pArrayDef->pArray);
			value.pArrayDef->pArray = NULL;
			free(value.pArrayDef);
			value.pArrayDef = NULL;
			return 0;
		}
		memset(value.pArrayDef->sizes, 0, (size_t)value.pArrayDef->countIndexes);

		for ( currIndex = 0; currIndex < value.pArrayDef->countIndexes; currIndex++ )
			value.pArrayDef->sizes[currIndex] = indici[currIndex];

		/*
		pTemp = (int*)value.pArrayDef->pArray;
		pTemp[0] = 1;
		printf("%s -> [0] : %d\n", token.Value.vString, pTemp[0]);
		pTemp[1] = 2;
		printf("%s -> [1] : %d\n", token.Value.vString, pTemp[1]);
		pTemp[2] = 3;
		printf("%s -> [2] : %d\n", token.Value.vString, pTemp[2]);
		pTemp[3] = 4;
		printf("%s -> [3] : %d\n", token.Value.vString, pTemp[3]);
		*/

		value.pArrayDef->indexes = NULL;
		scopeInsert(&g_Scope, token.Value.vString, T_ARRAY, g_currType, T_VAR, value, 0, g_NextFrameIndex);
		if ( g_genJVM )
		{
			g_NextFrameIndex++;
			jvmSetNumberLocals(g_NextFrameIndex);
		}
	}
	else
	{
		if ( !type() )
			return 0;
		if ( !var_list() )
			return 0;
		if ( !match(T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore decl. Atteso semicolon ';'.\n",
				g_Token.line,
				g_Token.column);
			return 0;
		}
	}

	return 1;
}

//type : T_KW_INT | T_KW_REAL | T_KW_BOOL | T_KW_STRING;
int type()
{
	switch ( g_Token.Type )
	{
	case T_KW_INT:
		g_currType = T_INTEGER;
		GetNextToken(&g_Token);
		return 1;
	case T_KW_REAL:
		g_currType = T_REAL;
		GetNextToken(&g_Token);
		return 1;
	case T_KW_BOOL:
		g_currType = T_BOOLEAN;
		GetNextToken(&g_Token);
		return 1;
	case T_KW_STRING:
		g_currType = T_STRING;
		GetNextToken(&g_Token);
		return 1;
		//break;
	default:
		printf("Linea %d Colonna %d: Errore type. Attesa keyword 'int' o 'real' o 'bool' o 'string'.\n", g_Token.line, g_Token.column);
		return 0;
		//break;
	}

	//return 0;
}

//var_list : T_ID ['=' expr] {',' T_ID['=' expr]};
int var_list()
{
	Token token = g_Token;
	Valore value;
	HashTable hashTable;
	char szVarName[256];
	char szLexeme[256];

	if ( g_Token.Type == T_ID )
		strcpy(szVarName, g_Token.Value.vString);

	if ( !match(T_ID) )
	{
		printf("Linea %d Colonna %d: Errore var_list. Atteso identificatore.\n",
				g_Token.line,
				g_Token.column);
		return 0;
	}
	if ( scopeFind(&g_Scope,token.Value.vString, &hashTable) )
	{
		printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
				token.line,
				token.column,
				token.Value.vString);
		return 0;
	}
	if ( g_currType == T_INTEGER )
		value.iVal = 0;
	else if ( g_currType == T_REAL )
		value.dblVal = 0;
	else if ( g_currType == T_STRING )
		value.strVal = NULL;
	else
		value.bVal = 0;

	scopeInsert(&g_Scope, token.Value.vString, T_SIMPLE, g_currType, T_VAR, value, 0, g_NextFrameIndex);
	if ( g_genJVM )
	{
		if ( g_currType == T_REAL )
			g_NextFrameIndex += 2;
		else
			g_NextFrameIndex++;
		jvmSetNumberLocals(g_NextFrameIndex);
	}

	if ( g_Token.Type == T_OP_ASSIGN )
	{
		//if ( !match(T_OP_ASSIGN) )
		//	return 0;
		GetNextToken(&g_Token);

		strcpy(szLexeme, g_Token.lexeme);

		if ( !expr() )
			return 0;

		if ( g_currConstType != -1 )
		{
			// controlla la consistenza dei tipi ed
			// eventualmente effettua un casting implicito
			if ( !TypeCheck(&value, &token) )
				return 0;

			if ( g_currConstType == T_BOOLEAN )
			{
				if ( strcmp(g_Token.Value.vString, "true") == 0 )
					value.bVal = 1;
				else
					value.bVal = 0;
			}

			//value = g_currConstValue;
			//scopeUpdateValue(&g_Scope, token.Value.vString, T_SIMPLE, g_currType, T_VAR, value, 1);
			scopeUpdateValue(&g_Scope, szVarName, T_SIMPLE, g_currType, T_VAR, value, 1, 1);

			if ( g_genTAC )
			{
				printf("%s = %s\n", szVarName, szLexeme);
			}
			else if ( g_genJVM )
			{
			}
		}
		else
		{
			printf("Linea %d Colonna %d: Errore var_list. In fase di dichiarazione è possibile assegnare solo valori costanti.\n",
					token.line,
					token.column);
			return 0;
		}
	}

	while ( g_Token.Type == T_COMMA )
	{
		//if ( !match(T_COMMA) )
		//	return 0;
		GetNextToken(&g_Token);

		token = g_Token;

		if ( g_Token.Type == T_ID )
			strcpy(szVarName, g_Token.Value.vString);

		if ( !match(T_ID) )
		{
			printf("Linea %d Colonna %d: Errore var_list. Atteso identificatore.\n",
				g_Token.line,
				g_Token.column);
			return 0;
		}
		if ( scopeFind(&g_Scope, token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore var_list. Identificatore '%s' già dichiarato.\n",
					token.line,
					token.column,
					token.Value.vString);
			return 0;
		}
		//scopeInsert(&g_Scope, token.Value.vString, T_SIMPLE, g_currType, T_VAR, value, 0);
		scopeInsert(&g_Scope, szVarName, T_SIMPLE, g_currType, T_VAR, value, 0, g_NextFrameIndex);
		if ( g_genJVM )
		{
			if ( g_currType == T_REAL )
				g_NextFrameIndex += 2;
			else
				g_NextFrameIndex++;
			jvmSetNumberLocals(g_NextFrameIndex);
		}

		if ( g_Token.Type == T_OP_ASSIGN )
		{
			//if ( !match(T_OP_ASSIGN) )
			//	return 0;
			GetNextToken(&g_Token);

			strcpy(szLexeme, g_Token.lexeme);

			if ( !expr() )
				return 0;

			if ( g_currConstType != -1 )
			{
				// controlla qui la consistenza dei tipi ed
				// eventualmente effettua un casting implicito
				if ( !TypeCheck(&value, &token) )
					return 0;

				if ( g_currConstType == T_BOOLEAN )
				{
					if ( strcmp(g_Token.Value.vString, "true") == 0 )
						value.bVal = 1;
					else
						value.bVal = 0;
				}

				//value = g_currConstValue;
				//scopeUpdateValue(&g_Scope, token.Value.vString, T_SIMPLE, g_currType, T_VAR, value, 1);
				scopeUpdateValue(&g_Scope, szVarName, T_SIMPLE, g_currType, T_VAR, value, 1, 1);

				if ( g_genTAC )
				{
					printf("%s = %s\n", szVarName, szLexeme);
				}
				else if ( g_genJVM )
				{
				}
			}
			else
			{
				printf("Linea %d Colonna %d: Errore var_list. In fase di dichiarazione è possibile assegnare solo valori costanti.\n",
						token.line,
						token.column);
				return 0;
			}
		}
	}

	return 1;
}

//code_block : T_KW_BEGIN stmt_list T_KW_END;
int code_block()
{
	nodeType *left = NULL;

	g_isCodeBlock = 1;

	if ( !match(T_KW_BEGIN) && !g_isStmtList )
	{
		printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'begin'.\n",
				g_Token.line,
				g_Token.column);
		return 0;
	}
	if ( !stmt_list() )
	{
		return 0;
	}
	if ( !match(T_KW_END) )
	{
		printf("Linea %d Colonna %d: Errore decl. Attesa keyword 'end'.\n",
				g_Token.line,
				g_Token.column);
		return 0;
	}

	left = g_stack[g_top--];
	g_stack[++g_top] = opr(T_CODE_BLOCK, 1, left);

	return 1;
}

//stmt_list : {stmt};
int stmt_list()
{
	nodeType *left, *right, *root;

	left = right = root = NULL;

	g_isStmtList = 1;

	root = opr(T_STMT_LIST, 2, NULL, NULL);
	g_stack[++g_top] = root;

	while ( g_Token.Type == T_KW_PRINT ||
		g_Token.Type == T_KW_READ ||
		g_Token.Type == T_KW_WHILE ||
		g_Token.Type == T_KW_DO ||
		g_Token.Type == T_KW_IF ||
		g_Token.Type == T_KW_BEGIN ||
		g_Token.Type == T_ID )
	{
		if ( !stmt() )
		{
			//g_isStmtList = 0;
			return 0;
		}

		right = g_stack[g_top--];
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_STMT_LIST, 2, left, right);
	} // fine while

	g_isStmtList = 0;

	return 1;
}

//stmt : T_KW_PRINT expr ';'
//		| T_KW_READ expr ';'
//		| T_KW_WHILE '(' expr ')' stmt
//		| T_KW_DO stmt T_KW_WHILE '(' expr ')'
//		| T_KW_IF '(' expr ')' stmt [T_KW_ELSE stmt]
//		| T_KW_BEGIN stmt_list T_KW_END
//		| T_ID {'[' expr ']'} '=' expr ';'
//		;
int stmt()
{
	nodeType *left, *right, *elseNode;
	HashTable hashTable;
	Valore value;
	char szIdName[256];
	int line, column;
	int currArrayIndex;

	left = right = elseNode = NULL;

	switch ( g_Token.Type )
	{
	case T_KW_PRINT:
		//if ( !match(T_KW_PRINT) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !expr() )
			return 0;
		if ( !match(T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Atteso semicolon ';'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_KW_PRINT, 1, left);
		break;
	case T_KW_READ:
		//if ( !match(T_KW_READ) )
		//	return 0;
		GetNextToken(&g_Token);
		g_isREAD = 1;
		if ( !expr() )
		{
			g_isREAD = 0;
			return 0;
		}
		if ( !match(T_SEMICOLON) )
		{
			g_isREAD = 0;
			printf("Linea %d Colonna %d: Errore stmt. Atteso semicolon ';'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_KW_READ, 1, left);
		g_isREAD = 0;
		break;
	case T_KW_WHILE:
		//if ( !match(T_KW_WHILE) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !match(T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !expr() )
			return 0;
		if ( !match(T_CPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ')'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( g_stack[g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !stmt() )
			return 0;
		right = g_stack[g_top--];
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_KW_WHILE, 2, left, right);
		break;
	case T_KW_DO:
		//if ( !match(T_KW_DO) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !stmt() )
			return 0;
		if ( !match(T_KW_WHILE) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa keyword 'while'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !match(T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !expr() )
			return 0;
		if ( !match(T_CPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ')'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( g_stack[g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		right = g_stack[g_top--];
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_KW_DO, 2, left, right);
		break;
	case T_KW_IF:
		//if ( !match(T_KW_IF) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !match(T_OPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi '('.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !expr() )
			return 0;
		if ( !match(T_CPAREN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ')'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( g_stack[g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore stmt. L' espressione tra "
					"parentesi tonde deve restituire un valore booleano.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		if ( !stmt() )
			return 0;
		elseNode = NULL;
		if ( g_Token.Type == T_KW_ELSE )
		{
			//if ( !match(T_KW_ELSE) )
			//	return 0;
			GetNextToken(&g_Token);
			if ( !stmt() )
				return 0;
			elseNode = g_stack[g_top--];
		}
		right = g_stack[g_top--];
		left = g_stack[g_top--];
		if ( elseNode )
			g_stack[++g_top] = opr(T_KW_IF, 3, left, right, elseNode);
		else
			g_stack[++g_top] = opr(T_KW_IF, 2, left, right);
		break;
	case T_KW_BEGIN:
		//if ( !match(T_KW_BEGIN) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !stmt_list() )
			return 0;
		if ( !match(T_KW_END) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Attesa keyword 'end'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		left = g_stack[g_top--];
		g_stack[++g_top] = opr(T_STMT_LIST, 1, left);
		break;
	case T_ID:
		if ( !scopeFind(&g_Scope, g_Token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Identificatore '%s' non dichiarato.\n",
					g_Token.line,
					g_Token.column,
					g_Token.Value.vString);
			return 0;
		}
		strcpy(szIdName, g_Token.Value.vString);

		line = g_Token.line;
		column = g_Token.column;
		//if ( !match(T_ID) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( hashTable.Tipo0 == T_SIMPLE && g_Token.Type == T_QOPAREN )
		{
			printf("Linea %d Colonna %d: Errore stmt. L'identificatore '%s' non e' un array. Parentesi quadra non valida.\n",
					line,
					column,
					szIdName);
			return 0;
		}
		else if ( hashTable.Tipo0 == T_ARRAY && g_Token.Type != T_QOPAREN )
		{
			printf("Linea %d Colonna %d: Errore stmt. L'identificatore '%s' e' un array. Attesa parentesi quadra.\n",
					line,
					column,
					szIdName);
			return 0;
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
			value.pArrayDef->indexes = (nodeType**)malloc(sizeof(nodeType*)*hashTable.Value.pArrayDef->countIndexes);
			if ( !value.pArrayDef->indexes )
			{
				printf("Errore pArrayDef->indexes: memoria insufficiente.\n");
				return 0;
			}
			memset(value.pArrayDef->indexes, 0, (size_t)hashTable.Value.pArrayDef->countIndexes);
		}
		else
		{
			value = hashTable.Value;
		}

		currArrayIndex = 0;
		while ( g_Token.Type == T_QOPAREN )
		{
			if ( currArrayIndex > hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						g_Token.line,
						g_Token.column);
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				return 0;
			}

			//if ( !match(T_QOPAREN) )
			//	return 0;
			GetNextToken(&g_Token);
			if ( !expr() )
			{
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;

				return 0;
			}

			if ( g_stack[g_top]->type1 != T_INTEGER )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Gli array vanno indicizzati con espressioni di tipo int.\n",
						g_Token.line,
						g_Token.column);
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;

				return 0;
			}
			if ( !match(T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore stmt. Attesa parentesi ']'.\n",
						g_Token.line,
						g_Token.column);
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;

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
						g_Token.line,
						g_Token.column);
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;

				return 0;
			}

			for ( currArrayIndex = hashTable.Value.pArrayDef->countIndexes - 1; currArrayIndex >= 0 ; currArrayIndex-- )
			{
				value.pArrayDef->indexes[currArrayIndex] = g_stack[g_top--];
			}

			/*
			printf("%s", szIdName);
			for ( currArrayIndex = 0; currArrayIndex < hashTable.Value.pArrayDef->countIndexes; currArrayIndex++ )
			{
				printf("[%d]", value.pArrayDef->indexes[currArrayIndex]->con.value.iVal);
			}
			printf("\n");
			*/
		}

		if ( !match(T_OP_ASSIGN) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Atteso operatore '='.\n",
					g_Token.line,
					g_Token.column);

			if ( hashTable.Tipo0 == T_ARRAY )
			{
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
					freeNode(value.pArrayDef->indexes[currArrayIndex--]);
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
			}

			return 0;
		}
		if ( !expr() )
		{
			if ( hashTable.Tipo0 == T_ARRAY )
			{
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
					freeNode(value.pArrayDef->indexes[currArrayIndex--]);
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
			}

			return 0;
		}
		if ( !match(T_SEMICOLON) )
		{
			printf("Linea %d Colonna %d: Errore stmt. Atteso semicolon ';'.\n",
					g_Token.line,
					g_Token.column);

			if ( hashTable.Tipo0 == T_ARRAY )
			{
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
					freeNode(value.pArrayDef->indexes[currArrayIndex--]);
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
			}

			return 0;
		}
		right = g_stack[g_top--];
		//if ( hashTable.Tipo0 != T_ARRAY )
		//	left = id(szIdName, "a", hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, hashTable.Value, 1);
		left = id(szIdName, "a", hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, 1);
		scopeUpdateValue(&g_Scope, szIdName, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, 1, -1);

		if ( left->type1 != right->type1 )
		{
			if ( left->type1 == T_INTEGER && right->type1 == T_REAL )
			{
				printf("Linea %d Colonna %d: Warning stmt. "
						"Si sta assegnando un valore di tipo real alla variabile '%s' di tipo int. "
						"Potrebbe verificarsi una perdita di dati.\n",
						line,
						column,
						szIdName);
				g_numWarnings++;
			}
			else if ( left->type1 == T_INTEGER && right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Non si puo' assegnare una valore booleano ad una variabile di tipo int.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
			else if ( left->type1 == T_INTEGER && right->type1 == T_STRING )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Non si puo' assegnare una valore di tipo string ad una variabile di tipo int.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
			else if ( left->type1 == T_REAL && right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Non si puo' assegnare una valore booleano ad una variabile di tipo real.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
			else if ( left->type1 == T_REAL && right->type1 == T_STRING )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Non si puo' assegnare una valore di tipo string ad una variabile di tipo real.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
			else if ( left->type1 == T_BOOLEAN && right->type1 == T_STRING )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Non si puo' assegnare una valore di tipo string ad una variabile di tipo bool.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
			else if ( left->type1 == T_STRING )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Alle variabili di tipo string e' possibile assegnare soltanto valori di tipo string.\n",
						line,
						column);

				if ( hashTable.Tipo0 == T_ARRAY )
				{
					currArrayIndex--;
					while ( currArrayIndex >= 0 )
						freeNode(value.pArrayDef->indexes[currArrayIndex--]);
					free(value.pArrayDef->indexes);
					value.pArrayDef->indexes = NULL;
					free(value.pArrayDef);
					value.pArrayDef = NULL;
				}
				freeNode(left);
				freeNode(right);

				return 0;
			}
		}
		g_stack[++g_top] = opr(T_OP_ASSIGN, 2, left, right);

		/*
		printf("%s", g_stack[g_top]->opr.op[0]->id.name);
		for ( currArrayIndex = 0; currArrayIndex < hashTable.Value.pArrayDef->countIndexes; currArrayIndex++ )
		{
			printf("[%d]", g_stack[g_top]->opr.op[0]->id.value.pArrayDef->indexes[currArrayIndex]->con.value.iVal);
		}
		printf(" = %d\n\n", g_stack[g_top]->opr.op[1]->con.value.iVal);
		*/

		break;
	default:
		printf("Linea %d Colonna %d: Errore stmt. Attesa 'print'."
				" o 'read' o 'while' o 'do' o 'if' o 'begin' o identificatore.\n",
				g_Token.line,
				g_Token.column);
		return 0;
	}

	return 1;
}

//expr  : expr1 {('&' | '|') expr1};
int expr()
{
	g_currConstType = -1;

	if ( !expr1() )
		return 0;

	while ( g_Token.Type == T_OP_AND || g_Token.Type == T_OP_OR )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = g_Token.line;
		column = g_Token.column;

		oper = g_Token.Type;

		/*
		if ( g_Token.Type == T_OP_AND )
			if ( !match(T_OP_AND) )
				return 0;
		else if ( g_Token.Type == T_OP_OR )
			if ( !match(T_OP_OR) )
				return 0;
		*/
		GetNextToken(&g_Token);

		if ( !expr1() )
			return 0;

		right = g_stack[g_top--];
		left  = g_stack[g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr4. Gli operatori booleani "
					"non accettano operandi di tipo stringa.\n",
					line,
					column);
			return 0;
		}
		g_stack[++g_top] = opr(oper, 2, left, right);
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			g_stack[g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			g_stack[g_top]->type1 = T_INTEGER;
		else
			g_stack[g_top]->type1 = T_BOOLEAN;
	}

	return 1;
}

//expr1 : expr2 {relop expr2};
int expr1()
{
	/*
	T_OP_LT,		// '<'
	T_OP_LE,		// '<='
	T_OP_EQ,		// '=='
	T_OP_NE,		// '!='
	T_OP_GT,		// '>'
	T_OP_GE,		// '>='
	*/

	if ( !expr2() )
		return 0;

	while ( g_Token.Type == T_OP_LT || g_Token.Type == T_OP_LE ||
			g_Token.Type == T_OP_EQ || g_Token.Type == T_OP_NE ||
			g_Token.Type == T_OP_GT || g_Token.Type == T_OP_GE )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = g_Token.line;
		column = g_Token.column;

		oper = g_Token.Type;

		/*
		if ( g_Token.Type == T_OP_LT )
			if ( !match(T_OP_LT) )
				return 0;
		else if ( g_Token.Type == T_OP_LE )
			if ( !match(T_OP_LE) )
				return 0;
		else if ( g_Token.Type == T_OP_EQ )
			if ( !match(T_OP_EQ) )
				return 0;
		else if ( g_Token.Type == T_OP_NE )
			if ( !match(T_OP_NE) )
				return 0;
		else if ( g_Token.Type == T_OP_GT )
			if ( !match(T_OP_GT) )
				return 0;
		else if ( g_Token.Type == T_OP_GE )
			if ( !match(T_OP_GE) )
				return 0;
		*/
		GetNextToken(&g_Token);

		if ( !expr2() )
			return 0;

		right = g_stack[g_top--];
		left  = g_stack[g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr4. Gli operatori di confronto "
					"non accettano espressioni di tipo stringa.\n",
					line,
					column);
			return 0;
		}
		g_stack[++g_top] = opr(oper, 2, left, right);
		g_stack[g_top]->type0 = T_SIMPLE;
		g_stack[g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			g_stack[g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			g_stack[g_top]->type1 = T_INTEGER;
		else
			g_stack[g_top]->type1 = T_BOOLEAN;
	}

	return 1;
}

//expr2 : expr3 {addop expr3};
int expr2()
{
	if ( !expr3() )
		return 0;

	while ( g_Token.Type == T_OP_PLUS || g_Token.Type == T_OP_MINUS )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = g_Token.line;
		column = g_Token.column;

		oper = g_Token.Type;

		/*
		if ( g_Token.Type == T_OP_PLUS )
			if ( !match(T_OP_PLUS) )
				return 0;
		else if ( g_Token.Type == T_OP_MINUS )
			if ( !match(T_OP_MINUS) )
				return 0;
		*/
		GetNextToken(&g_Token);

		if ( !expr3() )
			return 0;

		right = g_stack[g_top--];
		left  = g_stack[g_top--];

		//printf("%d - %d = %d\n", left->con.value.iVal, right->con.value.iVal, left->con.value.iVal - right->con.value.iVal);

		if ( oper == T_OP_MINUS )
		{
			if ( left->type1 == T_STRING || right->type1 == T_STRING ||
				left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore expr4. L'operatore di sottrazione "
						"accetta solo operandi di tipo integer o real.\n",
						line,
						column);
				return 0;
			}
		}
		else // T_PLUS
		{
			if ( (left->type1 == T_STRING && right->type1 != T_STRING) ||
				 (left->type1 != T_STRING && right->type1 == T_STRING) )
			{
				printf("Linea %d Colonna %d: Errore expr4. Gli operandi dell'operatore di addizione "
						"devono essere o entrambi di tipo numerico o entrambi di tipo stringa.\n",
						line,
						column);
				return 0;
			}
			else if ( left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
			{
				printf("Linea %d Colonna %d: Errore expr4. L'operatore di addizione "
						"non accetta operandi di tipo boolean.\n",
						line,
						column);
				return 0;
			}
		}
		g_stack[++g_top] = opr(oper, 2, left, right);
		g_stack[g_top]->type0 = T_SIMPLE;
		g_stack[g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			g_stack[g_top]->type1 = T_REAL;
		else if ( left->type1 == T_INTEGER || right->type1 == T_INTEGER )
			g_stack[g_top]->type1 = T_INTEGER;
		else
			g_stack[g_top]->type1 = T_STRING;
	}

	return 1;
}

//expr3 : expr4 {mulop expr4};
int expr3()
{
	if ( !expr4() )
		return 0;

	while ( g_Token.Type == T_OP_MULT || g_Token.Type == T_OP_DIV )
	{
		int oper;
		nodeType *left, *right;
		int line, column;

		line = g_Token.line;
		column = g_Token.column;

		oper = g_Token.Type;

		/*
		if ( g_Token.Type == T_OP_MULT )
			if ( !match(T_OP_MULT) )
				return 0;
		else if ( g_Token.Type == T_OP_DIV )
			if ( !match(T_OP_DIV) )
				return 0;
		*/
		GetNextToken(&g_Token);

		if ( !expr4() )
			return 0;

		right = g_stack[g_top--];
		left  = g_stack[g_top--];
		if ( left->type1 == T_STRING || right->type1 == T_STRING ||
			left->type1 == T_BOOLEAN || right->type1 == T_BOOLEAN )
		{
			printf("Linea %d Colonna %d: Errore expr4. Gli operatori di moltiplicazione/divisione "
					"accettano solo operandi di tipo integer o real.\n",
					line,
					column);
			return 0;
		}
		g_stack[++g_top] = opr(oper, 2, left, right);
		g_stack[g_top]->type0 = T_SIMPLE;
		g_stack[g_top]->type2 = T_VAR;
		if ( left->type1 == T_REAL || right->type1 == T_REAL )
			g_stack[g_top]->type1 = T_REAL;
		else
			g_stack[g_top]->type1 = T_INTEGER;

		// IN CASO DI ERRORE PROVARE COSI:
		/*
		right = g_stack[g_top--];
		left  = g_stack[g_top];
		g_stack[g_top] = opr(oper, 2, left, right);
		*/

	}

	return 1;
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
int expr4()
{
	HashTable hashTable;
	Valore value;
	nodeType *pNode = NULL;
	int currArrayIndex;

	switch( g_Token.Type )
	{
	case T_INT_LITERAL:
		if ( g_isCodeBlock )
		{
			Valore value;
			value.iVal = g_Token.Value.vInt;
			g_stack[++g_top] = con(T_INTEGER, g_Token.lexeme, value);
		}
		else
		{
			g_currConstType = T_INTEGER;
			g_currConstValue.iVal = g_Token.Value.vInt;
		}

		//if ( !match(T_INT_LITERAL) )
		//	return 0;
		GetNextToken(&g_Token);
		break;
	case T_REAL_LITERAL:
		if ( g_isCodeBlock )
		{
			Valore value;
			value.dblVal = g_Token.Value.vDouble;
			g_stack[++g_top] = con(T_REAL, g_Token.lexeme, value);
		}
		else
		{
			g_currConstType = T_REAL;
			g_currConstValue.dblVal = g_Token.Value.vDouble;
		}

		//if ( !match(T_REAL_LITERAL) )
		//	return 0;
		GetNextToken(&g_Token);
		break;
	case T_STRING_LITERAL:
		if ( g_isCodeBlock )
		{
			Valore value;
			value.strVal = (char*)malloc(sizeof(char)*strlen(g_Token.Value.vString)+1);
			if ( !value.strVal )
			{
				printf("Errore: memoria non sufficiente.\n");
				return 0;
			}
			strcpy(value.strVal, g_Token.Value.vString);
			g_stack[++g_top] = con(T_STRING, NULL, value);
		}
		else
		{
			g_currConstType = T_STRING;
			g_currConstValue.strVal = (char*)malloc(sizeof(char)*strlen(g_Token.Value.vString)+1);
			if ( !g_currConstValue.strVal )
			{
				printf("Errore: memoria non sufficiente.\n");
				return 0;
			}
			strcpy(g_currConstValue.strVal, g_Token.Value.vString);
		}

		//if ( !match(T_STRING_LITERAL) )
		//	return 0;
		GetNextToken(&g_Token);
		break;
	case T_KW_TRUE:
		if ( g_isCodeBlock )
		{
			Valore value;
			//value.bVal = TRUE;
			value.bVal = 1;
			g_stack[++g_top] = con(T_BOOLEAN, "true", value);
		}
		else
		{
			g_currConstType = T_BOOLEAN;
			g_currConstValue.bVal = g_Token.Value.vInt;
		}

		//if ( !match(T_KW_TRUE) )
		//	return 0;
		GetNextToken(&g_Token);
		break;
	case T_KW_FALSE:
		if ( g_isCodeBlock )
		{
			Valore value;
			//value.bVal = FALSE;
			value.bVal = 0;
			g_stack[++g_top] = con(T_BOOLEAN, "false", value);
		}
		else
		{
			g_currConstType = T_BOOLEAN;
			g_currConstValue.bVal = g_Token.Value.vInt;
		}

		//if ( !match(T_KW_FALSE) )
		//	return 0;
		GetNextToken(&g_Token);
		break;
	case T_OPAREN:
		//if ( !match(T_OPAREN) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !expr() )
			return 0;
		if ( !match(T_CPAREN) )
		{
			printf("Linea %d Colonna %d: Errore expr4. Attesa parentesi ')'.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		break;
	case T_OP_MINUS:
		//if ( !match(T_OP_MINUS) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !expr() )
			return 0;
		if ( g_stack[g_top]->type1 != T_INTEGER && g_stack[g_top]->type1 != T_REAL )
		{
			printf("Linea %d Colonna %d: Errore expr4. L'operatore meno unario "
					"accetta solo operandi di tipo integer o real.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		pNode = g_stack[g_top--];
		g_stack[++g_top] = opr(T_OP_UMINUS, 1, pNode);
		break;
	case T_OP_NOT:
		//if ( !match(T_OP_NOT) )
		//	return 0;
		GetNextToken(&g_Token);
		if ( !expr() )
			return 0;
		if ( g_stack[g_top]->type1 == T_STRING )
		{
			printf("Linea %d Colonna %d: Errore expr4. L'operatore not "
					"accetta solo espressioni di tipo boolean.\n",
					g_Token.line,
					g_Token.column);
			return 0;
		}
		pNode = g_stack[g_top--];
		g_stack[++g_top] = opr(T_OP_NOT, 1, pNode);
		break;
	case T_ID:
		if ( !scopeFind(&g_Scope, g_Token.Value.vString, &hashTable) )
		{
			printf("Linea %d Colonna %d: Errore expr4. Identificatore '%s' non dichiarato.\n",
					g_Token.line,
					g_Token.column,
					g_Token.Value.vString);
			return 0;
		}
		if ( hashTable.Tipo0 == T_SIMPLE && !hashTable.bInizializzato )
		{
			if ( !g_isREAD )
			{
				printf("Linea %d Colonna %d: Errore expr4. Identificatore '%s' non inizializzato.\n",
						g_Token.line,
						g_Token.column,
						g_Token.Value.vString);
				return 0;
			}
			else
			{
				hashTable.bInizializzato = 1;
				scopeUpdateValue(&g_Scope, hashTable.Key, hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, hashTable.Value, 1, -1);
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
			value.pArrayDef->indexes = (nodeType**)malloc(sizeof(nodeType*)*hashTable.Value.pArrayDef->countIndexes);
			if ( !value.pArrayDef->indexes )
			{
				printf("Errore pArrayDef->indexes: memoria insufficiente.\n");
				return 0;
			}
			memset(value.pArrayDef->indexes, 0, (size_t)hashTable.Value.pArrayDef->countIndexes);
		}
		else
		{
			value = hashTable.Value;
		}

		g_stack[++g_top] = id(g_Token.Value.vString, "a", hashTable.Tipo0, hashTable.Tipo1, hashTable.Tipo2, value, hashTable.bInizializzato);

		//if ( !match(T_ID) )
		//	return 0;
		GetNextToken(&g_Token);

		if ( g_Token.Type == T_QOPAREN )
		{
			pNode = g_stack[g_top--];
			currArrayIndex = 0;
		}
		while ( g_Token.Type == T_QOPAREN )
		{
			if ( currArrayIndex > hashTable.Value.pArrayDef->countIndexes )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Numero di indici non corrispondente.\n",
						g_Token.line,
						g_Token.column);

				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				freeNode(pNode);

				return 0;
			}

			//if ( !match(T_QOPAREN) )
			//	return 0;
			GetNextToken(&g_Token);
			if ( !expr() )
			{
				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				freeNode(pNode);

				return 0;
			}

			if ( g_stack[g_top]->type1 != T_INTEGER )
			{
				printf("Linea %d Colonna %d: Errore stmt. "
						"Gli array vanno indicizzati con espressioni di tipo int.\n",
						g_Token.line,
						g_Token.column);

				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				freeNode(pNode);

				return 0;
			}

			if ( !match(T_QCPAREN) )
			{
				printf("Linea %d Colonna %d: Errore expr4. Attesa parentesi ']'.\n",
						g_Token.line,
						g_Token.column);

				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				freeNode(pNode);

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
						g_Token.line,
						g_Token.column);

				currArrayIndex--;
				while ( currArrayIndex >= 0 )
				{
					freeNode(g_stack[g_top--]);
					currArrayIndex--;
				}
				free(value.pArrayDef->indexes);
				value.pArrayDef->indexes = NULL;
				free(value.pArrayDef);
				value.pArrayDef = NULL;
				freeNode(pNode);

				return 0;
			}

			for ( currArrayIndex = hashTable.Value.pArrayDef->countIndexes - 1; currArrayIndex >= 0 ; currArrayIndex-- )
			{
				pNode->id.value.pArrayDef->indexes[currArrayIndex] = g_stack[g_top--];
			}
			g_stack[++g_top] = pNode;

			/*
			printf("%s", g_stack[g_top]->id.name);
			for ( currArrayIndex = 0; currArrayIndex < hashTable.Value.pArrayDef->countIndexes; currArrayIndex++ )
			{
				printf("[%d]", g_stack[g_top]->id.value.pArrayDef->indexes[currArrayIndex]->con.value.iVal);
			}
			printf("\n\n");
			*/
		}

		break;
	default:
		printf("Linea %d Colonna %d: Errore stmt. Atteso 'int literal' o 'real literal'"
				" o 'string literal' o 'true' o 'false' o '(' o '-' o '!' o identificatore.\n",
				g_Token.line,
				g_Token.column);
		return 0;
	}

	return 1;
}

/*
	{
	}
*/
