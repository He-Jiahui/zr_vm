foreach (required_var IN ITEMS
        EXE
        PERF_RUNNER_EXE
        PYTHON_EXE
        MEASUREMENT_CONTRACT_MODULE
        PERFORMANCE_SUITE_SCRIPT
        CSV_SCRIPT
        AGGREGATE_SCRIPT
        TEST_OUTPUT_DIR)
    if (NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "${required_var} is required.")
    endif ()
endforeach ()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

foreach (path_var IN ITEMS
        EXE
        PERF_RUNNER_EXE
        PYTHON_EXE
        MEASUREMENT_CONTRACT_MODULE
        PERFORMANCE_SUITE_SCRIPT
        CSV_SCRIPT
        AGGREGATE_SCRIPT
        TEST_OUTPUT_DIR)
    file(TO_CMAKE_PATH "${${path_var}}" ${path_var})
endforeach ()

macro(benchmark_support_fail message_text)
    math(EXPR benchmark_support_failures "${benchmark_support_failures} + 1")
    string(APPEND benchmark_support_failure_messages "\n- ${message_text}")
endmacro()

macro(benchmark_support_expect_json_string json_text field_name expected_value label)
    string(JSON benchmark_support_value ERROR_VARIABLE benchmark_support_error GET "${json_text}" "${field_name}")
    if (NOT benchmark_support_error STREQUAL "NOTFOUND")
        benchmark_support_fail("${label}: missing or invalid ${field_name}: ${benchmark_support_error}")
    elseif (NOT benchmark_support_value STREQUAL "${expected_value}")
        benchmark_support_fail("${label}: expected ${field_name}=${expected_value}, got ${benchmark_support_value}")
    endif ()
endmacro()

macro(benchmark_support_expect_json_false json_text field_name label)
    string(JSON benchmark_support_type ERROR_VARIABLE benchmark_support_type_error TYPE "${json_text}" "${field_name}")
    string(JSON benchmark_support_value ERROR_VARIABLE benchmark_support_value_error GET "${json_text}" "${field_name}")
    if (NOT benchmark_support_type_error STREQUAL "NOTFOUND" OR
            NOT benchmark_support_value_error STREQUAL "NOTFOUND" OR
            NOT benchmark_support_type STREQUAL "BOOLEAN" OR
            benchmark_support_value)
        benchmark_support_fail("${label}: expected JSON false field ${field_name}")
    endif ()
endmacro()

set(benchmark_support_failures 0)
set(benchmark_support_failure_messages "")
file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

file(READ "${PERFORMANCE_SUITE_SCRIPT}" performance_suite_source)
if (NOT performance_suite_source MATCHES "pub fn scale\\(\\): int")
    benchmark_support_fail("performance suite does not generate canonical ZR bench_config syntax")
endif ()
if (performance_suite_source MATCHES "pub scale\\(\\): int")
    benchmark_support_fail("performance suite still generates removed keywordless ZR function syntax")
endif ()

set(scope_json "${TEST_OUTPUT_DIR}/runner_scope.json")
execute_process(
        COMMAND
        "${PERF_RUNNER_EXE}"
        --name support_fixture
        --iterations 1
        --warmup 0
        --json-out "${scope_json}"
        --measurement-scope process_end_to_end
        --prepare-scope none
        --runtime-reused false
        --compiler-reused false
        --jit-state-reused false
        -- "${EXE}"
        WORKING_DIRECTORY "${TEST_OUTPUT_DIR}"
        RESULT_VARIABLE scope_result
        OUTPUT_VARIABLE scope_stdout
        ERROR_VARIABLE scope_stderr)
if (NOT scope_result EQUAL 0)
    benchmark_support_fail("perf runner rejected the required measurement contract: ${scope_stderr}")
elseif (NOT EXISTS "${scope_json}")
    benchmark_support_fail("perf runner did not write ${scope_json}")
else ()
    file(READ "${scope_json}" scope_json_text)
    benchmark_support_expect_json_string("${scope_json_text}" measurement_scope process_end_to_end "perf runner")
    benchmark_support_expect_json_string("${scope_json_text}" prepare_scope none "perf runner")
    benchmark_support_expect_json_false("${scope_json_text}" runtime_reused "perf runner")
    benchmark_support_expect_json_false("${scope_json_text}" compiler_reused "perf runner")
    benchmark_support_expect_json_false("${scope_json_text}" jit_state_reused "perf runner")
endif ()

execute_process(
        COMMAND
        "${PERF_RUNNER_EXE}"
        --name missing_contract
        --iterations 1
        --warmup 0
        --json-out "${TEST_OUTPUT_DIR}/missing_contract.json"
        -- "${EXE}"
        WORKING_DIRECTORY "${TEST_OUTPUT_DIR}"
        RESULT_VARIABLE missing_contract_result
        OUTPUT_QUIET
        ERROR_QUIET)
if (missing_contract_result EQUAL 0)
    benchmark_support_fail("perf runner accepted a request with all measurement contract options missing")
endif ()

execute_process(
        COMMAND
        "${PERF_RUNNER_EXE}"
        --name invalid_bool
        --iterations 1
        --warmup 0
        --json-out "${TEST_OUTPUT_DIR}/invalid_bool.json"
        --measurement-scope process_end_to_end
        --prepare-scope none
        --runtime-reused no
        --compiler-reused false
        --jit-state-reused false
        -- "${EXE}"
        WORKING_DIRECTORY "${TEST_OUTPUT_DIR}"
        RESULT_VARIABLE invalid_bool_result
        OUTPUT_QUIET
        ERROR_QUIET)
