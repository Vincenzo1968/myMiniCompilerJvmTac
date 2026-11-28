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

/*
 * Support for structs: "lowering" at the source-text level.
 *
 * Before lexical analysis, the source is rewritten so that every struct disappears:
 *
 *   struct P { int x; real y; };        (the type definition is removed)
 *   P p;                                ->  int p$x; real p$y;
 *   array v[10] of P;                   ->  array v$x[10] of int; array v$y[10] of real;
 *   p.x = 3;                            ->  p$x = 3;
 *   v[i].y = 2.5;                       ->  v$y[i] = 2.5;
 *   p = q;                              ->  begin p$x = q$x; p$y = q$y; end
 *   f(p, 3)    with void f(P a, int n)  ->  f(p$x, p$y, 3)  with void f(int a$x, real a$y, int n)
 *   P g(int n) ...  return r;           ->  void g(int n, ref int zz_ret$x, ref real zz_ret$y)
 *   p = g(4);                           ->  g(4, p$x, p$y);
 *
 * A struct thus becomes a set of scalar variables (or parallel arrays for arrays of
 * structs), and all backends (interpreter, TAC, JVM, asm) and optimizations work unchanged.
 * Source line numbers are preserved (columns may shift); mangled names contain '$',
 * which a user-written identifier cannot contain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "context.h"
#include "structs.h"

#define SL_MAXDIMS   8
#define SL_MAXFIELDS 32
#define SL_MAXLEAVES 64
#define SL_MAXSCOPES 64
#define SL_MAXVARS   256
#define SL_MAXARGS   64

/* ---------------------------------------------------------------- buffer */

typedef struct {
	char *s;
	size_t n, cap;
} Buf;

static void bufAdd(Buf *b, const char *p, size_t len)
{
	if (b->n + len + 1 > b->cap) {
		size_t nc = b->cap ? b->cap * 2 : 256;
		char *ns;
		while (nc < b->n + len + 1) nc *= 2;
		ns = (char *)realloc(b->s, nc);
		if (!ns) return;
		b->s = ns;
		b->cap = nc;
	}
	memcpy(b->s + b->n, p, len);
	b->n += len;
	b->s[b->n] = '\0';
}

static void bufStr(Buf *b, const char *s) { bufAdd(b, s, strlen(s)); }
static void bufCh(Buf *b, char c) { bufAdd(b, &c, 1); }

/* ---------------------------------------------------------------- token */

enum { K_ID = 1, K_NUM, K_STR, K_PUNCT, K_EOF };

typedef struct {
	int kind;
	int start, end;   /* offset in source */
	int line;
	char *text;
} Tok;

typedef struct {
	char name[64];
	char typ[16];      /* int, real, bool, string (if sid < 0) */
	int sid;          /* struct nested, or -1 */
	int ndims;
	char dims[SL_MAXDIMS][16];
} Field;

typedef struct {
	char name[64];
	int nfields;
	Field fields[SL_MAXFIELDS];
} StructDef;

typedef struct {
	char suffix[256];   /* p.es. "pos$x" */
	char typ[16];
	int ndims;
	char dims[SL_MAXDIMS][16];
} Leaf;

typedef struct {
	char name[64];
	int sid;            /* -1: "regular" variable (only used to handle shadowing) */
	int ndims;
} VarEnt;

typedef struct {
	VarEnt vars[SL_MAXVARS];
	int n;
} ScopeEnt;

typedef struct {
	char name[64];
	int retSid;
} FuncEnt;

typedef struct {
	CompilerContext *ctx;
	const char *src;
	Tok *toks;
	int ntok, captok;
	StructDef *structs;
	int nstructs;
	ScopeEnt scopes[SL_MAXSCOPES];
	int nscopes;
	FuncEnt *funcs;
	int nfuncs;
	VarEnt pending[64];
	int npending;
	int curRetSid;        /* struct returned by the function currently being rewritten, or -1 */
	int err;
	Buf out;
	int pos;              /* copy position in source (non-flat mode) */
} SL;

typedef struct {
	int isLeaf;
	int sid;              /* struct current (if !isLeaf) */
	char name[256];       /* name mangled (prefix or leaf) */
	int nidx;
	char *idx[SL_MAXDIMS * 2];
	int total;            /* indices required to access a single element */
	int leafNdims;        /* own dimensions of the array leaf (isLeaf) */
	int next;             /* first token after the path */
	int line;
} Path;

static void slError(SL *sl, int line, const char *msg, const char *arg)
{
	if (arg)
		printf("Line %d: Struct error. %s '%s'.\n", line, msg, arg);
	else
		printf("Line %d: Struct error. %s\n", line, msg);
	sl->ctx->g_numErrors++;
	sl->err = 1;
}

static int isTok(const SL *sl, int i, const char *t)
{
	return i >= 0 && i < sl->ntok && sl->toks[i].kind != K_STR && strcmp(sl->toks[i].text, t) == 0;
}

static int isKind(const SL *sl, int i, int kind)
{
	return i >= 0 && i < sl->ntok && sl->toks[i].kind == kind;
}

static int isTypeKw(const char *s)
{
	return strcmp(s, "int") == 0 || strcmp(s, "real") == 0 || strcmp(s, "bool") == 0 || strcmp(s, "string") == 0;
}

static int isReserved(const char *s)
{
	static const char *kw[] = { "if", "else", "while", "do", "begin", "end", "print", "read", "return", "array",
	                            "of", "int", "real", "bool", "string", "void", "ref", "true", "false", "struct", NULL };
	int i;
	for (i = 0; kw[i]; i++)
		if (strcmp(s, kw[i]) == 0) return 1;
	return 0;
}

/* ---------------------------------------------------------------- tokenizer */

static int tokAdd(SL *sl, int kind, int start, int end, int line)
{
	Tok *t;
	if (sl->ntok >= sl->captok) {
		int nc = sl->captok ? sl->captok * 2 : 256;
		Tok *nt = (Tok *)realloc(sl->toks, (size_t)nc * sizeof(Tok));
		if (!nt) return 0;
		sl->toks = nt;
		sl->captok = nc;
	}
	t = &sl->toks[sl->ntok++];
	t->kind = kind;
	t->start = start;
	t->end = end;
	t->line = line;
	t->text = (char *)malloc((size_t)(end - start) + 1);
	if (!t->text) return 0;
	memcpy(t->text, sl->src + start, (size_t)(end - start));
	t->text[end - start] = '\0';
	return 1;
}

