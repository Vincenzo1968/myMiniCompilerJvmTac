#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "backend_x86_64.h"

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

/* Pool semplice per costanti reali (float/double) che richiedono un indirizzo in memoria */
typedef struct {
    char *label;
    double value;
} RealConst;

static RealConst *real_consts = NULL;
static int real_const_count = 0;

static const char* get_real_const_label(double val) {
    for (int i = 0; i < real_const_count; i++) {
        if (real_consts[i].value == val) {
            return real_consts[i].label;
        }
    }
    real_consts = realloc(real_consts, (real_const_count + 1) * sizeof(RealConst));
    char label[32];
    sprintf(label, "rc_%d", real_const_count);
    real_consts[real_const_count].label = strdup(label);
    real_consts[real_const_count].value = val;
    real_const_count++;
    return real_consts[real_const_count - 1].label;
}

static int get_var_offset(const char *name, enumTipo1 type, VarTable *vt) {
    if (!name) return 0;
    for (int i = 0; i < vt->count; i++) {
        if (strcmp(vt->items[i].name, name) == 0) {
            return vt->items[i].offset;
        }
    }
    // Alloca nuovo slot
    if (vt->count >= vt->capacity) {
        int newCap = (vt->capacity == 0) ? 64 : vt->capacity * 2;
        vt->items = (VarSlot *)realloc(vt->items, newCap * sizeof(VarSlot));
        vt->capacity = newCap;
    }
    vt->items[vt->count].name = strdup(name);
    vt->items[vt->count].offset = vt->current_offset;
    vt->items[vt->count].type1 = type;
    vt->current_offset -= 8; // Allineamento a 8 byte per semplicità e sicurezza
    vt->count++;
    return vt->items[vt->count - 1].offset;
}

static bool is_numeric_constant(const char *str) {
    if (!str) return false;
    char *endptr;
    strtod(str, &endptr);
    return (*endptr == '\0');
}

static void emit_load_operand(FILE *out, const char *operand, enumTipo1 type, VarTable *vt) {
    if (!operand) return;

    if (is_numeric_constant(operand)) {
        if (type == T_REAL) {
            double val = strtod(operand, NULL);
            const char *label = get_real_const_label(val);
            fprintf(out, "    movsd xmm0, [rel %s]\n", label);
        } else {
            long long val = strtoll(operand, NULL, 10);
            fprintf(out, "    mov rax, %lld\n", val);
        }
        return;
    }

    // È una variabile o un temporaneo
    int offset = get_var_offset(operand, type, vt);
    if (type == T_REAL) {
        fprintf(out, "    movsd xmm0, [rbp + %d]\n", offset);
    } else {
        fprintf(out, "    mov rax, [rbp + %d]\n", offset);
    }
}

static void emit_store_operand(FILE *out, const char *dest, enumTipo1 type, VarTable *vt) {
    if (!dest) return;
    int offset = get_var_offset(dest, type, vt);
    if (type == T_REAL) {
        fprintf(out, "    movsd [rbp + %d], xmm0\n", offset);
    } else {
        fprintf(out, "    mov [rbp + %d], rax\n", offset);
    }
}

/* =========================================================================
 * Funzione principale di generazione
 * ========================================================================= */
