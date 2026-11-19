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
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
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

#define JVM_MAX_CP_BYTES   65536
#define JVM_MAX_CODE_BYTES 65536

U2 g_ConstantPoolCount = 68;
U1 g_ArrayConstantPool[JVM_MAX_CP_BYTES];
U1 g_ArrayCode[JVM_MAX_CODE_BYTES];

int g_IndexConstantPool = 0;
int g_IndexBytecodeArray = 0;

int g_myType1 = -1;

U2 g_max_stack = 0;
U2 g_max_locals = 1;
U4 g_code_length = 0;

static int g_cur_stack = 0;

static void jvmUpdateStack(int delta)
{
	g_cur_stack += delta;
	if (g_cur_stack < 0)
		g_cur_stack = 0;
	if (g_cur_stack > (int)g_max_stack)
		g_max_stack = (U2)g_cur_stack;
}

static void jvmEmitU1(U1 b, int stackDelta)
{
	if (g_IndexBytecodeArray >= JVM_MAX_CODE_BYTES) {
		printf("Errore JVM: buffer bytecode esaurito.\n");
		return;
	}
	g_ArrayCode[g_IndexBytecodeArray++] = b;
	g_code_length++;
	if (stackDelta != 0)
		jvmUpdateStack(stackDelta);
}

static void jvmEmitU2(U2 v)
{
	BYTE *buff;
	U1 fb[8];
	if (g_IndexBytecodeArray + 2 > JVM_MAX_CODE_BYTES) {
		printf("Errore JVM: buffer bytecode esaurito.\n");
		return;
	}
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, v);
	}
	memcpy(g_ArrayCode + g_IndexBytecodeArray, &v, 2);
	g_IndexBytecodeArray += 2;
	g_code_length += 2;
}

static void jvmEmitS2(S2 v)
{
	BYTE *buff;
	U1 fb[8];
	if (g_IndexBytecodeArray + 2 > JVM_MAX_CODE_BYTES) {
		printf("Errore JVM: buffer bytecode esaurito.\n");
		return;
	}
	if (!isBigEndian()) {
		ARRAY_TO_S2(buff, v);
	}
	memcpy(g_ArrayCode + g_IndexBytecodeArray, &v, 2);
	g_IndexBytecodeArray += 2;
	g_code_length += 2;
}

// -------------------------- HashTable -----------------------------------------

jvmHashTable* jvmhtNewNode(char *Key, unsigned short c)
{
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

int jvmhtFind(jvmHashTable **pHashTable, char *Key, unsigned short *c)
{
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

void jvmhtInsert(jvmHashTable **pHashTable, char *Key, unsigned short c)
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
		pHashTable[index] = jvmhtNewNode(Key, c);
		return;
	}

	while (t != NULL) {
		if (strcmp(t->Key, Key) == 0) {
			printf("\nErrore: La chiave %s e' gia' presente nella hashtable\n", Key);
			return;
		}
		if (t->next == NULL) {
			t->next = jvmhtNewNode(Key, c);
			return;
		}
		t = t->next;
	}
}

void jvmhtFree(jvmHashTable* first)
{
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

void jvmSetNumberLocals(int n)
{
	g_max_locals = (U2)n;
}

void jvmWriteHeader(FILE *fp)
{
	U4 magic = 0xCAFEBABE;
	U2 minor_version = 0;
	U2 major_version = 50;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian()) {
		ARRAY_TO_U4(buff, magic);
		ARRAY_TO_U2(buff, minor_version);
		ARRAY_TO_U2(buff, major_version);
	}

	fwrite(&magic, sizeof(U4), 1, fp);
	fwrite(&minor_version, sizeof(U2), 1, fp);
	fwrite(&major_version, sizeof(U2), 1, fp);
}

static int jvmWriteCpUtf8(int index, const char *str)
{
	U1 tag = CONSTANT_Utf8;
	U2 length = (U2)strlen(str);
	U2 lengthBigEndian = length;
	U1 fb[8];
	BYTE *buff;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, str, length);
	index += length;
	return index;
}

static int jvmWriteCpTwoU2(int index, U1 tag, U2 v1, U2 v2)
{
	U1 fb[8];
	BYTE *buff;

	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, v1);
		ARRAY_TO_U2(buff, v2);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &v1, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &v2, 2);
	index += 2;
	return index;
}

