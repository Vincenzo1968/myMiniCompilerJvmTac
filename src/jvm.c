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
#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "context.h"
#include "scanner.h"
#include "endianess.h"
#include "jvm.h"
#include "tac.h"

#define ARRAY_TO_S2(arr,v){ buff = (BYTE*)&v;\
		FORMAT_WORD(arr,0);\
		v = *( (S2*)arr ); }

#define ARRAY_TO_S4(arr,v){ buff = (BYTE*)&v;\
		FORMAT_DWORD(arr,0);\
		v = *( (S4*)arr ); }

#define ARRAY_TO_S8(arr,v){ buff = (BYTE*)&v;\
		FORMAT_QWORD(arr,0);\
		v = *( (S8*)arr ); }

#define ARRAY_TO_U2(arr,v){ buff = (BYTE*)&v;\
		FORMAT_WORD(arr,0);\
		v = *( (U2*)arr ); }

#define ARRAY_TO_U4(arr,v){ buff = (BYTE*)&v;\
		FORMAT_DWORD(arr,0);\
		v = *( (U4*)arr ); }

#define ARRAY_TO_U8(arr,v){ buff = (BYTE*)&v;\
		FORMAT_QWORD(arr,0);\
		v = *( (U8*)arr ); }

#define ARRAY_TO_F4(arr,v){ buff = (BYTE*)&v;\
		FORMAT_DWORD(arr,0);\
		v = *( (F4*)arr ); }

#define ARRAY_TO_F8(arr,v){ buff = (BYTE*)&v;\
		FORMAT_QWORD(arr,0);\
		v = *( (F8*)arr ); }







static void jvmUpdateStack(CompilerContext *ctx, int delta)
{
	ctx->g_cur_stack += delta;
	if (ctx->g_cur_stack < 0)
		ctx->g_cur_stack = 0;
	if (ctx->g_cur_stack > (int)ctx->g_max_stack)
		ctx->g_max_stack = (U2)ctx->g_cur_stack;
}

static void jvmEmitU1(CompilerContext *ctx, U1 b, int stackDelta)
{
	if (ctx->g_IndexBytecodeArray >= JVM_MAX_CODE_BYTES) {
		printf("JVM Error: bytecode buffer exhausted.\n");
		return;
	}
	ctx->g_ArrayCode[ctx->g_IndexBytecodeArray++] = b;
	ctx->g_code_length++;
	if (stackDelta != 0)
		jvmUpdateStack(ctx, stackDelta);
}

static void jvmEmitU2(CompilerContext *ctx, U2 v)
{
	BYTE *buff;
	U1 fb[8];
	if (ctx->g_IndexBytecodeArray + 2 > JVM_MAX_CODE_BYTES) {
		printf("JVM Error: bytecode buffer exhausted.\n");
		return;
	}
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, v);
	}
	memcpy(ctx->g_ArrayCode + ctx->g_IndexBytecodeArray, &v, 2);
	ctx->g_IndexBytecodeArray += 2;
	ctx->g_code_length += 2;
}

static void jvmEmitS2(CompilerContext *ctx, S2 v)
{
	BYTE *buff;
	U1 fb[8];
	if (ctx->g_IndexBytecodeArray + 2 > JVM_MAX_CODE_BYTES) {
		printf("JVM Error: bytecode buffer exhausted.\n");
		return;
	}
	if (!isBigEndian(ctx)) {
		ARRAY_TO_S2(buff, v);
	}
	memcpy(ctx->g_ArrayCode + ctx->g_IndexBytecodeArray, &v, 2);
	ctx->g_IndexBytecodeArray += 2;
	ctx->g_code_length += 2;
}

// -------------------------- HashTable -----------------------------------------

jvmHashTable* jvmhtNewNode(CompilerContext *ctx, char *Key, unsigned short c)
{
	(void)ctx;
	jvmHashTable *n = (jvmHashTable*)malloc(sizeof(jvmHashTable));
	if (n == NULL)
		return NULL;

	n->Key = (char*)malloc(strlen(Key) + 1);
	if (n->Key == NULL) {
		free(n);
		return NULL;
	}
	strcpy(n->Key, Key);
	n->c = c;
	n->next = NULL;

	return n;
}

int jvmhtFind(CompilerContext *ctx, jvmHashTable **pHashTable, char *Key, unsigned short *c)
{
	(void)ctx;
	int index = 0;
	jvmHashTable *t = NULL;
	int a = 31415;
	char szTemp[1024];
	char *s = szTemp;

	strncpy(szTemp, Key, sizeof(szTemp) - 1);
	szTemp[sizeof(szTemp) - 1] = '\0';

	*c = 0;

	for (; *s != '\0'; s++) {
		if (*s == '\n')
			*s = ' ';
		index = (a * index + *s) % JVM_HT_SIZE;
	}
	if (index < 0)
		index *= -1;

	t = pHashTable[index];
	while (t != NULL) {
		if (strcmp(t->Key, Key) == 0) {
			*c = t->c;
			return 1;
		}
		t = t->next;
	}

	return 0;
}

void jvmhtInsert(CompilerContext *ctx, jvmHashTable **pHashTable, char *Key, unsigned short c)
{
	int index = 0;
	jvmHashTable *t = NULL;
	int a = 31415;
	char szTemp[1024];
	char *s = szTemp;

	strncpy(szTemp, Key, sizeof(szTemp) - 1);
	szTemp[sizeof(szTemp) - 1] = '\0';

	for (; *s != '\0'; s++) {
		if (*s == '\n')
			*s = ' ';
		index = (a * index + *s) % JVM_HT_SIZE;
	}
	if (index < 0)
		index *= -1;

	t = pHashTable[index];
	if (t == NULL) {
		pHashTable[index] = jvmhtNewNode(ctx, Key, c);
		return;
	}

	while (t != NULL) {
		if (strcmp(t->Key, Key) == 0) {
			printf("\nError: Key %s is already present in the hashtable\n", Key);
			return;
		}
		if (t->next == NULL) {
			t->next = jvmhtNewNode(ctx, Key, c);
			return;
		}
		t = t->next;
	}
}

void jvmhtFree(CompilerContext *ctx, jvmHashTable* first)
{
	(void)ctx;
	jvmHashTable *n1 = first, *n2 = NULL;
	while (n1 != NULL) {
		n2 = n1->next;
		if (n1->Key) {
			free(n1->Key);
		}
		free(n1);
		n1 = n2;
	}
}

// ------------------------------ JVM ----------------------------------------------

void jvmSetNumberLocals(CompilerContext *ctx, int n)
{
	ctx->g_max_locals = (U2)n;
}

void jvmWriteHeader(CompilerContext *ctx, FILE *fp)
{
	U4 magic = 0xCAFEBABE;
	U2 minor_version = 0;
	U2 major_version = 50;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U4(buff, magic);
		ARRAY_TO_U2(buff, minor_version);
		ARRAY_TO_U2(buff, major_version);
	}

	fwrite(&magic, sizeof(U4), 1, fp);
	fwrite(&minor_version, sizeof(U2), 1, fp);
	fwrite(&major_version, sizeof(U2), 1, fp);
}

static int jvmWriteCpUtf8(CompilerContext *ctx, int index, const char *str)
{
	U1 tag = CONSTANT_Utf8;
	U2 length = (U2)strlen(str);
	U2 lengthBigEndian = length;
	U1 fb[8];
	BYTE *buff;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(ctx->g_ArrayConstantPool + index, str, length);
	index += length;
	return index;
}

static int jvmWriteCpTwoU2(CompilerContext *ctx, int index, U1 tag, U2 v1, U2 v2)
{
	U1 fb[8];
	BYTE *buff;

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, v1);
		ARRAY_TO_U2(buff, v2);
	}
	ctx->g_ArrayConstantPool[index++] = tag;
	memcpy(ctx->g_ArrayConstantPool + index, &v1, 2);
	index += 2;
	memcpy(ctx->g_ArrayConstantPool + index, &v2, 2);
	index += 2;
	return index;
}

int jvmMakeConstantPool(CompilerContext *ctx, char *szSourceFile, char *szClassName)
{
	int k;
	int index = 0;
	U1 tag;
	U2 name_index;
	U1 fb[8];
	BYTE *buff;

	tag = CONSTANT_Class;
	for (k = 25; k < 33; k++) {
		ctx->g_ArrayConstantPool[index++] = tag;
		name_index = (U2)k;
		if (!isBigEndian(ctx)) {
			ARRAY_TO_U2(buff, name_index);
		}
		memcpy(ctx->g_ArrayConstantPool + index, &name_index, 2);
		index += 2;
	}

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Fieldref, 3, 33);

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 2, 36);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 6, 36);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 6, 39);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 6, 42);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 4, 45);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 3, 48);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 5, 51);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 7, 54);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 8, 57);

	index = jvmWriteCpUtf8(ctx, index, "Code");
	index = jvmWriteCpUtf8(ctx, index, "LineNumberTable");
	index = jvmWriteCpUtf8(ctx, index, "main");
	index = jvmWriteCpUtf8(ctx, index, "([Ljava/lang/String;)V");
	index = jvmWriteCpUtf8(ctx, index, "SourceFile");
	index = jvmWriteCpUtf8(ctx, index, szSourceFile);
	index = jvmWriteCpUtf8(ctx, index, szClassName);
	index = jvmWriteCpUtf8(ctx, index, "java/lang/Object");
	index = jvmWriteCpUtf8(ctx, index, "java/lang/System");
	index = jvmWriteCpUtf8(ctx, index, "java/io/PrintStream");
	index = jvmWriteCpUtf8(ctx, index, "java/io/Console");
	index = jvmWriteCpUtf8(ctx, index, "java/lang/StringBuilder");
	index = jvmWriteCpUtf8(ctx, index, "java/lang/Integer");
	index = jvmWriteCpUtf8(ctx, index, "java/lang/Double");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 34, 35);
	index = jvmWriteCpUtf8(ctx, index, "out");
	index = jvmWriteCpUtf8(ctx, index, "Ljava/io/PrintStream;");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 37, 38);
	index = jvmWriteCpUtf8(ctx, index, "<init>");
	index = jvmWriteCpUtf8(ctx, index, "()V");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 40, 41);
	index = jvmWriteCpUtf8(ctx, index, "append");
	index = jvmWriteCpUtf8(ctx, index, "(Ljava/lang/String;)Ljava/lang/StringBuilder;");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 43, 44);
	index = jvmWriteCpUtf8(ctx, index, "toString");
	index = jvmWriteCpUtf8(ctx, index, "()Ljava/lang/String;");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 46, 47);
	index = jvmWriteCpUtf8(ctx, index, "println");
	index = jvmWriteCpUtf8(ctx, index, "(Ljava/lang/String;)V");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 49, 50);
	index = jvmWriteCpUtf8(ctx, index, "console");
	index = jvmWriteCpUtf8(ctx, index, "()Ljava/io/Console;");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 52, 53);
	index = jvmWriteCpUtf8(ctx, index, "readLine");
	index = jvmWriteCpUtf8(ctx, index, "()Ljava/lang/String;");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 55, 56);
	index = jvmWriteCpUtf8(ctx, index, "parseInt");
	index = jvmWriteCpUtf8(ctx, index, "(Ljava/lang/String;)I");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 58, 59);
	index = jvmWriteCpUtf8(ctx, index, "parseDouble");
	index = jvmWriteCpUtf8(ctx, index, "(Ljava/lang/String;)D");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 4, 61);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 46, 62);
	index = jvmWriteCpUtf8(ctx, index, "(I)V");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 4, 64);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 46, 65);
	index = jvmWriteCpUtf8(ctx, index, "(D)V");

	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_Methodref, 4, 67);
	index = jvmWriteCpTwoU2(ctx, index, CONSTANT_NameAndType, 46, 68);
	index = jvmWriteCpUtf8(ctx, index, "(Z)V");

	return index;
}