if (invalid_bool_result EQUAL 0)
    benchmark_support_fail("perf runner accepted a non-canonical boolean")
endif ()

include("${MEASUREMENT_CONTRACT_MODULE}" OPTIONAL RESULT_VARIABLE measurement_contract_loaded)
if (measurement_contract_loaded STREQUAL "NOTFOUND")
    benchmark_support_fail("measurement contract module is missing")
else ()
    macro(benchmark_support_expect_mapping implementation_id expected_prepare_scope)
        zr_benchmark_measurement_contract_get("${implementation_id}"
                mapped_measurement_scope
                mapped_prepare_scope
                mapped_runtime_reused
                mapped_compiler_reused
                mapped_jit_state_reused)
        if (NOT mapped_measurement_scope STREQUAL "process_end_to_end" OR
                NOT mapped_prepare_scope STREQUAL "${expected_prepare_scope}" OR
                NOT mapped_runtime_reused STREQUAL "false" OR
                NOT mapped_compiler_reused STREQUAL "false" OR
                NOT mapped_jit_state_reused STREQUAL "false")
            benchmark_support_fail("${implementation_id} measurement mapping is incorrect")
        endif ()
    endmacro()

    benchmark_support_expect_mapping("c" "none")
    benchmark_support_expect_mapping("rust" "none")
    benchmark_support_expect_mapping("zr_interp" "source_load_compile_in_measurement")
    benchmark_support_expect_mapping("zr_binary" "bytecode_compile_before_measurement")
    benchmark_support_expect_mapping("python" "script_load_in_measurement")
    benchmark_support_expect_mapping("node" "script_load_in_measurement")
    benchmark_support_expect_mapping("qjs" "script_load_in_measurement")
    benchmark_support_expect_mapping("lua" "script_load_in_measurement")
    benchmark_support_expect_mapping("dotnet" "runtime_start_jit_in_measurement")
    benchmark_support_expect_mapping("java" "runtime_start_jit_in_measurement")

    foreach (persistent_prepare_scope IN ITEMS
            script_load_before_measurement
            runtime_start_before_measurement
            bytecode_compile_and_load_before_measurement)
        zr_benchmark_measurement_contract_validate(
                persistent_runtime
                "${persistent_prepare_scope}"
                true false false
                persistent_contract_valid)
        if (NOT persistent_contract_valid)
            benchmark_support_fail("persistent prepare scope rejected: ${persistent_prepare_scope}")
        endif ()
    endforeach ()

    zr_benchmark_measurement_contract_ratio("1.000" "2.000"
            "process_end_to_end" "process_end_to_end" compatible_ratio)
    zr_benchmark_measurement_contract_ratio("1.000" "2.000"
            "process_end_to_end" "persistent_runtime" mismatched_ratio)
    zr_benchmark_measurement_contract_ratio("1.000" "2.000"
            "" "process_end_to_end" missing_scope_ratio)
    if (NOT compatible_ratio STREQUAL "0.500")
        benchmark_support_fail("compatible scope ratio expected 0.500, got ${compatible_ratio}")
    endif ()
    if (NOT mismatched_ratio STREQUAL "null")
        benchmark_support_fail("mismatched scope ratio must be null, got ${mismatched_ratio}")
    endif ()
    if (NOT missing_scope_ratio STREQUAL "null")
        benchmark_support_fail("missing scope ratio must be null, got ${missing_scope_ratio}")
    endif ()
endif ()

set(tests_generated_dir "${TEST_OUTPUT_DIR}/tests_generated")
set(performance_dir "${tests_generated_dir}/performance")
file(MAKE_DIRECTORY "${performance_dir}")
file(WRITE "${performance_dir}/benchmark_report.json" [=[
{
  "suite": "performance_report",
  "generated_at_utc": "2026-08-29T00:00:00Z",
  "tier": "smoke",
  "warmup": 0,
  "iterations": 1,
  "cases": [{
    "name": "synthetic",
    "description": "measurement contract fixture",
    "workload_tag": "fixture",
    "scale": 1,
    "expected_checksum": "1",
    "implementations": [{
      "name": "C",
      "language": "C",
      "mode": "native",
      "status": "PASS",
      "measurement_scope": "process_end_to_end",
      "prepare_scope": "none",
      "runtime_reused": false,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0},
      "relative_to_c": 1.0
    }, {
      "name": "Lua",
      "language": "Lua",
      "mode": "script",
      "status": "SKIP",
      "measurement_scope": "persistent_runtime",
      "prepare_scope": "script_load_before_measurement",
      "runtime_reused": true,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": null,
      "relative_to_c": null
    }]
  }]
}
]=])
file(WRITE "${performance_dir}/comparison_report.json" [=[
{
  "suite": "comparison_report",
  "generated_at_utc": "2026-08-29T00:00:00Z",
  "tier": "smoke",
  "cases": [{
    "name": "synthetic",
    "workload_tag": "fixture",
    "measurement_scope": "process_end_to_end",
    "relative_to": {"c": null}
  }]
}
]=])

execute_process(
        COMMAND "${PYTHON_EXE}" "${CSV_SCRIPT}" --report-dir "${performance_dir}"
        RESULT_VARIABLE csv_result
        OUTPUT_VARIABLE csv_stdout
        ERROR_VARIABLE csv_stderr)
if (NOT csv_result EQUAL 0)
    benchmark_support_fail("CSV conversion failed: ${csv_stdout}${csv_stderr}")
