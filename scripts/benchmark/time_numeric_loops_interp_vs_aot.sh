#!/usr/bin/env bash
# Time numeric_loops: ZR interp vs aot_c (requires existing performance_suite copy + built CLI).
set -euo pipefail
ROOT="${1:-/mnt/e/Git/zr_vm/build/benchmark-gcc-release}"
CLI="${ROOT}/bin/zr_vm_cli"
PROJ="${ROOT}/tests_generated/performance_suite/cases/numeric_loops/zr/benchmark_numeric_loops.zrp"
if [[ ! -x "$CLI" ]]; then
    echo "missing or not executable: $CLI"
    exit 1
fi
if [[ ! -f "$PROJ" ]]; then
    echo "missing project (run ctest -R performance_report once): $PROJ"
    exit 1
fi

run3() {
    local label=$1
    shift
    echo "=== $label ==="
    export TIMEFORMAT='real %3R s'
    for _ in 1 2 3; do
        { time "$@" >/dev/null 2>&1; } 2>&1
    done
}

run3 "ZR interp" "$CLI" "$PROJ"
echo
echo "=== prepare aot_c (zr_vm_cli --compile --emit-aot-c) ==="
export TIMEFORMAT='real %3R s'
{ time "$CLI" --compile "$PROJ" --emit-aot-c >/dev/null 2>&1; } 2>&1
echo
run3 "ZR aot_c" "$CLI" "$PROJ" --execution-mode aot_c --require-aot-path
