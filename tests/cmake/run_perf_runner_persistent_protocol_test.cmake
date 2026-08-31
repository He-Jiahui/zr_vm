cmake_minimum_required(VERSION 3.19)

foreach (required_variable IN ITEMS PERF_RUNNER_EXE FIXTURE_EXE TEST_OUTPUT_DIR)
    if (NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif ()
endforeach ()

set(input_zr_benchmark_server_exe "${ZR_BENCHMARK_SERVER_EXE}")
if (DEFINED PERSISTENT_COMMAND_MODULE AND EXISTS "${PERSISTENT_COMMAND_MODULE}")
    include("${PERSISTENT_COMMAND_MODULE}")
    set(actual_benchmarks_dir "${BENCHMARKS_DIR}")
    set(PERF_LUA_EXE "fixture-lua")
    set(PERF_QJS_EXE "fixture-qjs")
    set(PERF_DOTNET_EXE "fixture-dotnet")
    set(PERF_DOTNET_RUNNER_DLL "fixture.dll")
    set(ZR_BENCHMARK_SERVER_EXE "fixture-zr-server")
    set(zr_project_file "fixture-project.zrp")
    set(BENCHMARKS_DIR "fixture-benchmarks")
    zr_benchmark_persistent_command_get(lua numeric_loops core
            mapped_supported mapped_command mapped_prepare mapped_runtime mapped_compiler mapped_jit)
    if (NOT mapped_supported OR NOT mapped_command MATCHES "--benchmark-server" OR
        NOT mapped_prepare STREQUAL "script_load_before_measurement" OR
        NOT mapped_runtime STREQUAL "true" OR NOT mapped_jit STREQUAL "false")
        message(FATAL_ERROR "Lua persistent command mapping is invalid")
    endif ()
    set(PERF_LUA_IS_LUAJIT TRUE)
    zr_benchmark_persistent_command_get(lua numeric_loops core
            mapped_supported mapped_command mapped_prepare mapped_runtime mapped_compiler mapped_jit)
    if (mapped_supported)
        message(FATAL_ERROR "LuaJIT was mislabeled as non-JIT persistent Lua")
    endif ()
    set(PERF_LUA_IS_LUAJIT FALSE)
    zr_benchmark_persistent_command_get(dotnet dispatch_loops stress
            mapped_supported mapped_command mapped_prepare mapped_runtime mapped_compiler mapped_jit)
    if (NOT mapped_supported OR NOT mapped_prepare STREQUAL "runtime_start_before_measurement" OR
        NOT mapped_runtime STREQUAL "true" OR NOT mapped_jit STREQUAL "true")
        message(FATAL_ERROR ".NET persistent command mapping is invalid")
    endif ()
    zr_benchmark_persistent_command_get(zr_binary numeric_loops core
            mapped_supported mapped_command mapped_prepare mapped_runtime mapped_compiler mapped_jit)
    if (NOT mapped_supported OR NOT mapped_command MATCHES "--project;fixture-project.zrp" OR
        NOT mapped_prepare STREQUAL "bytecode_compile_and_load_before_measurement" OR
        NOT mapped_runtime STREQUAL "true" OR NOT mapped_compiler STREQUAL "false" OR
        NOT mapped_jit STREQUAL "false")
        message(FATAL_ERROR "ZR binary persistent command mapping is invalid")
    endif ()
    zr_benchmark_persistent_command_get(lua sort_array core
            mapped_supported mapped_command mapped_prepare mapped_runtime mapped_compiler mapped_jit)
    if (mapped_supported OR NOT mapped_command STREQUAL "")
        message(FATAL_ERROR "unsupported steady case silently received a process fallback command")
    endif ()
    set(BENCHMARKS_DIR "${actual_benchmarks_dir}")
    set(ZR_BENCHMARK_SERVER_EXE "${input_zr_benchmark_server_exe}")
endif ()

file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

foreach (valid_request IN ITEMS "WARMUP 1 1" "RUN 42 1048576")
    execute_process(COMMAND "${FIXTURE_EXE}" --parse-request "${valid_request}"
            RESULT_VARIABLE parse_result)
    if (NOT parse_result EQUAL 0)
        message(FATAL_ERROR "fixture rejected canonical request: ${valid_request}")
    endif ()
endforeach ()
foreach (invalid_request IN ITEMS
        "RUN 1" "RUN 1 0" "RUN 01 1" "RUN 1 01" "RUN 1 1048577"
        "RUN 1 1 trailing" "RUN  1 1" "run 1 1")
    execute_process(COMMAND "${FIXTURE_EXE}" --parse-request "${invalid_request}"
            RESULT_VARIABLE parse_result)
    if (parse_result EQUAL 0)
        message(FATAL_ERROR "fixture accepted non-canonical request: ${invalid_request}")
    endif ()
endforeach ()

set(report_path "${TEST_OUTPUT_DIR}/normal.json")
file(REMOVE "${report_path}")

set(common_runner_args
        --name fixture
        --iterations 2
        --warmup 1
        --measurement-scope persistent_runtime
        --prepare-scope runtime_and_case_loaded_before_measurement
        --runtime-reused true
        --compiler-reused false
        --jit-state-reused false
        --persistent
        --checksum-contract fixture-contract-v1
        --expected-checksum 424242
        --ready-timeout-ms 2000
        --request-timeout-ms 2000
        --stop-timeout-ms 2000)

execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --json-out "${report_path}"
                ${common_runner_args}
                -- "${FIXTURE_EXE}" --fixture-behavior normal
        RESULT_VARIABLE runner_result
        OUTPUT_VARIABLE runner_stdout
        ERROR_VARIABLE runner_stderr)