else ()
    set(csv_contract_json "${TEST_OUTPUT_DIR}/csv_contract.json")
    set(csv_validator [=[import csv,json,sys
required=["measurement_scope","prepare_scope","runtime_reused","compiler_reused","jit_state_reused"]
with open(sys.argv[1],newline="",encoding="utf-8") as handle:
    rows=list(csv.DictReader(handle))
valid=len(rows)==2 and all(all(field in row and row[field]!="" for field in required) for row in rows)
with open(sys.argv[2],"w",encoding="utf-8") as handle:
    json.dump({"valid":valid,"rows":rows},handle)
sys.exit(0 if valid else 1)
]=])
    execute_process(
            COMMAND "${PYTHON_EXE}" -c "${csv_validator}"
                    "${performance_dir}/benchmark_speed_timings.csv" "${csv_contract_json}"
            RESULT_VARIABLE csv_contract_result
            OUTPUT_VARIABLE csv_contract_stdout
            ERROR_VARIABLE csv_contract_stderr)
    if (NOT csv_contract_result EQUAL 0)
        benchmark_support_fail("CSV dropped measurement contract fields: ${csv_contract_stderr}")
    endif ()
endif ()

set(summary_path "${tests_generated_dir}/benchmark_suite_summary.json")
execute_process(
        COMMAND "${PYTHON_EXE}" "${AGGREGATE_SCRIPT}"
                --tests-generated "${tests_generated_dir}"
                --out-json "${summary_path}"
                --skip-viewer-json
        RESULT_VARIABLE aggregate_result
        OUTPUT_VARIABLE aggregate_stdout
        ERROR_VARIABLE aggregate_stderr)
if (NOT aggregate_result EQUAL 0 OR NOT EXISTS "${summary_path}")
    benchmark_support_fail("aggregate conversion failed: ${aggregate_stdout}${aggregate_stderr}")
else ()
    file(READ "${summary_path}" summary_json)
    string(JSON summary_schema ERROR_VARIABLE summary_schema_error GET "${summary_json}" schema_version)
    string(JSON summary_contract_valid ERROR_VARIABLE summary_contract_error GET "${summary_json}" measurement_contract valid)
    string(JSON summary_issue_count ERROR_VARIABLE summary_issue_error LENGTH "${summary_json}" measurement_contract issues)
    if (NOT summary_schema_error STREQUAL "NOTFOUND" OR NOT summary_schema EQUAL 2)
        benchmark_support_fail("aggregate schema_version must be 2")
    endif ()
    if (NOT summary_contract_error STREQUAL "NOTFOUND" OR NOT summary_contract_valid)
        benchmark_support_fail("aggregate rejected a valid measurement contract")
    endif ()
    if (NOT summary_issue_error STREQUAL "NOTFOUND" OR NOT summary_issue_count EQUAL 0)
        benchmark_support_fail("valid aggregate must have no measurement contract issues")
    endif ()
endif ()

file(WRITE "${performance_dir}/benchmark_report.json" [=[
{
  "suite": "performance_report",
  "cases": [{
    "name": "missing_scope",
    "implementations": [{
      "name": "ZR interp",
      "language": "ZR",
      "mode": "interp",
      "status": "PASS",
      "summary": {"mean_wall_ms": 1.0},
      "relative_to_c": 9.999
    }, {
      "name": "C",
      "language": "C",
      "mode": "native",
      "status": "PASS",
      "measurement_scope": "process_end_to_end",
      "prepare_scope": "none",
      "runtime_reused": false,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0},
      "relative_to_c": 1.0
    }]
  }, {
    "name": "mismatched_scope",
    "implementations": [{
      "name": "ZR interp",
      "language": "ZR",
      "mode": "interp",
      "status": "PASS",
      "measurement_scope": "process_end_to_end",
      "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0},
      "relative_to_c": 9.999
    }, {
      "name": "C",
      "language": "C",
      "mode": "native",
      "status": "PASS",
      "measurement_scope": "persistent_runtime",
      "prepare_scope": "none",
      "runtime_reused": true,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0},
      "relative_to_c": 1.0
    }]
  }, {
    "name": "comparison_scope_mismatch",
    "implementations": [{
      "name": "ZR interp",
      "language": "ZR",
      "mode": "interp",
      "status": "PASS",
      "measurement_scope": "process_end_to_end",
      "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0},
      "relative_to_c": 9.999
    }, {
      "name": "C",
      "language": "C",
      "mode": "native",
      "status": "PASS",
      "measurement_scope": "process_end_to_end",
      "prepare_scope": "none",
      "runtime_reused": false,
      "compiler_reused": false,
      "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0},
      "relative_to_c": 1.0
    }]
  }]
}
]=])
file(WRITE "${performance_dir}/comparison_report.json" [=[
{
  "suite": "comparison_report",
  "cases": [{
    "name": "missing_scope",
    "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "9.999"}
  }, {
    "name": "mismatched_scope",
    "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "9.999"}
  }, {
    "name": "comparison_scope_mismatch",
    "measurement_scope": "persistent_runtime",
    "relative_to": {"c": "9.999"}
  }]
}
]=])

execute_process(
        COMMAND "${PYTHON_EXE}" "${CSV_SCRIPT}" --report-dir "${performance_dir}"
        RESULT_VARIABLE invalid_csv_result
        OUTPUT_VARIABLE invalid_csv_stdout
        ERROR_VARIABLE invalid_csv_stderr)
