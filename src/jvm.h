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

#ifndef JVM_H
#define JVM_H


struct CompilerContext;
typedef struct CompilerContext CompilerContext;
#define CONSTANT_Class  7  
#define CONSTANT_Fieldref  9  
#define CONSTANT_Methodref  10  
#define CONSTANT_InterfaceMethodref  11  
#define CONSTANT_String  8  
#define CONSTANT_Integer  3  
#define CONSTANT_Float  4  
#define CONSTANT_Long  5  
#define CONSTANT_Double  6  
#define CONSTANT_NameAndType  12  
#define CONSTANT_Utf8  1  

#define ACC_PUBLIC  0x0001
#define ACC_PRIVATE  0x0002
#define ACC_PROTECTED  0x0004
#define ACC_STATIC  0x0008
#define ACC_FINAL  0x0010
#define ACC_SYNCHRONIZED  0x0020  
#define ACC_SUPER  0x0020
#define ACC_VOLATILE  0x0040
#define ACC_TRANSIENT  0x0080 
#define ACC_NATIVE  0x0100
#define ACC_INTERFACE  0x0200
#define ACC_ABSTRACT 0x0400 
#define ACC_STRICT  0x0800


#define jvm_nop	0
#define jvm_aconst_null	1
#define jvm_iconst_m1	2
#define jvm_iconst_0	3
#define jvm_iconst_1	4
#define jvm_iconst_2	5
#define jvm_iconst_3	6
#define jvm_iconst_4	7
#define jvm_iconst_5	8
#define jvm_lconst_0	9
#define jvm_lconst_1	10
#define jvm_fconst_0	11
#define jvm_fconst_1	12
#define jvm_fconst_2	13
#define jvm_dconst_0	14
#define jvm_dconst_1	15
#define jvm_bipush	16
#define jvm_sipush	17
#define jvm_ldc	18
#define jvm_ldc_w	19
#define jvm_ldc2_w	20
#define jvm_iload	21
#define jvm_lload	22
#define jvm_fload	23
#define jvm_dload	24
#define jvm_aload	25
#define jvm_iload_0	26
#define jvm_iload_1	27
#define jvm_iload_2	28
#define jvm_iload_3	29
#define jvm_lload_0	30
#define jvm_lload_1	31
#define jvm_lload_2	32
#define jvm_lload_3	33
#define jvm_fload_0	34
#define jvm_fload_1	35
#define jvm_fload_2	36
#define jvm_fload_3	37
#define jvm_dload_0	38
#define jvm_dload_1	39
#define jvm_dload_2	40
#define jvm_dload_3	41
#define jvm_aload_0	42
#define jvm_aload_1	43
#define jvm_aload_2	44
#define jvm_aload_3	45
#define jvm_iaload	46
#define jvm_laload	47
#define jvm_faload	48
#define jvm_daload	49
#define jvm_aaload	50
#define jvm_baload	51
#define jvm_caload	52
#define jvm_saload	53
#define jvm_istore	54
#define jvm_lstore	55
#define jvm_fstore	56
#define jvm_dstore	57
#define jvm_astore	58
#define jvm_istore_0	59
#define jvm_istore_1	60
#define jvm_istore_2	61
#define jvm_istore_3	62
#define jvm_lstore_0	63
#define jvm_lstore_1	64
#define jvm_lstore_2	65
#define jvm_lstore_3	66
#define jvm_fstore_0	67
#define jvm_fstore_1	68
#define jvm_fstore_2	69
#define jvm_fstore_3	70
#define jvm_dstore_0	71
#define jvm_dstore_1	72
#define jvm_dstore_2	73
#define jvm_dstore_3	74
#define jvm_astore_0	75
#define jvm_astore_1	76
#define jvm_astore_2	77
#define jvm_astore_3	78
#define jvm_iastore	79
#define jvm_lastore	80
#define jvm_fastore	81
#define jvm_dastore	82
#define jvm_aastore	83
#define jvm_bastore	84
#define jvm_castore	85
#define jvm_sastore	86
#define jvm_pop	87
#define jvm_pop2	88
#define jvm_dup	89
#define jvm_dup_x1	90
#define jvm_dup_x2	91
#define jvm_dup2	92
#define jvm_dup2_x1	93
#define jvm_dup2_x2	94
#define jvm_swap	95
#define jvm_iadd	96
#define jvm_ladd	97
#define jvm_fadd	98
#define jvm_dadd	99
#define jvm_isub	100
#define jvm_lsub	101
#define jvm_fsub	102
#define jvm_dsub	103
#define jvm_imul	104
#define jvm_lmul	105
#define jvm_fmul	106
#define jvm_dmul	107
#define jvm_idiv	108
#define jvm_ldiv	109
#define jvm_fdiv	110
#define jvm_ddiv	111
#define jvm_irem	112
#define jvm_lrem	113
#define jvm_frem	114
#define jvm_drem	115
#define jvm_ineg	116
#define jvm_lneg	117
#define jvm_fneg	118
#define jvm_dneg	119
#define jvm_ishl	120
#define jvm_lshl	121
#define jvm_ishr	122
#define jvm_lshr	123
#define jvm_iushr	124
#define jvm_lushr	125
#define jvm_iand	126
#define jvm_land	127
#define jvm_ior	128
#define jvm_lor	129
#define jvm_ixor	130
#define jvm_lxor	131
#define jvm_iinc	132
#define jvm_i2l	133
#define jvm_i2f	134
#define jvm_i2d	135
#define jvm_l2i	136
#define jvm_l2f	137
#define jvm_l2d	138
#define jvm_f2i	139
#define jvm_f2l	140
#define jvm_f2d	141
#define jvm_d2i	142
#define jvm_d2l	143
#define jvm_d2f	144
#define jvm_i2b	145
#define jvm_i2c	146
#define jvm_i2s	147
#define jvm_lcmp	148
#define jvm_fcmpl	149
#define jvm_fcmpg	150
#define jvm_dcmpl	151
#define jvm_dcmpg	152
#define jvm_ifeq	153
#define jvm_ifne	154
#define jvm_iflt	155
#define jvm_ifge	156
#define jvm_ifgt	157
#define jvm_ifle	158
#define jvm_if_icmpeq	159
#define jvm_if_icmpne	160
#define jvm_if_icmplt	161
#define jvm_if_icmpge	162
#define jvm_if_icmpgt	163
#define jvm_if_icmple	164
#define jvm_if_acmpeq	165
#define jvm_if_acmpne	166
#define jvm_goto	167
#define jvm_jsr	168
#define jvm_ret	169
#define jvm_tableswitch	170
#define jvm_lookupswitch	171
#define jvm_ireturn	172
#define jvm_lreturn	173
#define jvm_freturn	174
#define jvm_dreturn	175
#define jvm_areturn	176
#define jvm_return	177
#define jvm_getstatic	178
#define jvm_putstatic	179
#define jvm_getfield	180
#define jvm_putfield	181
#define jvm_invokevirtual	182
#define jvm_invokespecial	183
#define jvm_invokestatic	184
#define jvm_invokeinterface	185
#define jvm_xxxunusedxxx	186
#define jvm_new	187
#define jvm_newarray	188
#define jvm_anewarray	189
#define jvm_arraylength	190
#define jvm_athrow	191
#define jvm_checkcast	192
#define jvm_instanceof	193
#define jvm_monitorenter	194
#define jvm_monitorexit	195
#define jvm_wide	196
#define jvm_multianewarray	197
#define jvm_ifnull	198
#define jvm_ifnonnull	199
#define jvm_goto_w	200
#define jvm_jsr_w	201
#define jvm_breakpoint	202
#define jvm_impdep1	254
#define jvm_impdep2	255


