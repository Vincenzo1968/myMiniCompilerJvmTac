#include <stdio.h>
#include <malloc.h>
#include <string.h> // necessario per memcpy
#include <ctype.h>
#include <math.h>
#include "scanner.h"
#include "endianess.h"
#include "jvm.h"

// #pragma warning(disable : 4996)

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

U2 g_ConstantPoolCount = 68;
//U2 g_ConstantPoolCount = 13;
U1 g_ArrayConstantPool[4096];
U1 g_ArrayCode[4096];

int g_IndexConstantPool;
int g_IndexBytecodeArray = 0;

int g_myType1 = -1;

U2 g_max_stack = 0;
U2 g_max_locals = 1;
U4 g_code_length = 0;


// -------------------------- HashTable -----------------------------------------


//HashTable* htNewNode(char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato)
jvmHashTable* jvmhtNewNode(char *Key, unsigned short c)
{
	jvmHashTable *n = NULL;

	n = (jvmHashTable*)malloc(sizeof(jvmHashTable));

	if( n == NULL )
		return NULL;

	n->Key = (char*)malloc(strlen(Key)+1);
	if ( n->Key == NULL )
	{ 
		free(n);
		return NULL;
	}
	strcpy(n->Key, Key);
	n->c = c;
	n->next = NULL;

	return n;
}

//int htFind(HashTable **pHashTable, char *Key, HashTable *pDati)
int jvmhtFind(jvmHashTable **pHashTable, char *Key, unsigned short *c)
{
	int index = 0;
	jvmHashTable *t = NULL;
	int a = 31415;
	/* int b = 27183; */
	char szTemp[1024];
	//char *s = Key;
	char *s = szTemp;

	strcpy(szTemp, Key);

	*c = 0;

	for(; *s != '\0'; s++)
	{
		if ( *s == '\n' )
			*s = ' ';
		index = (a*index + *s) % JVM_HT_SIZE;
	}
	if ( index < 0 )
		index *= -1;

	t = pHashTable[index];
	while ( t != NULL )
	{
		if ( strcmp(t->Key, Key) == 0 )
		{
			*c = t->c;
			return 1;
		}
		t = t->next;
	}

	return 0;
}

//void htInsert(HashTable **pHashTable, char *Key, enumTipo0 t0, enumTipo1 t1, enumTipo2 t2, Valore v, char bInizializzato)
void jvmhtInsert(jvmHashTable **pHashTable, char *Key, unsigned short c)
{
	int index = 0;
	jvmHashTable *t = NULL;
	int a = 31415;
	/* int b = 27183; */
	char szTemp[1024];
	//char *s = Key;
	char *s = szTemp;

	strcpy(szTemp, Key);

	for(; *s != '\0'; s++)
	{
		if ( *s == '\n' )
			*s = ' ';
		index = (a*index + *s) % JVM_HT_SIZE;
	}
	if ( index < 0 )
		index *= -1;

	t = pHashTable[index];
	if ( t == NULL )
	{
		pHashTable[index] = jvmhtNewNode(Key, c);
		return;
	}

	while ( t != NULL )
	{
		if ( strcmp(t->Key, Key) == 0 )
		{
			//pHashTable[index]->pLista = ListAppend(pHashTable[index]->pLista, pos);
			printf("\nErrore: La chiave %s e' gia' presente nella hashtable\n", Key);
			return;
		}
		if ( t->next == NULL )
		{
			t->next = jvmhtNewNode(Key, c);
			//t = t->next;
			//t->next = NULL;
			return;
		}
		t = t->next;
	}
}

//void htFree(HashTable* first)
void jvmhtFree(jvmHashTable* first)
{
	jvmHashTable *n1 = first, *n2 = NULL;
	while ( n1 != NULL )
	{
		n2 = n1->next;
		if ( n1->Key )
		{
			free(n1->Key);
		}
		free(n1);
		n1 = n2;
	}
}

// ------------------------------ JVM ----------------------------------------------

void jvmSetNumberLocals(int n)
{
	g_max_locals = n;
}

void jvmWriteHeader(FILE *fp)
{
	U4 magic = 0xCAFEBABE;
	U2 minor_version = 0;
	U2 major_version = 50;

	//unsigned char *buff;
	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	if ( !isBigEndian() )
	{
		ARRAY_TO_U4(buff, magic);
		ARRAY_TO_U2(buff, minor_version);
		ARRAY_TO_U2(buff, major_version);
	}

	fwrite(&magic, sizeof(U4), 1, fp);
	fwrite(&minor_version, sizeof(U2), 1, fp);
	fwrite(&major_version, sizeof(U2), 1, fp);
}

int jvmMakeConstantPool(char *szSourceFile, char *szClassName)
{
	int k;
	int index = 0;
	U1 tag;
	U2 name_index;
	U2 class_index;
	U2 name_and_type_index;
	U2 descriptor_index;
	//U2 string_index;
	U2 length;
	U2 lengthBigEndian;

	char szStringa[1024];

	U1 fb[8]; // Necessario per usare le macro;
	BYTE *buff;

	tag = CONSTANT_Class;
	for ( k = 25; k < 33; k++)
	{
		g_ArrayConstantPool[index++] = tag;
		name_index = k;
		if ( !isBigEndian() )
		{
			ARRAY_TO_U2(buff, name_index);
		}
		memcpy(g_ArrayConstantPool + index, &name_index, 2);
		index += 2;
	}

	tag = CONSTANT_Fieldref;
	class_index = 3;
	name_and_type_index = 33;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;



	tag = CONSTANT_Methodref;

	class_index = 2;
	name_and_type_index = 36;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 6;
	name_and_type_index = 36;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 6;
	name_and_type_index = 39;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 6;
	name_and_type_index = 42;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 4;
	name_and_type_index = 45;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 3;
	name_and_type_index = 48;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 5;
	name_and_type_index = 51;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 7;
	name_and_type_index = 54;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	class_index = 8;
	name_and_type_index = 57;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;




	tag = CONSTANT_Utf8;

	strcpy(szStringa, "Code");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "LineNumberTable");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "main");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "([Ljava/lang/String;)V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "SourceFile");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, szSourceFile);
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	//strcpy(szStringa, "PrintDemo");
	strcpy(szStringa, szClassName);
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/lang/Object");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/lang/System");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/io/PrintStream");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/io/Console");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/lang/StringBuilder");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/lang/Integer");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "java/lang/Double");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 34;
	descriptor_index = 35;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "out");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "Ljava/io/PrintStream;");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 37;
	descriptor_index = 38;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "<init>");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "()V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 40;
	descriptor_index = 41;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "append");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "(Ljava/lang/String;)Ljava/lang/StringBuilder;");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 43;
	descriptor_index = 44;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "toString");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "()Ljava/lang/String;");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 46;
	descriptor_index = 47;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	//strcpy(szStringa, "print");
	strcpy(szStringa, "println");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "(Ljava/lang/String;)V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 49;
	descriptor_index = 50;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "console");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "()Ljava/io/Console;");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 52;
	descriptor_index = 53;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "readLine");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "()Ljava/lang/String;");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 55;
	descriptor_index = 56;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "parseInt");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "(Ljava/lang/String;)I");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_NameAndType;
	name_index = 58;
	descriptor_index = 59;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "parseDouble");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	strcpy(szStringa, "(Ljava/lang/String;)D");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;



	//---------------------- Aggiunti in seguito --------------------

	tag = CONSTANT_Methodref;
	class_index = 4;
	name_and_type_index = 61;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	tag = CONSTANT_NameAndType;
	name_index = 46;
	descriptor_index = 62;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "(I)V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_Methodref;
	class_index = 4;
	name_and_type_index = 64;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	tag = CONSTANT_NameAndType;
	name_index = 46;
	descriptor_index = 65;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "(D)V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;


	tag = CONSTANT_Methodref;
	class_index = 4;
	name_and_type_index = 67;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, class_index);
		ARRAY_TO_U2(buff, name_and_type_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &class_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &name_and_type_index, 2);
	index += 2;

	tag = CONSTANT_NameAndType;
	name_index = 46;
	descriptor_index = 68;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, name_index);
		ARRAY_TO_U2(buff, descriptor_index);
	}
	g_ArrayConstantPool[index++] = tag;
	memcpy(g_ArrayConstantPool + index, &name_index, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, &descriptor_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szStringa, "(Z)V");
	length = strlen(szStringa);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szStringa, length);
	index += length;

	return index;
}