void x86_64Generate(CompilerContext *ctx, TACList *tac, Scope *pScope, const char *outputFileName)
{
    (void)ctx;    // Sopprime il warning "unused parameter"
    (void)pScope; // Sopprime il warning "unused parameter"

    FILE *out = fopen(outputFileName, "w");
    if (!out) {
        printf("Errore: impossibile aprire il file %s per la scrittura.\n", outputFileName);
        return;
    }

    VarTable vt;
    vt.items = NULL;
    vt.count = 0;
    vt.capacity = 0;
    vt.current_offset = -8;

    /* PASSAGGIO 1: Scansione per calcolare la dimensione esatta dello stack frame */
    for (int i = 0; i < tac->count; i++) {
        const TACInstr *in = &tac->items[i];
        if (in->dead) continue;
        if (in->res) get_var_offset(in->res, in->type1, &vt);
        if (in->arg1) get_var_offset(in->arg1, in->type1, &vt);
        if (in->arg2) get_var_offset(in->arg2, in->type1, &vt);
    }

    int frame_size = -vt.current_offset;
    // Allineamento a 16 byte dello stack prima delle chiamate (System V ABI)
    if ((frame_size % 16) != 0) {
        frame_size = ((frame_size / 16) + 1) * 16;
    }

    /* PASSAGGIO 2: Emissione intestazione e sezione dati */
    fprintf(out, "global main\n");
    fprintf(out, "extern printf\n");
    fprintf(out, "extern scanf\n\n");
    
    fprintf(out, "section .data\n");
    fprintf(out, "    fmt_int       db \"%%d\", 10, 0\n");
    fprintf(out, "    fmt_real      db \"%%f\", 10, 0\n");
    fprintf(out, "    fmt_str       db \"%%s\", 10, 0\n");
    fprintf(out, "    fmt_scan_int  db \"%%d\", 0\n");
    fprintf(out, "    fmt_scan_real db \"%%lf\", 0\n");
    
    for (int i = 0; i < real_const_count; i++) {
        fprintf(out, "    %-12s dq %f\n", real_consts[i].label, real_consts[i].value);
    }
    fprintf(out, "\n");

    /* PASSAGGIO 3: Sezione testo e prologo */
    fprintf(out, "section .text\n");
    fprintf(out, "main:\n");
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");
    fprintf(out, "    sub rsp, %d  ; Allocazione stack frame\n\n", frame_size);
    
    // INIZIALIZZAZIONE STACK A ZERO (per evitare warning di Valgrind)
    fprintf(out, "    ; Inizializza lo stack frame a zero\n");
    fprintf(out, "    mov rcx, %d\n", frame_size / 8);
    fprintf(out, "    xor rax, rax\n");
    fprintf(out, "    lea rdi, [rbp - %d]\n", frame_size);
    fprintf(out, "    rep stosq\n\n");

    /* PASSAGGIO 4: Traduzione istruzioni TAC */
    for (int i = 0; i < tac->count; i++) {
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
                emit_load_operand(out, in->arg1, in->type1, &vt);
                if (in->type1 == T_REAL) {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    ucomisd xmm0, xmm1\n");
                    fprintf(out, "    jp %s\n", in->res);   // NaN
                    fprintf(out, "    je %s\n", in->res);   // Uguale a 0.0
                } else {
                    fprintf(out, "    test rax, rax\n");
                    fprintf(out, "    jz %s\n", in->res);
                }
                break;

            case TAC_IF_TRUE:
                emit_load_operand(out, in->arg1, in->type1, &vt);
                if (in->type1 == T_REAL) {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    ucomisd xmm0, xmm1\n");
                    fprintf(out, "    jnp %s\n", in->res);  // Non NaN
                    fprintf(out, "    jne %s\n", in->res);  // Diverso da 0.0
                } else {
                    fprintf(out, "    test rax, rax\n");
                    fprintf(out, "    jnz %s\n", in->res);
                }
                break;

            case TAC_ASSIGN:
                emit_load_operand(out, in->arg1, in->type1, &vt);
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_NEG:
                emit_load_operand(out, in->arg1, in->type1, &vt);
                if (in->type1 == T_REAL) {
                    fprintf(out, "    xorpd xmm1, xmm1\n");
                    fprintf(out, "    subsd xmm0, xmm1\n"); // 0.0 - xmm0
                } else {
                    fprintf(out, "    neg rax\n");
                }
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_NOT:
                emit_load_operand(out, in->arg1, in->type1, &vt);
                fprintf(out, "    xor rax, 1\n"); // Inverte il bit meno significativo (0 <-> 1)
                emit_store_operand(out, in->res, in->type1, &vt);
                break;

            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV: {
                if (in->type1 == T_REAL) {
                    fprintf(out, "    movsd xmm0, [rbp + %d]\n", get_var_offset(in->arg1, T_REAL, &vt));
                    fprintf(out, "    movsd xmm1, [rbp + %d]\n", get_var_offset(in->arg2, T_REAL, &vt));
                    if (in->op == TAC_ADD) fprintf(out, "    addsd xmm0, xmm1\n");
                    else if (in->op == TAC_SUB) fprintf(out, "    subsd xmm0, xmm1\n");
                    else if (in->op == TAC_MUL) fprintf(out, "    mulsd xmm0, xmm1\n");
                    else if (in->op == TAC_DIV) fprintf(out, "    divsd xmm0, xmm1\n");
                } else {
                    fprintf(out, "    mov rax, [rbp + %d]\n", get_var_offset(in->arg1, T_INTEGER, &vt));
                    fprintf(out, "    mov rbx, [rbp + %d]\n", get_var_offset(in->arg2, T_INTEGER, &vt));
                    if (in->op == TAC_ADD) fprintf(out, "    add rax, rbx\n");
                    else if (in->op == TAC_SUB) fprintf(out, "    sub rax, rbx\n");
                    else if (in->op == TAC_MUL) fprintf(out, "    imul rax, rbx\n");
                    else if (in->op == TAC_DIV) {
                        fprintf(out, "    cqo\n"); // Estende il segno di rax in rdx:rax
                        fprintf(out, "    idiv rbx\n");
                    }
                }
                emit_store_operand(out, in->res, in->type1, &vt);
                break;
            }

            case TAC_PRINT: {
                if (in->type1 == T_INTEGER || in->type1 == T_BOOLEAN) {
                    emit_load_operand(out, in->arg1, in->type1, &vt);
                    fprintf(out, "    mov rdi, fmt_int\n");
                    fprintf(out, "    mov rsi, rax\n");
                    fprintf(out, "    mov rax, 0\n"); // 0 registri vettoriali usati
                    fprintf(out, "    call printf\n");
                } else if (in->type1 == T_REAL) {
                    emit_load_operand(out, in->arg1, in->type1, &vt);
                    fprintf(out, "    mov rdi, fmt_real\n");
                    fprintf(out, "    mov rax, 1\n"); // 1 registro vettoriale (xmm0) contiene l'argomento
                    fprintf(out, "    call printf\n");
                } else if (in->type1 == T_STRING) {
                    // Gestione semplificata per stringhe: assume che arg1 sia un puntatore valido
                    emit_load_operand(out, in->arg1, in->type1, &vt);
                    fprintf(out, "    mov rdi, fmt_str\n");
                    fprintf(out, "    mov rsi, rax\n");
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call printf\n");
                }
                break;
            }

            case TAC_READ: {
                int offset = get_var_offset(in->res, in->type1, &vt);
                if (in->type1 == T_REAL) {
                    fprintf(out, "    mov rdi, fmt_scan_real\n");
                    fprintf(out, "    lea rsi, [rbp + %d]\n", offset);
                    fprintf(out, "    mov rax, 0\n"); // Passiamo un puntatore, non il valore float diretto
                    fprintf(out, "    call scanf\n");
                } else {
                    fprintf(out, "    mov rdi, fmt_scan_int\n");
                    fprintf(out, "    lea rsi, [rbp + %d]\n", offset);
                    fprintf(out, "    mov rax, 0\n");
                    fprintf(out, "    call scanf\n");
                }
                break;
            }

            case TAC_LOAD_IDX:
            case TAC_STORE_IDX:
                fprintf(out, "    ; TODO: Implementare accesso array (richiede lookup base pointer da symtab)\n");
                break;

            case TAC_LT:
            case TAC_LE:
            case TAC_GT:
            case TAC_GE:
            case TAC_EQ:
            case TAC_NE: {
                // 1. Carica arg1 in rax
                emit_load_operand(out, in->arg1, T_INTEGER, &vt);
                
                // 2. Confronta con arg2
                if (is_numeric_constant(in->arg2)) {
                    long long val2 = strtoll(in->arg2, NULL, 10);
                    fprintf(out, "    cmp rax, %lld\n", val2);
                } else {
                    fprintf(out, "    mov rbx, [rbp + %d]\n", get_var_offset(in->arg2, T_INTEGER, &vt));
                    fprintf(out, "    cmp rax, rbx\n");
                }

                // 3. Imposta rax a 0, poi usa le istruzioni 'setcc' per impostare il byte basso (al) a 0 o 1
                fprintf(out, "    mov rax, 0\n");
                if (in->op == TAC_LT)      fprintf(out, "    setl al\n");
                else if (in->op == TAC_LE) fprintf(out, "    setle al\n");
                else if (in->op == TAC_GT) fprintf(out, "    setg al\n");
                else if (in->op == TAC_GE) fprintf(out, "    setge al\n");
                else if (in->op == TAC_EQ) fprintf(out, "    sete al\n");
                else if (in->op == TAC_NE) fprintf(out, "    setne al\n");
                
                // 4. ESTENSIONE A ZERO: converte il byte basso (al) in un valore a 64 bit pulito
                fprintf(out, "    movzx rax, al\n");
                
                // 5. Salva il risultato booleano
                emit_store_operand(out, in->res, T_BOOLEAN, &vt);
                break;
            }

            case TAC_AND: {
                emit_load_operand(out, in->arg1, T_BOOLEAN, &vt);
                if (is_numeric_constant(in->arg2)) {
                    long long val2 = strtoll(in->arg2, NULL, 10);
                    fprintf(out, "    and rax, %lld\n", val2);
                } else {
                    fprintf(out, "    mov rbx, [rbp + %d]\n", get_var_offset(in->arg2, T_BOOLEAN, &vt));
                    fprintf(out, "    and rax, rbx\n");
                }
                emit_store_operand(out, in->res, T_BOOLEAN, &vt);
                break;
            }

            case TAC_OR: {
                emit_load_operand(out, in->arg1, T_BOOLEAN, &vt);
                if (is_numeric_constant(in->arg2)) {
                    long long val2 = strtoll(in->arg2, NULL, 10);
                    fprintf(out, "    or rax, %lld\n", val2);
                } else {
                    fprintf(out, "    mov rbx, [rbp + %d]\n", get_var_offset(in->arg2, T_BOOLEAN, &vt));
                    fprintf(out, "    or rax, rbx\n");
                }
                emit_store_operand(out, in->res, T_BOOLEAN, &vt);
                break;
            }

            default:
                fprintf(out, "    ; Istruzione TAC non gestita: %d\n", in->op);
                break;
        }
        fprintf(out, "\n");
    }

    /* PASSAGGIO 5: Epilogo */
    fprintf(out, "    mov rax, 0      ; Codice di ritorno 0\n");
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
    
    /* Dichiarazione per il linker: lo stack NON è eseguibile (best practice di sicurezza) */
    fprintf(out, "section .note.GNU-stack noalloc noexec nowrite progbits\n");

    fclose(out);

    // Pulizia memoria temporanea
    for (int i = 0; i < vt.count; i++) free(vt.items[i].name);
    free(vt.items);
    for (int i = 0; i < real_const_count; i++) free(real_consts[i].label);
    free(real_consts);
    real_consts = NULL;
    real_const_count = 0;
}