int jvmMakeConstantPool(char *szSourceFile, char *szClassName)
{
	int k;
	int index = 0;
	U1 tag;
	U2 name_index;
	U1 fb[8];
	BYTE *buff;

	tag = CONSTANT_Class;
	for (k = 25; k < 33; k++) {
		g_ArrayConstantPool[index++] = tag;
		name_index = (U2)k;
		if (!isBigEndian()) {
			ARRAY_TO_U2(buff, name_index);
		}
		memcpy(g_ArrayConstantPool + index, &name_index, 2);
		index += 2;
	}

	index = jvmWriteCpTwoU2(index, CONSTANT_Fieldref, 3, 33);

	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 2, 36);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 6, 36);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 6, 39);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 6, 42);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 4, 45);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 3, 48);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 5, 51);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 7, 54);
	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 8, 57);

	index = jvmWriteCpUtf8(index, "Code");
	index = jvmWriteCpUtf8(index, "LineNumberTable");
	index = jvmWriteCpUtf8(index, "main");
	index = jvmWriteCpUtf8(index, "([Ljava/lang/String;)V");
	index = jvmWriteCpUtf8(index, "SourceFile");
	index = jvmWriteCpUtf8(index, szSourceFile);
	index = jvmWriteCpUtf8(index, szClassName);
	index = jvmWriteCpUtf8(index, "java/lang/Object");
	index = jvmWriteCpUtf8(index, "java/lang/System");
	index = jvmWriteCpUtf8(index, "java/io/PrintStream");
	index = jvmWriteCpUtf8(index, "java/io/Console");
	index = jvmWriteCpUtf8(index, "java/lang/StringBuilder");
	index = jvmWriteCpUtf8(index, "java/lang/Integer");
	index = jvmWriteCpUtf8(index, "java/lang/Double");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 34, 35);
	index = jvmWriteCpUtf8(index, "out");
	index = jvmWriteCpUtf8(index, "Ljava/io/PrintStream;");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 37, 38);
	index = jvmWriteCpUtf8(index, "<init>");
	index = jvmWriteCpUtf8(index, "()V");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 40, 41);
	index = jvmWriteCpUtf8(index, "append");
	index = jvmWriteCpUtf8(index, "(Ljava/lang/String;)Ljava/lang/StringBuilder;");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 43, 44);
	index = jvmWriteCpUtf8(index, "toString");
	index = jvmWriteCpUtf8(index, "()Ljava/lang/String;");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 46, 47);
	index = jvmWriteCpUtf8(index, "println");
	index = jvmWriteCpUtf8(index, "(Ljava/lang/String;)V");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 49, 50);
	index = jvmWriteCpUtf8(index, "console");
	index = jvmWriteCpUtf8(index, "()Ljava/io/Console;");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 52, 53);
	index = jvmWriteCpUtf8(index, "readLine");
	index = jvmWriteCpUtf8(index, "()Ljava/lang/String;");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 55, 56);
	index = jvmWriteCpUtf8(index, "parseInt");
	index = jvmWriteCpUtf8(index, "(Ljava/lang/String;)I");

	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 58, 59);
	index = jvmWriteCpUtf8(index, "parseDouble");
	index = jvmWriteCpUtf8(index, "(Ljava/lang/String;)D");

	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 4, 61);
	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 46, 62);
	index = jvmWriteCpUtf8(index, "(I)V");

	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 4, 64);
	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 46, 65);
	index = jvmWriteCpUtf8(index, "(D)V");

	index = jvmWriteCpTwoU2(index, CONSTANT_Methodref, 4, 67);
	index = jvmWriteCpTwoU2(index, CONSTANT_NameAndType, 46, 68);
	index = jvmWriteCpUtf8(index, "(Z)V");

	return index;
}

