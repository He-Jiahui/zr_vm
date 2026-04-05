if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif ()

if (NOT DEFINED PERF_RUNNER_EXE OR PERF_RUNNER_EXE STREQUAL "")
    message(FATAL_ERROR "PERF_RUNNER_EXE is required.")
endif ()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif ()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif ()

if (DEFINED TIER AND NOT TIER STREQUAL "")
    string(TOLOWER "${TIER}" PERF_REQUESTED_TIER)
elseif (DEFINED ENV{ZR_VM_TEST_TIER} AND NOT "$ENV{ZR_VM_TEST_TIER}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_TEST_TIER}" PERF_REQUESTED_TIER)
else ()
    set(PERF_REQUESTED_TIER "core")
endif ()

if (NOT PERF_REQUESTED_TIER STREQUAL "smoke" AND
        NOT PERF_REQUESTED_TIER STREQUAL "core" AND
        NOT PERF_REQUESTED_TIER STREQUAL "stress")
    message(FATAL_ERROR "Unsupported performance tier: ${PERF_REQUESTED_TIER}")
endif ()

if (PERF_REQUESTED_TIER STREQUAL "smoke")
    set(PERF_DEFAULT_WARMUP 1)
    set(PERF_DEFAULT_ITERATIONS 2)
elseif (PERF_REQUESTED_TIER STREQUAL "stress")
    set(PERF_DEFAULT_WARMUP 2)
    set(PERF_DEFAULT_ITERATIONS 8)
else ()
    set(PERF_DEFAULT_WARMUP 2)
    set(PERF_DEFAULT_ITERATIONS 4)
endif ()

if (DEFINED ENV{ZR_VM_PERF_WARMUP} AND NOT "$ENV{ZR_VM_PERF_WARMUP}" STREQUAL "")
    set(PERF_WARMUP "$ENV{ZR_VM_PERF_WARMUP}")
else ()
    set(PERF_WARMUP "${PERF_DEFAULT_WARMUP}")
endif ()

if (DEFINED ENV{ZR_VM_PERF_ITERATIONS} AND NOT "$ENV{ZR_VM_PERF_ITERATIONS}" STREQUAL "")
    set(PERF_ITERATIONS "$ENV{ZR_VM_PERF_ITERATIONS}")
else ()
    set(PERF_ITERATIONS "${PERF_DEFAULT_ITERATIONS}")
endif ()

if (NOT PERF_WARMUP MATCHES "^[0-9]+$" OR PERF_WARMUP LESS 1)
    message(FATAL_ERROR "Invalid PERF_WARMUP: ${PERF_WARMUP}")
endif ()

if (NOT PERF_ITERATIONS MATCHES "^[0-9]+$" OR PERF_ITERATIONS LESS 1)
    message(FATAL_ERROR "Invalid PERF_ITERATIONS: ${PERF_ITERATIONS}")
endif ()

set(PERF_SUITE_ROOT "${GENERATED_DIR}/performance_suite")
set(PERF_REPORT_DIR "${GENERATED_DIR}/performance")
file(REMOVE_RECURSE "${PERF_SUITE_ROOT}")
file(MAKE_DIRECTORY "${PERF_SUITE_ROOT}")
file(MAKE_DIRECTORY "${PERF_REPORT_DIR}")
file(MAKE_DIRECTORY "${PERF_SUITE_ROOT}/fixtures")

function(register_perf_case name project_dir description tiers)
    set(perf_case_names_local "${PERF_CASE_NAMES}")
    list(APPEND perf_case_names_local "${name}")
    set(PERF_CASE_NAMES "${perf_case_names_local}" PARENT_SCOPE)
    set("PERF_CASE_PROJECT_${name}" "${project_dir}" PARENT_SCOPE)
    set("PERF_CASE_DESCRIPTION_${name}" "${description}" PARENT_SCOPE)
    set("PERF_CASE_TIERS_${name}" "${tiers}" PARENT_SCOPE)
endfunction()

function(perf_case_matches_tier tiers out_var)
    list(FIND tiers "${PERF_REQUESTED_TIER}" perf_tier_index)
    if (perf_tier_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif ()
endfunction()

function(perf_copy_fixture project_dir destination_var)
    set(destination "${PERF_SUITE_ROOT}/fixtures/${project_dir}")
    file(REMOVE_RECURSE "${destination}")
    file(MAKE_DIRECTORY "${destination}")
    file(GLOB fixture_entries RELATIVE "${PROJECT_FIXTURES_DIR}/${project_dir}" "${PROJECT_FIXTURES_DIR}/${project_dir}/*")
    foreach (fixture_entry IN LISTS fixture_entries)
        if (fixture_entry STREQUAL "bin")
            continue()
        endif ()

        set(source_path "${PROJECT_FIXTURES_DIR}/${project_dir}/${fixture_entry}")
        if (IS_DIRECTORY "${source_path}")
            file(COPY "${source_path}" DESTINATION "${destination}")
        else ()
            execute_process(
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_path}" "${destination}/${fixture_entry}"
                    RESULT_VARIABLE copy_result
                    OUTPUT_VARIABLE copy_stdout
                    ERROR_VARIABLE copy_stderr
            )
            if (NOT copy_result EQUAL 0)
                message(FATAL_ERROR
                        "Failed to copy performance fixture '${project_dir}/${fixture_entry}'.\n${copy_stdout}${copy_stderr}")
            endif ()
        endif ()
    endforeach ()
    set(${destination_var} "${destination}" PARENT_SCOPE)
