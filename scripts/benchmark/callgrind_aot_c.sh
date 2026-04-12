#!/usr/bin/env bash
# Run Callgrind on zr_vm_cli --execution-mode aot_c (after --compile --emit-aot-c).
# Matches suite-style flags: --trace-children=no, optional counting mode (--cache-sim=no --branch-sim=no).
#
# Usage:
#   bash scripts/benchmark/callgrind_aot_c.sh [build_root] [repo_root] [out_base]
# Env:
#   PROJECT=hello_world | numeric_loops   (default hello_world)
#   NUMERIC_SCALE=1                       (only for numeric_loops)
#
# Outputs:
#   ${out_base}.callgrind.out
#   ${out_base}.callgrind.annotate.txt   (top functions by Ir)
set -euo pipefail

BUILD_ROOT="${1:-/mnt/e/Git/zr_vm/build/benchmark-gcc-release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${2:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
OUT_BASE="${3:-/tmp/zr_callgrind_aot_c}"

CLI="${BUILD_ROOT}/bin/zr_vm_cli"
PROJECT="${PROJECT:-hello_world}"
NUMERIC_SCALE="${NUMERIC_SCALE:-1}"

if [[ ! -x "$CLI" ]]; then
    echo "missing CLI: $CLI"
    exit 1
fi

WORKDIR=$(mktemp -d /tmp/zr_callgrind_aot_XXXXXX)
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

if [[ "$PROJECT" == "hello_world" ]]; then
    cp -a "${REPO_ROOT}/tests/fixtures/projects/hello_world/." "$WORKDIR/"
    ZRP="hello_world.zrp"
elif [[ "$PROJECT" == "numeric_loops" ]]; then
    cp -a "${REPO_ROOT}/tests/benchmarks/cases/numeric_loops/zr/." "$WORKDIR/"
    printf 'pub scale(): int {\n    return %s;\n}\n' "${NUMERIC_SCALE}" >"${WORKDIR}/src/bench_config.zr"
    ZRP="benchmark_numeric_loops.zrp"
else
    echo "PROJECT must be hello_world or numeric_loops"
    exit 1
fi

echo "=== prepare aot_c ==="
(cd "$WORKDIR" && "$CLI" --compile "$ZRP" --emit-aot-c)

CALLGRIND_OUT="${OUT_BASE}.callgrind.out"
ANNOTATE_OUT="${OUT_BASE}.callgrind.annotate.txt"
mkdir -p "$(dirname "$CALLGRIND_OUT")"

echo "=== valgrind --tool=callgrind (slow) ==="
echo "CLI cwd=$WORKDIR zrp=$ZRP"
set +e
(cd "$WORKDIR" && valgrind --tool=callgrind \
    --trace-children=no \
    --cache-sim=no \
    --branch-sim=no \
    --callgrind-out-file="${CALLGRIND_OUT}" \
    "$CLI" "$ZRP" --execution-mode aot_c --require-aot-path \
    >/dev/null 2>&1)
VG=$?
set -e
if [[ ! -f "$CALLGRIND_OUT" ]]; then
    echo "callgrind did not write ${CALLGRIND_OUT} (valgrind exit ${VG})"
    exit 1
fi

echo "=== callgrind_annotate (top by Ir) ==="
callgrind_annotate --auto=yes --sort=Ir "${CALLGRIND_OUT}" >"${ANNOTATE_OUT}"

head -n 80 "${ANNOTATE_OUT}"
echo ""
echo "Wrote: ${CALLGRIND_OUT}"
echo "Wrote: ${ANNOTATE_OUT}"
echo ""
echo "Tip: kcachegrind ${CALLGRIND_OUT}  or  callgrind_annotate --include=zraot ${CALLGRIND_OUT}"
