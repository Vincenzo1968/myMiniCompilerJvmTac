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

#ifndef ENDIANNESS_H
#define ENDIANNESS_H

#include <stdint.h>

#define BYTE uint8_t

#define S1 int8_t
#define S2 int16_t
#define S4 int32_t
#define S8 int64_t
#define U1 uint8_t
#define U2 uint16_t
#define U4 uint32_t
#define U8 uint64_t

#define F4 float
#define F8 double

#define FORMAT_WORD(arr,start){ fb[0]=arr[start+1];\
		fb[1]=arr[start];\
		arr[start]=fb[0];\
		arr[start+1]=fb[1]; }

#define FORMAT_DWORD(arr,start){ fb[0]=arr[start+3];\
		fb[1]=arr[start+2];\
		fb[2]=arr[start+1];\
		fb[3]=arr[start];\
		arr[start]=fb[0];\
		arr[start+1]=fb[1];\
		arr[start+2]=fb[2];\
		arr[start+3]=fb[3]; }

#define FORMAT_QWORD(arr,start){ fb[0]=arr[start+7];\
		fb[1]=arr[start+6];\
		fb[2]=arr[start+5];\
		fb[3]=arr[start+4];\
		fb[4]=arr[start+3];\
		fb[5]=arr[start+2];\
		fb[6]=arr[start+1];\
		fb[7]=arr[start];\
		arr[start]=fb[0];\
		arr[start+1]=fb[1];\
		arr[start+2]=fb[2];\
		arr[start+3]=fb[3];\
		arr[start+4]=fb[4];\
		arr[start+5]=fb[5];\
		arr[start+6]=fb[6];\
		arr[start+7]=fb[7]; }

#define SwapDouble(inDouble, retVal) \
{ \
	char *doubleToConvert = (char*)&inDouble; \
	char *returnDouble = (char*)retVal; \
\
	returnDouble[0] = doubleToConvert[7]; \
	returnDouble[1] = doubleToConvert[6]; \
	returnDouble[2] = doubleToConvert[5]; \
	returnDouble[3] = doubleToConvert[4]; \
	returnDouble[4] = doubleToConvert[3]; \
	returnDouble[5] = doubleToConvert[2]; \
	returnDouble[6] = doubleToConvert[1]; \
	returnDouble[7] = doubleToConvert[0]; \
}

char isBigEndian();

#endif // ENDIANNESS_H
