/*    Copyright (C) 2026 Vincenzo Lo Cicero

This file is part of myMiniCompilerJvmTac.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STRUCTS_H
#define STRUCTS_H

#include "context.h"

/* Riscrive il sorgente in ctx->g_szInput eliminando le struct (vedi structs.c).
   Restituisce 1 se tutto e' andato bene (anche se il programma non usa struct), 0 in caso di errore. */
int structsLower(CompilerContext *ctx);

#endif
