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

static int get_var_offset(const char *name, enumTipo1 type, VarTable *vt)
{
    if (!name) return 0;
    for (int i = 0; i < vt->count; i++) {
        if (strcmp(vt->items[i].name, name) == 0) {
            return vt->items[i].offset;
        }
    }
    if (vt->count >= vt->capacity) {
        int newCap = (vt->capacity == 0) ? 64 : vt->capacity * 2;
        vt->items = (VarSlot *)realloc(vt->items, newCap * sizeof(VarSlot));
        vt->capacity = newCap;
    }
    vt->items[vt->count].name = strdup(name);
    vt->items[vt->count].offset = vt->current_offset;
    vt->items[vt->count].type1 = type;
    vt->current_offset -= 8;
    vt->count++;
    return vt->items[vt->count - 1].offset;
}

static bool is_bool_literal(const char *str)
{
    return str && (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);
}

/* Costante numerica o booleana (true/false): non occupa uno slot nello stack frame. */
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

/* Tipo di un operando TAC: letterali dal loro aspetto, variabili e temporanei
   dal tipo con cui sono stati registrati nello stack frame. */
static enumTipo1 operand_type(const char *s, VarTable *vt, enumTipo1 dflt)
{
    if (!s) return dflt;
    if (is_string_literal(s)) return T_STRING;
    if (is_bool_literal(s)) return T_BOOLEAN;
    if (is_numeric_constant(s))
        return (strpbrk(s, ".eE") != NULL) ? T_REAL : T_INTEGER;
    for (int i = 0; i < vt->count; i++) {
        if (strcmp(vt->items[i].name, s) == 0)
            return vt->items[i].type1;
    }
    return dflt;
}

/* Registra nello stack frame una variabile o un temporaneo. Per le variabili
   dell'utente il tipo e' quello dichiarato (la symtab contiene i nomi univoci). */
static void register_operand(CompilerContext *ctx, Scope *pScope, const char *name, enumTipo1 dflt, VarTable *vt)
{
    enumTipo1 t = dflt;
    HashTable d;
    memset(&d, 0, sizeof(d));
    if (pScope && scopeFind(ctx, pScope, (char *)name, &d))
        t = d.Tipo1;
    get_var_offset(name, t, vt);
}

static void emit_load_operand(CompilerContext *ctx, FILE *out, const char *operand, enumTipo1 type, VarTable *vt, StrConstTable *st)
{
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

    int offset = get_var_offset(operand, type, vt);
    if (type == T_REAL) {
        fprintf(out, "    movsd xmm0, [rbp + %d]\n", offset);
    } else {
        fprintf(out, "    mov rax, [rbp + %d]\n", offset);
    }
}

/* Carica un operando come double in xmm<reg>, convertendolo se e' intero. */
static void emit_load_real(CompilerContext *ctx, FILE *out, const char *operand, VarTable *vt, int reg)
{
    if (is_numeric_constant(operand)) {
        const char *label = get_real_const_label(ctx, is_bool_literal(operand) ? (double)const_ivalue(operand) : strtod(operand, NULL));
        fprintf(out, "    movsd xmm%d, [rel %s]\n", reg, label);
        return;
    }
    int offset = get_var_offset(operand, T_REAL, vt);
    if (operand_type(operand, vt, T_REAL) == T_REAL) {
        fprintf(out, "    movsd xmm%d, [rbp + %d]\n", reg, offset);
    } else {
        fprintf(out, "    mov rax, [rbp + %d]\n", offset);
        fprintf(out, "    cvtsi2sd xmm%d, rax\n", reg);
    }
}

/* Carica un operando intero/booleano in un registro generale (costante inclusa). */
static void emit_load_int_reg(FILE *out, const char *operand, VarTable *vt, const char *reg)
{
    if (is_numeric_constant(operand))
        fprintf(out, "    mov %s, %lld\n", reg, const_ivalue(operand));
    else
        fprintf(out, "    mov %s, [rbp + %d]\n", reg, get_var_offset(operand, T_INTEGER, vt));
}

static void emit_store_operand(FILE *out, const char *dest, enumTipo1 type, VarTable *vt)
{
    if (!dest) return;
    int offset = get_var_offset(dest, type, vt);
    if (type == T_REAL) {
        fprintf(out, "    movsd [rbp + %d], xmm0\n", offset);
    } else {
        fprintf(out, "    mov [rbp + %d], rax\n", offset);
    }
}

