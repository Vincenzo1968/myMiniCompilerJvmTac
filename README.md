# myMiniCompilerJvmTac

An educational compiler written in ANSI/C99 featuring a recursive descent parser, an optimizing Three-Address Code (TAC) intermediate representation (`-O0` through `-O3`), an AST interpreter, and a native JVM bytecode (`.class`) backend driven by the optimized TAC.

## Features

- **Frontend & Memory Safety:** Hand-written lexical analyzer (`scanner.c`), recursive descent parser (`parser.c`) with panic-mode error recovery and trailing-token checks, AST builder (`ast.c`), and scoped symbol table (`symtab.c`) with clean memory management (zero leaks/use-after-free on syntax or semantic errors).
- **Data Types & Control Flow:**
  - Primitive types: `int`, `real` (64-bit IEEE 754 `double`), `bool`, and `string`.
  - Multidimensional arrays (`array name[N][M] of type`).
  - Control structures: `if-else`, `while`, `do-while`, compound blocks (`begin ... end`), `read`, and `print`.
  - Boolean operators (`&`, `|`, `!`) and relational/arithmetic expressions.
- **Three-Address Code (TAC) & Optimizer (`tac.c`):**
  - `-O0`: Raw TAC generation.
  - `-O1`: AST/TAC constant folding, algebraic simplifications, copy propagation, Control Flow Graph (CFG) construction, unreachable block elimination, local & global liveness-based Dead Code Elimination (DCE), and peephole jump optimizations.
  - `-O2`: Global Common Subexpression Elimination (CSE) with commutativity on top of the `-O1` CFG and liveness pipeline.
  - `-O3`: Full loop and control-flow optimization pass featuring Strength Reduction, linear Induction Variable Strength Reduction (IVSR), Jump Threading, dominator-tree Loop-Invariant Code Motion (LICM, including safe invariant division hoisting), and bounded Loop Unrolling.

- **JVM Bytecode Backend (`jvm.c`):**
  - Lowers the optimized TAC IR (`-O3` by default, configurable via `-O0`..`-O3`) directly into standalone Java `.class` files (Java 6 / major version 50) in C with zero external dependencies, supporting linearized multidimensional arrays, console I/O, `ldc`/`ldc_w` constants, and string concatenation via `StringBuilder`.

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

Using Autotools / `make`:

```bash
./configure
make
```

Or compile directly with `gcc`:

```bash
gcc -Wall -Wextra -pedantic -O2 src/ast.c src/endianess.c src/jvm.c src/main.c src/parser.c src/scanner.c src/symtab.c src/tac.c -o src/myCompiler
```

## Usage & Examples

### 1. Run Directly with the Built-in Interpreter

Without `tac` or `jvm`, the compiler executes the program via its AST interpreter:

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

Comparing `-O1` and `-O2`/`-O3` on `prg_test/myprog_02.prg` (which contains a dead `if (1 > 2)` branch and commutative expressions `a * b` and `b * a`):

```bash
src/myCompiler prg_test/myprog_02.prg tac -O1
```

```text
print "Inserisci a: "
read a
print "Inserisci b: "
read b
t2 = a * b
t3 = t2 + 1
t4 = b * a
t5 = t4 + 2
print "Eqque qua:
"
print t3
print t5
```

With `-O2` (and `-O3`), Global CSE recognizes `b * a` as equivalent to `a * b` and reuses `t2`:

```bash
src/myCompiler prg_test/myprog_02.prg tac -O2
```

```text
print "Inserisci a: "
read a
print "Inserisci b: "
read b
t2 = a * b
t3 = t2 + 1
t5 = t2 + 2
print "Eqque qua:
"
print t3
print t5
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

### 4. Syntax & Semantic Error Reporting

The parser reports line/column diagnostics and uses panic-mode recovery to continue checking the rest of the file:

```bash
src/myCompiler prg_test/test_error_recovery.prg tac -O0
```

```text
Linea 6 Colonna 2: Errore stmt. Atteso semicolon ';'.
Linea 8 Colonna 4: Errore expr4. Identificatore 'a' non inizializzato.
Linea 8 Colonna 4: Errore stmt. Atteso operatore '='.
Linea 8 Colonna 5: Errore stmt. Atteso operatore '='.
Linea 8 Colonna 26: Errore stmt. Identificatore 'variabile_sconosciuta' non dichiarato.
Linea 11 Colonna 2: Errore stmt. Non si puo' assegnare una valore di tipo string ad una variabile di tipo int.
Linea 13 Colonna 4: Errore expr4. Identificatore 'a' non inizializzato.
Linea 13 Colonna 4: Errore stmt. Atteso operatore '='.
Linea 14 Colonna 3: Errore stmt. Atteso operatore '='.
Linea 17 Colonna 6: Errore. Token inattesi dopo la fine del programma.
Errori: 10
Warnings: 0.
```