if (NOT runner_result EQUAL 0)
    message(FATAL_ERROR
            "persistent runner invocation failed (${runner_result})\nstdout:\n${runner_stdout}\nstderr:\n${runner_stderr}")
endif ()

if (NOT EXISTS "${report_path}")
    message(FATAL_ERROR "persistent runner did not create ${report_path}")
endif ()

file(READ "${report_path}" report_json)
string(JSON measurement_scope GET "${report_json}" measurement_scope)
string(JSON runtime_reused GET "${report_json}" runtime_reused)
string(JSON session_pid GET "${report_json}" persistent_session pid)
string(JSON same_pid GET "${report_json}" persistent_session same_pid)
string(JSON session_peak GET "${report_json}" persistent_session peak_working_set_bytes)
string(JSON run_count LENGTH "${report_json}" runs)
string(JSON default_sample_count GET "${report_json}" sample_count)
string(JSON default_repetitions GET "${report_json}" repetitions)
string(JSON default_extra_count GET "${report_json}" extra_sample_count)
math(EXPR default_expected_sample_count "2 + ${default_extra_count}")
if (NOT measurement_scope STREQUAL "persistent_runtime" OR NOT runtime_reused OR NOT same_pid OR
    session_pid LESS_EQUAL 0 OR session_peak LESS_EQUAL 0 OR NOT run_count EQUAL 2 OR
    NOT run_count EQUAL default_sample_count OR
    NOT default_sample_count EQUAL default_expected_sample_count OR
    NOT default_repetitions EQUAL 1 OR NOT default_extra_count EQUAL 0)
    message(FATAL_ERROR "persistent report session contract is incomplete: ${report_json}")
endif ()
foreach (run_index RANGE 0 1)
    string(JSON run_schema_version GET "${report_json}" runs ${run_index} schema_version)
    string(JSON run_pid GET "${report_json}" runs ${run_index} pid)
    string(JSON run_peak_type TYPE "${report_json}" runs ${run_index} peak_working_set_bytes)
    if (NOT run_schema_version EQUAL 3 OR NOT run_pid EQUAL session_pid OR
        NOT run_peak_type STREQUAL "NULL")
        message(FATAL_ERROR "run ${run_index} did not retain same-PID evidence and session-only RSS")
    endif ()
endforeach ()

set(timeout_report "${TEST_OUTPUT_DIR}/process-timeout.json")
set(descendant_pid_file "${TEST_OUTPUT_DIR}/process-timeout-descendant.pid")
if (WIN32)
    set(process_timeout_test_ms 5000)
else ()
    set(process_timeout_test_ms 100)