if (NOT invalid_csv_result EQUAL 0)
    benchmark_support_fail("invalid-scope CSV conversion failed: ${invalid_csv_stdout}${invalid_csv_stderr}")
else ()
    set(invalid_csv_contract_json "${TEST_OUTPUT_DIR}/invalid_csv_contract.json")
    set(invalid_csv_validator [=[import csv,json,sys
with open(sys.argv[1],newline="",encoding="utf-8") as handle:
    timing={(row["case_name"],row["implementation_name"]):row for row in csv.DictReader(handle)}
with open(sys.argv[2],newline="",encoding="utf-8") as handle:
    comparison={row["case_name"]:row for row in csv.DictReader(handle)}
checks={
    "missing_timing_ratio_empty": timing[("missing_scope","ZR interp")]["speed_ratio_vs_c_baseline"]=="",
    "mismatched_timing_ratio_empty": timing[("mismatched_scope","ZR interp")]["speed_ratio_vs_c_baseline"]=="",
    "valid_c_self_ratio_retained": timing[("mismatched_scope","C")]["speed_ratio_vs_c_baseline"]!="",
    "missing_comparison_ratio_empty": comparison["missing_scope"]["zr_interp_vs_c"]=="",
    "mismatched_comparison_ratio_empty": comparison["mismatched_scope"]["zr_interp_vs_c"]=="",
    "comparison_record_scope_mismatch_empty": comparison["comparison_scope_mismatch"]["zr_interp_vs_c"]=="",
}
valid=all(checks.values())
with open(sys.argv[3],"w",encoding="utf-8") as handle:
    json.dump({"valid":valid,"checks":checks},handle)
sys.exit(0 if valid else 1)
]=])
    execute_process(
            COMMAND "${PYTHON_EXE}" -c "${invalid_csv_validator}"
                    "${performance_dir}/benchmark_speed_timings.csv"
                    "${performance_dir}/zr_interp_vs_languages.csv"
                    "${invalid_csv_contract_json}"
            RESULT_VARIABLE invalid_csv_contract_result
            OUTPUT_VARIABLE invalid_csv_contract_stdout
            ERROR_VARIABLE invalid_csv_contract_stderr)
    if (NOT invalid_csv_contract_result EQUAL 0)
        benchmark_support_fail("CSV retained a ratio without compatible measurement contracts: ${invalid_csv_contract_stderr}")
    else ()
        file(READ "${invalid_csv_contract_json}" invalid_csv_contract_text)
        string(JSON invalid_csv_contract_valid ERROR_VARIABLE invalid_csv_contract_error GET
                "${invalid_csv_contract_text}" valid)
        if (NOT invalid_csv_contract_error STREQUAL "NOTFOUND" OR NOT invalid_csv_contract_valid)
            benchmark_support_fail("structured invalid-scope CSV contract result was false")
        endif ()
    endif ()
endif ()

execute_process(
        COMMAND "${PYTHON_EXE}" "${AGGREGATE_SCRIPT}"
                --tests-generated "${tests_generated_dir}"
                --out-json "${summary_path}"
                --skip-viewer-json
        RESULT_VARIABLE invalid_aggregate_result
        OUTPUT_VARIABLE invalid_aggregate_stdout
        ERROR_VARIABLE invalid_aggregate_stderr)
if (NOT invalid_aggregate_result EQUAL 0 OR NOT EXISTS "${summary_path}")
    benchmark_support_fail("invalid aggregate inspection failed: ${invalid_aggregate_stdout}${invalid_aggregate_stderr}")
