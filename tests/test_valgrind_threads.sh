#!/usr/bin/env bash

ROOT_DIR="$(pwd)"
REPO_DIR="$ROOT_DIR/myMiniCompilerJvmTac"
SRC_DIR="$REPO_DIR/src"
PRG_DIR="$REPO_DIR/prg_test"
TEST_SRC="$PRG_DIR/test_pthread_inproc.c"
TEST_BIN="$PRG_DIR/test_pthread_inproc"

TMPLOG=$(mktemp)
TMPWORK=$(mktemp -d)
TMPSUPP=$(mktemp)
FAIL_COUNT=0
CURRENT_COUNT=0

REPEATS=3
TOOLS=(memcheck helgrind drd)
TOTAL_RUNS=$((${#TOOLS[@]} * REPEATS))

trap 'rm -rf "$TMPLOG" "$TMPWORK" "$TMPSUPP"' EXIT

if [ ! -d "$PRG_DIR" ] || [ ! -f "$TEST_SRC" ]; then
    echo "Errore: '$TEST_SRC' non trovato."
    exit 1
fi

# Verifica se serve (ri)compilare test_pthread_inproc dentro prg_test
NEED_BUILD=0
if [ ! -x "$TEST_BIN" ] || [ "$TEST_SRC" -nt "$TEST_BIN" ]; then
    NEED_BUILD=1
else
    for f in "$SRC_DIR"/*.c "$SRC_DIR"/*.h; do
        if [ -f "$f" ] && [ "$f" -nt "$TEST_BIN" ]; then
            NEED_BUILD=1
            break
        fi
    done
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    echo "Compilazione di test_pthread_inproc in $PRG_DIR..."
    (
        cd "$PRG_DIR" || exit 1
        SRC_FILES=()
        for f in ../src/*.c; do
            [ "$(basename "$f")" = "main.c" ] && continue
            SRC_FILES+=("$f")
        done

        gcc -std=c11 -O2 -g -pthread test_pthread_inproc.c "${SRC_FILES[@]}" -o test_pthread_inproc
    )
    if [ $? -ne 0 ]; then
        echo "Errore durante la compilazione di test_pthread_inproc."
        exit 1
    fi
fi

printf '%s\n' \
    '{' \
    '   glibc_printf_buffer_write_race' \
    '   Helgrind:Race' \
    '   ...' \
    '   fun:__printf_buffer_write' \
    '}' > "$TMPSUPP"

for run in $(seq 1 "$REPEATS"); do
    for tool in "${TOOLS[@]}"; do
        CURRENT_COUNT=$((CURRENT_COUNT + 1))
        PERCENT=$((CURRENT_COUNT * 100 / TOTAL_RUNS))

        printf "\r\033[K[%3d%%] (%d/%d) Analisi thread [tool=%s | run=%d/%d]" \
               "$PERCENT" "$CURRENT_COUNT" "$TOTAL_RUNS" "$tool" "$run" "$REPEATS"

        if [ "$tool" = "memcheck" ]; then
            (
                cd "$TMPWORK" && \
                valgrind --tool=memcheck \
                         --leak-check=full \
                         --show-leak-kinds=all \
                         --errors-for-leak-kinds=all \
                         --log-file="$TMPLOG" \
                         "$TEST_BIN" "$PRG_DIR" >/dev/null 2>&1
            )
            PROG_RC=$?
            rm -f "$TMPWORK"/*.class "$TMPWORK"/*.asm

            if [ "$PROG_RC" -ne 0 ] || \
               ! grep -q "ERROR SUMMARY: 0 errors from 0 contexts" "$TMPLOG" || \
               ! grep -q "in use at exit: 0 bytes in 0 blocks" "$TMPLOG"; then
                FAIL_COUNT=$((FAIL_COUNT + 1))
                printf "\n"
                echo "================================================================"
                echo "[PROBLEMA] Tool: $tool | Run: $run | ExitCode: $PROG_RC"
                echo "================================================================"
                cat "$TMPLOG"
                echo ""
            fi
        else
            EXTRA_OPTS=()
            if [ "$tool" = "helgrind" ]; then
                EXTRA_OPTS=(--suppressions="$TMPSUPP")
            fi

            (
                cd "$TMPWORK" && \
                valgrind --tool="$tool" \
                         "${EXTRA_OPTS[@]}" \
                         --log-file="$TMPLOG" \
                         "$TEST_BIN" "$PRG_DIR" >/dev/null 2>&1
            )
            PROG_RC=$?
            rm -f "$TMPWORK"/*.class "$TMPWORK"/*.asm

            if [ "$PROG_RC" -ne 0 ] || \
               ! grep -q "ERROR SUMMARY: 0 errors from 0 contexts" "$TMPLOG"; then
                FAIL_COUNT=$((FAIL_COUNT + 1))
                printf "\n"
                echo "================================================================"
                echo "[PROBLEMA] Tool: $tool | Run: $run | ExitCode: $PROG_RC"
                echo "================================================================"
                cat "$TMPLOG"
                echo ""
            fi
        fi
    done
done

printf "\r\033[K"

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo 'OK! -> "ERROR SUMMARY: 0 errors from 0 contexts" SU TUTTI I TEST THREAD (memcheck, helgrind, drd).' "(Totale esecuzioni: $TOTAL_RUNS)"
    exit 0
else
    echo "RILEVATI $FAIL_COUNT CASI PROBLEMATICI su $TOTAL_RUNS esecuzioni."
    exit 1
fi
