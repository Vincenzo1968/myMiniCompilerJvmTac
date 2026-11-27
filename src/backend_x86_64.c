#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "backend_x86_64.h"

typedef struct {
    char *label;
    char *raw;
} StrConst;

typedef struct {
    StrConst *items;
    int count;
    int capacity;
} StrConstTable;

static bool is_string_literal(const char *str)
{
    if (!str) return false;
    size_t len = strlen(str);
    return (len >= 2 && str[0] == '"' && str[len - 1] == '"');
}

static const char *get_str_const_label(const char *raw, StrConstTable *st)
{
    if (!raw) return NULL;
    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->items[i].raw, raw) == 0) {
            return st->items[i].label;
        }
    }
    if (st->count >= st->capacity) {
        int newCap = (st->capacity == 0) ? 32 : st->capacity * 2;
        st->items = (StrConst *)realloc(st->items, (size_t)newCap * sizeof(StrConst));
        st->capacity = newCap;
    }
    char label[32];
    snprintf(label, sizeof(label), "str_%d", st->count);
    st->items[st->count].label = strdup(label);
    st->items[st->count].raw = strdup(raw);
    st->count++;
    return st->items[st->count - 1].label;
}

static void emit_nasm_string_def(FILE *out, const char *label, const char *raw)
{
    size_t len = strlen(raw);
    fprintf(out, "    %-12s db ", label);
    for (size_t i = 1; i + 1 < len; i++) {
        fprintf(out, "%u, ", (unsigned char)raw[i]);
    }
    fprintf(out, "0\n");
}

static const char* get_real_const_label(CompilerContext *ctx, double val)
{
    for (int i = 0; i < ctx->real_const_count; i++) {
        if (ctx->real_consts[i].value == val) {
            return ctx->real_consts[i].label;
        }
    }
    ctx->real_consts = realloc(ctx->real_consts, (ctx->real_const_count + 1) * sizeof(RealConst));
    char label[32];
    sprintf(label, "rc_%d", ctx->real_const_count);
    ctx->real_consts[ctx->real_const_count].label = strdup(label);
    ctx->real_consts[ctx->real_const_count].value = val;
    ctx->real_const_count++;
    return ctx->real_consts[ctx->real_const_count - 1].label;
}

static VarSlot *find_slot(VarTable *vt, const char *name)
{
    for (int i = 0; i < vt->count; i++)
        if (strcmp(vt->items[i].name, name) == 0)
            return &vt->items[i];
    return NULL;
}

static VarSlot *new_slot(VarTable *vt, const char *name, enumTipo1 type, int offset, int kind)
{
    if (vt->count >= vt->capacity) {
        int newCap = (vt->capacity == 0) ? 64 : vt->capacity * 2;
        vt->items = (VarSlot *)realloc(vt->items, newCap * sizeof(VarSlot));
        vt->capacity = newCap;
    }
    VarSlot *sl = &vt->items[vt->count++];
    sl->name = strdup(name);
    sl->offset = offset;
    sl->type1 = type;
    sl->kind = kind;
    return sl;
}

/* 8-byte slot in the stack frame (if the variable is not there yet) and its offset. */
static int get_var_offset(const char *name, enumTipo1 type, VarTable *vt)
{
    if (!name) return 0;
    VarSlot *sl = find_slot(vt, name);
    if (sl) return sl->offset;
    sl = new_slot(vt, name, type, vt->current_offset, SLOT_FRAME);
    vt->current_offset -= 8;
    return sl->offset;
}

static void add_global(GlobalTable *gt, const char *name, enumTipo1 type, int bytes)
{
    for (int i = 0; i < gt->count; i++)
        if (strcmp(gt->items[i].name, name) == 0)
            return;
    if (gt->count >= gt->capacity) {
        int newCap = (gt->capacity == 0) ? 32 : gt->capacity * 2;
        gt->items = (GlobalVar *)realloc(gt->items, newCap * sizeof(GlobalVar));
        gt->capacity = newCap;
    }
    gt->items[gt->count].name = strdup(name);
    gt->items[gt->count].type1 = type;
    gt->items[gt->count].bytes = (bytes + 7) & ~7;
    gt->count++;
}

/* Bytes occupied by one element (same values used by TAC for offsets). */
static int element_size(enumTipo1 t)
{
    switch (t) {
        case T_INTEGER: return 4;
        case T_REAL:    return 8;
        case T_BOOLEAN: return 1;
        case T_STRING:  return 8;
    }
    return 4;
}