else ()
    file(READ "${summary_path}" invalid_summary_json)
    string(JSON invalid_contract_valid ERROR_VARIABLE invalid_contract_error GET "${invalid_summary_json}" measurement_contract valid)
    string(JSON invalid_issue_count ERROR_VARIABLE invalid_issue_error LENGTH "${invalid_summary_json}" measurement_contract issues)
    string(JSON invalid_removed_count ERROR_VARIABLE invalid_removed_error LENGTH "${invalid_summary_json}" measurement_contract ratios_removed)
    string(JSON invalid_impl_ratio_type ERROR_VARIABLE invalid_impl_ratio_error TYPE
            "${invalid_summary_json}" reports benchmark_report cases 0 implementations 0 relative_to_c)
    string(JSON mismatched_impl_ratio_type ERROR_VARIABLE mismatched_impl_ratio_error TYPE
            "${invalid_summary_json}" reports benchmark_report cases 1 implementations 0 relative_to_c)
    string(JSON valid_c_ratio_type ERROR_VARIABLE valid_c_ratio_error TYPE
            "${invalid_summary_json}" reports benchmark_report cases 1 implementations 1 relative_to_c)
    string(JSON invalid_comparison_ratio_type ERROR_VARIABLE invalid_comparison_ratio_error TYPE
            "${invalid_summary_json}" reports comparison_report cases 0 relative_to c)
    string(JSON mismatched_comparison_ratio_type ERROR_VARIABLE mismatched_comparison_ratio_error TYPE
            "${invalid_summary_json}" reports comparison_report cases 1 relative_to c)
    string(JSON isolated_comparison_ratio_type ERROR_VARIABLE isolated_comparison_ratio_error TYPE
            "${invalid_summary_json}" reports comparison_report cases 2 relative_to c)
    string(JSON isolated_comparison_scope ERROR_VARIABLE isolated_comparison_scope_error GET
            "${invalid_summary_json}" reports comparison_report cases 2 measurement_scope)
    if (NOT invalid_contract_error STREQUAL "NOTFOUND" OR invalid_contract_valid)
        benchmark_support_fail("aggregate did not reject a missing measurement scope")
    endif ()
    if (NOT invalid_issue_error STREQUAL "NOTFOUND" OR invalid_issue_count LESS 1)
        benchmark_support_fail("aggregate did not report the missing contract issue")
    endif ()
    if (NOT invalid_removed_error STREQUAL "NOTFOUND" OR invalid_removed_count LESS 5)
        benchmark_support_fail("aggregate did not record all removed implementation/comparison ratios")
    endif ()
    if (NOT invalid_impl_ratio_error STREQUAL "NOTFOUND" OR NOT invalid_impl_ratio_type STREQUAL "NULL")
        benchmark_support_fail("aggregate retained implementation.relative_to_c with a missing scope")
    endif ()
    if (NOT mismatched_impl_ratio_error STREQUAL "NOTFOUND" OR NOT mismatched_impl_ratio_type STREQUAL "NULL")
        benchmark_support_fail("aggregate retained implementation.relative_to_c with mismatched scopes")
    endif ()
    if (NOT valid_c_ratio_error STREQUAL "NOTFOUND" OR NOT valid_c_ratio_type STREQUAL "NUMBER")
        benchmark_support_fail("aggregate removed a valid C self-ratio")
    endif ()
    if (NOT invalid_comparison_ratio_error STREQUAL "NOTFOUND" OR NOT invalid_comparison_ratio_type STREQUAL "NULL")
        benchmark_support_fail("aggregate retained comparison ratio with a missing scope")
    endif ()
    if (NOT mismatched_comparison_ratio_error STREQUAL "NOTFOUND" OR NOT mismatched_comparison_ratio_type STREQUAL "NULL")
        benchmark_support_fail("aggregate retained comparison ratio with mismatched scopes")
    endif ()
    if (NOT isolated_comparison_ratio_error STREQUAL "NOTFOUND" OR NOT isolated_comparison_ratio_type STREQUAL "NULL")
        benchmark_support_fail("aggregate retained ratio when only the comparison record scope mismatched")
    endif ()
    if (NOT isolated_comparison_scope_error STREQUAL "NOTFOUND" OR
            NOT isolated_comparison_scope STREQUAL "persistent_runtime")
        benchmark_support_fail("aggregate rewrote the comparison record measurement_scope")
    endif ()
endif ()

file(WRITE "${performance_dir}/benchmark_report.json" [=[
{
  "suite": "performance_report",
  "cases": [{
    "name": "duplicate_case",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0}, "relative_to_c": 9.999
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0}, "relative_to_c": 1.0
    }]
  }, {
    "name": "duplicate_case",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.1}, "relative_to_c": 9.999
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.1}, "relative_to_c": 1.0
    }]
  }, {
    "name": "duplicate_impl",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0}, "relative_to_c": 9.999
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0}, "relative_to_c": 1.0
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 3.0}, "relative_to_c": 1.0
    }]
  }, {
    "name": "valid_case",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0}, "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0}, "relative_to_c": 1.0
    }]
  }, {
    "name": "unique_control",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 1.0}, "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "summary": {"mean_wall_ms": 2.0}, "relative_to_c": 1.0
    }]
  }]
}
]=])
file(WRITE "${performance_dir}/comparison_report.json" [=[
{
  "suite": "comparison_report",
  "cases": [{
    "name": "duplicate_case", "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "9.999"}
  }, {
    "name": "duplicate_impl", "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "9.999"}
  }, {
    "name": "valid_case", "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "0.500"}
  }, {
    "name": "valid_case", "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "0.500"}
  }, {
    "name": "unique_control", "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "0.500"}
  }]
}
]=])

execute_process(
        COMMAND "${PYTHON_EXE}" "${CSV_SCRIPT}" --report-dir "${performance_dir}"
        RESULT_VARIABLE duplicate_csv_result
        OUTPUT_VARIABLE duplicate_csv_stdout
        ERROR_VARIABLE duplicate_csv_stderr)
if (NOT duplicate_csv_result EQUAL 0)
    benchmark_support_fail("duplicate-identity CSV conversion failed: ${duplicate_csv_stdout}${duplicate_csv_stderr}")
else ()
    set(duplicate_csv_contract_json "${TEST_OUTPUT_DIR}/duplicate_csv_contract.json")
    set(duplicate_csv_validator [=[import csv,json,sys
with open(sys.argv[1],newline="",encoding="utf-8") as handle:
    timing=list(csv.DictReader(handle))
with open(sys.argv[2],newline="",encoding="utf-8") as handle:
    comparison=list(csv.DictReader(handle))
ambiguous_timing=[row for row in timing if row["case_name"] in {"duplicate_case","duplicate_impl"}]
valid_timing=[row for row in timing if row["case_name"] in {"valid_case","unique_control"}]
ambiguous_comparison=[row for row in comparison if row["case_name"]!="unique_control"]
valid_comparison=[row for row in comparison if row["case_name"]=="unique_control"]
checks={
    "all_ambiguous_timing_ratios_empty": len(ambiguous_timing)==7 and all(row["speed_ratio_vs_c_baseline"]=="" for row in ambiguous_timing),
    "valid_timing_ratios_retained": len(valid_timing)==4 and all(row["speed_ratio_vs_c_baseline"]!="" for row in valid_timing),
    "all_ambiguous_comparison_ratios_empty": len(ambiguous_comparison)==4 and all(row["zr_interp_vs_c"]=="" for row in ambiguous_comparison),
    "unique_comparison_ratio_retained": len(valid_comparison)==1 and valid_comparison[0]["zr_interp_vs_c"]!="",
}
valid=all(checks.values())
with open(sys.argv[3],"w",encoding="utf-8") as handle:
    json.dump({"valid":valid,"checks":checks},handle)
sys.exit(0 if valid else 1)
]=])
    execute_process(
            COMMAND "${PYTHON_EXE}" -c "${duplicate_csv_validator}"
                    "${performance_dir}/benchmark_speed_timings.csv"
                    "${performance_dir}/zr_interp_vs_languages.csv"
                    "${duplicate_csv_contract_json}"
            RESULT_VARIABLE duplicate_csv_contract_result
            OUTPUT_VARIABLE duplicate_csv_contract_stdout
            ERROR_VARIABLE duplicate_csv_contract_stderr)
    if (NOT duplicate_csv_contract_result EQUAL 0)
        benchmark_support_fail("CSV did not fail closed for duplicate identities: ${duplicate_csv_contract_stderr}")
    endif ()
