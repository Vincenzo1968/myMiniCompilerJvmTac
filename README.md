# myMiniCompilerJvmTac

An educational compiler written in ANSI/C99 featuring a recursive descent parser, an optimizing Three-Address Code (TAC) intermediate representation (`-O0` through `-O3`), an AST interpreter, and dual backends: native JVM bytecode (`.class`) and x86_64 assembly (NASM syntax) for Linux, both driven by the optimized TAC.

## Features

- **Frontend & Memory Safety:** Lexical analyzer (`scanner.c`), recursive descent parser (`parser.c`) with panic-mode error recovery and token checks, AST builder (`ast.c`), and symbol table with clean memory management (zero errors/leaks with Valgrind).
- **Data Types, Functions & Control Flow:**
  - Primitive types (`int`, `real`, `bool`, `string`), multidimensional arrays, user-defined functions (with prototypes, recursion, `return`, pass-by-value and pass-by-reference `ref`), control structures (`if-else`, `while`, `do-while`, `begin ... end`, `read`, `print`), and logical/relational expressions.
- **Structs (`structs.c`):**
  - C-like `struct` types (scalar fields, arrays, nested structs), arrays of structs, whole-struct assignment, and struct parameters/results. Structs are *lowered* at source level before parsing, so every backend and optimization level handles them unchanged.
- **Three-Address Code (TAC) & Optimizer (`tac.c`):**
  - IR generation and multi-level optimization pipeline:
    - `-O0`: Raw TAC.
    - `-O1`: Constant folding, algebraic simplifications, copy propagation, CFG construction, local/global Dead Code Elimination (DCE), and jump optimization.
    - `-O2`: Global Common Subexpression Elimination (CSE) and Global Constant Propagation built on top of `-O1`.
    - `-O3`: Full loop and control-flow optimization (Strength Reduction, IVSR, Jump Threading, LICM, and bounded Loop Unrolling).
- **JVM Bytecode Backend (`jvm.c`):**
  - Directly translates optimized TAC into Java `.class` files (Java 6) with zero external dependencies, supporting static methods, global variables, array wrappers for `ref` parameters, I/O, and string concatenation.
- **x86_64 Assembly Backend (`backend_x86_64.c`):**
  - Generates NASM assembly for Linux x86_64 adhering to the System V AMD64 ABI, supporting functions, arrays, arithmetic, logic, and I/O.

## Language Grammar (EBNF)

```text
program    : top_list code_block ;
top_list   : { struct_def | decl | func_decl } ;
struct_def : 'struct' T_ID '{' { field } '}' ';' ;
field      : type T_ID { ',' T_ID } ';'
           | T_ID T_ID { ',' T_ID } ';'                       (* nested struct *)
           | 'array' T_ID '[' T_INT_LITERAL ']' { '[' T_INT_LITERAL ']' } 'of' ( type | T_ID ) ';'
           ;
decl       : type var_list ';'
           | T_ID T_ID { ',' T_ID } ';'                       (* struct variables *)
           | 'array' T_ID '[' T_INT_LITERAL ']' { '[' T_INT_LITERAL ']' } 'of' ( type | T_ID ) ';'
           ;
func_decl  : ( type | 'void' ) T_ID '(' [ param_list ] ')' ( ';' | func_body ) ;
param_list : param { ',' param } ;
param      : [ 'ref' ] type T_ID
           | 'array' T_ID '[' T_INT_LITERAL ']' { '[' T_INT_LITERAL ']' } 'of' type
           ;
func_body  : 'begin' { decl } stmt_list 'end' ;
type       : 'int' | 'real' | 'bool' | 'string' ;
var_list   : T_ID [ '=' expr ] { ',' T_ID [ '=' expr ] } ;
code_block : 'begin' stmt_list 'end' ;
stmt_list  : { stmt } ;
stmt       : 'print' expr ';'
           | 'read' expr ';'
           | 'while' '(' expr ')' stmt
           | 'do' stmt 'while' '(' expr ')'
           | 'if' '(' expr ')' stmt [ 'else' stmt ]
           | 'return' [ expr ] ';'
           | 'begin' stmt_list 'end'
           | T_ID '(' [ arg_list ] ')' ';'
           | T_ID { '[' expr ']' } '=' expr ';'
           ;
arg_list   : expr { ',' expr } ;
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
           | '-' expr
           | '!' expr
           | T_ID '(' [ arg_list ] ')'
           | T_ID { '[' expr ']' }
           ;
```

