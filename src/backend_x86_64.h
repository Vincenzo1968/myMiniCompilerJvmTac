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
    int current_offset; // Starts at -8 and decreases by 8
    GlobalTable *gt;
} VarTable;

/* 
 * Generates x86_64 assembly code (NASM syntax) from TAC.
 * The output file can be assembled and linked using:
 *   nasm -f elf64 output.asm -o output.o
 *   gcc -no-pie -o executable output.o
 */
void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName);

#endif // BACKEND_X86_64_H
