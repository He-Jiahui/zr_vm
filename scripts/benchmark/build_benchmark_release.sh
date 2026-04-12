#!/usr/bin/env bash
# Configure and build zr_vm in Release for benchmark runs (ctest performance_report).
# Output directory: build/benchmark-<gcc|clang>-release
#
# Usage:
#   ./scripts/benchmark/build_benchmark_release.sh gcc|clang [extra cmake --build args...]
#
# Environment:
#   ZR_VM_BUILD_JOBS  parallel jobs (default: nproc or 8)

set -euo pipefail

toolchain="${1:-}"
shift || true

if [[ "${toolchain}" != "gcc" && "${toolchain}" != "clang" ]]; then
  echo "usage: $0 gcc|clang [cmake --build passthrough...]" >&2
  echo "note: for MSVC on Windows, run: pwsh ./scripts/benchmark/build_benchmark_release.ps1 -Toolchain msvc" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
build_dir="${repo_root}/build/benchmark-${toolchain}-release"

if command -v ninja >/dev/null 2>&1; then
  generator="Ninja"
else
  generator="Unix Makefiles"
fi

jobs="${ZR_VM_BUILD_JOBS:-}"
if [[ -z "${jobs}" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc)"
  else
    jobs="8"
  fi
fi

case "${toolchain}" in
  gcc)
    export CC=gcc
    export CXX=g++
    ;;
  clang)
    export CC=clang
    export CXX=clang++
    ;;
esac

echo "Unit Test - Benchmark Release build (${toolchain})"
echo "Testing configure + build:
  repo_root=${repo_root}
  build_dir=${build_dir}
  generator=${generator}
  CMAKE_BUILD_TYPE=Release
  jobs=${jobs}"

start_ts=$(date +%s)

cmake -S "${repo_root}" -B "${build_dir}" \
  -G "${generator}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CC}" \
  -DCMAKE_CXX_COMPILER="${CXX}" \
  -DBUILD_TESTS=ON

cmake --build "${build_dir}" --parallel "${jobs}" "$@"

end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

echo "Pass - Cost Time:${elapsed}(s) - Benchmark Release build (${toolchain})"
echo "  ZR_VM_CMAKE_BUILD_DIR=${build_dir}"
echo "  next: ZR_VM_CMAKE_BUILD_DIR=${build_dir} ./scripts/benchmark/run_wsl_benchmarks_report_csv.sh"
echo "----"