## Build

Using Autotools / `make`:

```bash
./configure
make
```

Or compile directly with `gcc`:

```bash
gcc -Wall -Wextra -pedantic -O2 src/ast.c src/context.c src/endianess.c src/jvm.c src/backend_x86_64.c src/main.c src/parser.c src/scanner.c src/symtab.c src/tac.c -o src/myCompiler
```

## Usage & Examples

### 1. Run Directly with the Built-in Interpreter

Without `tac`, `jvm`, or `asm`, the compiler executes the program via its AST interpreter:

```bash
src/myCompiler prg_test/test_licm.prg
```

```text
Inserisci n: 5
Inserisci a: 2
Inserisci b: 3
Eqque qua: 55
```

### 2. Generate Three-Address Code (TAC) (`-O0` .. `-O3`)

Comparing `-O1` and `-O2`/`-O3` on `prg_test/test_all_optimizations.prg`:

```bash
$ myCompiler prg_test/test_all_optimizations.prg tac

t1 = 1 > 2
ifFalse t1 goto L001
c = 999
goto L002
L001:
c = 7
L002:
print c
print "\nPlease enter b: "
read b
a = 10
t2 = b > 5
ifFalse t2 goto L003
x = a
goto L004
L003:
x = a
L004:
print x
print "\n"
i = 0
s = 0
L005:
t3 = i < 4
ifFalse t3 goto L006
t4 = s + i
s = t4
t5 = i + 1
i = t5
goto L005
L006:
print s
print "\n"
$ myCompiler prg_test/test_all_optimizations.prg tac -O1

print 7
print "\nPlease enter b: "
read b
a = 10
t2 = b > 5
ifFalse t2 goto L003
x = a
goto L004
L003:
x = a
L004:
print x
print "\n"
i = 0
s = 0
L005:
t3 = i < 4
ifFalse t3 goto L006
t4 = s + i
s = t4
t5 = i + 1
i = t5
goto L005
L006:
print s
print "\n"
$ myCompiler prg_test/test_all_optimizations.prg tac -O2

print 7
print "\nPlease enter b: "
read b
t2 = b > 5
ifFalse t2 goto L003
goto L004
L003:
L004:
print 10
print "\n"
i = 0
s = 0
L005:
t3 = i < 4
ifFalse t3 goto L006
t4 = s + i
s = t4
t5 = i + 1
i = t5
goto L005
L006:
print s
print "\n"
$ myCompiler prg_test/test_all_optimizations.prg tac -O3

print 7
print "\nPlease enter b: "
read b
t2 = b > 5
ifFalse t2 goto L003
goto L004
L003:
L004:
print 10
print "\n"
print 6
print "\n"
$
```

### 3. Compile to JVM Bytecode (`.class`) and Run

The `jvm` target generates the TAC, optimizes it (`-O3` by default, or an explicit `-O0`..`-O3` flag), and emits `<program>.class`:

```bash
src/myCompiler prg_test/myprog_01.prg jvm && java myprog_01
```

```text
-7.85
```

```bash
src/myCompiler prg_test/myprog_02.prg jvm && java myprog_02
```

```text
Inserisci a: 
5
Inserisci b: 
8
Eqque qua:

41
42
```

```bash
src/myCompiler prg_test/test_licm.prg jvm && java test_licm
```

```text
Inserisci n: 
10
Inserisci a: 
5
Inserisci b: 
3
Eqque qua: 
250
```

You can also choose a specific optimization level when compiling for the JVM:

```bash
src/myCompiler prg_test/myprog_02.prg jvm -O0
```

#### 3.5 Compile to x86_64 Assembly and Run (Linux)

The `asm` target generates NASM-syntax assembly from the optimized TAC (`-O0`..`-O3`):

```bash
src/myCompiler prg_test/test_gcp.prg asm -O2
nasm -f elf64 prg_test/test_gcp.asm -o prg_test/test_gcp.o
gcc -no-pie -o prg_test/test_gcp_exec prg_test/test_gcp.o
./prg_test/test_gcp_exec
rm -f prg_test/test_gcp.asm prg_test/test_gcp.o prg_test/test_gcp_exec
```

##### Assemble with NASM

```bash
nasm -f elf64 prg_test/test_gcp.asm -o prg_test/test_gcp.o
```