void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName)
{
    FILE *fp = fopen(outputFileName, "w");
    if (!fp) {
        printf("Errore: impossibile aprire il file %s per la scrittura.\n", outputFileName);
        return;
    }

    VarTable vt;
    vt.items = NULL;
    vt.count = 0;
    vt.capacity = 0;
    vt.current_offset = -8;

    StrConstTable st;
    st.items = NULL;
    st.count = 0;
    st.capacity = 0;

    /* PASSAGGIO 1: Scansione per costanti stringa e stack frame */
    for (int i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;

        bool isCmp = (in->op >= TAC_LT && in->op <= TAC_NE);

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

        /* appoggi per la concatenazione di stringhe ('$' non e' valido negli identificatori) */
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

    /* Il testo viene prodotto in un buffer: la sezione .data va scritta dopo,
       perche' le costanti real vengono registrate mentre si traducono le istruzioni. */
    char *textBuf = NULL;
    size_t textLen = 0;
    FILE *out = open_memstream(&textBuf, &textLen);
    if (!out) {
        printf("Errore: memoria insufficiente.\n");
        fclose(fp);
        return;
    }

    /* PASSAGGIO 3: Sezione testo e prologo */
    fprintf(out, "section .text\n");
    fprintf(out, "main:\n");
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");
    fprintf(out, "    sub rsp, %d  ; Allocazione stack frame\n\n", frame_size);
    
    fprintf(out, "    ; Inizializza lo stack frame a zero\n");
    fprintf(out, "    mov rcx, %d\n", frame_size / 8);
    fprintf(out, "    xor rax, rax\n");
    fprintf(out, "    lea rdi, [rbp - %d]\n", frame_size);
    fprintf(out, "    rep stosq\n\n");

    /* PASSAGGIO 4: Traduzione istruzioni TAC */
    for (int i = 0; i < tac->count; i++)
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
                emit_load_operand(ctx, out, in->arg1, in->type1, &vt, &st);
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
                int offset = get_var_offset(in->res, in->type1, &vt);
                if (in->type1 == T_REAL)
                {
                    fprintf(out, "    mov rdi, fmt_scan_real\n");
                    fprintf(out, "    lea rsi, [rbp + %d]\n", offset);
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call scanf\n");
                }
                else
                {
                    fprintf(out, "    mov rdi, fmt_scan_int\n");
                    fprintf(out, "    lea rsi, [rbp + %d]\n", offset);
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call scanf\n");
                }
                break;
            }
            case TAC_LOAD_IDX:
            case TAC_STORE_IDX:
                fprintf(out, "    ; TODO: Implementare accesso array\n");
                break;
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
             default:
                 fprintf(out, "    ; Istruzione TAC non gestita: %d\n", in->op);
                 break;
         }
         fprintf(out, "\n");
    }
    
	// PASSAGGIO 5: Epilogo
	fprintf(out, "    mov rax, 0      ; Codice di ritorno 0\n");
	fprintf(out, "    leave\n");
	fprintf(out, "    ret\n");
	fprintf(out, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
	fclose(out);

    /* PASSAGGIO 2: Intestazione e sezione dati */
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
    
    for (int i = 0; i < ctx->real_const_count; i++) {
        fprintf(fp, "    %-12s dq %f\n", ctx->real_consts[i].label, ctx->real_consts[i].value);
    }
    for (int i = 0; i < st.count; i++) {
        emit_nasm_string_def(fp, st.items[i].label, st.items[i].raw);
    }
    fprintf(fp, "\n");

	fwrite(textBuf, 1, textLen, fp);
	free(textBuf);
	fclose(fp);
	for (int i = 0; i < vt.count; i++)
		free(vt.items[i].name);
	free(vt.items);
	
	for (int i = 0; i < st.count; i++)
	{
		free(st.items[i].label);
		free(st.items[i].raw);
	}
	free(st.items);
	
	for (int i = 0; i < ctx->real_const_count; i++)
		free(ctx->real_consts[i].label);
	free(ctx->real_consts);
	ctx->real_consts = NULL;
	ctx->real_const_count = 0;
}