/* Registers an array: static if global, otherwise a contiguous block in the stack frame
   (element k at base address + k*size). Parameter arrays are already registered. */
static void register_array(CompilerContext *ctx, Scope *pScope, const char *name, VarTable *vt)
{
    HashTable d;
    long long n = 1;
    int bytes;
    if (find_slot(vt, name)) return;
    memset(&d, 0, sizeof(d));
    if (!(pScope && scopeFind(ctx, pScope, (char *)name, &d) && d.Tipo0 == T_ARRAY && d.Value.pArrayDef)) {
        get_var_offset(name, T_INTEGER, vt);
        return;
    }
    for (int k = 0; k < d.Value.pArrayDef->countIndexes; k++)
        n *= d.Value.pArrayDef->sizes[k];
    bytes = (int)(n * element_size(d.Tipo1));
    bytes = (bytes + 7) & ~7;
    if (bytes < 8) bytes = 8;

    if (d.isGlobal) {
        add_global(vt->gt, name, d.Tipo1, bytes);
        new_slot(vt, name, d.Tipo1, 0, SLOT_GLOBAL);
    } else {
        new_slot(vt, name, d.Tipo1, vt->current_offset - (bytes - 8), SLOT_FRAME);
        vt->current_offset -= bytes;
    }
}

static bool is_bool_literal(const char *str)
{
    return str && (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);
}

/* Numeric or boolean constant (true/false): does not occupy a stack frame slot. */
static bool is_numeric_constant(const char *str)
{
    if (!str || *str == '\0') return false;
    if (is_bool_literal(str)) return true;
    char *endptr;
    strtod(str, &endptr);
    return (*endptr == '\0');
}

static long long const_ivalue(const char *str)
{
    if (strcmp(str, "true") == 0) return 1;
    if (strcmp(str, "false") == 0) return 0;
    return strtoll(str, NULL, 10);
}

/* Type of a TAC operand: literals from their syntax, variables and temporaries
   dal tipo con cui sono stati registrati. */
static enumTipo1 operand_type(const char *s, VarTable *vt, enumTipo1 dflt)
{
    if (!s) return dflt;
    if (is_string_literal(s)) return T_STRING;
    if (is_bool_literal(s)) return T_BOOLEAN;
    if (is_numeric_constant(s))
        return (strpbrk(s, ".eE") != NULL) ? T_REAL : T_INTEGER;
    VarSlot *sl = find_slot(vt, s);
    return sl ? sl->type1 : dflt;
}

/* Registers a variable or temporary. For user variables, the type is the one
   dichiarato (la symtab contiene i nomi univoci); le globali vanno in memoria statica. */
static void register_operand(CompilerContext *ctx, Scope *pScope, const char *name, enumTipo1 dflt, VarTable *vt)
{
    enumTipo1 t = dflt;
    HashTable d;
    if (find_slot(vt, name)) return;
    memset(&d, 0, sizeof(d));
    if (pScope && scopeFind(ctx, pScope, (char *)name, &d)) {
        t = d.Tipo1;
        if (d.isGlobal && d.Tipo0 == T_SIMPLE) {
            add_global(vt->gt, name, t, 8);
            new_slot(vt, name, t, 0, SLOT_GLOBAL);
            return;
        }
    }
    get_var_offset(name, t, vt);
}

/* Memory operand of a scalar variable: "[rbp + n]", "[rel g_x]", or "[r10]"
   (parametro 'ref': prima si carica in r10 l'indirizzo). */
static void mem_ref(FILE *out, VarTable *vt, const char *name, enumTipo1 type, char *buf, size_t n)
{
    VarSlot *sl = find_slot(vt, name);
    if (!sl) {
        get_var_offset(name, type, vt);
        sl = find_slot(vt, name);
    }
    switch (sl->kind) {
        case SLOT_GLOBAL:
            snprintf(buf, n, "[rel g_%s]", name);
            break;
        case SLOT_REFPARAM:
            fprintf(out, "    mov r10, [rbp + %d]\n", sl->offset);
            snprintf(buf, n, "[r10]");
            break;
        default:
            snprintf(buf, n, "[rbp + %d]", sl->offset);
            break;
    }
}