static int tokenize(SL *sl)
{
	const char *s = sl->src;
	int i = 0, line = 1, st;

	for (;;) {
		char c = s[i];
		if (c == '\0') break;
		if (c == '\n') { line++; i++; continue; }
		if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
		if (c == '/' && s[i + 1] == '/') {
			while (s[i] && s[i] != '\n') i++;
			continue;
		}
		if (c == '/' && s[i + 1] == '*') {
			i += 2;
			while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) {
				if (s[i] == '\n') line++;
				i++;
			}
			if (s[i]) i += 2;
			continue;
		}
		st = i;
		if (isalpha((unsigned char)c) || c == '_') {
			while (isalnum((unsigned char)s[i]) || s[i] == '_') i++;
			if (!tokAdd(sl, K_ID, st, i, line)) return 0;
		} else if (isdigit((unsigned char)c)) {
			while (isdigit((unsigned char)s[i])) i++;
			if (s[i] == '.' && isdigit((unsigned char)s[i + 1])) {
				i++;
				while (isdigit((unsigned char)s[i])) i++;
			}
			if ((s[i] == 'e' || s[i] == 'E') &&
			    (isdigit((unsigned char)s[i + 1]) ||
			     ((s[i + 1] == '+' || s[i + 1] == '-') && isdigit((unsigned char)s[i + 2])))) {
				i += 2;
				while (isdigit((unsigned char)s[i])) i++;
			}
			if (!tokAdd(sl, K_NUM, st, i, line)) return 0;
		} else if (c == '"') {
			i++;
			while (s[i] && s[i] != '"') {
				if (s[i] == '\\' && s[i + 1]) i++;
				if (s[i] == '\n') line++;
				i++;
			}
			if (s[i] == '"') i++;
			if (!tokAdd(sl, K_STR, st, i, line)) return 0;
		} else {
			i++;
			if ((c == '=' || c == '!' || c == '<' || c == '>') && s[i] == '=') i++;
			if (!tokAdd(sl, K_PUNCT, st, i, line)) return 0;
		}
	}
	return tokAdd(sl, K_EOF, i, i, line);
}

/* ---------------------------------------------------------------- tables */

static int structFind(const SL *sl, const char *name)
{
	int i;
	for (i = 0; i < sl->nstructs; i++)
		if (strcmp(sl->structs[i].name, name) == 0) return i;
	return -1;
}

static int funcFindSl(const SL *sl, const char *name)
{
	int i;
	for (i = 0; i < sl->nfuncs; i++)
		if (strcmp(sl->funcs[i].name, name) == 0) return i;
	return -1;
}

static VarEnt *varLookup(SL *sl, const char *name)
{
	int s, v;
	for (s = sl->nscopes - 1; s >= 0; s--)
		for (v = sl->scopes[s].n - 1; v >= 0; v--)
			if (strcmp(sl->scopes[s].vars[v].name, name) == 0)
				return &sl->scopes[s].vars[v];
	return NULL;
}

static void varDeclare(SL *sl, const char *name, int sid, int ndims)
{
	ScopeEnt *sc = &sl->scopes[sl->nscopes - 1];
	if (sc->n >= SL_MAXVARS) return;
	snprintf(sc->vars[sc->n].name, sizeof(sc->vars[0].name), "%s", name);
	sc->vars[sc->n].sid = sid;
	sc->vars[sc->n].ndims = ndims;
	sc->n++;
}

static int slScopePush(SL *sl)
{
	if (sl->nscopes >= SL_MAXSCOPES) return 0;
	sl->scopes[sl->nscopes].n = 0;
	sl->nscopes++;
	return 1;
}

/* struct leaves: path name, type, and dimensions */
static int collectLeaves(SL *sl, int sid, const char *prefix, char dims[][16], int nd, Leaf *out, int *nout)
{
	const StructDef *sd = &sl->structs[sid];
	int f, k;
	for (f = 0; f < sd->nfields; f++) {
		const Field *fl = &sd->fields[f];
		char name[256];
		char d2[SL_MAXDIMS][16];
		int n2 = nd;
		for (k = 0; k < nd; k++) memcpy(d2[k], dims[k], 16);
		for (k = 0; k < fl->ndims && n2 < SL_MAXDIMS; k++) memcpy(d2[n2++], fl->dims[k], 16);
		if (prefix[0]) snprintf(name, sizeof(name), "%s$%s", prefix, fl->name);
		else           snprintf(name, sizeof(name), "%s", fl->name);
		if (fl->sid >= 0) {
			if (!collectLeaves(sl, fl->sid, name, d2, n2, out, nout)) return 0;
		} else {
			Leaf *lf;
			if (*nout >= SL_MAXLEAVES) return 0;
			lf = &out[(*nout)++];
			snprintf(lf->suffix, sizeof(lf->suffix), "%s", name);
			snprintf(lf->typ, sizeof(lf->typ), "%s", fl->typ);
			lf->ndims = n2;
			for (k = 0; k < n2; k++) memcpy(lf->dims[k], d2[k], 16);
		}
	}
	return 1;
}

/* ---------------------------------------------------------------- struct type definitions */

