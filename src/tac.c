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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac.h"

static int s_label = 1;
static int s_temp = 1;

static char *tacStrdup(const char *s)
{
    char *p;
    size_t len;
    if (!s) return NULL;
    len = strlen(s) + 1;
    p = (char *)malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

static char *newTemp(void)
{
    char buf[32];
    sprintf(buf, "t%d", s_temp++);
    return tacStrdup(buf);
}

static char *newLabel(void)
{
    char buf[32];
    sprintf(buf, "L%03d", s_label++);
    return tacStrdup(buf);
}

void tacInit(TACList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void tacFree(TACList *list)
{
    int i;
    for (i = 0; i < list->count; i++) {
        free(list->items[i].arg1);
        free(list->items[i].arg2);
        free(list->items[i].res);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void tacEmit(TACList *list, TACOp op, const char *arg1, const char *arg2, const char *res, enumTipo1 t1)
{
    TACInstr *in;
    if (list->count >= list->capacity) {
        int newCap = (list->capacity == 0) ? 64 : list->capacity * 2;
        TACInstr *newItems = (TACInstr *)realloc(list->items, (size_t)newCap * sizeof(TACInstr));
        if (!newItems) {
            printf("Errore TAC: memoria insufficiente.\n");
            return;
        }
        list->items = newItems;
        list->capacity = newCap;
    }
    in = &list->items[list->count++];
    in->op = op;
    in->arg1 = tacStrdup(arg1);
    in->arg2 = tacStrdup(arg2);
    in->res = tacStrdup(res);
    in->type1 = t1;
    in->dead = 0;
}

/* =========================================================================
 * PASSAGGIO 1: Ottimizzazione sull'AST (Constant Folding & Semplificazioni)
 * ========================================================================= */

static int isZeroConst(nodeType *n)
{
    if (!n || n->type != typeConst) return 0;
    if (n->type1 == T_INTEGER) return n->con.value.iVal == 0;
    if (n->type1 == T_REAL)    return n->con.value.dblVal == 0.0;
    return 0;
}

static int isOneConst(nodeType *n)
{
    if (!n || n->type != typeConst) return 0;
    if (n->type1 == T_INTEGER) return n->con.value.iVal == 1;
    if (n->type1 == T_REAL)    return n->con.value.dblVal == 1.0;
    return 0;
}

nodeType *tacOptimizeAST(nodeType *p)
{
    int i;
    if (!p) return NULL;
    if (p->type != typeOperator) return p;

    for (i = 0; i < p->opr.nops; i++) {
        p->opr.op[i] = tacOptimizeAST(p->opr.op[i]);
    }

    /* Constant folding unario: -costante, !costante */
    if (p->opr.oper == T_OP_UMINUS && p->opr.nops >= 1 &&
        p->opr.op[0] != NULL && p->opr.op[0]->type == typeConst) {
        nodeType *c = p->opr.op[0];
        Valore v = c->con.value;
        char lex[64];
        if (c->type1 == T_INTEGER) {
            v.iVal = -v.iVal;
            sprintf(lex, "%d", v.iVal);
        } else if (c->type1 == T_REAL) {
            v.dblVal = -v.dblVal;
            sprintf(lex, "%.15g", v.dblVal);
        } else {
            return p;
        }
        nodeType *res = con(c->type1, lex, v);
        freeNode(p);
        return res;
    }

    if (p->opr.oper == T_OP_NOT && p->opr.nops >= 1 &&
        p->opr.op[0] != NULL && p->opr.op[0]->type == typeConst &&
        p->opr.op[0]->type1 == T_BOOLEAN) {
        Valore v = p->opr.op[0]->con.value;
        char lex[16];
        v.bVal = v.bVal ? FALSE : TRUE;
        strcpy(lex, v.bVal ? "true" : "false");
        nodeType *res = con(T_BOOLEAN, lex, v);
        freeNode(p);
        return res;
    }

    /* Constant folding binario aritmetico */
    if (p->opr.nops == 2 && p->opr.op[0] != NULL && p->opr.op[1] != NULL &&
        p->opr.op[0]->type == typeConst && p->opr.op[1]->type == typeConst) {
        nodeType *l = p->opr.op[0];
        nodeType *r = p->opr.op[1];
        if ((l->type1 == T_INTEGER || l->type1 == T_REAL) &&
            (r->type1 == T_INTEGER || r->type1 == T_REAL)) {
            int isReal = (l->type1 == T_REAL || r->type1 == T_REAL);
            double dl = (l->type1 == T_REAL) ? l->con.value.dblVal : (double)l->con.value.iVal;
            double dr = (r->type1 == T_REAL) ? r->con.value.dblVal : (double)r->con.value.iVal;
            int il = l->con.value.iVal;
            int ir = r->con.value.iVal;
            Valore v;
            char lex[64];
            int folded = 1;

            switch (p->opr.oper) {
                case T_OP_PLUS:
                    if (isReal) { v.dblVal = dl + dr; sprintf(lex, "%.15g", v.dblVal); }
                    else        { v.iVal = il + ir;   sprintf(lex, "%d", v.iVal); }
                    break;
                case T_OP_MINUS:
                    if (isReal) { v.dblVal = dl - dr; sprintf(lex, "%.15g", v.dblVal); }
                    else        { v.iVal = il - ir;   sprintf(lex, "%d", v.iVal); }
                    break;
                case T_OP_MULT:
                    if (isReal) { v.dblVal = dl * dr; sprintf(lex, "%.15g", v.dblVal); }
                    else        { v.iVal = il * ir;   sprintf(lex, "%d", v.iVal); }
                    break;
                case T_OP_DIV:
                    if ((isReal && dr == 0.0) || (!isReal && ir == 0)) {
                        folded = 0;
                    } else if (isReal) {
                        v.dblVal = dl / dr; sprintf(lex, "%.15g", v.dblVal);
                    } else {
                        v.iVal = il / ir; sprintf(lex, "%d", v.iVal);
                    }
                    break;
                default:
                    folded = 0;
                    break;
            }
            if (folded) {
                nodeType *res = con(isReal ? T_REAL : T_INTEGER, lex, v);
                freeNode(p);
                return res;
            }
        }
    }

    /* Semplificazioni algebriche */
    if (p->opr.nops == 2 && p->opr.op[0] != NULL && p->opr.op[1] != NULL) {
        if (p->opr.oper == T_OP_PLUS) {
            if (isZeroConst(p->opr.op[1])) { nodeType *k = p->opr.op[0]; p->opr.op[0] = NULL; freeNode(p); return k; }
            if (isZeroConst(p->opr.op[0])) { nodeType *k = p->opr.op[1]; p->opr.op[1] = NULL; freeNode(p); return k; }
        }
        if (p->opr.oper == T_OP_MINUS && isZeroConst(p->opr.op[1])) {
            nodeType *k = p->opr.op[0]; p->opr.op[0] = NULL; freeNode(p); return k;
        }
        if (p->opr.oper == T_OP_MULT) {
            if (isOneConst(p->opr.op[1])) { nodeType *k = p->opr.op[0]; p->opr.op[0] = NULL; freeNode(p); return k; }
            if (isOneConst(p->opr.op[0])) { nodeType *k = p->opr.op[1]; p->opr.op[1] = NULL; freeNode(p); return k; }
            if (isZeroConst(p->opr.op[0]) || isZeroConst(p->opr.op[1])) {
                Valore v;
                char lex[16];
                enumTipo1 t = (p->type1 == T_REAL) ? T_REAL : T_INTEGER;
                if (t == T_REAL) v.dblVal = 0.0; else v.iVal = 0;
                strcpy(lex, "0");
                nodeType *res = con(t, lex, v);
                freeNode(p);
                return res;
            }
        }
        if (p->opr.oper == T_OP_DIV && isOneConst(p->opr.op[1])) {
            nodeType *k = p->opr.op[0]; p->opr.op[0] = NULL; freeNode(p); return k;
        }
    }

    return p;
}

/* =========================================================================
 * PASSAGGIO 2: Traduzione AST -> TACList
 * ========================================================================= */

static char *emitTAC(TACList *tac, Scope *pScope, nodeType *p);

static int typeByteSize(enumTipo1 t1)
{
    switch (t1) {
        case T_INTEGER: return (int)sizeof(int);
        case T_REAL:    return (int)sizeof(double);
        case T_BOOLEAN: return (int)sizeof(char);
        case T_STRING:  return (int)sizeof(char *);
    }
    return (int)sizeof(int);
}

static char *emitArrayOffset(TACList *tac, Scope *pScope, nodeType *idNode, HashTable *Dati)
{
    int size = typeByteSize(Dati->Tipo1);
    char szNum[32];

    if (Dati->Value.pArrayDef->countIndexes == 1) {
        char *idx = emitTAC(tac, pScope, idNode->id.value.pArrayDef->indexes[0]);
        char *off = newTemp();
        sprintf(szNum, "%d", size);
        tacEmit(tac, TAC_MUL, idx, szNum, off, T_INTEGER);
        free(idx);
        return off;
    } else {
        int k;
        int loc = size * Dati->Value.pArrayDef->sizes[1];
        char *idx0 = emitTAC(tac, pScope, idNode->id.value.pArrayDef->indexes[0]);
        char *acc = newTemp();
        sprintf(szNum, "%d", loc);
        tacEmit(tac, TAC_MUL, idx0, szNum, acc, T_INTEGER);
        free(idx0);

        for (k = 1; k < Dati->Value.pArrayDef->countIndexes; k++) {
            char *idxK = emitTAC(tac, pScope, idNode->id.value.pArrayDef->indexes[k]);
            char *mulT = newTemp();
            char *sumT = newTemp();
            loc = (k < Dati->Value.pArrayDef->countIndexes - 1)
                  ? size * Dati->Value.pArrayDef->sizes[k + 1]
                  : size;
            sprintf(szNum, "%d", loc);
            tacEmit(tac, TAC_MUL, idxK, szNum, mulT, T_INTEGER);
            tacEmit(tac, TAC_ADD, acc, mulT, sumT, T_INTEGER);
            free(idxK);
            free(mulT);
            free(acc);
            acc = sumT;
        }
        return acc;
    }
}

static char *emitTAC(TACList *tac, Scope *pScope, nodeType *p)
{
    if (!p) return NULL;

    switch (p->type) {
    case typeConst:
        if (p->type1 == T_STRING) {
            size_t len = strlen(p->con.value.strVal) + 3;
            char *s = (char *)malloc(len);
            if (s) sprintf(s, "\"%s\"", p->con.value.strVal);
            return s;
        }
        return tacStrdup(p->con.lexeme);

    case typeId: {
        HashTable Dati = {0};
        if (!scopeFind(pScope, p->id.name, &Dati)) {
            printf("Errore TAC: identificatore '%s' non dichiarato\n", p->id.name);
            return tacStrdup("0");
        }
        if (Dati.Tipo0 == T_ARRAY) {
            char *off = emitArrayOffset(tac, pScope, p, &Dati);
            char *t = newTemp();
            tacEmit(tac, TAC_LOAD_IDX, p->id.name, off, t, Dati.Tipo1);
            free(off);
            return t;
        }
        return tacStrdup(p->id.name);
    }

    case typeOperator:
        switch (p->opr.oper) {            
        case T_STMT_LIST:
            free(emitTAC(tac, pScope, p->opr.op[0]));
            if (p->opr.nops > 1)
                return emitTAC(tac, pScope, p->opr.op[1]);
            return NULL;

        case T_CODE_BLOCK:
            return emitTAC(tac, pScope, p->opr.op[0]);

        case T_KW_WHILE: {
            char *lStart = newLabel();
            char *lEnd = newLabel();
            char *cond;
            tacEmit(tac, TAC_LABEL, NULL, NULL, lStart, T_INTEGER);
            cond = emitTAC(tac, pScope, p->opr.op[0]);
            tacEmit(tac, TAC_IF_FALSE, cond, NULL, lEnd, T_BOOLEAN);
            free(cond);
            free(emitTAC(tac, pScope, p->opr.op[1]));
            tacEmit(tac, TAC_GOTO, NULL, NULL, lStart, T_INTEGER);
            tacEmit(tac, TAC_LABEL, NULL, NULL, lEnd, T_INTEGER);
            free(lStart);
            free(lEnd);
            return NULL;
        }

        case T_KW_DO: {
            char *lStart = newLabel();
            char *cond;
            tacEmit(tac, TAC_LABEL, NULL, NULL, lStart, T_INTEGER);
            free(emitTAC(tac, pScope, p->opr.op[0]));
            cond = emitTAC(tac, pScope, p->opr.op[1]);
            tacEmit(tac, TAC_IF_TRUE, cond, NULL, lStart, T_BOOLEAN);
            free(cond);
            free(lStart);
            return NULL;
        }

        case T_KW_IF: {
            char *cond = emitTAC(tac, pScope, p->opr.op[0]);
            char *lElse = newLabel();
            tacEmit(tac, TAC_IF_FALSE, cond, NULL, lElse, T_BOOLEAN);
            free(cond);
            free(emitTAC(tac, pScope, p->opr.op[1]));
            if (p->opr.nops > 2) {
                char *lEnd = newLabel();
                tacEmit(tac, TAC_GOTO, NULL, NULL, lEnd, T_INTEGER);
                tacEmit(tac, TAC_LABEL, NULL, NULL, lElse, T_INTEGER);
                free(emitTAC(tac, pScope, p->opr.op[2]));
                tacEmit(tac, TAC_LABEL, NULL, NULL, lEnd, T_INTEGER);
                free(lEnd);
            } else {
                tacEmit(tac, TAC_LABEL, NULL, NULL, lElse, T_INTEGER);
            }
            free(lElse);
            return NULL;
        }

        case T_KW_PRINT: {
            char *v = emitTAC(tac, pScope, p->opr.op[0]);
            tacEmit(tac, TAC_PRINT, v, NULL, NULL, p->opr.op[0]->type1);
            free(v);
            return NULL;
        }

        case T_KW_READ: {
            nodeType *dst = p->opr.op[0];
            if (dst->type0 == T_ARRAY) {
                HashTable Dati = {0};
                if (scopeFind(pScope, dst->id.name, &Dati)) {
                    char *off = emitArrayOffset(tac, pScope, dst, &Dati);
                    char *tmp = newTemp();
                    tacEmit(tac, TAC_READ, NULL, NULL, tmp, dst->type1);
                    tacEmit(tac, TAC_STORE_IDX, off, tmp, dst->id.name, dst->type1);
                    free(off);
                    free(tmp);
                }
            } else {
                tacEmit(tac, TAC_READ, NULL, NULL, dst->id.name, dst->type1);
            }
            return NULL;
        }

        case T_OP_ASSIGN: {
            nodeType *lhs = p->opr.op[0];
            char *rhs = emitTAC(tac, pScope, p->opr.op[1]);
            if (lhs->type0 == T_ARRAY) {
                HashTable Dati = {0};
                if (scopeFind(pScope, lhs->id.name, &Dati)) {
                    char *off = emitArrayOffset(tac, pScope, lhs, &Dati);
                    tacEmit(tac, TAC_STORE_IDX, off, rhs, lhs->id.name, lhs->type1);
                    free(off);
                }
            } else {
                tacEmit(tac, TAC_ASSIGN, rhs, NULL, lhs->id.name, lhs->type1);
            }
            return rhs;
        }

        case T_OP_UMINUS:
        case T_OP_NOT: {
            char *a = emitTAC(tac, pScope, p->opr.op[0]);
            char *t = newTemp();
            tacEmit(tac, (p->opr.oper == T_OP_UMINUS) ? TAC_NEG : TAC_NOT, a, NULL, t, p->type1);
            free(a);
            return t;
        }

        default: {
            TACOp op;
            char *l, *r, *t;
            switch (p->opr.oper) {
                case T_OP_PLUS:  op = TAC_ADD; break;
                case T_OP_MINUS: op = TAC_SUB; break;
                case T_OP_MULT:  op = TAC_MUL; break;
                case T_OP_DIV:   op = TAC_DIV; break;
                case T_OP_LT:    op = TAC_LT;  break;
                case T_OP_LE:    op = TAC_LE;  break;
                case T_OP_GT:    op = TAC_GT;  break;
                case T_OP_GE:    op = TAC_GE;  break;
                case T_OP_EQ:    op = TAC_EQ;  break;
                case T_OP_NE:    op = TAC_NE;  break;
                case T_OP_AND:   op = TAC_AND; break;
                case T_OP_OR:    op = TAC_OR;  break;
                default: return NULL;
            }
            l = emitTAC(tac, pScope, p->opr.op[0]);
            r = emitTAC(tac, pScope, p->opr.op[1]);
            t = newTemp();
            tacEmit(tac, op, l, r, t, p->type1);
            free(l);
            free(r);
            return t;
        }
        }
    }
    return NULL;
}

typedef struct {
    int start;
    int end;
    int *preds;
    int numPreds;
    int capPreds;
    int *succs;
    int numSuccs;
    int capSuccs;
    int reachable;
} BasicBlock;

typedef struct {
    BasicBlock *blocks;
    int numBlocks;
} CFG;

static void tacCompact(TACList *tac);
static void cfgBuild(const TACList *tac, CFG *cfg);
static void cfgFree(CFG *cfg);
static int cfgEliminateUnreachable(TACList *tac, CFG *cfg);
static int cfgGlobalLivenessDCE(TACList *tac, const CFG *cfg);

/* =========================================================================
 * PASSAGGIO 3: Ottimizzatore -O1 su TACList
 * ========================================================================= */

typedef struct {
    char *var;
    char *val;
} CopyEntry;

typedef struct {
    CopyEntry *items;
    int count;
    int capacity;
} CopyEnv;

static void envClear(CopyEnv *env)
{
    int i;
    for (i = 0; i < env->count; i++) {
        free(env->items[i].var);
        free(env->items[i].val);
    }
    env->count = 0;
}

static void envFree(CopyEnv *env)
{
    envClear(env);
    free(env->items);
    env->items = NULL;
    env->capacity = 0;
}

static const char *envLookup(const CopyEnv *env, const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = env->count - 1; i >= 0; i--) {
        if (strcmp(env->items[i].var, name) == 0)
            return env->items[i].val;
    }
    return NULL;
}

static void envSet(CopyEnv *env, const char *var, const char *val)
{
    int i;
    /* Invalida vecchie mappature che definivano o usavano `var` */
    for (i = 0; i < env->count; ) {
        if (strcmp(env->items[i].var, var) == 0 || strcmp(env->items[i].val, var) == 0) {
            free(env->items[i].var);
            free(env->items[i].val);
            env->items[i] = env->items[--env->count];
        } else {
            i++;
        }
    }
    if (!val) return;
    if (env->count >= env->capacity) {
        int newCap = (env->capacity == 0) ? 32 : env->capacity * 2;
        CopyEntry *n = (CopyEntry *)realloc(env->items, (size_t)newCap * sizeof(CopyEntry));
        if (!n) return;
        env->items = n;
        env->capacity = newCap;
    }
    env->items[env->count].var = tacStrdup(var);
    env->items[env->count].val = tacStrdup(val);
    env->count++;
}

static int isNumericLit(const char *s, double *outVal, int *isInt)
{
    char *end = NULL;
    double d;
    if (!s || !*s) return 0;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0 || s[0] == '"') return 0;
    d = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    *outVal = d;
    *isInt = (strchr(s, '.') == NULL && strchr(s, 'e') == NULL && strchr(s, 'E') == NULL);
    return 1;
}

static int isBoolLit(const char *s, int *outBool)
{
    if (!s) return 0;
    if (strcmp(s, "true") == 0)  { *outBool = 1; return 1; }
    if (strcmp(s, "false") == 0) { *outBool = 0; return 1; }
    return 0;
}

static int countUses(const TACList *tac, const char *name)
{
    int i, uses = 0;
    for (i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;
        if (in->arg1 && strcmp(in->arg1, name) == 0) uses++;
        if (in->arg2 && strcmp(in->arg2, name) == 0) uses++;
        if (in->op == TAC_STORE_IDX && in->res && strcmp(in->res, name) == 0) uses++;
    }
    return uses;
}

static int countLabelRefs(const TACList *tac, const char *label)
{
    int i, refs = 0;
    for (i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;
        if ((in->op == TAC_GOTO || in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) &&
            in->res && strcmp(in->res, label) == 0) {
            refs++;
        }
    }
    return refs;
}

void tacOptimizeO1(TACList *tac)
{
    CopyEnv env;
    int changed;
    env.items = NULL;
    env.count = 0;
    env.capacity = 0;

    do {
        int i;
        changed = 0;
        envClear(&env);

        /* 1) Propagazione locale + Folding + Semplificazioni algebriche */
        for (i = 0; i < tac->count; i++) {
            TACInstr *in = &tac->items[i];
            double d1, d2;
            int i1, i2, b1, b2;

            if (in->dead) continue;

            if (in->op == TAC_LABEL || in->op == TAC_GOTO ||
                in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) {
                if ((in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) && in->arg1) {
                    const char *rep = envLookup(&env, in->arg1);
                    if (rep && strcmp(rep, in->arg1) != 0) {
                        free(in->arg1);
                        in->arg1 = tacStrdup(rep);
                        changed = 1;
                    }
                    /* Semplifica salti con condizione costante */
                    if (isBoolLit(in->arg1, &b1) ||
                        (isNumericLit(in->arg1, &d1, &i1) && ((b1 = (d1 != 0.0)), 1))) {
                        int takeJump = (in->op == TAC_IF_TRUE) ? b1 : !b1;
                        free(in->arg1);
                        in->arg1 = NULL;
                        if (takeJump) in->op = TAC_GOTO;
                        else          in->dead = 1;
                        changed = 1;
                    }
                }
                envClear(&env);
                continue;
            }

            if (in->arg1) {
                const char *rep = envLookup(&env, in->arg1);
                if (rep && strcmp(rep, in->arg1) != 0) {
                    free(in->arg1);
                    in->arg1 = tacStrdup(rep);
                    changed = 1;
                }
            }
            if (in->arg2) {
                const char *rep = envLookup(&env, in->arg2);
                if (rep && strcmp(rep, in->arg2) != 0) {
                    free(in->arg2);
                    in->arg2 = tacStrdup(rep);
                    changed = 1;
                }
            }

            /* Constant folding su operatori binari aritmetici e relazionali */
            if ((in->op >= TAC_ADD && in->op <= TAC_NE) &&
                isNumericLit(in->arg1, &d1, &i1) && isNumericLit(in->arg2, &d2, &i2)) {
                char buf[64];
                int ok = 1;
                //int isInt = i1 && i2;
                int isInt = i1 && i2 && (in->type1 != T_REAL);

                if (in->op >= TAC_ADD && in->op <= TAC_DIV) {
                    double rv = 0.0;
                    if (in->op == TAC_ADD) rv = d1 + d2;
                    else if (in->op == TAC_SUB) rv = d1 - d2;
                    else if (in->op == TAC_MUL) rv = d1 * d2;
                    else if (in->op == TAC_DIV) {
                        if (d2 == 0.0) ok = 0;
                        else rv = isInt ? (double)((int)d1 / (int)d2) : (d1 / d2);
                    }
                    if (ok) {
                        if (isInt) sprintf(buf, "%d", (int)rv);
                        else       sprintf(buf, "%.15g", rv);
                    }
                } else {
                    int br = 0;
                    if (in->op == TAC_LT)      br = (d1 < d2);
                    else if (in->op == TAC_LE) br = (d1 <= d2);
                    else if (in->op == TAC_GT) br = (d1 > d2);
                    else if (in->op == TAC_GE) br = (d1 >= d2);
                    else if (in->op == TAC_EQ) br = (d1 == d2);
                    else if (in->op == TAC_NE) br = (d1 != d2);
                    sprintf(buf, "%d", br);
                }

                if (ok) {
                    free(in->arg1);
                    free(in->arg2);
                    in->op = TAC_ASSIGN;
                    in->arg1 = tacStrdup(buf);
                    in->arg2 = NULL;
                    changed = 1;
                }
            }

            /* Semplificazioni algebriche sul TAC (es. x * 1 -> x, x + 0 -> x, x * 0 -> 0) */
            if (in->op == TAC_ADD || in->op == TAC_SUB) {
                if (isNumericLit(in->arg2, &d2, &i2) && d2 == 0.0) {
                    free(in->arg2); in->arg2 = NULL;
                    in->op = TAC_ASSIGN; changed = 1;
                } else if (in->op == TAC_ADD && isNumericLit(in->arg1, &d1, &i1) && d1 == 0.0) {
                    free(in->arg1); in->arg1 = in->arg2; in->arg2 = NULL;
                    in->op = TAC_ASSIGN; changed = 1;
                }
            } else if (in->op == TAC_MUL) {
                if (isNumericLit(in->arg2, &d2, &i2) && d2 == 1.0) {
                    free(in->arg2); in->arg2 = NULL;
                    in->op = TAC_ASSIGN; changed = 1;
                } else if (isNumericLit(in->arg1, &d1, &i1) && d1 == 1.0) {
                    free(in->arg1); in->arg1 = in->arg2; in->arg2 = NULL;
                    in->op = TAC_ASSIGN; changed = 1;
                } else if ((isNumericLit(in->arg1, &d1, &i1) && d1 == 0.0) ||
                           (isNumericLit(in->arg2, &d2, &i2) && d2 == 0.0)) {
                    free(in->arg1); free(in->arg2);
                    in->op = TAC_ASSIGN;
                    in->arg1 = tacStrdup("0");
                    in->arg2 = NULL;
                    changed = 1;
                }
            } else if (in->op == TAC_DIV && isNumericLit(in->arg2, &d2, &i2) && d2 == 1.0) {
                free(in->arg2); in->arg2 = NULL;
                in->op = TAC_ASSIGN; changed = 1;
            }

            /* Folding logico AND / OR */
            if ((in->op == TAC_AND || in->op == TAC_OR) &&
                (isBoolLit(in->arg1, &b1) || (isNumericLit(in->arg1, &d1, &i1) && ((b1 = (d1 != 0.0)), 1))) &&
                (isBoolLit(in->arg2, &b2) || (isNumericLit(in->arg2, &d2, &i2) && ((b2 = (d2 != 0.0)), 1)))) {
                int br = (in->op == TAC_AND) ? (b1 && b2) : (b1 || b2);
                free(in->arg1); free(in->arg2);
                in->op = TAC_ASSIGN;
                in->arg1 = tacStrdup(br ? "true" : "false");
                in->arg2 = NULL;
                changed = 1;
            }

            /* Folding unario NEG / NOT */
            if (in->op == TAC_NEG && isNumericLit(in->arg1, &d1, &i1)) {
                char buf[64];
                if (i1) sprintf(buf, "%d", -(int)d1);
                else    sprintf(buf, "%.15g", -d1);
                free(in->arg1);
                in->op = TAC_ASSIGN;
                in->arg1 = tacStrdup(buf);
                changed = 1;
            } else if (in->op == TAC_NOT && isBoolLit(in->arg1, &b1)) {
                free(in->arg1);
                in->op = TAC_ASSIGN;
                in->arg1 = tacStrdup(b1 ? "false" : "true");
                changed = 1;
            }

            /* Aggiorna mappa di propagazione */
            if (in->op == TAC_ASSIGN && in->res && in->arg1) {
                if (strcmp(in->res, in->arg1) == 0) {
                    in->dead = 1;
                    changed = 1;
                } else {
                    envSet(&env, in->res, in->arg1);
                }
            } else if (in->res && in->op != TAC_STORE_IDX) {
                envSet(&env, in->res, NULL);
            }
        }

        /* 2) Dead Code Elimination: rimuove assegnamenti mai letti */
        for (i = 0; i < tac->count; i++) {
            TACInstr *in = &tac->items[i];
            if (in->dead) continue;
            if (in->res && in->op != TAC_LABEL && in->op != TAC_GOTO &&
                in->op != TAC_IF_FALSE && in->op != TAC_IF_TRUE &&
                in->op != TAC_STORE_IDX && in->op != TAC_READ) {
                if (countUses(tac, in->res) == 0) {
                    in->dead = 1;
                    changed = 1;
                }
            }
        }

        /* 3) Peephole: `goto Lx` subito prima di `Lx:` e label inutilizzate */
        for (i = 0; i < tac->count; i++) {
            if (tac->items[i].dead) continue;
            if (tac->items[i].op == TAC_GOTO) {
                int j = i + 1;
                while (j < tac->count && tac->items[j].dead) j++;
                if (j < tac->count && tac->items[j].op == TAC_LABEL &&
                    strcmp(tac->items[i].res, tac->items[j].res) == 0) {
                    tac->items[i].dead = 1;
                    changed = 1;
                }
            }
        }
        for (i = 0; i < tac->count; i++) {
            if (!tac->items[i].dead && tac->items[i].op == TAC_LABEL &&
                countLabelRefs(tac, tac->items[i].res) == 0) {
                tac->items[i].dead = 1;
                changed = 1;
            }
        }

        /* 4) CFG: eliminazione codice irraggiungibile e liveness DCE globale */
        tacCompact(tac);
        if (tac->count > 0) {
            CFG cfg;
            cfgBuild(tac, &cfg);
            if (cfgEliminateUnreachable(tac, &cfg)) changed = 1;
            if (cfgGlobalLivenessDCE(tac, &cfg))    changed = 1;
            cfgFree(&cfg);
        }
    } while (changed);

    tacCompact(tac);
    envFree(&env);
}

void tacPrint(const TACList *list)
{
    int i;
    for (i = 0; i < list->count; i++) {
        const TACInstr *in = &list->items[i];
        if (in->dead) continue;
        switch (in->op) {
            case TAC_ASSIGN:    printf("%s = %s\n", in->res, in->arg1); break;
            case TAC_NEG:       printf("%s = neg %s\n", in->res, in->arg1); break;
            case TAC_NOT:       printf("%s = not %s\n", in->res, in->arg1); break;
            case TAC_ADD:       printf("%s = %s + %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_SUB:       printf("%s = %s - %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_MUL:       printf("%s = %s * %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_DIV:       printf("%s = %s / %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_LT:        printf("%s = %s < %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_LE:        printf("%s = %s <= %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_GT:        printf("%s = %s > %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_GE:        printf("%s = %s >= %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_EQ:        printf("%s = %s == %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_NE:        printf("%s = %s != %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_AND:       printf("%s = %s and %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_OR:        printf("%s = %s or %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_LOAD_IDX:  printf("%s = %s[%s]\n", in->res, in->arg1, in->arg2); break;
            case TAC_STORE_IDX: printf("%s[%s] = %s\n", in->res, in->arg1, in->arg2); break;
            case TAC_LABEL:     printf("%s:\n", in->res); break;
            case TAC_GOTO:      printf("goto %s\n", in->res); break;
            case TAC_IF_FALSE:  printf("ifFalse %s goto %s\n", in->arg1, in->res); break;
            case TAC_IF_TRUE:   printf("if %s goto %s\n", in->arg1, in->res); break;
            case TAC_PRINT:     printf("print %s\n", in->arg1); break;
            case TAC_READ:      printf("read %s\n", in->res); break;
        }
    }
}

void tacBuild(nodeType **ppTree, Scope *pScope, int optLevel, TACList *outTac)
{
    nodeType *pTree = ppTree ? *ppTree : NULL;

    s_label = 1;
    s_temp = 1;
    tacInit(outTac);

    if (!pTree) {
        return;
    }

    if (optLevel >= 1) {
        pTree = tacOptimizeAST(pTree);
        if (ppTree) {
            *ppTree = pTree;
        }
    }

    free(emitTAC(outTac, pScope, pTree));

    if (optLevel >= 1) {
        tacOptimizeO1(outTac);
    }

    if (optLevel >= 2) {
        tacOptimizeO2(outTac);
    }

    if (optLevel >= 3) {
        tacOptimizeO3(outTac);
    }
}

void tacGenerate(nodeType *pTree, Scope *pScope, int optLevel)
{
    TACList tac;
    tacBuild(&pTree, pScope, optLevel, &tac);
    tacPrint(&tac);
    tacFree(&tac);
}

/* =========================================================================
 * PASSAGGIO 4: Ottimizzatore -O2 (Basic Blocks, CFG, CSE, Global Liveness)
 * ========================================================================= */

static void tacCompact(TACList *tac)
{
    int i, w = 0;
    for (i = 0; i < tac->count; i++) {
        if (tac->items[i].dead) {
            free(tac->items[i].arg1);
            free(tac->items[i].arg2);
            free(tac->items[i].res);
        } else {
            if (w != i) {
                tac->items[w] = tac->items[i];
            }
            w++;
        }
    }
    tac->count = w;
}

static void addIntUnique(int **arr, int *count, int *cap, int val)
{
    int i;
    for (i = 0; i < *count; i++) {
        if ((*arr)[i] == val) return;
    }
    if (*count >= *cap) {
        int newCap = (*cap == 0) ? 4 : (*cap * 2);
        int *n = (int *)realloc(*arr, (size_t)newCap * sizeof(int));
        if (!n) return;
        *arr = n;
        *cap = newCap;
    }
    (*arr)[(*count)++] = val;
}

static void cfgAddEdge(CFG *cfg, int from, int to)
{
    addIntUnique(&cfg->blocks[from].succs, &cfg->blocks[from].numSuccs, &cfg->blocks[from].capSuccs, to);
    addIntUnique(&cfg->blocks[to].preds,   &cfg->blocks[to].numPreds,   &cfg->blocks[to].capPreds,   from);
}

static int cfgFindBlockByLabel(const TACList *tac, const CFG *cfg, const char *label)
{
    int b;
    if (!label) return -1;
    for (b = 0; b < cfg->numBlocks; b++) {
        const TACInstr *first = &tac->items[cfg->blocks[b].start];
        if (first->op == TAC_LABEL && first->res && strcmp(first->res, label) == 0) {
            return b;
        }
    }
    return -1;
}

static void cfgBuild(const TACList *tac, CFG *cfg)
{
    int i, b;
    int *isLeader;

    cfg->blocks = NULL;
    cfg->numBlocks = 0;
    if (tac->count == 0) return;

    isLeader = (int *)calloc((size_t)tac->count, sizeof(int));
    if (!isLeader) return;

    isLeader[0] = 1;
    for (i = 0; i < tac->count; i++) {
        TACOp op = tac->items[i].op;
        if (op == TAC_LABEL) {
            isLeader[i] = 1;
        }
        if (op == TAC_GOTO || op == TAC_IF_FALSE || op == TAC_IF_TRUE) {
            if (i + 1 < tac->count) {
                isLeader[i + 1] = 1;
            }
        }
    }

    for (i = 0; i < tac->count; i++) {
        if (isLeader[i]) cfg->numBlocks++;
    }

    cfg->blocks = (BasicBlock *)calloc((size_t)cfg->numBlocks, sizeof(BasicBlock));
    if (!cfg->blocks) {
        free(isLeader);
        cfg->numBlocks = 0;
        return;
    }

    b = -1;
    for (i = 0; i < tac->count; i++) {
        if (isLeader[i]) {
            if (b >= 0) cfg->blocks[b].end = i - 1;
            b++;
            cfg->blocks[b].start = i;
        }
    }
    if (b >= 0) cfg->blocks[b].end = tac->count - 1;
    free(isLeader);

    for (b = 0; b < cfg->numBlocks; b++) {
        const TACInstr *last = &tac->items[cfg->blocks[b].end];
        if (last->op == TAC_GOTO) {
            int target = cfgFindBlockByLabel(tac, cfg, last->res);
            if (target >= 0) cfgAddEdge(cfg, b, target);
        } else if (last->op == TAC_IF_FALSE || last->op == TAC_IF_TRUE) {
            int target = cfgFindBlockByLabel(tac, cfg, last->res);
            if (b + 1 < cfg->numBlocks) cfgAddEdge(cfg, b, b + 1);
            if (target >= 0) cfgAddEdge(cfg, b, target);
        } else {
            if (b + 1 < cfg->numBlocks) cfgAddEdge(cfg, b, b + 1);
        }
    }
}

static void cfgFree(CFG *cfg)
{
    int b;
    for (b = 0; b < cfg->numBlocks; b++) {
        free(cfg->blocks[b].preds);
        free(cfg->blocks[b].succs);
    }
    free(cfg->blocks);
    cfg->blocks = NULL;
    cfg->numBlocks = 0;
}

static void cfgMarkReachableDFS(CFG *cfg, int b)
{
    int i;
    if (cfg->blocks[b].reachable) return;
    cfg->blocks[b].reachable = 1;
    for (i = 0; i < cfg->blocks[b].numSuccs; i++) {
        cfgMarkReachableDFS(cfg, cfg->blocks[b].succs[i]);
    }
}

static int cfgEliminateUnreachable(TACList *tac, CFG *cfg)
{
    int b, i, changed = 0;
    if (cfg->numBlocks == 0) return 0;

    cfgMarkReachableDFS(cfg, 0);
    for (b = 0; b < cfg->numBlocks; b++) {
        if (!cfg->blocks[b].reachable) {
            for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
                if (!tac->items[i].dead) {
                    tac->items[i].dead = 1;
                    changed = 1;
                }
            }
        }
    }
    return changed;
}

/* --- Common Subexpression Elimination (Global Available Expressions) --- */

typedef struct {
    TACOp op;
    char *arg1;
    char *arg2;
    char *holder;
} ExprEntry;

static int isPureExprOp(TACOp op)
{
    return (op == TAC_NEG || op == TAC_NOT || (op >= TAC_ADD && op <= TAC_OR));
}

static int isCommutativeOp(TACOp op)
{
    return (op == TAC_ADD || op == TAC_MUL || op == TAC_EQ ||
            op == TAC_NE  || op == TAC_AND || op == TAC_OR);
}

static void canonicalArgs(TACOp op, const char *a1, const char *a2, const char **o1, const char **o2)
{
    if (a2 && isCommutativeOp(op) && strcmp(a1, a2) > 0) {
        *o1 = a2;
        *o2 = a1;
    } else {
        *o1 = a1;
        *o2 = a2;
    }
}

static int strOptEq(const char *a, const char *b)
{
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

static int isScalarWriteOp(TACOp op)
{
    return (op != TAC_LABEL && op != TAC_GOTO &&
            op != TAC_IF_FALSE && op != TAC_IF_TRUE &&
            op != TAC_STORE_IDX && op != TAC_PRINT);
}

static void cseSimulateBlock(const TACList *tac, const CFG *cfg, int b,
                             const ExprEntry *exprs, int numExprs,
                             const unsigned char *inSet, unsigned char *outSet)
{
    int i, e;
    memcpy(outSet, inSet, (size_t)numExprs);

    for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
        const TACInstr *in = &tac->items[i];
        int matched = 0;

        if (in->dead) continue;

        if (isPureExprOp(in->op) && in->res && in->arg1) {
            const char *c1, *c2;
            canonicalArgs(in->op, in->arg1, in->arg2, &c1, &c2);
            for (e = 0; e < numExprs; e++) {
                if (outSet[e] && exprs[e].op == in->op &&
                    strOptEq(exprs[e].arg1, c1) && strOptEq(exprs[e].arg2, c2) &&
                    strcmp(exprs[e].holder, in->res) != 0) {
                    matched = 1;
                    break;
                }
            }
        }

        /* Se `in->res` viene scritto (non da STORE_IDX), uccide espressioni che lo usano */
        if (in->res && isScalarWriteOp(in->op)) {
            for (e = 0; e < numExprs; e++) {
                if (!outSet[e]) continue;
                if (strcmp(exprs[e].holder, in->res) == 0 ||
                    (exprs[e].arg1 && strcmp(exprs[e].arg1, in->res) == 0) ||
                    (exprs[e].arg2 && strcmp(exprs[e].arg2, in->res) == 0)) {
                    outSet[e] = 0;
                }
            }
        }

        /* Genera l'espressione solo se non e' gia' stata sostituita e `res` non coincide con gli argomenti */
        if (!matched && isPureExprOp(in->op) && in->res && in->arg1) {
            if (strcmp(in->res, in->arg1) != 0 && (!in->arg2 || strcmp(in->res, in->arg2) != 0)) {
                const char *c1, *c2;
                canonicalArgs(in->op, in->arg1, in->arg2, &c1, &c2);
                for (e = 0; e < numExprs; e++) {
                    if (exprs[e].op == in->op &&
                        strOptEq(exprs[e].arg1, c1) && strOptEq(exprs[e].arg2, c2) &&
                        strcmp(exprs[e].holder, in->res) == 0) {
                        outSet[e] = 1;
                        break;
                    }
                }
            }
        }
    }
}

static int cfgGlobalCSE(TACList *tac, const CFG *cfg)
{
    ExprEntry *exprs = NULL;
    int numExprs = 0, capExprs = 0;
    unsigned char *inSets, *outSets, *curSet;
    int i, b, e, dfChanged, changed = 0;

    if (cfg->numBlocks == 0) return 0;

    for (i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead || !isPureExprOp(in->op) || !in->res || !in->arg1) continue;
        if (strcmp(in->res, in->arg1) == 0) continue;
        if (in->arg2 && strcmp(in->res, in->arg2) == 0) continue;

        {
            const char *c1, *c2;
            int found = 0;
            canonicalArgs(in->op, in->arg1, in->arg2, &c1, &c2);
            for (e = 0; e < numExprs; e++) {
                if (exprs[e].op == in->op &&
                    strOptEq(exprs[e].arg1, c1) && strOptEq(exprs[e].arg2, c2) &&
                    strcmp(exprs[e].holder, in->res) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (numExprs >= capExprs) {
                    int newCap = (capExprs == 0) ? 32 : capExprs * 2;
                    ExprEntry *n = (ExprEntry *)realloc(exprs, (size_t)newCap * sizeof(ExprEntry));
                    if (!n) break;
                    exprs = n;
                    capExprs = newCap;
                }
                exprs[numExprs].op = in->op;
                exprs[numExprs].arg1 = tacStrdup(c1);
                exprs[numExprs].arg2 = tacStrdup(c2);
                exprs[numExprs].holder = tacStrdup(in->res);
                numExprs++;
            }
        }
    }

    if (numExprs == 0) return 0;

    inSets  = (unsigned char *)calloc((size_t)cfg->numBlocks * (size_t)numExprs, 1);
    outSets = (unsigned char *)malloc((size_t)cfg->numBlocks * (size_t)numExprs);
    curSet  = (unsigned char *)malloc((size_t)numExprs);
    if (!inSets || !outSets || !curSet) {
        free(inSets); free(outSets); free(curSet);
        for (e = 0; e < numExprs; e++) {
            free(exprs[e].arg1); free(exprs[e].arg2); free(exprs[e].holder);
        }
        free(exprs);
        return 0;
    }

    /* Inizializzazione per intersezione: blocco 0 = vuoto, altri = TOP (tutti 1) */
    memset(outSets, 0, (size_t)numExprs);
    if (cfg->numBlocks > 1) {
        memset(outSets + numExprs, 1, (size_t)(cfg->numBlocks - 1) * (size_t)numExprs);
    }

    do {
        dfChanged = 0;
        for (b = 0; b < cfg->numBlocks; b++) {
            unsigned char *inB  = inSets  + (size_t)b * (size_t)numExprs;
            unsigned char *outB = outSets + (size_t)b * (size_t)numExprs;

            if (b == 0 || cfg->blocks[b].numPreds == 0) {
                memset(inB, 0, (size_t)numExprs);
            } else {
                int p;
                int p0 = cfg->blocks[b].preds[0];
                memcpy(inB, outSets + (size_t)p0 * (size_t)numExprs, (size_t)numExprs);
                for (p = 1; p < cfg->blocks[b].numPreds; p++) {
                    int pk = cfg->blocks[b].preds[p];
                    const unsigned char *outPk = outSets + (size_t)pk * (size_t)numExprs;
                    for (e = 0; e < numExprs; e++) {
                        inB[e] &= outPk[e];
                    }
                }
            }

            cseSimulateBlock(tac, cfg, b, exprs, numExprs, inB, curSet);
            if (memcmp(outB, curSet, (size_t)numExprs) != 0) {
                memcpy(outB, curSet, (size_t)numExprs);
                dfChanged = 1;
            }
        }
    } while (dfChanged);

    /* Riscrittura delle sottoespressioni comuni */
    for (b = 0; b < cfg->numBlocks; b++) {
        memcpy(curSet, inSets + (size_t)b * (size_t)numExprs, (size_t)numExprs);
        for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
            TACInstr *in = &tac->items[i];
            int replaced = 0;

            if (in->dead) continue;

            if (isPureExprOp(in->op) && in->res && in->arg1) {
                const char *c1, *c2;
                canonicalArgs(in->op, in->arg1, in->arg2, &c1, &c2);
                for (e = 0; e < numExprs; e++) {
                    if (curSet[e] && exprs[e].op == in->op &&
                        strOptEq(exprs[e].arg1, c1) && strOptEq(exprs[e].arg2, c2) &&
                        strcmp(exprs[e].holder, in->res) != 0) {
                        free(in->arg1);
                        free(in->arg2);
                        in->op = TAC_ASSIGN;
                        in->arg1 = tacStrdup(exprs[e].holder);
                        in->arg2 = NULL;
                        replaced = 1;
                        changed = 1;
                        break;
                    }
                }
            }

            if (in->res && isScalarWriteOp(in->op)) {
                for (e = 0; e < numExprs; e++) {
                    if (!curSet[e]) continue;
                    if (strcmp(exprs[e].holder, in->res) == 0 ||
                        (exprs[e].arg1 && strcmp(exprs[e].arg1, in->res) == 0) ||
                        (exprs[e].arg2 && strcmp(exprs[e].arg2, in->res) == 0)) {
                        curSet[e] = 0;
                    }
                }
            }

            if (!replaced && isPureExprOp(in->op) && in->res && in->arg1) {
                if (strcmp(in->res, in->arg1) != 0 && (!in->arg2 || strcmp(in->res, in->arg2) != 0)) {
                    const char *c1, *c2;
                    canonicalArgs(in->op, in->arg1, in->arg2, &c1, &c2);
                    for (e = 0; e < numExprs; e++) {
                        if (exprs[e].op == in->op &&
                            strOptEq(exprs[e].arg1, c1) && strOptEq(exprs[e].arg2, c2) &&
                            strcmp(exprs[e].holder, in->res) == 0) {
                            curSet[e] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }

    free(inSets);
    free(outSets);
    free(curSet);
    for (e = 0; e < numExprs; e++) {
        free(exprs[e].arg1);
        free(exprs[e].arg2);
        free(exprs[e].holder);
    }
    free(exprs);
    return changed;
}

/* --- Global Live Variable Analysis (DCE tra rami if/else e cicli) --- */

static int isLiteralStr(const char *s)
{
    double d;
    int i, b;
    if (!s || !*s) return 1;
    if (s[0] == '"') return 1;
    if (isBoolLit(s, &b)) return 1;
    if (isNumericLit(s, &d, &i)) return 1;
    return 0;
}

static int findOrAddVar(char ***vars, int *numVars, int *capVars, const char *name, int allowAdd)
{
    int i;
    if (!name || isLiteralStr(name)) return -1;
    for (i = 0; i < *numVars; i++) {
        if (strcmp((*vars)[i], name) == 0) return i;
    }
    if (!allowAdd) return -1;
    if (*numVars >= *capVars) {
        int newCap = (*capVars == 0) ? 32 : (*capVars * 2);
        char **n = (char **)realloc(*vars, (size_t)newCap * sizeof(char *));
        if (!n) return -1;
        *vars = n;
        *capVars = newCap;
    }
    (*vars)[*numVars] = tacStrdup(name);
    return (*numVars)++;
}

static void cfgComputeLiveness(const TACList *tac, const CFG *cfg,
                               char ***outVars, int *outNumVars,
                               unsigned char **outInSets, unsigned char **outOutSets)
{
    char **vars = NULL;
    int numVars = 0, capVars = 0;
    unsigned char *useSets, *defSets, *inSets, *outSets;
    int i, b, v, dfChanged;

    *outVars = NULL;
    *outNumVars = 0;
    *outInSets = NULL;
    *outOutSets = NULL;

    if (cfg->numBlocks == 0) return;

    for (i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;
        findOrAddVar(&vars, &numVars, &capVars, in->arg1, 1);
        findOrAddVar(&vars, &numVars, &capVars, in->arg2, 1);
        if (in->res && (isScalarWriteOp(in->op) || in->op == TAC_STORE_IDX)) {
            findOrAddVar(&vars, &numVars, &capVars, in->res, 1);
        }
    }

    if (numVars == 0) return;

    useSets = (unsigned char *)calloc((size_t)cfg->numBlocks * (size_t)numVars, 1);
    defSets = (unsigned char *)calloc((size_t)cfg->numBlocks * (size_t)numVars, 1);
    inSets  = (unsigned char *)calloc((size_t)cfg->numBlocks * (size_t)numVars, 1);
    outSets = (unsigned char *)calloc((size_t)cfg->numBlocks * (size_t)numVars, 1);

    if (!useSets || !defSets || !inSets || !outSets) {
        free(useSets); free(defSets); free(inSets); free(outSets);
        for (v = 0; v < numVars; v++) free(vars[v]);
        free(vars);
        return;
    }

    for (b = 0; b < cfg->numBlocks; b++) {
        unsigned char *useB = useSets + (size_t)b * (size_t)numVars;
        unsigned char *defB = defSets + (size_t)b * (size_t)numVars;
        for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
            const TACInstr *in = &tac->items[i];
            int id;
            if (in->dead) continue;

            id = findOrAddVar(&vars, &numVars, &capVars, in->arg1, 0);
            if (id >= 0 && !defB[id]) useB[id] = 1;

            id = findOrAddVar(&vars, &numVars, &capVars, in->arg2, 0);
            if (id >= 0 && !defB[id]) useB[id] = 1;

            if (in->op == TAC_STORE_IDX) {
                id = findOrAddVar(&vars, &numVars, &capVars, in->res, 0);
                if (id >= 0 && !defB[id]) useB[id] = 1;
            } else if (in->res && isScalarWriteOp(in->op)) {
                id = findOrAddVar(&vars, &numVars, &capVars, in->res, 0);
                if (id >= 0) defB[id] = 1;
            }
        }
    }

    do {
        dfChanged = 0;
        for (b = cfg->numBlocks - 1; b >= 0; b--) {
            unsigned char *useB = useSets + (size_t)b * (size_t)numVars;
            unsigned char *defB = defSets + (size_t)b * (size_t)numVars;
            unsigned char *inB  = inSets  + (size_t)b * (size_t)numVars;
            unsigned char *outB = outSets + (size_t)b * (size_t)numVars;
            int s;

            memset(outB, 0, (size_t)numVars);
            for (s = 0; s < cfg->blocks[b].numSuccs; s++) {
                int succ = cfg->blocks[b].succs[s];
                const unsigned char *inS = inSets + (size_t)succ * (size_t)numVars;
                for (v = 0; v < numVars; v++) {
                    outB[v] |= inS[v];
                }
            }

            for (v = 0; v < numVars; v++) {
                unsigned char newIn = (unsigned char)(useB[v] | (outB[v] & (unsigned char)(!defB[v])));
                if (newIn != inB[v]) {
                    inB[v] = newIn;
                    dfChanged = 1;
                }
            }
        }
    } while (dfChanged);

    free(useSets);
    free(defSets);
    *outVars = vars;
    *outNumVars = numVars;
    *outInSets = inSets;
    *outOutSets = outSets;
}

static int cfgGlobalLivenessDCE(TACList *tac, const CFG *cfg)
{
    char **vars = NULL;
    int numVars = 0;
    unsigned char *inSets = NULL, *outSets = NULL, *live = NULL;
    int i, b, v, changed = 0;

    cfgComputeLiveness(tac, cfg, &vars, &numVars, &inSets, &outSets);
    if (numVars == 0) return 0;

    live = (unsigned char *)malloc((size_t)numVars);
    if (!live) {
        free(inSets); free(outSets);
        for (v = 0; v < numVars; v++) free(vars[v]);
        free(vars);
        return 0;
    }

    for (b = 0; b < cfg->numBlocks; b++) {
        memcpy(live, outSets + (size_t)b * (size_t)numVars, (size_t)numVars);
        for (i = cfg->blocks[b].end; i >= cfg->blocks[b].start; i--) {
            TACInstr *in = &tac->items[i];
            int id;
            if (in->dead) continue;

            if (in->res && isScalarWriteOp(in->op) && in->op != TAC_READ) {
                id = findOrAddVar(&vars, &numVars, &numVars, in->res, 0);
                if (id >= 0 && !live[id]) {
                    in->dead = 1;
                    changed = 1;
                    continue;
                }
            }

            if (in->res && isScalarWriteOp(in->op)) {
                id = findOrAddVar(&vars, &numVars, &numVars, in->res, 0);
                if (id >= 0) live[id] = 0;
            }
            id = findOrAddVar(&vars, &numVars, &numVars, in->arg1, 0);
            if (id >= 0) live[id] = 1;

            id = findOrAddVar(&vars, &numVars, &numVars, in->arg2, 0);
            if (id >= 0) live[id] = 1;

            if (in->op == TAC_STORE_IDX) {
                id = findOrAddVar(&vars, &numVars, &numVars, in->res, 0);
                if (id >= 0) live[id] = 1;
            }
        }
    }

    free(live);
    free(inSets);
    free(outSets);
    for (v = 0; v < numVars; v++) free(vars[v]);
    free(vars);
    return changed;
}

void tacOptimizeO2(TACList *tac)
{
    int changed;
    do {
        CFG cfg;
        int stepChanged = 0;

        tacCompact(tac);
        if (tac->count == 0) break;

        cfgBuild(tac, &cfg);
        stepChanged |= cfgEliminateUnreachable(tac, &cfg);
        stepChanged |= cfgGlobalCSE(tac, &cfg);
        stepChanged |= cfgGlobalLivenessDCE(tac, &cfg);
        cfgFree(&cfg);

        if (stepChanged) {
            tacOptimizeO1(tac);
        }
        changed = stepChanged;
    } while (changed);

    tacCompact(tac);
}

/* =========================================================================
 * PASSAGGIO 5: Ottimizzatore -O3 (Strength Reduction, Jump Threading,
 *                                LICM su Dominatori, Loop Unrolling)
 * ========================================================================= */

static int tacStrengthReduction(TACList *tac)
{
    CopyEnv negEnv, notEnv;
    int i, changed = 0;

    negEnv.items = NULL; negEnv.count = 0; negEnv.capacity = 0;
    notEnv.items = NULL; notEnv.count = 0; notEnv.capacity = 0;

    for (i = 0; i < tac->count; i++) {
        TACInstr *in = &tac->items[i];
        double d1, d2;
        int i1, i2;

        if (in->dead) continue;

        if (in->op == TAC_LABEL || in->op == TAC_GOTO ||
            in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) {
            envClear(&negEnv);
            envClear(&notEnv);
            continue;
        }

        /* Doppio unario: neg(neg x) -> x, not(not x) -> x */
        if (in->op == TAC_NEG && in->arg1) {
            const char *orig = envLookup(&negEnv, in->arg1);
            if (orig) {
                free(in->arg1);
                in->op = TAC_ASSIGN;
                in->arg1 = tacStrdup(orig);
                changed = 1;
            }
        } else if (in->op == TAC_NOT && in->arg1) {
            const char *orig = envLookup(&notEnv, in->arg1);
            if (orig) {
                free(in->arg1);
                in->op = TAC_ASSIGN;
                in->arg1 = tacStrdup(orig);
                changed = 1;
            }
        }

        /* Strength reduction: x * 2 -> x + x, x / 2 (real) -> x * 0.5 */
        if (in->op == TAC_MUL && in->arg1 && in->arg2) {
            if (isNumericLit(in->arg2, &d2, &i2) && d2 == 2.0 && !isNumericLit(in->arg1, &d1, &i1)) {
                free(in->arg2);
                in->op = TAC_ADD;
                in->arg2 = tacStrdup(in->arg1);
                changed = 1;
            } else if (isNumericLit(in->arg1, &d1, &i1) && d1 == 2.0 && !isNumericLit(in->arg2, &d2, &i2)) {
                free(in->arg1);
                in->op = TAC_ADD;
                in->arg1 = tacStrdup(in->arg2);
                changed = 1;
            }
        } else if (in->op == TAC_DIV && in->type1 == T_REAL && in->arg2) {
            if (isNumericLit(in->arg2, &d2, &i2) && d2 == 2.0) {
                free(in->arg2);
                in->op = TAC_MUL;
                in->arg2 = tacStrdup("0.5");
                changed = 1;
            }
        }

        if (in->res && isScalarWriteOp(in->op)) {
            envSet(&negEnv, in->res, (in->op == TAC_NEG && in->arg1) ? in->arg1 : NULL);
            envSet(&notEnv, in->res, (in->op == TAC_NOT && in->arg1) ? in->arg1 : NULL);
        }
    }

    envFree(&negEnv);
    envFree(&notEnv);
    return changed;
}

static int cfgJumpThreading(TACList *tac, const CFG *cfg)
{
    int b, i, changed = 0;

    for (b = 0; b < cfg->numBlocks; b++) {
        int s = cfg->blocks[b].start;
        int e = cfg->blocks[b].end;
        const char *l1, *l2;

        if (s + 1 != e) continue;
        if (tac->items[s].op != TAC_LABEL || tac->items[e].op != TAC_GOTO) continue;

        l1 = tac->items[s].res;
        l2 = tac->items[e].res;
        if (!l1 || !l2 || strcmp(l1, l2) == 0) continue;

        for (i = 0; i < tac->count; i++) {
            TACInstr *in = &tac->items[i];
            if (in->dead) continue;
            if ((in->op == TAC_GOTO || in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) &&
                in->res && strcmp(in->res, l1) == 0) {
                free(in->res);
                in->res = tacStrdup(l2);
                changed = 1;
            }
        }
    }
    return changed;
}

/* --- Dominatori e Loop-Invariant Code Motion (LICM) --- */

static unsigned char *cfgComputeDominators(const CFG *cfg)
{
    int n = cfg->numBlocks;
    unsigned char *dom, *newDom;
    int b, j, p, changed;

    if (n == 0) return NULL;
    dom = (unsigned char *)malloc((size_t)n * (size_t)n);
    newDom = (unsigned char *)malloc((size_t)n);
    if (!dom || !newDom) {
        free(dom); free(newDom);
        return NULL;
    }

    memset(dom, 0, (size_t)n);
    dom[0] = 1;
    if (n > 1) {
        memset(dom + n, 1, (size_t)(n - 1) * (size_t)n);
    }

    do {
        changed = 0;
        for (b = 1; b < n; b++) {
            unsigned char *domB = dom + (size_t)b * (size_t)n;
            if (cfg->blocks[b].numPreds == 0) {
                memset(newDom, 0, (size_t)n);
            } else {
                int p0 = cfg->blocks[b].preds[0];
                memcpy(newDom, dom + (size_t)p0 * (size_t)n, (size_t)n);
                for (p = 1; p < cfg->blocks[b].numPreds; p++) {
                    int pk = cfg->blocks[b].preds[p];
                    const unsigned char *domPk = dom + (size_t)pk * (size_t)n;
                    for (j = 0; j < n; j++) newDom[j] &= domPk[j];
                }
            }
            newDom[b] = 1;
            if (memcmp(domB, newDom, (size_t)n) != 0) {
                memcpy(domB, newDom, (size_t)n);
                changed = 1;
            }
        }
    } while (changed);

    free(newDom);
    return dom;
}

static void tacInsertInstructions(TACList *tac, int pos, const TACInstr *ins, int insCount)
{
    int i;
    if (insCount <= 0) return;
    if (tac->count + insCount > tac->capacity) {
        int newCap = (tac->capacity == 0) ? 64 : tac->capacity * 2;
        TACInstr *n;
        while (newCap < tac->count + insCount) newCap *= 2;
        n = (TACInstr *)realloc(tac->items, (size_t)newCap * sizeof(TACInstr));
        if (!n) return;
        tac->items = n;
        tac->capacity = newCap;
    }
    if (pos < tac->count) {
        memmove(&tac->items[pos + insCount], &tac->items[pos],
                (size_t)(tac->count - pos) * sizeof(TACInstr));
    }
    for (i = 0; i < insCount; i++) {
        tac->items[pos + i].op = ins[i].op;
        tac->items[pos + i].arg1 = tacStrdup(ins[i].arg1);
        tac->items[pos + i].arg2 = tacStrdup(ins[i].arg2);
        tac->items[pos + i].res = tacStrdup(ins[i].res);
        tac->items[pos + i].type1 = ins[i].type1;
        tac->items[pos + i].dead = 0;
    }
    tac->count += insCount;
}

static int cfgLICM(TACList *tac, const CFG *cfg)
{
    int n = cfg->numBlocks;
    unsigned char *dom;
    char **vars = NULL;
    int numVars = 0;
    unsigned char *liveInSets = NULL, *liveOutSets = NULL;
    int h, t, i, b, v, k;
    int *inLoop = NULL, *stack = NULL;

    if (n == 0) return 0;
    dom = cfgComputeDominators(cfg);
    if (!dom) return 0;

    cfgComputeLiveness(tac, cfg, &vars, &numVars, &liveInSets, &liveOutSets);
    inLoop = (int *)malloc((size_t)n * sizeof(int));
    stack  = (int *)malloc((size_t)n * sizeof(int));

    if (!inLoop || !stack) {
        free(dom); free(inLoop); free(stack);
        free(liveInSets); free(liveOutSets);
        for (v = 0; v < numVars; v++) free(vars[v]);
        free(vars);
        return 0;
    }

    for (h = 0; h < n; h++) {
        int hasBackEdge = 0;
        int top = 0;
        int *defInLoop = NULL;
        unsigned char *liveAtExit = NULL;
        int *hoistIdx = NULL;
        int hoistCount = 0, hoistCap = 0;

        if (tac->items[cfg->blocks[h].start].op != TAC_LABEL) continue;

        memset(inLoop, 0, (size_t)n * sizeof(int));
        inLoop[h] = 1;

        for (t = 0; t < cfg->blocks[h].numPreds; t++) {
            int tail = cfg->blocks[h].preds[t];
            if (dom[tail * n + h]) {
                hasBackEdge = 1;
                if (!inLoop[tail]) {
                    inLoop[tail] = 1;
                    stack[top++] = tail;
                }
            }
        }
        if (!hasBackEdge) continue;

        /* Raccogli tutti i blocchi del ciclo naturale camminando all'indietro */
        while (top > 0) {
            int cur = stack[--top];
            int p;
            for (p = 0; p < cfg->blocks[cur].numPreds; p++) {
                int pred = cfg->blocks[cur].preds[p];
                if (!inLoop[pred]) {
                    inLoop[pred] = 1;
                    stack[top++] = pred;
                }
            }
        }

        if (numVars == 0) continue;
        defInLoop  = (int *)calloc((size_t)numVars, sizeof(int));
        liveAtExit = (unsigned char *)calloc((size_t)numVars, 1);
        if (!defInLoop || !liveAtExit) {
            free(defInLoop); free(liveAtExit);
            continue;
        }

        for (b = 0; b < n; b++) {
            int s;
            if (!inLoop[b]) continue;
            for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
                const TACInstr *in = &tac->items[i];
                if (!in->dead && in->res && isScalarWriteOp(in->op)) {
                    int id = findOrAddVar(&vars, &numVars, &numVars, in->res, 0);
                    if (id >= 0) defInLoop[id]++;
                }
            }
            for (s = 0; s < cfg->blocks[b].numSuccs; s++) {
                int succ = cfg->blocks[b].succs[s];
                if (!inLoop[succ]) {
                    const unsigned char *inS = liveInSets + (size_t)succ * (size_t)numVars;
                    for (v = 0; v < numVars; v++) liveAtExit[v] |= inS[v];
                }
            }
        }

        for (b = 0; b < n; b++) {
            if (!inLoop[b]) continue;
            for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
                const TACInstr *in = &tac->items[i];
                int resId, id1, id2, u, safe;
                double d2;
                int i2;

                if (in->dead) continue;
                if (!isPureExprOp(in->op) && in->op != TAC_ASSIGN) continue;
                if (!in->res || !in->arg1) continue;
                if (in->op == TAC_DIV && !(isNumericLit(in->arg2, &d2, &i2) && d2 != 0.0)) continue;

                id1 = findOrAddVar(&vars, &numVars, &numVars, in->arg1, 0);
                if (id1 >= 0 && defInLoop[id1] > 0) continue;

                id2 = findOrAddVar(&vars, &numVars, &numVars, in->arg2, 0);
                if (id2 >= 0 && defInLoop[id2] > 0) continue;

                resId = findOrAddVar(&vars, &numVars, &numVars, in->res, 0);
                if (resId < 0 || defInLoop[resId] != 1) continue;

                /* Verifica che res non sia letto prima di `i` in `b` */
                safe = 1;
                for (k = cfg->blocks[b].start; k < i; k++) {
                    const TACInstr *prev = &tac->items[k];
                    if (prev->dead) continue;
                    if ((prev->arg1 && strcmp(prev->arg1, in->res) == 0) ||
                        (prev->arg2 && strcmp(prev->arg2, in->res) == 0) ||
                        (prev->op == TAC_STORE_IDX && prev->res && strcmp(prev->res, in->res) == 0)) {
                        safe = 0;
                        break;
                    }
                }
                if (!safe) continue;

                /* `b` deve dominare tutti gli altri blocchi del ciclo che usano `res` */
                for (u = 0; u < n && safe; u++) {
                    if (!inLoop[u] || u == b) continue;
                    for (k = cfg->blocks[u].start; k <= cfg->blocks[u].end; k++) {
                        const TACInstr *other = &tac->items[k];
                        if (other->dead) continue;
                        if ((other->arg1 && strcmp(other->arg1, in->res) == 0) ||
                            (other->arg2 && strcmp(other->arg2, in->res) == 0) ||
                            (other->op == TAC_STORE_IDX && other->res && strcmp(other->res, in->res) == 0)) {
                            if (!dom[u * n + b]) {
                                safe = 0;
                                break;
                            }
                        }
                    }
                }
                if (!safe) continue;

                /* Se `res` e' vivo all'uscita del ciclo, `b` deve dominare tutte le uscite */
                if (liveAtExit[resId]) {
                    for (u = 0; u < n && safe; u++) {
                        int s, isExit = 0;
                        if (!inLoop[u]) continue;
                        for (s = 0; s < cfg->blocks[u].numSuccs; s++) {
                            if (!inLoop[cfg->blocks[u].succs[s]]) { isExit = 1; break; }
                        }
                        if (isExit) {
                            if (!dom[u * n + b] || (u == b && i >= cfg->blocks[b].end)) {
                                safe = 0;
                            }
                        }
                    }
                }
                if (!safe) continue;

                if (hoistCount >= hoistCap) {
                    int newCap = (hoistCap == 0) ? 8 : hoistCap * 2;
                    int *hn = (int *)realloc(hoistIdx, (size_t)newCap * sizeof(int));
                    if (!hn) break;
                    hoistIdx = hn;
                    hoistCap = newCap;
                }
                hoistIdx[hoistCount++] = i;
            }
        }

        if (hoistCount > 0) {
            int hdrStart = cfg->blocks[h].start;
            char *hdrLabel = tacStrdup(tac->items[hdrStart].res);
            char *preLabel = newLabel();
            TACInstr *preIns = (TACInstr *)calloc((size_t)(1 + hoistCount), sizeof(TACInstr));

            if (preIns && hdrLabel && preLabel) {
                /* Reindirizza i salti esterni diretti al ciclo verso `preLabel` */
                for (b = 0; b < n; b++) {
                    if (inLoop[b]) continue;
                    for (i = cfg->blocks[b].start; i <= cfg->blocks[b].end; i++) {
                        TACInstr *in = &tac->items[i];
                        if (!in->dead &&
                            (in->op == TAC_GOTO || in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE) &&
                            in->res && strcmp(in->res, hdrLabel) == 0) {
                            free(in->res);
                            in->res = tacStrdup(preLabel);
                        }
                    }
                }

                preIns[0].op = TAC_LABEL;
                preIns[0].res = preLabel;
                preIns[0].type1 = T_INTEGER;

                for (k = 0; k < hoistCount; k++) {
                    int idx = hoistIdx[k];
                    preIns[1 + k] = tac->items[idx];
                    tac->items[idx].dead = 1;
                }

                tacInsertInstructions(tac, hdrStart, preIns, 1 + hoistCount);
            }

            free(preIns);
            free(hdrLabel);
            free(preLabel);
            free(hoistIdx);
            free(defInLoop);
            free(liveAtExit);
            free(dom); free(inLoop); free(stack);
            free(liveInSets); free(liveOutSets);
            for (v = 0; v < numVars; v++) free(vars[v]);
            free(vars);
            return 1;
        }

        free(hoistIdx);
        free(defInLoop);
        free(liveAtExit);
    }

    free(dom);
    free(inLoop);
    free(stack);
    free(liveInSets);
    free(liveOutSets);
    for (v = 0; v < numVars; v++) free(vars[v]);
    free(vars);
    return 0;
}

/* --- Bounded Loop Unrolling (cicli piccoli con bound costante) --- */

static void tacCloneList(const TACList *src, TACList *dst)
{
    int i;
    tacInit(dst);
    for (i = 0; i < src->count; i++) {
        if (!src->items[i].dead) {
            tacEmit(dst, src->items[i].op, src->items[i].arg1,
                    src->items[i].arg2, src->items[i].res, src->items[i].type1);
        }
    }
}

static void tacSwapLists(TACList *a, TACList *b)
{
    TACList tmp = *a;
    *a = *b;
    *b = tmp;
}

static int evalScalarInstr(const TACInstr *in, CopyEnv *env, char *outBuf, size_t outSize)
{
    const char *a1 = in->arg1;
    const char *a2 = in->arg2;
    double d1, d2;
    int i1, i2, b1, b2;
    (void)outSize;

    if (a1) {
        const char *rep = envLookup(env, a1);
        if (rep) a1 = rep;
    }
    if (a2) {
        const char *rep = envLookup(env, a2);
        if (rep) a2 = rep;
    }

    if (in->op == TAC_ASSIGN && a1) {
        sprintf(outBuf, "%s", a1);
        return 1;
    }
    if (in->op == TAC_NEG && a1 && isNumericLit(a1, &d1, &i1)) {
        if (i1) sprintf(outBuf, "%d", -(int)d1);
        else    sprintf(outBuf, "%.15g", -d1);
        return 1;
    }
    if (in->op == TAC_NOT && a1 && isBoolLit(a1, &b1)) {
        sprintf(outBuf, "%s", b1 ? "false" : "true");
        return 1;
    }
    if (in->op >= TAC_ADD && in->op <= TAC_NE && a1 && a2 &&
        isNumericLit(a1, &d1, &i1) && isNumericLit(a2, &d2, &i2)) {
        //int isInt = i1 && i2;
        int isInt = i1 && i2 && (in->type1 != T_REAL);
        if (in->op <= TAC_DIV) {
            double rv = 0.0;
            if (in->op == TAC_ADD) rv = d1 + d2;
            else if (in->op == TAC_SUB) rv = d1 - d2;
            else if (in->op == TAC_MUL) rv = d1 * d2;
            else {
                if (d2 == 0.0) return 0;
                rv = isInt ? (double)((int)d1 / (int)d2) : (d1 / d2);
            }
            if (isInt) sprintf(outBuf, "%d", (int)rv);
            else       sprintf(outBuf, "%.15g", rv);
            return 1;
        } else {
            int br = 0;
            if (in->op == TAC_LT)      br = (d1 < d2);
            else if (in->op == TAC_LE) br = (d1 <= d2);
            else if (in->op == TAC_GT) br = (d1 > d2);
            else if (in->op == TAC_GE) br = (d1 >= d2);
            else if (in->op == TAC_EQ) br = (d1 == d2);
            else if (in->op == TAC_NE) br = (d1 != d2);
            sprintf(outBuf, "%d", br);
            return 1;
        }
    }
    if ((in->op == TAC_AND || in->op == TAC_OR) && a1 && a2 &&
        (isBoolLit(a1, &b1) || (isNumericLit(a1, &d1, &i1) && ((b1 = (d1 != 0.0)), 1))) &&
        (isBoolLit(a2, &b2) || (isNumericLit(a2, &d2, &i2) && ((b2 = (d2 != 0.0)), 1)))) {
        int br = (in->op == TAC_AND) ? (b1 && b2) : (b1 || b2);
        sprintf(outBuf, "%s", br ? "true" : "false");
        return 1;
    }
    return 0;
}

static int tacTryUnrollFirstLoop(TACList *tac)
{
    TACList scratch;
    int peel;
    
    int savedLabel = s_label;

    tacCloneList(tac, &scratch);

    for (peel = 0; peel < 16; peel++) {
        CFG cfg;
        unsigned char *dom;
        int bH, bPrev, bT = -1, n, i, k;
        int foundLoop = 0;
        const char *hdrLabel;
        CopyEnv env;
        int condConst = 0, condTrue = 0;

        tacCompact(&scratch);
        if (scratch.count == 0) { tacFree(&scratch); s_label = savedLabel; return 0; }

        cfgBuild(&scratch, &cfg);
        n = cfg.numBlocks;
        dom = cfgComputeDominators(&cfg);
        if (!dom) { cfgFree(&cfg); tacFree(&scratch); s_label = savedLabel; return 0; }

        /* Cerca un ciclo `while` semplice e piccolo */
        for (bH = 1; bH < n; bH++) {
            int backPred = -1, extPred = -1, valid = 1;
            int loopStartIdx, loopEndIdx;
            const TACInstr *lastH, *lastT;

            if (cfg.blocks[bH].numPreds != 2) continue;
            if (scratch.items[cfg.blocks[bH].start].op != TAC_LABEL) continue;

            for (i = 0; i < 2; i++) {
                int p = cfg.blocks[bH].preds[i];
                if (dom[p * n + bH]) backPred = p;
                else                 extPred = p;
            }
            if (backPred < 0 || extPred != bH - 1) continue;
            if (cfg.blocks[extPred].numSuccs != 1) continue;
            if (scratch.items[cfg.blocks[extPred].end].op == TAC_GOTO) continue;

            bT = backPred;
            if (bT < bH) continue;

            loopStartIdx = cfg.blocks[bH].start;
            loopEndIdx   = cfg.blocks[bT].end;
            if (loopEndIdx - loopStartIdx + 1 > 24) continue;

            lastH = &scratch.items[cfg.blocks[bH].end];
            lastT = &scratch.items[cfg.blocks[bT].end];
            if (lastH->op != TAC_IF_FALSE) continue;
            if (lastT->op != TAC_GOTO || !lastT->res ||
                strcmp(lastT->res, scratch.items[loopStartIdx].res) != 0) continue;
            
            /* Evita unrolling su cicli con I/O (read / print) */
            for (k = loopStartIdx; k <= loopEndIdx && valid; k++) {
                TACOp op = scratch.items[k].op;
                if (op == TAC_READ || op == TAC_PRINT) {
                    valid = 0;
                    break;
                }
            }
            if (!valid) continue;
             /* Nessun blocco esterno deve saltare dentro [bH+1 .. bT] */
            for (k = bH + 1; k <= bT && valid; k++) {
                int p;
                for (p = 0; p < cfg.blocks[k].numPreds; p++) {
                    int pred = cfg.blocks[k].preds[p];
                    if (pred < bH || pred > bT) { valid = 0; break; }
                }
            }
            if (!valid) continue;

            foundLoop = 1;
            bPrev = extPred;
            break;
        }

        if (!foundLoop) {
            free(dom);
            cfgFree(&cfg);
            if (peel > 0) {
                /* Il ciclo e' stato completamente eliminato! */
                tacSwapLists(tac, &scratch);
                tacFree(&scratch);
                return 1;
            }
            tacFree(&scratch);
            
            s_label = savedLabel;
            
            return 0;
        }

        /* Valuta costanti lungo `bPrev` poi lungo `bH` fino al salto */
        env.items = NULL; env.count = 0; env.capacity = 0;
        for (i = cfg.blocks[bPrev].start; i <= cfg.blocks[bPrev].end; i++) {
            const TACInstr *in = &scratch.items[i];
            char valBuf[128];
            if (in->dead) continue;
            if (in->res && isScalarWriteOp(in->op)) {
                if (evalScalarInstr(in, &env, valBuf, sizeof(valBuf))) {
                    envSet(&env, in->res, valBuf);
                } else {
                    envSet(&env, in->res, NULL);
                }
            }
        }
        for (i = cfg.blocks[bH].start; i < cfg.blocks[bH].end; i++) {
            const TACInstr *in = &scratch.items[i];
            char valBuf[128];
            if (in->dead || in->op == TAC_LABEL) continue;
            if (in->res && isScalarWriteOp(in->op)) {
                if (evalScalarInstr(in, &env, valBuf, sizeof(valBuf))) {
                    envSet(&env, in->res, valBuf);
                } else {
                    envSet(&env, in->res, NULL);
                }
            } else {
                condConst = -1; /* Istruzione con side effect in bH */
                break;
            }
        }

        if (condConst == 0) {
            const TACInstr *br = &scratch.items[cfg.blocks[bH].end];
            const char *cStr = envLookup(&env, br->arg1);
            double d;
            int isInt, bVal;
            if (!cStr) cStr = br->arg1;
            if (cStr && isBoolLit(cStr, &bVal)) {
                condConst = 1;
                condTrue = bVal;
            } else if (cStr && isNumericLit(cStr, &d, &isInt)) {
                condConst = 1;
                condTrue = (d != 0.0);
            }
        }
        envFree(&env);

        if (condConst != 1) {
            free(dom); cfgFree(&cfg); tacFree(&scratch);
            
            s_label = savedLabel;
            
            return 0;
        }

        hdrLabel = scratch.items[cfg.blocks[bH].start].res;

        if (!condTrue) {
            /* Condizione falsa all'ingresso del ciclo: salta all'uscita */
            int hdrStart = cfg.blocks[bH].start;
            int hdrBodyCount = cfg.blocks[bH].end - hdrStart; /* esclude label e br */
            TACInstr *exitIns = (TACInstr *)calloc((size_t)hdrBodyCount + 1, sizeof(TACInstr));
            if (exitIns) {
                int w = 0;
                for (i = hdrStart + 1; i < cfg.blocks[bH].end; i++) {
                    exitIns[w++] = scratch.items[i];
                }
                exitIns[w].op = TAC_GOTO;
                exitIns[w].res = scratch.items[cfg.blocks[bH].end].res;
                exitIns[w].type1 = T_INTEGER;
                w++;
                tacInsertInstructions(&scratch, hdrStart, exitIns, w);
                free(exitIns);
            }
            free(dom); cfgFree(&cfg);
            tacOptimizeO2(&scratch);
            tacSwapLists(tac, &scratch);
            tacFree(&scratch);
            return 1;
        } else {
            /* Condizione vera: srotola una iterazione prima di `bH` */
            int hdrStart = cfg.blocks[bH].start;
            int bodyStart = hdrStart + 1;
            int bodyEnd = cfg.blocks[bT].end - 1; /* esclude goto hdrLabel finale */
            int bodyLen = bodyEnd - bodyStart + 1;
            TACInstr *peeled = (TACInstr *)calloc((size_t)(bodyLen > 0 ? bodyLen : 1), sizeof(TACInstr));
            CopyEnv lblMap;
            int w = 0;

            lblMap.items = NULL; lblMap.count = 0; lblMap.capacity = 0;
            for (i = bodyStart; i <= bodyEnd; i++) {
                if (scratch.items[i].op == TAC_LABEL && scratch.items[i].res) {
                    char *nl = newLabel();
                    envSet(&lblMap, scratch.items[i].res, nl);
                    free(nl);
                }
            }

            if (peeled) {
                for (i = bodyStart; i <= bodyEnd; i++) {
                    if (i == cfg.blocks[bH].end) continue; /* salta il check noto vero */
                    peeled[w] = scratch.items[i];
                    if (peeled[w].op == TAC_LABEL || peeled[w].op == TAC_GOTO ||
                        peeled[w].op == TAC_IF_FALSE || peeled[w].op == TAC_IF_TRUE) {
                        const char *mapped = envLookup(&lblMap, peeled[w].res);
                        if (mapped) peeled[w].res = (char *)mapped;
                    }
                    (void)hdrLabel;
                    w++;
                }
                tacInsertInstructions(&scratch, hdrStart, peeled, w);
                free(peeled);
            }
            envFree(&lblMap);
            free(dom); cfgFree(&cfg);
            tacOptimizeO1(&scratch);
            tacOptimizeO2(&scratch);
        }
    }

    tacFree(&scratch);
    
    s_label = savedLabel;
    
    return 0;
}

void tacOptimizeO3(TACList *tac)
{
    int changed;
    tacOptimizeO2(tac);

    do {
        CFG cfg;
        int stepChanged = 0;

        stepChanged |= tacStrengthReduction(tac);
        if (stepChanged) tacOptimizeO1(tac);

        tacCompact(tac);
        if (tac->count == 0) break;

        cfgBuild(tac, &cfg);
        if (cfgJumpThreading(tac, &cfg)) stepChanged = 1;
        if (cfgLICM(tac, &cfg))          stepChanged = 1;
        cfgFree(&cfg);

        if (tacTryUnrollFirstLoop(tac))  stepChanged = 1;

        if (stepChanged) {
            tacOptimizeO2(tac);
        }
        changed = stepChanged;
    } while (changed);

	tacOptimizeO1(tac);
	
    tacCompact(tac);
}
