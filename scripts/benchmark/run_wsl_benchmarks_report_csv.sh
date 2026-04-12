#!/usr/bin/env bash
# Run cross-language benchmarks via CMake ctest (performance_report) and emit CSV speed reports.
# Intended for WSL/Linux; from Windows:  wsl -e bash /mnt/e/Git/zr_vm/scripts/benchmark/run_wsl_benchmarks_report_csv.sh
#
# Default: two ctest passes —
#   1) ZR_VM_TEST_TIER=profile + ZR_VM_PERF_CALLGRIND_COUNTING=1 (Callgrind instruction-counting mode);
#      output directory is renamed to tests_generated/performance_profile_callgrind/
#   2) timing pass: ZR_VM_TEST_TIER restored (default core), Callgrind counting off; writes tests_generated/performance/
# CSV / benchmark_suite_summary.json use pass (2) only.
#
# Usage:
#   ./scripts/benchmark/run_wsl_benchmarks_report_csv.sh [CMAKE_BUILD_DIR]
#
# If CMAKE_BUILD_DIR is omitted, the script searches for CMakeCache.txt under the repo in this order:
#   $BUILD_DIR, $ZR_VM_CMAKE_BUILD_DIR, then: build/benchmark-gcc-release, build/benchmark-clang-release,
#   build, cmake-build-debug, cmake-build-release, out/build.
#
# Environment (optional):
#   ZR_VM_TEST_TIER=smoke|core|stress|profile   used for the timing pass (2); default core if unset
#   ZR_VM_PERF_WARMUP, ZR_VM_PERF_ITERATIONS     (see tests/benchmarks/README.md)
#   BENCHMARK_DUAL_CTEST=0                      run a single ctest only (current env; no callgrind pass)
#   BUILD_DIR or ZR_VM_CMAKE_BUILD_DIR           CMake binary dir when not passed as $1
#   BENCHMARK_CSV_SKIP_CTEST=1                    only run CSV + aggregate (needs existing performance/)

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
py_tool="${script_dir}/benchmark_reports_to_csv.py"
aggregate_tool="${script_dir}/aggregate_benchmark_summary.py"

zr_vm_resolve_cmake_build_dir() {
  local repo="$1"
  local explicit="$2"
  local tried=()
  local d
  local candidates=()

  if [[ -n "${explicit}" ]]; then
    candidates+=("${explicit}")
  fi
  if [[ -n "${BUILD_DIR:-}" ]]; then
    candidates+=("${BUILD_DIR}")
  fi
  if [[ -n "${ZR_VM_CMAKE_BUILD_DIR:-}" ]]; then
    candidates+=("${ZR_VM_CMAKE_BUILD_DIR}")
  fi
  candidates+=(
    "${repo}/build/benchmark-gcc-release"
    "${repo}/build/benchmark-clang-release"
    "${repo}/build"
    "${repo}/cmake-build-debug"
    "${repo}/cmake-build-release"
    "${repo}/out/build"
  )

  for d in "${candidates[@]}"; do
    tried+=("${d}")
    if [[ ! -d "${d}" ]]; then
      continue
    fi
    if [[ -f "${d}/CMakeCache.txt" ]]; then
      (cd "${d}" && pwd)
      return 0
    fi
  done

  echo "error: no CMake build directory with CMakeCache.txt found." >&2
  echo "Tried:" >&2
  for d in "${tried[@]}"; do
    echo "  - ${d}" >&2
  done
  echo "hint: configure first, e.g. cmake -S ${repo} -B ${repo}/build && cmake --build ${repo}/build" >&2
  echo "hint: or pass the build dir: $0 /path/to/cmake/binary/dir" >&2
  return 1
}

build_dir="$(zr_vm_resolve_cmake_build_dir "${repo_root}" "${1:-}")" || exit 1

if ! command -v ctest >/dev/null 2>&1; then
  echo "error: ctest not found in PATH" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "error: python3 not found in PATH" >&2
  exit 1
fi

report_dir="${build_dir}/tests_generated/performance"
tests_generated_root="${build_dir}/tests_generated"
callgrind_archive_dir="${tests_generated_root}/performance_profile_callgrind"
timing_tier="${ZR_VM_TEST_TIER:-core}"