/* struct Name { fields } ;   returns the index of the token after ';' */
static int parseStructDef(SL *sl, int i)
{
	StructDef *sd;
	int line = sl->toks[i].line;
	StructDef *ns;

	if (!isKind(sl, i + 1, K_ID) || !isTok(sl, i + 2, "{")) {
		slError(sl, line, "Expected syntax: struct Name { fields };", NULL);
		return -1;
	}
	if (structFind(sl, sl->toks[i + 1].text) >= 0) {
		slError(sl, line, "Struct type already defined", sl->toks[i + 1].text);
		return -1;
	}
	ns = (StructDef *)realloc(sl->structs, (size_t)(sl->nstructs + 1) * sizeof(StructDef));
	if (!ns) return -1;
	sl->structs = ns;
	sd = &sl->structs[sl->nstructs];
	memset(sd, 0, sizeof(*sd));
	snprintf(sd->name, sizeof(sd->name), "%s", sl->toks[i + 1].text);
	i += 3;

	while (!isTok(sl, i, "}")) {
		Field tmpl;
		int isArr = 0;
		memset(&tmpl, 0, sizeof(tmpl));
		tmpl.sid = -1;

		if (sl->toks[i].kind == K_EOF) {
			slError(sl, line, "Unterminated struct definition.", NULL);
			return -1;
		}
		if (isTok(sl, i, "array")) {
			/* array name[N]... of type|Struct ; */
			if (!isKind(sl, i + 1, K_ID)) { slError(sl, sl->toks[i].line, "Expected array field name.", NULL); return -1; }
			snprintf(tmpl.name, sizeof(tmpl.name), "%s", sl->toks[i + 1].text);
			i += 2;
			while (isTok(sl, i, "[")) {
				if (!isKind(sl, i + 1, K_NUM) || !isTok(sl, i + 2, "]") || tmpl.ndims >= SL_MAXDIMS) {
					slError(sl, sl->toks[i].line, "Invalid array field size (expected an integer constant).", NULL);
					return -1;
				}
				snprintf(tmpl.dims[tmpl.ndims++], 16, "%s", sl->toks[i + 1].text);
				i += 3;
			}
			if (tmpl.ndims == 0 || !isTok(sl, i, "of") || !isKind(sl, i + 1, K_ID)) {
				slError(sl, sl->toks[i].line, "Array field syntax: array name[N] of type;", NULL);
				return -1;
			}
			isArr = 1;
			if (isTypeKw(sl->toks[i + 1].text)) {
				snprintf(tmpl.typ, sizeof(tmpl.typ), "%s", sl->toks[i + 1].text);
			} else {
				tmpl.sid = structFind(sl, sl->toks[i + 1].text);
				if (tmpl.sid < 0) { slError(sl, sl->toks[i + 1].line, "Unknown type", sl->toks[i + 1].text); return -1; }
			}
			i += 2;
			if (!isTok(sl, i, ";")) { slError(sl, sl->toks[i].line, "Expected ';' after field.", NULL); return -1; }
			i++;
		} else if (isKind(sl, i, K_ID) && (isTypeKw(sl->toks[i].text) || structFind(sl, sl->toks[i].text) >= 0)) {
			/* type name {, name} ; */
			char tyname[64];
			snprintf(tyname, sizeof(tyname), "%s", sl->toks[i].text);
			if (isTypeKw(tyname)) snprintf(tmpl.typ, sizeof(tmpl.typ), "%.15s", tyname);
			else                  tmpl.sid = structFind(sl, tyname);
			i++;
			for (;;) {
				if (!isKind(sl, i, K_ID)) { slError(sl, sl->toks[i].line, "Expected field name.", NULL); return -1; }
				snprintf(tmpl.name, sizeof(tmpl.name), "%s", sl->toks[i].text);
				i++;
				if (isTok(sl, i, ",")) {
					/* multiple fields of the same type: register this one and continue */
					int k;
					for (k = 0; k < sd->nfields; k++)
						if (strcmp(sd->fields[k].name, tmpl.name) == 0) { slError(sl, sl->toks[i].line, "Duplicate field", tmpl.name); return -1; }
					if (sd->nfields >= SL_MAXFIELDS) { slError(sl, sl->toks[i].line, "Too many fields.", NULL); return -1; }
					sd->fields[sd->nfields++] = tmpl;
					i++;
					continue;
				}
				break;
			}
			if (!isTok(sl, i, ";")) { slError(sl, sl->toks[i].line, "Expected ';' after field.", NULL); return -1; }
			i++;
		} else {
			slError(sl, sl->toks[i].line, "Invalid field in struct definition", sl->toks[i].text);
			return -1;
		}
		(void)isArr;
		{
			int k;
			for (k = 0; k < sd->nfields; k++)
				if (strcmp(sd->fields[k].name, tmpl.name) == 0) { slError(sl, sl->toks[i].line, "Duplicate field", tmpl.name); return -1; }
			if (sd->nfields >= SL_MAXFIELDS) { slError(sl, sl->toks[i].line, "Too many fields.", NULL); return -1; }
			sd->fields[sd->nfields++] = tmpl;
		}
	}
	i++; /* '}' */
	if (!isTok(sl, i, ";")) {
		slError(sl, sl->toks[i].line, "Expected ';' after '}' in struct definition.", NULL);
		return -1;
	}
	if (sd->nfields == 0) {
		slError(sl, line, "Struct has no fields.", NULL);
		return -1;
	}
	sl->nstructs++;
	return i + 1;
}

/* ---------------------------------------------------------------- preliminary scan */

static void funcAdd(SL *sl, const char *name, int retSid)
{
	FuncEnt *nf;
	if (funcFindSl(sl, name) >= 0) return;
	nf = (FuncEnt *)realloc(sl->funcs, (size_t)(sl->nfuncs + 1) * sizeof(FuncEnt));
	if (!nf) return;
	sl->funcs = nf;
	snprintf(sl->funcs[sl->nfuncs].name, sizeof(sl->funcs[0].name), "%s", name);
	sl->funcs[sl->nfuncs].retSid = retSid;
	sl->nfuncs++;
}

/* First pass: detects and removes struct definitions, and registers top-level functions */
static int prescan(SL *sl, int *removeFlags)
{
	int i = 0, depth = 0;
	while (sl->toks[i].kind != K_EOF) {
		if (isTok(sl, i, "struct") && sl->toks[i].kind == K_ID) {
			int j;
			if (depth != 0) {
				slError(sl, sl->toks[i].line, "Structs must be defined at the outermost level, outside begin...end blocks.", NULL);
				return 0;
			}
			j = parseStructDef(sl, i);
			if (j < 0) return 0;
			{
				int k;
				for (k = i; k < j; k++) removeFlags[k] = 1;
			}
			i = j;
			continue;
		}
		if (isTok(sl, i, "begin") && sl->toks[i].kind == K_ID) depth++;
		else if (isTok(sl, i, "end") && sl->toks[i].kind == K_ID) depth--;
		else if (depth == 0 && sl->toks[i].kind == K_ID && isKind(sl, i + 1, K_ID) && isTok(sl, i + 2, "(")) {
			const char *t = sl->toks[i].text;
			if (strcmp(t, "void") == 0 || isTypeKw(t))
				funcAdd(sl, sl->toks[i + 1].text, -1);
			else if (structFind(sl, t) >= 0)
				funcAdd(sl, sl->toks[i + 1].text, structFind(sl, t));
		}
		i++;
	}
	return 1;
}

/* ---------------------------------------------------------------- token utility */

/* index of the ')' or ']' token closing the one at i (which is '(' or '['); -1 if not found */
static int matchClose(const SL *sl, int i)
{
	int depth = 0, k;
	for (k = i; sl->toks[k].kind != K_EOF; k++) {
		if (sl->toks[k].kind == K_PUNCT) {
			char c = sl->toks[k].text[0];
			if (sl->toks[k].text[1] == '\0') {
				if (c == '(' || c == '[') depth++;
				else if (c == ')' || c == ']') {
					depth--;
					if (depth == 0) return k;
				}
			}
		}
	}
	return -1;
}

static int newlinesBetween(const SL *sl, int a, int b)
{
	int n = 0, k;
	for (k = a; k < b; k++)
		if (sl->src[k] == '\n') n++;
	return n;
}

static void rewriteRange(SL *sl, int i0, int i1, Buf *out, int flat);

/* rewrites range [a,b) into an allocated string (flat mode) */
static char *rewriteToString(SL *sl, int a, int b)
{
	Buf tmp;
	memset(&tmp, 0, sizeof(tmp));
	rewriteRange(sl, a, b, &tmp, 1);
	if (!tmp.s) {
		tmp.s = (char *)malloc(1);
		if (tmp.s) tmp.s[0] = '\0';
	}
	return tmp.s;
}