/*
char *I_Set[256] =
{
"nop",
"aconst_null",
"iconst_m1",
"iconst_0",
"iconst_1",
"iconst_2",
"iconst_3",
"iconst_4",
"iconst_5",
"lconst_0",
"lconst_1",
"fconst_0",
"fconst_1",
"fconst_2",
"dconst_0",
"dconst_1",
"bipush",
"sipush",
"ldc",
"ldc_w",
"ldc2_w",
"iload",
"lload",
"fload",
"dload",
"aload",
"iload_0",
"iload_1",
"iload_2",
"iload_3",
"lload_0",
"lload_1",
"lload_2",
"lload_3",
"fload_0",
"fload_1",
"fload_2",
"fload_3",
"dload_0",
"dload_1",
"dload_2",
"dload_3",
"aload_0",
"aload_1",
"aload_2",
"aload_3",
"iaload",
"laload",
"faload",
"daload",
"aaload",
"baload",
"caload",
"saload",
"istore",
"lstore",
"fstore",
"dstore",
"astore",
"istore_0",
"istore_1",
"istore_2",
"istore_3",
"lstore_0",
"lstore_1",
"lstore_2",
"lstore_3",
"fstore_0",
"fstore_1",
"fstore_2",
"fstore_3",
"dstore_0",
"dstore_1",
"dstore_2",
"dstore_3",
"astore_0",
"astore_1",
"astore_2",
"astore_3",
"iastore",
"lastore",
"fastore",
"dastore",
"aastore",
"bastore",
"castore",
"sastore",
"pop",
"pop2",
"dup",
"dup_x1",
"dup_x2",
"dup2",
"dup2_x1",
"dup2_x2",
"swap",
"iadd",
"ladd",
"fadd",
"dadd",
"isub",
"lsub",
"fsub",
"dsub",
"imul",
"lmul",
"fmul",
"dmul",
"idiv",
"ldiv",
"fdiv",
"ddiv",
"irem",
"lrem",
"frem",
"drem",
"ineg",
"lneg",
"fneg",
"dneg",
"ishl",
"lshl",
"ishr",
"lshr",
"iushr",
"lushr",
"iand",
"land",
"ior",
"lor",
"ixor",
"lxor",
"iinc",
"i2l",
"i2f",
"i2d",
"l2i",
"l2f",
"l2d",
"f2i",
"f2l",
"f2d",
"d2i",
"d2l",
"d2f",
"i2b",
"i2c",
"i2s",
"lcmp",
"fcmpl",
"fcmpg",
"dcmpl",
"dcmpg",
"ifeq",
"ifne",
"iflt",
"ifge",
"ifgt",
"ifle",
"if_icmpeq",
"if_icmpne",
"if_icmplt",
"if_icmpge",
"if_icmpgt",
"if_icmple",
"if_acmpeq",
"if_acmpne",
"goto",
"jsr",
"ret",
"tableswitch",
"lookupswitch",
"ireturn",
"lreturn",
"freturn",
"dreturn",
"areturn",
"return",
"getstatic",
"putstatic",
"getfield",
"putfield",
"invokevirtual",
"invokespecial",
"invokestatic",
"invokeinterface",
"xxxunusedxxx",
"new",
"newarray",
"anewarray",
"arraylength",
"athrow",
"checkcast",
"instanceof",
"monitorenter",
"monitorexit",
"wide",
"multianewarray",
"ifnull",
"ifnonnull",
"goto_w",
"jsr_w",
"breakpoint",
"", "", "", "", "", "", "", "", 
"", "", "", "", "", "", "", "", 
"", "", "", "", "", "", "", "", 
"", "", "", "", "", "", "", "", 
"", "", "", "", "", "", "", "", 
"", "", "", "", "", "", "", "", 
"", "", "", 
"impdep1",
"impdep2"
};
*/

