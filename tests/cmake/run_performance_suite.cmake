if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif ()

if (NOT DEFINED PERF_RUNNER_EXE OR PERF_RUNNER_EXE STREQUAL "")
    message(FATAL_ERROR "PERF_RUNNER_EXE is required.")
endif ()

if (NOT DEFINED NATIVE_BENCHMARK_EXE OR NATIVE_BENCHMARK_EXE STREQUAL "")
    message(FATAL_ERROR "NATIVE_BENCHMARK_EXE is required.")
endif ()

if (NOT DEFINED BENCHMARKS_DIR OR BENCHMARKS_DIR STREQUAL "")
    message(FATAL_ERROR "BENCHMARKS_DIR is required.")
endif ()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif ()

file(TO_CMAKE_PATH "${CLI_EXE}" CLI_EXE)
file(TO_CMAKE_PATH "${PERF_RUNNER_EXE}" PERF_RUNNER_EXE)
file(TO_CMAKE_PATH "${NATIVE_BENCHMARK_EXE}" NATIVE_BENCHMARK_EXE)
file(TO_CMAKE_PATH "${BENCHMARKS_DIR}" BENCHMARKS_DIR)
file(TO_CMAKE_PATH "${GENERATED_DIR}" GENERATED_DIR)

include("${BENCHMARKS_DIR}/registry.cmake")

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

if (PERF_REQUESTED_TIER STREQUAL "stress")
    set(PERF_DEFAULT_WARMUP 1)
    set(PERF_DEFAULT_ITERATIONS 2)
else ()
    set(PERF_DEFAULT_WARMUP 1)
    set(PERF_DEFAULT_ITERATIONS 1)
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

set(PERF_SCALE "${ZR_VM_BENCHMARK_TIER_SCALE_${PERF_REQUESTED_TIER}}")
if (NOT PERF_SCALE)
    message(FATAL_ERROR "Missing scale for tier ${PERF_REQUESTED_TIER}")
endif ()

set(PERF_SUITE_ROOT "${GENERATED_DIR}/performance_suite")
set(PERF_REPORT_DIR "${GENERATED_DIR}/performance")
set(PERF_TOOLCHAIN_DIR "${PERF_SUITE_ROOT}/toolchains")
if (WIN32)
    set(PERF_HOST_EXE_SUFFIX ".exe")
else ()
    set(PERF_HOST_EXE_SUFFIX "")
endif ()
file(REMOVE_RECURSE "${PERF_SUITE_ROOT}")
file(MAKE_DIRECTORY "${PERF_SUITE_ROOT}")
file(MAKE_DIRECTORY "${PERF_REPORT_DIR}")
file(MAKE_DIRECTORY "${PERF_TOOLCHAIN_DIR}")

function(perf_normalize_output input_text out_var)
    string(REPLACE "\r\n" "\n" normalized "${input_text}")
    string(REPLACE "\r" "\n" normalized "${normalized}")
    string(STRIP "${normalized}" normalized)
    set(${out_var} "${normalized}" PARENT_SCOPE)
endfunction()

function(perf_strip_contract_noise input_text out_var)
    set(filtered "${input_text}")
    string(REGEX REPLACE "(^|\n)\\[module-init\\][^\n]*" "" filtered "${filtered}")
    string(REPLACE "\n\n" "\n" filtered "${filtered}")
    string(STRIP "${filtered}" filtered)
    set(${out_var} "${filtered}" PARENT_SCOPE)
endfunction()