/*
	tag = CONSTANT_Class;
	for ( k = 25; k < 33; k++)
	{
		g_ArrayConstantPool[index++] = tag;
		name_index = k;
		if ( !isBigEndian() )
		{
			ARRAY_TO_U2(buff, name_index);
		}
		memcpy(g_ArrayConstantPool + index, &name_index, 2);
		index += 2;
	}
*/

int jvmAddArrayClass(int type, int sizes, int index)
{
	char szClassName[512];
	U1 tag;
	U2 string_index;
	U2 length;
	U2 lengthBigEndian;

	U1 fb[8]; // Necessario per usare le macro;
	BYTE *buff;

	int k;

	tag = CONSTANT_Class;
	g_ArrayConstantPool[index++] = tag;
	string_index = g_ConstantPoolCount + 2;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	strcpy(szClassName, "[");
	for ( k = 1; k < sizes; k++ )
	{
		strcat(szClassName, "[");
	}
	if ( type == T_INTEGER )
		strcat(szClassName, "I");
	if ( type == T_REAL )
		strcat(szClassName, "D");
	if ( type == T_BOOLEAN )
		strcat(szClassName, "Z");
	if ( type == T_STRING )
		strcat(szClassName, "Ljava/lang/String;");
	length = strlen(szClassName);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, szClassName, length);
	index += length;

	g_ConstantPoolCount += 2;

	return index;
}


int jvmAddStringConstant(char *sz, int index)
{
	U1 tag;
	U2 string_index;
	U2 length;
	U2 lengthBigEndian;

	U1 fb[8]; // Necessario per usare le macro;
	BYTE *buff;

	tag = CONSTANT_String;
	g_ArrayConstantPool[index++] = tag;
	string_index = g_ConstantPoolCount + 2;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, string_index);
	}
	memcpy(g_ArrayConstantPool + index, &string_index, 2);
	index += 2;

	tag = CONSTANT_Utf8;
	length = strlen(sz);
	lengthBigEndian = length;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, lengthBigEndian);
	}
	memcpy(g_ArrayConstantPool + index, &lengthBigEndian, 2);
	index += 2;
	memcpy(g_ArrayConstantPool + index, sz, length);
	index += length;

	g_ConstantPoolCount += 2;

	return index;
}

int jvmAddIntegerConstant(int c, int index)
{
	U1 tag;
	U4 bytes = (U4)c;

	U1 fb[8]; // Necessario per usare le macro;
	BYTE *buff;

	tag = CONSTANT_Integer;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U4(buff, bytes);
	}
	memcpy(g_ArrayConstantPool + index, &bytes, 4);
	index += 4;

	g_ConstantPoolCount++;

	return index;
}

int jvmAddRealConstant(double c, int index)
{
	U1 tag;
	F8 bytes = (F8)c;

	//U1 fb[8]; // Necessario per usare le macro;
	//BYTE *buff;

	tag = CONSTANT_Double;
	g_ArrayConstantPool[index++] = tag;
	if ( !isBigEndian() )
	{
		/* ARRAY_TO_F8(buff, bytes); */
		SwapDouble(c, &bytes);
	}
	memcpy(g_ArrayConstantPool + index, &bytes, 8);
	index += 8;

	//g_ConstantPoolCount++;
	g_ConstantPoolCount += 2;

	return index;
}

void jvmWriteInfo(FILE *fp)
{
	U2 access_flags = ACC_SUPER;
    U2 this_class = 1;
    U2 super_class = 2;
    U2 interfaces_count = 0;
    //U2 interfaces[interfaces_count];
    U2 fields_count = 0;
    //field_info fields[fields_count];
    U2 methods_count = 2;

	//unsigned char *buff;
	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	if ( !isBigEndian() )
	{
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

    //U2 name_index = 5;
    //U2 descriptor_index = 6;
    U2 name_index = 37;
    U2 descriptor_index = 38;

    U2 attributes_count = 1;
    //attribute_info attributes[attributes_count];

	//U2 attribute_name_index = 7;
	U2 attribute_name_index = 19;


    //U4 attribute_length = 29;
    U4 attribute_length = 17;
    U2 max_stack = 1;
    U2 max_locals = 1;
    U4 code_length = 5;
    //U1 code[code_length];
    U2 exception_table_length = 0;
	/*
    {    	u2 start_pc;
          	u2 end_pc;
         	u2  handler_pc;
          	u2  catch_type;
    }	exception_table[exception_table_length];
	*/
    //U2 attributes_count;
    //attribute_info attributes[attributes_count];

	//unsigned char *buff;
	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	if ( !isBigEndian() )
	{
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

	//param = 3; //Method java/lang/Object."<init>":()V
	param = 10; //Method java/lang/Object."<init>":()V
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, param);
	}
	fwrite(&param, sizeof(U2), 1, fp);

	bytecode = jvm_return;
	fwrite(&bytecode, sizeof(U1), 1, fp);


	exception_table_length = 0;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void jvmWriteMainMethod(FILE *fp, nodeType *pTree, jvmHashTable **pHT)
{
	(void)pTree;
	(void)pHT;

	U2 access_flags = ACC_PUBLIC | ACC_STATIC;

    //U2 name_index = 8;
    //U2 descriptor_index = 9;
    U2 name_index = 21;
    U2 descriptor_index = 22;

    U2 attributes_count = 1;
    //attribute_info attributes[attributes_count];

	//U2 attribute_name_index = 7;
	U2 attribute_name_index = 19;

    //U4 attribute_length = 37;
    U4 attribute_length;
    //U2 max_stack;
    //U2 max_locals;
    //U4 code_length;
    U4 code_lengthBigEndian;
    //U1 code[code_length];
    U2 exception_table_length = 0;
	/*
    {    	u2 start_pc;
          	u2 end_pc;
         	u2  handler_pc;
          	u2  catch_type;
    }	exception_table[exception_table_length];
	*/
    //U2 attributes_count;
    //attribute_info attributes[attributes_count];

	//unsigned char *buff;
	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	if ( !isBigEndian() )
	{
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

	//MakeCodeArray_PrintDemo(&max_stack, &max_locals, &code_length);
	//MakeBytecodeMain(pTree, pHT);

	code_lengthBigEndian = g_code_length;

	attribute_length = 12;
	attribute_length += g_code_length;

	if ( !isBigEndian() )
	{
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
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, exception_table_length);
	}
	fwrite(&exception_table_length, sizeof(U2), 1, fp);

	attributes_count = 0;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);
}

