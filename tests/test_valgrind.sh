#!/usr/bin/env bash

COMPILER="./myMiniCompilerJvmTac/src/myCompiler"
TEST_DIR="./myMiniCompilerJvmTac/prg_test"
TMPLOG=$(mktemp)
FAIL_COUNT=0
CURRENT_COUNT=0

trap 'rm -f "$TMPLOG"' EXIT

if [ ! -x "$COMPILER" ]; then
    echo "Errore: eseguibile '$COMPILER' non trovato o non eseguibile."
    exit 1
fi

shopt -s nullglob
FILES=("$TEST_DIR"/*.prg)
shopt -u nullglob

NUM_FILES=${#FILES[@]}
if [ "$NUM_FILES" -eq 0 ]; then
    echo "Nessun file .prg trovato in '$TEST_DIR'."
    exit 0
fi

# Ogni file viene testato 12 volte (3 modalità x 4 livelli di ottimizzazione)
TOTAL_RUNS=$((NUM_FILES * 12))

for f in "${FILES[@]}"; do
    base=$(basename "${f%.prg}")

    for mode in tac jvm asm; do
        for opt in 0 1 2 3; do
            CURRENT_COUNT=$((CURRENT_COUNT + 1))
            PERCENT=$((CURRENT_COUNT * 100 / TOTAL_RUNS))

            printf "\r\033[K[%3d%%] (%d/%d) Analisi: %s.prg [%s -O%s]" \
                   "$PERCENT" "$CURRENT_COUNT" "$TOTAL_RUNS" "$base" "$mode" "$opt"

            valgrind --leak-check=full \
                     --show-leak-kinds=all \
                     --errors-for-leak-kinds=all \
                     --log-file="$TMPLOG" \
                     "$COMPILER" "$f" "$mode" -O"$opt" >/dev/null 2>&1

            # Pulizia dei file generati (jvm scrive nella dir corrente, asm accanto al .prg)
            if [ "$mode" = "jvm" ]; then
                rm -f "${base}.class"
            elif [ "$mode" = "asm" ]; then
                rm -f "${f%.prg}.asm"
            fi

            if ! grep -q "ERROR SUMMARY: 0 errors from 0 contexts" "$TMPLOG" || \
               ! grep -q "in use at exit: 0 bytes in 0 blocks" "$TMPLOG"; then
                FAIL_COUNT=$((FAIL_COUNT + 1))
                printf "\n"
                echo "================================================================"
                echo "[PROBLEMA] File: $f | Modalita': $mode | Livello: -O$opt"
                echo "================================================================"
                cat "$TMPLOG"
                echo ""
            fi
        done
    done
done

printf "\r\033[K"

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo 'OK! -> "ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)" SU TUTTI I FILE TESTATI.' "(Totale esecuzioni: $TOTAL_RUNS)"
    exit 0
else
    echo "RILEVATI $FAIL_COUNT CASI PROBLEMATICI su $TOTAL_RUNS esecuzioni."
    exit 1
fi
