#!/usr/bin/env bash
set -euo pipefail

TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$TESTS_DIR/../ac-compiler/ac"
EXPECTED_DIR="$TESTS_DIR/expected"
CACHE_DIR="$TESTS_DIR/ac-cache"

pass=0
fail=0
skip=0

run_test() {
    local ac_file="$1"
    local name
    name="$(basename "$ac_file" .ac)"
    local expected="$EXPECTED_DIR/$name.txt"

    if [[ ! -f "$expected" ]]; then
        echo "  SKIP  $name  (no expected output)"
        ((skip++)) || true
        return
    fi

    local py_file="$TESTS_DIR/$name.py"
    "$COMPILER" "$ac_file" --target PY --force >/dev/null 2>&1

    local actual
    actual="$(python3 "$py_file" 2>&1)"

    if diff -q <(echo "$actual") "$expected" >/dev/null 2>&1; then
        echo "  PASS  $name"
        ((pass++)) || true
    else
        echo "  FAIL  $name"
        echo "    expected: $(cat "$expected")"
        echo "    actual:   $actual"
        ((fail++)) || true
    fi
}

echo "=== AC test suite ==="

for f in "$TESTS_DIR"/test_*.ac; do
    run_test "$f"
done

echo ""
echo "Results: $pass passed, $fail failed, $skip skipped"
[[ $fail -eq 0 ]]
