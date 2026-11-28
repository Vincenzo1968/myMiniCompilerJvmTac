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
#include "symtab.h"

// -------------------------- HashTable -----------------------------------------

HashTable* htNewNode(CompilerContext *ctx, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex)
{
	(void)ctx;
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
	n->uniqueName = (char*)malloc(strlen(Key)+1);
	if ( n->uniqueName == NULL )
	{
		free(n->Key);
		free(n);
		return NULL;
	}
	strcpy(n->uniqueName, Key);
	n->Tipo0 = t0;
	n->Tipo1 = t1;
	n->Tipo2 = t2;
	n->Value = v;
	n->bInizializzato = bInizializzato;
	n->bDecl = bDecl;
	n->frameIndex = frameIndex;
	n->isGlobal = 0;
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
				free(n->uniqueName);
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

int htFind(CompilerContext *ctx, HashTable **pHashTable, char *Key, HashTable *pDati)
{
	(void)ctx;
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
			pDati->uniqueName = t->uniqueName;
			pDati->Tipo0 = t->Tipo0;
			pDati->Tipo1 = t->Tipo1;
			pDati->Tipo2 = t->Tipo2;
			pDati->Value = t->Value;
			pDati->bInizializzato = t->bInizializzato;
			pDati->frameIndex = t->frameIndex;
			pDati->isGlobal = t->isGlobal;
			return index;
		}
		t = t->next;
	}

	return -1;
}

void htInsert(CompilerContext *ctx, HashTable **pHashTable, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex)
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
		pHashTable[index] = htNewNode(ctx, Key, t0, t1, t2, v, bInizializzato, bDecl, frameIndex);
		return;
	}

	while ( t != NULL )
	{
		if ( strcmp(t->Key, Key) == 0 )
		{
			//pHashTable[index]->pLista = ListAppend(pHashTable[index]->pLista, pos);
			printf("\nError: Key %s is already present in the hashtable\n", Key);
			return;
		}
		if ( t->next == NULL )
		{
			t->next = htNewNode(ctx, Key, t0, t1, t2, v, bInizializzato, bDecl, frameIndex);
			//t = t->next;
			//t->next = NULL;
			return;
		}
		t = t->next;
	}
}

