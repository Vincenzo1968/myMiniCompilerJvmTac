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

#include <stdio.h>
#include <string.h>
#include <malloc.h>

//#pragma warning(disable : 4996)

#include "symtab.h"


// -------------------------- HashTable -----------------------------------------

HashTable* htNewNode(char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex)
{
	HashTable *n = NULL;

	n = (HashTable*)malloc(sizeof(HashTable));

	if( n == NULL )
		return NULL;

	n->Key = (char*)malloc(strlen(Key)+1);
	if ( n->Key == NULL )
	{
		free(n);
		n = NULL;
		return NULL;
	}
	strcpy(n->Key, Key);
	n->Tipo0 = t0;
	n->Tipo1 = t1;
	n->Tipo2 = t2;
	n->Value = v;
	n->bInizializzato = bInizializzato;
	n->bDecl = bDecl;
	n->frameIndex = frameIndex;
	n->next = NULL;

	if ( t0 != T_ARRAY && !bInizializzato )
	{
		switch(t1)
		{
		case T_INTEGER:
			n->Value.iVal = 0;
			break;
		case T_REAL:
			n->Value.dblVal = 0.0;
			break;
		case T_STRING:
			n->Value.strVal = (char*)malloc(sizeof(char)*1);
			if ( ! n->Value.strVal )
			{
				free(n->Key);
				free(n);
				return NULL;
			}
			strcpy(n->Value.strVal, "");
			break;
		case T_BOOLEAN:
			n->Value.bVal = FALSE;
			break;
		}
	}

	return n;
}

int htFind(HashTable **pHashTable, char *Key, HashTable *pDati)
{
	int index = 0;
	HashTable *t = NULL;
	int a = 31415;
	//int b = 27183;
	char *s = Key;

	for(; *s != '\0'; s++)
		index = (a*index + *s) % HT_SIZE;
	if ( index < 0 )
		index *= -1;

	t = pHashTable[index];
	while ( t != NULL )
	{
		if ( strcmp(t->Key, Key) == 0 )
		{
			pDati->Key = t->Key;
			pDati->Tipo0 = t->Tipo0;
			pDati->Tipo1 = t->Tipo1;
			pDati->Tipo2 = t->Tipo2;
			pDati->Value = t->Value;
			pDati->bInizializzato = t->bInizializzato;
			pDati->frameIndex = t->frameIndex;
			return index;
		}
		t = t->next;
	}

	return -1;
}

void htInsert(HashTable **pHashTable, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex)
{
	int index = 0;
	HashTable *t = NULL;
	int a = 31415;
	//int b = 27183;
	char *s = Key;

	for(; *s != '\0'; s++)
		index = (a*index + *s) % HT_SIZE;
	if ( index < 0 )
		index *= -1;

	t = pHashTable[index];
	if ( t == NULL )
	{
		pHashTable[index] = htNewNode(Key, t0, t1, t2, v, bInizializzato, bDecl, frameIndex);
		return;
	}

	while ( t != NULL )
	{
		if ( strcmp(t->Key, Key) == 0 )
		{
			//pHashTable[index]->pLista = ListAppend(pHashTable[index]->pLista, pos);
			printf("\nErrore: La chiave %s e' gia' presente nella hashtable\n", Key);
			return;
		}
		if ( t->next == NULL )
		{
			t->next = htNewNode(Key, t0, t1, t2, v, bInizializzato, bDecl, frameIndex);
			//t = t->next;
			//t->next = NULL;
			return;
		}
		t = t->next;
	}
}

void htFree(HashTable* first)
{
	HashTable *n1 = first, *n2 = NULL;
	while ( n1 != NULL )
	{
		n2 = n1->next;
		if ( n1->Key )
		{
			free(n1->Key);
			n1->Key = NULL;
		}

		if ( n1->Tipo0 == T_SIMPLE && n1->Tipo1 == T_STRING )
		{
			if ( n1->Value.strVal )
			{
				free(n1->Value.strVal);
				n1->Value.strVal = NULL;
			}
		}
		else if ( n1->Tipo0 == T_ARRAY )
		{
			if ( n1->Tipo1 != T_STRING )
			{
				free(n1->Value.pArrayDef->pArray);
				n1->Value.pArrayDef->pArray = NULL;
			}
			else
			{
				int k;
				int count = 1;
				char **pszArray = (char**)n1->Value.pArrayDef->pArray;
				for ( k = 0; k < n1->Value.pArrayDef->countIndexes; k++ )
				{
					count *= n1->Value.pArrayDef->sizes[k];
				}
				for ( k = 0; k < count; k++ )
				{
					if ( pszArray[k] != NULL )
						free(pszArray[k]);
					free(pszArray);
				}
			}
			free(n1->Value.pArrayDef->sizes);
			n1->Value.pArrayDef->sizes = NULL;
			free(n1->Value.pArrayDef);
			n1->Value.pArrayDef = NULL;
		}
		free(n1);
		n1 = n2;
	}
}