endif ()

set(duplicate_summary_path "${tests_generated_dir}/benchmark_suite_summary_duplicates.json")
execute_process(
        COMMAND "${PYTHON_EXE}" "${AGGREGATE_SCRIPT}"
                --tests-generated "${tests_generated_dir}"
                --out-json "${duplicate_summary_path}"
                --skip-viewer-json
        RESULT_VARIABLE duplicate_aggregate_result
        OUTPUT_VARIABLE duplicate_aggregate_stdout
        ERROR_VARIABLE duplicate_aggregate_stderr)
if (NOT duplicate_aggregate_result EQUAL 0 OR NOT EXISTS "${duplicate_summary_path}")
    benchmark_support_fail("duplicate-identity aggregate inspection failed: ${duplicate_aggregate_stdout}${duplicate_aggregate_stderr}")
else ()
    file(READ "${duplicate_summary_path}" duplicate_summary_json)
    string(JSON duplicate_contract_valid ERROR_VARIABLE duplicate_contract_error GET
            "${duplicate_summary_json}" measurement_contract valid)
    string(JSON duplicate_issue_count ERROR_VARIABLE duplicate_issue_error LENGTH
            "${duplicate_summary_json}" measurement_contract issues)
    string(JSON duplicate_removed_count ERROR_VARIABLE duplicate_removed_error LENGTH
            "${duplicate_summary_json}" measurement_contract ratios_removed)
    if (NOT duplicate_contract_error STREQUAL "NOTFOUND" OR duplicate_contract_valid)
        benchmark_support_fail("aggregate accepted duplicate benchmark/comparison identities")
    endif ()
    if (NOT duplicate_issue_error STREQUAL "NOTFOUND" OR duplicate_issue_count LESS 3)
        benchmark_support_fail("aggregate did not report useful duplicate identity issues")
    endif ()
    if (NOT duplicate_removed_error STREQUAL "NOTFOUND" OR duplicate_removed_count LESS 11)
        benchmark_support_fail("aggregate did not record all duplicate-identity ratio removals")
    endif ()
    foreach (duplicate_ratio_path IN ITEMS
            "reports;benchmark_report;cases;0;implementations;0;relative_to_c"
            "reports;benchmark_report;cases;1;implementations;1;relative_to_c"
            "reports;benchmark_report;cases;2;implementations;0;relative_to_c"
            "reports;benchmark_report;cases;2;implementations;1;relative_to_c"
            "reports;benchmark_report;cases;2;implementations;2;relative_to_c"
            "reports;comparison_report;cases;0;relative_to;c"
            "reports;comparison_report;cases;1;relative_to;c"
            "reports;comparison_report;cases;2;relative_to;c"
            "reports;comparison_report;cases;3;relative_to;c")
        string(JSON duplicate_ratio_type ERROR_VARIABLE duplicate_ratio_error TYPE
                "${duplicate_summary_json}" ${duplicate_ratio_path})
        if (NOT duplicate_ratio_error STREQUAL "NOTFOUND" OR NOT duplicate_ratio_type STREQUAL "NULL")
            benchmark_support_fail("aggregate retained ambiguous ratio at ${duplicate_ratio_path}")
        endif ()
    endforeach ()
    string(JSON valid_unique_ratio_type ERROR_VARIABLE valid_unique_ratio_error TYPE
            "${duplicate_summary_json}" reports benchmark_report cases 3 implementations 0 relative_to_c)
    if (NOT valid_unique_ratio_error STREQUAL "NOTFOUND" OR NOT valid_unique_ratio_type STREQUAL "NUMBER")
        benchmark_support_fail("aggregate removed an unambiguous benchmark ratio")
    endif ()
    string(JSON unique_comparison_ratio_type ERROR_VARIABLE unique_comparison_ratio_error TYPE
            "${duplicate_summary_json}" reports comparison_report cases 4 relative_to c)
    if (NOT unique_comparison_ratio_error STREQUAL "NOTFOUND" OR
            NOT unique_comparison_ratio_type STREQUAL "STRING")
        benchmark_support_fail("aggregate removed the unique valid comparison ratio")
    endif ()
endif ()

