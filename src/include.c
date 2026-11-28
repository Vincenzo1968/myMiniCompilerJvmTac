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


#define _POSIX_C_SOURCE 200809L

/*
 * Inclusion of multiple source files.
 *
 * Una riga  #include "percorso"  viene sostituita dal contenuto del file indicato (percorso relativo
 * alla directory del file che contiene la direttiva). Un file gia' incluso viene ignorato (come con un
 * include guard); a circular inclusion is an error. Per ogni riga del testo espanso si conserva il
 * file e la riga originali, cosi' i messaggi diagnostici (stampati con diag_printf) indicano il file giusto.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

#include "include.h"

#define INC_MAX_DEPTH 32

typedef struct {
	char *s;
	size_t n, cap;
} Out;

typedef struct {
	CompilerContext *ctx;
	Out out;
	LineMap *map;
	int capLines;
	char **canonDone;   /* file gia' incluso (percorsi canonici) */
	int ndone, capDone;
	char *active[INC_MAX_DEPTH + 1];
	int nactive;
	int usedInclude;
} Inc;

static void outAdd(Out *o, const char *p, size_t len)
{
	if (o->n + len + 1 > o->cap) {
		size_t nc = o->cap ? o->cap * 2 : 1024;
		char *ns;
		while (nc < o->n + len + 1) nc *= 2;
		ns = (char *)realloc(o->s, nc);
		if (!ns) return;
		o->s = ns;
		o->cap = nc;
	}
	memcpy(o->s + o->n, p, len);
	o->n += len;
	o->s[o->n] = '\0';
}

static int mapAddFile(Inc *st, const char *name)
{
	char **nf = (char **)realloc(st->map->files, (size_t)(st->map->nfiles + 1) * sizeof(char *));
	if (!nf) return -1;
	st->map->files = nf;
	st->map->files[st->map->nfiles] = strdup(name);
	return st->map->nfiles++;
}

static int mapAddLine(Inc *st, int fileIdx, int line)
{
	if (st->map->nlines + 1 >= st->capLines) {
		int nc = st->capLines ? st->capLines * 2 : 1024;
		int *a = (int *)realloc(st->map->fileIdx, (size_t)nc * sizeof(int));
		int *b;
		if (!a) return 0;
		st->map->fileIdx = a;
		b = (int *)realloc(st->map->origLine, (size_t)nc * sizeof(int));
		if (!b) return 0;
		st->map->origLine = b;
		st->capLines = nc;
	}
	st->map->nlines++;
	st->map->fileIdx[st->map->nlines] = fileIdx;
	st->map->origLine[st->map->nlines] = line;
	return 1;
}

static char *readWholeFile(const char *path)
{
	FILE *fp = fopen(path, "rb");
	long sz;
	char *buf;
	size_t rd;
	if (!fp) return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
	sz = ftell(fp);
	if (sz < 0) { fclose(fp); return NULL; }
	fseek(fp, 0, SEEK_SET);
	buf = (char *)malloc((size_t)sz + 1);
	if (!buf) { fclose(fp); return NULL; }
	rd = fread(buf, 1, (size_t)sz, fp);
	buf[rd] = '\0';
	fclose(fp);
	return buf;
}

