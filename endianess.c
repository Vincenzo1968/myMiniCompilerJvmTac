#include "endianess.h"

/*
ritorna 1 se la macchina è big-endian, 0 se little-endian
*/
char isBigEndian()
{
	unsigned short i = 0xABCD;
	unsigned char *buff;

	buff = (unsigned char*)&i;

	if( buff[0] == 0xAB )
		return 1;

	return 0;
}