int jvmAddArrayClass(CompilerContext *ctx, int type, int sizes, int index)
{
	char szClassName[512];
	U1 tag = CONSTANT_Class;
	U2 string_index = (U2)(ctx->g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;
	int k;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	strcpy(szClassName, "[");
	for (k = 1; k < sizes; k++) {
		strcat(szClassName, "[");
	}
	if (type == T_INTEGER)      strcat(szClassName, "I");
	else if (type == T_REAL)    strcat(szClassName, "D");
	else if (type == T_BOOLEAN) strcat(szClassName, "Z");
	else if (type == T_STRING)  strcat(szClassName, "Ljava/lang/String;");

	index = jvmWriteCpUtf8(ctx, index, szClassName);
	ctx->g_ConstantPoolCount += 2;

	return index;
}

int jvmAddClassConstant(CompilerContext *ctx, const char *szClassName, int index)
{
	U1 tag = CONSTANT_Class;
	U2 string_index = (U2)(ctx->g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	index = jvmWriteCpUtf8(ctx, index, szClassName);
	ctx->g_ConstantPoolCount += 2;

	return index;
}

int jvmAddStringConstant(CompilerContext *ctx, char *sz, int index)
{
	U1 tag = CONSTANT_String;
	U2 string_index = (U2)(ctx->g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	index = jvmWriteCpUtf8(ctx, index, sz);
	ctx->g_ConstantPoolCount += 2;

	return index;
}

int jvmAddIntegerConstant(CompilerContext *ctx, int c, int index)
{
	U1 tag = CONSTANT_Integer;
	U4 bytes = (U4)c;
	U1 fb[8];
	BYTE *buff;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U4(buff, bytes);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &bytes, 4);
	index += 4;

	ctx->g_ConstantPoolCount++;

	return index;
}

int jvmAddRealConstant(CompilerContext *ctx, double c, int index)
{
	U1 tag = CONSTANT_Double;
	F8 bytes = (F8)c;

	ctx->g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian(ctx)) {
		SwapDouble(c, &bytes);
	}
	memcpy(ctx->g_ArrayConstantPool + index, &bytes, 8);
	index += 8;

	ctx->g_ConstantPoolCount += 2;

	return index;
}

void jvmWriteInfo(CompilerContext *ctx, FILE *fp)
{
	U2 access_flags = ACC_SUPER;
	U2 this_class = 1;
	U2 super_class = 2;
	U2 interfaces_count = 0;
	U2 fields_count = 0;
	U2 methods_count = 2;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, access_flags);
		ARRAY_TO_U2(buff, this_class);
		ARRAY_TO_U2(buff, super_class);
		ARRAY_TO_U2(buff, interfaces_count);
		ARRAY_TO_U2(buff, fields_count);
		ARRAY_TO_U2(buff, methods_count);
	}

	fwrite(&access_flags, sizeof(U2), 1, fp);
	fwrite(&this_class, sizeof(U2), 1, fp);
	fwrite(&super_class, sizeof(U2), 1, fp);
	fwrite(&interfaces_count, sizeof(U2), 1, fp);
	fwrite(&fields_count, sizeof(U2), 1, fp);
	fwrite(&methods_count, sizeof(U2), 1, fp);
}

void jvmWriteInitMethod(CompilerContext *ctx, FILE *fp)
{
	U1 bytecode;
	U2 param;
	U2 access_flags = 0;
	U2 name_index = 37;
	U2 descriptor_index = 38;
	U2 attributes_count = 1;
	U2 attribute_name_index = 19;
	U4 attribute_length = 17;
	U2 max_stack = 1;
	U2 max_locals = 1;
	U4 code_length = 5;
	U2 exception_table_length = 0;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, access_flags);
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
		ARRAY_TO_U2(buff, attributes_count);
		ARRAY_TO_U2(buff, attribute_name_index);
		ARRAY_TO_U4(buff, attribute_length);
		ARRAY_TO_U2(buff, max_stack);
		ARRAY_TO_U2(buff, max_locals);
		ARRAY_TO_U4(buff, code_length);
	}

	fwrite(&access_flags, sizeof(U2), 1, fp);
	fwrite(&name_index, sizeof(U2), 1, fp);
	fwrite(&descriptor_index, sizeof(U2), 1, fp);
	fwrite(&attributes_count, sizeof(U2), 1, fp);
	fwrite(&attribute_name_index, sizeof(U2), 1, fp);
	fwrite(&attribute_length, sizeof(U4), 1, fp);
	fwrite(&max_stack, sizeof(U2), 1, fp);
	fwrite(&max_locals, sizeof(U2), 1, fp);
	fwrite(&code_length, sizeof(U4), 1, fp);

	bytecode = jvm_aload_0;
	fwrite(&bytecode, sizeof(U1), 1, fp);

	bytecode = jvm_invokespecial;
	fwrite(&bytecode, sizeof(U1), 1, fp);

	param = 10; // Method java/lang/Object."<init>":()V
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, param);
	}
	fwrite(&param, sizeof(U2), 1, fp);

	bytecode = jvm_return;
	fwrite(&bytecode, sizeof(U1), 1, fp);

	exception_table_length = 0;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void jvmWriteMainMethod(CompilerContext *ctx, FILE *fp, nodeType *pTree, jvmHashTable **pHT)
{
	(void)pTree;
	(void)pHT;

	U2 access_flags = ACC_PUBLIC | ACC_STATIC;
	U2 name_index = 21;
	U2 descriptor_index = 22;
	U2 attributes_count = 1;
	U2 attribute_name_index = 19;
	U4 attribute_length;
	U4 code_lengthBigEndian;
	U2 exception_table_length = 0;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, access_flags);
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
		ARRAY_TO_U2(buff, attributes_count);
		ARRAY_TO_U2(buff, attribute_name_index);
	}

	fwrite(&access_flags, sizeof(U2), 1, fp);
	fwrite(&name_index, sizeof(U2), 1, fp);
	fwrite(&descriptor_index, sizeof(U2), 1, fp);
	fwrite(&attributes_count, sizeof(U2), 1, fp);
	fwrite(&attribute_name_index, sizeof(U2), 1, fp);

	code_lengthBigEndian = ctx->g_code_length;
	attribute_length = 12 + ctx->g_code_length;

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U4(buff, attribute_length);
		ARRAY_TO_U2(buff, ctx->g_max_stack);
		ARRAY_TO_U2(buff, ctx->g_max_locals);
		ARRAY_TO_U4(buff, code_lengthBigEndian);
	}

	fwrite(&attribute_length, sizeof(U4), 1, fp);
	fwrite(&ctx->g_max_stack, sizeof(U2), 1, fp);
	fwrite(&ctx->g_max_locals, sizeof(U2), 1, fp);
	fwrite(&code_lengthBigEndian, sizeof(U4), 1, fp);

	fwrite(ctx->g_ArrayCode, ctx->g_code_length, 1, fp);

	exception_table_length = 0;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void MakeOutputFileName(CompilerContext *ctx, char *szInputFileName, char *szOutputFileName, char *szFileNameWithoutExt, char *szExt)
{
	(void)ctx;
	int k;
	int dotIndex;
	int len = (int)strlen(szInputFileName);

	strcpy(szOutputFileName, "");
	strcpy(szFileNameWithoutExt, "");
	strcpy(szExt, "");

	k = len - 1;
	while (k >= 0) {
		if (szInputFileName[k] == '.')
			break;
		--k;
	}
	k++;

	if (k > 0) {
		memcpy(szExt, szInputFileName + k, (size_t)(len - k));
		szExt[len - k] = '\0';
		k -= 2;
		dotIndex = k + 1;
	} else {
		dotIndex = len;
	}

	while (k >= 0) {
		if (!isalnum((unsigned char)szInputFileName[k]) && szInputFileName[k] != '_')
			break;
		--k;
	}
	k++;

	memcpy(szFileNameWithoutExt, szInputFileName + k, (size_t)(dotIndex - k));
	szFileNameWithoutExt[dotIndex - k] = '\0';

	strcpy(szOutputFileName, szFileNameWithoutExt);
	strcat(szOutputFileName, ".class");
}

/* ---------------- Bytecode emission helpers ---------------- */

static void jvmPushInt(CompilerContext *ctx, int val, jvmHashTable **pHT)
{
	if (val >= -1 && val <= 5) {
		static const U1 iconstOps[7] = {
			jvm_iconst_m1, jvm_iconst_0, jvm_iconst_1,
			jvm_iconst_2,  jvm_iconst_3, jvm_iconst_4, jvm_iconst_5
		};
		jvmEmitU1(ctx, iconstOps[val + 1], +1);
	} else if (val >= -128 && val <= 127) {
		jvmEmitU1(ctx, jvm_bipush, +1);
		jvmEmitU1(ctx, (U1)(S1)val, 0);
	} else if (val >= -32768 && val <= 32767) {
		jvmEmitU1(ctx, jvm_sipush, +1);
		jvmEmitS2(ctx, (S2)val);
	} else {
		char key[64];
		unsigned short idx = 0;
		sprintf(key, "I:%d", val);
		if (!jvmhtFind(ctx, pHT, key, &idx)) {
			idx = (unsigned short)(ctx->g_ConstantPoolCount + 1);
			jvmhtInsert(ctx, pHT, key, idx);
			ctx->g_IndexConstantPool = jvmAddIntegerConstant(ctx, val, ctx->g_IndexConstantPool);
		}
		if (idx <= 255) {
			jvmEmitU1(ctx, jvm_ldc, +1);
			jvmEmitU1(ctx, (U1)idx, 0);
		} else {
			jvmEmitU1(ctx, jvm_ldc_w, +1);
			jvmEmitU2(ctx, (U2)idx);
		}
	}
}

static void jvmPushDouble(CompilerContext *ctx, double val, jvmHashTable **pHT)
{
	if (val == 0.0) {
		jvmEmitU1(ctx, jvm_dconst_0, +2);
	} else if (val == 1.0) {
		jvmEmitU1(ctx, jvm_dconst_1, +2);
	} else {
		char key[128];
		unsigned short idx = 0;
		sprintf(key, "D:%.17g", val);
		if (!jvmhtFind(ctx, pHT, key, &idx)) {
			idx = (unsigned short)(ctx->g_ConstantPoolCount + 1);
			jvmhtInsert(ctx, pHT, key, idx);
			ctx->g_IndexConstantPool = jvmAddRealConstant(ctx, val, ctx->g_IndexConstantPool);
		}
		jvmEmitU1(ctx, jvm_ldc2_w, +2);
		jvmEmitU2(ctx, (U2)idx);
	}
}

