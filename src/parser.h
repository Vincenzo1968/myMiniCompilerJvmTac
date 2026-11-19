#ifndef __MYPARSER__
#define __MYPARSER__

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

int Parse(char *szFileName, nodeType **pTree, int genTAC, int genJVM);

int match(TokenTypeEnum ExpectedToken);

void Panic();

int program();
int decl_list();
int decl();
int type(void);
int var_list();
int code_block();
int stmt_list();
int stmt();
int expr();
int expr1();
int expr2();
int expr3();
int expr4();

#endif /* __MYPARSER__ */