int jvmAddArrayClass(int type, int sizes, int index)
{
	char szClassName[512];
	U1 tag = CONSTANT_Class;
	U2 string_index = (U2)(g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;
	int k;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	strcpy(szClassName, "[");
	for (k = 1; k < sizes; k++) {
		strcat(szClassName, "[");
	}
	if (type == T_INTEGER)      strcat(szClassName, "I");
	else if (type == T_REAL)    strcat(szClassName, "D");
	else if (type == T_BOOLEAN) strcat(szClassName, "Z");
	else if (type == T_STRING)  strcat(szClassName, "Ljava/lang/String;");

	index = jvmWriteCpUtf8(index, szClassName);
	g_ConstantPoolCount += 2;

	return index;
}

int jvmAddClassConstant(const char *szClassName, int index)
{
	U1 tag = CONSTANT_Class;
	U2 string_index = (U2)(g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	index = jvmWriteCpUtf8(index, szClassName);
	g_ConstantPoolCount += 2;

	return index;
}

int jvmAddStringConstant(char *sz, int index)
{
	U1 tag = CONSTANT_String;
	U2 string_index = (U2)(g_ConstantPoolCount + 2);
	U1 fb[8];
	BYTE *buff;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	index = jvmWriteCpUtf8(index, sz);
	g_ConstantPoolCount += 2;

	return index;
}

int jvmAddIntegerConstant(int c, int index)
{
	U1 tag = CONSTANT_Integer;
	U4 bytes = (U4)c;
	U1 fb[8];
	BYTE *buff;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		ARRAY_TO_U4(buff, bytes);
	}
	memcpy(g_ArrayConstantPool + index, &bytes, 4);
	index += 4;

	g_ConstantPoolCount++;

	return index;
}

int jvmAddRealConstant(double c, int index)
{
	U1 tag = CONSTANT_Double;
	F8 bytes = (F8)c;

	g_ArrayConstantPool[index++] = tag;
	if (!isBigEndian()) {
		SwapDouble(c, &bytes);
	}
	memcpy(g_ArrayConstantPool + index, &bytes, 8);
	index += 8;

	g_ConstantPoolCount += 2;

	return index;
}

void jvmWriteInfo(FILE *fp)
{
	U2 access_flags = ACC_SUPER;
	U2 this_class = 1;
	U2 super_class = 2;
	U2 interfaces_count = 0;
	U2 fields_count = 0;
	U2 methods_count = 2;

	BYTE *buff;
	U1 fb[8];

	if (!isBigEndian()) {
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

void jvmWriteInitMethod(FILE *fp)
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

	if (!isBigEndian()) {
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
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, param);
	}
	fwrite(&param, sizeof(U2), 1, fp);

	bytecode = jvm_return;
	fwrite(&bytecode, sizeof(U1), 1, fp);

	exception_table_length = 0;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void jvmWriteMainMethod(FILE *fp, nodeType *pTree, jvmHashTable **pHT)
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

	if (!isBigEndian()) {
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

	code_lengthBigEndian = g_code_length;
	attribute_length = 12 + g_code_length;

	if (!isBigEndian()) {
		ARRAY_TO_U4(buff, attribute_length);
		ARRAY_TO_U2(buff, g_max_stack);
		ARRAY_TO_U2(buff, g_max_locals);
		ARRAY_TO_U4(buff, code_lengthBigEndian);
	}

	fwrite(&attribute_length, sizeof(U4), 1, fp);
	fwrite(&g_max_stack, sizeof(U2), 1, fp);
	fwrite(&g_max_locals, sizeof(U2), 1, fp);
	fwrite(&code_lengthBigEndian, sizeof(U4), 1, fp);

	fwrite(g_ArrayCode, g_code_length, 1, fp);

	exception_table_length = 0;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void MakeOutputFileName(char *szInputFileName, char *szOutputFileName, char *szFileNameWithoutExt, char *szExt)
{
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

/* ---------------- Helper per emissione bytecode ---------------- */

static void jvmPushInt(int val, jvmHashTable **pHT)
{
	if (val >= -1 && val <= 5) {
		static const U1 iconstOps[7] = {
			jvm_iconst_m1, jvm_iconst_0, jvm_iconst_1,
			jvm_iconst_2,  jvm_iconst_3, jvm_iconst_4, jvm_iconst_5
		};
		jvmEmitU1(iconstOps[val + 1], +1);
	} else if (val >= -128 && val <= 127) {
		jvmEmitU1(jvm_bipush, +1);
		jvmEmitU1((U1)(S1)val, 0);
	} else if (val >= -32768 && val <= 32767) {
		jvmEmitU1(jvm_sipush, +1);
		jvmEmitS2((S2)val);
	} else {
		char key[64];
		unsigned short idx = 0;
		sprintf(key, "I:%d", val);
		if (!jvmhtFind(pHT, key, &idx)) {
			idx = (unsigned short)(g_ConstantPoolCount + 1);
			jvmhtInsert(pHT, key, idx);
			g_IndexConstantPool = jvmAddIntegerConstant(val, g_IndexConstantPool);
		}
		if (idx <= 255) {
			jvmEmitU1(jvm_ldc, +1);
			jvmEmitU1((U1)idx, 0);
		} else {
			jvmEmitU1(jvm_ldc_w, +1);
			jvmEmitU2((U2)idx);
		}
	}
}

static void jvmPushDouble(double val, jvmHashTable **pHT)
{
	if (val == 0.0) {
		jvmEmitU1(jvm_dconst_0, +2);
	} else if (val == 1.0) {
		jvmEmitU1(jvm_dconst_1, +2);
	} else {
		char key[128];
		unsigned short idx = 0;
		sprintf(key, "D:%.17g", val);
		if (!jvmhtFind(pHT, key, &idx)) {
			idx = (unsigned short)(g_ConstantPoolCount + 1);
			jvmhtInsert(pHT, key, idx);
			g_IndexConstantPool = jvmAddRealConstant(val, g_IndexConstantPool);
		}
		jvmEmitU1(jvm_ldc2_w, +2);
		jvmEmitU2((U2)idx);
	}
}

static void jvmPushString(const char *sz, jvmHashTable **pHT)
{
	size_t len = strlen(sz);
	char *key = (char *)malloc(len + 4);
	unsigned short idx = 0;

	if (!key) {
		printf("Errore JVM: memoria insufficiente.\n");
		return;
	}
	sprintf(key, "S:%s", sz);
	if (!jvmhtFind(pHT, key, &idx)) {
		idx = (unsigned short)(g_ConstantPoolCount + 1);
		jvmhtInsert(pHT, key, idx);
		g_IndexConstantPool = jvmAddStringConstant((char *)sz, g_IndexConstantPool);
	}
	free(key);

	if (idx <= 255) {
		jvmEmitU1(jvm_ldc, +1);
		jvmEmitU1((U1)idx, 0);
	} else {
		jvmEmitU1(jvm_ldc_w, +1);
		jvmEmitU2((U2)idx);
	}
}

static void jvmEmitLoadLocal(int slot, enumTipo1 t1)
{
	if (t1 == T_INTEGER || t1 == T_BOOLEAN) {
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_iload_0 + slot), +1);
		else { jvmEmitU1(jvm_iload, +1); jvmEmitU1((U1)slot, 0); }
	} else if (t1 == T_REAL) {
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_dload_0 + slot), +2);
		else { jvmEmitU1(jvm_dload, +2); jvmEmitU1((U1)slot, 0); }
	} else { /* T_STRING o reference (array) */
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_aload_0 + slot), +1);
		else { jvmEmitU1(jvm_aload, +1); jvmEmitU1((U1)slot, 0); }
	}
}

static void jvmEmitStoreLocal(int slot, enumTipo1 t1)
{
	if (t1 == T_INTEGER || t1 == T_BOOLEAN) {
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_istore_0 + slot), -1);
		else { jvmEmitU1(jvm_istore, -1); jvmEmitU1((U1)slot, 0); }
	} else if (t1 == T_REAL) {
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_dstore_0 + slot), -2);
		else { jvmEmitU1(jvm_dstore, -2); jvmEmitU1((U1)slot, 0); }
	} else { /* T_STRING o reference (array) */
		if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_astore_0 + slot), -1);
		else { jvmEmitU1(jvm_astore, -1); jvmEmitU1((U1)slot, 0); }
	}
}

static void jvmEmitLoadArrayRef(int slot)
{
	if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_aload_0 + slot), +1);
	else { jvmEmitU1(jvm_aload, +1); jvmEmitU1((U1)slot, 0); }
}

static void jvmEmitStoreArrayRef(int slot)
{
	if (slot >= 0 && slot <= 3) jvmEmitU1((U1)(jvm_astore_0 + slot), -1);
	else { jvmEmitU1(jvm_astore, -1); jvmEmitU1((U1)slot, 0); }
}

void jvmTypeCheck(int t1, int t2)
{
	if (t1 == t2)
		return;

	if ((t1 == T_INTEGER || t1 == T_BOOLEAN) && t2 == T_REAL) {
		jvmEmitU1(jvm_d2i, -1);
	} else if (t1 == T_REAL && (t2 == T_INTEGER || t2 == T_BOOLEAN)) {
		jvmEmitU1(jvm_i2d, +1);
	}
}