echo "Unit Test - WSL benchmark suite + CSV export"
echo "Testing benchmark CSV export:
  repo_root=${repo_root}
  build_dir=${build_dir}
  timing_pass_tier=${timing_tier}
  BENCHMARK_DUAL_CTEST=${BENCHMARK_DUAL_CTEST:-1}
  report_dir=${report_dir}"

start_ts=$(date +%s)
if [[ "${BENCHMARK_CSV_SKIP_CTEST:-0}" == "1" ]]; then
  ctest_rc=0
elif [[ "${BENCHMARK_DUAL_CTEST:-1}" == "1" ]]; then
  echo "---- pass 1/2: profile tier + Callgrind instruction counting (ZR_VM_PERF_CALLGRIND_COUNTING=1)"
  (
    cd "${build_dir}"
    export ZR_VM_TEST_TIER=profile
    export ZR_VM_PERF_CALLGRIND_COUNTING=1
    ctest -R '^performance_report$' --output-on-failure
  )
  rc_a=$?
  if [[ "${rc_a}" -ne 0 ]]; then
    end_ts=$(date +%s)
    elapsed=$((end_ts - start_ts))
    echo "Fail - Cost Time:${elapsed}(s) - WSL benchmark CSV export:
 pass 1 (profile + callgrind counting) failed (exit ${rc_a})"
    exit "${rc_a}"
  fi
  if [[ -d "${report_dir}" ]]; then
    rm -rf "${callgrind_archive_dir}"
    mv "${report_dir}" "${callgrind_archive_dir}"
  else
    echo "error: expected ${report_dir} after pass 1" >&2
    exit 1
  fi

  echo "---- pass 2/2: timing pass (tier=${timing_tier}, callgrind counting off)"
  (
    cd "${build_dir}"
    export ZR_VM_TEST_TIER="${timing_tier}"
    unset ZR_VM_PERF_CALLGRIND_COUNTING
    ctest -R '^performance_report$' --output-on-failure
  )
  ctest_rc=$?
else
  (
    cd "${build_dir}"
    ctest -R '^performance_report$' --output-on-failure
  )
  ctest_rc=$?
fi
end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

if [[ "${ctest_rc}" -ne 0 ]]; then
  echo "Fail - Cost Time:${elapsed}(s) - WSL benchmark CSV export:
 ctest performance_report failed (exit ${ctest_rc})"
  exit "${ctest_rc}"
fi

if [[ ! -f "${report_dir}/benchmark_report.json" ]]; then
  echo "error: missing ${report_dir}/benchmark_report.json (run ctest first or fix build_dir)" >&2
  exit 1
fi

python3 "${py_tool}" --report-dir "${report_dir}"

if [[ -d "${tests_generated_root}" ]]; then
  python3 "${aggregate_tool}" \
    --tests-generated "${tests_generated_root}" \
    --bundle-html "${tests_generated_root}/benchmark_compare_embedded.html"
  if [[ "${BENCHMARK_DUAL_CTEST:-1}" == "1" && -d "${callgrind_archive_dir}" ]]; then
    python3 "${aggregate_tool}" \
      --tests-generated "${tests_generated_root}" \
      --performance-subdir performance_profile_callgrind \
      --out-json "${tests_generated_root}/benchmark_suite_summary_callgrind.json" \
      --skip-viewer-json
  fi
fi

echo "Pass - Cost Time:${elapsed}(s) - WSL benchmark CSV export
  timings (pass 2): ${report_dir}/benchmark_speed_timings.csv
  zr_interp vs langs: ${report_dir}/zr_interp_vs_languages.csv (if comparison_report.json present)
  summary json: ${tests_generated_root}/benchmark_suite_summary.json
  html viewer data: ${tests_generated_root}/benchmark_html_viewer.json
  html embedded (double-click): ${tests_generated_root}/benchmark_compare_embedded.html
  viewer template: ${script_dir}/benchmark_compare_viewer.html"
if [[ "${BENCHMARK_DUAL_CTEST:-1}" == "1" && -d "${callgrind_archive_dir}" ]]; then
  echo "  callgrind profile pass (pass 1): ${callgrind_archive_dir}/
  summary json (callgrind): ${tests_generated_root}/benchmark_suite_summary_callgrind.json"
fi
echo "----"
