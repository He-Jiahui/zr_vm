#!/usr/bin/env bash
# Configure and build zr_vm in Release for benchmark runs using a full source /
# toolchain cache identity. Only the final keyed directory is reused by WSL runs.
#
# Usage:
#   ./scripts/benchmark/build_benchmark_release.sh gcc|clang [extra cmake --build args...]
#
# Environment:
#   ZR_VM_BUILD_JOBS       parallel jobs (default: nproc or 8)
#   ZR_VM_BENCHMARK_CACHE_ROOT  cache root (default: ${HOME}/.cache/zr-vm-benchmark)

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
cache_root="${ZR_VM_BENCHMARK_CACHE_ROOT:-${HOME}/.cache/zr-vm-benchmark}"
bootstrap_root="${cache_root}/.bootstrap-${toolchain}-$$"

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
echo "Testing configure + keyed build:
  repo_root=${repo_root}
  cache_root=${cache_root}
  bootstrap_dir=${bootstrap_root}
  generator=${generator}
  CMAKE_BUILD_TYPE=Release
  jobs=${jobs}"

start_ts=$(date +%s)

mkdir -p "${cache_root}"
rm -rf "${bootstrap_root}"
cmake -S "${repo_root}" -B "${bootstrap_root}" \
  -G "${generator}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CC}" \
  -DCMAKE_CXX_COMPILER="${CXX}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DBUILD_TESTS=ON

compiler_version="$(${CC} --version | head -n 1)"
compiler_target="$(printf '' | "${CC}" -dumpmachine 2>/dev/null || printf 'unknown-target')"
cache_identity="${bootstrap_root}/benchmark_cache_identity.json"
python3 "${script_dir}/benchmark_environment_contract.py" cache-key \
  --repo-root "${repo_root}" \
  --build-dir "${bootstrap_root}" \
  --toolchain "${toolchain}" \
  --compiler-path "$(command -v "${CC}")" \
  --compiler-version "${compiler_version}" \
  --compiler-target "${compiler_target}" \
  --generator "${generator}" \
  --configuration Release \
  --flag CMAKE_C_FLAGS="${CMAKE_C_FLAGS:-}" \
  --flag CMAKE_C_FLAGS_RELEASE="${CMAKE_C_FLAGS_RELEASE:--O3 -DNDEBUG}" \
  --flag CMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS:-}" \
  --flag CMAKE_EXE_LINKER_FLAGS_RELEASE="${CMAKE_EXE_LINKER_FLAGS_RELEASE:-}" \
  --flag CMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS:-}" \
  --flag CMAKE_SHARED_LINKER_FLAGS_RELEASE="${CMAKE_SHARED_LINKER_FLAGS_RELEASE:-}" \
  --output "${cache_identity}"

source_key="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["source_key"])' "${cache_identity}")"
toolchain_key="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["toolchain_key"])' "${cache_identity}")"
build_dir="${cache_root}/${source_key}/${toolchain_key}"

if [[ -e "${build_dir}" ]]; then
  if [[ "${ZR_VM_BENCHMARK_REUSE_CACHE:-1}" != "1" ]]; then
    echo "error: keyed benchmark cache already exists and reuse is disabled: ${build_dir}" >&2
    rm -rf "${bootstrap_root}"
    exit 2
  fi
  existing_identity="${build_dir}/benchmark_cache_identity.json"
  if [[ ! -f "${existing_identity}" ]]; then
    echo "error: keyed benchmark cache is missing its identity: ${build_dir}" >&2
    rm -rf "${bootstrap_root}"
    exit 2
  fi
  if [[ "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["relative_path"])' "${existing_identity}")" != "${source_key}/${toolchain_key}" ]]; then
    echo "error: source/build contract changed for existing benchmark cache" >&2
    rm -rf "${bootstrap_root}"
    exit 2
  fi
  rm -rf "${bootstrap_root}"
  echo "benchmark cache hit: ${build_dir}"
else
  rm -rf "${bootstrap_root}"
  mkdir -p "$(dirname "${build_dir}")"
  cmake -S "${repo_root}" -B "${build_dir}" \
    -G "${generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTS=ON
  cmake --build "${build_dir}" --parallel "${jobs}" "$@"
fi

final_identity="${build_dir}/benchmark_cache_identity.json"

# Re-read the final build cache and verify the full contract did not change
# while the source tree or toolchain was being prepared.
python3 "${script_dir}/benchmark_environment_contract.py" cache-key \
  --repo-root "${repo_root}" \
  --build-dir "${build_dir}" \
  --toolchain "${toolchain}" \
  --compiler-path "$(command -v "${CC}")" \
  --compiler-version "${compiler_version}" \
  --compiler-target "${compiler_target}" \
  --generator "${generator}" \
  --configuration Release \
  --flag CMAKE_C_FLAGS="${CMAKE_C_FLAGS:-}" \
  --flag CMAKE_C_FLAGS_RELEASE="${CMAKE_C_FLAGS_RELEASE:--O3 -DNDEBUG}" \
  --flag CMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS:-}" \
  --flag CMAKE_EXE_LINKER_FLAGS_RELEASE="${CMAKE_EXE_LINKER_FLAGS_RELEASE:-}" \
  --flag CMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS:-}" \
  --flag CMAKE_SHARED_LINKER_FLAGS_RELEASE="${CMAKE_SHARED_LINKER_FLAGS_RELEASE:-}" \
  --output "${final_identity}.verified"
if [[ "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["relative_path"])' "${final_identity}.verified")" != "${source_key}/${toolchain_key}" ]]; then
  echo "error: source/build contract changed while creating benchmark cache" >&2
  exit 2
fi
mv "${final_identity}.verified" "${final_identity}"

end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

echo "Pass - Cost Time:${elapsed}(s) - Benchmark Release build (${toolchain})"
echo "  ZR_VM_CMAKE_BUILD_DIR=${build_dir}"
echo "  next: ZR_VM_CMAKE_BUILD_DIR=${build_dir} ./scripts/benchmark/run_wsl_benchmarks_report_csv.sh"
echo "----"