void MakeOutputFileName(char *szInputFileName, char *szOutputFileName, char *szFileNameWithoutExt, char *szExt)
{
	int k;
	int dotIndex;
	int len = strlen(szInputFileName);

	strcpy(szOutputFileName, "");
	strcpy(szFileNameWithoutExt, "");
	strcpy(szExt, "");

	k = len - 1;

	while ( k >= 0 )
	{
		if ( szInputFileName[k] == '.' )
			break;
		--k;
	}
	k++;

	if ( k > 0 )
	{
		memcpy(szExt, szInputFileName + k, len - k);
		szExt[len - k] = '\0';

		k -= 2;
		dotIndex = k + 1;
	}
	else
	{
		dotIndex = len;
	}

	while ( k >= 0 )
	{
		if ( !isalnum(szInputFileName[k]) && szInputFileName[k] != '_' )
			break;
		--k;
	}
	k++;

	memcpy(szFileNameWithoutExt, szInputFileName + k, dotIndex - k);
	szFileNameWithoutExt[dotIndex - k] = '\0';

	strcpy(szOutputFileName, szFileNameWithoutExt);
	strcat(szOutputFileName, ".class");
}

void MakeBytecodeDeclArrayMain(jvmHashTable **pHT, HashTable *pDati)
{
	char szInt[128];
	char szClassName[128];
	int k;

	U2 constClassIndex;
	U2 constIntegerIndex;

	U1 bytecode;
	U1 paramU1;
	U2 paramU2;

	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	HashTable *t = pDati;


	for ( k = 0; k < t->Value.pArrayDef->countIndexes; k++ )
	{
		//************** INIZIO LOAD *******************

		if ( t->Value.pArrayDef->sizes[k] >= -128 && t->Value.pArrayDef->sizes[k] <= 127 )
		{
			if ( t->Value.pArrayDef->sizes[k] >= -1 && t->Value.pArrayDef->sizes[k] <= 5 )
			{
				if ( t->Value.pArrayDef->sizes[k] == 0 )
					bytecode = jvm_iconst_0;
				else if ( t->Value.pArrayDef->sizes[k] == 1 )
					bytecode = jvm_iconst_1;
				else if ( t->Value.pArrayDef->sizes[k] == 2 )
					bytecode = jvm_iconst_2;
				else if ( t->Value.pArrayDef->sizes[k] == 3 )
					bytecode = jvm_iconst_3;
				else if ( t->Value.pArrayDef->sizes[k] == 4 )
					bytecode = jvm_iconst_4;
				else if ( t->Value.pArrayDef->sizes[k] == 5 )
					bytecode = jvm_iconst_5;

				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;
			}
			else
			{
				bytecode = jvm_bipush;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU1 = (U1)t->Value.pArrayDef->sizes[k];
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
			}
		}
		else if ( t->Value.pArrayDef->sizes[k] >= -32768 && t->Value.pArrayDef->sizes[k] <= 32767 )
		{
			bytecode = jvm_sipush;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
			g_max_stack++;
			g_code_length++;
			g_IndexBytecodeArray++;

			paramU2 = (U2)t->Value.pArrayDef->sizes[k];
			if ( !isBigEndian() )
			{
				ARRAY_TO_S2(buff, paramU2);
			}
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
			g_code_length += 2;
			g_IndexBytecodeArray += 2;
		}
		else
		{
			sprintf(szInt, "%d", t->Value.pArrayDef->sizes[k]);
			if ( !jvmhtFind(pHT, szInt, (unsigned short*)&constIntegerIndex) )
			{
				constIntegerIndex = (unsigned char)(g_ConstantPoolCount + 1);
				jvmhtInsert(pHT, szInt, constIntegerIndex);
				g_IndexConstantPool = jvmAddIntegerConstant(t->Value.pArrayDef->sizes[k], g_IndexConstantPool);
			}

			bytecode = jvm_ldc;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
			g_max_stack++;
			g_code_length++;
			g_IndexBytecodeArray++;

			paramU1 = constIntegerIndex;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
			g_code_length++;
			g_IndexBytecodeArray++;
		}
	}

	if ( t->Value.pArrayDef->countIndexes == 1 )
	{
		bytecode = jvm_newarray;

		switch ( t->Tipo1 )
		{
		case T_INTEGER:
			paramU1 = 10;
			break;
		case T_REAL:
			paramU1 = 7;
			break;
		case T_BOOLEAN:
			paramU1 = 4;
			break;
		case T_STRING:
			paramU1 = 8; // byte
			break;
		}

		memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
		g_max_stack++;
		g_code_length++;
		g_IndexBytecodeArray++;

		memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
		g_code_length++;
		g_IndexBytecodeArray++;

		//************** FINE LOAD *******************
	}
	else // Array Multidimensionale
	{
		bytecode = jvm_multianewarray;

		memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
		g_max_stack++;
		g_code_length++;
		g_IndexBytecodeArray++;

		//sprintf(szInt, "%d", t->Value.pArrayDef->sizes[k]);
		strcpy(szClassName, "[");
		for ( k = 1; k < t->Value.pArrayDef->countIndexes; k++ )
		{
			strcat(szClassName, "[");
		}
		if ( t->Tipo1 == T_INTEGER )
			strcat(szClassName, "I");
		if ( t->Tipo1 == T_REAL )
			strcat(szClassName, "D");
		if ( t->Tipo1 == T_BOOLEAN )
			strcat(szClassName, "Z");
		if ( t->Tipo1 == T_STRING )
			strcat(szClassName, "Ljava/lang/String;");

		if ( !jvmhtFind(pHT, szClassName, (unsigned short*)&constClassIndex) )
		{
			constClassIndex = (unsigned short)(g_ConstantPoolCount + 1);
			jvmhtInsert(pHT, szClassName, constClassIndex);
			g_IndexConstantPool = jvmAddArrayClass(t->Tipo1, t->Value.pArrayDef->countIndexes, g_IndexConstantPool);
		}

		paramU2 = constClassIndex;
		if ( !isBigEndian() )
		{
			ARRAY_TO_U2(buff, paramU2);
		}
		memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
		g_code_length += 2;
		g_IndexBytecodeArray += 2;

		paramU1 = (U1)t->Value.pArrayDef->countIndexes;
		memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
		g_code_length++;
		g_IndexBytecodeArray++;

		//************** FINE LOAD *******************
	}


	//************** INIZIO STORE ****************
	if (pDati->frameIndex >= 0 && pDati->frameIndex <= 3)
	{
		if ( (U1)pDati->frameIndex == 0 )
			bytecode = jvm_astore_0;
		else if ( (U1)pDati->frameIndex == 1 )
			bytecode = jvm_astore_1;
		else if ( (U1)pDati->frameIndex == 2 )
			bytecode = jvm_astore_2;
		else if ( (U1)pDati->frameIndex == 3 )
			bytecode = jvm_astore_3;
		memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
		g_max_stack--;
		g_code_length++;
		g_IndexBytecodeArray++;
	}
	else
	{
		bytecode = jvm_astore;
		memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
		g_max_stack--;
		g_code_length++;
		g_IndexBytecodeArray++;
		paramU1 = (U1)pDati->frameIndex;
		memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
		g_code_length++;
		g_IndexBytecodeArray++;
	}
	//************** FINE STORE ***************
}