static void jvmPushString(CompilerContext *ctx, const char *sz, jvmHashTable **pHT)
{
	size_t len = strlen(sz);
	char *key = (char *)malloc(len + 4);
	unsigned short idx = 0;

	if (!key) {
		printf("JVM Error: out of memory.\n");
		return;
	}
	sprintf(key, "S:%s", sz);
	if (!jvmhtFind(ctx, pHT, key, &idx)) {
		idx = (unsigned short)(ctx->g_ConstantPoolCount + 1);
		jvmhtInsert(ctx, pHT, key, idx);
		ctx->g_IndexConstantPool = jvmAddStringConstant(ctx, (char *)sz, ctx->g_IndexConstantPool);
	}
	free(key);

	if (idx <= 255) {
		jvmEmitU1(ctx, jvm_ldc, +1);
		jvmEmitU1(ctx, (U1)idx, 0);
	} else {
		jvmEmitU1(ctx, jvm_ldc_w, +1);
		jvmEmitU2(ctx, (U2)idx);
	}
}

static void jvmEmitLoadLocal(CompilerContext *ctx, int slot, enumTipo1 t1)
{
	if (t1 == T_INTEGER || t1 == T_BOOLEAN) {
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_iload_0 + slot), +1);
		else { jvmEmitU1(ctx, jvm_iload, +1); jvmEmitU1(ctx, (U1)slot, 0); }
	} else if (t1 == T_REAL) {
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_dload_0 + slot), +2);
		else { jvmEmitU1(ctx, jvm_dload, +2); jvmEmitU1(ctx, (U1)slot, 0); }
	} else { /* T_STRING o reference (array) */
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_aload_0 + slot), +1);
		else { jvmEmitU1(ctx, jvm_aload, +1); jvmEmitU1(ctx, (U1)slot, 0); }
	}
}

static void jvmEmitStoreLocal(CompilerContext *ctx, int slot, enumTipo1 t1)
{
	if (t1 == T_INTEGER || t1 == T_BOOLEAN) {
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_istore_0 + slot), -1);
		else { jvmEmitU1(ctx, jvm_istore, -1); jvmEmitU1(ctx, (U1)slot, 0); }
	} else if (t1 == T_REAL) {
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_dstore_0 + slot), -2);
		else { jvmEmitU1(ctx, jvm_dstore, -2); jvmEmitU1(ctx, (U1)slot, 0); }
	} else { /* T_STRING o reference (array) */
		if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_astore_0 + slot), -1);
		else { jvmEmitU1(ctx, jvm_astore, -1); jvmEmitU1(ctx, (U1)slot, 0); }
	}
}

static void jvmEmitLoadArrayRef(CompilerContext *ctx, int slot)
{
	if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_aload_0 + slot), +1);
	else { jvmEmitU1(ctx, jvm_aload, +1); jvmEmitU1(ctx, (U1)slot, 0); }
}

static void jvmEmitStoreArrayRef(CompilerContext *ctx, int slot)
{
	if (slot >= 0 && slot <= 3) jvmEmitU1(ctx, (U1)(jvm_astore_0 + slot), -1);
	else { jvmEmitU1(ctx, jvm_astore, -1); jvmEmitU1(ctx, (U1)slot, 0); }
}

void jvmTypeCheck(CompilerContext *ctx, int t1, int t2)
{
	if (t1 == t2)
		return;

	if ((t1 == T_INTEGER || t1 == T_BOOLEAN) && t2 == T_REAL) {
		jvmEmitU1(ctx, jvm_d2i, -1);
	} else if (t1 == T_REAL && (t2 == T_INTEGER || t2 == T_BOOLEAN)) {
		jvmEmitU1(ctx, jvm_i2d, +1);
	}
}

/* Alloca ogni array come array 1D linearizzato di (sizes[0] * ... * sizes[n-1]) elementi,
 * consistent with the linearized offset produced by tac.c */
void MakeBytecodeDeclArrayMain(CompilerContext *ctx, jvmHashTable **pHT, HashTable *pDati)
{
	int k;
	int totalSize = 1;
	HashTable *t = pDati;

	for (k = 0; k < t->Value.pArrayDef->countIndexes; k++) {
		totalSize *= t->Value.pArrayDef->sizes[k];
	}

	jvmPushInt(ctx, totalSize, pHT);

	if (t->Tipo1 == T_STRING) {
		unsigned short strClassIdx = 0;
		if (!jvmhtFind(ctx, pHT, "C:java/lang/String", &strClassIdx)) {
			strClassIdx = (unsigned short)(ctx->g_ConstantPoolCount + 1);
			jvmhtInsert(ctx, pHT, "C:java/lang/String", strClassIdx);
			ctx->g_IndexConstantPool = jvmAddClassConstant(ctx, "java/lang/String", ctx->g_IndexConstantPool);
		}
		jvmEmitU1(ctx, jvm_anewarray, 0); /* pop count (+0 net) -> push arrayref */
		jvmEmitU2(ctx, (U2)strClassIdx);
	} else {
		U1 atype = 10;
		if (t->Tipo1 == T_INTEGER)      atype = 10; /* T_INT */
		else if (t->Tipo1 == T_REAL)    atype = 7;  /* T_DOUBLE */
		else if (t->Tipo1 == T_BOOLEAN) atype = 4;  /* T_BOOLEAN */
		jvmEmitU1(ctx, jvm_newarray, 0); /* pop count (+0 net) -> push arrayref */
		jvmEmitU1(ctx, atype, 0);
	}

	jvmEmitStoreArrayRef(ctx, pDati->frameIndex);
}

void MakeBytecodeDeclInitMain(CompilerContext *ctx, jvmHashTable **pHT, Scope *pScope)
{
	int index;
	HashTable *t;
	HashTable **pHashTable;
	HashTable Dati = {0};

	pHashTable = pScope->stack[0];

	for (index = 0; index < HT_SIZE; index++) {
		t = pHashTable[index];
		while (t != NULL) {
			scopeFind(ctx, pScope, t->Key, &Dati);

			/* Scalar initializers ("int x = 8;") are no longer emitted here:
			   sono assegnazioni nell'AST e quindi arrivano dal TAC, al punto di
			   dichiarazione. Qui restano solo le allocazioni degli array. */
			if (t->Tipo0 == T_ARRAY) {
				MakeBytecodeDeclArrayMain(ctx, pHT, &Dati);
			}

			t = t->next;
		}
	}
}

/* ---------------- Traduzione TAC -> Bytecode JVM ---------------- */

/* ---------------- Funzioni, globali e segmenti (fase 3) ---------------- */

enum { VK_LOCAL = 0, VK_STATIC, VK_REF };

typedef struct {
	int kind;            /* VK_LOCAL: local slot; VK_STATIC: static field; VK_REF: single-element array in slot */
	int slot;
	enumTipo1 type1;     /* scalar type or element type if isArray */
	int isArray;
	const char *name;
} VarRef;

typedef struct {
	char *name;
	char *desc;
	U2 nameIdx;
	U2 descIdx;
} FieldRec;

typedef struct {
	char *name;
	char *desc;
	U2 nameIdx;
	U2 descIdx;
	U1 *code;
	int codeLen;
	U2 maxStack;
	U2 maxLocals;
} MethodRec;

typedef struct {
	FieldRec *fields;
	int nFields, capFields;
	MethodRec *methods;
	int nMethods, capMethods;
	int funcMode;        /* program contains functions: globals = static fields */
} JvmUnit;

typedef struct {
	char *name;
	int kind;
	int slot;
	enumTipo1 type1;
	int isArray;
	int isParam;
} SegVar;

typedef struct JvmSeg {
	SegVar *vars;
	int n, cap;
	int nextSlot;
	int funcMode;
	int isVoid;
	enumTipo1 retType;
	JvmUnit *unit;
	jvmHashTable **pHT;
} JvmSeg;

typedef struct {
	char *name;
	enumTipo1 type1;
	int slot;
} TempSlot;

typedef struct {
	TempSlot *items;
	int count;
	int capacity;
	JvmSeg *seg;
} TempTable;

typedef struct {
	char *name;
	int offset;
} LabelPos;

typedef struct {
	int instrPos;
	int operandPos;
	char *labelName;
} JumpFixup;

static void tempTableInit(CompilerContext *ctx, TempTable *tt)
{
	(void)ctx;
	tt->items = NULL;
	tt->count = 0;
	tt->capacity = 0;
	tt->seg = NULL;
}

static void tempTableFree(CompilerContext *ctx, TempTable *tt)
{
	(void)ctx;
	int i;
	for (i = 0; i < tt->count; i++)
		free(tt->items[i].name);
	free(tt->items);
	tt->items = NULL;
	tt->count = 0;
	tt->capacity = 0;
}

static int tempTableFind(CompilerContext *ctx, const TempTable *tt, const char *name)
{
	(void)ctx;
	int i;
	if (!name) return -1;
	for (i = 0; i < tt->count; i++) {
		if (strcmp(tt->items[i].name, name) == 0)
			return i;
	}
	return -1;
}

static int tempTableEnsure(CompilerContext *ctx, TempTable *tt, const char *name, enumTipo1 type1)
{
	int idx = tempTableFind(ctx, tt, name);
	if (idx >= 0)
		return idx;

	if (tt->count >= tt->capacity) {
		int newCap = (tt->capacity == 0) ? 32 : tt->capacity * 2;
		TempSlot *n = (TempSlot *)realloc(tt->items, (size_t)newCap * sizeof(TempSlot));
		if (!n) return -1;
		tt->items = n;
		tt->capacity = newCap;
	}
	idx = tt->count++;
	tt->items[idx].name = (char *)malloc(strlen(name) + 1);
	if (tt->items[idx].name)
		strcpy(tt->items[idx].name, name);
	tt->items[idx].type1 = type1;
	tt->items[idx].slot = (int)ctx->g_max_locals;
	ctx->g_max_locals += (type1 == T_REAL) ? 2 : 1;
	return idx;
}

/* ---- dynamic constant pool entries for fields and methods ---- */

static U2 jvmCpUtf8(CompilerContext *ctx, const char *s)
{
	ctx->g_IndexConstantPool = jvmWriteCpUtf8(ctx, ctx->g_IndexConstantPool, s);
	return (U2)(++ctx->g_ConstantPoolCount);
}

static U2 jvmCpTwoU2(CompilerContext *ctx, U1 tag, U2 v1, U2 v2)
{
	ctx->g_IndexConstantPool = jvmWriteCpTwoU2(ctx, ctx->g_IndexConstantPool, tag, v1, v2);
	return (U2)(++ctx->g_ConstantPoolCount);
}

static const char *jvmElemDesc(enumTipo1 t)
{
	switch (t) {
		case T_INTEGER: return "I";
		case T_REAL:    return "D";
		case T_BOOLEAN: return "Z";
		case T_STRING:  return "Ljava/lang/String;";
	}
	return "I";
}