/* Base address of an array in a register. */
static void array_base(FILE *out, VarTable *vt, const char *name, const char *reg)
{
    VarSlot *sl = find_slot(vt, name);
    if (!sl) {
        fprintf(out, "    xor %s, %s\n", reg, reg);
        return;
    }
    switch (sl->kind) {
        case SLOT_GLOBAL:   fprintf(out, "    lea %s, [rel g_%s]\n", reg, name); break;
        case SLOT_ARRPARAM: fprintf(out, "    mov %s, [rbp + %d]\n", reg, sl->offset); break;
        default:            fprintf(out, "    lea %s, [rbp + %d]\n", reg, sl->offset); break;
    }
}

/* Address of a scalar variable in a register (for 'param ref' and scanf). */
static void var_address(FILE *out, VarTable *vt, const char *name, enumTipo1 type, const char *reg)
{
    VarSlot *sl = find_slot(vt, name);
    if (!sl) {
        get_var_offset(name, type, vt);
        sl = find_slot(vt, name);
    }
    switch (sl->kind) {
        case SLOT_GLOBAL:   fprintf(out, "    lea %s, [rel g_%s]\n", reg, name); break;
        case SLOT_REFPARAM: fprintf(out, "    mov %s, [rbp + %d]\n", reg, sl->offset); break;
        default:            fprintf(out, "    lea %s, [rbp + %d]\n", reg, sl->offset); break;
    }
}

/* An uninitialized string (NULL pointer) behaves like "" (as in the interpreter). */
static void emit_string_default(FILE *out, VarTable *vt)
{
    int n = vt->gt->lbl++;
    fprintf(out, "    test rax, rax\n");
    fprintf(out, "    jnz .Lstr%d\n", n);
    fprintf(out, "    lea rax, [rel str_empty]\n");
    fprintf(out, ".Lstr%d:\n", n);
}

static void emit_load_operand(CompilerContext *ctx, FILE *out, const char *operand, enumTipo1 type, VarTable *vt, StrConstTable *st)
{
    char m[64];
    if (!operand) return;

    if (is_string_literal(operand)) {
        const char *label = get_str_const_label(operand, st);
        fprintf(out, "    lea rax, [rel %s]\n", label);
        return;
    }

    if (is_numeric_constant(operand)) {
        if (type == T_REAL) {
            double val = strtod(operand, NULL);
            const char *label = get_real_const_label(ctx, val);
            fprintf(out, "    movsd xmm0, [rel %s]\n", label);
        } else {
            long long val = const_ivalue(operand);
            fprintf(out, "    mov rax, %lld\n", val);
        }
        return;
    }

    mem_ref(out, vt, operand, type, m, sizeof(m));
    if (type == T_REAL) {
        fprintf(out, "    movsd xmm0, %s\n", m);
    } else {
        fprintf(out, "    mov rax, %s\n", m);
        if (type == T_STRING)
            emit_string_default(out, vt);
    }
}

/* Loads an operand as double into xmm<reg>, converting it if it is an integer. */
static void emit_load_real(CompilerContext *ctx, FILE *out, const char *operand, VarTable *vt, int reg)
{
    char m[64];
    if (is_numeric_constant(operand)) {
        const char *label = get_real_const_label(ctx, is_bool_literal(operand) ? (double)const_ivalue(operand) : strtod(operand, NULL));
        fprintf(out, "    movsd xmm%d, [rel %s]\n", reg, label);
        return;
    }
    enumTipo1 t = operand_type(operand, vt, T_REAL);
    mem_ref(out, vt, operand, T_REAL, m, sizeof(m));
    if (t == T_REAL) {
        fprintf(out, "    movsd xmm%d, %s\n", reg, m);
    } else {
        fprintf(out, "    mov rax, %s\n", m);
        fprintf(out, "    cvtsi2sd xmm%d, rax\n", reg);
    }
}

/* Loads an integer/boolean operand into a general-purpose register (constants included). */
static void emit_load_int_reg(FILE *out, const char *operand, VarTable *vt, const char *reg)
{
    char m[64];
    if (is_numeric_constant(operand)) {
        fprintf(out, "    mov %s, %lld\n", reg, const_ivalue(operand));
    } else {
        mem_ref(out, vt, operand, T_INTEGER, m, sizeof(m));
        fprintf(out, "    mov %s, %s\n", reg, m);
    }
}

/* Loads an operand into the destination type register (rax or xmm0), converting if needed:
   int -> real con cvtsi2sd, real -> int/bool con troncamento (cvttsd2si), come l'interprete. */
