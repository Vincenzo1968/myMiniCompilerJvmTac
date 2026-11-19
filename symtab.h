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

#ifndef SYMTAB_H
#define SYMTAB_H

#define HT_SIZE 251
//#define HT_SIZE 509
//#define HT_SIZE 1021
//#define HT_SIZE 2039
//#define HT_SIZE 4093
//#define HT_SIZE 8191
//#define HT_SIZE 16381
//#define HT_SIZE 32749

#define SCOPE_SIZE 4096

struct nodeTypeTag;

typedef enum
{
    T_SIMPLE,
    T_ARRAY
} enumTipo0;

typedef enum
{
    T_INTEGER,
	T_REAL,
    T_STRING,
    T_BOOLEAN
} enumTipo1;

typedef enum
{
    T_CONST,
    T_VAR
} enumTipo2;

typedef struct tagArrayDef
{
	void *pArray;
	int countIndexes;
	//int *indexes;
	int *sizes;
	struct nodeTypeTag **indexes;
} ArrayDef;

typedef union
{
    int iVal;
	double dblVal;
    char *strVal;
    enum {FALSE, TRUE} bVal;
	ArrayDef *pArrayDef;
} Valore;

typedef struct tagHashTable
{
	char *Key;
	enumTipo0 Tipo0;
	enumTipo1 Tipo1;
	enumTipo2 Tipo2;
	Valore Value;
	char bInizializzato;
	char bDecl;
	int frameIndex;
	struct tagHashTable *next;
} HashTable;

typedef struct tagScope
{
	int top;
	HashTable **stack[SCOPE_SIZE];
} Scope;

HashTable* htNewNode(char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex);
int htFind(HashTable **pHashTable, char *Key, HashTable *pDati);
void htInsert(HashTable **pHashTable, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex);
void htFree(HashTable* first);
void htPrint(HashTable **pHashTable);
void htPrintTAC(HashTable **pHashTable);

void scopeInit(Scope *pScope);
void scopePush(Scope *pScope);
int  scopePop(Scope *pScope);
void scopeFree(Scope *pScope);
int  scopeFind(Scope* pScope, char *Key, HashTable *pDati);
int  scopeInsert(Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, int frameIndex);
int scopeUpdateValue(Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl);


#endif // SYMTAB_H