void MakeBytecodeDeclInitMain(jvmHashTable **pHT, Scope *pScope)
{
	int index;
	HashTable *t;
	HashTable **pHashTable;

	HashTable Dati;

	char szInt[128];
	char szReal[128];

	U2 constStringIndex;
	U2 constIntegerIndex;
	U2 constRealIndex;

	U1 bytecode;
	U1 paramU1;
	U2 paramU2;
	//U4 paramU4;
	//U8 paramU8;

	S1 paramS1;
	S2 paramS2;
	//S4 paramS4;
	//S8 paramS8;

	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;


	pHashTable = pScope->stack[0];

	for ( index = 0; index < HT_SIZE - 1; index++ )
	{
		t = pHashTable[index];
		while ( t != NULL )
		{
			scopeFind(pScope, t->Key, &Dati);

			if ( t->Tipo0 == T_SIMPLE && t->bInizializzato && t->bDecl )
			{
				//scopeFind(pScope, t->Key, &Dati);

				//************** INIZIO LOAD *******************

				switch ( t->Tipo1 )
				{
				case T_INTEGER:
					//printf("int %d ", t->Value.iVal);

					if ( t->Value.iVal >= -128 && t->Value.iVal <= 127 )
					{
						if ( t->Value.iVal >= -1 && t->Value.iVal <= 5 )
						{
							if ( t->Value.iVal == -1 )
								bytecode = jvm_iconst_m1;
							else if ( t->Value.iVal == 0 )
								bytecode = jvm_iconst_0;
							else if ( t->Value.iVal == 1 )
								bytecode = jvm_iconst_1;
							else if ( t->Value.iVal == 2 )
								bytecode = jvm_iconst_2;
							else if ( t->Value.iVal == 3 )
								bytecode = jvm_iconst_3;
							else if ( t->Value.iVal == 4 )
								bytecode = jvm_iconst_4;
							else if ( t->Value.iVal == 5 )
								bytecode = jvm_iconst_5;

							memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
							g_max_stack++;
							g_code_length++;
							g_IndexBytecodeArray++;
						}
						else
						{
							bytecode = jvm_bipush;
							memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
							g_max_stack++;
							g_code_length++;
							g_IndexBytecodeArray++;

							paramS1 = (S1)t->Value.iVal;
							memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramS1, 1);
							g_code_length++;
							g_IndexBytecodeArray++;
						}
					}
					else if ( t->Value.iVal >= -32768 && t->Value.iVal <= 32767 )
					{
						bytecode = jvm_sipush;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						g_max_stack++;
						g_code_length++;
						g_IndexBytecodeArray++;

						paramS2 = (S2)t->Value.iVal;
						if ( !isBigEndian() )
						{
							ARRAY_TO_S2(buff, paramS2);
						}
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramS2, 2);
						g_code_length += 2;
						g_IndexBytecodeArray += 2;
					}
					else
					{
						sprintf(szInt, "%d", t->Value.iVal);
						if ( !jvmhtFind(pHT, szInt, (unsigned short*)&constIntegerIndex) )
						{
							constIntegerIndex = (unsigned char)(g_ConstantPoolCount + 1);
							jvmhtInsert(pHT, szInt, constIntegerIndex);
							g_IndexConstantPool = jvmAddIntegerConstant(t->Value.iVal, g_IndexConstantPool);
						}

						bytecode = jvm_ldc;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						g_max_stack++;
						g_code_length++;
						g_IndexBytecodeArray++;

						paramU1 = constIntegerIndex;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
						g_code_length++;
						g_IndexBytecodeArray++;
					}

					break;
				case T_REAL:
					//printf("real %lf ", t->Value.dblVal);

					if ( t->Value.dblVal == 0.0 || t->Value.dblVal == 1.0 )
					{
						if ( t->Value.dblVal == 0.0 )
							bytecode = jvm_dconst_0;
						else if ( t->Value.dblVal == 1.0 )
							bytecode = jvm_dconst_1;

						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						//g_max_stack++;
						g_max_stack += 2;
						g_code_length++;
						g_IndexBytecodeArray++;
					}
					else
					{
						sprintf(szReal, "%lf", t->Value.dblVal);
						if ( !jvmhtFind(pHT, szReal, &constRealIndex) )
						{
							constRealIndex = (unsigned short)(g_ConstantPoolCount + 1);
							jvmhtInsert(pHT, szReal, constRealIndex);
							g_IndexConstantPool = jvmAddRealConstant(t->Value.dblVal, g_IndexConstantPool);
						}

						bytecode = jvm_ldc2_w;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						//g_max_stack++;
						g_max_stack += 2;
						g_code_length++;
						g_IndexBytecodeArray++;

						paramU2 = constRealIndex;
						if ( !isBigEndian() )
						{
							ARRAY_TO_U2(buff, paramU2);
						}
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
						g_code_length += 2;
						g_IndexBytecodeArray += 2;
					}

					break;
				case T_BOOLEAN:
					//if ( t->Value.bVal )
					//	printf("bool true ");
					//else
					//	printf("bool false ");

					//printf("\nLOAD BOOLEAN\n");
					//printf("value  -> %s\n", t->Value.bVal ? "true" : "false");
					//printf("value2 -> %s\n", t->Value.iVal ? "true" : "false");

					if ( t->Value.bVal == 0 )
					{
						bytecode = jvm_iconst_0;
						//printf("value -> false\n");
					}
					else if ( t->Value.bVal == 1 )
					{
						bytecode = jvm_iconst_1;
						//printf("value -> true\n");
					}

					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_max_stack++;
					g_code_length++;
					g_IndexBytecodeArray++;

					break;
				case T_STRING:
					//printf("string '%s' ", t->Value.strVal);

					if ( !jvmhtFind(pHT, t->Value.strVal, (unsigned short*)&constStringIndex) )
					{
						constStringIndex = (unsigned char)(g_ConstantPoolCount + 1);
						jvmhtInsert(pHT, t->Value.strVal, constStringIndex);
						g_IndexConstantPool = jvmAddStringConstant(t->Value.strVal, g_IndexConstantPool);
					}

					bytecode = jvm_ldc;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_max_stack++;
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU1 = constStringIndex;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					break;
				}
				//************** FINE LOAD *******************


				//************** INIZIO STORE ****************
				if (Dati.frameIndex >= 0 && Dati.frameIndex <= 3)
				{
					if ( (U1)Dati.frameIndex == 0 )
					{
						if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							bytecode = jvm_istore_0;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_dstore_0;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_astore_0;
					}
					else if ( (U1)Dati.frameIndex == 1 )
					{
						if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							bytecode = jvm_istore_1;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_dstore_1;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_astore_1;
					}
					else if ( (U1)Dati.frameIndex == 2 )
					{
						if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							bytecode = jvm_istore_2;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_dstore_2;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_astore_2;
					}
					else if ( (U1)Dati.frameIndex == 3 )
					{
						if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							bytecode = jvm_istore_3;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_dstore_3;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_astore_3;
					}

					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					if ( Dati.Tipo1 != T_REAL )
						g_max_stack--;
					else
						g_max_stack -= 2;
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else
				{
					if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
						bytecode = jvm_istore;
					else if ( Dati.Tipo1 == T_REAL )
						bytecode = jvm_dstore;
					else if ( Dati.Tipo1 == T_STRING )
						bytecode = jvm_astore;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					if ( Dati.Tipo1 != T_REAL )
						g_max_stack--;
					else
						g_max_stack -= 2;
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU1 = (U1)Dati.frameIndex;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				//************** FINE STORE ***************
			}
			else if ( t->Tipo0 == T_ARRAY )
			{
				MakeBytecodeDeclArrayMain(pHT, &Dati);
			}

			t = t->next;
		}
	}
}

