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

#include "context.h"
#include "endianess.h"

/*
ritorna 1 se la macchina è big-endian, 0 se little-endian
*/
char isBigEndian(CompilerContext *ctx)
{
	(void)ctx;
	unsigned short i = 0xABCD;
	unsigned char *buff;

	buff = (unsigned char*)&i;

	if( buff[0] == 0xAB )
		return 1;

	return 0;
}
