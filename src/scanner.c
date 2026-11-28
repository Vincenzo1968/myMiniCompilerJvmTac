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

#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "context.h"
#include "scanner.h"
#include "include.h"
#define printf diag_printf

int GetDepth(CompilerContext *ctx)
{
	return ctx->g_depth;
}

void SetDepth(CompilerContext *ctx, int depth)
{
	ctx->g_depth = depth;
}

int LeggiFile(CompilerContext *ctx, char *szFileName)
{
	FILE *fp = NULL;
	int dimFile = 0;
	int bytereads = 0;

	if ( ctx->g_szInput != NULL )
	{
		free(ctx->g_szInput);
		ctx->g_szInput = NULL;
	}

	fp = fopen(szFileName, "rb");
	if ( fp == NULL )
	{
		printf("Error opening file %s.\n", szFileName);
		return 0;
	}

	if ( fseek(fp, 0, SEEK_END) )
	{
		fclose(fp);
		return 0;
	}

	dimFile = ftell(fp);
	if ( dimFile <= 0 )
	{
		fclose(fp);
		return 0;
	}

	ctx->g_szInput = (char*)malloc(sizeof(char)*dimFile + 1);
	if ( !ctx->g_szInput )
	{
		printf("Error: out of memory.\n");
		fclose(fp);
		return 0;
	}

	fseek(fp, 0, SEEK_SET);

	bytereads = fread(ctx->g_szInput, 1, dimFile, fp);
	*(ctx->g_szInput + bytereads) = '\0';

	fclose(fp);

	return 1;
}

void FreeInputbuffer(CompilerContext *ctx)
{
	if ( ctx->g_szInput != NULL )
	{
		free(ctx->g_szInput);
		ctx->g_szInput = NULL;
	}
}

// True if the token owns a malloc-allocated Value.vString.
// Note: keywords (T_KW_*) also pass through state S1 and allocate vString.
static int tokenOwnsString(CompilerContext *ctx, TokenTypeEnum t)
{
	(void)ctx;
    switch (t)
    {
        case T_ID:
        case T_STRING_LITERAL:
        case T_KW_IF: case T_KW_ELSE: case T_KW_WHILE: case T_KW_DO:
        case T_KW_BEGIN: case T_KW_END: case T_KW_ARRAY: case T_KW_OF:
        case T_KW_STRING: case T_KW_INT: case T_KW_REAL: case T_KW_BOOL:
        case T_KW_FALSE: case T_KW_TRUE: case T_KW_PRINT: case T_KW_READ:
        case T_KW_VOID: case T_KW_REF: case T_KW_RETURN:
            return 1;
        default:
            return 0;   /* T_INT_LITERAL, T_REAL_LITERAL, T_UNKNOWN, T_EOF, ... */
    }
}

int PeekIsFunctionDecl(CompilerContext *ctx)
{
	/* saves scanner state, reads two tokens, and restores it */
	int savedPos = ctx->g_pos, savedLine = ctx->g_line, savedCol = ctx->g_column, savedDepth = ctx->g_depth;
	Token t;
	int res = 0;

	memset(&t, 0, sizeof(t));
	t.Type = T_UNKNOWN;
	GetNextToken(ctx, &t);
	if ( t.Type == T_ID )
	{
		GetNextToken(ctx, &t);
		res = (t.Type == T_OPAREN);
	}
	if ( tokenOwnsString(ctx, t.Type) && t.Value.vString != NULL )
		free(t.Value.vString);

	ctx->g_pos = savedPos;
	ctx->g_line = savedLine;
	ctx->g_column = savedCol;
	ctx->g_depth = savedDepth;
	return res;
}