endif ()
file(REMOVE "${timeout_report}" "${descendant_pid_file}")
execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --name process-timeout --iterations 1 --warmup 0
                --json-out "${timeout_report}" --process-timeout-ms ${process_timeout_test_ms}
                --measurement-scope process_end_to_end --prepare-scope process_start
                --runtime-reused false --compiler-reused false --jit-state-reused false
                -- "${FIXTURE_EXE}" --fixture-process-hang
                --descendant-pid-file "${descendant_pid_file}"
        RESULT_VARIABLE timeout_result
        OUTPUT_VARIABLE timeout_stdout
        ERROR_VARIABLE timeout_stderr)
if (timeout_result EQUAL 0 OR NOT "${timeout_stdout}${timeout_stderr}" MATCHES "process timeout")
    message(FATAL_ERROR
            "hanging process did not fail with a timeout diagnostic:\n${timeout_stdout}${timeout_stderr}")
endif ()
if (EXISTS "${timeout_report}")
    message(FATAL_ERROR "timed out process left a misleading successful report")
endif ()
if (NOT EXISTS "${descendant_pid_file}")
    message(FATAL_ERROR "hanging process did not record its descendant PID")
endif ()
file(READ "${descendant_pid_file}" descendant_pid)
string(STRIP "${descendant_pid}" descendant_pid)
execute_process(
        COMMAND "${FIXTURE_EXE}" --check-process-dead "${descendant_pid}"
        RESULT_VARIABLE descendant_check_result)
if (NOT descendant_check_result EQUAL 0)
    message(FATAL_ERROR "process timeout leaked descendant PID ${descendant_pid}")
endif ()

execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --json-out "${TEST_OUTPUT_DIR}/persistent-process-timeout.json"
                ${common_runner_args} --process-timeout-ms ${process_timeout_test_ms}
                -- "${FIXTURE_EXE}" --fixture-behavior normal
        RESULT_VARIABLE persistent_timeout_option_result
        OUTPUT_VARIABLE persistent_timeout_option_stdout
        ERROR_VARIABLE persistent_timeout_option_stderr)
if (persistent_timeout_option_result EQUAL 0 OR
    NOT "${persistent_timeout_option_stdout}${persistent_timeout_option_stderr}" MATCHES
        "process-only option")
    message(FATAL_ERROR "persistent mode accepted --process-timeout-ms")
endif ()

function(expect_calibrated_path mode)
    set(calibrated_report "${TEST_OUTPUT_DIR}/calibrated-${mode}.json")
    file(REMOVE "${calibrated_report}")
    if (mode STREQUAL "persistent")
        set(mode_args
                --measurement-scope persistent_runtime
                --prepare-scope runtime_and_case_loaded_before_measurement
                --runtime-reused true --compiler-reused false --jit-state-reused false
                --persistent --checksum-contract fixture-contract-v1 --expected-checksum 424242
                --ready-timeout-ms 2000 --request-timeout-ms 5000 --stop-timeout-ms 2000)
                set(fixture_args "${FIXTURE_EXE}" --fixture-behavior normal --fixture-sleep-ms 500)
    else ()
        set(mode_args
                --measurement-scope process_end_to_end --prepare-scope process_start
                --runtime-reused false --compiler-reused false --jit-state-reused false)
        set(fixture_args "${FIXTURE_EXE}" --fixture-process-ms 15)
    endif ()
    execute_process(
            COMMAND "${PERF_RUNNER_EXE}"
                    --name "calibrated-${mode}" --iterations 2 --warmup 0
                    --json-out "${calibrated_report}" --min-sample-ms 2000
                    ${mode_args} -- ${fixture_args}
            RESULT_VARIABLE calibrated_result
            OUTPUT_VARIABLE calibrated_stdout
            ERROR_VARIABLE calibrated_stderr)
    if (NOT calibrated_result EQUAL 0)
        message(FATAL_ERROR
                "${mode} calibration failed (${calibrated_result}):\n${calibrated_stdout}${calibrated_stderr}")
    endif ()
    file(READ "${calibrated_report}" calibrated_json)
    string(JSON repetitions GET "${calibrated_json}" repetitions)
    string(JSON calibrated_aggregate GET "${calibrated_json}" calibration aggregate_wall_ms)
    string(JSON calibration_enabled GET "${calibrated_json}" calibration enabled)
    string(JSON calibrated_stability GET "${calibrated_json}" stability)
    string(JSON calibrated_gate GET "${calibrated_json}" gate_eligible)
    if (NOT calibration_enabled OR repetitions LESS 2 OR calibrated_aggregate LESS 2000)
        message(FATAL_ERROR "${mode} did not calibrate by doubling repetitions: ${calibrated_json}")
    endif ()
    math(EXPR repetitions_minus_one "${repetitions} - 1")
    math(EXPR repetition_power_check "${repetitions} & ${repetitions_minus_one}")
    if (NOT repetition_power_check EQUAL 0)
        message(FATAL_ERROR "${mode} calibration repetitions are not a power of two: ${repetitions}")
    endif ()
    string(JSON aggregate_wall GET "${calibrated_json}" runs 0 aggregate_wall_ms)
    string(JSON normalized_wall GET "${calibrated_json}" runs 0 wall_ms)
    if (aggregate_wall LESS 1 OR normalized_wall LESS 1 OR normalized_wall GREATER_EQUAL aggregate_wall)
        message(FATAL_ERROR "${mode} aggregate/normalized wall time is invalid: ${calibrated_json}")
    endif ()
    if (mode STREQUAL "persistent" AND
        (NOT calibrated_stability STREQUAL "STABLE" OR calibrated_gate))
        message(FATAL_ERROR
                "stable two-sample report must not be gate eligible: ${calibrated_json}")
    endif ()