static void pathFree(Path *p)
{
	int k;
	for (k = 0; k < p->nidx; k++) free(p->idx[k]);
	p->nidx = 0;
}

/* Parses a path starting from an identifier that is a struct variable.
   Returns 0 on error (already reported)). */
static int parsePath(SL *sl, int i, int iend, Path *p)
{
	VarEnt *v = varLookup(sl, sl->toks[i].text);
	int sid;

	memset(p, 0, sizeof(*p));
	p->line = sl->toks[i].line;
	snprintf(p->name, sizeof(p->name), "%s", sl->toks[i].text);
	sid = v->sid;
	p->sid = sid;
	p->total = v->ndims;
	i++;

	for (;;) {
		while (i < iend && isTok(sl, i, "[") && p->nidx < p->total) {
			int close = matchClose(sl, i);
			if (close < 0 || close >= iend) {
				slError(sl, p->line, "Unclosed bracket ']'.", NULL);
				pathFree(p);
				return 0;
			}
			p->idx[p->nidx++] = rewriteToString(sl, i + 1, close);
			i = close + 1;
		}
		if (i < iend && isTok(sl, i, ".")) {
			const StructDef *sd;
			int f;
			if (p->isLeaf) {
				slError(sl, p->line, "Field is not a struct: invalid '.'.", NULL);
				pathFree(p);
				return 0;
			}
			if (p->nidx < p->total) {
				slError(sl, p->line, "Missing indices before field access.", NULL);
				pathFree(p);
				return 0;
			}
			if (!isKind(sl, i + 1, K_ID)) {
				slError(sl, p->line, "Expected field name after '.'.", NULL);
				pathFree(p);
				return 0;
			}
			sd = &sl->structs[p->sid];
			for (f = 0; f < sd->nfields; f++)
				if (strcmp(sd->fields[f].name, sl->toks[i + 1].text) == 0) break;
			if (f == sd->nfields) {
				char msg[200];
				snprintf(msg, sizeof(msg), "Struct '%s' has no field", sd->name);
				slError(sl, p->line, msg, sl->toks[i + 1].text);
				pathFree(p);
				return 0;
			}
			{
				size_t l = strlen(p->name);
				snprintf(p->name + l, sizeof(p->name) - l, "$%s", sd->fields[f].name);
			}
			p->total += sd->fields[f].ndims;
			if (sd->fields[f].sid >= 0) {
				p->sid = sd->fields[f].sid;
			} else {
				p->isLeaf = 1;
				p->leafNdims = sd->fields[f].ndims;
			}
			i += 2;
			continue;
		}
		break;
	}

	p->next = i;
	if (p->nidx != p->total && p->nidx != 0) {
		slError(sl, p->line, "Mismatched number of indices.", NULL);
		pathFree(p);
		return 0;
	}
	if (p->isLeaf && p->nidx == 0 && p->total > 0 && p->total != p->leafNdims) {
		slError(sl, p->line, "To use an array of structs, the array field requires all indices.", NULL);
		pathFree(p);
		return 0;
	}
	return 1;
}

/* text of a leaf (or scalar/element) with the path indices */
static void pathLeafText(const Path *p, Buf *out)
{
	int k;
	bufStr(out, p->name);
	for (k = 0; k < p->nidx; k++) {
		bufCh(out, '[');
		bufStr(out, p->idx[k]);
		bufCh(out, ']');
	}
}

/* expands a whole struct into the list of leaf names; returns leaf count, -1 on error */
static int expandWhole(SL *sl, const Path *p, char **names, int maxn, int forAssign)
{
	Leaf leaves[SL_MAXLEAVES];
	int nl = 0, k;
	char noDims[1][16];

	if (forAssign && p->nidx == 0 && p->total > 0) {
		slError(sl, p->line, "Assignment of an entire array of structs is not supported (assign individual elements).", NULL);
		return -1;
	}
	if (!collectLeaves(sl, p->sid, "", noDims, 0, leaves, &nl) || nl > maxn) {
		slError(sl, p->line, "Struct has too many fields.", NULL);
		return -1;
	}
	for (k = 0; k < nl; k++) {
		Buf b;
		int j;
		memset(&b, 0, sizeof(b));
		if (leaves[k].ndims > 0 && p->nidx > 0 && !forAssign) {
			slError(sl, p->line, "A struct with array fields cannot be copied or passed by value, field", leaves[k].suffix);
			free(b.s);
			for (j = 0; j < k; j++) free(names[j]);
			return -1;
		}
		bufStr(&b, p->name);
		bufCh(&b, '$');
		bufStr(&b, leaves[k].suffix);
		for (j = 0; j < p->nidx; j++) {
			bufCh(&b, '[');
			bufStr(&b, p->idx[j]);
			bufCh(&b, ']');
		}
		names[k] = b.s;
	}
	return nl;
}

/* ---------------------------------------------------------------- calls */

/* Rewrites call arguments: tok[open] = '(' ... tok[close] = ')'.
   Entire structs become the list of their fields. */
static int rewriteArgs(SL *sl, int open, int close, Buf *out, const char *extraLeading)
{
	int a = open + 1, first = 1;

	if (extraLeading) { /* unused */ }
	while (a < close) {
		int b = a, depth = 0;
		while (b < close) {
			if (sl->toks[b].kind == K_PUNCT && sl->toks[b].text[1] == '\0') {
				char c = sl->toks[b].text[0];
				if (c == '(' || c == '[') depth++;
				else if (c == ')' || c == ']') depth--;
				else if (c == ',' && depth == 0) break;
			}
			b++;
		}
		if (!first) bufStr(out, ", ");
		first = 0;
		if (isKind(sl, a, K_ID) && varLookup(sl, sl->toks[a].text) && varLookup(sl, sl->toks[a].text)->sid >= 0) {
			Path p;
			if (!parsePath(sl, a, b, &p)) return 0;
			if (p.next == b && !p.isLeaf) {
				char *names[SL_MAXLEAVES];
				int n = expandWhole(sl, &p, names, SL_MAXLEAVES, 0), k;
				if (n < 0) { pathFree(&p); return 0; }
				for (k = 0; k < n; k++) {
					if (k) bufStr(out, ", ");
					bufStr(out, names[k]);
					free(names[k]);
				}
				pathFree(&p);
			} else {
				pathFree(&p);
				rewriteRange(sl, a, b, out, 1);
			}
		} else {
			rewriteRange(sl, a, b, out, 1);
		}
		a = b + 1;
		if (b >= close) break;
	}
	return !sl->err;
}

/* ---------------------------------------------------------------- statements */