static void emit_load_as(CompilerContext *ctx, FILE *out, const char *operand, enumTipo1 dest,
                         VarTable *vt, StrConstTable *st)
{
    enumTipo1 src = operand_type(operand, vt, dest);
    if (dest == T_REAL) {
        emit_load_real(ctx, out, operand, vt, 0);
    } else if (dest != T_STRING && src == T_REAL) {
        emit_load_real(ctx, out, operand, vt, 0);
        fprintf(out, "    cvttsd2si rax, xmm0\n");
    } else {
        emit_load_operand(ctx, out, operand, dest, vt, st);
    }
}

static void emit_store_operand(FILE *out, const char *dest, enumTipo1 type, VarTable *vt)
{
    char m[64];
    if (!dest) return;
    mem_ref(out, vt, dest, type, m, sizeof(m));
    if (type == T_REAL) {
        fprintf(out, "    movsd %s, xmm0\n", m);
    } else {
        fprintf(out, "    mov %s, rax\n", m);
    }
}

/* Generates a function (fd != NULL) or the main block (fd == NULL) starting
   dalle istruzioni TAC [start, end). Ogni funzione ha il proprio stack frame.
   Convenzione di chiamata: argomenti impilati in ordine (il primo per primo, 8 byte
   ciascuno), a chi chiama il compito di toglierli; risultato in rax (xmm0 se real). */