/* Alloca ogni array come array 1D linearizzato di (sizes[0] * ... * sizes[n-1]) elementi,
 * coerente con l'offset linearizzato prodotto da tac.c */
void MakeBytecodeDeclArrayMain(jvmHashTable **pHT, HashTable *pDati)
{
	int k;
	int totalSize = 1;
	HashTable *t = pDati;

	for (k = 0; k < t->Value.pArrayDef->countIndexes; k++) {
		totalSize *= t->Value.pArrayDef->sizes[k];
	}

	jvmPushInt(totalSize, pHT);

	if (t->Tipo1 == T_STRING) {
		unsigned short strClassIdx = 0;
		if (!jvmhtFind(pHT, "C:java/lang/String", &strClassIdx)) {
			strClassIdx = (unsigned short)(g_ConstantPoolCount + 1);
			jvmhtInsert(pHT, "C:java/lang/String", strClassIdx);
			g_IndexConstantPool = jvmAddClassConstant("java/lang/String", g_IndexConstantPool);
		}
		jvmEmitU1(jvm_anewarray, 0); /* pop count (+0 net) -> push arrayref */
		jvmEmitU2((U2)strClassIdx);
	} else {
		U1 atype = 10;
		if (t->Tipo1 == T_INTEGER)      atype = 10; /* T_INT */
		else if (t->Tipo1 == T_REAL)    atype = 7;  /* T_DOUBLE */
		else if (t->Tipo1 == T_BOOLEAN) atype = 4;  /* T_BOOLEAN */
		jvmEmitU1(jvm_newarray, 0); /* pop count (+0 net) -> push arrayref */
		jvmEmitU1(atype, 0);
	}

	jvmEmitStoreArrayRef(pDati->frameIndex);
}

void MakeBytecodeDeclInitMain(jvmHashTable **pHT, Scope *pScope)
{
	int index;
	HashTable *t;
	HashTable **pHashTable;
	HashTable Dati;

	pHashTable = pScope->stack[0];

	for (index = 0; index < HT_SIZE; index++) {
		t = pHashTable[index];
		while (t != NULL) {
			scopeFind(pScope, t->Key, &Dati);

			if (t->Tipo0 == T_SIMPLE && t->bInizializzato && t->bDecl) {
				switch (t->Tipo1) {
				case T_INTEGER:
					jvmPushInt(t->Value.iVal, pHT);
					break;
				case T_REAL:
					jvmPushDouble(t->Value.dblVal, pHT);
					break;
				case T_BOOLEAN:
					jvmPushInt(t->Value.bVal ? 1 : 0, pHT);
					break;
				case T_STRING:
					jvmPushString(t->Value.strVal ? t->Value.strVal : "", pHT);
					break;
				}
				jvmEmitStoreLocal(Dati.frameIndex, Dati.Tipo1);
			} else if (t->Tipo0 == T_ARRAY) {
				MakeBytecodeDeclArrayMain(pHT, &Dati);
			}

			t = t->next;
		}
	}
}

/* ---------------- Traduzione TAC -> Bytecode JVM ---------------- */

typedef struct {
	char *name;
	enumTipo1 type1;
	int slot;
} TempSlot;

