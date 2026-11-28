#!/usr/bin/env bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

DEFAULT_COMPILER="/usr/local/bin/myCompiler"
DEFAULT_PRG_DIR="$PROJECT_ROOT/prg_test"
DEFAULT_OUTPUT_DIR="$PROJECT_ROOT/output"

COMPILER="$DEFAULT_COMPILER"
PRG_DIR="$DEFAULT_PRG_DIR"
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"

usage() {
    cat <<EOF
Uso: $(basename "$0") [-c COMPILER] [-p PRG_DIR] [-o OUTPUT_DIR]

Default:
  COMPILER   = $DEFAULT_COMPILER
  PRG_DIR    = $DEFAULT_PRG_DIR
  OUTPUT_DIR = $DEFAULT_OUTPUT_DIR

Esempi:
  $(basename "$0")
  $(basename "$0") -c /usr/local/bin/myCompiler
  $(basename "$0") -p ./prg_test -o ./output
  $(basename "$0") /usr/local/bin/myCompiler ./prg_test
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        -c|--compiler)
            [ $# -ge 2 ] || { usage; exit 1; }
            COMPILER="$2"
            shift 2
            ;;
        -p|--prg-dir)
            [ $# -ge 2 ] || { usage; exit 1; }
            PRG_DIR="$2"
            shift 2
            ;;
        -o|--output-dir)
            [ $# -ge 2 ] || { usage; exit 1; }
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            echo "Opzione sconosciuta: $1" >&2
            usage
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

# Supporto anche a parametri posizionali:
#   script [COMPILER] [PRG_DIR]
if [ $# -ge 1 ]; then
    COMPILER="$1"
    shift
fi

if [ $# -ge 1 ]; then
    PRG_DIR="$1"
    shift
fi

if [ $# -gt 0 ]; then
    usage
    exit 1
fi

# Risolve il compilatore: se non contiene '/', lo cerca nel PATH.
if [[ "$COMPILER" != */* ]]; then
    if ! COMPILER="$(command -v "$COMPILER" 2>/dev/null)"; then
        echo "Errore: eseguibile '$COMPILER' non trovato nel PATH." >&2
        exit 1
    fi
elif [ ! -x "$COMPILER" ]; then
    echo "Errore: eseguibile '$COMPILER' non trovato o non eseguibile." >&2
    exit 1
fi

if [ ! -d "$PRG_DIR" ]; then
    echo "Errore: directory dei programmi di test '$PRG_DIR' non trovata." >&2
    exit 1
fi

PRG_DIR="$(cd -- "$PRG_DIR" && pwd)"

mkdir -p -- "$OUTPUT_DIR" || {
    echo "Errore: impossibile creare la directory '$OUTPUT_DIR'." >&2
    exit 1
}
OUTPUT_DIR="$(cd -- "$OUTPUT_DIR" && pwd)"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "Errore: valgrind non trovato nel PATH." >&2
    exit 1
fi

TMPLOG=$(mktemp) || exit 1
trap 'rm -f "$TMPLOG"' EXIT

FAIL_COUNT=0
CURRENT_COUNT=0

shopt -s nullglob
FILES=("$PRG_DIR"/*.prg)
shopt -u nullglob

NUM_FILES=${#FILES[@]}
if [ "$NUM_FILES" -eq 0 ]; then
    echo "Nessun file .prg trovato in '$PRG_DIR'."
    exit 0
fi

# Ogni file viene testato 12 volte: 3 modalità x 4 livelli di ottimizzazione.
TOTAL_RUNS=$((NUM_FILES * 12))

for f in "${FILES[@]}"; do
    base=$(basename "${f%.prg}")

    for mode in tac jvm asm; do
        for opt in 0 1 2 3; do
            CURRENT_COUNT=$((CURRENT_COUNT + 1))
            PERCENT=$((CURRENT_COUNT * 100 / TOTAL_RUNS))

            printf "\r\033[K[%3d%%] (%d/%d) Analisi: %s.prg [%s -O%s]" \
                   "$PERCENT" "$CURRENT_COUNT" "$TOTAL_RUNS" "$base" "$mode" "$opt"

            # Copia il .prg dentro output/, così anche eventuali file generati
            # "accanto al sorgente" finiscono in output/.
            RUN_PRG="$OUTPUT_DIR/${base}.prg"

            rm -f -- \
                "$RUN_PRG" \
                "$OUTPUT_DIR/${base}.class" \
                "$OUTPUT_DIR/${base}.asm"

            cp -- "$f" "$RUN_PRG"
            # library files for #include (*.inc) must sit next to the copied program
            cp -f -- "$PRG_DIR"/*.inc "$OUTPUT_DIR"/ 2>/dev/null || true

            (
                cd "$OUTPUT_DIR" || exit 1
                valgrind --leak-check=full \
                         --show-leak-kinds=all \
                         --errors-for-leak-kinds=all \
                         --log-file="$TMPLOG" \
                         "$COMPILER" "$RUN_PRG" "$mode" -O"$opt" >/dev/null 2>&1
            )

            # Pulizia dei file generati durante il test.
            rm -f -- \
                "$RUN_PRG" \
                "$OUTPUT_DIR/${base}.class" \
                "$OUTPUT_DIR/${base}.asm"

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
    echo 'OK! -> "ERROR SUMMARY: 0 errors from 0 contexts" SU TUTTI I FILE TESTATI.' \
         "(Totale esecuzioni: $TOTAL_RUNS)"
    exit 0
else
    echo "RILEVATI $FAIL_COUNT CASI PROBLEMATICI su $TOTAL_RUNS esecuzioni."
    exit 1
fi