function(perf_escape_json_string input_text out_var)
    set(escaped "${input_text}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    string(REPLACE "\t" "\\t" escaped "${escaped}")
    set(${out_var} "${escaped}" PARENT_SCOPE)
endfunction()

function(perf_json_array_from_list out_var)
    set(result "[")
    set(needs_comma FALSE)
    foreach (item IN LISTS ARGN)
        perf_escape_json_string("${item}" escaped_item)
        if (needs_comma)
            string(APPEND result ", ")
        endif ()
        string(APPEND result "\"${escaped_item}\"")
        set(needs_comma TRUE)
    endforeach ()
    string(APPEND result "]")
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(perf_extract_metric text key out_var)
    string(REGEX MATCH "${key}=([0-9]+(\\.[0-9]+)?)" perf_match "${text}")
    if (perf_match STREQUAL "")
        message(FATAL_ERROR "Failed to parse metric '${key}' from performance output:\n${text}")
    endif ()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(perf_decimal_to_milli value out_var)
    string(REGEX MATCH "^([0-9]+)\\.([0-9][0-9][0-9])$" matched "${value}")
    if (matched STREQUAL "")
        message(FATAL_ERROR "Expected decimal with three fractional digits, got: ${value}")
    endif ()
    math(EXPR milli "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
    set(${out_var} "${milli}" PARENT_SCOPE)
endfunction()

function(perf_format_milli_decimal milli_value out_var)
    math(EXPR whole "${milli_value} / 1000")
    math(EXPR frac "${milli_value} % 1000")
    if (frac LESS 10)
        set(frac_text "00${frac}")
    elseif (frac LESS 100)
        set(frac_text "0${frac}")
    else ()
        set(frac_text "${frac}")
    endif ()
    set(${out_var} "${whole}.${frac_text}" PARENT_SCOPE)
endfunction()

function(perf_relative_to_c value base out_var)
    if (value STREQUAL "" OR base STREQUAL "")
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

    perf_decimal_to_milli("${value}" value_milli)
    perf_decimal_to_milli("${base}" base_milli)
    if (base_milli LESS 1)
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

    math(EXPR ratio_milli "((${value_milli} * 1000) + (${base_milli} / 2)) / ${base_milli}")
    perf_format_milli_decimal("${ratio_milli}" ratio_text)
    set(${out_var} "${ratio_text}" PARENT_SCOPE)
endfunction()

function(perf_case_matches_tier case_name out_var)
    set(case_tiers "${ZR_VM_BENCHMARK_TIERS_${case_name}}")
    list(FIND case_tiers "${PERF_REQUESTED_TIER}" case_tier_index)
    if (case_tier_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif ()
endfunction()

function(perf_prepare_zr_case case_name out_project_dir_var out_project_file_var)
    set(source_dir "${BENCHMARKS_DIR}/cases/${case_name}/zr")
    set(destination_dir "${PERF_SUITE_ROOT}/cases/${case_name}/zr")
    set(project_file "${destination_dir}/benchmark_${case_name}.zrp")

    file(REMOVE_RECURSE "${destination_dir}")
    file(MAKE_DIRECTORY "${destination_dir}")
    file(COPY "${source_dir}/src" DESTINATION "${destination_dir}")
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/benchmark_${case_name}.zrp" "${project_file}"
            RESULT_VARIABLE copy_result
            OUTPUT_VARIABLE copy_stdout
            ERROR_VARIABLE copy_stderr)
    if (NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy benchmark project for ${case_name}.\n${copy_stdout}${copy_stderr}")
    endif ()

    file(WRITE
            "${destination_dir}/src/bench_config.zr"
            "pub scale(): int {\n"
            "    return ${PERF_SCALE};\n"
            "}\n")

    set(${out_project_dir_var} "${destination_dir}" PARENT_SCOPE)
    set(${out_project_file_var} "${project_file}" PARENT_SCOPE)
endfunction()

function(perf_append_note kind case_name implementation_name note)
    set(entry "- `${case_name}` / `${implementation_name}`: ${note}")
    if (kind STREQUAL "failure")
        set(PERF_FAILURE_NOTES "${PERF_FAILURE_NOTES}\n${entry}" PARENT_SCOPE)
    else ()
        set(PERF_SKIP_NOTES "${PERF_SKIP_NOTES}\n${entry}" PARENT_SCOPE)
    endif ()
endfunction()

find_program(PERF_PYTHON_EXE NAMES python python3)
find_program(PERF_NODE_EXE NAMES node)
find_program(PERF_CARGO_EXE NAMES cargo)
find_program(PERF_DOTNET_EXE NAMES dotnet)
find_program(PERF_LLVM_HOST_TOOL NAMES clang-cl clang)

set(PERF_RUST_RUNNER_EXE "")
if (PERF_CARGO_EXE)
    set(PERF_RUST_TARGET_DIR "${PERF_TOOLCHAIN_DIR}/rust")
    execute_process(
            COMMAND "${PERF_CARGO_EXE}" build --manifest-path "${BENCHMARKS_DIR}/rust_runner/Cargo.toml" --release --target-dir "${PERF_RUST_TARGET_DIR}"
            RESULT_VARIABLE rust_build_result
            OUTPUT_VARIABLE rust_build_stdout
            ERROR_VARIABLE rust_build_stderr)
    if (NOT rust_build_result EQUAL 0)
        message(FATAL_ERROR "Failed to build Rust benchmark runner.\n${rust_build_stdout}${rust_build_stderr}")
    endif ()
    set(PERF_RUST_RUNNER_EXE "${PERF_RUST_TARGET_DIR}/release/zr_vm_benchmark_runner${PERF_HOST_EXE_SUFFIX}")
endif ()

set(PERF_DOTNET_RUNNER_DLL "")
if (PERF_DOTNET_EXE)
    set(PERF_DOTNET_OUTPUT_DIR "${PERF_TOOLCHAIN_DIR}/dotnet")
    execute_process(
            COMMAND "${PERF_DOTNET_EXE}" build "${BENCHMARKS_DIR}/dotnet_runner/BenchmarkRunner.csproj" -c Release -o "${PERF_DOTNET_OUTPUT_DIR}"
            RESULT_VARIABLE dotnet_build_result
            OUTPUT_VARIABLE dotnet_build_stdout
            ERROR_VARIABLE dotnet_build_stderr)
    if (NOT dotnet_build_result EQUAL 0)
        message(FATAL_ERROR "Failed to build .NET benchmark runner.\n${dotnet_build_stdout}${dotnet_build_stderr}")
    endif ()
    set(PERF_DOTNET_RUNNER_DLL "${PERF_DOTNET_OUTPUT_DIR}/BenchmarkRunner.dll")
endif ()

message("==========")
message("Running suite: performance_report")
message("Tier: ${PERF_REQUESTED_TIER}")
message("Scale: ${PERF_SCALE}")
message("Warmup iterations: ${PERF_WARMUP}")
message("Measured iterations: ${PERF_ITERATIONS}")
message("Benchmarks root: ${BENCHMARKS_DIR}")
message("==========")

set(PERF_CASE_COUNT 0)
set(PERF_MARKDOWN_ROWS "")
set(PERF_JSON_CASES "")
set(PERF_SKIP_NOTES "")
set(PERF_FAILURE_NOTES "")
set(PERF_HARD_FAILURE FALSE)

foreach (case_name IN LISTS ZR_VM_BENCHMARK_CASE_NAMES)
    perf_case_matches_tier("${case_name}" case_enabled)
    if (NOT case_enabled)
        continue()
    endif ()

    math(EXPR PERF_CASE_COUNT "${PERF_CASE_COUNT} + 1")
    perf_prepare_zr_case("${case_name}" zr_project_dir zr_project_file)

    set(case_description "${ZR_VM_BENCHMARK_DESCRIPTION_${case_name}}")
    set(case_banner "${ZR_VM_BENCHMARK_PASS_BANNER_${case_name}}")
    set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_${PERF_REQUESTED_TIER}}")
    set(case_expected_output "${case_banner}\n${case_checksum}")
    set(case_impl_jsons "")
    set(case_c_baseline_mean "")

    foreach (implementation_id IN LISTS ZR_VM_BENCHMARK_IMPLEMENTATIONS_${case_name})
        set(implementation_name "")
        set(language "")
        set(mode "")
        set(status "SKIP")
        set(note "")
        set(command_list "")
        set(working_directory "${PERF_SUITE_ROOT}")
        set(correctness_output "")
        set(prepare_command "")
        set(should_measure FALSE)
        set(json_object "")
        set(relative_to_c "null")
        set(mean_wall_ms "")
        set(median_wall_ms "")
        set(min_wall_ms "")
        set(max_wall_ms "")
        set(stddev_wall_ms "")
        set(mean_peak_mib "")
        set(max_peak_mib "")

        if (implementation_id STREQUAL "c")
            set(implementation_name "C")
            set(language "C")
            set(mode "native")
            set(command_list "${NATIVE_BENCHMARK_EXE};--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
            set(working_directory "${BENCHMARKS_DIR}")
            set(should_measure TRUE)
        elseif (implementation_id STREQUAL "zr_interp")
            set(implementation_name "ZR interp")
            set(language "ZR")
            set(mode "interp")
            set(command_list "${CLI_EXE};${zr_project_file}")
            set(working_directory "${zr_project_dir}")
            set(should_measure TRUE)
        elseif (implementation_id STREQUAL "zr_binary")
            set(implementation_name "ZR binary")
            set(language "ZR")
            set(mode "binary")
            set(prepare_command "${CLI_EXE};--compile;${zr_project_file}")
            set(command_list "${CLI_EXE};${zr_project_file};--execution-mode;binary")
            set(working_directory "${zr_project_dir}")
            set(should_measure TRUE)
        elseif (implementation_id STREQUAL "zr_aot_c")
            set(implementation_name "ZR aot_c")
            set(language "ZR")
            set(mode "aot_c")
            set(prepare_command "${CLI_EXE};--compile;${zr_project_file};--emit-aot-c")
            set(command_list "${CLI_EXE};${zr_project_file};--execution-mode;aot_c;--require-aot-path")
            set(working_directory "${zr_project_dir}")
            set(should_measure TRUE)
        elseif (implementation_id STREQUAL "zr_aot_llvm")
            set(implementation_name "ZR aot_llvm")
            set(language "ZR")
            set(mode "aot_llvm")
            if (PERF_LLVM_HOST_TOOL)
                set(prepare_command "${CLI_EXE};--compile;${zr_project_file};--emit-aot-llvm")
                set(command_list "${CLI_EXE};${zr_project_file};--execution-mode;aot_llvm;--require-aot-path")
                set(working_directory "${zr_project_dir}")
                set(should_measure TRUE)
            else ()
                set(note "LLVM host adapter unavailable: clang/clang-cl not found")
            endif ()
        elseif (implementation_id STREQUAL "python")
            set(implementation_name "Python")
            set(language "Python")
            set(mode "script")
            if (PERF_PYTHON_EXE)
                set(command_list "${PERF_PYTHON_EXE};${BENCHMARKS_DIR}/cases/${case_name}/python/main.py;--tier;${PERF_REQUESTED_TIER}")
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/python")
                set(should_measure TRUE)
            else ()
                set(note "Python executable not found")
            endif ()
        elseif (implementation_id STREQUAL "node")
            set(implementation_name "Node.js")
            set(language "Node.js")
            set(mode "script")
            if (PERF_NODE_EXE)
                set(command_list "${PERF_NODE_EXE};${BENCHMARKS_DIR}/cases/${case_name}/node/main.js;--tier;${PERF_REQUESTED_TIER}")
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/node")
                set(should_measure TRUE)
            else ()
                set(note "Node.js executable not found")
            endif ()
        elseif (implementation_id STREQUAL "rust")
            set(implementation_name "Rust")
            set(language "Rust")
            set(mode "native")
            if (PERF_RUST_RUNNER_EXE)
                set(command_list "${PERF_RUST_RUNNER_EXE};--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
                set(working_directory "${BENCHMARKS_DIR}")
                set(should_measure TRUE)
            else ()
                set(note "Rust toolchain not found")
            endif ()
        elseif (implementation_id STREQUAL "dotnet")
            set(implementation_name "C#/.NET")
            set(language "C#/.NET")
            set(mode "native")
            if (PERF_DOTNET_RUNNER_DLL)
                set(command_list "${PERF_DOTNET_EXE};${PERF_DOTNET_RUNNER_DLL};--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
                set(working_directory "${BENCHMARKS_DIR}")
                set(should_measure TRUE)
            else ()
                set(note ".NET SDK not found")
            endif ()
        else ()
            message(FATAL_ERROR "Unknown implementation id in registry: ${implementation_id}")
        endif ()

        if (should_measure)
            set(status "PENDING")
            if (NOT prepare_command STREQUAL "")
                execute_process(
                        COMMAND ${prepare_command}
                        WORKING_DIRECTORY "${working_directory}"
                        RESULT_VARIABLE prepare_result
                        OUTPUT_VARIABLE prepare_stdout
                        ERROR_VARIABLE prepare_stderr
                        TIMEOUT 600)
                if (NOT prepare_result EQUAL 0)
                    set(prepare_output "${prepare_stdout}${prepare_stderr}")
                    if ((implementation_id STREQUAL "zr_aot_c" OR implementation_id STREQUAL "zr_aot_llvm") AND
                            prepare_output MATCHES "lowering unsupported")
                        set(status "SKIP")
                        set(note "AOT lowering unsupported for this benchmark")
                        perf_append_note("skip" "${case_name}" "${implementation_name}" "${note}")
                    else ()
                        set(status "FAIL")
                        set(note "prepare step failed")
                        perf_append_note("failure"
                                "${case_name}"
                                "${implementation_name}"
                                "prepare step failed.\n${prepare_output}")
                        set(PERF_HARD_FAILURE TRUE)
                    endif ()
                endif ()
            endif ()

            if (NOT status STREQUAL "FAIL" AND NOT status STREQUAL "SKIP")
                execute_process(
                        COMMAND ${command_list}
                        WORKING_DIRECTORY "${working_directory}"
                        RESULT_VARIABLE correctness_result
                        OUTPUT_VARIABLE correctness_stdout
                        ERROR_VARIABLE correctness_stderr
                        TIMEOUT 600)
                perf_normalize_output("${correctness_stdout}${correctness_stderr}" correctness_output)
                perf_strip_contract_noise("${correctness_output}" correctness_output)
                perf_normalize_output("${case_expected_output}" expected_output_normalized)
                if (NOT correctness_result EQUAL 0)
                    if (implementation_id STREQUAL "zr_binary" AND correctness_output MATCHES "failed to load project entry") 
                        set(status "SKIP")
                        set(note "binary entry loader unavailable for this benchmark")
                        perf_append_note("skip" "${case_name}" "${implementation_name}" "${note}")
                    else ()
                        set(status "FAIL")
                        set(note "correctness run failed")
                        perf_append_note("failure"
                                "${case_name}"
                                "${implementation_name}"
                                "correctness run failed with exit code ${correctness_result}.\n${correctness_output}")
                        set(PERF_HARD_FAILURE TRUE)
                    endif ()
                elseif (NOT correctness_output STREQUAL expected_output_normalized)
                    set(status "FAIL")
                    set(note "correctness output mismatch")
                    perf_append_note("failure"
                            "${case_name}"
                            "${implementation_name}"
                            "expected `${expected_output_normalized}` but got `${correctness_output}`")
                    set(PERF_HARD_FAILURE TRUE)
                endif ()
            endif ()

            if (NOT status STREQUAL "FAIL" AND NOT status STREQUAL "SKIP")
                set(perf_json_path "${PERF_REPORT_DIR}/${case_name}__${implementation_id}.json")
                execute_process(
                        COMMAND
                        "${PERF_RUNNER_EXE}"
                        "--name" "${implementation_name}"
                        "--iterations" "${PERF_ITERATIONS}"
                        "--warmup" "${PERF_WARMUP}"
                        "--json-out" "${perf_json_path}"
                        "--working-directory" "${working_directory}"
                        "--"
                        ${command_list}
                        RESULT_VARIABLE perf_runner_result
                        OUTPUT_VARIABLE perf_runner_stdout
                        ERROR_VARIABLE perf_runner_stderr
                        TIMEOUT 1800)
                set(perf_runner_output "${perf_runner_stdout}${perf_runner_stderr}")
                if (NOT perf_runner_result EQUAL 0)
                    set(status "FAIL")
                    set(note "measurement failed")
                    perf_append_note("failure"
                            "${case_name}"
                            "${implementation_name}"
                            "perf runner failed.\n${perf_runner_output}")
                    set(PERF_HARD_FAILURE TRUE)
                else ()
                    set(status "PASS")
                    perf_extract_metric("${perf_runner_output}" "mean_wall_ms" mean_wall_ms)
                    perf_extract_metric("${perf_runner_output}" "median_wall_ms" median_wall_ms)
                    perf_extract_metric("${perf_runner_output}" "min_wall_ms" min_wall_ms)
                    perf_extract_metric("${perf_runner_output}" "max_wall_ms" max_wall_ms)
                    perf_extract_metric("${perf_runner_output}" "stddev_wall_ms" stddev_wall_ms)
                    perf_extract_metric("${perf_runner_output}" "mean_peak_mib" mean_peak_mib)
                    perf_extract_metric("${perf_runner_output}" "max_peak_mib" max_peak_mib)
                    if (implementation_id STREQUAL "c")
                        set(case_c_baseline_mean "${mean_wall_ms}")
                    endif ()
                endif ()
            endif ()
        else ()
            set(status "SKIP")
            if (note STREQUAL "")
                set(note "implementation unavailable")
            endif ()
            perf_append_note("skip" "${case_name}" "${implementation_name}" "${note}")
        endif ()

        if (status STREQUAL "PASS")
            if (implementation_id STREQUAL "c")
                set(relative_to_c "1.000")
            else ()
                perf_relative_to_c("${mean_wall_ms}" "${case_c_baseline_mean}" relative_to_c)
            endif ()
        endif ()

        if (status STREQUAL "PASS")
            set(markdown_mean_wall "${mean_wall_ms}")
            set(markdown_median_wall "${median_wall_ms}")
            set(markdown_min_wall "${min_wall_ms}")
            set(markdown_max_wall "${max_wall_ms}")
            set(markdown_stddev_wall "${stddev_wall_ms}")
            set(markdown_mean_peak "${mean_peak_mib}")
            set(markdown_max_peak "${max_peak_mib}")
            if (relative_to_c STREQUAL "null")
                set(markdown_relative "-")
            else ()
                set(markdown_relative "${relative_to_c}")
            endif ()

            file(READ "${perf_json_path}" perf_json_text)
            string(STRIP "${perf_json_text}" perf_json_text)
            string(REGEX REPLACE "}[ \t\r\n]*$" "" perf_json_text "${perf_json_text}")
            perf_escape_json_string("${language}" json_language)
            perf_escape_json_string("${mode}" json_mode)
            string(APPEND perf_json_text
                    ",\n  \"language\": \"${json_language}\""
                    ",\n  \"mode\": \"${json_mode}\""
                    ",\n  \"status\": \"PASS\""
                    ",\n  \"relative_to_c\": ${relative_to_c}\n}")
            set(json_object "${perf_json_text}")
        else ()
            set(markdown_mean_wall "-")
            set(markdown_median_wall "-")
            set(markdown_min_wall "-")
            set(markdown_max_wall "-")
            set(markdown_stddev_wall "-")
            set(markdown_mean_peak "-")
            set(markdown_max_peak "-")
            set(markdown_relative "-")
            perf_escape_json_string("${implementation_name}" json_impl_name)
            perf_escape_json_string("${language}" json_language)
            perf_escape_json_string("${mode}" json_mode)
            perf_escape_json_string("${working_directory}" json_workdir)
            perf_escape_json_string("${note}" json_note)
            perf_json_array_from_list(json_command ${command_list})
            string(CONCAT json_object
                    "{\n"
                    "  \"name\": \"${json_impl_name}\",\n"
                    "  \"language\": \"${json_language}\",\n"
                    "  \"mode\": \"${json_mode}\",\n"
                    "  \"status\": \"${status}\",\n"
                    "  \"command\": ${json_command},\n"
                    "  \"working_directory\": \"${json_workdir}\",\n"
                    "  \"runs\": [],\n"
                    "  \"summary\": null,\n"
                    "  \"relative_to_c\": null,\n"
                    "  \"note\": \"${json_note}\"\n"
                    "}")
        endif ()

        string(APPEND PERF_MARKDOWN_ROWS
                "| ${case_name} | ${implementation_name} | ${language} | ${status} | ${markdown_mean_wall} | ${markdown_median_wall} | ${markdown_min_wall} | ${markdown_max_wall} | ${markdown_stddev_wall} | ${markdown_mean_peak} | ${markdown_max_peak} | ${markdown_relative} |\n")

        if (case_impl_jsons STREQUAL "")
            set(case_impl_jsons "${json_object}")
        else ()
            set(case_impl_jsons "${case_impl_jsons},\n${json_object}")
        endif ()
    endforeach ()

    perf_escape_json_string("${case_name}" json_case_name)
    perf_escape_json_string("${case_description}" json_case_description)
    perf_escape_json_string("${case_banner}" json_case_banner)
    string(CONCAT case_json
            "    {\n"
            "      \"name\": \"${json_case_name}\",\n"
            "      \"description\": \"${json_case_description}\",\n"
            "      \"pass_banner\": \"${json_case_banner}\",\n"
            "      \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
            "      \"scale\": ${PERF_SCALE},\n"
            "      \"expected_checksum\": ${case_checksum},\n"
            "      \"implementations\": [\n${case_impl_jsons}\n      ]\n"
            "    }")

    if (PERF_JSON_CASES STREQUAL "")
        set(PERF_JSON_CASES "${case_json}")
    else ()
        set(PERF_JSON_CASES "${PERF_JSON_CASES},\n${case_json}")
    endif ()
endforeach ()

if (PERF_CASE_COUNT EQUAL 0)
    message(FATAL_ERROR "performance_report selected zero benchmark cases for tier '${PERF_REQUESTED_TIER}'.")
endif ()

string(TIMESTAMP PERF_GENERATED_AT_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.md" PERF_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.json" PERF_JSON_PATH_NORMALIZED)

string(CONCAT PERF_MARKDOWN_REPORT
        "# ZR VM Performance Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Scale: ${PERF_SCALE}\n"
        "- Warmup Iterations Per Implementation: ${PERF_WARMUP}\n"
        "- Measured Iterations Per Implementation: ${PERF_ITERATIONS}\n"
        "- Benchmarks Root: `${BENCHMARKS_DIR}`\n"
        "- Cases: ${PERF_CASE_COUNT}\n\n"
        "| case | implementation | language | status | mean wall ms | median wall ms | min wall ms | max wall ms | stddev wall ms | mean peak MiB | max peak MiB | relative_to_c |\n"
        "| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n"
        "${PERF_MARKDOWN_ROWS}\n")

if (NOT PERF_SKIP_NOTES STREQUAL "")
    string(APPEND PERF_MARKDOWN_REPORT "\n## Skip Notes\n${PERF_SKIP_NOTES}\n")
endif ()

if (NOT PERF_FAILURE_NOTES STREQUAL "")
    string(APPEND PERF_MARKDOWN_REPORT "\n## Failure Notes\n${PERF_FAILURE_NOTES}\n")
endif ()

string(APPEND PERF_MARKDOWN_REPORT
        "\n## Artifacts\n\n"
        "- Markdown Report: `${PERF_MARKDOWN_PATH_NORMALIZED}`\n"
        "- JSON Report: `${PERF_JSON_PATH_NORMALIZED}`\n")

file(WRITE "${PERF_REPORT_DIR}/benchmark_report.md" "${PERF_MARKDOWN_REPORT}")
file(WRITE
        "${PERF_REPORT_DIR}/benchmark_report.json"
        "{\n"
        "  \"suite\": \"performance_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"scale\": ${PERF_SCALE},\n"
        "  \"warmup\": ${PERF_WARMUP},\n"
        "  \"iterations\": ${PERF_ITERATIONS},\n"
        "  \"cases\": [\n${PERF_JSON_CASES}\n  ]\n"
        "}\n")

message("Performance markdown report: ${PERF_REPORT_DIR}/benchmark_report.md")
message("Performance json report: ${PERF_REPORT_DIR}/benchmark_report.json")

if (PERF_HARD_FAILURE)
    message(FATAL_ERROR "performance_report encountered benchmark failures. See generated report for details.")
endif ()
