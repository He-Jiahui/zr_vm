#!/usr/bin/env bash
# Compile and profile the strong-typed opcode perf closure fixtures with Callgrind.
#
# Usage:
#   bash scripts/benchmark/run_typed_opcode_perf_closure.sh [build_root] [repo_root] [out_dir]
#
# Env:
#   TYPED_EQUALITY_SCALE=12
#   UNSIGNED_ARITHMETIC_SCALE=20
#   KNOWN_CALL_SCALE=16
set -euo pipefail

BUILD_ROOT="${1:-/mnt/e/Git/zr_vm/build/benchmark-gcc-release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${2:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
OUT_DIR="${3:-${BUILD_ROOT}/tests_generated/typed_opcode_perf}"
CLI="${BUILD_ROOT}/bin/zr_vm_cli"

TYPED_EQUALITY_SCALE="${TYPED_EQUALITY_SCALE:-12}"
UNSIGNED_ARITHMETIC_SCALE="${UNSIGNED_ARITHMETIC_SCALE:-20}"
KNOWN_CALL_SCALE="${KNOWN_CALL_SCALE:-16}"

VALGRIND_EXE="${VALGRIND_EXE:-$(command -v valgrind || true)}"
CALLGRIND_ANNOTATE_EXE="${CALLGRIND_ANNOTATE_EXE:-$(command -v callgrind_annotate || true)}"

if [[ ! -x "${CLI}" ]]; then
    echo "missing CLI: ${CLI}" >&2
    exit 1
fi
if [[ -z "${VALGRIND_EXE}" ]]; then
    echo "missing valgrind" >&2
    exit 1
fi
if [[ -z "${CALLGRIND_ANNOTATE_EXE}" ]]; then
    echo "missing callgrind_annotate" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"

report_path="${OUT_DIR}/typed_opcode_perf_closure_report.md"
{
    echo "# Typed Opcode Perf Closure Report"
    echo
    echo "- Build root: \`${BUILD_ROOT}\`"
    echo "- Repo root: \`${REPO_ROOT}\`"
    echo "- CLI: \`${CLI}\`"
    echo "- Valgrind: \`${VALGRIND_EXE}\`"
    echo "- Callgrind annotate: \`${CALLGRIND_ANNOTATE_EXE}\`"
    echo "- Counting mode: \`--cache-sim=no --branch-sim=no\`"
    echo
} >"${report_path}"

check_regex_present() {
    local file_path="$1"
    local pattern="$2"
    local description="$3"
    if ! grep -Eq "${pattern}" "${file_path}"; then
        echo "expected ${description} in ${file_path}" >&2
        exit 1
    fi
}

check_fixed_present() {
    local file_path="$1"
    local needle="$2"
    local description="$3"
    if ! grep -Fq "${needle}" "${file_path}"; then
        echo "expected ${description} in ${file_path}" >&2
        exit 1
    fi
}

check_fixed_absent() {
    local file_path="$1"
    local needle="$2"
    local description="$3"
    if grep -Fq "${needle}" "${file_path}"; then
        echo "unexpected ${description} in ${file_path}" >&2
        exit 1
    fi
}

check_regex_absent() {
    local file_path="$1"
    local pattern="$2"
    local description="$3"
    if grep -Eq "${pattern}" "${file_path}"; then
        echo "unexpected ${description} in ${file_path}" >&2
        exit 1
    fi
}

top_function_lines() {
    local annotate_path="$1"
    awk '
        /file:function/ && $1 ~ /^Ir$/ { in_table=1; next }
        in_table && NF == 0 { exit }
        in_table {
            print $0
            count++
            if (count >= 8) {
                exit
            }
        }
    ' "${annotate_path}"
}

write_known_call_source() {
    local output_path="$1"
    local scale="$2"
    local groups=$((scale * 32))
    local group_index

    {
        echo 'var benchConfig = %import("bench_config");'
        echo
        echo "answer(): int {"
        echo "    return 11;"
        echo "}"
        echo
        echo "bonus(): int {"
        echo "    return 17;"
        echo "}"
        echo
        echo "var factor = benchConfig.scale();"
        echo "var checksum = 0;"
        echo

        for ((group_index = 0; group_index < groups; group_index++)); do
            echo "checksum = checksum + answer() * factor;"
            echo "checksum = checksum + bonus() * factor;"
            echo "checksum = checksum + answer() + bonus();"
            echo "checksum = checksum + answer() * (factor + 1);"
        done

        echo
        echo 'return "BENCH_KNOWN_CALL_PASS\n" + <string> checksum;'
    } >"${output_path}"
}