endfunction()

function(perf_compile_project case_name project_file)
    execute_process(
            COMMAND "${CLI_EXE}" "--compile" "${project_file}"
            RESULT_VARIABLE compile_result
            OUTPUT_VARIABLE compile_stdout
            ERROR_VARIABLE compile_stderr
    )

    if (NOT compile_result EQUAL 0)
        message(FATAL_ERROR
                "Performance case '${case_name}' failed during precompile.\nOutput:\n${compile_stdout}${compile_stderr}")
    endif ()
endfunction()

function(perf_assert_binary_output case_name project_file expected_output)
    execute_process(
            COMMAND "${CLI_EXE}" "${project_file}" "--execution-mode" "binary"
            RESULT_VARIABLE run_result
            OUTPUT_VARIABLE run_stdout
            ERROR_VARIABLE run_stderr
    )

    set(actual_output "${run_stdout}${run_stderr}")
    string(REPLACE "\r\n" "\n" actual_output "${actual_output}")
    string(REPLACE "\r" "\n" actual_output "${actual_output}")
    string(STRIP "${actual_output}" actual_output)
    string(STRIP "${expected_output}" expected_output_normalized)

    if (NOT run_result EQUAL 0)
        message(FATAL_ERROR
                "Performance case '${case_name}' failed during binary correctness run.\nOutput:\n${actual_output}")
    endif ()

    if (NOT actual_output STREQUAL expected_output_normalized)
        message(FATAL_ERROR
                "Performance case '${case_name}' produced unexpected output.\nExpected:\n${expected_output_normalized}\nActual:\n${actual_output}")
    endif ()
endfunction()

function(perf_extract_metric text key out_var)
    string(REGEX MATCH "${key}=([0-9]+(\\.[0-9]+)?)" perf_match "${text}")
    if (perf_match STREQUAL "")
        message(FATAL_ERROR "Failed to parse metric '${key}' from performance output:\n${text}")
    endif ()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

register_perf_case(
        "benchmark_numeric_loops"
        "benchmark_numeric_loops"
        "Integer arithmetic and branch-heavy loops"
        "smoke;core;stress"
)
set(PERF_CASE_EXPECTED_benchmark_numeric_loops "BENCH_NUMERIC_LOOPS_PASS\n939148")

register_perf_case(
        "benchmark_dispatch_loops"
        "benchmark_dispatch_loops"
        "Method dispatch under deterministic loop pressure"
        "core;stress"
)
set(PERF_CASE_EXPECTED_benchmark_dispatch_loops "BENCH_DISPATCH_LOOPS_PASS\n912035")

register_perf_case(
        "benchmark_container_pipeline"
        "benchmark_container_pipeline"
        "Container queue/set/map aggregation pipeline"
        "core;stress"
)
set(PERF_CASE_EXPECTED_benchmark_container_pipeline "BENCH_CONTAINER_PIPELINE_PASS\n1774511")

message("==========")
message("Running suite: performance_report")
message("Tier: ${PERF_REQUESTED_TIER}")
message("Warmup iterations: ${PERF_WARMUP}")
message("Measured iterations: ${PERF_ITERATIONS}")
message("==========")

set(PERF_CASE_NAMES_ACTIVE "")
set(PERF_MARKDOWN_ROWS "")
set(PERF_JSON_CASES "")