##### Link with GCC (use `-no-pie` for simpler addressing)

```bash
gcc -no-pie -o prg_test/test_gcp_exec prg_test/test_gcp.o
```

##### Run the native executable

```bash
./prg_test/test_gcp_exec
```

```text
10
```

### 4. Syntax & Semantic Error Reporting

The parser handles diagnostics by line and column and recovers from errors without generating cascading false positives.

```bash
src/myCompiler prg_test/test_error_recovery.prg
```

```text
Linea 6 Colonna 2: Errore stmt. Atteso semicolon ';'.
Linea 8 Colonna 26: Errore expr4. Identificatore 'variabile_sconosciuta' non dichiarato.
Linea 11 Colonna 2: Errore stmt. Non si puo' assegnare una valore di tipo string ad una variabile di tipo int.
Linea 14 Colonna 3: Errore stmt. Attesa parentesi ')'.
Linea 17 Colonna 6: Errore. Token inattesi dopo la fine del programma.
Errori: 4
Warnings: 0
```

### 5. Global Constant Propagation (`-O2`)

On `prg_test/test_gcp.prg`, the `-O2` optimization propagates constants and simplifies conditional branches.

```bash
src/myCompiler prg_test/test_gcp.prg tac -O1
```

```text
read b
a = 10
t1 = b > 5
ifFalse t1 goto L001
x = a
goto L002
L001:
x = a
L002:
print x
```

```bash
src/myCompiler prg_test/test_gcp.prg tac -O2
```

```text
read b
t1 = b > 5
ifFalse t1 goto L001
goto L002
L001:
L002:
print 10
```

### 6. In-Process Thread-Safety Test (`pthread`)

The entire compiler pipeline is reentrant and thread-safe via per-thread `CompilerContext` instances, verified with Valgrind (`memcheck`, `helgrind`, `drd`).

```bash
gcc -Wall -Wextra -pedantic -O2 -pthread -Isrc \
    prg_test/test_pthread_inproc.c \
    src/ast.o src/context.o src/endianess.o src/jvm.o src/backend_x86_64.o \
    src/parser.o src/scanner.o src/symtab.o src/tac.o \
    -lm -o test_pthread_inproc

./test_pthread_inproc ./prg_test
rm -f test_pthread_inproc *.class
```

### 7. Functions

Test suite covering recursion, mutual recursion, pass-by-value, pass-by-reference (`ref`), array parameters, and interprocedural optimizations:

```bash
src/myCompiler prg_test/test_func_adv.prg
```

```text
3.500000
140
61
5.500000
mutua ok
8
30
10
4
7.000000
ababab
xx-yy
0.000000
2
30
5040
21.000000
```

```bash
src/myCompiler prg_test/test_func_basic.prg
```

```text
120
15
3.000000
Ciao Mondo
Ciao aCiao b
4 pari
7 dispari
14
9
55
2 1
1 2 6 
101
```

```bash
src/myCompiler prg_test/test_func_opt.prg
```

```text
7
2
32 6
90
9 3
50
```

### 8. Structs

Struct types are declared at the outermost level, before the main block. A struct variable is declared as `Name p;`, an array of structs as `array v[10] of Name;`; fields are accessed with `.` (`p.x`, `v[i].pos.y`). Structs can be assigned as a whole (`q = p;`, `v[i] = p;`), passed by value or by `ref`, and returned by functions (`p = f(...);`).

```text
struct Punto { int x; real y; };
struct Seg   { Punto a; Punto b; string nome; };

Punto mid(Punto p, Punto q);
```

Internally every struct variable becomes one scalar per leaf field (`p.x` -> `p$x`) and every array of structs one array per leaf field (`v[i].x` -> `v$x[i]`), so the interpreter, TAC, JVM and x86_64 backends need no struct-specific code. To see the rewritten source set `MYCOMPILER_DUMP_LOWERED=1`:

```bash
MYCOMPILER_DUMP_LOWERED=1 src/myCompiler prg_test/test_struct_basic.prg
src/myCompiler prg_test/test_struct_basic.prg
```

```text
3 13
8 1.500000
13
seg 13 1.500000
7
13 1.500000
```

Current limits: structs cannot be initialized in their declaration or compared as a whole; a struct with array fields is passed only with `ref` and cannot be returned; a struct-returning function is used only as `p = f(...);`; array elements cannot be passed by `ref`.