endfunction()

expect_calibrated_path(process)
expect_calibrated_path(persistent)

set(stable_ten_report "${TEST_OUTPUT_DIR}/stable-ten.json")
execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --name stable-ten --iterations 10 --warmup 0
                --json-out "${stable_ten_report}"
                --measurement-scope persistent_runtime
                --prepare-scope runtime_and_case_loaded_before_measurement
                --runtime-reused true --compiler-reused false --jit-state-reused false
                --persistent --checksum-contract fixture-contract-v1 --expected-checksum 424242
                --ready-timeout-ms 2000 --request-timeout-ms 2000 --stop-timeout-ms 2000
                -- "${FIXTURE_EXE}" --fixture-behavior normal --fixture-sleep-ms 500
        RESULT_VARIABLE stable_ten_result
        OUTPUT_VARIABLE stable_ten_stdout
        ERROR_VARIABLE stable_ten_stderr)
if (NOT stable_ten_result EQUAL 0)
    message(FATAL_ERROR "stable ten-sample fixture failed:\n${stable_ten_stdout}${stable_ten_stderr}")
endif ()
file(READ "${stable_ten_report}" stable_ten_json)
string(JSON stable_ten_count GET "${stable_ten_json}" sample_count)
string(JSON stable_ten_status GET "${stable_ten_json}" stability)
string(JSON stable_ten_gate GET "${stable_ten_json}" gate_eligible)
if (NOT stable_ten_count EQUAL 10 OR NOT stable_ten_status STREQUAL "STABLE" OR NOT stable_ten_gate)
    message(FATAL_ERROR "stable ten-sample report must be gate eligible: ${stable_ten_json}")
endif ()
foreach (run_index RANGE 0 9)
    string(JSON stable_ten_schema_version GET "${stable_ten_json}" runs ${run_index} schema_version)
    if (NOT stable_ten_schema_version EQUAL 3)
        message(FATAL_ERROR "stable ten-sample run ${run_index} has the wrong schema version")
    endif ()
endforeach ()

set(unstable_report "${TEST_OUTPUT_DIR}/unstable.json")
execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --name unstable --iterations 10 --warmup 0 --max-extra-samples 10
                --json-out "${unstable_report}"
                --measurement-scope persistent_runtime
                --prepare-scope runtime_and_case_loaded_before_measurement
                --runtime-reused true --compiler-reused false --jit-state-reused false
                --persistent --checksum-contract fixture-contract-v1 --expected-checksum 424242
                --ready-timeout-ms 2000 --request-timeout-ms 2000 --stop-timeout-ms 2000
                -- "${FIXTURE_EXE}" --fixture-behavior unstable_timing
        RESULT_VARIABLE unstable_result
        OUTPUT_VARIABLE unstable_stdout
        ERROR_VARIABLE unstable_stderr)