static void emitLeafDecl(Buf *out, const char *name, const Leaf *lf, char dims[][16], int nd)
{
	int k;
	if (nd + lf->ndims == 0) {
		bufStr(out, lf->typ);
		bufCh(out, ' ');
		bufStr(out, name);
		bufStr(out, "; ");
		return;
	}
	bufStr(out, "array ");
	bufStr(out, name);
	for (k = 0; k < nd; k++) { bufCh(out, '['); bufStr(out, dims[k]); bufCh(out, ']'); }
	for (k = 0; k < lf->ndims; k++) { bufCh(out, '['); bufStr(out, lf->dims[k]); bufCh(out, ']'); }
	bufStr(out, " of ");
	bufStr(out, lf->typ);
	bufStr(out, "; ");
}

/* declares a struct variable (optionally an array of structs) */
static int emitStructVar(SL *sl, int line, const char *name, int sid, char dims[][16], int nd, Buf *out)
{
	Leaf leaves[SL_MAXLEAVES];
	int nl = 0, k;
	char noDims[1][16];
	(void)noDims;

	if (!collectLeaves(sl, sid, "", dims, 0, leaves, &nl)) {
		slError(sl, line, "Struct has too many fields.", NULL);
		return 0;
	}
	for (k = 0; k < nl; k++) {
		Buf nm;
		memset(&nm, 0, sizeof(nm));
		bufStr(&nm, name);
		bufCh(&nm, '$');
		bufStr(&nm, leaves[k].suffix);
		emitLeafDecl(out, nm.s ? nm.s : "", &leaves[k], dims, nd);
		free(nm.s);
	}
	varDeclare(sl, name, sid, nd);
	return 1;
}

/* Rewrites "type name(parameters)"; i = return type token. Returns the index of the token after ')'. */
static int rewriteFuncHeader(SL *sl, int i, Buf *out)
{
	int line = sl->toks[i].line;
	const char *retName = sl->toks[i].text;
	int retSid = structFind(sl, retName);
	int nameTok = i + 1;
	int open = i + 2;
	int close = matchClose(sl, open);
	int a, first = 1;
	Leaf retLeaves[SL_MAXLEAVES];
	int nRet = 0, k;
	char noDims[1][16];

	if (close < 0) {
		slError(sl, line, "Unclosed parenthesis ')' in function header.", NULL);
		return -1;
	}
	sl->npending = 0;
	sl->curRetSid = retSid;

	if (retSid >= 0) {
		if (!collectLeaves(sl, retSid, "", noDims, 0, retLeaves, &nRet)) {
			slError(sl, line, "Struct has too many fields.", NULL);
			return -1;
		}
		for (k = 0; k < nRet; k++)
			if (retLeaves[k].ndims > 0) {
				slError(sl, line, "A function cannot return a struct with array fields, field", retLeaves[k].suffix);
				return -1;
			}
		bufStr(out, "void ");
	} else {
		bufStr(out, retName);
		bufCh(out, ' ');
	}
	bufStr(out, sl->toks[nameTok].text);
	bufCh(out, '(');

	a = open + 1;
	while (a < close) {
		int b = a;
		int isRef = 0, isArr = 0, j;
		char pname[64];
		int tyTok;
		int ndimsP = 0;

		while (b < close && !isTok(sl, b, ",")) b++;
		j = a;
		if (isTok(sl, j, "ref")) { isRef = 1; j++; }
		if (!first) bufStr(out, ", ");
		first = 0;

		if (isTok(sl, j, "array")) {
			/* array name[]...[] of type */
			int sidP = -1;
			isArr = 1;
			if (!isKind(sl, j + 1, K_ID)) { slError(sl, line, "Expected array parameter name.", NULL); return -1; }
			snprintf(pname, sizeof(pname), "%s", sl->toks[j + 1].text);
			j += 2;
			while (isTok(sl, j, "[") && isTok(sl, j + 1, "]")) { ndimsP++; j += 2; }
			if (!isTok(sl, j, "of") || !isKind(sl, j + 1, K_ID)) {
				/* unrecognized form: copy verbatim, leave error reporting to the parser */
				for (k = a; k < b; k++) { if (k > a) bufCh(out, ' '); bufStr(out, sl->toks[k].text); }
				a = b + 1;
				continue;
			}
			tyTok = j + 1;
			sidP = structFind(sl, sl->toks[tyTok].text);
			if (sidP < 0) {
				for (k = a; k < b; k++) { if (k > a) bufCh(out, ' '); bufStr(out, sl->toks[k].text); }
				if (sl->npending < 64) {
					snprintf(sl->pending[sl->npending].name, 64, "%s", pname);
					sl->pending[sl->npending].sid = -1;
					sl->pending[sl->npending].ndims = 0;
					sl->npending++;
				}
			} else {
				Leaf lv[SL_MAXLEAVES];
				int nl = 0;
				if (!collectLeaves(sl, sidP, "", noDims, 0, lv, &nl)) { slError(sl, line, "Struct has too many fields.", NULL); return -1; }
				for (k = 0; k < nl; k++) {
					int d;
					if (lv[k].ndims > 0) { slError(sl, line, "A struct array parameter cannot have array fields, field", lv[k].suffix); return -1; }
					if (k) bufStr(out, ", ");
					bufStr(out, "array ");
					bufStr(out, pname);
					bufCh(out, '$');
					bufStr(out, lv[k].suffix);
					for (d = 0; d < ndimsP; d++) bufStr(out, "[]");
					bufStr(out, " of ");
					bufStr(out, lv[k].typ);
				}
				if (sl->npending < 64) {
					snprintf(sl->pending[sl->npending].name, 64, "%s", pname);
					sl->pending[sl->npending].sid = sidP;
					sl->pending[sl->npending].ndims = ndimsP;
					sl->npending++;
				}
			}
		} else if (isKind(sl, j, K_ID) && structFind(sl, sl->toks[j].text) >= 0 && isKind(sl, j + 1, K_ID)) {
			int sidP = structFind(sl, sl->toks[j].text);
			Leaf lv[SL_MAXLEAVES];
			int nl = 0;
			snprintf(pname, sizeof(pname), "%s", sl->toks[j + 1].text);
			if (!collectLeaves(sl, sidP, "", noDims, 0, lv, &nl)) { slError(sl, line, "Struct has too many fields.", NULL); return -1; }
			for (k = 0; k < nl; k++) {
				if (k) bufStr(out, ", ");
				if (lv[k].ndims > 0) {
					int d;
					/* array field: always by reference, and the struct cannot be passed by value */
					if (!isRef) { slError(sl, line, "A struct with array fields can only be passed with 'ref', field", lv[k].suffix); return -1; }
					bufStr(out, "array ");
					bufStr(out, pname);
					bufCh(out, '$');
					bufStr(out, lv[k].suffix);
					for (d = 0; d < lv[k].ndims; d++) bufStr(out, "[]");
					bufStr(out, " of ");
					bufStr(out, lv[k].typ);
				} else {
					if (isRef) bufStr(out, "ref ");
					bufStr(out, lv[k].typ);
					bufCh(out, ' ');
					bufStr(out, pname);
					bufCh(out, '$');
					bufStr(out, lv[k].suffix);
				}
			}
			if (sl->npending < 64) {
				snprintf(sl->pending[sl->npending].name, 64, "%s", pname);
				sl->pending[sl->npending].sid = sidP;
				sl->pending[sl->npending].ndims = 0;
				sl->npending++;
			}
		} else {
			/* regular parameter: copy verbatim; the name is the last identifier */
			for (k = a; k < b; k++) { if (k > a) bufCh(out, ' '); bufStr(out, sl->toks[k].text); }
			if (b - 1 >= a && sl->toks[b - 1].kind == K_ID && sl->npending < 64) {
				snprintf(sl->pending[sl->npending].name, 64, "%s", sl->toks[b - 1].text);
				sl->pending[sl->npending].sid = -1;
				sl->pending[sl->npending].ndims = 0;
				sl->npending++;
			}
		}
		(void)isArr;
		a = b + 1;
	}

	if (retSid >= 0) {
		for (k = 0; k < nRet; k++) {
			if (!first) bufStr(out, ", ");
			first = 0;
			bufStr(out, "ref ");
			bufStr(out, retLeaves[k].typ);
			bufStr(out, " zz_ret$");
			bufStr(out, retLeaves[k].suffix);
		}
		if (sl->npending < 64) {
			snprintf(sl->pending[sl->npending].name, 64, "zz_ret");
			sl->pending[sl->npending].sid = retSid;
			sl->pending[sl->npending].ndims = 0;
			sl->npending++;
		}
	}
	bufCh(out, ')');
	return close + 1;
}