void htFree(CompilerContext *ctx, HashTable* first)
{
	(void)ctx;
	HashTable *n1 = first, *n2 = NULL;
	while ( n1 != NULL )
	{
		n2 = n1->next;
		if ( n1->Key )
		{
			free(n1->Key);
			n1->Key = NULL;
		}
		if ( n1->uniqueName )
		{
			free(n1->uniqueName);
			n1->uniqueName = NULL;
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
				char **pszArray = (char**)n1->Value.pArrayDef->pArray;
				if ( pszArray != NULL && n1->Value.pArrayDef->sizes != NULL )
				{
					int k;
					int count = 1;
					for ( k = 0; k < n1->Value.pArrayDef->countIndexes; k++ )
					{
						count *= n1->Value.pArrayDef->sizes[k];
					}
					for ( k = 0; k < count; k++ )
					{
						if ( pszArray[k] != NULL )
							free(pszArray[k]);
					}
				}
				free(pszArray);
				n1->Value.pArrayDef->pArray = NULL;
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

void htPrint(CompilerContext *ctx, HashTable **pHashTable)
{
	(void)ctx;
	int index;
	HashTable *t = NULL;

	for ( index = 0; index < HT_SIZE - 1; index++ )
	{
		t = pHashTable[index];
		while ( t != NULL )
		{
			if ( t->Tipo0 == T_SIMPLE && t->bInizializzato )
			{
				printf("%5d: '%s' -> Value : ", index, t->Key);
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
				printf("%5d: '%s' -> Value : uninitialized ", index, t->Key);
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

void htPrintTAC(CompilerContext *ctx, HashTable **pHashTable)
{
	(void)ctx;
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

void scopeInit(CompilerContext *ctx, Scope *pScope)
{
	(void)ctx;
	HashTable **pHT = NULL;
	int x;

	pScope->top = 0;

	pHT = (HashTable**)malloc(sizeof(HashTable*) * HT_SIZE);
	if ( pHT != NULL )
	{
		for ( x = 0; x < HT_SIZE; x++ )
		{
			pHT[x] = NULL;
		}
	}
	else
	{
		printf("Out of memory.\n");
		return;
	}

	pScope->stack[pScope->top] = pHT;

	for ( x = 1; x < SCOPE_SIZE; x++ )
		pScope->stack[x] = NULL;
}

void scopePush(CompilerContext *ctx, Scope *pScope)
{
	(void)ctx;
	HashTable **pHT = NULL;
	int x;

	pScope->top++;
	if ( pScope->top > SCOPE_SIZE - 1 )
	{
		printf("Scope stack full!\n");
		pScope->top = SCOPE_SIZE - 1;
		return;
	}

	pHT = (HashTable**)malloc(sizeof(HashTable*) * HT_SIZE);
	if ( pHT != NULL )
	{
		for ( x = 0; x < HT_SIZE; x++ )
		{
			pHT[x] = NULL;
		}
	}
	else
	{
		printf("Out of memory.\n");
		return;
	}

	pScope->stack[pScope->top] = pHT;
}

int scopePop(CompilerContext *ctx, Scope *pScope)
{
	HashTable **pHT = NULL;
	int x;

	if ( pScope->top < 0 )
		return pScope->top;

	pHT = pScope->stack[pScope->top];
	for ( x = 0; x < HT_SIZE; x++)
		htFree(ctx, pHT[x]);
	free(pHT);
	pHT = NULL;
	pScope->stack[pScope->top] = NULL;
	pScope->top--;

	return pScope->top;
}

void scopeFree(CompilerContext *ctx, Scope *pScope)
{
	while ( scopePop(ctx, pScope) >= 0 )
		;
}

int scopeFind(CompilerContext *ctx, Scope* pScope, char *Key, HashTable *pDati)
{
	HashTable **pHT = NULL;
	int x;

	for ( x = pScope->top; x >= 0; x-- )
	{
		pHT = pScope->stack[x];
		if ( htFind(ctx, pHT, Key, pDati) >= 0 )
		{
			return 1;
		}
	}

	return 0;
}

int scopeInsert(CompilerContext *ctx, Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, int frameIndex)
{
	HashTable **pHT = NULL;
	HashTable Dati;

	pHT = pScope->stack[pScope->top];

	if ( htFind(ctx, pHT, Key, &Dati) >= 0 )
	{
		printf("Error: variable '%s' has already been declared in scope %d\n", Key, pScope->top);
		return 0;
	}

	htInsert(ctx, pHT, Key, t0, t1, t2, v, bInizializzato, 0, frameIndex);

	return 1;
}

int scopeUpdateValue(CompilerContext *ctx, Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl)
{
	HashTable **pHT = NULL;
	HashTable Dati;
	int x;
	int index;

	for ( x = pScope->top; x >= 0; x-- )
	{
		pHT = pScope->stack[x];

		index = htFind(ctx, pHT, Key, &Dati);
		if ( index >= 0 )
		{
			HashTable *pTable = pHT[index];
			while ( strcmp(pTable->Key, Key) != 0 )
				pTable = pTable->next;

			if ( pTable->Tipo0 == T_SIMPLE && pTable->Tipo1 == T_STRING &&
				 pTable->Value.strVal != NULL && pTable->Value.strVal != v.strVal )
			{
				free(pTable->Value.strVal);
				pTable->Value.strVal = NULL;
			}

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

	printf("Error: variable '%s' not declared\n", Key);
	return 0;
}


// -------------------------- Nomi univoci e scope persistenti -----------------------------------------

static int htIndexOf(const char *Key)
{
	int index = 0;
	int a = 31415;
	const char *s = Key;

	for(; *s != '\0'; s++)
		index = (a*index + *s) % HT_SIZE;
	if ( index < 0 )
		index *= -1;

	return index;
}

/* True if 'cand' is already used as a unique name by any variable,
   both in the scopes still open and in those already closed (moved to stack[0]). */
static int uniqueNameInUse(Scope *pScope, const char *cand)
{
	int x, k;
	HashTable *t;

	for ( x = 0; x <= pScope->top; x++ )
	{
		HashTable **pHT = pScope->stack[x];
		if ( pHT == NULL )
			continue;
		for ( k = 0; k < HT_SIZE; k++ )
		{
			for ( t = pHT[k]; t != NULL; t = t->next )
			{
				if ( t->uniqueName != NULL && strcmp(t->uniqueName, cand) == 0 )
					return 1;
			}
		}
	}

	return 0;
}

void scopeAssignUniqueName(CompilerContext *ctx, Scope *pScope, const char *Key)
{
	(void)ctx;
	HashTable **pHT = pScope->stack[pScope->top];
	HashTable *t;
	char cand[512];
	int n = 0;

	for ( t = pHT[htIndexOf(Key)]; t != NULL; t = t->next )
	{
		if ( strcmp(t->Key, Key) == 0 )
			break;
	}
	if ( t == NULL )
		return;

	t->isGlobal = (pScope->top == 0);

	/* Root scope keeps original names (source names do not
	   never contain '.', so "x.1" cannot collide with a user variable). */
	if ( pScope->top > 0 )
	{
		/* Newly inserted node already has uniqueName == Key: exclude it
		   dal controllo temporaneamente. */
		char *saved = t->uniqueName;
		t->uniqueName = NULL;

		snprintf(cand, sizeof(cand), "%s", Key);
		while ( uniqueNameInUse(pScope, cand) )
		{
			n++;
			snprintf(cand, sizeof(cand), "%s.%d", Key, n);
		}

		t->uniqueName = saved;
		if ( n > 0 )
		{
			char *newName = (char*)malloc(strlen(cand) + 1);
			if ( newName != NULL )
			{
				strcpy(newName, cand);
				free(t->uniqueName);
				t->uniqueName = newName;
			}
		}
	}
}

int scopePopKeep(CompilerContext *ctx, Scope *pScope)
{
	(void)ctx;
	HashTable **pHT;
	HashTable **pRoot;
	int x;

	if ( pScope->top <= 0 )
		return pScope->top;

	pHT = pScope->stack[pScope->top];
	pRoot = pScope->stack[0];

	for ( x = 0; x < HT_SIZE; x++ )
	{
		HashTable *n = pHT[x];
		while ( n != NULL )
		{
			HashTable *next = n->next;
			int idx = htIndexOf(n->uniqueName);

			/* key becomes the unique name */
			free(n->Key);
			n->Key = (char*)malloc(strlen(n->uniqueName) + 1);
			strcpy(n->Key, n->uniqueName);

			n->next = pRoot[idx];
			pRoot[idx] = n;

			n = next;
		}
	}

	free(pHT);
	pScope->stack[pScope->top] = NULL;
	pScope->top--;

	return pScope->top;
}


ArrayDef *arrayDefClone(const ArrayDef *src, enumTipo1 t1)
{
	ArrayDef *a;
	size_t count = 1;
	size_t elemSize;
	int k;

	if ( src == NULL || src->sizes == NULL )
		return NULL;

	for ( k = 0; k < src->countIndexes; k++ )
		count *= (size_t)src->sizes[k];

	if ( t1 == T_REAL )
		elemSize = sizeof(double);
	else if ( t1 == T_STRING )
		elemSize = sizeof(char*);
	else
		elemSize = sizeof(int);

	a = (ArrayDef*)calloc(1, sizeof(ArrayDef));
	if ( a == NULL )
		return NULL;

	a->countIndexes = src->countIndexes;
	a->indexes = NULL;
	a->sizes = (int*)malloc((size_t)src->countIndexes * sizeof(int));
	a->pArray = calloc(count, elemSize);
	if ( a->sizes == NULL || a->pArray == NULL )
	{
		free(a->sizes);
		free(a->pArray);
		free(a);
		return NULL;
	}
	for ( k = 0; k < src->countIndexes; k++ )
		a->sizes[k] = src->sizes[k];

	return a;
}

void arrayDefFreeEmpty(ArrayDef *a)
{
	if ( a == NULL )
		return;
	free(a->pArray);
	free(a->sizes);
	free(a);
}
