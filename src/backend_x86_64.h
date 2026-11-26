#ifndef BACKEND_X86_64_H
#define BACKEND_X86_64_H

#include "context.h"
#include "tac.h"
#include "symtab.h"

/* =========================================================================
 * Strutture per la mappatura delle variabili
 * ========================================================================= */

/* Dove vive una variabile:
 *   SLOT_FRAME    nello stack frame della funzione (offset da rbp; positivo per gli argomenti)
 *   SLOT_GLOBAL   in memoria statica (.bss), etichetta g_<nome>
 *   SLOT_REFPARAM parametro 'ref': lo slot contiene l'indirizzo della variabile del chiamante
 *   SLOT_ARRPARAM parametro array: lo slot contiene l'indirizzo base dell'array del chiamante */
enum { SLOT_FRAME = 0, SLOT_GLOBAL, SLOT_REFPARAM, SLOT_ARRPARAM };

typedef struct {
    char *name;
    int offset;        // Offset rispetto a rbp
    enumTipo1 type1;
    int kind;          // SLOT_*
} VarSlot;

/* Variabili globali: condivise da tutte le funzioni. */
typedef struct {
    char *name;
    enumTipo1 type1;
    int bytes;
} GlobalVar;

typedef struct {
    GlobalVar *items;
    int count;
    int capacity;
    int lbl;           // contatore per le label locali (.Lnn), unico in tutto il file
} GlobalTable;

typedef struct {
    VarSlot *items;
    int count;
    int capacity;
    int current_offset; // Inizia a -8 e decresce di 8
    GlobalTable *gt;
} VarTable;

/* 
 * Genera il codice assembly x86_64 (sintassi NASM) a partire dal TAC.
 * Il file di output potrà essere assemblato e linkato con:
 *   nasm -f elf64 output.asm -o output.o
 *   gcc -no-pie -o executable output.o
 */
void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName);

#endif // BACKEND_X86_64_H