file(WRITE "${performance_dir}/benchmark_report.json" [=[
{
  "suite": "performance_report",
  "cases": [{
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }, {
    "name": "",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }, {
    "name": "   ",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }, {
    "name": 17,
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }, {
    "name": "malformed_impls",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }, {
      "status": "PASS", "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false, "relative_to_c": 2.0
    }, {
      "name": "", "status": "PASS", "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false, "relative_to_c": 2.0
    }, {
      "name": "   ", "status": "PASS", "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false, "relative_to_c": 2.0
    }, {
      "name": 17, "status": "PASS", "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false, "relative_to_c": 2.0
    }]
  }, {
    "name": "malformed_identity_control",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }]
}
]=])
file(WRITE "${performance_dir}/comparison_report.json" [=[
{
  "suite": "comparison_report",
  "cases": [{
    "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }, {
    "name": "", "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }, {
    "name": "   ", "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }, {
    "name": 17, "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }, {
    "name": "malformed_impls", "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }, {
    "name": "malformed_identity_control", "measurement_scope": "process_end_to_end", "relative_to": {"c": "0.500"}
  }]
}
]=])

execute_process(
        COMMAND "${PYTHON_EXE}" "${CSV_SCRIPT}" --report-dir "${performance_dir}"
        RESULT_VARIABLE malformed_csv_result
        OUTPUT_VARIABLE malformed_csv_stdout
        ERROR_VARIABLE malformed_csv_stderr)
if (NOT malformed_csv_result EQUAL 0)
    benchmark_support_fail("malformed-identity CSV conversion failed: ${malformed_csv_stdout}${malformed_csv_stderr}")
else ()
    set(malformed_csv_contract_json "${TEST_OUTPUT_DIR}/malformed_csv_contract.json")
    set(malformed_csv_validator [=[import csv,json,sys
with open(sys.argv[1],newline="",encoding="utf-8") as handle:
    timing=list(csv.DictReader(handle))
with open(sys.argv[2],newline="",encoding="utf-8") as handle:
    comparison=list(csv.DictReader(handle))
invalid_timing=[row for row in timing if row["case_name"]!="malformed_identity_control"]
control_timing=[row for row in timing if row["case_name"]=="malformed_identity_control"]
invalid_comparison=[row for row in comparison if row["case_name"]!="malformed_identity_control"]
control_comparison=[row for row in comparison if row["case_name"]=="malformed_identity_control"]
checks={
    "malformed_timing_ratios_empty": len(invalid_timing)==14 and all(row["speed_ratio_vs_c_baseline"]=="" for row in invalid_timing),
    "control_timing_ratios_retained": len(control_timing)==2 and all(row["speed_ratio_vs_c_baseline"]!="" for row in control_timing),
    "malformed_comparison_ratios_empty": len(invalid_comparison)==5 and all(row["zr_interp_vs_c"]=="" for row in invalid_comparison),
    "control_comparison_ratio_retained": len(control_comparison)==1 and control_comparison[0]["zr_interp_vs_c"]!="",
}
valid=all(checks.values())
with open(sys.argv[3],"w",encoding="utf-8") as handle:
    json.dump({"valid":valid,"checks":checks},handle)
sys.exit(0 if valid else 1)
]=])
    execute_process(
            COMMAND "${PYTHON_EXE}" -c "${malformed_csv_validator}"
                    "${performance_dir}/benchmark_speed_timings.csv"
                    "${performance_dir}/zr_interp_vs_languages.csv"
                    "${malformed_csv_contract_json}"
            RESULT_VARIABLE malformed_csv_contract_result
            OUTPUT_VARIABLE malformed_csv_contract_stdout
            ERROR_VARIABLE malformed_csv_contract_stderr)
    if (NOT malformed_csv_contract_result EQUAL 0)
        benchmark_support_fail("CSV did not fail closed for malformed identities: ${malformed_csv_contract_stderr}")
    endif ()
endif ()

set(malformed_summary_path "${tests_generated_dir}/benchmark_suite_summary_malformed_identities.json")
execute_process(
        COMMAND "${PYTHON_EXE}" "${AGGREGATE_SCRIPT}"
                --tests-generated "${tests_generated_dir}"
                --out-json "${malformed_summary_path}"
                --skip-viewer-json
        RESULT_VARIABLE malformed_aggregate_result
        OUTPUT_VARIABLE malformed_aggregate_stdout
        ERROR_VARIABLE malformed_aggregate_stderr)
if (NOT malformed_aggregate_result EQUAL 0 OR NOT EXISTS "${malformed_summary_path}")
    benchmark_support_fail("malformed-identity aggregate inspection failed: ${malformed_aggregate_stdout}${malformed_aggregate_stderr}")