void htPrint(HashTable **pHashTable)
{
	int index;
	HashTable *t = NULL;

	for ( index = 0; index < HT_SIZE - 1; index++ )
	{
		t = pHashTable[index];
		while ( t != NULL )
		{
			if ( t->Tipo0 == T_SIMPLE && t->bInizializzato )
			{
				printf("%5d: '%s' -> Valore : ", index, t->Key);
				switch ( t->Tipo1 )
				{
				case T_INTEGER:
					printf("int %d ", t->Value.iVal);
					break;
				case T_REAL:
					printf("real %lf ", t->Value.dblVal);
					break;
				case T_BOOLEAN:
					if ( t->Value.bVal )
						printf("bool true ");
					else
						printf("bool false ");
					break;
				case T_STRING:
					printf("string '%s' ", t->Value.strVal);
					break;
				}
			}
			else if ( t->Tipo0 == T_SIMPLE )
			{
				printf("%5d: '%s' -> Valore : non inizializzato ", index, t->Key);
			}
			else // T_ARRAY
			{
				int k;
				printf("%5d: '%s' -> array", index, t->Key);
				for ( k = 0; k < t->Value.pArrayDef->countIndexes; k++ )
					printf("[%d]", t->Value.pArrayDef->sizes[k]);
				printf(" of " );
				switch ( t->Tipo1 )
				{
				case T_INTEGER:
					printf("integer ");
					break;
				case T_REAL:
					printf("real ");
					break;
				case T_BOOLEAN:
					printf("bool ");
					break;
				case T_STRING:
					printf("string ");
					break;
				}
			}
			printf("\n");

			t = t->next;
		}
	}
}

void htPrintTAC(HashTable **pHashTable)
{
	int index;
	HashTable *t = NULL;

	for ( index = 0; index < HT_SIZE - 1; index++ )
	{
		t = pHashTable[index];
		while ( t != NULL )
		{
			if ( t->Tipo0 == T_SIMPLE && t->bInizializzato )
			{
				printf("%s = ", t->Key);
				switch ( t->Tipo1 )
				{
				case T_INTEGER:
					printf("%d\n", t->Value.iVal);
					break;
				case T_REAL:
					printf("%lf\n", t->Value.dblVal);
					break;
				case T_BOOLEAN:
					if ( t->Value.bVal )
						printf("true\n");
					else
						printf("false\n");
					break;
				case T_STRING:
					printf("\"%s\"\n", t->Value.strVal);
					break;
				}
			}
			t = t->next;
		}
	}
}


// -------------------------- Scope stack -----------------------------------------

void scopeInit(Scope *pScope)
{
	HashTable **pHT = NULL;
	int x;

	pScope->top = 0;

	pHT = (HashTable**)malloc(sizeof(HashTable*) * HT_SIZE);
	if ( pHT != NULL )
	{
		for ( x = 0; x < HT_SIZE; x++ )
		{
			pHT[x] = (HashTable*)malloc(sizeof(HashTable));
			if ( pHT[x] == NULL )
			{
				printf("Memoria non sufficiente.\n");
				free(pHT);
				pHT = NULL;
				return; 
			}
			pHT[x] = NULL;
		}
	}
	else
	{
		printf("Memoria non sufficiente.\n");
		return;
	}

	pScope->stack[pScope->top] = pHT;

	for ( x = 1; x < SCOPE_SIZE; x++ )
		pScope->stack[x] = NULL;
}

void scopePush(Scope *pScope)
{
	HashTable **pHT = NULL;
	int x;

	pScope->top++;
	if ( pScope->top > SCOPE_SIZE - 1 )
	{
		printf("Stack scope pieno!\n");
		pScope->top = SCOPE_SIZE - 1;
		return;
	}

	pHT = (HashTable**)malloc(sizeof(HashTable*) * HT_SIZE);
	if ( pHT != NULL )
	{
		for ( x = 0; x < HT_SIZE; x++ )
		{
			pHT[x] = (HashTable*)malloc(sizeof(HashTable));
			if ( pHT[x] == NULL )
			{
				printf("Memoria non sufficiente.\n");
				free(pHT);
				pHT = NULL;
				return; 
			}
			pHT[x] = NULL;
		}
	}
	else
	{
		printf("Memoria non sufficiente.\n");
		return;
	}

	pScope->stack[pScope->top] = pHT;
}

int scopePop(Scope *pScope)
{
	HashTable **pHT = NULL;
	int x;

	if ( pScope->top < 0 )
		return pScope->top;

	pHT = pScope->stack[pScope->top];
	for ( x = 0; x < HT_SIZE; x++)
		htFree(pHT[x]);
	free(pHT);
	pHT = NULL;
	pScope->stack[pScope->top] = NULL;
	pScope->top--;

	return pScope->top;
}

void scopeFree(Scope *pScope)
{
	while ( scopePop(pScope) >= 0 )
		;
}

int scopeFind(Scope* pScope, char *Key, HashTable *pDati)
{
	HashTable **pHT = NULL;
	int x;

	for ( x = pScope->top; x >= 0; x-- )
	{
		pHT = pScope->stack[x];
		if ( htFind(pHT, Key, pDati) >= 0 )
		{
			return 1;
		}
	}

	return 0;
}

int scopeInsert(Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, int frameIndex)
{
	HashTable **pHT = NULL;
	HashTable Dati;

	pHT = pScope->stack[pScope->top];

	if ( htFind(pHT, Key, &Dati) >= 0 )
	{
		printf("Errore: la variabile '%s' e' gia' stata dichiarata nello scope %d\n", Key, pScope->top);
		return 0;
	}

	htInsert(pHT, Key, t0, t1, t2, v, bInizializzato, 0, frameIndex);

	return 1;
}

int scopeUpdateValue(Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl)
{
	HashTable **pHT = NULL;
	HashTable Dati;
	int x;
	int index;

	for ( x = pScope->top; x >= 0; x-- )
	{
		pHT = pScope->stack[x];

		index = htFind(pHT, Key, &Dati);
		if ( index >= 0 )
		{
			HashTable *pTable = pHT[index];
			while ( strcmp(pTable->Key, Key) != 0 )
				pTable = pTable->next;
			pTable->Tipo0 = t0;
			pTable->Tipo1 = t1;
			pTable->Tipo2 = t2;
			pTable->Value = v;
			pTable->bInizializzato = bInizializzato;
			if ( bDecl != -1 )
				pTable->bDecl = bDecl;

			return 1;
		}
	}

	printf("Errore: variabile '%s' non dichiarata\n", Key);
	return 0;
}