void LoadArray(nodeType *p, jvmHashTable **pHT, Scope *pScope)
{
	U1 bytecode;
	U1 paramU1;

	int k;

	//if ( p->opr.op[0]->type0 == T_ARRAY )
	if ( p->type0 == T_ARRAY )
	{
		HashTable Dati;
		scopeFind(pScope, p->id.name, &Dati);

		if ( Dati.frameIndex >= 0 && Dati.frameIndex <= 3 )
		{
			if ( (U1)Dati.frameIndex == 0 )
				bytecode = jvm_aload_0;
			else if ( (U1)Dati.frameIndex == 1 )
				bytecode = jvm_aload_1;
			else if ( (U1)Dati.frameIndex == 2 )
				bytecode = jvm_aload_2;
			else if ( (U1)Dati.frameIndex == 3 )
				bytecode = jvm_aload_3;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
			g_max_stack++;
			g_code_length++;
			g_IndexBytecodeArray++;
		}
		else
		{
			bytecode = jvm_aload;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
			g_max_stack++;
			g_code_length++;
			g_IndexBytecodeArray++;
			paramU1 = (U1)Dati.frameIndex;
			memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
			g_code_length++;
			g_IndexBytecodeArray++;
		}

		MakeBytecodeMain(p->id.value.pArrayDef->indexes[0], pHT, pScope);

		if ( p->id.value.pArrayDef->countIndexes > 1 )
		{
			for ( k = 1; k < p->id.value.pArrayDef->countIndexes; k++ )
			{
				bytecode = jvm_aaload;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				if ( p->id.value.pArrayDef )
					MakeBytecodeMain(p->id.value.pArrayDef->indexes[k], pHT, pScope);
			}
		}
	}
}

void jvmTypeCheck(int t1, int t2)
{
	U1 bytecode;

	if ( t1 == t2 )
		return;

	switch ( t1 )
	{
	case T_INTEGER:
		if ( t2 == T_REAL )
			bytecode = jvm_d2i;
		break;
	case T_REAL:
		if ( t2 == T_INTEGER || t2 == T_BOOLEAN )
			bytecode = jvm_i2d;
		break;
	case T_BOOLEAN:
		if ( t2 == T_REAL )
			bytecode = jvm_d2i;
		break;
	}

	memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
	g_code_length++;
	g_IndexBytecodeArray++;
}