static void jvmDescOf(enumTipo1 t, int isArray, char *out)
{
	out[0] = '\0';
	if (isArray) strcpy(out, "[");
	strcat(out, jvmElemDesc(t));
}

static void jvmMethodDesc(const FuncDef *f, char *out)
{
	int i;
	strcpy(out, "(");
	for (i = 0; i < f->nparams; i++) {
		char d[64];
		jvmDescOf(f->params[i].t1, (f->params[i].t0 == T_ARRAY || f->params[i].isRef), d);
		strcat(out, d);
	}
	strcat(out, ")");
	strcat(out, f->isVoid ? "V" : jvmElemDesc(f->retType));
}

static U2 jvmFieldrefIdx(CompilerContext *ctx, jvmHashTable **pHT, JvmUnit *unit,
                         const char *name, const char *desc)
{
	char key[600];
	unsigned short idx = 0;
	U2 nameIdx, descIdx, natIdx;
	FieldRec *fr;

	snprintf(key, sizeof(key), "F:%s", name);
	if (jvmhtFind(ctx, pHT, key, &idx))
		return idx;

	nameIdx = jvmCpUtf8(ctx, name);
	descIdx = jvmCpUtf8(ctx, desc);
	natIdx = jvmCpTwoU2(ctx, CONSTANT_NameAndType, nameIdx, descIdx);
	idx = jvmCpTwoU2(ctx, CONSTANT_Fieldref, 1, natIdx);
	jvmhtInsert(ctx, pHT, key, idx);

	if (unit->nFields >= unit->capFields) {
		int nc = unit->capFields ? unit->capFields * 2 : 16;
		FieldRec *nf = (FieldRec *)realloc(unit->fields, (size_t)nc * sizeof(FieldRec));
		if (!nf) return idx;
		unit->fields = nf;
		unit->capFields = nc;
	}
	fr = &unit->fields[unit->nFields++];
	fr->name = strdup(name);
	fr->desc = strdup(desc);
	fr->nameIdx = nameIdx;
	fr->descIdx = descIdx;
	return idx;
}

/* Methodref of a static method in the current class; also returns the indices
   delle Utf8 di nome e descrittore (servono per la method_info). */
static U2 jvmMethodrefIdx(CompilerContext *ctx, jvmHashTable **pHT, const char *name, const char *desc,
                          U2 *pNameIdx, U2 *pDescIdx)
{
	char key[600];
	unsigned short idx = 0, ni = 0, di = 0;
	U2 natIdx;

	snprintf(key, sizeof(key), "M:%s", name);
	if (jvmhtFind(ctx, pHT, key, &idx)) {
		snprintf(key, sizeof(key), "MN:%s", name);
		jvmhtFind(ctx, pHT, key, &ni);
		snprintf(key, sizeof(key), "MD:%s", name);
		jvmhtFind(ctx, pHT, key, &di);
	} else {
		ni = jvmCpUtf8(ctx, name);
		di = jvmCpUtf8(ctx, desc);
		natIdx = jvmCpTwoU2(ctx, CONSTANT_NameAndType, ni, di);
		idx = jvmCpTwoU2(ctx, CONSTANT_Methodref, 1, natIdx);
		snprintf(key, sizeof(key), "M:%s", name);
		jvmhtInsert(ctx, pHT, key, idx);
		snprintf(key, sizeof(key), "MN:%s", name);
		jvmhtInsert(ctx, pHT, key, ni);
		snprintf(key, sizeof(key), "MD:%s", name);
		jvmhtInsert(ctx, pHT, key, di);
	}
	if (pNameIdx) *pNameIdx = ni;
	if (pDescIdx) *pDescIdx = di;
	return idx;
}

/* ---- variable table of a segment (main or function) ---- */

static SegVar *segFind(const JvmSeg *seg, const char *name)
{
	int i;
	if (!seg || !name) return NULL;
	for (i = 0; i < seg->n; i++)
		if (strcmp(seg->vars[i].name, name) == 0)
			return &seg->vars[i];
	return NULL;
}

static SegVar *segAdd(JvmSeg *seg, const char *name, int kind, int slot, enumTipo1 t, int isArray, int isParam)
{
	SegVar *v;
	if (seg->n >= seg->cap) {
		int nc = seg->cap ? seg->cap * 2 : 16;
		SegVar *nv = (SegVar *)realloc(seg->vars, (size_t)nc * sizeof(SegVar));
		if (!nv) return NULL;
		seg->vars = nv;
		seg->cap = nc;
	}
	v = &seg->vars[seg->n++];
	v->name = strdup(name);
	v->kind = kind;
	v->slot = slot;
	v->type1 = t;
	v->isArray = isArray;
	v->isParam = isParam;
	return v;
}

static void segFree(JvmSeg *seg)
{
	int i;
	for (i = 0; i < seg->n; i++) free(seg->vars[i].name);
	free(seg->vars);
	seg->vars = NULL;
	seg->n = seg->cap = 0;
}

static int jvmVarLookup(CompilerContext *ctx, Scope *pScope, const TempTable *tt, const char *name, VarRef *vr)
{
	HashTable Dati = {0};
	const JvmSeg *seg = tt ? tt->seg : NULL;
	SegVar *sv;

	if (!name) return 0;
	if (seg && seg->funcMode && (sv = segFind(seg, name)) != NULL) {
		vr->kind = sv->kind;
		vr->slot = sv->slot;
		vr->type1 = sv->type1;
		vr->isArray = sv->isArray;
		vr->name = sv->name;
		return 1;
	}
	if (!scopeFind(ctx, pScope, (char *)name, &Dati))
		return 0;
	vr->type1 = Dati.Tipo1;
	vr->isArray = (Dati.Tipo0 == T_ARRAY);
	vr->name = name;
	if (seg && seg->funcMode && Dati.isGlobal) {
		vr->kind = VK_STATIC;
		vr->slot = -1;
	} else {
		vr->kind = VK_LOCAL;
		vr->slot = Dati.frameIndex;
	}
	return 1;
}

static void jvmEmitXaload(CompilerContext *ctx, enumTipo1 t)
{
	if (t == T_INTEGER)      jvmEmitU1(ctx, jvm_iaload, -1);
	else if (t == T_REAL)    jvmEmitU1(ctx, jvm_daload, 0);
	else if (t == T_BOOLEAN) jvmEmitU1(ctx, jvm_baload, -1);
	else                     jvmEmitU1(ctx, jvm_aaload, -1);
}

static void jvmEmitXastore(CompilerContext *ctx, enumTipo1 t)
{
	if (t == T_INTEGER)      jvmEmitU1(ctx, jvm_iastore, -3);
	else if (t == T_REAL)    jvmEmitU1(ctx, jvm_dastore, -4);
	else if (t == T_BOOLEAN) jvmEmitU1(ctx, jvm_bastore, -3);
	else                     jvmEmitU1(ctx, jvm_aastore, -3);
}

/* Loads the value of a scalar variable (local, static, or ref parameter) */
static void jvmEmitLoadVar(CompilerContext *ctx, const VarRef *vr, const TempTable *tt)
{
	if (vr->kind == VK_STATIC) {
		char d[64];
		U2 idx;
		jvmDescOf(vr->type1, 0, d);
		idx = jvmFieldrefIdx(ctx, tt->seg->pHT, tt->seg->unit, vr->name, d);
		jvmEmitU1(ctx, jvm_getstatic, (vr->type1 == T_REAL) ? +2 : +1);
		jvmEmitU2(ctx, idx);
	} else if (vr->kind == VK_REF) {
		jvmEmitLoadArrayRef(ctx, vr->slot);
		jvmEmitU1(ctx, jvm_iconst_0, +1);
		jvmEmitXaload(ctx, vr->type1);
	} else {
		jvmEmitLoadLocal(ctx, vr->slot, vr->type1);
	}
}

/* Stores the value on top of the stack (already matching the variable's type) */
static void jvmEmitStoreVar(CompilerContext *ctx, const VarRef *vr, const TempTable *tt)
{
	if (vr->kind == VK_STATIC) {
		char d[64];
		U2 idx;
		jvmDescOf(vr->type1, 0, d);
		idx = jvmFieldrefIdx(ctx, tt->seg->pHT, tt->seg->unit, vr->name, d);
		jvmEmitU1(ctx, jvm_putstatic, (vr->type1 == T_REAL) ? -2 : -1);
		jvmEmitU2(ctx, idx);
	} else if (vr->kind == VK_REF) {
		jvmEmitLoadArrayRef(ctx, vr->slot);
		if (vr->type1 == T_REAL) {
			jvmEmitU1(ctx, jvm_dup_x2, +1);
			jvmEmitU1(ctx, jvm_pop, -1);
			jvmEmitU1(ctx, jvm_iconst_0, +1);
			jvmEmitU1(ctx, jvm_dup_x2, +1);
			jvmEmitU1(ctx, jvm_pop, -1);
		} else {
			jvmEmitU1(ctx, jvm_swap, 0);
			jvmEmitU1(ctx, jvm_iconst_0, +1);
			jvmEmitU1(ctx, jvm_swap, 0);
		}
		jvmEmitXastore(ctx, vr->type1);
	} else {
		jvmEmitStoreLocal(ctx, vr->slot, vr->type1);
	}
}

/* Loads the reference to an array (local, parameter, or static) */
static void jvmEmitLoadArrayVar(CompilerContext *ctx, const VarRef *vr, const TempTable *tt)
{
	if (vr->kind == VK_STATIC) {
		char d[64];
		U2 idx;
		jvmDescOf(vr->type1, 1, d);
		idx = jvmFieldrefIdx(ctx, tt->seg->pHT, tt->seg->unit, vr->name, d);
		jvmEmitU1(ctx, jvm_getstatic, +1);
		jvmEmitU2(ctx, idx);
	} else {
		jvmEmitLoadArrayRef(ctx, vr->slot);
	}
}

/* With the element count already on the stack, allocates an array of the given type */
static void jvmEmitNewArrayOfType(CompilerContext *ctx, jvmHashTable **pHT, enumTipo1 t1)
{
	if (t1 == T_STRING) {
		unsigned short strClassIdx = 0;
		if (!jvmhtFind(ctx, pHT, "C:java/lang/String", &strClassIdx)) {
			strClassIdx = (unsigned short)(ctx->g_ConstantPoolCount + 1);
			jvmhtInsert(ctx, pHT, "C:java/lang/String", strClassIdx);
			ctx->g_IndexConstantPool = jvmAddClassConstant(ctx, "java/lang/String", ctx->g_IndexConstantPool);
		}
		jvmEmitU1(ctx, jvm_anewarray, 0);
		jvmEmitU2(ctx, (U2)strClassIdx);
	} else {
		U1 atype = 10;
		if (t1 == T_REAL) atype = 7;
		else if (t1 == T_BOOLEAN) atype = 4;
		jvmEmitU1(ctx, jvm_newarray, 0);
		jvmEmitU1(ctx, atype, 0);
	}
}

