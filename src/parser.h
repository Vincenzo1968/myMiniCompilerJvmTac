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

#ifndef MYPARSER_H
#define MYPARSER_H

struct CompilerContext;
typedef struct CompilerContext CompilerContext;
#include "scanner.h"
#include "ast.h"

/*
typedef enum tagNonTerminal
{
	eSyntax,
	eSyntaxRule,
	eDefinitionsList,
	eSingleDefinition,
	ePrimary,
	eOptionalSequence,
	eRepeatedSequence,
	eGroupedSequence
}NonTerminal;
*/

int Parse(CompilerContext *ctx, char *szFileName, nodeType **pTree, int genTAC, int genJVM);

int match(CompilerContext *ctx, TokenTypeEnum ExpectedToken);

void Panic(CompilerContext *ctx);

int program(CompilerContext *ctx);
int decl_list(CompilerContext *ctx);
int decl(CompilerContext *ctx);
int type(CompilerContext *ctx);
int var_list(CompilerContext *ctx);
int code_block(CompilerContext *ctx);
int stmt_list(CompilerContext *ctx);
int stmt(CompilerContext *ctx);
int expr(CompilerContext *ctx);
int expr1(CompilerContext *ctx);
int expr2(CompilerContext *ctx);
int expr3(CompilerContext *ctx);
int expr4(CompilerContext *ctx);

#endif // MYPARSER_H