void MakeBytecodeMain(nodeType *p, jvmHashTable **pHT, Scope *pScope)
{
	U2 constStringIndex;
	U2 constIntegerIndex;
	U2 constRealIndex;

	U1 bytecode = 0;
	U1 paramU1;
	U2 paramU2;

	S1 paramS1;
	S2 paramS2;

	U2 segnaposto1, segnaposto2;

	static int bLeftSide = 0;

	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

    if (!p)
	{
		return;
	}

    switch(p->type)
	{
    case typeConst:
		{
			switch( p->type1 )
			{
			case T_INTEGER:
				if ( p->con.value.iVal >= -128 && p->con.value.iVal <= 127 )
				{
					if ( p->con.value.iVal >= -1 && p->con.value.iVal <= 5 )
					{
						if ( p->con.value.iVal == -1 )
							bytecode = jvm_iconst_m1;
						else if ( p->con.value.iVal == 0 )
							bytecode = jvm_iconst_0;
						else if ( p->con.value.iVal == 1 )
							bytecode = jvm_iconst_1;
						else if ( p->con.value.iVal == 2 )
							bytecode = jvm_iconst_2;
						else if ( p->con.value.iVal == 3 )
							bytecode = jvm_iconst_3;
						else if ( p->con.value.iVal == 4 )
							bytecode = jvm_iconst_4;
						else if ( p->con.value.iVal == 5 )
							bytecode = jvm_iconst_5;

						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						g_max_stack++;
						g_code_length++;
						g_IndexBytecodeArray++;
					}
					else
					{
						bytecode = jvm_bipush;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						g_max_stack++;
						g_code_length++;
						g_IndexBytecodeArray++;

						paramS1 = (S1)p->con.value.iVal;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramS1, 1);
						g_code_length++;
						g_IndexBytecodeArray++;
					}
				}
				else if ( p->con.value.iVal >= -32768 && p->con.value.iVal <= 32767 )
				{
					bytecode = jvm_sipush;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_max_stack++;
					g_code_length++;
					g_IndexBytecodeArray++;

					paramS2 = (S2)p->con.value.iVal;
					if ( !isBigEndian() )
					{
						ARRAY_TO_S2(buff, paramS2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramS2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;
				}
				else
				{
					if ( !jvmhtFind(pHT, p->con.lexeme, (unsigned short*)&constIntegerIndex) )
					{
						constIntegerIndex = (unsigned char)(g_ConstantPoolCount + 1);
						jvmhtInsert(pHT, p->con.lexeme, constIntegerIndex);
						g_IndexConstantPool = jvmAddIntegerConstant(p->con.value.iVal, g_IndexConstantPool);
					}

					bytecode = jvm_ldc;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_max_stack++;
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU1 = constIntegerIndex;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				break;
			case T_REAL:
				if ( p->con.value.dblVal == 0.0 || p->con.value.dblVal == 1.0 )
				{
					if ( p->con.value.dblVal == 0.0 )
						bytecode = jvm_dconst_0;
					else if ( p->con.value.dblVal == 1.0 )
						bytecode = jvm_dconst_1;

					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					//g_max_stack++;
					g_max_stack += 2;
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else
				{
					if ( !jvmhtFind(pHT, p->con.lexeme, &constRealIndex) )
					{
						constRealIndex = (unsigned short)(g_ConstantPoolCount + 1);
						jvmhtInsert(pHT, p->con.lexeme, constRealIndex);
						g_IndexConstantPool = jvmAddRealConstant(p->con.value.dblVal, g_IndexConstantPool);
					}

					bytecode = jvm_ldc2_w;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					//g_max_stack++;
					g_max_stack += 2;
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = constRealIndex;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;
				}

				break;
			case T_BOOLEAN:
				if ( p->con.value.bVal == 0 )
					bytecode = jvm_iconst_0;
				else if ( p->con.value.bVal == 1 )
					bytecode = jvm_iconst_1;

				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				break;
			case T_STRING:
				if ( !jvmhtFind(pHT, p->con.value.strVal, (unsigned short*)&constStringIndex) )
				{
					constStringIndex = (unsigned char)(g_ConstantPoolCount + 1);
					jvmhtInsert(pHT, p->con.value.strVal, constStringIndex);
					g_IndexConstantPool = jvmAddStringConstant(p->con.value.strVal, g_IndexConstantPool);
				}

				bytecode = jvm_ldc;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU1 = constStringIndex;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				break;
			}

			return;
		}
		break;
    case typeId:
		{
			HashTable Dati;
			if ( scopeFind(pScope, p->id.name, &Dati) )
			{
				if ( Dati.Tipo0 == T_ARRAY )
				{
					if ( bLeftSide )
					{
						if ( Dati.Tipo1 == T_INTEGER )
							bytecode = jvm_iastore;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_dastore;
						else if ( Dati.Tipo1 == T_BOOLEAN )
							bytecode = jvm_bastore;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_aastore;
					}
					else
					{
						if ( Dati.Tipo1 == T_INTEGER )
							bytecode = jvm_iaload;
						else if ( Dati.Tipo1 == T_REAL )
							bytecode = jvm_daload;
						else if ( Dati.Tipo1 == T_BOOLEAN )
							bytecode = jvm_baload;
						else if ( Dati.Tipo1 == T_STRING )
							bytecode = jvm_aaload;
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					if ( bLeftSide )
					{
						if ( Dati.Tipo1 != T_REAL )
							g_max_stack--;
						else
							g_max_stack -= 2;
					}
					else
					{
						if ( Dati.Tipo1 != T_REAL )
							g_max_stack++;
						else
							g_max_stack += 2;
					}
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else // Dati.Tipo0 == T_SIMPLE
				{
					if (Dati.frameIndex >= 0 && Dati.frameIndex <= 3)
					{
						if ( (U1)Dati.frameIndex == 0 )
						{
							if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							{
								if ( bLeftSide )
									bytecode = jvm_istore_0;
								else
									bytecode = jvm_iload_0;
							}
							else if ( Dati.Tipo1 == T_REAL )
							{
								if ( bLeftSide )
									bytecode = jvm_dstore_0;
								else
									bytecode = jvm_dload_0;
							}
							else if ( Dati.Tipo1 == T_STRING )
							{
								if ( bLeftSide )
									bytecode = jvm_astore_0;
								else
									bytecode = jvm_aload_0;
							}
						}
						else if ( (U1)Dati.frameIndex == 1 )
						{
							if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							{
								if ( bLeftSide )
									bytecode = jvm_istore_1;
								else
									bytecode = jvm_iload_1;
							}
							else if ( Dati.Tipo1 == T_REAL )
							{
								if ( bLeftSide )
									bytecode = jvm_dstore_1;
								else
									bytecode = jvm_dload_1;
							}
							else if ( Dati.Tipo1 == T_STRING )
							{
								if ( bLeftSide )
									bytecode = jvm_astore_1;
								else
									bytecode = jvm_aload_1;
							}
						}
						else if ( (U1)Dati.frameIndex == 2 )
						{
							if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							{
								if ( bLeftSide )
									bytecode = jvm_istore_2;
								else
									bytecode = jvm_iload_2;
							}
							else if ( Dati.Tipo1 == T_REAL )
							{
								if ( bLeftSide )
									bytecode = jvm_dstore_2;
								else
									bytecode = jvm_dload_2;
							}
							else if ( Dati.Tipo1 == T_STRING )
							{
								if ( bLeftSide )
									bytecode = jvm_astore_2;
								else
									bytecode = jvm_aload_2;
							}
						}
						else if ( (U1)Dati.frameIndex == 3 )
						{
							if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
							{
								if ( bLeftSide )
									bytecode = jvm_istore_3;
								else
									bytecode = jvm_iload_3;
							}
							else if ( Dati.Tipo1 == T_REAL )
							{
								if ( bLeftSide )
									bytecode = jvm_dstore_3;
								else
									bytecode = jvm_dload_3;
							}
							else if ( Dati.Tipo1 == T_STRING )
							{
								if ( bLeftSide )
									bytecode = jvm_astore_3;
								else
									bytecode = jvm_aload_3;
							}
						}

						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						if ( bLeftSide )
						{
							if ( Dati.Tipo1 != T_REAL )
								g_max_stack--;
							else
								g_max_stack -= 2;
						}
						else
						{
							if ( Dati.Tipo1 != T_REAL )
								g_max_stack++;
							else
								g_max_stack += 2;
						}
						g_code_length++;
						g_IndexBytecodeArray++;
					}
					else
					{
						if ( Dati.Tipo1 == T_INTEGER || Dati.Tipo1 == T_BOOLEAN)
						{
							if ( bLeftSide )
								bytecode = jvm_istore;
							else
								bytecode = jvm_iload;
						}
						else if ( Dati.Tipo1 == T_REAL )
						{
							if ( bLeftSide )
								bytecode = jvm_dstore;
							else
								bytecode = jvm_dload;
						}
						else if ( Dati.Tipo1 == T_STRING )
						{
							if ( bLeftSide )
								bytecode = jvm_astore;
							else
								bytecode = jvm_aload;
						}
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
						if ( bLeftSide )
						{
							if ( Dati.Tipo1 != T_REAL )
								g_max_stack--;
							else
								g_max_stack -= 2;
						}
						else
						{
							if ( Dati.Tipo1 != T_REAL )
								g_max_stack++;
							else
								g_max_stack += 2;
						}
						g_code_length++;
						g_IndexBytecodeArray++;

						paramU1 = (U1)Dati.frameIndex;
						memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU1, 1);
						g_code_length++;
						g_IndexBytecodeArray++;
					}
				}
			}
			else
			{
				printf("Errore: X identificatore '%s' non dichiarato\n", p->id.name);
			}

			return;
		}
		break;
    case typeOperator:
		{
			switch(p->opr.oper)
			{
			case T_KW_WHILE:
				segnaposto1 = g_IndexBytecodeArray;
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_if_icmpeq;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto2 = g_IndexBytecodeArray;

				paramU2 = 0;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				MakeBytecodeMain(p->opr.op[1], pHT, pScope);

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = segnaposto1 - g_IndexBytecodeArray + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);

				return;
			case T_KW_DO: // do-while
				segnaposto1 = g_IndexBytecodeArray;
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_if_icmpeq;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto2 = g_IndexBytecodeArray;

				paramU2 = 0;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				MakeBytecodeMain(p->opr.op[0], pHT, pScope);

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = segnaposto1 - g_IndexBytecodeArray + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);

				return;
			case T_KW_IF:
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);

				if (p->opr.nops > 2)
				{
					// if else
					bytecode = jvm_iconst_0;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_if_icmpeq;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
					segnaposto2 = g_IndexBytecodeArray;

					paramU2 = 0;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					MakeBytecodeMain(p->opr.op[1], pHT, pScope);

					bytecode = jvm_goto;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
					segnaposto1 = g_IndexBytecodeArray;

					paramU2 = 0;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);

					MakeBytecodeMain(p->opr.op[2], pHT, pScope);

					paramU2 = g_IndexBytecodeArray - segnaposto1 + 1;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + segnaposto1, &paramU2, 2);
				}
				else
				{
					// if
					bytecode = jvm_iconst_0;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_if_icmpeq;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
					segnaposto2 = g_IndexBytecodeArray;

					paramU2 = 0;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					MakeBytecodeMain(p->opr.op[1], pHT, pScope);

					paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);
				}

				return;
			case T_KW_PRINT:
				bytecode = jvm_getstatic;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 9; ////Field java/lang/System.out:Ljava/io/PrintStream;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				LoadArray(p->opr.op[0], pHT, pScope);

				MakeBytecodeMain(p->opr.op[0], pHT, pScope);

				bytecode = jvm_invokevirtual;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				if ( p->opr.op[0]->type1 == T_STRING )
					paramU2 = 14; //  java/io/PrintStream.print:(Ljava/lang/String;)V
				else if ( p->opr.op[0]->type1 == T_INTEGER )
					paramU2 = 60; //  java/io/PrintStream.print:(I)V
				else if ( p->opr.op[0]->type1 == T_REAL )
					paramU2 = 63; //  java/io/PrintStream.print:(D)V
				else if ( p->opr.op[0]->type1 == T_BOOLEAN )
					paramU2 = 66; //  java/io/PrintStream.print:(Z)V
				else
				{
					if (g_myType1 == T_INTEGER)
						paramU2 = 60; //  java/io/PrintStream.print:(I)V
					else if (g_myType1 == T_REAL)
						paramU2 = 63; //  java/io/PrintStream.print:(D)V
				}
				
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				return;
			case T_KW_READ:
				bytecode = jvm_invokestatic;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_max_stack++;
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 15; // java/lang/System.console:()Ljava/io/Console;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_invokevirtual;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 16; // java/io/Console.readLine:()Ljava/lang/String;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				if ( p->opr.op[0]->type1 != T_STRING )
				{
					bytecode = jvm_invokestatic;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					if ( p->opr.op[0]->type1 == T_INTEGER )
						paramU2 = 17; // java/lang/Integer.parseInt:(Ljava/lang/String;)I
					else if ( p->opr.op[0]->type1 == T_REAL )
						paramU2 = 18; // java/lang/Double.parseDouble:(Ljava/lang/String;)D
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;
				}

				LoadArray(p->opr.op[0], pHT, pScope);
				bLeftSide = 1;
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				bLeftSide = 0;

				return;
			case T_STMT_LIST:
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				return;
			case T_CODE_BLOCK:
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				return;
			case T_OP_ASSIGN:
				LoadArray(p->opr.op[0], pHT, pScope);
				LoadArray(p->opr.op[1], pHT, pScope);
				bLeftSide = 0;
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				if ( g_myType1 == -1 )
					jvmTypeCheck(p->opr.op[0]->type1, p->opr.op[1]->type1);
				else
					jvmTypeCheck(p->opr.op[0]->type1, g_myType1);
				bLeftSide = 1;
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				bLeftSide = 0;
				g_myType1 = -1;
				return;
			case T_OP_UMINUS:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);

				if ( p->opr.op[0]->type1 == T_INTEGER )
				{
					bytecode = jvm_ineg;
					g_myType1 = T_INTEGER;
				}
				else if ( p->opr.op[0]->type1 == T_REAL )
				{
					bytecode = jvm_dneg;
					g_myType1 = T_REAL;
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_PLUS:
				if ( p->type1 != T_STRING )
				{
					LoadArray(p->opr.op[0], pHT, pScope);
					MakeBytecodeMain(p->opr.op[0], pHT, pScope);
					jvmTypeCheck(p->type1, p->opr.op[0]->type1);

					LoadArray(p->opr.op[1], pHT, pScope);
					MakeBytecodeMain(p->opr.op[1], pHT, pScope);
					jvmTypeCheck(p->type1, p->opr.op[1]->type1);

					if ( p->type1 == T_INTEGER )
						bytecode = jvm_iadd;
					else if ( p->type1 == T_REAL )
						bytecode = jvm_dadd;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else // Concatenazione di due stringhe
				{
					bytecode = jvm_new;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = 6; // class java/lang/StringBuilder
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					bytecode = jvm_dup;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_invokespecial;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = 11; // java/lang/StringBuilder."<init>":()V
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					LoadArray(p->opr.op[0], pHT, pScope);
					MakeBytecodeMain(p->opr.op[0], pHT, pScope);

					bytecode = jvm_invokevirtual;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = 12; // java/lang/StringBuilder.append:(Ljava/lang/String;)Ljava/lang/StringBuilder;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					LoadArray(p->opr.op[1], pHT, pScope);
					MakeBytecodeMain(p->opr.op[1], pHT, pScope);

					bytecode = jvm_invokevirtual;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = 12; // java/lang/StringBuilder.append:(Ljava/lang/String;)Ljava/lang/StringBuilder;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;

					bytecode = jvm_invokevirtual;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					paramU2 = 13; // java/lang/StringBuilder.toString:()Ljava/lang/String;
					if ( !isBigEndian() )
					{
						ARRAY_TO_U2(buff, paramU2);
					}
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
					g_code_length += 2;
					g_IndexBytecodeArray += 2;
				}

				return;
			case T_OP_MINUS:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
					bytecode = jvm_isub;
				else if ( p->type1 == T_REAL )
					bytecode = jvm_dsub;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_MULT:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
					bytecode = jvm_imul;
				else if ( p->type1 == T_REAL )
					bytecode = jvm_dmul;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_DIV:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
					bytecode = jvm_idiv;
				else if ( p->type1 == T_REAL )
					bytecode = jvm_ddiv;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_LT:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmpge;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpg;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_ifge;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_GT:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmple;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpl;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_ifle;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_GE:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmplt;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpl;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_iflt;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_LE:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmpgt;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpl;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_ifgt;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_NE:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmpeq;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpl;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_ifeq;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_EQ:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				if ( p->type1 == T_INTEGER )
				{
					bytecode = jvm_if_icmpne;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}
				else if ( p->type1 == T_REAL )
				{
					bytecode = jvm_dcmpl;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;

					bytecode = jvm_ifne;
					memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
					g_code_length++;
					g_IndexBytecodeArray++;
				}

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_AND:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				bytecode = jvm_ifeq;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto1 = g_IndexBytecodeArray;

				paramU2 = 0;
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				bytecode = jvm_ifeq;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto2 = g_IndexBytecodeArray;

				paramU2 = 0;
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				paramU2 = g_IndexBytecodeArray - segnaposto1 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto1, &paramU2, 2);

				paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_OR:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				bytecode = jvm_ifne;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto1 = g_IndexBytecodeArray;

				paramU2 = 0;
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				LoadArray(p->opr.op[1], pHT, pScope);
				MakeBytecodeMain(p->opr.op[1], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[1]->type1);

				bytecode = jvm_ifeq;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;
				segnaposto2 = g_IndexBytecodeArray;

				paramU2 = 0;
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				paramU2 = g_IndexBytecodeArray - segnaposto1 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto1, &paramU2, 2);

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				paramU2 = g_IndexBytecodeArray - segnaposto2 + 1;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + segnaposto2, &paramU2, 2);

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			case T_OP_NOT:
				LoadArray(p->opr.op[0], pHT, pScope);
				MakeBytecodeMain(p->opr.op[0], pHT, pScope);
				jvmTypeCheck(p->type1, p->opr.op[0]->type1);

				bytecode = jvm_ifne;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 7; // = 6 + 1 ( + 1 perchè comprende il byte dell'istruzione if_icmpge o ifge )
				//paramU2 = -5;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_1;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				bytecode = jvm_goto;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				paramU2 = 4; // = 3 + 1 ( + 1 perchè comprende il byte dell'istruzione goto )
				//paramU2 = -4;
				if ( !isBigEndian() )
				{
					ARRAY_TO_U2(buff, paramU2);
				}
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &paramU2, 2);
				g_code_length += 2;
				g_IndexBytecodeArray += 2;

				bytecode = jvm_iconst_0;
				memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
				g_code_length++;
				g_IndexBytecodeArray++;

				return;
			}
		}
		break;
    }

	//bytecode = jvm_return;
	//memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
	//g_code_length++;

	// return;
}

