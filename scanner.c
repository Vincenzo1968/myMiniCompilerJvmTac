#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "scanner.h"

//#pragma warning(disable : 4996)

static char *g_szInput = NULL;

static int g_pos = 0;

static int g_line = 1;
static int g_column = 1;

static int g_depth = 0;

int GetDepth()
{
	return g_depth;
}

void SetDepth(int depth)
{
	g_depth = depth;
}

int LeggiFile(char *szFileName)
{
	FILE *fp = NULL;
	int dimFile = 0;
	int bytereads = 0;

	if ( g_szInput != NULL )
	{
		free(g_szInput);
		g_szInput = NULL;
	}

	fp = fopen(szFileName, "rb");
	if ( fp == NULL )
	{
		printf("Errore nell'apertura del file %s.\n", szFileName);
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

	g_szInput = (char*)malloc(sizeof(char)*dimFile + 1);
	if ( !g_szInput )
	{
		printf("Errore: memoria insufficiente.\n");
		fclose(fp);
		return 0;
	}

	fseek(fp, 0, SEEK_SET);

	bytereads = fread(g_szInput, 1, dimFile, fp);
	*(g_szInput + bytereads) = '\0';

	fclose(fp);

	return 1;
}

void FreeInputbuffer()
{
	if ( g_szInput != NULL )
	{
		free(g_szInput);
		g_szInput = NULL;
	}
}

void GetNextToken(Token *pToken)
{
	States state = S0;
	char c;
	char lexeme[4096];
	int k = 0;

	if ( g_szInput == NULL )
	{
		printf("Errore scanner: NULL Input\n");
		pToken->Type = T_ERROR;
		return;
	}

	pToken->Value.vString = NULL;

	c = *(g_szInput + g_pos);

	while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
	{
		if ( c == '\n' )
		{
			g_line++;
			g_column = 1;
		}
		g_pos++;
		c = *(g_szInput + g_pos);
	}

	if ( c == '\0' )
	{
		pToken->Type = T_EOF;
		FreeInputbuffer();
		return;
	}

	while ( 1 )
	{
		c = *(g_szInput + g_pos++);

		switch(state)
		{
		case S0:
			k = 0;
			pToken->line = g_line;
			pToken->column = g_column;

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
				FreeInputbuffer();
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
			else if ( c == '_' )
			{
				lexeme[k++] = c;
			}
			else
			{
				lexeme[k] = '\0';
				pToken->Type = T_ID;
				pToken->line = g_line;
				pToken->column = g_column;
				if ( pToken->Value.vString != NULL )
				{
					free(pToken->Value.vString);
					pToken->Value.vString = NULL;
				}
				pToken->Value.vString = (char*)malloc(sizeof(char) * k + 1);
				if ( pToken->Value.vString != NULL )
					strcpy(pToken->Value.vString, lexeme);
				--g_pos;
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
				printf("Errore scanner: Linea %d Colonna %d: carattere non valido %c\nExpexted: newline, tab o barra rovesciata\n", pToken->line, pToken->column, c);
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
				printf("Errore scanner: Linea %d Colonna %d: carattere non valido %c\nExpected: apice di chiusura\n", pToken->line, pToken->column, c);
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
				g_pos--;
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
				g_pos--;
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
				g_pos--;
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
				g_pos--;
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
				g_line++;
				g_column = 1;

				pToken->Type = T_ERROR;
				printf("Errore scanner: Linea %d Colonna %d: La stringa deve terminare nella stessa riga.\n", pToken->line, pToken->column);
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
				g_line++;
				g_column = 1;

				pToken->Type = T_ERROR;
				printf("Errore scanner: Linea %d Colonna %d: La stringa deve terminare nella stessa riga.\n", pToken->line, pToken->column);

				return;
			}
			else
			{
				pToken->Type = T_ERROR;
				printf("Errore scanner: Linea %d Colonna %d: Carattere speciale '\\%c' non riconosciuto.\n", pToken->line, pToken->column, c);
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
				--g_pos;
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
				--g_pos;
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
				--g_pos;
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
				printf("Errore scanner: Linea %d Colonna %d: Formato numero reale non valido.\n", pToken->line, pToken->column);
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
				printf("Errore scanner: Linea %d Colonna %d: Formato numero reale non valido.\n", pToken->line, pToken->column);
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
				--g_pos;
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
				printf("Errore scanner: Linea %d Colonna %d: carattere '.' non valido.\n", pToken->line, pToken->column);
				--g_pos;
				return;
			}
			break;
		case S18:
			if ( c == '/' ) // Commento fino a fine riga
			{
				state = S19;
			}
			else if ( c == '*' ) // Commento fino a */
			{
				state = S20;
			}
			else
			{
				pToken->Type = T_OP_DIV;
				g_pos--;
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
						g_line++;
						g_column = 1;
					}
					g_pos++;
					c = *(g_szInput + g_pos);
				}

				if ( c == '\0' )
				{
					pToken->Type = T_EOF;
					FreeInputbuffer();
					return;
				}

				state = S0;
			}
			else if ( c == '\0' )
			{
				pToken->Type = T_EOF;
				FreeInputbuffer();
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
				printf("Errore scanner: Linea %d Colonna %d: attesi caratteri '*/' di fine commento.\nTrovato fine del file.\n", pToken->line, pToken->column);
				--g_pos;
				return;
			}
			break;
		case S21:
			if ( c == '/' )
			{
				c = *(g_szInput + g_pos);
				if ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
				{
					while ( c == ' ' || c == '\t' || c == '\r' || c == '\n' )
					{
						if ( c == '\n' )
						{
							g_line++;
							g_column = 1;
						}
						g_pos++;
						c = *(g_szInput + g_pos);
					}

					if ( c == '\0' )
					{
						pToken->Type = T_EOF;
						FreeInputbuffer();
						return;
					}
				}

				state = S0;
			}
			else if ( c == '\0' )
			{
				pToken->Type = T_ERROR;
				printf("Errore scanner: Linea %d Colonna %d: attesi caratteri '*/' di fine commento.\nTrovato fine del file.\n", pToken->line, pToken->column);
				--g_pos;
				return;
			}
			else
			{
				state = S20;
			}
			break;
		} /* fine switch */

		g_column++;
	} /* fine while */
}
