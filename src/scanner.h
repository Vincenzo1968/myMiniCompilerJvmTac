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

#ifndef __MYSCANNER__
#define __MYSCANNER__


struct CompilerContext;
typedef struct CompilerContext CompilerContext;
typedef enum tagTokenType
{
	T_ERROR,
	T_UNKNOWN,
	T_EOF,
	T_ID,
	//T_CHAR_LITERAL,
	T_STRING_LITERAL,
	T_INT_LITERAL,
	T_REAL_LITERAL,
	T_OPAREN,		// '('
	T_CPAREN,		// ')'
	T_QOPAREN,		// '['
	T_QCPAREN,		// ']'
	//T_GOPAREN,		// '{'
	//T_GCPAREN,		// '}'
	//T_COLON,		// ':'
	T_SEMICOLON,	// ';'
	//T_DOT,			// '.'
	T_COMMA,		// ','
	T_KW_IF,
	T_KW_ELSE,
	T_KW_WHILE,
	T_KW_DO,
	T_KW_BEGIN,
	T_KW_END,
	T_KW_ARRAY,
	T_KW_OF,
	T_KW_STRING,
	T_KW_INT,
	T_KW_REAL,
	T_KW_BOOL,
	T_KW_FALSE,
	T_KW_TRUE,
	T_KW_PRINT,
	T_KW_READ,
	T_OP_ASSIGN,	// '='
	T_OP_AND,		// '&'
	T_OP_OR,		// '|'
	T_OP_NOT,		// '!'
	T_OP_UMINUS,	// '-'
	T_OP_LT,		// '<'
	T_OP_LE,		// '<='
	T_OP_EQ,		// '=='
	T_OP_NE,		// '!='
	T_OP_GT,		// '>'
	T_OP_GE,		// '>='
	T_OP_PLUS,		// '+'
	T_OP_MINUS,		// '-'
	T_OP_MULT,		// '*'
	T_OP_DIV,		// '/'
	T_STMT_LIST,
	T_CODE_BLOCK,
	/* funzioni (aggiunti in coda per non spostare i valori esistenti) */
	T_KW_VOID,
	T_KW_REF,
	T_KW_RETURN,
	T_FUNC_CALL		// nodo AST: op[0] = id con il nome, op[1..] = argomenti
}TokenTypeEnum;

typedef struct tagToken
{
	TokenTypeEnum Type;
	union tagValue
	{
		char vChar;
		int vInt;
		double vDouble;
		char *vString;
	} Value;

	char lexeme[128]; // usato solo per le costanti numeriche.

	int line;
	int column;
}Token;

typedef enum tagStates
{
	S0,
	S1,
	S2,
	S3,
	S4,
	S5,
	S6,
	S7,
	S8,
	S9,
	S10,
	S11,
	S12,
	S13,
	S14,
	S15,
	S16,
	S17,
	S18,
	S19,
	S20,
	S21
} States;

int GetDepth(CompilerContext *ctx);
void SetDepth(CompilerContext *ctx, int depth);
int LeggiFile(CompilerContext *ctx, char *szFileName);
void FreeInputbuffer(CompilerContext *ctx);

void GetNextToken(CompilerContext *ctx, Token *pToken);
/* Guarda avanti senza consumare: con il token corrente = tipo (o 'void'), vero se
   seguono T_ID e '(' , cioe' se si tratta del prototipo/definizione di una funzione. */
int PeekIsFunctionDecl(CompilerContext *ctx);

#endif /* __MYSCANNER__ */