if (NOT unstable_result EQUAL 0)
    message(FATAL_ERROR "unstable fixture failed:\n${unstable_stdout}${unstable_stderr}")
endif ()
file(READ "${unstable_report}" unstable_json)
string(JSON unstable_sample_count GET "${unstable_json}" sample_count)
string(JSON unstable_extra_count GET "${unstable_json}" extra_sample_count)
string(JSON unstable_run_count LENGTH "${unstable_json}" runs)
string(JSON unstable_status GET "${unstable_json}" stability)
string(JSON unstable_gate GET "${unstable_json}" gate_eligible)
math(EXPR unstable_expected_sample_count "10 + ${unstable_extra_count}")
if (NOT unstable_sample_count EQUAL 20 OR NOT unstable_extra_count EQUAL 10 OR
    NOT unstable_run_count EQUAL unstable_sample_count OR
    NOT unstable_sample_count EQUAL unstable_expected_sample_count OR
    NOT unstable_status STREQUAL "UNSTABLE" OR unstable_gate)
    message(FATAL_ERROR "unstable fixture did not exhaust ten extra samples: ${unstable_json}")
endif ()
foreach (run_index RANGE 0 19)
    string(JSON unstable_schema_version GET "${unstable_json}" runs ${run_index} schema_version)
    if (NOT unstable_schema_version EQUAL 3)
        message(FATAL_ERROR "unstable run ${run_index} has the wrong schema version")
    endif ()
endforeach ()

set(profile_report "${TEST_OUTPUT_DIR}/profile.json")
execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --name profile --iterations 1 --warmup 0 --profile
                --json-out "${profile_report}"
                --measurement-scope process_end_to_end --prepare-scope process_start
                --runtime-reused false --compiler-reused false --jit-state-reused false
                -- "${FIXTURE_EXE}" --fixture-process-ms 1
        RESULT_VARIABLE profile_result
        OUTPUT_VARIABLE profile_stdout
        ERROR_VARIABLE profile_stderr)
if (NOT profile_result EQUAL 0)
    message(FATAL_ERROR "profile fixture failed:\n${profile_stdout}${profile_stderr}")
endif ()
file(READ "${profile_report}" profile_json)
string(JSON profile_count GET "${profile_json}" sample_count)
string(JSON profile_comparable GET "${profile_json}" comparable)
string(JSON profile_gate GET "${profile_json}" gate_eligible)
string(JSON profile_stability GET "${profile_json}" stability)
if (NOT profile_count EQUAL 1 OR profile_comparable OR profile_gate OR
    NOT profile_stability STREQUAL "NOT_COMPARABLE")
    message(FATAL_ERROR "profile report was not explicitly non-comparable: ${profile_json}")
endif ()

function(expect_new_option_rejection label expected_pattern)
    execute_process(
            COMMAND "${PERF_RUNNER_EXE}"
                    --name invalid --iterations 2 --warmup 0
                    --json-out "${TEST_OUTPUT_DIR}/invalid-${label}.json"
                    --measurement-scope process_end_to_end --prepare-scope process_start
                    --runtime-reused false --compiler-reused false --jit-state-reused false
                    ${ARGN} -- "${FIXTURE_EXE}" --fixture-process-ms 1
            RESULT_VARIABLE invalid_result
            OUTPUT_VARIABLE invalid_stdout
            ERROR_VARIABLE invalid_stderr)
    if (invalid_result EQUAL 0 OR
        NOT "${invalid_stdout}${invalid_stderr}" MATCHES "${expected_pattern}")
        message(FATAL_ERROR "invalid ${label} was not rejected strictly")
    endif ()
endfunction()