static void gen_segment(CompilerContext *ctx, FILE *out, TACList *tac, int start, int end, Scope *pScope,
                        StrConstTable *st_, GlobalTable *gt, const FuncDef *fd)
{
    VarTable vt;
    StrConstTable st = *st_;   /* working copy, written back to st_ at the end */
    vt.items = NULL;
    vt.count = 0;
    vt.capacity = 0;
    vt.current_offset = -8;
    vt.gt = gt;

    /* argomenti: p0 all'indirizzo piu' alto, poi a scendere (rbp+8 = indirizzo di ritorno) */
    if (fd) {
        for (int k = 0; k < fd->nparams; k++) {
            const FuncParam *fp = &fd->params[k];
            int off = 16 + 8 * (fd->nparams - 1 - k);
            new_slot(&vt, fp->uniqueName ? fp->uniqueName : fp->name, fp->t1, off,
                     fp->t0 == T_ARRAY ? SLOT_ARRPARAM : (fp->isRef ? SLOT_REFPARAM : SLOT_FRAME));
        }
    }

    /* PASSAGGIO 1: costanti stringa, variabili e temporanei */
    for (int i = start; i < end; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;

        bool isCmp = (in->op >= TAC_LT && in->op <= TAC_NE);

        /* array access: the array name occupies a block, the offset is an integer */
        if (in->op == TAC_LOAD_IDX || in->op == TAC_STORE_IDX) {
            const char *arr = (in->op == TAC_LOAD_IDX) ? in->arg1 : in->res;
            const char *off = (in->op == TAC_LOAD_IDX) ? in->arg2 : in->arg1;
            register_array(ctx, pScope, arr, &vt);
            if (off && !is_numeric_constant(off))
                register_operand(ctx, pScope, off, T_INTEGER, &vt);
            if (in->op == TAC_LOAD_IDX) {
                if (in->res) register_operand(ctx, pScope, in->res, in->type1, &vt);
            } else if (in->arg2) {
                if (is_string_literal(in->arg2))
                    get_str_const_label(in->arg2, &st);
                else if (!is_numeric_constant(in->arg2))
                    register_operand(ctx, pScope, in->arg2, in->type1, &vt);
            }
            continue;
        }

        if (in->op == TAC_PARAM_ARR) {
            register_array(ctx, pScope, in->arg1, &vt);
            continue;
        }

        if (in->op == TAC_CALL) {
            /* arg1 = function name, arg2 = argument count: these are not variables */
            if (in->res) register_operand(ctx, pScope, in->res, in->type1, &vt);
            continue;
        }

        if (in->op != TAC_LABEL && in->op != TAC_GOTO && in->op != TAC_IF_FALSE && in->op != TAC_IF_TRUE) {
            if (in->res && !is_string_literal(in->res) && !is_numeric_constant(in->res))
                register_operand(ctx, pScope, in->res, isCmp ? T_BOOLEAN : in->type1, &vt);
        }

        if (in->arg1) {
            if (is_string_literal(in->arg1))
                get_str_const_label(in->arg1, &st);
            else if (!is_numeric_constant(in->arg1))
                register_operand(ctx, pScope, in->arg1, isCmp ? T_INTEGER : in->type1, &vt);
        }

        if (in->arg2) {
            if (is_string_literal(in->arg2))
                get_str_const_label(in->arg2, &st);
            else if (!is_numeric_constant(in->arg2))
                register_operand(ctx, pScope, in->arg2, isCmp ? T_INTEGER : in->type1, &vt);
        }

        /* scratch variables for string concatenation ('$' is not valid in identifiers) */
        if (in->op == TAC_ADD && in->type1 == T_STRING) {
            get_var_offset("$cat_a", T_STRING, &vt);
            get_var_offset("$cat_b", T_STRING, &vt);
            get_var_offset("$cat_r", T_STRING, &vt);
        }
    }

    int frame_size = -vt.current_offset;
    if ((frame_size % 16) != 0) {
        frame_size = ((frame_size / 16) + 1) * 16;
    }

    /* PASSAGGIO 3: prologo */
    if (fd)
        fprintf(out, "f_%s:\n", fd->name);
    else
        fprintf(out, "main:\n");
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");
    fprintf(out, "    sub rsp, %d  ; Allocazione stack frame\n\n", frame_size);

    fprintf(out, "    ; Inizializza lo stack frame a zero\n");
    fprintf(out, "    mov rcx, %d\n", frame_size / 8);
    fprintf(out, "    xor rax, rax\n");
    fprintf(out, "    lea rdi, [rbp - %d]\n", frame_size);
    fprintf(out, "    rep stosq\n\n");

    /* PASS 4: translation of TAC instructions */
    for (int i = start; i < end; i++)
    {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;

        fprintf(out, "    ; %s\n", in->res ? in->res : "");
        switch (in->op) {
            case TAC_LABEL:
                fprintf(out, "%s:\n", in->res);
                break;

            case TAC_GOTO:
                fprintf(out, "    jmp %s\n", in->res);
                break;

            case TAC_IF_FALSE:
                emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                if (in->type1 == T_REAL)
                {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    ucomisd xmm0, xmm1\n");
                    fprintf(out, "    jp %s\n", in->res);
                    fprintf(out, "    je %s\n", in->res);
                }
                else
                {
                    fprintf(out, "    test rax, rax\n");
                    fprintf(out, "    jz %s\n", in->res);
                }
                break;

            case TAC_IF_TRUE:
                emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                if (in->type1 == T_REAL)
                {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    ucomisd xmm0, xmm1\n");
                    fprintf(out, "    jnp %s\n", in->res);
                    fprintf(out, "    jne %s\n", in->res);
                }
                else
                {
                    fprintf(out, "    test rax, rax\n");
                    fprintf(out, "    jnz %s\n", in->res);
                }
                break;

            case TAC_ASSIGN:
                emit_load_as(ctx, out, in->arg1, in->type1, &vt, &st);
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_NEG:
                emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                if (in->type1 == T_REAL)
                {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    subsd xmm1, xmm0\n");
                    fprintf(out, "    movapd xmm0, xmm1\n");
                }
                else
                {
                    fprintf(out, "    neg rax\n");
                }
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_NOT:
                emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                fprintf(out, "    xor rax, 1\n");
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV: {
                if (in->type1 == T_STRING && in->op == TAC_ADD)
                {
                    int oa = get_var_offset("$cat_a", T_STRING, &vt);
                    int ob = get_var_offset("$cat_b", T_STRING, &vt);
                    int orr = get_var_offset("$cat_r", T_STRING, &vt);
                    emit_load_operand(ctx, out, in->arg1, T_STRING, &vt, &st);
                    fprintf(out, "    mov [rbp + %d], rax\n", oa);
                    emit_load_operand(ctx, out, in->arg2, T_STRING, &vt, &st);
                    fprintf(out, "    mov [rbp + %d], rax\n", ob);
                    fprintf(out, "    mov rdi, [rbp + %d]\n", oa);
                    fprintf(out, "    call strlen\n");
                    fprintf(out, "    mov [rbp + %d], rax\n", orr);
                    fprintf(out, "    mov rdi, [rbp + %d]\n", ob);
                    fprintf(out, "    call strlen\n");
                    fprintf(out, "    add rax, [rbp + %d]\n", orr);
                    fprintf(out, "    lea rdi, [rax + 1]\n");
                    fprintf(out, "    call malloc\n");
                    fprintf(out, "    mov [rbp + %d], rax\n", orr);
                    fprintf(out, "    mov rdi, rax\n");
                    fprintf(out, "    mov rsi, [rbp + %d]\n", oa);
                    fprintf(out, "    call strcpy\n");
                    fprintf(out, "    mov rdi, [rbp + %d]\n", orr);
                    fprintf(out, "    mov rsi, [rbp + %d]\n", ob);
                    fprintf(out, "    call strcat\n");
                    fprintf(out, "    mov rax, [rbp + %d]\n", orr);
                }
                else if (in->type1 == T_REAL)
                {
                    emit_load_real(ctx, out, in->arg1, &vt, 0);
                    emit_load_real(ctx, out, in->arg2, &vt, 1);
                    if (in->op == TAC_ADD) fprintf(out, "    addsd xmm0, xmm1\n");
                    else if (in->op == TAC_SUB) fprintf(out, "    subsd xmm0, xmm1\n");
                    else if (in->op == TAC_MUL) fprintf(out, "    mulsd xmm0, xmm1\n");
                    else if (in->op == TAC_DIV) fprintf(out, "    divsd xmm0, xmm1\n");
                }
                else
                {
                    emit_load_operand(ctx, out, in->arg1, T_INTEGER, &vt, &st);
                    emit_load_int_reg(out, in->arg2, &vt, "rcx");
                    if (in->op == TAC_ADD) fprintf(out, "    add rax, rcx\n");
                    else if (in->op == TAC_SUB) fprintf(out, "    sub rax, rcx\n");
                    else if (in->op == TAC_MUL) fprintf(out, "    imul rax, rcx\n");
                    else if (in->op == TAC_DIV)
                    {
                        fprintf(out, "    cqo\n");
                        fprintf(out, "    idiv rcx\n");
                    }
                }
                emit_store_operand(out, in->res, in->type1, &vt);
                break;
            }
            case TAC_PRINT:
            {
                if (in->type1 == T_INTEGER || in->type1 == T_BOOLEAN)
                {
                    emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                    fprintf(out, "    mov rdi, fmt_int\n");
                    fprintf(out, "    mov rsi, rax\n");
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call printf\n");
                }
                else if (in->type1 == T_REAL)
                {
                    emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                    fprintf(out, "    mov rdi, fmt_real\n");
                    fprintf(out, "    mov rax, 1\n");
                    fprintf(out, "    call printf\n");
                }
                else if (in->type1 == T_STRING)
                {
                    emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
                    fprintf(out, "    mov rdi, fmt_str\n");
                    fprintf(out, "    mov rsi, rax\n");
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call printf\n");
                }
                break;
            }
            case TAC_READ:
            {
                var_address(out, &vt, in->res, in->type1, "rsi");
                fprintf(out, "    mov rdi, %s\n", in->type1 == T_REAL ? "fmt_scan_real" : "fmt_scan_int");
                fprintf(out, "    mov rax, 0\n");
                fprintf(out, "    call scanf\n");
                break;
            }
            case TAC_LOAD_IDX:
            case TAC_STORE_IDX:
            {
                bool isLoad = (in->op == TAC_LOAD_IDX);
                const char *arr = isLoad ? in->arg1 : in->res;
                const char *off = isLoad ? in->arg2 : in->arg1;
                enumTipo1 et = in->type1;

                if (!isLoad)
                {
                    /* value to store in rax / xmm0 (first, because rcx and r11 are needed later) */
                    emit_load_as(ctx, out, in->arg2, et, &vt, &st);
                }
                emit_load_int_reg(out, off, &vt, "rcx");
                array_base(out, &vt, arr, "r11");
                fprintf(out, "    add r11, rcx\n");

                if (isLoad)
                {
                    if (et == T_REAL)         fprintf(out, "    movsd xmm0, [r11]\n");
                    else if (et == T_INTEGER) fprintf(out, "    movsxd rax, dword [r11]\n");
                    else if (et == T_BOOLEAN) fprintf(out, "    movzx eax, byte [r11]\n");
                    else
                    {
                        fprintf(out, "    mov rax, [r11]\n");
                        emit_string_default(out, &vt);
                    }
                    emit_store_operand(out, in->res, et, &vt);
                }
                else
                {
                    if (et == T_REAL)         fprintf(out, "    movsd [r11], xmm0\n");
                    else if (et == T_INTEGER) fprintf(out, "    mov [r11], eax\n");
                    else if (et == T_BOOLEAN) fprintf(out, "    mov [r11], al\n");
                    else                      fprintf(out, "    mov [r11], rax\n");
                }
                break;
            }
            case TAC_LT:
            case TAC_LE:
            case TAC_GT:
            case TAC_GE:
            case TAC_EQ:
            case TAC_NE:
            {
                bool realCmp = operand_type(in->arg1, &vt, T_INTEGER) == T_REAL ||
                               operand_type(in->arg2, &vt, T_INTEGER) == T_REAL;
                if (realCmp)
                {
                    emit_load_real(ctx, out, in->arg1, &vt, 0);
                    emit_load_real(ctx, out, in->arg2, &vt, 1);
                    fprintf(out, "    ucomisd xmm0, xmm1\n");
                    fprintf(out, "    mov rax, 0\n");
                    if (in->op == TAC_LT)      fprintf(out, "    setb al\n");
                    else if (in->op == TAC_LE) fprintf(out, "    setbe al\n");
                    else if (in->op == TAC_GT) fprintf(out, "    seta al\n");
                    else if (in->op == TAC_GE) fprintf(out, "    setae al\n");
                    else if (in->op == TAC_EQ) fprintf(out, "    sete al\n");
                    else if (in->op == TAC_NE) fprintf(out, "    setne al\n");
                }
                else
                {
                    emit_load_operand(ctx, out, in->arg1, T_INTEGER, &vt, &st);
                    emit_load_int_reg(out, in->arg2, &vt, "rcx");
                    fprintf(out, "    cmp rax, rcx\n");
                    fprintf(out, "    mov rax, 0\n");
                    if (in->op == TAC_LT)      fprintf(out, "    setl al\n");
                    else if (in->op == TAC_LE) fprintf(out, "    setle al\n");
                    else if (in->op == TAC_GT) fprintf(out, "    setg al\n");
                    else if (in->op == TAC_GE) fprintf(out, "    setge al\n");
                    else if (in->op == TAC_EQ) fprintf(out, "    sete al\n");
                    else if (in->op == TAC_NE) fprintf(out, "    setne al\n");
                }
                fprintf(out, "    movzx rax, al\n");
                emit_store_operand(out, in->res, T_BOOLEAN, &vt);
                break;
            }
            case TAC_AND:
            case TAC_OR:
            {
                emit_load_operand(ctx, out, in->arg1, T_BOOLEAN, &vt, &st);
                emit_load_int_reg(out, in->arg2, &vt, "rcx");
                fprintf(out, "    %s rax, rcx\n", in->op == TAC_AND ? "and" : "or");
                emit_store_operand(out, in->res, T_BOOLEAN, &vt);
                break;
            }
            case TAC_PARAM:
            case TAC_PARAM_REF:
            case TAC_PARAM_ARR:
                /* arguments are pushed all together by TAC_CALL */
                break;

            case TAC_CALL:
            {
                int n = atoi(in->arg2 ? in->arg2 : "0");
                int fi = funcFind(ctx, in->arg1);
                int pad = n & 1;     /* rsp must be a multiple of 16 at the time of the call */
                if (fi < 0)
                {
                    fprintf(out, "    ; funzione '%s' sconosciuta\n", in->arg1);
                    break;
                }
                if (pad) fprintf(out, "    sub rsp, 8\n");
                for (int k = 0; k < n; k++)
                {
                    const TACInstr *pi = &tac->items[i - n + k];
                    const FuncParam *fp = &ctx->funcs[fi].params[k];
                    if (pi->op == TAC_PARAM_ARR)
                    {
                        array_base(out, &vt, pi->arg1, "rax");
                    }
                    else if (pi->op == TAC_PARAM_REF)
                    {
                        var_address(out, &vt, pi->arg1, fp->t1, "rax");
                    }
                    else
                    {
                        emit_load_as(ctx, out, pi->arg1, fp->t1, &vt, &st);
                        if (fp->t1 == T_REAL)
                            fprintf(out, "    movq rax, xmm0\n");
                    }
                    fprintf(out, "    push rax\n");
                }
                fprintf(out, "    call f_%s\n", in->arg1);
                if (n + pad > 0) fprintf(out, "    add rsp, %d\n", 8 * (n + pad));
                if (in->res)
                    emit_store_operand(out, in->res, in->type1, &vt);
                break;
            }

            case TAC_RETURN:
                if (fd && !fd->isVoid && in->arg1)
                    emit_load_as(ctx, out, in->arg1, fd->retType, &vt, &st);
                fprintf(out, "    leave\n");
                fprintf(out, "    ret\n");
                break;

             default:
                 fprintf(out, "    ; Istruzione TAC non gestita: %d\n", in->op);
                 break;
         }
         fprintf(out, "\n");
    }

    /* PASS 5: epilogue (exit without 'return': value 0) */
    if (fd && !fd->isVoid && fd->retType == T_REAL)
        fprintf(out, "    xorpd xmm0, xmm0\n");
    else
        fprintf(out, "    mov rax, 0\n");
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n\n");

    *st_ = st;
    for (int i = 0; i < vt.count; i++)
        free(vt.items[i].name);
    free(vt.items);
}

