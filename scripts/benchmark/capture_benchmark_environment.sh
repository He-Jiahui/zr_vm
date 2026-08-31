#!/usr/bin/env bash
# Run a benchmark command under one allowed logical CPU and record its environment.

set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
contract_script="${script_dir}/benchmark_environment_contract.py"
original_arguments=("$@")

repo_root=""
build_dir=""
output_path=""
command_arguments=()

usage() {
  echo "usage: $0 --repo-root PATH --build-dir PATH --output PATH -- COMMAND [ARG ...]" >&2
}

while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --repo-root)
      [[ "$#" -ge 2 ]] || { usage; exit 2; }
      repo_root="$2"
      shift 2
      ;;
    --build-dir)
      [[ "$#" -ge 2 ]] || { usage; exit 2; }
      build_dir="$2"
      shift 2
      ;;
    --output)
      [[ "$#" -ge 2 ]] || { usage; exit 2; }
      output_path="$2"
      shift 2
      ;;
    --)
      shift
      command_arguments=("$@")
      break
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ -z "${repo_root}" || -z "${build_dir}" || -z "${output_path}" || "${#command_arguments[@]}" -eq 0 ]]; then
  usage
  exit 2
fi
if [[ ! -f "${contract_script}" ]]; then
  echo "error: missing environment contract script: ${contract_script}" >&2
  exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "error: python3 is required" >&2
  exit 2
fi
if [[ ! -r /proc/self/status ]]; then
  echo "error: Linux /proc/self/status is required" >&2
  exit 2
fi

observed_mask="$(sed -n 's/^Cpus_allowed_list:[[:space:]]*//p' /proc/self/status)"
if [[ -z "${observed_mask}" ]]; then
  echo "error: could not read Cpus_allowed_list from /proc/self/status" >&2
  exit 2
fi

run_captured_command() {
  local isolation_status="$1"
  local selected_cpu="$2"
  local isolation_reason="$3"
  local capture_arguments
  local command_result
  local finalize_result

  capture_arguments=(
    "${contract_script}" capture
    --repo-root "${repo_root}"
    --build-dir "${build_dir}"
    --output "${output_path}"
    --isolation-status "${isolation_status}"
    --observed-mask "${observed_mask}"
    --process-id "$$"
  )
  if [[ -n "${selected_cpu}" ]]; then
    capture_arguments+=(--selected-cpu "${selected_cpu}")
  fi
  if [[ -n "${isolation_reason}" ]]; then
    capture_arguments+=(--isolation-reason "${isolation_reason}")
  fi
  if [[ -n "${ZR_VM_BENCHMARK_CONFIGURATION:-}" ]]; then
    capture_arguments+=(--configuration "${ZR_VM_BENCHMARK_CONFIGURATION}")
  fi

  python3 "${capture_arguments[@]}"
  if [[ "$?" -ne 0 ]]; then
    return 2
  fi

  "${command_arguments[@]}"
  command_result="$?"

  python3 "${contract_script}" finalize \
    --repo-root "${repo_root}" \
    --input "${output_path}" \
    --output "${output_path}"
  finalize_result="$?"
  if [[ "${finalize_result}" -ne 0 ]]; then
    return 2
  fi
  return "${command_result}"
}

if [[ -n "${_ZR_VM_BENCHMARK_PINNED_CPU:-}" ]]; then
  pinned_cpu="${_ZR_VM_BENCHMARK_PINNED_CPU}"
  if [[ "${pinned_cpu}" =~ ^[0-9]+$ && "${observed_mask}" == "${pinned_cpu}" ]]; then
    run_captured_command "ISOLATED" "${pinned_cpu}" ""
    exit "$?"
  fi
  run_captured_command \
    "NON_ISOLATED" \
    "" \
    "taskset child mask verification failed: requested=${pinned_cpu}, observed=${observed_mask}"
  exit "$?"
fi

if [[ "${ZR_VM_BENCHMARK_DISABLE_AFFINITY:-0}" == "1" ]]; then
  run_captured_command "NON_ISOLATED" "" "affinity disabled by ZR_VM_BENCHMARK_DISABLE_AFFINITY"
  exit "$?"
fi

if ! command -v taskset >/dev/null 2>&1; then
  run_captured_command "NON_ISOLATED" "" "taskset is unavailable"
  exit "$?"
fi

selection_arguments=(
  "${contract_script}" select-cpu
  --allowed-list "${observed_mask}"
)
if [[ -n "${ZR_VM_BENCHMARK_CPU:-}" ]]; then
  selection_arguments+=(--requested-cpu "${ZR_VM_BENCHMARK_CPU}")
fi
selection_output="$(python3 "${selection_arguments[@]}" 2>&1)"
selection_result="$?"
if [[ "${selection_result}" -ne 0 || ! "${selection_output}" =~ ^[0-9]+$ ]]; then
  run_captured_command "NON_ISOLATED" "" "CPU topology selection failed: ${selection_output}"
  exit "$?"
fi

selected_cpu="${selection_output}"
if ! taskset -c "${selected_cpu}" true >/dev/null 2>&1; then
  run_captured_command "NON_ISOLATED" "" "taskset rejected CPU ${selected_cpu}"
  exit "$?"
fi

_ZR_VM_BENCHMARK_PINNED_CPU="${selected_cpu}" \
  taskset -c "${selected_cpu}" bash "$0" "${original_arguments[@]}"
exit "$?"
