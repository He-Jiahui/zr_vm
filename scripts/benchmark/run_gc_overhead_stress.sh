#!/usr/bin/env bash
# Run the GC overhead paired benchmarks directly via run_performance_suite.cmake
# so stress-tier GC-only measurements are not cut off by the CTest
# performance_report TIMEOUT=1800 property.
#
# Usage (WSL/Linux, from repo root):
#   bash scripts/benchmark/run_gc_overhead_stress.sh [build-dir]
#
# Default build-dir: build/benchmark-gcc-release
# Default implementations:
#   c,zr_interp,zr_binary,zr_aot_c,zr_aot_llvm
#
# Outputs:
#   <build-dir>/tests_generated/performance/gc_overhead_report.md
#   <build-dir>/tests_generated/performance/gc_overhead_report.json

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT}/build/benchmark-gcc-release}"

CLI_EXE="${BUILD_DIR}/bin/zr_vm_cli"
PERF_RUNNER_EXE="${BUILD_DIR}/bin/zr_vm_perf_runner"
NATIVE_BENCHMARK_EXE="${BUILD_DIR}/bin/zr_vm_native_benchmark_runner"
PERF_SCRIPT="${ROOT}/tests/cmake/run_performance_suite.cmake"
BENCHMARKS_DIR="${ROOT}/tests/benchmarks"
GENERATED_DIR="${BUILD_DIR}/tests_generated"

export ZR_VM_TEST_TIER="${ZR_VM_TEST_TIER:-stress}"
export ZR_VM_PERF_ONLY_CASES="${ZR_VM_PERF_ONLY_CASES:-gc_fragment_baseline,gc_fragment_stress}"
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS="${ZR_VM_PERF_ONLY_IMPLEMENTATIONS:-c,zr_interp,zr_binary,zr_aot_c,zr_aot_llvm}"

echo "ZR_VM_TEST_TIER=${ZR_VM_TEST_TIER}"
echo "ZR_VM_PERF_ONLY_CASES=${ZR_VM_PERF_ONLY_CASES}"
echo "ZR_VM_PERF_ONLY_IMPLEMENTATIONS=${ZR_VM_PERF_ONLY_IMPLEMENTATIONS}"
echo "cmake -DCLI_EXE=${CLI_EXE} -DPERF_RUNNER_EXE=${PERF_RUNNER_EXE} -DNATIVE_BENCHMARK_EXE=${NATIVE_BENCHMARK_EXE} -DBENCHMARKS_DIR=${BENCHMARKS_DIR} -DGENERATED_DIR=${GENERATED_DIR} -DHOST_BINARY_DIR=${BUILD_DIR} -DTIER=${ZR_VM_TEST_TIER} -P ${PERF_SCRIPT}"

cmake \
  "-DCLI_EXE=${CLI_EXE}" \
  "-DPERF_RUNNER_EXE=${PERF_RUNNER_EXE}" \
  "-DNATIVE_BENCHMARK_EXE=${NATIVE_BENCHMARK_EXE}" \
  "-DBENCHMARKS_DIR=${BENCHMARKS_DIR}" \
  "-DGENERATED_DIR=${GENERATED_DIR}" \
  "-DHOST_BINARY_DIR=${BUILD_DIR}" \
  "-DTIER=${ZR_VM_TEST_TIER}" \
  -P "${PERF_SCRIPT}"