/* If the line is a directive  #include "x"  copia x in name e restituisce 1 */
static int parseDirective(const char *line, size_t len, char *name, size_t nameSize)
{
	size_t i = 0, k = 0;
	while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
	if (i >= len || line[i] != '#') return 0;
	i++;
	while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
	if (len - i < 7 || strncmp(line + i, "include", 7) != 0) return 0;
	i += 7;
	while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
	if (i >= len || line[i] != '"') return -1; /* direttiva malformata */
	i++;
	while (i < len && line[i] != '"') {
		if (k + 1 >= nameSize) return -1;
		name[k++] = line[i++];
	}
	if (i >= len) return -1;
	name[k] = '\0';
	i++;
	while (i < len && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
	if (i < len && !(line[i] == '/' && i + 1 < len && line[i + 1] == '/')) return -1;
	return k > 0 ? 1 : -1;
}

static int expandText(Inc *st, const char *text, const char *display, const char *canon, int depth);

static int includeFile(Inc *st, const char *fromDisplay, int fromLine, const char *name, int depth)
{
	char path[PATH_MAX + 1], disp[PATH_MAX + 1], canon[PATH_MAX + 1];
	char *text;
	int i, ok;

	/* percorso relativo alla directory del file che include */
	if (name[0] == '/') {
		snprintf(disp, sizeof(disp), "%s", name);
	} else {
		const char *slash = strrchr(fromDisplay, '/');
		size_t dirLen = slash ? (size_t)(slash - fromDisplay) : 0;
		if (slash && dirLen + 1 + strlen(name) < sizeof(disp)) {
			memcpy(disp, fromDisplay, dirLen);
			disp[dirLen] = '/';
			strcpy(disp + dirLen + 1, name);
		} else if (!slash && strlen(name) < sizeof(disp)) {
			strcpy(disp, name);
		} else {
			printf("%s: Line %d: Include error. Path too long for '%s'.\n", fromDisplay, fromLine, name);
			st->ctx->g_numErrors++;
			return 0;
		}
	}
	snprintf(path, sizeof(path), "%s", disp);

	if (!realpath(path, canon)) {
		printf("%s: Line %d: Include error. Cannot open file '%s'.\n", fromDisplay, fromLine, name);
		st->ctx->g_numErrors++;
		return 0;
	}
	for (i = 0; i < st->nactive; i++)
		if (strcmp(st->active[i], canon) == 0) {
			printf("%s: Line %d: Include error. Circular inclusion of '%s'.\n", fromDisplay, fromLine, name);
			st->ctx->g_numErrors++;
			return 0;
		}
	for (i = 0; i < st->ndone; i++)
		if (strcmp(st->canonDone[i], canon) == 0)
			return 1; /* gia' incluso: ignorato */
	if (depth >= INC_MAX_DEPTH) {
		printf("%s: Line %d: Include error. Includes nested too deeply.\n", fromDisplay, fromLine);
		st->ctx->g_numErrors++;
		return 0;
	}
	text = readWholeFile(path);
	if (!text) {
		printf("%s: Line %d: Include error. Cannot read file '%s'.\n", fromDisplay, fromLine, name);
		st->ctx->g_numErrors++;
		return 0;
	}
	ok = expandText(st, text, disp, canon, depth + 1);
	free(text);
	return ok;
}

static int expandText(Inc *st, const char *text, const char *display, const char *canon, int depth)
{
	int fileIdx = mapAddFile(st, display), lineNo = 1, ok = 1;
	const char *p = text;
	char *myCanon = strdup(canon);

	if (fileIdx < 0 || !myCanon) { free(myCanon); return 0; }
	st->active[st->nactive++] = myCanon;

	while (*p && ok) {
		const char *eol = strchr(p, '\n');
		size_t len = eol ? (size_t)(eol - p) : strlen(p);
		char name[PATH_MAX];
		int d = parseDirective(p, len, name, sizeof(name));

		if (d == 1) {
			st->usedInclude = 1;
			ok = includeFile(st, display, lineNo, name, depth);
		} else if (d == -1) {
			printf("%s: Line %d: Include error. Malformed #include directive (expected: #include \"file\").\n", display, lineNo);
			st->ctx->g_numErrors++;
			ok = 0;
		} else {
			outAdd(&st->out, p, len);
			outAdd(&st->out, "\n", 1);
			if (!mapAddLine(st, fileIdx, lineNo)) ok = 0;
		}
		lineNo++;
		p = eol ? eol + 1 : p + len;
	}

	st->nactive--;
	/* file completed: a later #include of the same file is ignored */
	if (ok) {
		if (st->ndone >= st->capDone) {
			int nc = st->capDone ? st->capDone * 2 : 8;
			char **nd = (char **)realloc(st->canonDone, (size_t)nc * sizeof(char *));
			if (!nd) { free(myCanon); return 0; }
			st->canonDone = nd;
			st->capDone = nc;
		}
		st->canonDone[st->ndone++] = myCanon;
	} else {
		free(myCanon);
	}
	return ok;
}

int includeExpand(CompilerContext *ctx, const char *mainFile, LineMap **pmap)
{
	Inc st;
	char canon[PATH_MAX + 1];
	int ok, i;

	*pmap = NULL;
	if (!ctx->g_szInput || !strstr(ctx->g_szInput, "#include")) return 1;

	memset(&st, 0, sizeof(st));
	st.ctx = ctx;
	st.map = (LineMap *)calloc(1, sizeof(LineMap));
	if (!st.map) return 0;
	if (!realpath(mainFile, canon)) snprintf(canon, sizeof(canon), "%s", mainFile);

	ok = expandText(&st, ctx->g_szInput, mainFile, canon, 0);

	for (i = 0; i < st.ndone; i++) free(st.canonDone[i]);
	free(st.canonDone);

	if (ok && st.usedInclude && st.out.s) {
		free(ctx->g_szInput);
		ctx->g_szInput = st.out.s;
		*pmap = st.map;
		return 1;
	}
	free(st.out.s);
	if (ok) { lineMapFree(st.map); return 1; }
	lineMapFree(st.map);
	return 0;
}

void lineMapFree(LineMap *map)
{
	int i;
	if (!map) return;
	for (i = 0; i < map->nfiles; i++) free(map->files[i]);
	free(map->files);
	free(map->fileIdx);
	free(map->origLine);
	free(map);
}

/* ------------------------------------------------------------------ messaggi diagnostici */

static _Thread_local const LineMap *tl_map = NULL;

void diagSetLineMap(const LineMap *map)
{
	tl_map = map;
}

/* Like printf, but with a line map translates "Line N" (line of the expanded text) into "[file] Line M". */
int diag_printf(const char *fmt, ...)
{
	va_list ap, ap2;
	char small[1024];
	char *buf = small;
	int n, res;
	const char *q;

	va_start(ap, fmt);
	va_copy(ap2, ap);
	n = vsnprintf(small, sizeof(small), fmt, ap);
	va_end(ap);
	if (n >= (int)sizeof(small)) {
		buf = (char *)malloc((size_t)n + 1);
		if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
		else buf = small;
	}
	va_end(ap2);

	q = tl_map ? strstr(buf, "Line ") : NULL;
	if (q && isdigit((unsigned char)q[5])) {
		char *end;
		long ln = strtol(q + 5, &end, 10);
		if (ln >= 1 && ln <= tl_map->nlines) {
			res = printf("%.*s[%s] Line %d%s", (int)(q - buf), buf, tl_map->files[tl_map->fileIdx[ln]],
			             tl_map->origLine[ln], end);
			if (buf != small) free(buf);
			return res;
		}
	}
	res = fputs(buf, stdout) >= 0 ? (int)strlen(buf) : -1;
	if (buf != small) free(buf);
	return res;
}