#include "symtab.h"
#include "ast.h"

#define JVM_HT_SIZE 4096

typedef struct tag_jvmHashTable
{
	char *Key;
	unsigned short c; // constant pool table index
	struct tag_jvmHashTable *next;
} jvmHashTable;

jvmHashTable* jvmhtNewNode(CompilerContext *ctx, char *Key, unsigned short c);
int jvmhtFind(CompilerContext *ctx, jvmHashTable **pHashTable, char *Key, unsigned short *c);
void jvmhtInsert(CompilerContext *ctx, jvmHashTable **pHashTable, char *Key, unsigned short c);
void jvmhtFree(CompilerContext *ctx, jvmHashTable* first);

void jvmSetNumberLocals(CompilerContext *ctx, int n);

void jvmWriteHeader(CompilerContext *ctx, FILE *fp);
int jvmMakeConstantPool(CompilerContext *ctx, char *szSourceFile, char *szClassName);
int jvmAddArrayClass(CompilerContext *ctx, int type, int sizes, int index);
int jvmAddStringConstant(CompilerContext *ctx, char *sz, int index);
int jvmAddIntegerConstant(CompilerContext *ctx, int c, int index);
int jvmAddRealConstant(CompilerContext *ctx, double c, int index);
void jvmWriteInfo(CompilerContext *ctx, FILE *fp);
void jvmWriteInitMethod(CompilerContext *ctx, FILE *fp);
void jvmWriteMainMethod(CompilerContext *ctx, FILE *fp, nodeType *pTree, jvmHashTable **pHT);
void MakeOutputFileName(CompilerContext *ctx, char *szInputFileName, char *szOutputFileName, char *szFileNameWithoutExt, char *szExt);
void jvmTypeCheck(CompilerContext *ctx, int t1, int t2);
void MakeBytecodeMain(CompilerContext *ctx, nodeType *p, jvmHashTable **pHT, Scope *pScope);
int jvmAddClassConstant(CompilerContext *ctx, const char *szClassName, int index);
void jvmStart(CompilerContext *ctx, char *szInputFileName, nodeType **ppTree, Scope *pScope, int optLevel);

#endif // JVM_H