expect_new_option_rejection(min-sample "Invalid --min-sample-ms" --min-sample-ms 0)
expect_new_option_rejection(extra-samples "Invalid --max-extra-samples" --max-extra-samples 11)
expect_new_option_rejection(bootstrap-seed "Invalid --bootstrap-seed" --bootstrap-seed -1)
expect_new_option_rejection(profile-count "Profile mode requires" --profile)
expect_new_option_rejection(total-samples "must not exceed 20" --iterations 20 --max-extra-samples 1)

function(expect_persistent_failure behavior expected_pattern)
    set(failure_report "${TEST_OUTPUT_DIR}/${behavior}.json")
    file(REMOVE "${failure_report}")
    execute_process(
            COMMAND "${PERF_RUNNER_EXE}"
                    --json-out "${failure_report}"
                    ${common_runner_args}
                    --ready-timeout-ms 1000
                    --request-timeout-ms 1000
                    --stop-timeout-ms 1000
                    ${ARGN}
                    -- "${FIXTURE_EXE}" --fixture-behavior "${behavior}"
            RESULT_VARIABLE failure_result
            OUTPUT_VARIABLE failure_stdout
            ERROR_VARIABLE failure_stderr)
    if (failure_result EQUAL 0)
        message(FATAL_ERROR "${behavior} unexpectedly succeeded")
    endif ()
    set(failure_output "${failure_stdout}${failure_stderr}")
    if (NOT failure_output MATCHES "${expected_pattern}")
        message(FATAL_ERROR
                "${behavior} failed without expected diagnostic /${expected_pattern}/:\n${failure_output}")
    endif ()
    if (EXISTS "${failure_report}")
        message(FATAL_ERROR "${behavior} left a misleading successful report")
    endif ()
endfunction()

expect_persistent_failure(ready_timeout "response timeout")
expect_persistent_failure(ready_contract_mismatch "READY contract mismatch")
expect_persistent_failure(malformed_ready "READY contract mismatch")
expect_persistent_failure(done_checksum_mismatch "checksum mismatch")
expect_persistent_failure(wrong_index "index mismatch")
expect_persistent_failure(error_response "child ERROR")
expect_persistent_failure(malformed_response "malformed persistent response")
expect_persistent_failure(leading_space_response "malformed persistent response")
expect_persistent_failure(tab_response "not strict ASCII")
expect_persistent_failure(double_space_response "malformed persistent response")
expect_persistent_failure(plus_index_response "malformed persistent response index")
expect_persistent_failure(leading_zero_index_response "malformed persistent response index")
expect_persistent_failure(overlong_response "line is overlong")
expect_persistent_failure(early_exit_before_ready "exited before response")
expect_persistent_failure(early_exit_after_ready "exited before response")
expect_persistent_failure(request_timeout "response timeout")
expect_persistent_failure(stop_timeout "STOP timeout")
expect_persistent_failure(stop_nonzero "nonzero after STOP")
expect_persistent_failure(repetition_checksum_mismatch "child ERROR" --min-sample-ms 100)

execute_process(
        COMMAND "${PERF_RUNNER_EXE}"
                --name invalid
                --iterations 1
                --warmup 0
                --json-out "${TEST_OUTPUT_DIR}/missing-scope.json"
                --prepare-scope runtime_and_case_loaded_before_measurement
                --runtime-reused true
                --compiler-reused false
                --jit-state-reused false
                --persistent
                --checksum-contract fixture-contract-v1
                --expected-checksum 424242
                --ready-timeout-ms 100
                --request-timeout-ms 100
                --stop-timeout-ms 100
                -- "${FIXTURE_EXE}" --fixture-behavior normal
        RESULT_VARIABLE missing_scope_result
        OUTPUT_VARIABLE missing_scope_stdout
        ERROR_VARIABLE missing_scope_stderr)
if (missing_scope_result EQUAL 0 OR
    NOT "${missing_scope_stdout}${missing_scope_stderr}" MATCHES "Persistent mode requires")
    message(FATAL_ERROR "missing persistent scope was not rejected safely")
endif ()

