#ifndef __MYPARSER__
#define __MYPARSER__


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

#endif /* __MYPARSER__ */