void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName)
{
    FILE *fp = fopen(outputFileName, "w");
    if (!fp) {
        printf("Error: unable to open file %s for writing.\n", outputFileName);
        return;
    }

    StrConstTable st;
    st.items = NULL;
    st.count = 0;
    st.capacity = 0;

    GlobalTable gt;
    memset(&gt, 0, sizeof(gt));

    /* Text is produced in a buffer: .data and .bss sections must be written first,
       ma le costanti real e le variabili globali si scoprono mentre si traducono le istruzioni. */
    char *textBuf = NULL;
    size_t textLen = 0;
    FILE *out = open_memstream(&textBuf, &textLen);
    if (!out) {
        printf("Error: out of memory.\n");
        fclose(fp);
        return;
    }

    fprintf(out, "section .text\n");

    /* main block ends with 'endfunc main' (if functions exist), followed by func/endfunc */
    int mainEnd = tac->count;
    for (int i = 0; i < tac->count; i++) {
        if (tac->items[i].op == TAC_ENDFUNC && tac->items[i].res && strcmp(tac->items[i].res, "main") == 0) {
            mainEnd = i;
            break;
        }
    }
    gen_segment(ctx, out, tac, 0, mainEnd, pScope, &st, &gt, NULL);

    for (int i = mainEnd + 1; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->op == TAC_FUNC) {
            int j = i + 1;
            while (j < tac->count && tac->items[j].op != TAC_ENDFUNC)
                j++;
            int fi = funcFind(ctx, in->res);
            if (fi >= 0)
                gen_segment(ctx, out, tac, i + 1, j, pScope, &st, &gt, &ctx->funcs[fi]);
            i = j;
        }
    }

    fprintf(out, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
    fclose(out);

    /* Intestazione e sezioni dati */
    fprintf(fp, "global main\n");
    fprintf(fp, "extern printf\n");
    fprintf(fp, "extern scanf\n");
    fprintf(fp, "extern strlen\n");
    fprintf(fp, "extern malloc\n");
    fprintf(fp, "extern strcpy\n");
    fprintf(fp, "extern strcat\n\n");

    fprintf(fp, "section .data\n");
    fprintf(fp, "    fmt_int       db \"%%d\", 10, 0\n");
    fprintf(fp, "    fmt_real      db \"%%f\", 10, 0\n");
    fprintf(fp, "    fmt_str       db \"%%s\", 0\n");
    fprintf(fp, "    fmt_scan_int  db \"%%d\", 0\n");
    fprintf(fp, "    fmt_scan_real db \"%%lf\", 0\n");
    fprintf(fp, "    str_empty     db 0\n");

    for (int i = 0; i < ctx->real_const_count; i++) {
        fprintf(fp, "    %-12s dq %f\n", ctx->real_consts[i].label, ctx->real_consts[i].value);
    }
    for (int i = 0; i < st.count; i++) {
        emit_nasm_string_def(fp, st.items[i].label, st.items[i].raw);
    }
    fprintf(fp, "\n");

    if (gt.count > 0) {
        fprintf(fp, "section .bss\n");
        fprintf(fp, "    alignb 8\n");
        for (int i = 0; i < gt.count; i++)
            fprintf(fp, "    g_%-10s resb %d\n", gt.items[i].name, gt.items[i].bytes);
        fprintf(fp, "\n");
    }

    fwrite(textBuf, 1, textLen, fp);
    free(textBuf);
    fclose(fp);

    for (int i = 0; i < st.count; i++)
    {
        free(st.items[i].label);
        free(st.items[i].raw);
    }
    free(st.items);

    for (int i = 0; i < gt.count; i++)
        free(gt.items[i].name);
    free(gt.items);

    for (int i = 0; i < ctx->real_const_count; i++)
        free(ctx->real_consts[i].label);
    free(ctx->real_consts);
    ctx->real_consts = NULL;
    ctx->real_const_count = 0;
}
