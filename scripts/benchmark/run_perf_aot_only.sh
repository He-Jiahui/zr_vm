#!/usr/bin/env bash
# Run performance_report for AOT C and AOT LLVM only (diagnosis / isolation).
# Usage (WSL, from repo root):
#   bash scripts/benchmark/run_perf_aot_only.sh [build-dir]
# Default build-dir: build/benchmark-gcc-release
#
# Optional: include native C baseline for relative_to_c:
#   ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_aot_c,zr_aot_llvm bash scripts/benchmark/run_perf_aot_only.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT}/build/benchmark-gcc-release}"

export ZR_VM_PERF_ONLY_IMPLEMENTATIONS="${ZR_VM_PERF_ONLY_IMPLEMENTATIONS:-zr_aot_c,zr_aot_llvm}"
export ZR_VM_TEST_TIER="${ZR_VM_TEST_TIER:-core}"
export BENCHMARK_DUAL_CTEST="${BENCHMARK_DUAL_CTEST:-0}"

echo "ZR_VM_PERF_ONLY_IMPLEMENTATIONS=${ZR_VM_PERF_ONLY_IMPLEMENTATIONS}"
echo "ZR_VM_TEST_TIER=${ZR_VM_TEST_TIER}"
echo "ctest --test-dir ${BUILD_DIR} -R '^performance_report\$'"

ctest -R '^performance_report$' --test-dir "${BUILD_DIR}" --output-on-failure
