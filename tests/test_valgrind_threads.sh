#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

DEFAULT_PRG_DIR="$PROJECT_ROOT/prg_test"
DEFAULT_OUTPUT_DIR="$PROJECT_ROOT/output"

PRG_DIR="$DEFAULT_PRG_DIR"
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"
TEST_SRC_INPUT=""

usage() {
    cat <<EOF
Uso: $(basename "$0") [-p PRG_DIR] [-o OUTPUT_DIR] [-t TEST_SRC]

Default:
  PRG_DIR    = $DEFAULT_PRG_DIR
  OUTPUT_DIR = $DEFAULT_OUTPUT_DIR
  TEST_SRC   = \$PRG_DIR/test_pthread_inproc.c
               (fallback: \$OUTPUT_DIR/test_pthread_inproc.c)

Il sorgente .c viene copiato in \$OUTPUT_DIR/test_pthread_inproc.c
e il binario compilato in \$OUTPUT_DIR/test_pthread_inproc.
L'eseguibile viene poi lanciato da \$OUTPUT_DIR.

Esempi:
  $(basename "$0")
  $(basename "$0") -t ./prg_test/test_pthread_inproc.c
  $(basename "$0") -p ./prg_test -o ./output
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        -p|--prg-dir)    [ $# -ge 2 ] || { usage; exit 1; }; PRG_DIR="$2";      shift 2 ;;
        -o|--output-dir) [ $# -ge 2 ] || { usage; exit 1; }; OUTPUT_DIR="$2";   shift 2 ;;
        -t|--test-src)   [ $# -ge 2 ] || { usage; exit 1; }; TEST_SRC_INPUT="$2"; shift 2 ;;
        -h|--help)       usage; exit 0 ;;
        --)              shift; break ;;
        -*)              echo "Opzione sconosciuta: $1" >&2; usage; exit 1 ;;
        *)               break ;;
    esac