/* Allocates a linearized 1D array and leaves its reference on the stack */
static void jvmEmitNewArrayOnStack(CompilerContext *ctx, jvmHashTable **pHT, const HashTable *t)
{
	int k, totalSize = 1;
	for (k = 0; k < t->Value.pArrayDef->countIndexes; k++)
		totalSize *= t->Value.pArrayDef->sizes[k];
	jvmPushInt(ctx, totalSize, pHT);
	jvmEmitNewArrayOfType(ctx, pHT, t->Tipo1);
}

/* Initial value of a local or global variable (empty string, 0, allocated array) */
static void jvmEmitInitVar(CompilerContext *ctx, jvmHashTable **pHT, const TempTable *tt,
                           const HashTable *Dati, const VarRef *vr)
{
	if (vr->isArray) {
		jvmEmitNewArrayOnStack(ctx, pHT, Dati);
		if (vr->kind == VK_STATIC) {
			char d[64];
			U2 idx;
			jvmDescOf(vr->type1, 1, d);
			idx = jvmFieldrefIdx(ctx, pHT, tt->seg->unit, vr->name, d);
			jvmEmitU1(ctx, jvm_putstatic, -1);
			jvmEmitU2(ctx, idx);
		} else {
			jvmEmitStoreArrayRef(ctx, vr->slot);
		}
		return;
	}
	if (vr->type1 == T_STRING)      jvmPushString(ctx, "", pHT);
	else if (vr->type1 == T_REAL)   jvmPushDouble(ctx, 0.0, pHT);
	else                            jvmPushInt(ctx, 0, pHT);
	jvmEmitStoreVar(ctx, vr, tt);
}

static int isTacNumericLit(CompilerContext *ctx, const char *s, double *outDbl, long *outInt, int *isInt)
{
	(void)ctx;
	char *end = NULL;
	double d;
	if (!s || !*s) return 0;
	if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0 || s[0] == '"') return 0;
	d = strtod(s, &end);
	if (end == s || *end != '\0') return 0;
	*outDbl = d;
	*isInt = (strchr(s, '.') == NULL && strchr(s, 'e') == NULL && strchr(s, 'E') == NULL);
	if (*isInt)
		*outInt = strtol(s, NULL, 10);
	else
		*outInt = (long)d;
	return 1;
}

static int isTacBoolLit(CompilerContext *ctx, const char *s, int *outBool)
{
	(void)ctx;
	if (!s) return 0;
	if (strcmp(s, "true") == 0)  { *outBool = 1; return 1; }
	if (strcmp(s, "false") == 0) { *outBool = 0; return 1; }
	return 0;
}

static int isTacStringLit(CompilerContext *ctx, const char *s)
{
	(void)ctx;
	size_t len;
	if (!s) return 0;
	len = strlen(s);
	return (len >= 2 && s[0] == '"' && s[len - 1] == '"');
}

static int tacElemByteSize(CompilerContext *ctx, enumTipo1 t1)
{
	(void)ctx;
	switch (t1) {
		case T_INTEGER: return (int)sizeof(int);
		case T_REAL:    return (int)sizeof(double);
		case T_BOOLEAN: return (int)sizeof(char);
		case T_STRING:  return (int)sizeof(char *);
	}
	return (int)sizeof(int);
}

static enumTipo1 jvmInferOperandType(CompilerContext *ctx, const char *opStr, Scope *pScope, const TempTable *tt, enumTipo1 fallback)
{
	VarRef vr;
	double d;
	long iv;
	int isInt, b;
	int tidx;

	if (!opStr) return fallback;
	if (isTacStringLit(ctx, opStr)) return T_STRING;
	if (isTacBoolLit(ctx, opStr, &b)) return T_BOOLEAN;
	if (isTacNumericLit(ctx, opStr, &d, &iv, &isInt)) return isInt ? T_INTEGER : T_REAL;

	if (jvmVarLookup(ctx, pScope, tt, opStr, &vr))
		return vr.type1;

	tidx = tempTableFind(ctx, tt, opStr);
	if (tidx >= 0)
		return tt->items[tidx].type1;

	return fallback;
}

/* Pre-assigns type and local slot to all temporaries present in the TACList */
static void jvmPrepareTemps(CompilerContext *ctx, const TACList *tac, int from, int to, Scope *pScope, TempTable *tt)
{
	int i;
	/* Ensure g_max_locals covers all user variables */
	if (!(tt->seg && tt->seg->funcMode) && pScope->top >= 0 && pScope->stack[0]) {
		for (i = 0; i < HT_SIZE; i++) {
			HashTable *t = pScope->stack[0][i];
			while (t) {
				int need = t->frameIndex + ((t->Tipo0 == T_SIMPLE && t->Tipo1 == T_REAL) ? 2 : 1);
				if (need > (int)ctx->g_max_locals)
					ctx->g_max_locals = (U2)need;
				t = t->next;
			}
		}
	}

	for (i = from; i < to; i++) {
		const TACInstr *in = &tac->items[i];
		VarRef vrTmp;
		enumTipo1 tRes;

		if (in->dead || !in->res) continue;
		if (in->op == TAC_LABEL || in->op == TAC_GOTO ||
		    in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE ||
		    in->op == TAC_STORE_IDX)
			continue;

		if (jvmVarLookup(ctx, pScope, tt, in->res, &vrTmp))
			continue;

		if (in->op >= TAC_LT && in->op <= TAC_OR) {
			tRes = T_BOOLEAN;
		} else if (in->op == TAC_NOT) {
			tRes = T_BOOLEAN;
		} else if (in->op == TAC_NEG) {
			tRes = jvmInferOperandType(ctx, in->arg1, pScope, tt, in->type1);
		} else if (in->op == TAC_ASSIGN) {
			tRes = jvmInferOperandType(ctx, in->arg1, pScope, tt, in->type1);
		} else {
			tRes = in->type1;
		}

		tempTableEnsure(ctx, tt, in->res, tRes);
	}
}

static void jvmLoadOperand(CompilerContext *ctx, const char *opStr, enumTipo1 targetType,
                           jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	VarRef vr;
	double d;
	long iv;
	int isInt, b;
	int tidx;

	if (isTacStringLit(ctx, opStr)) {
		size_t len = strlen(opStr);
		char *raw = (char *)malloc(len - 1);
		if (raw) {
			memcpy(raw, opStr + 1, len - 2);
			raw[len - 2] = '\0';
			jvmPushString(ctx, raw, pHT);
			free(raw);
		}
		return;
	}

	if (isTacBoolLit(ctx, opStr, &b)) {
		if (targetType == T_REAL)
			jvmPushDouble(ctx, b ? 1.0 : 0.0, pHT);
		else
			jvmPushInt(ctx, b ? 1 : 0, pHT);
		return;
	}

	if (isTacNumericLit(ctx, opStr, &d, &iv, &isInt)) {
		if (targetType == T_REAL) {
			jvmPushDouble(ctx, d, pHT);
		} else if (targetType == T_BOOLEAN) {
			jvmPushInt(ctx, d != 0.0 ? 1 : 0, pHT);
		} else {
			jvmPushInt(ctx, (int)iv, pHT);
		}
		return;
	}

	if (jvmVarLookup(ctx, pScope, tt, opStr, &vr)) {
		jvmEmitLoadVar(ctx, &vr, tt);
		jvmTypeCheck(ctx, targetType, vr.type1);
		return;
	}

	tidx = tempTableFind(ctx, tt, opStr);
	if (tidx >= 0) {
		jvmEmitLoadLocal(ctx, tt->items[tidx].slot, tt->items[tidx].type1);
		jvmTypeCheck(ctx, targetType, tt->items[tidx].type1);
		return;
	}

	printf("JVM Error: operand '%s' not found.\n", opStr ? opStr : "(null)");
	if (targetType == T_REAL) jvmPushDouble(ctx, 0.0, pHT);
	else                      jvmPushInt(ctx, 0, pHT);
}

static void jvmLoadAsBool(CompilerContext *ctx, const char *opStr, jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	enumTipo1 t = jvmInferOperandType(ctx, opStr, pScope, tt, T_INTEGER);
	if (t == T_REAL) {
		jvmLoadOperand(ctx, opStr, T_REAL, pHT, pScope, tt);
		jvmEmitU1(ctx, jvm_dconst_0, +2);
		jvmEmitU1(ctx, jvm_dcmpl, -3); /* pop 4, push 1 => -3 */
	} else {
		jvmLoadOperand(ctx, opStr, T_INTEGER, pHT, pScope, tt);
	}
	jvmEmitU1(ctx, jvm_ifeq, -1);
	jvmEmitS2(ctx, 7);
	jvmEmitU1(ctx, jvm_iconst_1, +1);
	jvmEmitU1(ctx, jvm_goto, 0);
	jvmEmitS2(ctx, 4);
	jvmEmitU1(ctx, jvm_iconst_0, +1);
}

static void jvmStoreDest(CompilerContext *ctx, const char *dstName, enumTipo1 valueType, Scope *pScope, TempTable *tt)
{
	VarRef vr;
	int tidx;

	if (jvmVarLookup(ctx, pScope, tt, dstName, &vr)) {
		jvmTypeCheck(ctx, vr.type1, valueType);
		jvmEmitStoreVar(ctx, &vr, tt);
		return;
	}

	tidx = tempTableEnsure(ctx, tt, dstName, valueType);
	if (tidx >= 0) {
		jvmTypeCheck(ctx, tt->items[tidx].type1, valueType);
		jvmEmitStoreLocal(ctx, tt->items[tidx].slot, tt->items[tidx].type1);
	}
}

static void jvmEmitArrayIndexFromByteOffset(CompilerContext *ctx, const char *offStr, enumTipo1 elemType,
                                            jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	int elemSize = tacElemByteSize(ctx, elemType);
	double d;
	long iv;
	int isInt;

	if (isTacNumericLit(ctx, offStr, &d, &iv, &isInt)) {
		jvmPushInt(ctx, (int)(iv / elemSize), pHT);
		return;
	}

	jvmLoadOperand(ctx, offStr, T_INTEGER, pHT, pScope, tt);
	if (elemSize == 1) {
		return;
	} else if (elemSize == 2) {
		jvmEmitU1(ctx, jvm_iconst_1, +1);
		jvmEmitU1(ctx, jvm_ishr, -1);
	} else if (elemSize == 4) {
		jvmEmitU1(ctx, jvm_iconst_2, +1);
		jvmEmitU1(ctx, jvm_ishr, -1);
	} else if (elemSize == 8) {
		jvmEmitU1(ctx, jvm_iconst_3, +1);
		jvmEmitU1(ctx, jvm_ishr, -1);
	} else {
		jvmPushInt(ctx, elemSize, pHT);
		jvmEmitU1(ctx, jvm_idiv, -1);
	}
}