function(expect_process_label_rejection label)
    execute_process(
            COMMAND "${PERF_RUNNER_EXE}"
                    --name invalid
                    --iterations 1
                    --warmup 0
                    --json-out "${TEST_OUTPUT_DIR}/${label}.json"
                    ${ARGN}
                    --compiler-reused false
                    --jit-state-reused false
                    -- "${CMAKE_COMMAND}" -E true
            RESULT_VARIABLE invalid_result
            OUTPUT_VARIABLE invalid_stdout
            ERROR_VARIABLE invalid_stderr)
    if (invalid_result EQUAL 0 OR
        NOT "${invalid_stdout}${invalid_stderr}" MATCHES "Process mode cannot claim persistent")
        message(FATAL_ERROR "${label} was not rejected as a false process-mode label")
    endif ()
endfunction()

expect_process_label_rejection(process-persistent-scope
        --measurement-scope persistent_runtime
        --prepare-scope none
        --runtime-reused false)
expect_process_label_rejection(process-runtime-reuse
        --measurement-scope process_end_to_end
        --prepare-scope none
        --runtime-reused true)
expect_process_label_rejection(process-persistent-options
        --measurement-scope process_end_to_end
        --prepare-scope none
        --runtime-reused false
        --checksum-contract fixture-contract-v1)