foreach (perf_case_name IN LISTS PERF_CASE_NAMES)
    perf_case_matches_tier("${PERF_CASE_TIERS_${perf_case_name}}" perf_case_enabled)
    if (NOT perf_case_enabled)
        continue()
    endif ()

    list(APPEND PERF_CASE_NAMES_ACTIVE "${perf_case_name}")
    perf_copy_fixture("${PERF_CASE_PROJECT_${perf_case_name}}" perf_case_dir)
    set(perf_project_file "${perf_case_dir}/${PERF_CASE_PROJECT_${perf_case_name}}.zrp")
    perf_compile_project("${perf_case_name}" "${perf_project_file}")
    perf_assert_binary_output("${perf_case_name}" "${perf_project_file}" "${PERF_CASE_EXPECTED_${perf_case_name}}")

    set(perf_case_json_path "${PERF_REPORT_DIR}/${perf_case_name}.json")
    execute_process(
            COMMAND
            "${PERF_RUNNER_EXE}"
            "--name" "${perf_case_name}"
            "--iterations" "${PERF_ITERATIONS}"
            "--warmup" "${PERF_WARMUP}"
            "--json-out" "${perf_case_json_path}"
            "--working-directory" "${perf_case_dir}"
            "--"
            "${CLI_EXE}"
            "${perf_project_file}"
            "--execution-mode"
            "binary"
            RESULT_VARIABLE perf_runner_result
            OUTPUT_VARIABLE perf_runner_stdout
            ERROR_VARIABLE perf_runner_stderr
    )

    set(perf_runner_output "${perf_runner_stdout}${perf_runner_stderr}")
    if (NOT perf_runner_result EQUAL 0)
        message(FATAL_ERROR
                "Performance runner failed for case '${perf_case_name}'.\nOutput:\n${perf_runner_output}")
    endif ()

    perf_extract_metric("${perf_runner_output}" "mean_wall_ms" perf_mean_wall_ms)
    perf_extract_metric("${perf_runner_output}" "median_wall_ms" perf_median_wall_ms)
    perf_extract_metric("${perf_runner_output}" "min_wall_ms" perf_min_wall_ms)
    perf_extract_metric("${perf_runner_output}" "max_wall_ms" perf_max_wall_ms)
    perf_extract_metric("${perf_runner_output}" "stddev_wall_ms" perf_stddev_wall_ms)
    perf_extract_metric("${perf_runner_output}" "mean_peak_mib" perf_mean_peak_mib)
    perf_extract_metric("${perf_runner_output}" "max_peak_mib" perf_max_peak_mib)
    perf_extract_metric("${perf_runner_output}" "mean_peak_bytes" perf_mean_peak_bytes)
    perf_extract_metric("${perf_runner_output}" "max_peak_bytes" perf_max_peak_bytes)

    set(PERF_MARKDOWN_ROWS
            "${PERF_MARKDOWN_ROWS}| ${perf_case_name} | ${PERF_CASE_DESCRIPTION_${perf_case_name}} | ${perf_mean_wall_ms} | ${perf_median_wall_ms} | ${perf_min_wall_ms} | ${perf_max_wall_ms} | ${perf_stddev_wall_ms} | ${perf_mean_peak_mib} | ${perf_max_peak_mib} | ${perf_mean_peak_bytes} | ${perf_max_peak_bytes} |\n")

    file(READ "${perf_case_json_path}" perf_case_json)
    if (PERF_JSON_CASES STREQUAL "")
        set(PERF_JSON_CASES "${perf_case_json}")
    else ()
        set(PERF_JSON_CASES "${PERF_JSON_CASES},\n${perf_case_json}")
    endif ()
endforeach ()

list(LENGTH PERF_CASE_NAMES_ACTIVE PERF_CASE_COUNT)
if (PERF_CASE_COUNT EQUAL 0)
    message(FATAL_ERROR "performance_report selected zero benchmark cases for tier '${PERF_REQUESTED_TIER}'.")
endif ()

string(TIMESTAMP PERF_GENERATED_AT_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
file(TO_CMAKE_PATH "${CLI_EXE}" PERF_CLI_EXE_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_RUNNER_EXE}" PERF_RUNNER_EXE_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.md" PERF_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.json" PERF_JSON_PATH_NORMALIZED)

string(CONCAT PERF_MARKDOWN_REPORT
        "# ZR VM Performance Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Warmup Iterations Per Case: ${PERF_WARMUP}\n"
        "- Measured Iterations Per Case: ${PERF_ITERATIONS}\n"
        "- CLI Executable: `${PERF_CLI_EXE_NORMALIZED}`\n"
        "- Perf Runner: `${PERF_RUNNER_EXE_NORMALIZED}`\n"
        "- Validated Mode: `binary`\n"
        "- Cases: ${PERF_CASE_COUNT}\n\n"
        "| Case | Description | Mean Wall (ms) | Median Wall (ms) | Min Wall (ms) | Max Wall (ms) | Stddev Wall (ms) | Mean Peak (MiB) | Max Peak (MiB) | Mean Peak (bytes) | Max Peak (bytes) |\n"
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n"
        "${PERF_MARKDOWN_ROWS}\n"
        "## Artifacts\n\n"
        "- Markdown Report: `${PERF_MARKDOWN_PATH_NORMALIZED}`\n"
        "- JSON Report: `${PERF_JSON_PATH_NORMALIZED}`\n")

file(WRITE "${PERF_REPORT_DIR}/benchmark_report.md" "${PERF_MARKDOWN_REPORT}")
file(WRITE
        "${PERF_REPORT_DIR}/benchmark_report.json"
        "{\n"
        "  \"suite\": \"performance_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"warmup\": ${PERF_WARMUP},\n"
        "  \"iterations\": ${PERF_ITERATIONS},\n"
        "  \"validated_mode\": \"binary\",\n"
        "  \"cases\": [\n${PERF_JSON_CASES}\n  ]\n"
        "}\n")

message("Performance markdown report: ${PERF_REPORT_DIR}/benchmark_report.md")
message("Performance json report: ${PERF_REPORT_DIR}/benchmark_report.json")
