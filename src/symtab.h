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


struct CompilerContext;
typedef struct CompilerContext CompilerContext;
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
	/* Nome univoco nell'intero programma (es. "x", "x.1", "x.2"...): e' quello
	   usato da TAC, JVM e ASM, cosi' variabili omonime di scope diversi non
	   collidono. Per le variabili dello scope globale coincide con Key. */
	char *uniqueName;
	struct tagHashTable *next;
} HashTable;

typedef struct tagScope
{
	int top;
	HashTable **stack[SCOPE_SIZE];
} Scope;

HashTable* htNewNode(CompilerContext *ctx, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex);
int htFind(CompilerContext *ctx, HashTable **pHashTable, char *Key, HashTable *pDati);
void htInsert(CompilerContext *ctx, HashTable **pHashTable, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl, int frameIndex);
void htFree(CompilerContext *ctx, HashTable* first);
void htPrint(CompilerContext *ctx, HashTable **pHashTable);
void htPrintTAC(CompilerContext *ctx, HashTable **pHashTable);

void scopeInit(CompilerContext *ctx, Scope *pScope);
void scopePush(CompilerContext *ctx, Scope *pScope);
int  scopePop(CompilerContext *ctx, Scope *pScope);
/* Come scopePop, ma NON distrugge le variabili dello scope: le sposta nello scope
   radice (stack[0]) con chiave = uniqueName, cosi' TAC/JVM/ASM possono ancora
   risolverle dopo il parsing. Usata solo dal parser su g_Scope. */
int  scopePopKeep(CompilerContext *ctx, Scope *pScope);
/* Copia profonda (sizes + storage azzerato) della definizione di un array: l'interprete
   ne crea una nuova per ogni ingresso nel blocco che lo dichiara. Gli elementi stringa
   partono NULL, quindi non serve copiarne il contenuto. */
ArrayDef *arrayDefClone(const ArrayDef *src, enumTipo1 t1);
void arrayDefFreeEmpty(ArrayDef *a);
/* Assegna alla variabile appena dichiarata nello scope corrente un nome univoco. */
void scopeAssignUniqueName(CompilerContext *ctx, Scope *pScope, const char *Key);
void scopeFree(CompilerContext *ctx, Scope *pScope);
int  scopeFind(CompilerContext *ctx, Scope* pScope, char *Key, HashTable *pDati);
int  scopeInsert(CompilerContext *ctx, Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, int frameIndex);
int scopeUpdateValue(CompilerContext *ctx, Scope *pScope, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato, char bDecl);


#endif // SYMTAB_H
