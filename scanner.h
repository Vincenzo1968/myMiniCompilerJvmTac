#ifndef __MYSCANNER__
#define __MYSCANNER__

typedef enum tagTokenType
{
	T_ERROR,
	T_UNKNOWN,
	T_EOF,
	T_ID,
	//T_CHAR_LITERAL,
	T_STRING_LITERAL,
	T_INT_LITERAL,
	T_REAL_LITERAL,
	T_OPAREN,		// '('
	T_CPAREN,		// ')'
	T_QOPAREN,		// '['
	T_QCPAREN,		// ']'
	//T_GOPAREN,		// '{'
	//T_GCPAREN,		// '}'
	//T_COLON,		// ':'
	T_SEMICOLON,	// ';'
	//T_DOT,			// '.'
	T_COMMA,		// ','
	T_KW_IF,
	T_KW_ELSE,
	T_KW_WHILE,
	T_KW_DO,
	T_KW_BEGIN,
	T_KW_END,
	T_KW_ARRAY,
	T_KW_OF,
	T_KW_STRING,
	T_KW_INT,
	T_KW_REAL,
	T_KW_BOOL,
	T_KW_FALSE,
	T_KW_TRUE,
	T_KW_PRINT,
	T_KW_READ,
	T_OP_ASSIGN,	// '='
	T_OP_AND,		// '&'
	T_OP_OR,		// '|'
	T_OP_NOT,		// '!'
	T_OP_UMINUS,	// '-'
	T_OP_LT,		// '<'
	T_OP_LE,		// '<='
	T_OP_EQ,		// '=='
	T_OP_NE,		// '!='
	T_OP_GT,		// '>'
	T_OP_GE,		// '>='
	T_OP_PLUS,		// '+'
	T_OP_MINUS,		// '-'
	T_OP_MULT,		// '*'
	T_OP_DIV,		// '/'
	T_STMT_LIST,
	T_CODE_BLOCK
}TokenTypeEnum;

typedef struct tagToken
{
	TokenTypeEnum Type;
	union tagValue
	{
		char vChar;
		int vInt;
		double vDouble;
		char *vString;
	} Value;

	char lexeme[128]; // usato solo per le costanti numeriche.

	int line;
	int column;
}Token;

typedef enum tagStates
{
	S0,
	S1,
	S2,
	S3,
	S4,
	S5,
	S6,
	S7,
	S8,
	S9,
	S10,
	S11,
	S12,
	S13,
	S14,
	S15,
	S16,
	S17,
	S18,
	S19,
	S20,
	S21
} States;

int GetDepth();
void SetDepth(int depth);
int LeggiFile(char *szFileName);
void FreeInputbuffer();

void GetNextToken(Token *pToken);

#endif /* __MYSCANNER__ */