else ()
    set(malformed_aggregate_validator [=[import json,sys
with open(sys.argv[1],encoding="utf-8") as handle:
    summary=json.load(handle)
benchmark=summary["reports"]["benchmark_report"]["cases"]
comparison=summary["reports"]["comparison_report"]["cases"]
invalid_benchmark=benchmark[:5]
control_benchmark=benchmark[5]
invalid_comparison=comparison[:5]
control_comparison=comparison[5]
checks={
    "contract_invalid": summary["measurement_contract"]["valid"] is False,
    "identity_issues_reported": len(summary["measurement_contract"]["issues"])>=12,
    "removals_recorded": len(summary["measurement_contract"]["ratios_removed"])>=19,
    "malformed_benchmark_ratios_null": all(impl.get("relative_to_c") is None for case in invalid_benchmark for impl in case["implementations"]),
    "control_benchmark_ratios_retained": all(impl.get("relative_to_c") is not None for impl in control_benchmark["implementations"]),
    "malformed_comparison_ratios_null": all(case["relative_to"]["c"] is None for case in invalid_comparison),
    "control_comparison_ratio_retained": control_comparison["relative_to"]["c"] is not None,
    "benchmark_names_preserved": "name" not in benchmark[0] and benchmark[1]["name"]=="" and benchmark[2]["name"]=="   " and benchmark[3]["name"]==17,
    "implementation_names_preserved": "name" not in benchmark[4]["implementations"][2] and benchmark[4]["implementations"][3]["name"]=="" and benchmark[4]["implementations"][4]["name"]=="   " and benchmark[4]["implementations"][5]["name"]==17,
    "comparison_names_preserved": "name" not in comparison[0] and comparison[1]["name"]=="" and comparison[2]["name"]=="   " and comparison[3]["name"]==17,
}
valid=all(checks.values())
with open(sys.argv[2],"w",encoding="utf-8") as handle:
    json.dump({"valid":valid,"checks":checks},handle)
sys.exit(0 if valid else 1)
]=])
    set(malformed_aggregate_contract_json "${TEST_OUTPUT_DIR}/malformed_aggregate_contract.json")
    execute_process(
            COMMAND "${PYTHON_EXE}" -c "${malformed_aggregate_validator}"
                    "${malformed_summary_path}" "${malformed_aggregate_contract_json}"
            RESULT_VARIABLE malformed_aggregate_contract_result
            OUTPUT_VARIABLE malformed_aggregate_contract_stdout
            ERROR_VARIABLE malformed_aggregate_contract_stderr)
    if (NOT malformed_aggregate_contract_result EQUAL 0)
        benchmark_support_fail("aggregate did not fail closed for malformed identities: ${malformed_aggregate_contract_stderr}")
    endif ()
endif ()

file(WRITE "${performance_dir}/benchmark_report.json" [=[
{
  "suite": "performance_report",
  "cases": [{
    "name": "structural_control",
    "implementations": [{
      "name": "ZR interp", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "source_load_compile_in_measurement",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 0.5
    }, {
      "name": "C", "status": "PASS",
      "measurement_scope": "process_end_to_end", "prepare_scope": "none",
      "runtime_reused": false, "compiler_reused": false, "jit_state_reused": false,
      "relative_to_c": 1.0
    }]
  }]
}
]=])
file(WRITE "${performance_dir}/comparison_report.json" [=[
{
  "suite": "comparison_report",
  "cases": [17, {
    "name": "malformed_relative_to",
    "measurement_scope": "process_end_to_end",
    "relative_to": []
  }, {
    "name": "structural_control",
    "measurement_scope": "process_end_to_end",
    "relative_to": {"c": "0.500"}
  }]
}
]=])
set(structural_summary_path "${tests_generated_dir}/benchmark_suite_summary_structural.json")
execute_process(
        COMMAND "${PYTHON_EXE}" "${AGGREGATE_SCRIPT}"
                --tests-generated "${tests_generated_dir}"
                --out-json "${structural_summary_path}"
                --skip-viewer-json
        RESULT_VARIABLE structural_aggregate_result
        OUTPUT_VARIABLE structural_aggregate_stdout
        ERROR_VARIABLE structural_aggregate_stderr)
if (NOT structural_aggregate_result EQUAL 0 OR NOT EXISTS "${structural_summary_path}")
    benchmark_support_fail("structural aggregate inspection failed: ${structural_aggregate_stdout}${structural_aggregate_stderr}")
else ()
    file(READ "${structural_summary_path}" structural_summary_json)
    string(JSON structural_contract_valid ERROR_VARIABLE structural_contract_error GET
            "${structural_summary_json}" measurement_contract valid)
    string(JSON structural_issue_count ERROR_VARIABLE structural_issue_error LENGTH
            "${structural_summary_json}" measurement_contract issues)
    string(JSON structural_nonobject_issue ERROR_VARIABLE structural_nonobject_issue_error GET
            "${structural_summary_json}" measurement_contract issues 0)
    string(JSON structural_relative_issue ERROR_VARIABLE structural_relative_issue_error GET
            "${structural_summary_json}" measurement_contract issues 1)
    string(JSON structural_control_ratio_type ERROR_VARIABLE structural_control_ratio_error TYPE
            "${structural_summary_json}" reports comparison_report cases 2 relative_to c)
    if (NOT structural_contract_error STREQUAL "NOTFOUND" OR structural_contract_valid)
        benchmark_support_fail("aggregate accepted malformed comparison structure")
    endif ()
    if (NOT structural_issue_error STREQUAL "NOTFOUND" OR structural_issue_count LESS 2)
        benchmark_support_fail("aggregate did not report structural comparison issues")
    endif ()
    if (NOT structural_nonobject_issue_error STREQUAL "NOTFOUND" OR
            NOT structural_nonobject_issue MATCHES "case record is not an object")
        benchmark_support_fail("aggregate did not identify a non-object comparison case")
    endif ()
    if (NOT structural_relative_issue_error STREQUAL "NOTFOUND" OR
            NOT structural_relative_issue MATCHES "relative_to must be an object")
        benchmark_support_fail("aggregate did not identify non-object comparison relative_to")
    endif ()
    if (NOT structural_control_ratio_error STREQUAL "NOTFOUND" OR
            NOT structural_control_ratio_type STREQUAL "STRING")
        benchmark_support_fail("aggregate removed the structural positive-control ratio")
    endif ()
endif ()

if (benchmark_support_failures GREATER 0)
    message(FATAL_ERROR "benchmark_support failed with ${benchmark_support_failures} issue(s):${benchmark_support_failure_messages}")
endif ()

message("Benchmark measurement contract PASS")
