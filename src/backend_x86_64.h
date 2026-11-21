#ifndef BACKEND_X86_64_H
#define BACKEND_X86_64_H

#include "context.h"
#include "tac.h"
#include "symtab.h"

/* =========================================================================
 * Strutture per la mappatura delle variabili sullo stack frame
 * ========================================================================= */
typedef struct {
    char *name;
    int offset;        // Offset negativo rispetto a rbp
    enumTipo1 type1;
} VarSlot;

typedef struct {
    VarSlot *items;
    int count;
    int capacity;
    int current_offset; // Inizia a -8 e decresce di 8
} VarTable;

/* 
 * Genera il codice assembly x86_64 (sintassi NASM) a partire dal TAC.
 * Il file di output potrà essere assemblato e linkato con:
 *   nasm -f elf64 output.asm -o output.o
 *   gcc -no-pie -o executable output.o
 */
void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName);

#endif // BACKEND_X86_64_H