done
if [ $# -ge 1 ]; then PRG_DIR="$1"; shift; fi
if [ $# -gt 0 ]; then usage; exit 1; fi

# --- validazione PRG_DIR ---
if [ ! -d "$PRG_DIR" ]; then
    echo "Errore: directory dei programmi di test '$PRG_DIR' non trovata." >&2
    exit 1
fi
PRG_DIR="$(cd -- "$PRG_DIR" && pwd)"

# --- OUTPUT_DIR: crea se non esiste ---
mkdir -p -- "$OUTPUT_DIR" || {
    echo "Errore: impossibile creare la directory '$OUTPUT_DIR'." >&2
    exit 1
}
OUTPUT_DIR="$(cd -- "$OUTPUT_DIR" && pwd)"

# --- SRC_DIR ---
SRC_DIR="$PROJECT_ROOT/src"
if [ ! -d "$SRC_DIR" ]; then
    echo "Errore: directory sorgenti '$SRC_DIR' non trovata." >&2
    exit 1
fi

# --- determina il sorgente .c ---
TEST_SRC=""
if [ -n "$TEST_SRC_INPUT" ]; then
    if [[ "$TEST_SRC_INPUT" = /* ]]; then
        TEST_SRC="$TEST_SRC_INPUT"
    else
        TEST_SRC="$(cd -- "$(dirname -- "$TEST_SRC_INPUT")" 2>/dev/null && pwd)/$(basename -- "$TEST_SRC_INPUT")"
        [ -z "$TEST_SRC" ] && TEST_SRC="$TEST_SRC_INPUT"
    fi
elif [ -f "$PRG_DIR/test_pthread_inproc.c" ]; then
    TEST_SRC="$PRG_DIR/test_pthread_inproc.c"
elif [ -f "$OUTPUT_DIR/test_pthread_inproc.c" ]; then
    TEST_SRC="$OUTPUT_DIR/test_pthread_inproc.c"
fi

if [ -z "$TEST_SRC" ] || [ ! -f "$TEST_SRC" ]; then
    echo "Errore: sorgente 'test_pthread_inproc.c' non trovato." >&2
    echo "" >&2
    echo "Posizioni controllate:" >&2
    echo "  1) $PRG_DIR/test_pthread_inproc.c" >&2
    echo "  2) $OUTPUT_DIR/test_pthread_inproc.c" >&2
    [ -n "$TEST_SRC_INPUT" ] && echo "  3) $TEST_SRC_INPUT (da -t)" >&2
    echo "" >&2
    echo "Contenuto di PRG_DIR:" >&2
    ls -la "$PRG_DIR" >&2
    echo "" >&2
    echo "Suggerimenti:" >&2
    echo "  - Recupera il file (git fsck --lost-found / git log --all / Trash / backup)" >&2
    echo "  - Oppure passalo esplicitamente: $0 -t /percorso/test_pthread_inproc.c" >&2
    exit 1
fi

# --- copia il .c in OUTPUT_DIR ---
DEST_SRC="$OUTPUT_DIR/test_pthread_inproc.c"
if [ "$TEST_SRC" != "$DEST_SRC" ]; then
    cp -f -- "$TEST_SRC" "$DEST_SRC" || {
        echo "Errore: impossibile copiare '$TEST_SRC' in '$DEST_SRC'." >&2
        exit 1
    }
fi
TEST_BIN="$OUTPUT_DIR/test_pthread_inproc"

# --- tools ---
command -v valgrind >/dev/null 2>&1 || { echo "Errore: valgrind non trovato nel PATH." >&2; exit 1; }
command -v gcc      >/dev/null 2>&1 || { echo "Errore: gcc non trovato nel PATH." >&2;      exit 1; }

# --- rebuild se necessario ---
NEED_BUILD=0
if [ ! -x "$TEST_BIN" ] || [ "$DEST_SRC" -nt "$TEST_BIN" ]; then
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
    echo "Compilazione di test_pthread_inproc in $OUTPUT_DIR..."

    SRC_FILES=()
    for f in "$SRC_DIR"/*.c; do
        [ "$(basename "$f")" = "main.c" ] && continue
        SRC_FILES+=("$f")
    done

    if ! (
        cd "$OUTPUT_DIR" || exit 1
        gcc -std=c11 -O2 -g -pthread \
            -I"$PROJECT_ROOT" \
            -I"$SRC_DIR" \
            "$DEST_SRC" "${SRC_FILES[@]}" \
            -o "$TEST_BIN"
    ); then
        echo "Errore durante la compilazione di test_pthread_inproc." >&2
        exit 1
    fi
fi

# --- temporanei ---
TMPLOG=$(mktemp)                                    || exit 1
TMPSUPP=$(mktemp)                                   || exit 1
TMPWORK=$(mktemp -d "$OUTPUT_DIR/threads_work.XXXXXX") || exit 1

# shellcheck disable=SC2317  # invocata indirettamente da trap
cleanup() { rm -rf -- "$TMPLOG" "$TMPSUPP" "$TMPWORK"; }
trap cleanup EXIT

printf '%s\n' \
    '{' \
    '   glibc_printf_buffer_write_race' \
    '   Helgrind:Race' \
    '   ...' \
    '   fun:__printf_buffer_write' \
    '}' > "$TMPSUPP"

FAIL_COUNT=0
CURRENT_COUNT=0
REPEATS=3
TOOLS=(memcheck helgrind drd)
TOTAL_RUNS=$(( ${#TOOLS[@]} * REPEATS ))

for run in $(seq 1 "$REPEATS"); do
    for tool in "${TOOLS[@]}"; do
        CURRENT_COUNT=$((CURRENT_COUNT + 1))
        PERCENT=$((CURRENT_COUNT * 100 / TOTAL_RUNS))

        printf "\r\033[K[%3d%%] (%d/%d) Analisi thread [tool=%s | run=%d/%d]" \
               "$PERCENT" "$CURRENT_COUNT" "$TOTAL_RUNS" "$tool" "$run" "$REPEATS"

        rm -rf -- "$TMPWORK"
        mkdir -p -- "$TMPWORK"

        EXTRA_OPTS=()
        if [ "$tool" = "memcheck" ]; then
            EXTRA_OPTS=(--leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all)
        elif [ "$tool" = "helgrind" ]; then
            EXTRA_OPTS=(--suppressions="$TMPSUPP")
        fi

        (
            cd "$TMPWORK" || exit 1
            valgrind --tool="$tool" \
                     "${EXTRA_OPTS[@]}" \
                     --log-file="$TMPLOG" \
                     "$TEST_BIN" "$PRG_DIR" >/dev/null 2>&1
        )
        PROG_RC=$?

        rm -f -- "$TMPWORK"/*.class "$TMPWORK"/*.asm 2>/dev/null || true

        BAD=0
        [ "$PROG_RC" -ne 0 ] && BAD=1
        grep -q "ERROR SUMMARY: 0 errors from 0 contexts" "$TMPLOG" || BAD=1
        if [ "$tool" = "memcheck" ]; then
            grep -q "in use at exit: 0 bytes in 0 blocks" "$TMPLOG" || BAD=1
        fi

        if [ "$BAD" -eq 1 ]; then
            FAIL_COUNT=$((FAIL_COUNT + 1))
            printf "\n"
            echo "================================================================"
            echo "[PROBLEMA] Tool: $tool | Run: $run | ExitCode: $PROG_RC"
            echo "================================================================"
            cat "$TMPLOG"
            echo ""
        fi
    done
done

printf "\r\033[K"

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo 'OK! -> "ERROR SUMMARY: 0 errors from 0 contexts" SU TUTTI I TEST THREAD ' \
         '(memcheck, helgrind, drd).' "(Totale esecuzioni: $TOTAL_RUNS)"
    exit 0
else
    echo "RILEVATI $FAIL_COUNT CASI PROBLEMATICI su $TOTAL_RUNS esecuzioni."
    exit 1
fi
