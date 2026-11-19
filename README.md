# myMiniCompilerJvmTac

An educational compiler written in ANSI/C99 featuring a recursive descent parser, an optimizing Three-Address Code (TAC) intermediate representation (`-O0` through `-O3`), and a native JVM bytecode (`.class`) backend.

## Features

- **Frontend:** Hand-written lexical analyzer (`scanner.c`), recursive descent parser (`parser.c`), AST builder (`ast.c`), and scoped symbol table (`symtab.c`).
- **Data Types & Control Flow:**
  - Primitive types: `int`, `real` (64-bit IEEE 754 `double`), `bool`, and `string`.
  - Multidimensional arrays (`array name[N][M] of type`).
  - Control structures: `if-else`, `while`, `do-while`, compound blocks (`begin ... end`), `read`, and `print`.
  - Short-circuit boolean operators (`&`, `|`, `!`) and relational/arithmetic expressions.
- **Three-Address Code (TAC) & Optimizer (`tac.c`):**
  - `-O0`: Raw TAC generation.
  - `-O1`: AST/TAC constant folding, algebraic simplifications, copy propagation, local dead code elimination, and peephole jump optimizations.
  - `-O2`: Control Flow Graph (CFG) construction, unreachable block elimination, Global Common Subexpression Elimination (CSE) with commutativity, and Global Liveness-based Dead Code Elimination.
  - `-O3`: Strength reduction, jump threading, dominator-tree Loop-Invariant Code Motion (LICM), and bounded loop unrolling.
- **JVM Bytecode Backend (`jvm.c`):**
  - Generates standalone Java `.class` files (Java 6 / major version 50) directly in C without external dependencies, supporting multidimensional arrays (`multianewarray`), console I/O, and string concatenation via `StringBuilder`.

## Language Grammar (EBNF)

```ebnf
program    : decl_list code_block ;
decl_list  : { decl } ;
decl       : type var_list ';'
           | 'array' T_ID '[' T_INT_LITERAL ']' { '[' T_INT_LITERAL ']' } 'of' type ';'
           ;
type       : 'int' | 'real' | 'bool' | 'string' ;
var_list   : T_ID [ '=' expr ] { ',' T_ID [ '=' expr ] } ;
code_block : 'begin' stmt_list 'end' ;
stmt_list  : { stmt } ;
stmt       : 'print' expr ';'
           | 'read' expr ';'
           | 'while' '(' expr ')' stmt
           | 'do' stmt 'while' '(' expr ')'
           | 'if' '(' expr ')' stmt [ 'else' stmt ]
           | 'begin' stmt_list 'end'
           | T_ID { '[' expr ']' } '=' expr ';'
           ;
expr       : expr1 { ( '&' | '|' ) expr1 } ;
expr1      : expr2 { relop expr2 } ;
expr2      : expr3 { addop expr3 } ;
expr3      : expr4 { mulop expr4 } ;
expr4      : T_INT_LITERAL
           | T_REAL_LITERAL
           | T_STRING_LITERAL
           | 'true'
           | 'false'
           | '(' expr ')'
           | '-' expr4
           | '!' expr4
           | T_ID { '[' expr ']' }
           ;
```

## Build

Compile with `gcc`:

```bash
gcc -Wall -W -pedantic -O2 ast.c endianess.c jvm.c main.c parser.c scanner.c symtab.c tac.c -o myCompiler
```

Or using `make`:

```bash
make
```

## Usage

### Generate Three-Address Code (TAC)

```bash
./myCompiler prg_test/test_licm.prg tac -O0
./myCompiler prg_test/test_licm.prg tac -O1
./myCompiler prg_test/test_licm.prg tac -O2
./myCompiler prg_test/test_licm.prg tac -O3
```

### Compile to JVM Bytecode and Run

```bash
./myCompiler prg_test/myprog_01.prg jvm
java myprog_01
```