void GetNextToken(CompilerContext *ctx, Token *pToken)
{
	States state = S0;
	char c;
	char lexeme[4096];
	int k = 0;

	if ( ctx->g_szInput == NULL )
	{
		printf("Scanner error: NULL input\n");
		pToken->Type = T_ERROR;
		return;
	}

	if ( tokenOwnsString(ctx, pToken->Type) && pToken->Value.vString != NULL )
		free(pToken->Value.vString);
	pToken->Value.vString = NULL;   // always zero out the union before filling it 

	c = *(ctx->g_szInput + ctx->g_pos);

	while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
	{
		if ( c == '\n' )
		{
			ctx->g_line++;
			ctx->g_column = 1;
		}
		ctx->g_pos++;
		c = *(ctx->g_szInput + ctx->g_pos);
	}

	if ( c == '\0' )
	{
		pToken->Type = T_EOF;
		FreeInputbuffer(ctx);
		return;
	}

	while ( 1 )
	{
		c = *(ctx->g_szInput + ctx->g_pos++);

		switch(state)
		{
		case S0:
			k = 0;
			pToken->line = ctx->g_line;
			pToken->column = ctx->g_column;

			//if ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
			//{
			//	while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
			//	{
			//		g_pos++;
			//		c = *(szInput + g_pos);
			//	}
			//}

			if ( c == '\0' )
			{
				pToken->Type = T_EOF;
				FreeInputbuffer(ctx);
				return;
			}
			else if ( c == '(' )
			{
				pToken->Type = T_OPAREN;
				return;
			}
			else if ( c == ')' )
			{
				pToken->Type = T_CPAREN;
				return;
			}
			else if ( c == '[' )
			{
				pToken->Type = T_QOPAREN;
				return;
			}
			else if ( c == ']' )
			{
				pToken->Type = T_QCPAREN;
				return;
			}
			/*
			else if ( c == '{' )
			{
				pToken->Type = T_GOPAREN;
				return;
			}
			else if ( c == '}' )
			{
				pToken->Type = T_GCPAREN;
				return;
			}
			*/
			/*
			else if ( c == '|' )
			{
				if ( !g_depth )
					pToken->Type = T_VERTBAR;
				else
					pToken->Type = T_ALTERNATE;
				return;
			}
			else if ( c == ':' )
			{
				pToken->Type = T_COLON;
				return;
			}
			*/
			else if ( c == ',' )
			{
				pToken->Type = T_COMMA;
				return;
			}
			else if ( c == ';' )
			{
				pToken->Type = T_SEMICOLON;
				return;
			}
			else if ( c == '.' )
			{
				state = S17;
			}
			else if ( c == '<' )
			{
				state = S5;
			}
			else if ( c == '=' )
			{
				state = S6;
			}
			else if ( c == '>' )
			{
				state = S7;
			}
			else if ( c == '!' )
			{
				state = S8;
			}
			else if ( c == '&' )
			{
				pToken->Type = T_OP_AND;
				return;
			}
			else if ( c == '|' )
			{
				pToken->Type = T_OP_OR;
				return;
			}
			else if ( c == '+' )
			{
				pToken->Type = T_OP_PLUS;
				return;
			}
			else if ( c == '-' )
			{
				pToken->Type = T_OP_MINUS;
				return;
			}
			else if ( c == '*' )
			{
				pToken->Type = T_OP_MULT;
				return;
			}
			else if ( c == '/' )
			{
				//pToken->Type = T_OP_DIV;
				//return;
				state = S18;
			}
			else if ( c == '"' )
			{
				state = S9;
			}
			else if ( ((c >= 'a' && c <= 'z')) || ((c >= 'A') && (c <= 'Z')) )
			{
				lexeme[k++] = c;
				state = S1;
			}
			/*
			else if ( c == '\'' )
			{
				state = S2;
			}
			*/
			else if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
				state = S11;
			}
			else
			{
				pToken->Type = T_UNKNOWN;
				pToken->Value.vChar = c;
				return;
			}
			break;
			default:
				break;		
		case S1:
			if ( ((c >= 'a' && c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) )
			{
				lexeme[k++] = c;
			}
			else if ( c == '_' || c == '$' ) /* '$' appears only in generated names for struct fields */
			{
				lexeme[k++] = c;
			}
			else
			{
				lexeme[k] = '\0';
				pToken->Type = T_ID;
				pToken->line = ctx->g_line;
				pToken->column = ctx->g_column;
				if ( pToken->Value.vString != NULL )
				{
					free(pToken->Value.vString);
					pToken->Value.vString = NULL;
				}
				pToken->Value.vString = (char*)malloc(sizeof(char) * k + 1);
				if ( pToken->Value.vString != NULL )
					strcpy(pToken->Value.vString, lexeme);
				--ctx->g_pos;
				//state = S0;
				if ( strcmp(lexeme, "if") == 0 )
					pToken->Type = T_KW_IF;
				else if ( strcmp(lexeme, "else") == 0 )
					pToken->Type = T_KW_ELSE;
				else if ( strcmp(lexeme, "while") == 0 )
					pToken->Type = T_KW_WHILE;
				else if ( strcmp(lexeme, "do") == 0 )
					pToken->Type = T_KW_DO;
				else if ( strcmp(lexeme, "begin") == 0 )
					pToken->Type = T_KW_BEGIN;
				else if ( strcmp(lexeme, "end") == 0 )
					pToken->Type = T_KW_END;
				else if ( strcmp(lexeme, "array") == 0 )
					pToken->Type = T_KW_ARRAY;
				else if ( strcmp(lexeme, "of") == 0 )
					pToken->Type = T_KW_OF;
				else if ( strcmp(lexeme, "string") == 0 )
					pToken->Type = T_KW_STRING;
				else if ( strcmp(lexeme, "int") == 0 )
					pToken->Type = T_KW_INT;
				else if ( strcmp(lexeme, "real") == 0 )
					pToken->Type = T_KW_REAL;
				else if ( strcmp(lexeme, "bool") == 0 )
					pToken->Type = T_KW_BOOL;
				else if ( strcmp(lexeme, "false") == 0 )
					pToken->Type = T_KW_FALSE;
				else if ( strcmp(lexeme, "true") == 0 )
					pToken->Type = T_KW_TRUE;
				else if ( strcmp(lexeme, "void") == 0 )
					pToken->Type = T_KW_VOID;
				else if ( strcmp(lexeme, "ref") == 0 )
					pToken->Type = T_KW_REF;
				else if ( strcmp(lexeme, "return") == 0 )
					pToken->Type = T_KW_RETURN;
				else if ( strcmp(lexeme, "print") == 0 )
					pToken->Type = T_KW_PRINT;
				else if ( strcmp(lexeme, "read") == 0 )
					pToken->Type = T_KW_READ;

				return;
			}
			break;
		/*
		case S2:
			if ( c == '\\' )
			{
				state = S3;
			}
			else
			{
				if ( c == '\n' )
				{
					g_line++;
					g_column = 1;
				}

				lexeme[0] = c;
				state = S4;
			}
			break;
		case S3:
			if ( c == '\\' )
			{
				lexeme[0] = c;
				state = S4;
			}
			else if ( c == '\'' )
			{
				lexeme[0] = c;
				state = S4;
			}
			else if ( c == 'r' )
			{
				lexeme[0] = '\r';
				state = S4;
			}
			else if ( c == 'n' )
			{
				lexeme[0] = '\n';
				state = S4;
			}
			else if ( c == 't' )
			{
				lexeme[0] = '\t';
				state = S4;
			}
			else
			{
				if ( c == '\n' )
				{
					g_line++;
					g_column = 1;
				}

				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: invalid character %c\nExpected: newline, tab, or backslash\n", pToken->line, pToken->column, c);
				return;
			}
			break;
		case S4:
			if ( c == '\'' )
			{
				pToken->Type = T_CHAR;
				pToken->Value.vChar = lexeme[0];
				return;
			}
			else
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: invalid character %c\nExpected: closing single quote\n", pToken->line, pToken->column, c);
				return;
			}
			break;
		*/
		case S5:
			if ( c == '=' )
			{
				pToken->Type = T_OP_LE;
				return;
			}
			else
			{
				pToken->Type = T_OP_LT;
				ctx->g_pos--;
				return;
			}
			break;
		case S6:
			if ( c == '=' )
			{
				pToken->Type = T_OP_EQ;
				return;
			}
			else
			{
				pToken->Type = T_OP_ASSIGN;
				ctx->g_pos--;
				return;
			}
			break;
		case S7:
			if ( c == '=' )
			{
				pToken->Type = T_OP_GE;
				return;
			}
			else
			{
				pToken->Type = T_OP_GT;
				ctx->g_pos--;
				return;
			}
			break;
		case S8:
			if ( c == '=' )
			{
				pToken->Type = T_OP_NE;
				return;
			}
			else
			{
				pToken->Type = T_OP_NOT;
				ctx->g_pos--;
				return;
			}
			break;
		case S9:
			if ( c == '\\' )
			{
				state = S10;
			}
			else if ( c == '"' )
			{
				lexeme[k] = '\0';
				pToken->Type = T_STRING_LITERAL;
				if ( pToken->Value.vString != NULL )
				{
					free(pToken->Value.vString);
					pToken->Value.vString = NULL;
				}
				pToken->Value.vString = (char*)malloc(sizeof(char) * k + 1);
				if ( pToken->Value.vString != NULL )
					strcpy(pToken->Value.vString, lexeme);
				return;
			}
			else if ( c == '\n' )
			{
				ctx->g_line++;
				ctx->g_column = 1;

				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: String must terminate on the same line.\n", pToken->line, pToken->column);
				return;
			}
			else
			{
				lexeme[k++] = c;
			}
			break;
		case S10:
			if ( c == '"' )
			{
				lexeme[k++] = '"';
				state = S9;
			}
			else if ( c == '\\' )
			{
				lexeme[k++] = '\\';
				state = S9;
			}
			else if ( c == 't' )
			{
				lexeme[k++] = '\t';
				state = S9;
			}
			else if ( c == 'r' )
			{
				lexeme[k++] = '\r';
				state = S9;
			}
			else if ( c == 'n' )
			{
				lexeme[k++] = '\n';
				state = S9;
			}
			else if ( c == '\n' )
			{
				ctx->g_line++;
				ctx->g_column = 1;

				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: String must terminate on the same line.\n", pToken->line, pToken->column);

				return;
			}
			else
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: Unrecognized escape character '\\%c'.\n", pToken->line, pToken->column, c);
				return;
			}
			break;
		case S11:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
			}
			else if ( c == '.' )
			{
				lexeme[k++] = c;
				state = S12;
			}
			else if ( c == 'E' || c == 'e' )
			{
				lexeme[k++] = c;
				state = S14;
			}
			else
			{
				pToken->Type = T_INT_LITERAL;
				lexeme[k] = '\0';
				pToken->Value.vInt = atoi(lexeme);
				strcpy(pToken->lexeme, lexeme);
				--ctx->g_pos;
				return;
			}
			break;
		case S12:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
				state = S13;
			}
			else
			{
				pToken->Type = T_REAL_LITERAL;
				lexeme[k++] = '0';
				lexeme[k] = '\0';
				pToken->Value.vDouble = atof(lexeme);
				strcpy(pToken->lexeme, lexeme);
				--ctx->g_pos;
				return;
			}
			break;
		case S13:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
			}
			else if ( c == 'E' || c == 'e' )
			{
				lexeme[k++] = c;
				state = S14;
			}
			else
			{
				pToken->Type = T_REAL_LITERAL;
				lexeme[k++] = '0';
				lexeme[k] = '\0';
				pToken->Value.vDouble = atof(lexeme);
				strcpy(pToken->lexeme, lexeme);
				--ctx->g_pos;
				return;
			}
			break;
		case S14:
			if ( (c == '+') || (c == '-') )
			{
				lexeme[k++] = c;
				state = S15;
			}
			else if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
				state = S16;
			}
			else
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: Invalid real number format.\n", pToken->line, pToken->column);
				return;
			}
			break;
		case S15:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
				state = S16;
			}
			else
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: Invalid real number format.\n", pToken->line, pToken->column);
				return;
			}
			break;
		case S16:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = c;
				state = S16;
			}
			else
			{
				pToken->Type = T_REAL_LITERAL;
				lexeme[k] = '\0';
				pToken->Value.vDouble = atof(lexeme);
				strcpy(pToken->lexeme, lexeme);
				--ctx->g_pos;
				return;
			}
			break;
		case S17:
			if ( (c >= '0') && (c <= '9') )
			{
				lexeme[k++] = '0';
				lexeme[k++] = '.';
				lexeme[k++] = c;
				state = S13;
			}
			else
			{
				//pToken->Type = T_DOT;
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: invalid character '.'.\n", pToken->line, pToken->column);
				--ctx->g_pos;
				return;
			}
			break;
		case S18:
			if ( c == '/' ) // Comment to end of line
			{
				state = S19;
			}
			else if ( c == '*' ) // Comment up to */
			{
				state = S20;
			}
			else
			{
				pToken->Type = T_OP_DIV;
				ctx->g_pos--;
				return;
			}
			break;
		case S19:
			if ( c == '\r' || c == '\n' )
			{
				while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
				{
					if ( c == '\n' )
					{
						ctx->g_line++;
						ctx->g_column = 1;
					}
					ctx->g_pos++;
					c = *(ctx->g_szInput + ctx->g_pos);
				}

				if ( c == '\0' )
				{
					pToken->Type = T_EOF;
					FreeInputbuffer(ctx);
					return;
				}

				state = S0;
			}
			else if ( c == '\0' )
			{
				pToken->Type = T_EOF;
				FreeInputbuffer(ctx);
				return;
			}
			break;
		case S20:
			if ( c == '*' )
			{
				state = S21;
			}
			else if ( c == '\0' )
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: expected '*/' closing comment characters.\nFound end of file.\n", pToken->line, pToken->column);
				--ctx->g_pos;
				return;
			}
			break;
		case S21:
			if ( c == '/' )
			{
				c = *(ctx->g_szInput + ctx->g_pos);
				if ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
				{
					while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
					{
						if ( c == '\n' )
						{
							ctx->g_line++;
							ctx->g_column = 1;
						}
						ctx->g_pos++;
						c = *(ctx->g_szInput + ctx->g_pos);
					}

					if ( c == '\0' )
					{
						pToken->Type = T_EOF;
						FreeInputbuffer(ctx);
						return;
					}
				}

				state = S0;
			}
			else if ( c == '\0' )
			{
				pToken->Type = T_ERROR;
				printf("Scanner error: Line %d Column %d: expected '*/' closing comment characters.\nFound end of file.\n", pToken->line, pToken->column);
				--ctx->g_pos;
				return;
			}
			else
			{
				state = S20;
			}
			break;
		} /* end switch */

		ctx->g_column++;
	} /* end while */
}
