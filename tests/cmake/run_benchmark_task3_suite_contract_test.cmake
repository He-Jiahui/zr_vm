if (DEFINED TASK3_EXPECT_INVALID_POLICY AND TASK3_EXPECT_INVALID_POLICY)
    if (NOT DEFINED TASK3_MODULE OR TASK3_MODULE STREQUAL "")
        message(FATAL_ERROR "TASK3_MODULE is required")
    endif ()
    include("${TASK3_MODULE}")
    zr_benchmark_task3_resolve_policy(
            steady core 1 1 5 21
            invalid_warmup invalid_iterations invalid_extra invalid_profile invalid_minimum_mode)
    message(FATAL_ERROR "initial sample count above 20 was accepted")
endif ()

foreach (required_variable IN ITEMS
        TASK3_MODULE EXECUTION_PLAN_SCRIPT PERFORMANCE_SUITE_SCRIPT ASSEMBLY_SCRIPT TESTS_CMAKE PYTHON_EXE TEST_OUTPUT_DIR)
    if (NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif ()
endforeach ()

file(READ "${TESTS_CMAKE}" tests_cmake_source)
foreach (registered_test IN ITEMS
        benchmark_statistics_python
        benchmark_execution_plan_python
        benchmark_registry_contract
        benchmark_task3_suite_contract
        benchmark_task3_report_consumers_python)
    if (NOT tests_cmake_source MATCHES "NAME[ \t\r\n]+${registered_test}([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR "tests/CMakeLists.txt does not register ${registered_test}")
    endif ()
endforeach ()

file(READ "${PERFORMANCE_SUITE_SCRIPT}" performance_suite_source)
file(READ "${ASSEMBLY_SCRIPT}" performance_assembly_source)
string(APPEND performance_suite_source "\n${performance_assembly_source}")
foreach (required_marker IN ITEMS
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/benchmark_task3_suite.cmake\")"
        "zr_benchmark_task3_create_execution_plan("
        "zr_benchmark_task3_parse_runner_report("
        "--min-sample-ms"
        "--max-extra-samples"
        "--bootstrap-seed"
        "--profile"
        "\\\"schema_version\\\": 3"
        "\\\"execution_plan\\\""
        "\\\"measurement_policy\\\""
        "\\\"min_sample_ms\\\"")
    string(FIND "${performance_suite_source}" "${required_marker}" marker_index)
    if (marker_index LESS 0)
        message(FATAL_ERROR "performance suite is missing Task 3 marker: ${required_marker}")
    endif ()
endforeach ()
string(REGEX MATCHALL "perf_extract_metric\\(" stdout_metric_extractors "${performance_suite_source}")
list(LENGTH stdout_metric_extractors stdout_metric_extractor_count)
if (stdout_metric_extractor_count GREATER 1)
    message(FATAL_ERROR "performance suite still consumes runner statistics from stdout")
endif ()

include("${TASK3_MODULE}")

function(expect_policy scope tier default_warmup default_iterations requested_warmup requested_iterations
         expected_warmup expected_iterations expected_extra expected_profile expected_minimum_mode)
    zr_benchmark_task3_resolve_policy(
            "${scope}"
            "${tier}"
            "${default_warmup}"
            "${default_iterations}"
            "${requested_warmup}"
            "${requested_iterations}"
            actual_warmup
            actual_iterations
            actual_extra
            actual_profile
            actual_minimum_mode)
    if (NOT actual_warmup STREQUAL "${expected_warmup}" OR
        NOT actual_iterations STREQUAL "${expected_iterations}" OR
        NOT actual_extra STREQUAL "${expected_extra}" OR
        NOT actual_profile STREQUAL "${expected_profile}" OR
        NOT actual_minimum_mode STREQUAL "${expected_minimum_mode}")
        message(FATAL_ERROR
                "policy mismatch for ${scope}/${tier}: "
                "${actual_warmup}/${actual_iterations}/${actual_extra}/${actual_profile}/${actual_minimum_mode}")
    endif ()
endfunction()

expect_policy(process core 1 1 "" "" 1 1 10 false registry)
expect_policy(process stress 1 2 3 4 3 4 10 false registry)
expect_policy(steady core 1 1 "" "" 5 10 10 false registry)
expect_policy(steady core 1 1 7 12 7 12 8 false registry)
expect_policy(steady core 1 1 7 20 7 20 0 false registry)
expect_policy(process profile 0 1 99 99 0 1 0 true disabled)

zr_benchmark_task3_dotnet_jit_state_reused(TRUE 0 TRUE calibrated_jit_reused)
zr_benchmark_task3_dotnet_jit_state_reused(TRUE 1 FALSE warmed_jit_reused)
zr_benchmark_task3_dotnet_jit_state_reused(TRUE 0 FALSE cold_jit_reused)
zr_benchmark_task3_dotnet_jit_state_reused(FALSE 5 TRUE process_jit_reused)
if (NOT calibrated_jit_reused OR NOT warmed_jit_reused OR cold_jit_reused OR process_jit_reused)
    message(FATAL_ERROR
            "unexpected .NET JIT reuse policy: "
            "${calibrated_jit_reused}/${warmed_jit_reused}/${cold_jit_reused}/${process_jit_reused}")
endif ()

execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -DTASK3_EXPECT_INVALID_POLICY=ON
                -DTASK3_MODULE=${TASK3_MODULE}
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE invalid_policy_result
        OUTPUT_VARIABLE invalid_policy_stdout
        ERROR_VARIABLE invalid_policy_stderr)
if (invalid_policy_result EQUAL 0 OR
    NOT "${invalid_policy_stdout}${invalid_policy_stderr}" MATCHES "exceeds the 20-sample runner limit")
    message(FATAL_ERROR
            "initial sample count above 20 did not fail closed:\n"
            "${invalid_policy_stdout}${invalid_policy_stderr}")
endif ()

foreach (valid_seed IN ITEMS 0 1 18446744073709551615)
    zr_benchmark_task3_validate_seed("${valid_seed}" seed_valid)
    if (NOT seed_valid)
        message(FATAL_ERROR "valid seed rejected: ${valid_seed}")
    endif ()
endforeach ()
foreach (invalid_seed IN ITEMS -1 1.0 true 18446744073709551616 999999999999999999999)
    zr_benchmark_task3_validate_seed("${invalid_seed}" seed_valid)
    if (seed_valid)
        message(FATAL_ERROR "invalid seed accepted: ${invalid_seed}")
    endif ()
endforeach ()

file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")
set(plan_path "${TEST_OUTPUT_DIR}/execution-plan.json")
set(jobs_json [=[[
  {"case":"a","implementation":"c"},
  {"case":"a","implementation":"zr_interp"},
  {"case":"b","implementation":"c"},
  {"case":"b","implementation":"zr_interp"},
  {"case":"c","implementation":"c"},
  {"case":"c","implementation":"zr_interp"}
]]=])
zr_benchmark_task3_create_execution_plan(
        "${PYTHON_EXE}"
        "${EXECUTION_PLAN_SCRIPT}"
        "${jobs_json}"
        0
        [=[["b","c"]]=]
        [=[["zr_interp"]]=]
        "${plan_path}"
        plan_json)
string(JSON plan_algorithm GET "${plan_json}" algorithm)
string(JSON plan_seed GET "${plan_json}" seed)
string(JSON plan_job_count GET "${plan_json}" job_count)
string(JSON first_case GET "${plan_json}" jobs 0 case)
string(JSON first_implementation GET "${plan_json}" jobs 0 implementation)
string(JSON second_case GET "${plan_json}" jobs 1 case)
if (NOT plan_algorithm STREQUAL "fisher_yates_splitmix64" OR
    NOT plan_seed EQUAL 0 OR
    NOT plan_job_count EQUAL 2 OR
    NOT first_case STREQUAL "b" OR
    NOT first_implementation STREQUAL "zr_interp" OR
    NOT second_case STREQUAL "c")
    message(FATAL_ERROR "execution plan was not filtered before the fixed-seed shuffle: ${plan_json}")
endif ()

set(runner_json [=[{
  "iterations": 10,
  "sample_count": 13,
  "extra_sample_count": 3,
  "repetitions": 8,
  "warmup": 5,
  "calibration": {"enabled": true, "min_sample_ms": 750.0, "aggregate_wall_ms": 900.0, "repetitions": 8},
  "stability": "STABLE",
  "comparable": true,
  "gate_eligible": true,
  "persistent_session": null,
  "summary": {
    "mean_wall_ms": 100.0,
    "median_wall_ms": 99.0,
    "min_wall_ms": 95.0,
    "max_wall_ms": 110.0,
    "stddev_wall_ms": 4.0,
    "mad_wall_ms": 2.0,
    "coefficient_of_variation": 0.04,
    "bootstrap": {"seed": "17", "statistic": "median", "resamples": 10000, "low": 97.0, "high": 102.0},
    "mean_peak_working_set_bytes": 1048576,
    "median_peak_working_set_bytes": 1048576,
    "min_peak_working_set_bytes": 1048576,
    "max_peak_working_set_bytes": 2097152
  }
}]=])
zr_benchmark_task3_parse_runner_report("${runner_json}" parsed)
foreach (expectation IN ITEMS
        "parsed_mean_wall_ms=100.0"
        "parsed_median_wall_ms=99.0"
        "parsed_mad_wall_ms=2.0"
        "parsed_bootstrap_low=97.0"
        "parsed_bootstrap_high=102.0"
        "parsed_sample_count=13"
        "parsed_extra_sample_count=3"
        "parsed_repetitions=8"
        "parsed_stability=STABLE"
        "parsed_comparable=ON"
        "parsed_gate_eligible=ON"
        "parsed_calibration_enabled=ON"
        "parsed_calibration_min_sample_ms=750.0"
        "parsed_mean_peak_working_set_bytes=1048576"
        "parsed_max_peak_working_set_bytes=2097152")
    string(REPLACE "=" ";" expectation_parts "${expectation}")
    list(GET expectation_parts 0 variable_name)
    list(GET expectation_parts 1 expected_value)
    if (NOT "${${variable_name}}" STREQUAL "${expected_value}")
        message(FATAL_ERROR "runner JSON field ${variable_name}: expected ${expected_value}, got ${${variable_name}}")
    endif ()
endforeach ()
if (NOT parsed_coefficient_of_variation EQUAL 0.04)
    message(FATAL_ERROR
            "runner JSON coefficient_of_variation: expected 0.04, got ${parsed_coefficient_of_variation}")
endif ()

set(profile_json [=[{
  "iterations": 1,
  "sample_count": 1,
  "extra_sample_count": 0,
  "repetitions": 1,
  "warmup": 0,
  "calibration": {"enabled": false, "min_sample_ms": null, "aggregate_wall_ms": null, "repetitions": 1},
  "stability": "NOT_COMPARABLE",
  "comparable": false,
  "gate_eligible": false,
  "persistent_session": null,
  "summary": {
    "mean_wall_ms": 1.0, "median_wall_ms": 1.0, "min_wall_ms": 1.0, "max_wall_ms": 1.0,
    "stddev_wall_ms": 0.0, "mad_wall_ms": 0.0, "coefficient_of_variation": 0.0,
    "bootstrap": {"seed": "17", "statistic": "median", "resamples": 10000, "low": 1.0, "high": 1.0},
    "mean_peak_working_set_bytes": 1, "median_peak_working_set_bytes": 1,
    "min_peak_working_set_bytes": 1, "max_peak_working_set_bytes": 1
  }
}]=])
zr_benchmark_task3_parse_runner_report("${profile_json}" profile)
if (profile_comparable OR profile_gate_eligible OR profile_calibration_enabled OR
    NOT profile_repetitions EQUAL 1 OR NOT profile_stability STREQUAL "NOT_COMPARABLE")
    message(FATAL_ERROR "profile runner report did not fail closed")
endif ()

message(STATUS "Benchmark Task 3 suite contract PASS")