typedef struct {
	TempSlot *items;
	int count;
	int capacity;
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

static void tempTableInit(TempTable *tt)
{
	tt->items = NULL;
	tt->count = 0;
	tt->capacity = 0;
}

static void tempTableFree(TempTable *tt)
{
	int i;
	for (i = 0; i < tt->count; i++)
		free(tt->items[i].name);
	free(tt->items);
	tt->items = NULL;
	tt->count = 0;
	tt->capacity = 0;
}

static int tempTableFind(const TempTable *tt, const char *name)
{
	int i;
	if (!name) return -1;
	for (i = 0; i < tt->count; i++) {
		if (strcmp(tt->items[i].name, name) == 0)
			return i;
	}
	return -1;
}

static int tempTableEnsure(TempTable *tt, const char *name, enumTipo1 type1)
{
	int idx = tempTableFind(tt, name);
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
	tt->items[idx].slot = (int)g_max_locals;
	g_max_locals += (type1 == T_REAL) ? 2 : 1;
	return idx;
}

static int isTacNumericLit(const char *s, double *outDbl, long *outInt, int *isInt)
{
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

static int isTacBoolLit(const char *s, int *outBool)
{
	if (!s) return 0;
	if (strcmp(s, "true") == 0)  { *outBool = 1; return 1; }
	if (strcmp(s, "false") == 0) { *outBool = 0; return 1; }
	return 0;
}

static int isTacStringLit(const char *s)
{
	size_t len;
	if (!s) return 0;
	len = strlen(s);
	return (len >= 2 && s[0] == '"' && s[len - 1] == '"');
}

static int tacElemByteSize(enumTipo1 t1)
{
	switch (t1) {
		case T_INTEGER: return (int)sizeof(int);
		case T_REAL:    return (int)sizeof(double);
		case T_BOOLEAN: return (int)sizeof(char);
		case T_STRING:  return (int)sizeof(char *);
	}
	return (int)sizeof(int);
}

static enumTipo1 jvmInferOperandType(const char *opStr, Scope *pScope, const TempTable *tt, enumTipo1 fallback)
{
	HashTable Dati;
	double d;
	long iv;
	int isInt, b;
	int tidx;

	if (!opStr) return fallback;
	if (isTacStringLit(opStr)) return T_STRING;
	if (isTacBoolLit(opStr, &b)) return T_BOOLEAN;
	if (isTacNumericLit(opStr, &d, &iv, &isInt)) return isInt ? T_INTEGER : T_REAL;

	if (scopeFind(pScope, (char *)opStr, &Dati))
		return Dati.Tipo1;

	tidx = tempTableFind(tt, opStr);
	if (tidx >= 0)
		return tt->items[tidx].type1;

	return fallback;
}

/* Pre-assegna tipo e slot locale a tutti i temporanei presenti nella TACList */
static void jvmPrepareTemps(const TACList *tac, Scope *pScope, TempTable *tt)
{
	int i;
	/* Assicuriamoci che g_max_locals copra tutte le variabili utente */
	if (pScope->top >= 0 && pScope->stack[0]) {
		for (i = 0; i < HT_SIZE; i++) {
			HashTable *t = pScope->stack[0][i];
			while (t) {
				int need = t->frameIndex + ((t->Tipo0 == T_SIMPLE && t->Tipo1 == T_REAL) ? 2 : 1);
				if (need > (int)g_max_locals)
					g_max_locals = (U2)need;
				t = t->next;
			}
		}
	}

	for (i = 0; i < tac->count; i++) {
		const TACInstr *in = &tac->items[i];
		HashTable Dati;
		enumTipo1 tRes;

		if (in->dead || !in->res) continue;
		if (in->op == TAC_LABEL || in->op == TAC_GOTO ||
		    in->op == TAC_IF_FALSE || in->op == TAC_IF_TRUE ||
		    in->op == TAC_STORE_IDX)
			continue;

		if (scopeFind(pScope, in->res, &Dati))
			continue;

		if (in->op >= TAC_LT && in->op <= TAC_OR) {
			tRes = T_BOOLEAN;
		} else if (in->op == TAC_NOT) {
			tRes = T_BOOLEAN;
		} else if (in->op == TAC_NEG) {
			tRes = jvmInferOperandType(in->arg1, pScope, tt, in->type1);
		} else if (in->op == TAC_ASSIGN) {
			tRes = jvmInferOperandType(in->arg1, pScope, tt, in->type1);
		} else {
			tRes = in->type1;
		}

		tempTableEnsure(tt, in->res, tRes);
	}
}

static void jvmLoadOperand(const char *opStr, enumTipo1 targetType,
                           jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	HashTable Dati;
	double d;
	long iv;
	int isInt, b;
	int tidx;

	if (isTacStringLit(opStr)) {
		size_t len = strlen(opStr);
		char *raw = (char *)malloc(len - 1);
		if (raw) {
			memcpy(raw, opStr + 1, len - 2);
			raw[len - 2] = '\0';
			jvmPushString(raw, pHT);
			free(raw);
		}
		return;
	}

	if (isTacBoolLit(opStr, &b)) {
		if (targetType == T_REAL)
			jvmPushDouble(b ? 1.0 : 0.0, pHT);
		else
			jvmPushInt(b ? 1 : 0, pHT);
		return;
	}

	if (isTacNumericLit(opStr, &d, &iv, &isInt)) {
		if (targetType == T_REAL) {
			jvmPushDouble(d, pHT);
		} else if (targetType == T_BOOLEAN) {
			jvmPushInt(d != 0.0 ? 1 : 0, pHT);
		} else {
			jvmPushInt((int)iv, pHT);
		}
		return;
	}

	if (scopeFind(pScope, (char *)opStr, &Dati)) {
		jvmEmitLoadLocal(Dati.frameIndex, Dati.Tipo1);
		jvmTypeCheck(targetType, Dati.Tipo1);
		return;
	}

	tidx = tempTableFind(tt, opStr);
	if (tidx >= 0) {
		jvmEmitLoadLocal(tt->items[tidx].slot, tt->items[tidx].type1);
		jvmTypeCheck(targetType, tt->items[tidx].type1);
		return;
	}

	printf("Errore JVM: operando '%s' non trovato.\n", opStr ? opStr : "(null)");
	if (targetType == T_REAL) jvmPushDouble(0.0, pHT);
	else                      jvmPushInt(0, pHT);
}

static void jvmLoadAsBool(const char *opStr, jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	enumTipo1 t = jvmInferOperandType(opStr, pScope, tt, T_INTEGER);
	if (t == T_REAL) {
		jvmLoadOperand(opStr, T_REAL, pHT, pScope, tt);
		jvmEmitU1(jvm_dconst_0, +2);
		jvmEmitU1(jvm_dcmpl, -3); /* pop 4, push 1 => -3 */
	} else {
		jvmLoadOperand(opStr, T_INTEGER, pHT, pScope, tt);
	}
	jvmEmitU1(jvm_ifeq, -1);
	jvmEmitS2(7);
	jvmEmitU1(jvm_iconst_1, +1);
	jvmEmitU1(jvm_goto, 0);
	jvmEmitS2(4);
	jvmEmitU1(jvm_iconst_0, +1);
}

static void jvmStoreDest(const char *dstName, enumTipo1 valueType, Scope *pScope, TempTable *tt)
{
	HashTable Dati;
	int tidx;

	if (scopeFind(pScope, (char *)dstName, &Dati)) {
		jvmTypeCheck(Dati.Tipo1, valueType);
		jvmEmitStoreLocal(Dati.frameIndex, Dati.Tipo1);
		return;
	}

	tidx = tempTableEnsure(tt, dstName, valueType);
	if (tidx >= 0) {
		jvmTypeCheck(tt->items[tidx].type1, valueType);
		jvmEmitStoreLocal(tt->items[tidx].slot, tt->items[tidx].type1);
	}
}

static void jvmEmitArrayIndexFromByteOffset(const char *offStr, enumTipo1 elemType,
                                            jvmHashTable **pHT, Scope *pScope, const TempTable *tt)
{
	int elemSize = tacElemByteSize(elemType);
	double d;
	long iv;
	int isInt;

	if (isTacNumericLit(offStr, &d, &iv, &isInt)) {
		jvmPushInt((int)(iv / elemSize), pHT);
		return;
	}

	jvmLoadOperand(offStr, T_INTEGER, pHT, pScope, tt);
	if (elemSize == 1) {
		return;
	} else if (elemSize == 2) {
		jvmEmitU1(jvm_iconst_1, +1);
		jvmEmitU1(jvm_ishr, -1);
	} else if (elemSize == 4) {
		jvmEmitU1(jvm_iconst_2, +1);
		jvmEmitU1(jvm_ishr, -1);
	} else if (elemSize == 8) {
		jvmEmitU1(jvm_iconst_3, +1);
		jvmEmitU1(jvm_ishr, -1);
	} else {
		jvmPushInt(elemSize, pHT);
		jvmEmitU1(jvm_idiv, -1);
	}
}

static void MakeBytecodeFromTAC(const TACList *tac, jvmHashTable **pHT, Scope *pScope)
{
	TempTable tt;
	LabelPos *labels = NULL;
	int numLabels = 0, capLabels = 0;
	JumpFixup *fixups = NULL;
	int numFixups = 0, capFixups = 0;
	int i;

	tempTableInit(&tt);
	jvmPrepareTemps(tac, pScope, &tt);

	for (i = 0; i < tac->count; i++) {
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
			labels[numLabels].offset = g_IndexBytecodeArray;
			numLabels++;
			break;
		}

		case TAC_GOTO: {
			int instrPos = g_IndexBytecodeArray;
			jvmEmitU1(jvm_goto, 0);
			if (numFixups >= capFixups) {
				int nc = (capFixups == 0) ? 32 : capFixups * 2;
				JumpFixup *nf = (JumpFixup *)realloc(fixups, (size_t)nc * sizeof(JumpFixup));
				if (nf) { fixups = nf; capFixups = nc; }
			}
			if (numFixups < capFixups) {
				fixups[numFixups].instrPos = instrPos;
				fixups[numFixups].operandPos = g_IndexBytecodeArray;
				fixups[numFixups].labelName = (char *)malloc(strlen(in->res) + 1);
				if (fixups[numFixups].labelName) strcpy(fixups[numFixups].labelName, in->res);
				numFixups++;
			}
			jvmEmitS2(0);
			break;
		}

		case TAC_IF_FALSE:
		case TAC_IF_TRUE: {
			enumTipo1 condType = jvmInferOperandType(in->arg1, pScope, &tt, T_INTEGER);
			int instrPos;
			if (condType == T_REAL) {
				jvmLoadOperand(in->arg1, T_REAL, pHT, pScope, &tt);
				jvmEmitU1(jvm_dconst_0, +2);
				jvmEmitU1(jvm_dcmpl, -3);
			} else {
				jvmLoadOperand(in->arg1, T_INTEGER, pHT, pScope, &tt);
			}
			instrPos = g_IndexBytecodeArray;
			jvmEmitU1((in->op == TAC_IF_FALSE) ? jvm_ifeq : jvm_ifne, -1);
			if (numFixups >= capFixups) {
				int nc = (capFixups == 0) ? 32 : capFixups * 2;
				JumpFixup *nf = (JumpFixup *)realloc(fixups, (size_t)nc * sizeof(JumpFixup));
				if (nf) { fixups = nf; capFixups = nc; }
			}
			if (numFixups < capFixups) {
				fixups[numFixups].instrPos = instrPos;
				fixups[numFixups].operandPos = g_IndexBytecodeArray;
				fixups[numFixups].labelName = (char *)malloc(strlen(in->res) + 1);
				if (fixups[numFixups].labelName) strcpy(fixups[numFixups].labelName, in->res);
				numFixups++;
			}
			jvmEmitS2(0);
			break;
		}

		case TAC_ASSIGN: {
			HashTable Dati;
			enumTipo1 dstType = in->type1;
			int tidx;
			if (scopeFind(pScope, in->res, &Dati))
				dstType = Dati.Tipo1;
			else if ((tidx = tempTableFind(&tt, in->res)) >= 0)
				dstType = tt.items[tidx].type1;
			jvmLoadOperand(in->arg1, dstType, pHT, pScope, &tt);
			jvmStoreDest(in->res, dstType, pScope, &tt);
			break;
		}

		case TAC_NEG: {
			enumTipo1 t = jvmInferOperandType(in->arg1, pScope, &tt, in->type1);
			if (t != T_REAL) t = T_INTEGER;
			jvmLoadOperand(in->arg1, t, pHT, pScope, &tt);
			jvmEmitU1((t == T_REAL) ? jvm_dneg : jvm_ineg, 0);
			jvmStoreDest(in->res, t, pScope, &tt);
			break;
		}

		case TAC_NOT: {
			jvmLoadAsBool(in->arg1, pHT, pScope, &tt);
			jvmEmitU1(jvm_ifne, -1);
			jvmEmitS2(7);
			jvmEmitU1(jvm_iconst_1, +1);
			jvmEmitU1(jvm_goto, 0);
			jvmEmitS2(4);
			jvmEmitU1(jvm_iconst_0, +1);
			jvmStoreDest(in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_ADD:
		case TAC_SUB:
		case TAC_MUL:
		case TAC_DIV: {
			if (in->op == TAC_ADD && in->type1 == T_STRING) {
				jvmEmitU1(jvm_new, +1);
				jvmEmitU2(6); /* java/lang/StringBuilder */
				jvmEmitU1(jvm_dup, +1);
				jvmEmitU1(jvm_invokespecial, -1);
				jvmEmitU2(11); /* StringBuilder.<init>()V */

				jvmLoadOperand(in->arg1, T_STRING, pHT, pScope, &tt);
				jvmEmitU1(jvm_invokevirtual, -1);
				jvmEmitU2(12); /* StringBuilder.append(String) */

				jvmLoadOperand(in->arg2, T_STRING, pHT, pScope, &tt);
				jvmEmitU1(jvm_invokevirtual, -1);
				jvmEmitU2(12); /* StringBuilder.append(String) */

				jvmEmitU1(jvm_invokevirtual, 0);
				jvmEmitU2(13); /* StringBuilder.toString() */

				jvmStoreDest(in->res, T_STRING, pScope, &tt);
			} else {
				enumTipo1 opType = (in->type1 == T_REAL) ? T_REAL : T_INTEGER;
				U1 opByte = jvm_iadd;
				int stackDelta = (opType == T_REAL) ? -2 : -1;

				jvmLoadOperand(in->arg1, opType, pHT, pScope, &tt);
				jvmLoadOperand(in->arg2, opType, pHT, pScope, &tt);

				if (in->op == TAC_ADD)      opByte = (opType == T_REAL) ? jvm_dadd : jvm_iadd;
				else if (in->op == TAC_SUB) opByte = (opType == T_REAL) ? jvm_dsub : jvm_isub;
				else if (in->op == TAC_MUL) opByte = (opType == T_REAL) ? jvm_dmul : jvm_imul;
				else if (in->op == TAC_DIV) opByte = (opType == T_REAL) ? jvm_ddiv : jvm_idiv;

				jvmEmitU1(opByte, stackDelta);
				jvmStoreDest(in->res, opType, pScope, &tt);
			}
			break;
		}

		case TAC_LT:
		case TAC_LE:
		case TAC_GT:
		case TAC_GE:
		case TAC_EQ:
		case TAC_NE: {
			enumTipo1 t1 = jvmInferOperandType(in->arg1, pScope, &tt, in->type1);
			enumTipo1 t2 = jvmInferOperandType(in->arg2, pScope, &tt, in->type1);
			enumTipo1 cmpType = (t1 == T_REAL || t2 == T_REAL || in->type1 == T_REAL) ? T_REAL : T_INTEGER;

			jvmLoadOperand(in->arg1, cmpType, pHT, pScope, &tt);
			jvmLoadOperand(in->arg2, cmpType, pHT, pScope, &tt);

			if (cmpType == T_INTEGER) {
				U1 br = jvm_if_icmpne;
				if (in->op == TAC_LT)      br = jvm_if_icmpge;
				else if (in->op == TAC_LE) br = jvm_if_icmpgt;
				else if (in->op == TAC_GT) br = jvm_if_icmple;
				else if (in->op == TAC_GE) br = jvm_if_icmplt;
				else if (in->op == TAC_EQ) br = jvm_if_icmpne;
				else if (in->op == TAC_NE) br = jvm_if_icmpeq;
				jvmEmitU1(br, -2);
			} else {
				U1 cmpOp = (in->op == TAC_LT || in->op == TAC_LE) ? jvm_dcmpg : jvm_dcmpl;
				U1 br = jvm_ifne;
				if (in->op == TAC_LT)      br = jvm_ifge;
				else if (in->op == TAC_LE) br = jvm_ifgt;
				else if (in->op == TAC_GT) br = jvm_ifle;
				else if (in->op == TAC_GE) br = jvm_iflt;
				else if (in->op == TAC_EQ) br = jvm_ifne;
				else if (in->op == TAC_NE) br = jvm_ifeq;
				jvmEmitU1(cmpOp, -3);
				jvmEmitU1(br, -1);
			}

			jvmEmitS2(7);
			jvmEmitU1(jvm_iconst_1, +1);
			jvmEmitU1(jvm_goto, 0);
			jvmEmitS2(4);
			jvmEmitU1(jvm_iconst_0, +1);
			jvmStoreDest(in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_AND:
		case TAC_OR: {
			jvmLoadAsBool(in->arg1, pHT, pScope, &tt);
			jvmLoadAsBool(in->arg2, pHT, pScope, &tt);
			jvmEmitU1((in->op == TAC_AND) ? jvm_iand : jvm_ior, -1);
			jvmStoreDest(in->res, T_BOOLEAN, pScope, &tt);
			break;
		}

		case TAC_LOAD_IDX: {
			HashTable Dati;
			if (!scopeFind(pScope, in->arg1, &Dati)) {
				printf("Errore JVM: array '%s' non dichiarato.\n", in->arg1);
				break;
			}
			jvmEmitLoadArrayRef(Dati.frameIndex);
			jvmEmitArrayIndexFromByteOffset(in->arg2, Dati.Tipo1, pHT, pScope, &tt);

			if (Dati.Tipo1 == T_INTEGER) {
				jvmEmitU1(jvm_iaload, -1);
			} else if (Dati.Tipo1 == T_REAL) {
				jvmEmitU1(jvm_daload, 0); /* pop 2, push 2 */
			} else if (Dati.Tipo1 == T_BOOLEAN) {
				jvmEmitU1(jvm_baload, -1);
			} else if (Dati.Tipo1 == T_STRING) {
				jvmEmitU1(jvm_aaload, -1);
			}
			jvmStoreDest(in->res, Dati.Tipo1, pScope, &tt);
			break;
		}

		case TAC_STORE_IDX: {
			HashTable Dati;
			if (!scopeFind(pScope, in->res, &Dati)) {
				printf("Errore JVM: array '%s' non dichiarato.\n", in->res);
				break;
			}
			jvmEmitLoadArrayRef(Dati.frameIndex);
			jvmEmitArrayIndexFromByteOffset(in->arg1, Dati.Tipo1, pHT, pScope, &tt);
			jvmLoadOperand(in->arg2, Dati.Tipo1, pHT, pScope, &tt);

			if (Dati.Tipo1 == T_INTEGER) {
				jvmEmitU1(jvm_iastore, -3);
			} else if (Dati.Tipo1 == T_REAL) {
				jvmEmitU1(jvm_dastore, -4);
			} else if (Dati.Tipo1 == T_BOOLEAN) {
				jvmEmitU1(jvm_bastore, -3);
			} else if (Dati.Tipo1 == T_STRING) {
				jvmEmitU1(jvm_aastore, -3);
			}
			break;
		}

		case TAC_PRINT: {
			enumTipo1 pType = jvmInferOperandType(in->arg1, pScope, &tt, in->type1);
			U2 methodRef = 60;
			int argSlots = 1;

			/* Se in->type1 specifica T_BOOLEAN ma l'operando era int (es. constant folding),
			 * rispetta in->type1 per stampare true/false */
			if (in->type1 == T_BOOLEAN && pType == T_INTEGER)
				pType = T_BOOLEAN;

			jvmEmitU1(jvm_getstatic, +1);
			jvmEmitU2(9); /* java/lang/System.out */

			jvmLoadOperand(in->arg1, pType, pHT, pScope, &tt);

			if (pType == T_STRING)       { methodRef = 14; argSlots = 1; }
			else if (pType == T_INTEGER) { methodRef = 60; argSlots = 1; }
			else if (pType == T_REAL)    { methodRef = 63; argSlots = 2; }
			else if (pType == T_BOOLEAN) { methodRef = 66; argSlots = 1; }

			jvmEmitU1(jvm_invokevirtual, -(1 + argSlots));
			jvmEmitU2(methodRef);
			break;
		}

		case TAC_READ: {
			HashTable Dati;
			enumTipo1 rType = in->type1;
			if (scopeFind(pScope, in->res, &Dati))
				rType = Dati.Tipo1;

			jvmEmitU1(jvm_invokestatic, +1);
			jvmEmitU2(15); /* System.console() */
			jvmEmitU1(jvm_invokevirtual, 0);
			jvmEmitU2(16); /* Console.readLine() */

			if (rType == T_INTEGER) {
				jvmEmitU1(jvm_invokestatic, 0);
				jvmEmitU2(17); /* Integer.parseInt */
			} else if (rType == T_REAL) {
				jvmEmitU1(jvm_invokestatic, +1); /* String -> double (1 -> 2 slot) */
				jvmEmitU2(18); /* Double.parseDouble */
			} else if (rType == T_BOOLEAN) {
				jvmEmitU1(jvm_invokestatic, 0);
				jvmEmitU2(17); /* Integer.parseInt */
				jvmEmitU1(jvm_ifeq, -1);
				jvmEmitS2(7);
				jvmEmitU1(jvm_iconst_1, +1);
				jvmEmitU1(jvm_goto, 0);
				jvmEmitS2(4);
				jvmEmitU1(jvm_iconst_0, +1);
			}

			jvmStoreDest(in->res, rType, pScope, &tt);
			break;
		}
		}
	}

	/* Risoluzione dei salti (patch degli offset relativi a 16 bit) */
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
			printf("Errore JVM: label '%s' non trovata.\n", fixups[i].labelName);
		} else {
			S2 rel = (S2)(targetOffset - fixups[i].instrPos);
			BYTE *buff;
			U1 fb[8];
			if (!isBigEndian()) {
				ARRAY_TO_S2(buff, rel);
			}
			memcpy(g_ArrayCode + fixups[i].operandPos, &rel, 2);
		}
	}

	for (i = 0; i < numLabels; i++) free(labels[i].name);
	free(labels);
	for (i = 0; i < numFixups; i++) free(fixups[i].labelName);
	free(fixups);
	tempTableFree(&tt);
}

void MakeBytecodeMain(nodeType *p, jvmHashTable **pHT, Scope *pScope)
{
	(void)p;
	(void)pHT;
	(void)pScope;
}

void jvmStart(char *szInputFileName, nodeType **ppTree, Scope *pScope, int optLevel)
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

	MakeOutputFileName(szInputFileName, szOutputFileName, szFileNameWithoutExt, szExt);
	strcpy(szSourceFile, szFileNameWithoutExt);
	if (szExt[0] != '\0') {
		strcat(szSourceFile, ".");
		strcat(szSourceFile, szExt);
	}
	strcpy(szClassName, szFileNameWithoutExt);

	fp = fopen(szOutputFileName, "wb");
	if (!fp) {
		printf("Errore. Impossibile creare il file %s.\n", szOutputFileName);
		return;
	}

	pHT = (jvmHashTable**)calloc(JVM_HT_SIZE, sizeof(jvmHashTable*));
	if (pHT == NULL) {
		printf("Memoria non sufficiente.\n");
		fclose(fp);
		return;
	}

	/* 1) Costruisce e ottimizza il TAC (-O0 .. -O3) */
	tacBuild(ppTree, pScope, optLevel, &tac);

	/* 2) Inizializza stato del generatore JVM */
	g_ConstantPoolCount = 68;
	g_IndexBytecodeArray = 0;
	g_code_length = 0;
	g_max_stack = 1;
	g_cur_stack = 0;

	jvmWriteHeader(fp);
	g_IndexConstantPool = jvmMakeConstantPool(szSourceFile, szClassName);

	/* 3) Inizializza variabili dichiarate/array ed emette bytecode dal TAC */
	MakeBytecodeDeclInitMain(pHT, pScope);
	MakeBytecodeFromTAC(&tac, pHT, pScope);

	jvmEmitU1(jvm_return, 0);

	tacFree(&tac);

	constant_pool_count = (U2)(g_ConstantPoolCount + 1);
	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, constant_pool_count);
	}
	fwrite(&constant_pool_count, sizeof(U2), 1, fp);
	fwrite(g_ArrayConstantPool, (size_t)g_IndexConstantPool, 1, fp);

	jvmWriteInfo(fp);
	jvmWriteInitMethod(fp);
	jvmWriteMainMethod(fp, *ppTree, pHT);

	if (!isBigEndian()) {
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);

	fclose(fp);

	for (x = 0; x < JVM_HT_SIZE; x++)
		jvmhtFree(pHT[x]);
	free(pHT);
}
