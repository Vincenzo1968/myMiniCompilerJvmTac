#!/usr/bin/env bash
set -uo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FAIL=0
"$SCRIPT_DIR/test_valgrind.sh"         || FAIL=$((FAIL + 1))
"$SCRIPT_DIR/test_valgrind_threads.sh" || FAIL=$((FAIL + 1))
echo
if [ "$FAIL" -eq 0 ]; then
    echo "TUTTI I TEST OK"
    exit 0
else
    echo "$FAIL SUITE FALLITE"
    exit 1
fi