/* ---------------------------------------------------------------- rewriting */

static void emitTok(SL *sl, int i, Buf *out, int flat, int *first)
{
	if (flat) {
		if (!*first) bufCh(out, ' ');
		*first = 0;
		bufStr(out, sl->toks[i].text);
	} else {
		bufAdd(out, sl->src + sl->pos, (size_t)(sl->toks[i].start - sl->pos));
		bufStr(out, sl->toks[i].text);
		sl->pos = sl->toks[i].end;
	}
}

/* replaces tokens [i, j) with the given text, preserving line count */
static void emitReplacement(SL *sl, int i, int j, const char *text, Buf *out, int flat, int *first)
{
	if (flat) {
		if (!*first) bufCh(out, ' ');
		*first = 0;
		bufStr(out, text);
	} else {
		int nl = newlinesBetween(sl, sl->toks[i].start, sl->toks[j - 1].end), k;
		bufAdd(out, sl->src + sl->pos, (size_t)(sl->toks[i].start - sl->pos));
		bufStr(out, text);
		for (k = 0; k < nl; k++) bufCh(out, '\n');
		sl->pos = sl->toks[j - 1].end;
	}
}

static void rewriteRange(SL *sl, int i0, int i1, Buf *out, int flat)
{
	int i = i0, first = 1;

	while (i < i1 && !sl->err) {
		const Tok *t = &sl->toks[i];

		if (t->kind != K_ID) { emitTok(sl, i, out, flat, &first); i++; continue; }

		/* ---- blocks and scope ---- */
		if (!flat && strcmp(t->text, "begin") == 0) {
			if (!slScopePush(sl)) { slError(sl, t->line, "Blocks nested too deeply.", NULL); return; }
			if (sl->nscopes == 2) {
				int k;
				for (k = 0; k < sl->npending; k++)
					varDeclare(sl, sl->pending[k].name, sl->pending[k].sid, sl->pending[k].ndims);
				sl->npending = 0;
			}
			emitTok(sl, i, out, flat, &first);
			i++;
			continue;
		}
		if (!flat && strcmp(t->text, "end") == 0) {
			if (sl->nscopes > 1) sl->nscopes--;
			if (sl->nscopes == 1) sl->curRetSid = -1;
			emitTok(sl, i, out, flat, &first);
			i++;
			continue;
		}

		/* ---- function headers (outermost level) ---- */
		if (!flat && sl->nscopes == 1 && isKind(sl, i + 1, K_ID) && isTok(sl, i + 2, "(") &&
		    (strcmp(t->text, "void") == 0 || isTypeKw(t->text) || structFind(sl, t->text) >= 0) &&
		    !isReserved(sl->toks[i + 1].text)) {
			Buf hdr;
			int j;
			memset(&hdr, 0, sizeof(hdr));
			j = rewriteFuncHeader(sl, i, &hdr);
			if (j < 0) { free(hdr.s); return; }
			emitReplacement(sl, i, j, hdr.s ? hdr.s : "", out, flat, &first);
			free(hdr.s);
			i = j;
			if (isTok(sl, i, ";")) { sl->npending = 0; sl->curRetSid = -1; }
			continue;
		}

		/* ---- declaration of struct variables: Name a, b; ---- */
		if (!flat && structFind(sl, t->text) >= 0 && isKind(sl, i + 1, K_ID) &&
		    (isTok(sl, i + 2, ";") || isTok(sl, i + 2, ",") || isTok(sl, i + 2, "="))) {
			int sid = structFind(sl, t->text), j = i + 1;
			Buf d;
			memset(&d, 0, sizeof(d));
			for (;;) {
				char nm[64];
				char noDims[1][16];
				if (!isKind(sl, j, K_ID)) { slError(sl, t->line, "Expected variable name.", NULL); free(d.s); return; }
				snprintf(nm, sizeof(nm), "%s", sl->toks[j].text);
				if (isTok(sl, j + 1, "=")) { slError(sl, t->line, "Structs cannot be initialized in declaration.", NULL); free(d.s); return; }
				if (!emitStructVar(sl, t->line, nm, sid, noDims, 0, &d)) { free(d.s); return; }
				j++;
				if (isTok(sl, j, ",")) { j++; continue; }
				break;
			}
			if (!isTok(sl, j, ";")) { slError(sl, t->line, "Expected ';' after declaration.", NULL); free(d.s); return; }
			j++;
			emitReplacement(sl, i, j, d.s ? d.s : "", out, flat, &first);
			free(d.s);
			i = j;
			continue;
		}

		/* ---- array of structs:  array v[N]... of Name;  (and regular arrays: register name only) ---- */
		if (!flat && strcmp(t->text, "array") == 0 && isKind(sl, i + 1, K_ID)) {
			int j = i + 2, nd = 0;
			char dims[SL_MAXDIMS][16];
			while (isTok(sl, j, "[") && isKind(sl, j + 1, K_NUM) && isTok(sl, j + 2, "]") && nd < SL_MAXDIMS) {
				snprintf(dims[nd++], 16, "%s", sl->toks[j + 1].text);
				j += 3;
			}
			if (nd > 0 && isTok(sl, j, "of") && isKind(sl, j + 1, K_ID) && structFind(sl, sl->toks[j + 1].text) >= 0 &&
			    isTok(sl, j + 2, ";")) {
				Buf d;
				memset(&d, 0, sizeof(d));
				if (!emitStructVar(sl, t->line, sl->toks[i + 1].text, structFind(sl, sl->toks[j + 1].text), dims, nd, &d)) {
					free(d.s);
					return;
				}
				emitReplacement(sl, i, j + 3, d.s ? d.s : "", out, flat, &first);
				free(d.s);
				i = j + 3;
				continue;
			}
			varDeclare(sl, sl->toks[i + 1].text, -1, 0);
			emitTok(sl, i, out, flat, &first);
			i++;
			continue;
		}

		/* ---- regular declarations: register names (may shadow a struct) ---- */
		if (!flat && isTypeKw(t->text) && isKind(sl, i + 1, K_ID) && !isTok(sl, i + 2, "(")) {
			int j = i + 1;
			for (;;) {
				int depth = 0;
				if (!isKind(sl, j, K_ID)) break;
				varDeclare(sl, sl->toks[j].text, -1, 0);
				j++;
				while (sl->toks[j].kind != K_EOF) {
					if (isTok(sl, j, "(") || isTok(sl, j, "[")) depth++;
					else if (isTok(sl, j, ")") || isTok(sl, j, "]")) depth--;
					else if (depth == 0 && (isTok(sl, j, ",") || isTok(sl, j, ";"))) break;
					j++;
				}
				if (isTok(sl, j, ",")) { j++; continue; }
				break;
			}
			emitTok(sl, i, out, flat, &first);
			i++;
			continue;
		}

		/* ---- struct return ---- */
		if (!flat && strcmp(t->text, "return") == 0 && sl->curRetSid >= 0) {
			Path p;
			if (!isKind(sl, i + 1, K_ID) || !varLookup(sl, sl->toks[i + 1].text) ||
			    varLookup(sl, sl->toks[i + 1].text)->sid != sl->curRetSid) {
				slError(sl, t->line, "In a function returning a struct, 'return' requires a struct variable of the same type.", NULL);
				return;
			}
			if (!parsePath(sl, i + 1, i1, &p)) return;
			if (p.isLeaf || p.sid != sl->curRetSid || !isTok(sl, p.next, ";")) {
				slError(sl, t->line, "'return' requires a struct variable (or an element of an array of structs) of the same type.", NULL);
				pathFree(&p);
				return;
			}
			{
				char *src[SL_MAXLEAVES];
				Leaf leaves[SL_MAXLEAVES];
				int nl = 0, n, k;
				char noDims[1][16];
				Buf r;
				memset(&r, 0, sizeof(r));
				n = expandWhole(sl, &p, src, SL_MAXLEAVES, 1);
				if (n < 0) { pathFree(&p); return; }
				collectLeaves(sl, sl->curRetSid, "", noDims, 0, leaves, &nl);
				bufStr(&r, "begin ");
				for (k = 0; k < n; k++) {
					bufStr(&r, "zz_ret$");
					bufStr(&r, leaves[k].suffix);
					bufStr(&r, " = ");
					bufStr(&r, src[k]);
					bufStr(&r, "; ");
					free(src[k]);
				}
				bufStr(&r, "return; end");
				emitReplacement(sl, i, p.next + 1, r.s, out, flat, &first);
				free(r.s);
				i = p.next + 1;
				pathFree(&p);
			}
			continue;
		}

		/* ---- path on a struct variable ---- */
		{
			VarEnt *v = varLookup(sl, t->text);
			if (v && v->sid >= 0 && !(i > i0 && isTok(sl, i - 1, "."))) {
				Path p;
				if (!parsePath(sl, i, i1, &p)) return;
				if (p.isLeaf) {
					Buf leaf;
					memset(&leaf, 0, sizeof(leaf));
					pathLeafText(&p, &leaf);
					emitReplacement(sl, i, p.next, leaf.s ? leaf.s : "", out, flat, &first);
					free(leaf.s);
					i = p.next;
					pathFree(&p);
					continue;
				}
				/* whole struct: allowed only as  A = B;  or  A = f(...); */
				if (!flat && isTok(sl, p.next, "=")) {
					char *lhs[SL_MAXLEAVES];
					int nlhs;
					int r = p.next + 1;
					Buf res;
					memset(&res, 0, sizeof(res));

					nlhs = expandWhole(sl, &p, lhs, SL_MAXLEAVES, 1);
					if (nlhs < 0) { pathFree(&p); return; }

					if (isKind(sl, r, K_ID) && varLookup(sl, sl->toks[r].text) && varLookup(sl, sl->toks[r].text)->sid >= 0) {
						Path q;
						char *rhs[SL_MAXLEAVES];
						int nr, k;
						if (!parsePath(sl, r, i1, &q)) {
							for (k = 0; k < nlhs; k++) free(lhs[k]);
							pathFree(&p);
							return;
						}
						if (q.isLeaf || q.sid != p.sid || !isTok(sl, q.next, ";")) {
							slError(sl, t->line, "Assignment requires two structs of the same type.", NULL);
							for (k = 0; k < nlhs; k++) free(lhs[k]);
							pathFree(&p); pathFree(&q);
							return;
						}
						nr = expandWhole(sl, &q, rhs, SL_MAXLEAVES, 1);
						if (nr != nlhs) {
							for (k = 0; k < nlhs; k++) free(lhs[k]);
							if (nr > 0) for (k = 0; k < nr; k++) free(rhs[k]);
							pathFree(&p); pathFree(&q);
							return;
						}
						{
							Leaf lv[SL_MAXLEAVES];
							int nlv = 0, maxd = 0, d;
							char noDims[1][16];
							collectLeaves(sl, p.sid, "", noDims, 0, lv, &nlv);
							bufStr(&res, "begin ");
							for (k = 0; k < nlv; k++)
								if (lv[k].ndims > maxd) maxd = lv[k].ndims;
							for (d = 0; d < maxd; d++) {
								char nm[40];
								snprintf(nm, sizeof(nm), "int zz_k%d; ", d);
								bufStr(&res, nm);
							}
							for (k = 0; k < nlhs; k++) {
								if (k < nlv && lv[k].ndims > 0) {
									/* array field: copy element by element using loops */
									for (d = 0; d < lv[k].ndims; d++) {
										char nm[200];
										snprintf(nm, sizeof(nm), "zz_k%d = 0; while (zz_k%d < %s) begin ", d, d, lv[k].dims[d]);
										bufStr(&res, nm);
									}
									bufStr(&res, lhs[k]);
									for (d = 0; d < lv[k].ndims; d++) { char nm[24]; snprintf(nm, sizeof(nm), "[zz_k%d]", d); bufStr(&res, nm); }
									bufStr(&res, " = ");
									bufStr(&res, rhs[k]);
									for (d = 0; d < lv[k].ndims; d++) { char nm[24]; snprintf(nm, sizeof(nm), "[zz_k%d]", d); bufStr(&res, nm); }
									bufStr(&res, "; ");
									for (d = lv[k].ndims - 1; d >= 0; d--) {
										char nm[64];
										snprintf(nm, sizeof(nm), "zz_k%d = zz_k%d + 1; end ", d, d);
										bufStr(&res, nm);
									}
								} else {
									bufStr(&res, lhs[k]);
									bufStr(&res, " = ");
									bufStr(&res, rhs[k]);
									bufStr(&res, "; ");
								}
								free(rhs[k]);
								free(lhs[k]);
							}
						}
						bufStr(&res, "end");
						emitReplacement(sl, i, q.next + 1, res.s, out, flat, &first);
						free(res.s);
						i = q.next + 1;
						pathFree(&p);
						pathFree(&q);
						continue;
					} else if (isKind(sl, r, K_ID) && isTok(sl, r + 1, "(") && funcFindSl(sl, sl->toks[r].text) >= 0 &&
					           sl->funcs[funcFindSl(sl, sl->toks[r].text)].retSid == p.sid) {
						int open = r + 1, close = matchClose(sl, open), k;
						if (close < 0 || !isTok(sl, close + 1, ";")) {
							slError(sl, t->line, "Invalid function call.", NULL);
							for (k = 0; k < nlhs; k++) free(lhs[k]);
							pathFree(&p);
							return;
						}
						bufStr(&res, sl->toks[r].text);
						bufCh(&res, '(');
						{
							Buf args;
							memset(&args, 0, sizeof(args));
							if (!rewriteArgs(sl, open, close, &args, NULL)) {
								free(args.s);
								for (k = 0; k < nlhs; k++) free(lhs[k]);
								pathFree(&p);
								free(res.s);
								return;
							}
							if (args.s && args.n > 0) { bufStr(&res, args.s); bufStr(&res, ", "); }
							free(args.s);
						}
						for (k = 0; k < nlhs; k++) {
							if (k) bufStr(&res, ", ");
							bufStr(&res, lhs[k]);
							free(lhs[k]);
						}
						bufStr(&res, ");");
						emitReplacement(sl, i, close + 2, res.s, out, flat, &first);
						free(res.s);
						i = close + 2;
						pathFree(&p);
						continue;
					} else {
						int k;
						slError(sl, t->line, "Assigning a struct requires a struct of the same type or a function call returning it.", NULL);
						for (k = 0; k < nlhs; k++) free(lhs[k]);
						pathFree(&p);
						return;
					}
				}
				slError(sl, t->line, "A whole struct can only be used in an assignment, as a function argument, or in 'return'", t->text);
				pathFree(&p);
				return;
			}
		}

		/* ---- function calls ---- */
		if (isTok(sl, i + 1, "(") && !isReserved(t->text) && !varLookup(sl, t->text) && funcFindSl(sl, t->text) >= 0) {
			int open = i + 1, close = matchClose(sl, open);
			Buf call;
			if (close < 0 || close >= i1) {
				slError(sl, t->line, "Unclosed parenthesis ')'.", NULL);
				return;
			}
			if (sl->funcs[funcFindSl(sl, t->text)].retSid >= 0) {
				slError(sl, t->line, "Function returns a struct: it can only be used as  p = name(...);  with p of the same type", t->text);
				return;
			}
			memset(&call, 0, sizeof(call));
			bufStr(&call, t->text);
			bufCh(&call, '(');
			if (!rewriteArgs(sl, open, close, &call, NULL)) { free(call.s); return; }
			bufCh(&call, ')');
			emitReplacement(sl, i, close + 1, call.s, out, flat, &first);
			free(call.s);
			i = close + 1;
			continue;
		}

		emitTok(sl, i, out, flat, &first);
		i++;
	}
}

