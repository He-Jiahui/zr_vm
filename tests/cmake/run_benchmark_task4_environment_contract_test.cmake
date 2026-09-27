cmake_minimum_required(VERSION 3.20)

# Task 4 CTest 入口同时检查环境解析结果和主套件/Task 3 汇总的接线。
# TEST_OUTPUT_DIR 是本测试独占的构建树目录，入口会清除并重建它。
foreach (required_var IN ITEMS TASK4_MODULE PERFORMANCE_SUITE_SCRIPT ASSEMBLY_SCRIPT TEST_OUTPUT_DIR)
    if (NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required_var}=...")
    endif ()
endforeach ()

include("${TASK4_MODULE}")
file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

# IN_PROGRESS 的隔离快照有效但仍待最终确认，不能提前授予比较资格。
set(isolated_path "${TEST_OUTPUT_DIR}/isolated.json")
file(WRITE "${isolated_path}"
        "{\"capture_status\":\"IN_PROGRESS\","
        "\"isolation\":{\"status\":\"ISOLATED\",\"policy\":\"single_logical_cpu_v1\",\"selected_cpu\":3}}")
zr_benchmark_task4_resolve_environment(
        "Linux" "${isolated_path}"
        isolated_valid isolated_comparable isolated_json isolated_issue)
if (NOT isolated_valid OR isolated_comparable OR NOT isolated_issue STREQUAL "")
    message(FATAL_ERROR "isolated pending capture must remain provisionally non-comparable: ${isolated_issue}")
endif ()
string(JSON isolated_status GET "${isolated_json}" status)
if (NOT isolated_status STREQUAL "PENDING_FINALIZATION")
    message(FATAL_ERROR "unexpected isolated status: ${isolated_status}")
endif ()

set(nonisolated_path "${TEST_OUTPUT_DIR}/nonisolated.json")
file(WRITE "${nonisolated_path}"
        "{\"capture_status\":\"IN_PROGRESS\","
        "\"isolation\":{\"status\":\"NON_ISOLATED\",\"policy\":\"single_logical_cpu_v1\",\"selected_cpu\":null}}")
zr_benchmark_task4_resolve_environment(
        "Linux" "${nonisolated_path}"
        nonisolated_valid nonisolated_comparable nonisolated_json nonisolated_issue)
if (NOT nonisolated_valid OR nonisolated_comparable)
    message(FATAL_ERROR "NON_ISOLATED capture must remain provisionally non-comparable")
endif ()
string(JSON nonisolated_status GET "${nonisolated_json}" status)
if (NOT nonisolated_status STREQUAL "INCOMPARABLE")
    message(FATAL_ERROR "unexpected non-isolated status: ${nonisolated_status}")
endif ()

# 缺报告的 Linux 运行与没有等价亲和性证据的 Windows 运行具有不同有效性。
zr_benchmark_task4_resolve_environment(
        "Linux" ""
        missing_valid missing_comparable missing_json missing_issue)
if (missing_valid OR missing_comparable OR NOT missing_issue STREQUAL "ENVIRONMENT_REPORT_REQUIRED")
    message(FATAL_ERROR "Linux must reject a suite run outside the capture wrapper")
endif ()

zr_benchmark_task4_resolve_environment(
        "Windows" ""
        windows_valid windows_comparable windows_json windows_issue)
if (NOT windows_valid OR windows_comparable OR NOT windows_issue STREQUAL "")
    message(FATAL_ERROR "Windows must remain a valid diagnostic-only suite")
endif ()
string(JSON windows_status GET "${windows_json}" status)
if (NOT windows_status STREQUAL "INCOMPARABLE")
    message(FATAL_ERROR "Windows environment status must be INCOMPARABLE")
endif ()
string(FIND "${windows_json}" "WINDOWS_NATIVE_AFFINITY_CAPTURE_UNAVAILABLE" windows_reason_index)
if (windows_reason_index LESS 0)
    message(FATAL_ERROR "Windows diagnostic reason is missing")
endif ()

# 文本检查只证明包含关键接线；报告值是否真被门控须由端到端用例验证。
file(READ "${PERFORMANCE_SUITE_SCRIPT}" suite_source)
foreach (required_text IN ITEMS
        "benchmark_task4_environment.cmake"
        "PERF_TASK4_ENVIRONMENT_JSON"
        "PERF_TASK4_PROVISIONAL_COMPARABLE")
    string(FIND "${suite_source}" "${required_text}" source_index)
    if (source_index LESS 0)
        message(FATAL_ERROR "performance suite is missing Task4 integration: ${required_text}")
    endif ()
endforeach ()

file(READ "${ASSEMBLY_SCRIPT}" assembly_source)
string(FIND "${assembly_source}" "PERF_TASK4_PROVISIONAL_COMPARABLE" assembly_index)
if (assembly_index LESS 0)
    message(FATAL_ERROR "case assembly does not gate ratios on Task4 environment eligibility")
endif ()