static void MakeBytecodeFromTAC(CompilerContext *ctx, const TACList *tac, int from, int to,
								jvmHashTable **pHT, Scope *pScope, JvmSeg *seg)
{
	TempTable tt;
	int curCall = -1, paramPos = 0, nBoxes = 0;
	struct { int slot; const char *name; enumTipo1 type1; } boxes[40];
	LabelPos *labels = NULL;
	int numLabels = 0, capLabels = 0;
	JumpFixup *fixups = NULL;
	int numFixups = 0, capFixups = 0;
	int i;

	tempTableInit(ctx, &tt);
	tt.seg = seg;
	jvmPrepareTemps(ctx, tac, from, to, pScope, &tt);

	for (i = from; i < to; i++) {
		const TACInstr *in = &tac->items[i];
		if (in->dead) continue;

		switch (in->op) {
		case TAC_LABEL: {
			if (numLabels >= capLabels) {
				int nc = (capLabels == 0) ? 32 : capLabels * 2;
				LabelPos *nl = (LabelPos *)realloc(labels, (size_t)nc * sizeof(LabelPos));
				if (!nl) break;
				labels = nl;
				capLabels = nc;
			}
			labels[numLabels].name = (char *)malloc(strlen(in->res) + 1);
			if (labels[numLabels].name) strcpy(labels[numLabels].name, in->res);
			labels[numLabels].offset = ctx->g_IndexBytecodeArray;
			numLabels++;
			break;
		}

		case TAC_GOTO: {
			int instrPos = ctx->g_IndexBytecodeArray;
			jvmEmitU1(ctx, jvm_goto, 0);
			if (numFixups >= capFixups) {
				int nc = (capFixups == 0) ? 32 : capFixups * 2;
				JumpFixup *nf = (JumpFixup *)realloc(fixups, (size_t)nc * sizeof(JumpFixup));
				if (nf) { fixups = nf; capFixups = nc; }
			}
			if (numFixups < capFixups) {
				fixups[numFixups].instrPos = instrPos;
				fixups[numFixups].operandPos = ctx->g_IndexBytecodeArray;
				fixups[numFixups].labelName = (char *)malloc(strlen(in->res) + 1);
				if (fixups[numFixups].labelName) strcpy(fixups[numFixups].labelName, in->res);
				numFixups++;
			}
			jvmEmitS2(ctx, 0);
			break;
		}

		case TAC_IF_FALSE:
		case TAC_IF_TRUE: {
			enumTipo1 condType = jvmInferOperandType(ctx, in->arg1, pScope, &tt, T_INTEGER);
			int instrPos;
			if (condType == T_REAL) {
				jvmLoadOperand(ctx, in->arg1, T_REAL, pHT, pScope, &tt);
				jvmEmitU1(ctx, jvm_dconst_0, +2);
				jvmEmitU1(ctx, jvm_dcmpl, -3);
			} else {
				jvmLoadOperand(ctx, in->arg1, T_INTEGER, pHT, pScope, &tt);
			}
			instrPos = ctx->g_IndexBytecodeArray;
			jvmEmitU1(ctx, (in->op == TAC_IF_FALSE) ? jvm_ifeq : jvm_ifne, -1);
			if (numFixups >= capFixups) {
				int nc = (capFixups == 0) ? 32 : capFixups * 2;
				JumpFixup *nf = (JumpFixup *)realloc(fixups, (size_t)nc * sizeof(JumpFixup));
				if (nf) { fixups = nf; capFixups = nc; }
			}
			if (numFixups < capFixups) {
				fixups[numFixups].instrPos = instrPos;
				fixups[numFixups].operandPos = ctx->g_IndexBytecodeArray;
				fixups[numFixups].labelName = (char *)malloc(strlen(in->res) + 1);
				if (fixups[numFixups].labelName) strcpy(fixups[numFixups].labelName, in->res);
				numFixups++;
			}
			jvmEmitS2(ctx, 0);
			break;
		}

		case TAC_ASSIGN: {
			VarRef vrA;
			enumTipo1 dstType = in->type1;
			int tidx;
			if (jvmVarLookup(ctx, pScope, &tt, in->res, &vrA))
				dstType = vrA.type1;
			else if ((tidx = tempTableFind(ctx, &tt, in->res)) >= 0)
				dstType = tt.items[tidx].type1;
			jvmLoadOperand(ctx, in->arg1, dstType, pHT, pScope, &tt);
			jvmStoreDest(ctx, in->res, dstType, pScope, &tt);
			break;
		}

		case TAC_NEG: {
			enumTipo1 t = jvmInferOperandType(ctx, in->arg1, pScope, &tt, in->type1);
			if (t != T_REAL) t = T_INTEGER;
			jvmLoadOperand(ctx, in->arg1, t, pHT, pScope, &tt);
			jvmEmitU1(ctx, (t == T_REAL) ? jvm_dneg : jvm_ineg, 0);
			jvmStoreDest(ctx, in->res, t, pScope, &tt);
			break;
		}

		case TAC_NOT: {
			jvmLoadAsBool(ctx, in->arg1, pHT, pScope, &tt);
			jvmEmitU1(ctx, jvm_ifne, -1);
			jvmEmitS2(ctx, 7);
			jvmEmitU1(ctx, jvm_iconst_1, +1);
			jvmEmitU1(ctx, jvm_goto, 0);
			jvmEmitS2(ctx, 4);
			jvmEmitU1(ctx, jvm_iconst_0, +1);
			jvmStoreDest(ctx, in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_ADD:
		case TAC_SUB:
		case TAC_MUL:
		case TAC_DIV: {
			if (in->op == TAC_ADD && in->type1 == T_STRING) {
				jvmEmitU1(ctx, jvm_new, +1);
				jvmEmitU2(ctx, 6); /* java/lang/StringBuilder */
				jvmEmitU1(ctx, jvm_dup, +1);
				jvmEmitU1(ctx, jvm_invokespecial, -1);
				jvmEmitU2(ctx, 11); /* StringBuilder.<init>()V */

				jvmLoadOperand(ctx, in->arg1, T_STRING, pHT, pScope, &tt);
				jvmEmitU1(ctx, jvm_invokevirtual, -1);
				jvmEmitU2(ctx, 12); /* StringBuilder.append(String) */

				jvmLoadOperand(ctx, in->arg2, T_STRING, pHT, pScope, &tt);
				jvmEmitU1(ctx, jvm_invokevirtual, -1);
				jvmEmitU2(ctx, 12); /* StringBuilder.append(String) */

				jvmEmitU1(ctx, jvm_invokevirtual, 0);
				jvmEmitU2(ctx, 13); /* StringBuilder.toString() */

				jvmStoreDest(ctx, in->res, T_STRING, pScope, &tt);
			} else {
				enumTipo1 opType = (in->type1 == T_REAL) ? T_REAL : T_INTEGER;
				U1 opByte = jvm_iadd;
				int stackDelta = (opType == T_REAL) ? -2 : -1;

				jvmLoadOperand(ctx, in->arg1, opType, pHT, pScope, &tt);
				jvmLoadOperand(ctx, in->arg2, opType, pHT, pScope, &tt);

				if (in->op == TAC_ADD)      opByte = (opType == T_REAL) ? jvm_dadd : jvm_iadd;
				else if (in->op == TAC_SUB) opByte = (opType == T_REAL) ? jvm_dsub : jvm_isub;
				else if (in->op == TAC_MUL) opByte = (opType == T_REAL) ? jvm_dmul : jvm_imul;
				else if (in->op == TAC_DIV) opByte = (opType == T_REAL) ? jvm_ddiv : jvm_idiv;

				jvmEmitU1(ctx, opByte, stackDelta);
				jvmStoreDest(ctx, in->res, opType, pScope, &tt);
			}
			break;
		}

		case TAC_LT:
		case TAC_LE:
		case TAC_GT:
		case TAC_GE:
		case TAC_EQ:
		case TAC_NE: {
			enumTipo1 t1 = jvmInferOperandType(ctx, in->arg1, pScope, &tt, in->type1);
			enumTipo1 t2 = jvmInferOperandType(ctx, in->arg2, pScope, &tt, in->type1);
			enumTipo1 cmpType = (t1 == T_REAL || t2 == T_REAL || in->type1 == T_REAL) ? T_REAL : T_INTEGER;

			jvmLoadOperand(ctx, in->arg1, cmpType, pHT, pScope, &tt);
			jvmLoadOperand(ctx, in->arg2, cmpType, pHT, pScope, &tt);

			if (cmpType == T_INTEGER) {
				U1 br = jvm_if_icmpne;
				if (in->op == TAC_LT)      br = jvm_if_icmpge;
				else if (in->op == TAC_LE) br = jvm_if_icmpgt;
				else if (in->op == TAC_GT) br = jvm_if_icmple;
				else if (in->op == TAC_GE) br = jvm_if_icmplt;
				else if (in->op == TAC_EQ) br = jvm_if_icmpne;
				else if (in->op == TAC_NE) br = jvm_if_icmpeq;
				jvmEmitU1(ctx, br, -2);
			} else {
				U1 cmpOp = (in->op == TAC_LT || in->op == TAC_LE) ? jvm_dcmpg : jvm_dcmpl;
				U1 br = jvm_ifne;
				if (in->op == TAC_LT)      br = jvm_ifge;
				else if (in->op == TAC_LE) br = jvm_ifgt;
				else if (in->op == TAC_GT) br = jvm_ifle;
				else if (in->op == TAC_GE) br = jvm_iflt;
				else if (in->op == TAC_EQ) br = jvm_ifne;
				else if (in->op == TAC_NE) br = jvm_ifeq;
				jvmEmitU1(ctx, cmpOp, -3);
				jvmEmitU1(ctx, br, -1);
			}

			jvmEmitS2(ctx, 7);
			jvmEmitU1(ctx, jvm_iconst_1, +1);
			jvmEmitU1(ctx, jvm_goto, 0);
			jvmEmitS2(ctx, 4);
			jvmEmitU1(ctx, jvm_iconst_0, +1);
			jvmStoreDest(ctx, in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_AND:
		case TAC_OR: {
			jvmLoadAsBool(ctx, in->arg1, pHT, pScope, &tt);
			jvmLoadAsBool(ctx, in->arg2, pHT, pScope, &tt);
			jvmEmitU1(ctx, (in->op == TAC_AND) ? jvm_iand : jvm_ior, -1);
			jvmStoreDest(ctx, in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_LOAD_IDX: {
			VarRef vrL;
			if (!jvmVarLookup(ctx, pScope, &tt, in->arg1, &vrL)) {
				printf("JVM Error: array '%s' not declared.\n", in->arg1);
				break;
			}
			jvmEmitLoadArrayVar(ctx, &vrL, &tt);
			jvmEmitArrayIndexFromByteOffset(ctx, in->arg2, vrL.type1, pHT, pScope, &tt);
			jvmEmitXaload(ctx, vrL.type1);
			jvmStoreDest(ctx, in->res, vrL.type1, pScope, &tt);
			break;
		}

		case TAC_STORE_IDX: {
			VarRef vrS;
			if (!jvmVarLookup(ctx, pScope, &tt, in->res, &vrS)) {
				printf("JVM Error: array '%s' not declared.\n", in->res);
				break;
			}
			jvmEmitLoadArrayVar(ctx, &vrS, &tt);
			jvmEmitArrayIndexFromByteOffset(ctx, in->arg1, vrS.type1, pHT, pScope, &tt);
			jvmLoadOperand(ctx, in->arg2, vrS.type1, pHT, pScope, &tt);
			jvmEmitXastore(ctx, vrS.type1);
			break;
		}

		case TAC_PRINT: {
			enumTipo1 pType = jvmInferOperandType(ctx, in->arg1, pScope, &tt, in->type1);
			U2 methodRef = 60;
			int argSlots = 1;

			/* Se in->type1 specifica T_BOOLEAN ma l'operando era int (es. constant folding),
			 * respects in->type1 to print true/false */
			if (in->type1 == T_BOOLEAN && pType == T_INTEGER)
				pType = T_BOOLEAN;

			jvmEmitU1(ctx, jvm_getstatic, +1);
			jvmEmitU2(ctx, 9); /* java/lang/System.out */

			jvmLoadOperand(ctx, in->arg1, pType, pHT, pScope, &tt);

			if (pType == T_STRING)       { methodRef = 14; argSlots = 1; }
			else if (pType == T_INTEGER) { methodRef = 60; argSlots = 1; }
			else if (pType == T_REAL)    { methodRef = 63; argSlots = 2; }
			else if (pType == T_BOOLEAN) { methodRef = 66; argSlots = 1; }

			jvmEmitU1(ctx, jvm_invokevirtual, -(1 + argSlots));
			jvmEmitU2(ctx, methodRef);
			break;
		}

		case TAC_READ: {
			VarRef vrR;
			enumTipo1 rType = in->type1;
			if (jvmVarLookup(ctx, pScope, &tt, in->res, &vrR))
				rType = vrR.type1;

			jvmEmitU1(ctx, jvm_invokestatic, +1);
			jvmEmitU2(ctx, 15); /* System.console() */
			jvmEmitU1(ctx, jvm_invokevirtual, 0);
			jvmEmitU2(ctx, 16); /* Console.readLine() */

			if (rType == T_INTEGER) {
				jvmEmitU1(ctx, jvm_invokestatic, 0);
				jvmEmitU2(ctx, 17); /* Integer.parseInt */
			} else if (rType == T_REAL) {
				jvmEmitU1(ctx, jvm_invokestatic, +1); /* String -> double (1 -> 2 slot) */
				jvmEmitU2(ctx, 18); /* Double.parseDouble */
			} else if (rType == T_BOOLEAN) {
				jvmEmitU1(ctx, jvm_invokestatic, 0);
				jvmEmitU2(ctx, 17); /* Integer.parseInt */
				jvmEmitU1(ctx, jvm_ifeq, -1);
				jvmEmitS2(ctx, 7);
				jvmEmitU1(ctx, jvm_iconst_1, +1);
				jvmEmitU1(ctx, jvm_goto, 0);
				jvmEmitS2(ctx, 4);
				jvmEmitU1(ctx, jvm_iconst_0, +1);
			}

			jvmStoreDest(ctx, in->res, rType, pScope, &tt);
			break;
		}
		case TAC_PARAM:
		case TAC_PARAM_REF:
		case TAC_PARAM_ARR: {
			int fi, j;
			const FuncParam *fp;
			if (curCall < 0) {
				for (j = i + 1; j < to; j++)
					if (!tac->items[j].dead && tac->items[j].op == TAC_CALL) break;
				curCall = j;
				paramPos = 0;
				nBoxes = 0;
			}
			fi = (curCall < to) ? funcFind(ctx, tac->items[curCall].arg1) : -1;
			if (fi < 0 || paramPos >= ctx->funcs[fi].nparams || paramPos >= 32) {
				printf("JVM Error: invalid function call.\n");
				break;
			}
			fp = &ctx->funcs[fi].params[paramPos++];
			if (in->op == TAC_PARAM) {
				jvmLoadOperand(ctx, in->arg1, fp->t1, pHT, pScope, &tt);
			} else {
				VarRef vrP;
				if (!jvmVarLookup(ctx, pScope, &tt, in->arg1, &vrP)) {
					printf("JVM Error: variable '%s' not declared.\n", in->arg1);
					break;
				}
				if (in->op == TAC_PARAM_ARR) {
					jvmEmitLoadArrayVar(ctx, &vrP, &tt);
				} else if (vrP.kind == VK_REF) {
					/* forwarded ref parameter: pass the same single-element array */
					jvmEmitLoadArrayRef(ctx, vrP.slot);
				} else {
					/* copy-in/copy-out: single-element box, read back after the call */
					int slot = (int)ctx->g_max_locals;
					ctx->g_max_locals = (U2)(ctx->g_max_locals + 1);
					jvmPushInt(ctx, 1, pHT);
					jvmEmitNewArrayOfType(ctx, pHT, vrP.type1);
					jvmEmitU1(ctx, jvm_dup, +1);
					jvmEmitU1(ctx, jvm_iconst_0, +1);
					jvmEmitLoadVar(ctx, &vrP, &tt);
					jvmEmitXastore(ctx, vrP.type1);
					jvmEmitU1(ctx, jvm_dup, +1);
					jvmEmitStoreArrayRef(ctx, slot);
					if (nBoxes < 40) {
						boxes[nBoxes].slot = slot;
						boxes[nBoxes].name = in->arg1;
						boxes[nBoxes].type1 = vrP.type1;
						nBoxes++;
					}
				}
			}
			break;
		}

		case TAC_CALL: {
			int fi = funcFind(ctx, in->arg1), k, argSlots = 0, retSlots;
			const FuncDef *fd;
			char desc[512];
			U2 mi;
			if (fi < 0) {
				printf("JVM Error: function '%s' not declared.\n", in->arg1);
				break;
			}
			fd = &ctx->funcs[fi];
			jvmMethodDesc(fd, desc);
			mi = jvmMethodrefIdx(ctx, pHT, fd->name, desc, NULL, NULL);
			for (k = 0; k < fd->nparams; k++)
				argSlots += (fd->params[k].t0 != T_ARRAY && !fd->params[k].isRef && fd->params[k].t1 == T_REAL) ? 2 : 1;
			retSlots = fd->isVoid ? 0 : (fd->retType == T_REAL ? 2 : 1);
			jvmEmitU1(ctx, jvm_invokestatic, retSlots - argSlots);
			jvmEmitU2(ctx, mi);
			if (in->res)
				jvmStoreDest(ctx, in->res, fd->retType, pScope, &tt);
			else if (!fd->isVoid)
				jvmEmitU1(ctx, (retSlots == 2) ? jvm_pop2 : jvm_pop, -retSlots);
			for (k = 0; k < nBoxes; k++) {
				VarRef vrB;
				if (!jvmVarLookup(ctx, pScope, &tt, boxes[k].name, &vrB)) continue;
				jvmEmitLoadArrayRef(ctx, boxes[k].slot);
				jvmEmitU1(ctx, jvm_iconst_0, +1);
				jvmEmitXaload(ctx, boxes[k].type1);
				jvmEmitStoreVar(ctx, &vrB, &tt);
			}
			curCall = -1;
			nBoxes = 0;
			break;
		}

		case TAC_RETURN: {
			if (seg->isVoid || !in->arg1) {
				jvmEmitU1(ctx, jvm_return, 0);
			} else {
				jvmLoadOperand(ctx, in->arg1, seg->retType, pHT, pScope, &tt);
				if (seg->retType == T_REAL)        jvmEmitU1(ctx, jvm_dreturn, -2);
				else if (seg->retType == T_STRING) jvmEmitU1(ctx, jvm_areturn, -1);
				else                               jvmEmitU1(ctx, jvm_ireturn, -1);
			}
			break;
		}

		default:
			break;
		}
	}

	/* Jump resolution (patching 16-bit relative offsets) */
	for (i = 0; i < numFixups; i++) {
		int j;
		int targetOffset = -1;
		for (j = 0; j < numLabels; j++) {
			if (strcmp(labels[j].name, fixups[i].labelName) == 0) {
				targetOffset = labels[j].offset;
				break;
			}
		}
		if (targetOffset < 0) {
			printf("JVM Error: label '%s' not found.\n", fixups[i].labelName);
		} else {
			S2 rel = (S2)(targetOffset - fixups[i].instrPos);
			BYTE *buff;
			U1 fb[8];
			if (!isBigEndian(ctx)) {
				ARRAY_TO_S2(buff, rel);
			}
			memcpy(ctx->g_ArrayCode + fixups[i].operandPos, &rel, 2);
		}
	}

	for (i = 0; i < numLabels; i++) free(labels[i].name);
	free(labels);
	for (i = 0; i < numFixups; i++) free(fixups[i].labelName);
	free(fixups);
	tempTableFree(ctx, &tt);
}

/* Stores the newly generated bytecode as a method of the class */
static void jvmAddMethodRec(CompilerContext *ctx, JvmUnit *unit, U2 nameIdx, U2 descIdx)
{
	MethodRec *m;
	if (unit->nMethods >= unit->capMethods) {
		int nc = unit->capMethods ? unit->capMethods * 2 : 8;
		MethodRec *nm = (MethodRec *)realloc(unit->methods, (size_t)nc * sizeof(MethodRec));
		if (!nm) return;
		unit->methods = nm;
		unit->capMethods = nc;
	}
	m = &unit->methods[unit->nMethods++];
	m->name = NULL;
	m->desc = NULL;
	m->nameIdx = nameIdx;
	m->descIdx = descIdx;
	m->codeLen = ctx->g_IndexBytecodeArray;
	m->code = (U1 *)malloc((size_t)(m->codeLen > 0 ? m->codeLen : 1));
	if (m->code) memcpy(m->code, ctx->g_ArrayCode, (size_t)m->codeLen);
	m->maxStack = ctx->g_max_stack;
	m->maxLocals = ctx->g_max_locals;
}

static void segConsider(CompilerContext *ctx, Scope *pScope, JvmSeg *seg, const char *name)
{
	HashTable D = {0};
	int isArr;
	if (!name || !*name || segFind(seg, name)) return;
	if (!scopeFind(ctx, pScope, (char *)name, &D)) return;
	if (D.isGlobal) return;
	isArr = (D.Tipo0 == T_ARRAY);
	segAdd(seg, name, VK_LOCAL, seg->nextSlot, D.Tipo1, isArr, 0);
	seg->nextSlot += (!isArr && D.Tipo1 == T_REAL) ? 2 : 1;
}

/* Generates the method for a TAC segment: the main block (fd == NULL) or a function. */
static void jvmGenSegment(CompilerContext *ctx, JvmUnit *unit, jvmHashTable **pHT, const TACList *tac,
                          int from, int to, Scope *pScope, const FuncDef *fd, U2 nameIdx, U2 descIdx)
{
	JvmSeg seg;
	int i;

	memset(&seg, 0, sizeof(seg));
	seg.funcMode = unit->funcMode;
	seg.unit = unit;
	seg.pHT = pHT;
	seg.isVoid = fd ? fd->isVoid : 1;
	seg.retType = fd ? fd->retType : T_INTEGER;

	ctx->g_IndexBytecodeArray = 0;
	ctx->g_code_length = 0;
	ctx->g_max_stack = 1;
	ctx->g_cur_stack = 0;

	if (!unit->funcMode) {
		/* programma senza funzioni: comportamento storico */
		MakeBytecodeDeclInitMain(ctx, pHT, pScope);
		MakeBytecodeFromTAC(ctx, tac, from, to, pHT, pScope, &seg);
		jvmEmitU1(ctx, jvm_return, 0);
		jvmAddMethodRec(ctx, unit, nameIdx, descIdx);
		return;
	}

	{
		TempTable tt0;
		int slot = fd ? 0 : 1; /* in main, slot 0 is args */

		if (fd) {
			for (i = 0; i < fd->nparams; i++) {
				const FuncParam *fp = &fd->params[i];
				const char *pn = fp->uniqueName ? fp->uniqueName : fp->name;
				if (fp->t0 == T_ARRAY) {
					segAdd(&seg, pn, VK_LOCAL, slot, fp->t1, 1, 1);
					slot++;
				} else if (fp->isRef) {
					segAdd(&seg, pn, VK_REF, slot, fp->t1, 0, 1);
					slot++;
				} else {
					segAdd(&seg, pn, VK_LOCAL, slot, fp->t1, 0, 1);
					slot += (fp->t1 == T_REAL) ? 2 : 1;
				}
			}
		}
		seg.nextSlot = slot;

		/* local variables referenced in the segment */
		for (i = from; i < to; i++) {
			const TACInstr *in = &tac->items[i];
			if (in->dead) continue;
			switch (in->op) {
			case TAC_LABEL: case TAC_GOTO: case TAC_CALL:
			case TAC_FUNC: case TAC_ENDFUNC:
				break;
			case TAC_IF_FALSE: case TAC_IF_TRUE:
				segConsider(ctx, pScope, &seg, in->arg1);
				break;
			default:
				segConsider(ctx, pScope, &seg, in->arg1);
				segConsider(ctx, pScope, &seg, in->arg2);
				segConsider(ctx, pScope, &seg, in->res);
				break;
			}
		}
		ctx->g_max_locals = (U2)seg.nextSlot;

		tempTableInit(ctx, &tt0);
		tt0.seg = &seg;

		/* main: initialize globals (static fields) that do not have the right default value */
		if (!fd && pScope->top >= 0 && pScope->stack[0]) {
			int b;
			for (b = 0; b < HT_SIZE; b++) {
				HashTable *t = pScope->stack[0][b];
				while (t) {
					if (t->isGlobal && (t->Tipo0 == T_ARRAY || t->Tipo1 == T_STRING)) {
						HashTable D = {0};
						if (scopeFind(ctx, pScope, t->Key, &D)) {
							VarRef vr;
							vr.kind = VK_STATIC;
							vr.slot = -1;
							vr.type1 = D.Tipo1;
							vr.isArray = (D.Tipo0 == T_ARRAY);
							vr.name = t->Key;
							jvmEmitInitVar(ctx, pHT, &tt0, &D, &vr);
						}
					}
					t = t->next;
				}
			}
		}

		/* locals: initial value (unassigned variables default to 0 / "" / allocated array) */
		for (i = 0; i < seg.n; i++) {
			HashTable D = {0};
			VarRef vr;
			if (seg.vars[i].isParam) continue;
			if (!scopeFind(ctx, pScope, seg.vars[i].name, &D)) continue;
			vr.kind = VK_LOCAL;
			vr.slot = seg.vars[i].slot;
			vr.type1 = seg.vars[i].type1;
			vr.isArray = seg.vars[i].isArray;
			vr.name = seg.vars[i].name;
			jvmEmitInitVar(ctx, pHT, &tt0, &D, &vr);
		}
		tempTableFree(ctx, &tt0);
	}

	MakeBytecodeFromTAC(ctx, tac, from, to, pHT, pScope, &seg);

	/* safety exit if the body reaches the end without a 'return' */
	if (!fd || fd->isVoid) {
		jvmEmitU1(ctx, jvm_return, 0);
	} else if (fd->retType == T_REAL) {
		jvmEmitU1(ctx, jvm_dconst_0, +2);
		jvmEmitU1(ctx, jvm_dreturn, -2);
	} else if (fd->retType == T_STRING) {
		jvmPushString(ctx, "", pHT);
		jvmEmitU1(ctx, jvm_areturn, -1);
	} else {
		jvmEmitU1(ctx, jvm_iconst_0, +1);
		jvmEmitU1(ctx, jvm_ireturn, -1);
	}

	jvmAddMethodRec(ctx, unit, nameIdx, descIdx);
	segFree(&seg);
}

static void jvmWriteU2(CompilerContext *ctx, FILE *fp, U2 v)
{
	BYTE *buff;
	U1 fb[8];
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, v);
	}
	fwrite(&v, sizeof(U2), 1, fp);
}

static void jvmWriteU4(CompilerContext *ctx, FILE *fp, U4 v)
{
	BYTE *buff;
	U1 fb[8];
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U4(buff, v);
	}
	fwrite(&v, sizeof(U4), 1, fp);
}

static void jvmWriteMethodRec(CompilerContext *ctx, FILE *fp, const MethodRec *m, U2 access)
{
	jvmWriteU2(ctx, fp, access);
	jvmWriteU2(ctx, fp, m->nameIdx);
	jvmWriteU2(ctx, fp, m->descIdx);
	jvmWriteU2(ctx, fp, 1);            /* attributes_count */
	jvmWriteU2(ctx, fp, 19);           /* "Code" */
	jvmWriteU4(ctx, fp, (U4)(12 + m->codeLen));
	jvmWriteU2(ctx, fp, m->maxStack);
	jvmWriteU2(ctx, fp, m->maxLocals);
	jvmWriteU4(ctx, fp, (U4)m->codeLen);
	fwrite(m->code, (size_t)m->codeLen, 1, fp);
	jvmWriteU2(ctx, fp, 0);            /* exception_table_length */
	jvmWriteU2(ctx, fp, 0);            /* attributes_count */
}

static void jvmWriteClassBody(CompilerContext *ctx, FILE *fp, const JvmUnit *unit)
{
	int i;
	jvmWriteU2(ctx, fp, ACC_SUPER);
	jvmWriteU2(ctx, fp, 1);            /* this_class */
	jvmWriteU2(ctx, fp, 2);            /* super_class */
	jvmWriteU2(ctx, fp, 0);            /* interfaces_count */
	jvmWriteU2(ctx, fp, (U2)unit->nFields);
	for (i = 0; i < unit->nFields; i++) {
		jvmWriteU2(ctx, fp, ACC_STATIC);
		jvmWriteU2(ctx, fp, unit->fields[i].nameIdx);
		jvmWriteU2(ctx, fp, unit->fields[i].descIdx);
		jvmWriteU2(ctx, fp, 0);
	}
	jvmWriteU2(ctx, fp, (U2)(1 + unit->nMethods)); /* <init> + main + funzioni */
	jvmWriteInitMethod(ctx, fp);
	for (i = 0; i < unit->nMethods; i++)
		jvmWriteMethodRec(ctx, fp, &unit->methods[i], (i == 0) ? (ACC_PUBLIC | ACC_STATIC) : ACC_STATIC);
}

static void jvmUnitFree(JvmUnit *unit)
{
	int i;
	for (i = 0; i < unit->nFields; i++) { free(unit->fields[i].name); free(unit->fields[i].desc); }
	free(unit->fields);
	for (i = 0; i < unit->nMethods; i++) free(unit->methods[i].code);
	free(unit->methods);
}

void MakeBytecodeMain(CompilerContext *ctx, nodeType *p, jvmHashTable **pHT, Scope *pScope)
{
	(void)ctx;
	(void)p;
	(void)pHT;
	(void)pScope;
}

void jvmStart(CompilerContext *ctx, char *szInputFileName, nodeType **ppTree, Scope *pScope, int optLevel)
{
	FILE *fp;
	U2 constant_pool_count;
	U2 attributes_count = 0;

	char szOutputFileName[512];
	char szFileNameWithoutExt[512];
	char szExt[512];
	char szSourceFile[512];
	char szClassName[512];

	BYTE *buff;
	U1 fb[8];

	jvmHashTable **pHT;
	int x;
	TACList tac;
	JvmUnit unit;

	MakeOutputFileName(ctx, szInputFileName, szOutputFileName, szFileNameWithoutExt, szExt);
	strcpy(szSourceFile, szFileNameWithoutExt);
	if (szExt[0] != '\0') {
		strcat(szSourceFile, ".");
		strcat(szSourceFile, szExt);
	}
	strcpy(szClassName, szFileNameWithoutExt);

	fp = fopen(szOutputFileName, "wb");
	if (!fp) {
		printf("Error: unable to create file %s.\n", szOutputFileName);
		return;
	}

	memset(&unit, 0, sizeof(unit));
	pHT = (jvmHashTable**)calloc(JVM_HT_SIZE, sizeof(jvmHashTable*));
	if (pHT == NULL) {
		printf("Out of memory.\n");
		fclose(fp);
		return;
	}

	/* 1) Build and optimize TAC (-O0 .. -O3) */
	tacBuild(ctx, ppTree, pScope, optLevel, &tac);

	/* 2) Initialize JVM generator state */
	ctx->g_ConstantPoolCount = 68;
	ctx->g_IndexBytecodeArray = 0;
	ctx->g_code_length = 0;
	ctx->g_max_stack = 1;
	ctx->g_cur_stack = 0;

	jvmWriteHeader(ctx, fp);
	ctx->g_IndexConstantPool = jvmMakeConstantPool(ctx, szSourceFile, szClassName);

	/* 3) One method for the main block and one for each function */
	unit.funcMode = (ctx->funcCount > 0);
	{
		int mainEnd = tac.count, k, j;
		for (k = 0; k < tac.count; k++) {
			if (tac.items[k].op == TAC_ENDFUNC && tac.items[k].res && strcmp(tac.items[k].res, "main") == 0) {
				mainEnd = k;
				break;
			}
		}
		jvmGenSegment(ctx, &unit, pHT, &tac, 0, mainEnd, pScope, NULL, 21, 22);

		for (k = mainEnd + 1; k < tac.count; k++) {
			if (tac.items[k].op == TAC_FUNC) {
				int fi = funcFind(ctx, tac.items[k].res);
				char desc[512];
				U2 ni = 0, di = 0;
				for (j = k + 1; j < tac.count && tac.items[j].op != TAC_ENDFUNC; j++) { }
				if (fi >= 0) {
					jvmMethodDesc(&ctx->funcs[fi], desc);
					jvmMethodrefIdx(ctx, pHT, ctx->funcs[fi].name, desc, &ni, &di);
					jvmGenSegment(ctx, &unit, pHT, &tac, k + 1, j, pScope, &ctx->funcs[fi], ni, di);
				}
				k = j;
			}
		}
	}

	tacFree(ctx, &tac);

	constant_pool_count = (U2)(ctx->g_ConstantPoolCount + 1);
	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, constant_pool_count);
	}
	fwrite(&constant_pool_count, sizeof(U2), 1, fp);
	fwrite(ctx->g_ArrayConstantPool, (size_t)ctx->g_IndexConstantPool, 1, fp);

	jvmWriteClassBody(ctx, fp, &unit);
	jvmUnitFree(&unit);

	if (!isBigEndian(ctx)) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);

	fclose(fp);

	for (x = 0; x < JVM_HT_SIZE; x++)
		jvmhtFree(ctx, pHT[x]);
	free(pHT);
}
