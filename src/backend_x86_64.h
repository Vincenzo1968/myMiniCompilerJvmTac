#ifndef BACKEND_X86_64_H
#define BACKEND_X86_64_H

#include "context.h"
#include "tac.h"
#include "symtab.h"

/* =========================================================================
 * Structures for variable mapping
 * ========================================================================= */

/* Where a variable lives:
 *   SLOT_FRAME    in the function stack frame (offset from rbp; positive for arguments)
 *   SLOT_GLOBAL   in static memory (.bss), label g_<name>
 *   SLOT_REFPARAM 'ref' parameter: the slot holds the address of the caller's variable
 *   SLOT_ARRPARAM array parameter: the slot holds the base address of the caller's array */
enum { SLOT_FRAME = 0, SLOT_GLOBAL, SLOT_REFPARAM, SLOT_ARRPARAM };

typedef struct {
    char *name;
    int offset;        // Offset rispetto a rbp
    enumTipo1 type1;
    int kind;          // SLOT_*
} VarSlot;

/* Global variables: shared across all functions. */
typedef struct {
    char *name;
    enumTipo1 type1;
    int bytes;
} GlobalVar;

typedef struct {
    GlobalVar *items;
    int count;
    int capacity;
    int lbl;           // counter for local labels (.Lnn), unique across the file
} GlobalTable;

typedef struct {
    VarSlot *items;
    int count;
    int capacity;
    int current_offset; // Inizia a -8 e decresce di 8
    GlobalTable *gt;
} VarTable;

/* 
 * Generates x86_64 assembly code (NASM syntax) from TAC.
 * Il file di output potrà essere assemblato e linkato con:
 *   nasm -f elf64 output.asm -o output.o
 *   gcc -no-pie -o executable output.o
 */
void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName);

#endif // BACKEND_X86_64_H