run_case() {
    local case_name="$1"
    local project_rel="$2"
    local project_file="$3"
    local scale="$4"
    local banner="$5"
    local workdir
    local zri_copy_path="${OUT_DIR}/${case_name}.main.zri"
    local run_output_path="${OUT_DIR}/${case_name}.run.txt"
    local callgrind_out_path="${OUT_DIR}/${case_name}.callgrind.out"
    local annotate_path="${OUT_DIR}/${case_name}.callgrind.annotate.txt"
    local total_ir
    local top_lines

    workdir="$(mktemp -d "/tmp/${case_name}_XXXXXX")"
    trap 'rm -rf "${workdir}"' RETURN

    cp -a "${REPO_ROOT}/${project_rel}/." "${workdir}/"
    cat >"${workdir}/src/bench_config.zr" <<EOF
pub scale(): int {
    return ${scale};
}
EOF

    if [[ "${case_name}" == "known_call" ]]; then
        write_known_call_source "${workdir}/src/main.zr" "${scale}"
    fi

    (
        cd "${workdir}"
        "${CLI}" --compile "${project_file}" --intermediate >/dev/null
    )

    cp "${workdir}/bin/main.zri" "${zri_copy_path}"

    case "${case_name}" in
        typed_equality)
            check_fixed_present "${zri_copy_path}" 'LOGICAL_EQUAL_BOOL' 'LOGICAL_EQUAL_BOOL'
            check_fixed_present "${zri_copy_path}" 'LOGICAL_NOT_EQUAL_SIGNED' 'LOGICAL_NOT_EQUAL_SIGNED'
            check_fixed_present "${zri_copy_path}" 'LOGICAL_NOT_EQUAL_UNSIGNED' 'LOGICAL_NOT_EQUAL_UNSIGNED'
            check_fixed_present "${zri_copy_path}" 'LOGICAL_EQUAL_FLOAT' 'LOGICAL_EQUAL_FLOAT'
            check_fixed_present "${zri_copy_path}" 'LOGICAL_EQUAL_STRING' 'LOGICAL_EQUAL_STRING'
            check_regex_absent "${zri_copy_path}" '(^|[^A-Z_])LOGICAL_EQUAL([^A-Z_]|$)' 'generic LOGICAL_EQUAL'
            check_regex_absent "${zri_copy_path}" '(^|[^A-Z_])LOGICAL_NOT_EQUAL([^A-Z_]|$)' 'generic LOGICAL_NOT_EQUAL'
            ;;
        unsigned_arithmetic)
            check_fixed_present "${zri_copy_path}" 'ADD_UNSIGNED' 'ADD_UNSIGNED family'
            check_fixed_present "${zri_copy_path}" 'SUB_UNSIGNED' 'SUB_UNSIGNED family'
            check_fixed_present "${zri_copy_path}" 'MUL_UNSIGNED' 'MUL_UNSIGNED family'
            check_fixed_present "${zri_copy_path}" 'DIV_UNSIGNED' 'DIV_UNSIGNED family'
            check_fixed_present "${zri_copy_path}" 'MOD_UNSIGNED' 'MOD_UNSIGNED family'
            check_regex_absent "${zri_copy_path}" '(^|[^A-Z_])ADD_INT(_[A-Z_]+)?([^A-Z_]|$)' 'ADD_INT family'
            check_regex_absent "${zri_copy_path}" '(^|[^A-Z_])SUB_INT(_[A-Z_]+)?([^A-Z_]|$)' 'SUB_INT family'
            ;;
        known_call)
            check_fixed_present "${zri_copy_path}" 'SUPER_KNOWN_VM_CALL_NO_ARGS' 'SUPER_KNOWN_VM_CALL_NO_ARGS'
            ;;
        *)
            echo "unknown case: ${case_name}" >&2
            exit 1
            ;;
    esac

    (
        cd "${workdir}"
        "${CLI}" "${project_file}" --execution-mode binary >"${run_output_path}"
    )

    if ! grep -Fxq "${banner}" "${run_output_path}"; then
        echo "missing pass banner ${banner} in ${run_output_path}" >&2
        cat "${run_output_path}" >&2
        exit 1
    fi

    (
        cd "${workdir}"
        "${VALGRIND_EXE}" --tool=callgrind \
            --trace-children=no \
            --cache-sim=no \
            --branch-sim=no \
            --callgrind-out-file="${callgrind_out_path}" \
            "${CLI}" "${project_file}" --execution-mode binary >/dev/null 2>&1
    )

    "${CALLGRIND_ANNOTATE_EXE}" --auto=yes --threshold=100 "${callgrind_out_path}" >"${annotate_path}"

    check_fixed_absent "${annotate_path}" 'ZrCore_Value_Equal' 'ZrCore_Value_Equal helper'
    check_fixed_absent "${annotate_path}" 'function_pre_call_dispatch' 'function_pre_call_dispatch helper'

    total_ir="$(awk '/PROGRAM TOTALS/ { print $1; exit }' "${annotate_path}")"
    top_lines="$(top_function_lines "${annotate_path}")"

    {
        echo "## ${case_name}"
        echo
        echo "- Scale: \`${scale}\`"
        echo "- Project: \`${project_rel}\`"
        echo "- Run output: \`${run_output_path}\`"
        echo "- Intermediate: \`${zri_copy_path}\`"
        echo "- Callgrind: \`${callgrind_out_path}\`"
        echo "- Callgrind annotate: \`${annotate_path}\`"
        echo "- Total Ir: \`${total_ir}\`"
        echo "- Banned helpers absent: \`ZrCore_Value_Equal\`, \`function_pre_call_dispatch\`"
        echo "- Top functions:"
        if [[ -n "${top_lines}" ]]; then
            while IFS= read -r line; do
                echo "  ${line}"
            done <<<"${top_lines}"
        else
            echo "  unavailable"
        fi
        echo
    } >>"${report_path}"

    rm -rf "${workdir}"
    trap - RETURN
}

run_case "typed_equality" \
    "tests/fixtures/projects/benchmark_typed_equality" \
    "benchmark_typed_equality.zrp" \
    "${TYPED_EQUALITY_SCALE}" \
    "BENCH_TYPED_EQUALITY_PASS"
run_case "unsigned_arithmetic" \
    "tests/fixtures/projects/benchmark_unsigned_arithmetic" \
    "benchmark_unsigned_arithmetic.zrp" \
    "${UNSIGNED_ARITHMETIC_SCALE}" \
    "BENCH_UNSIGNED_ARITHMETIC_PASS"
run_case "known_call" \
    "tests/fixtures/projects/benchmark_known_call" \
    "benchmark_known_call.zrp" \
    "${KNOWN_CALL_SCALE}" \
    "BENCH_KNOWN_CALL_PASS"

echo "Wrote report: ${report_path}"
