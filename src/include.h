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


#ifndef INCLUDE_H
#define INCLUDE_H

#include "context.h"

/* Map from lines of the expanded source to the original (file, line). */
typedef struct {
	char **files;     /* file names, as written in the directives */
	int nfiles;
	int *fileIdx;     /* per expanded line (1-based, index 0 unused) */
	int *origLine;
	int nlines;
} LineMap;

/* Expands #include "file" directives in ctx->g_szInput (the text of mainFile). If there are none,
   nothing changes and *map stays NULL. Otherwise ctx->g_szInput is replaced by the expanded text
   and *map receives the line map. Returns 1 on success, 0 on error (already reported). */
int includeExpand(CompilerContext *ctx, const char *mainFile, LineMap **map);
void lineMapFree(LineMap *map);

/* Active map for diagnostics of the current thread (NULL: none). */
void diagSetLineMap(const LineMap *map);
int diag_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* INCLUDE_H */