/* ---------------------------------------------------------------- start point */

int structsLower(CompilerContext *ctx)
{
	SL *sl;
	int *removeFlags = NULL;
	int ok = 1, hasStruct = 0, i;
	size_t srcLen;

	if (!ctx->g_szInput) return 1;

	/* fast track: no word 'struct' */
	{
		const char *p = ctx->g_szInput;
		while ((p = strstr(p, "struct")) != NULL) {
			if ((p == ctx->g_szInput || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
			    !(isalnum((unsigned char)p[6]) || p[6] == '_')) {
				hasStruct = 1;
				break;
			}
			p += 6;
		}
	}
	if (!hasStruct) return 1;

	sl = (SL *)calloc(1, sizeof(SL));
	if (!sl) return 0;
	sl->ctx = ctx;
	sl->src = ctx->g_szInput;
	srcLen = strlen(sl->src);
	sl->curRetSid = -1;
	sl->nscopes = 1;
	sl->scopes[0].n = 0;

	if (!tokenize(sl)) { ok = 0; goto done; }

	removeFlags = (int *)calloc((size_t)sl->ntok + 1, sizeof(int));
	if (!removeFlags) { ok = 0; goto done; }
	if (!prescan(sl, removeFlags)) { ok = 0; goto done; }

	/* builds output skipping struct definitions (preserving line numbers) */
	{
		int k = 0;
		sl->pos = 0;
		while (sl->toks[k].kind != K_EOF && !sl->err) {
			if (removeFlags[k]) {
				int j = k;
				while (removeFlags[j]) j++;
				{
					int nl = newlinesBetween(sl, sl->toks[k].start, sl->toks[j - 1].end), n;
					bufAdd(&sl->out, sl->src + sl->pos, (size_t)(sl->toks[k].start - sl->pos));
					for (n = 0; n < nl; n++) bufCh(&sl->out, '\n');
					sl->pos = sl->toks[j - 1].end;
				}
				k = j;
			} else {
				int j = k;
				while (sl->toks[j].kind != K_EOF && !removeFlags[j]) j++;
				rewriteRange(sl, k, j, &sl->out, 0);
				k = j;
			}
		}
		if (!sl->err)
			bufAdd(&sl->out, sl->src + sl->pos, srcLen - (size_t)sl->pos);
	}
	if (sl->err) ok = 0;

done:
	if (ok && sl->out.s) {
		if (getenv("MYCOMPILER_DUMP_LOWERED"))
			fprintf(stderr, "%s\n", sl->out.s);
		free(ctx->g_szInput);
		ctx->g_szInput = sl->out.s;
		sl->out.s = NULL;
	}
	free(sl->out.s);
	free(removeFlags);
	for (i = 0; i < sl->ntok; i++) free(sl->toks[i].text);
	free(sl->toks);
	free(sl->structs);
	free(sl->funcs);
	free(sl);
	return ok;
}