void jvmStart(char *szInputFileName, nodeType *pTree, Scope *pScope)
{
	FILE *fp;
	U2 constant_pool_count;
	U2 attributes_count = 0;
	//int indexConstantPool;

	U1 bytecode;

	//char *szFileName = "PrintDemo.class";

	char szOutputFileName[512];
	char szFileNameWithoutExt[512];
	char szExt[512];
	char szSourceFile[512];
	char szClassName[512];

	//unsigned char *buff;
	BYTE *buff;
	U1 fb[8]; // Necessario per usare le macro;

	jvmHashTable **pHT;
	int x;

	MakeOutputFileName(szInputFileName, szOutputFileName, szFileNameWithoutExt, szExt);
	strcpy(szSourceFile, szFileNameWithoutExt);
	strcat(szSourceFile, ".");
	strcat(szSourceFile, szExt);

	strcpy(szClassName, szFileNameWithoutExt);

	fp = fopen(szOutputFileName, "wb");
	if ( !fp )
	{
		printf("Errore. Impossibile creare il file %s.\n", szOutputFileName);
		return;
	}

	/*
	pHT = (jvmHashTable**)malloc(sizeof(jvmHashTable*) * JVM_HT_SIZE);
	if ( pHT != NULL )
	{
		for ( x = 0; x < JVM_HT_SIZE; x++ )
		{
			pHT[x] = (jvmHashTable*)malloc(sizeof(jvmHashTable));
			if ( pHT[x] == NULL )
			{
				printf("Memoria non sufficiente.\n");
				return;
			}
			//pHT[x] = NULL;
		}
	}
	else
	{
		printf("Memoria non sufficiente.\n");
		return;
	}
	*/
	
	pHT = (jvmHashTable**)calloc(JVM_HT_SIZE, sizeof(jvmHashTable*));
	if ( pHT == NULL )
	{
		printf("Memoria non sufficiente.\n");
		return;
	}


	jvmWriteHeader(fp);
	g_IndexConstantPool = jvmMakeConstantPool(szSourceFile, szClassName);

	MakeBytecodeDeclInitMain(pHT, pScope);
	MakeBytecodeMain(pTree, pHT, pScope);

	bytecode = jvm_return;
	memcpy(g_ArrayCode + g_IndexBytecodeArray, &bytecode, 1);
	g_code_length++;


	constant_pool_count = g_ConstantPoolCount + 1;
	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, constant_pool_count);
	}
	fwrite(&constant_pool_count, sizeof(U2), 1, fp);
	fwrite(g_ArrayConstantPool, g_IndexConstantPool, 1, fp);

	jvmWriteInfo(fp);
	jvmWriteInitMethod(fp);
	jvmWriteMainMethod(fp, pTree, pHT);

	if ( !isBigEndian() )
	{
		ARRAY_TO_U2(buff, attributes_count);
	}
	fwrite(&attributes_count, sizeof(U2), 1, fp);

	fclose(fp);

	for ( x = 0; x < JVM_HT_SIZE; x++)
		jvmhtFree(pHT[x]);
	free(pHT);
}