if (DEFINED BENCHMARKS_DIR AND IS_DIRECTORY "${BENCHMARKS_DIR}")
    find_program(protocol_lua_exe NAMES lua lua54 lua5.4)
    find_program(protocol_qjs_exe NAMES qjs quickjs)

    function(expect_language_server language expected_checksum)
        set(language_report "${TEST_OUTPUT_DIR}/${language}.json")
        set(language_jit_reused false)
        if (language STREQUAL "dotnet")
            set(language_jit_reused true)
        endif ()
        file(REMOVE "${language_report}")
        execute_process(
                COMMAND "${PERF_RUNNER_EXE}"
                        --name "${language}"
                        --iterations 2
                        --warmup 1
                        --json-out "${language_report}"
                        --measurement-scope persistent_runtime
                        --prepare-scope runtime_and_case_loaded_before_measurement
                        --runtime-reused true
                        --compiler-reused false
                        --jit-state-reused ${language_jit_reused}
                        --persistent
                        --checksum-contract benchmark-checksum-v1:numeric_loops:smoke
                        --expected-checksum "${expected_checksum}"
                        --ready-timeout-ms 2000
                        --request-timeout-ms 2000
                        --stop-timeout-ms 2000
                        -- ${ARGN}
                RESULT_VARIABLE language_result
                OUTPUT_VARIABLE language_stdout
                ERROR_VARIABLE language_stderr)
        if (NOT language_result EQUAL 0)
            message(FATAL_ERROR
                    "${language} persistent server failed:\n${language_stdout}${language_stderr}")
        endif ()
    endfunction()

    function(expect_language_index_rejection language executable)
        set(invalid_index_input "${TEST_OUTPUT_DIR}/${language}-invalid-index.txt")
        file(WRITE "${invalid_index_input}" "RUN 2147483648 1\nSTOP\n")
        execute_process(
                COMMAND "${executable}" ${ARGN}
                INPUT_FILE "${invalid_index_input}"
                RESULT_VARIABLE invalid_index_result
                OUTPUT_VARIABLE invalid_index_stdout
                ERROR_VARIABLE invalid_index_stderr)
        if (invalid_index_result EQUAL 0 OR
            NOT "${invalid_index_stdout}${invalid_index_stderr}" MATCHES "ERROR 0 malformed-request")
            message(FATAL_ERROR
                    "${language} accepted a request index outside signed-int range:\n"
                    "${invalid_index_stdout}${invalid_index_stderr}")
        endif ()
    endfunction()

    if (protocol_lua_exe)
        expect_language_server(lua 48943705
                "${protocol_lua_exe}"
                "${BENCHMARKS_DIR}/cases/numeric_loops/lua/main.lua"
                --benchmark-server --case numeric_loops --tier smoke)
        expect_language_index_rejection(lua "${protocol_lua_exe}"
                "${BENCHMARKS_DIR}/cases/numeric_loops/lua/main.lua"
                --benchmark-server --case numeric_loops --tier smoke)
    endif ()
    if (protocol_qjs_exe)
        expect_language_server(qjs 48943705
                "${protocol_qjs_exe}" -m
                "${BENCHMARKS_DIR}/cases/numeric_loops/qjs/main.js"
                --benchmark-server --case numeric_loops --tier smoke)
        expect_language_index_rejection(qjs "${protocol_qjs_exe}" -m
                "${BENCHMARKS_DIR}/cases/numeric_loops/qjs/main.js"
                --benchmark-server --case numeric_loops --tier smoke)
    endif ()

    if (DEFINED CLI_EXE AND EXISTS "${CLI_EXE}" AND
        DEFINED ZR_BENCHMARK_SERVER_EXE AND EXISTS "${ZR_BENCHMARK_SERVER_EXE}")
        set(zr_runtime_dir "${TEST_OUTPUT_DIR}/zr")
        set(zr_project_file "${zr_runtime_dir}/benchmark_numeric_loops.zrp")
        file(REMOVE_RECURSE "${zr_runtime_dir}")
        file(MAKE_DIRECTORY "${zr_runtime_dir}")
        file(COPY "${BENCHMARKS_DIR}/cases/numeric_loops/zr/src" DESTINATION "${zr_runtime_dir}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy
                "${BENCHMARKS_DIR}/cases/numeric_loops/zr/benchmark_numeric_loops.zrp"
                "${zr_project_file}"
                RESULT_VARIABLE zr_copy_result)
        if (NOT zr_copy_result EQUAL 0)
            message(FATAL_ERROR "failed to prepare ZR persistent runtime project")
        endif ()
        file(WRITE "${zr_runtime_dir}/src/bench_config.zr"
                "pub fn scale(): int {\n    return 1;\n}\n")
        execute_process(COMMAND "${CLI_EXE}" --compile "${zr_project_file}"
                RESULT_VARIABLE zr_compile_result
                OUTPUT_VARIABLE zr_compile_stdout
                ERROR_VARIABLE zr_compile_stderr)
        if (NOT zr_compile_result EQUAL 0)
            message(FATAL_ERROR "ZR benchmark project compilation failed:\n${zr_compile_stdout}${zr_compile_stderr}")
        endif ()
        expect_language_server(zr 48943705
                "${ZR_BENCHMARK_SERVER_EXE}" --benchmark-server
                --project "${zr_project_file}" --case numeric_loops --tier smoke)
    else ()
        message(STATUS "SKIP: ZR persistent runtime coverage unavailable (CLI/server target not supplied)")
    endif ()

    find_program(protocol_dotnet_exe NAMES dotnet)
    if (protocol_dotnet_exe AND DEFINED DOTNET_PROJECT AND EXISTS "${DOTNET_PROJECT}")
        set(dotnet_output_dir "${TEST_OUTPUT_DIR}/dotnet")
        set(dotnet_intermediate_dir "${TEST_OUTPUT_DIR}/dotnet-obj")
        file(REMOVE_RECURSE "${dotnet_output_dir}" "${dotnet_intermediate_dir}")
        execute_process(
                COMMAND "${protocol_dotnet_exe}" build "${DOTNET_PROJECT}" --configuration Release --nologo
                        --output "${dotnet_output_dir}"
                        "-p:BaseIntermediateOutputPath=${dotnet_intermediate_dir}/"
                        "-p:MSBuildProjectExtensionsPath=${dotnet_intermediate_dir}/"
                RESULT_VARIABLE dotnet_build_result
                OUTPUT_VARIABLE dotnet_build_stdout
                ERROR_VARIABLE dotnet_build_stderr)
        if (NOT dotnet_build_result EQUAL 0)
            message(FATAL_ERROR ".NET benchmark runner build failed:\n${dotnet_build_stdout}${dotnet_build_stderr}")
        endif ()
        expect_language_server(dotnet 48943705
                "${protocol_dotnet_exe}" "${dotnet_output_dir}/BenchmarkRunner.dll"
                --benchmark-server --case numeric_loops --tier smoke)
    else ()
        message(STATUS "SKIP: .NET persistent runtime coverage unavailable (dotnet/project not supplied)")
    endif ()
endif ()

message(STATUS "Persistent performance protocol PASS")
