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

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

file(TO_CMAKE_PATH "${CLI_EXE}" CLI_EXE)
file(TO_CMAKE_PATH "${PERF_RUNNER_EXE}" PERF_RUNNER_EXE)
file(TO_CMAKE_PATH "${NATIVE_BENCHMARK_EXE}" NATIVE_BENCHMARK_EXE)
file(TO_CMAKE_PATH "${BENCHMARKS_DIR}" BENCHMARKS_DIR)
file(TO_CMAKE_PATH "${GENERATED_DIR}" GENERATED_DIR)

if (NOT EXISTS "${CLI_EXE}")
    message(FATAL_ERROR "CLI executable does not exist: ${CLI_EXE}. Build target zr_vm_cli_executable first.")
endif ()
if (NOT EXISTS "${PERF_RUNNER_EXE}")
    message(FATAL_ERROR "Performance runner does not exist: ${PERF_RUNNER_EXE}. Build target zr_vm_perf_runner first.")
endif ()
if (NOT EXISTS "${NATIVE_BENCHMARK_EXE}")
    message(FATAL_ERROR "Native benchmark runner does not exist: ${NATIVE_BENCHMARK_EXE}. Build target zr_vm_native_benchmark_runner first.")
endif ()
if (NOT IS_DIRECTORY "${BENCHMARKS_DIR}")
    message(FATAL_ERROR "Benchmarks directory does not exist: ${BENCHMARKS_DIR}")
endif ()

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
        NOT PERF_REQUESTED_TIER STREQUAL "stress" AND
        NOT PERF_REQUESTED_TIER STREQUAL "profile")
    message(FATAL_ERROR "Unsupported performance tier: ${PERF_REQUESTED_TIER}")
endif ()

if (PERF_REQUESTED_TIER STREQUAL "stress")
    set(PERF_DEFAULT_WARMUP 1)
    set(PERF_DEFAULT_ITERATIONS 2)
elseif (PERF_REQUESTED_TIER STREQUAL "profile")
    set(PERF_DEFAULT_WARMUP 0)
    set(PERF_DEFAULT_ITERATIONS 1)
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

if (NOT PERF_WARMUP MATCHES "^[0-9]+$" OR PERF_WARMUP LESS 0)
    message(FATAL_ERROR "Invalid PERF_WARMUP: ${PERF_WARMUP}")
endif ()

if (NOT PERF_ITERATIONS MATCHES "^[0-9]+$" OR PERF_ITERATIONS LESS 1)
    message(FATAL_ERROR "Invalid PERF_ITERATIONS: ${PERF_ITERATIONS}")
endif ()

# Callgrind: optional instruction-counting mode (no cache / branch simulation), via Valgrind flags.
# See: valgrind --tool=callgrind --help (simulation options).
set(PERF_CALLGRIND_COUNTING_MODE FALSE)
if (DEFINED ENV{ZR_VM_PERF_CALLGRIND_COUNTING} AND NOT "$ENV{ZR_VM_PERF_CALLGRIND_COUNTING}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_PERF_CALLGRIND_COUNTING}" PERF_CALLGRIND_COUNTING_ENV)
    if (PERF_CALLGRIND_COUNTING_ENV STREQUAL "1" OR
            PERF_CALLGRIND_COUNTING_ENV STREQUAL "yes" OR
            PERF_CALLGRIND_COUNTING_ENV STREQUAL "on" OR
            PERF_CALLGRIND_COUNTING_ENV STREQUAL "true")
        set(PERF_CALLGRIND_COUNTING_MODE TRUE)
    endif ()
endif ()
if (PERF_CALLGRIND_COUNTING_MODE)
    set(PERF_CALLGRIND_DOC_LINE "- Callgrind counting mode: **on** (passes `--cache-sim=no --branch-sim=no` to callgrind)\n")
    set(PERF_CALLGRIND_JSON_BOOL "true")
else ()
    set(PERF_CALLGRIND_DOC_LINE "- Callgrind counting mode: **off** (set `ZR_VM_PERF_CALLGRIND_COUNTING=1` to enable)\n")
    set(PERF_CALLGRIND_JSON_BOOL "false")
endif ()

# Optional: ZR_VM_PERF_ONLY_IMPLEMENTATIONS=comma-separated ids (e.g. zr_aot_c,zr_aot_llvm) to run a subset for diagnosis.
set(PERF_ONLY_FILTER_ACTIVE FALSE)
set(PERF_ONLY_IMPLEMENTATION_LIST "")
if (DEFINED ENV{ZR_VM_PERF_ONLY_IMPLEMENTATIONS} AND NOT "$ENV{ZR_VM_PERF_ONLY_IMPLEMENTATIONS}" STREQUAL "")
    set(PERF_ONLY_FILTER_ACTIVE TRUE)
    string(REPLACE "," ";" PERF_ONLY_IMPLEMENTATION_LIST "$ENV{ZR_VM_PERF_ONLY_IMPLEMENTATIONS}")
endif ()
if (PERF_ONLY_FILTER_ACTIVE)
    message("ZR_VM_PERF_ONLY_IMPLEMENTATIONS filter active: ${PERF_ONLY_IMPLEMENTATION_LIST}")
endif ()

set(PERF_ONLY_CASES_FILTER_ACTIVE FALSE)
set(PERF_ONLY_CASE_LIST "")
if (DEFINED ENV{ZR_VM_PERF_ONLY_CASES} AND NOT "$ENV{ZR_VM_PERF_ONLY_CASES}" STREQUAL "")
    set(PERF_ONLY_CASES_FILTER_ACTIVE TRUE)
    string(REPLACE "," ";" PERF_ONLY_CASE_LIST "$ENV{ZR_VM_PERF_ONLY_CASES}")
endif ()
if (PERF_ONLY_CASES_FILTER_ACTIVE)
    message("ZR_VM_PERF_ONLY_CASES filter active: ${PERF_ONLY_CASE_LIST}")
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

function(perf_decimal_delta value base out_var)
    if (value STREQUAL "" OR base STREQUAL "")
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

    perf_decimal_to_milli("${value}" value_milli)
    perf_decimal_to_milli("${base}" base_milli)
    math(EXPR delta_milli "${value_milli} - ${base_milli}")
    if (delta_milli LESS 0)
        math(EXPR delta_abs "${delta_milli} * -1")
        perf_format_milli_decimal("${delta_abs}" delta_text)
        set(${out_var} "-${delta_text}" PARENT_SCOPE)
    else ()
        perf_format_milli_decimal("${delta_milli}" delta_text)
        set(${out_var} "${delta_text}" PARENT_SCOPE)
    endif ()
endfunction()

function(perf_overhead_percent value base out_var)
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

    math(EXPR delta_milli "${value_milli} - ${base_milli}")
    if (delta_milli LESS 0)
        math(EXPR delta_abs "${delta_milli} * -1")
        math(EXPR percent_milli "((${delta_abs} * 100000) + (${base_milli} / 2)) / ${base_milli}")
        perf_format_milli_decimal("${percent_milli}" percent_text)
        set(${out_var} "-${percent_text}" PARENT_SCOPE)
    else ()
        math(EXPR percent_milli "((${delta_milli} * 100000) + (${base_milli} / 2)) / ${base_milli}")
        perf_format_milli_decimal("${percent_milli}" percent_text)
        set(${out_var} "${percent_text}" PARENT_SCOPE)
    endif ()
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

function(perf_case_scale case_name out_var)
    if (PERF_REQUESTED_TIER STREQUAL "profile")
        set(case_scale "${ZR_VM_BENCHMARK_PROFILE_SCALE_${case_name}}")
    else ()
        set(case_scale "${ZR_VM_BENCHMARK_TIER_SCALE_${PERF_REQUESTED_TIER}}")
    endif ()

    if (NOT case_scale)
        message(FATAL_ERROR "Missing scale for case ${case_name} tier ${PERF_REQUESTED_TIER}")
    endif ()

    set(${out_var} "${case_scale}" PARENT_SCOPE)
endfunction()

function(perf_implementation_is_core_gated case_name implementation_id out_var)
    set(core_implementations "${ZR_VM_BENCHMARK_CORE_IMPLEMENTATIONS_${case_name}}")
    list(FIND core_implementations "${implementation_id}" implementation_index)
    if (implementation_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif ()
endfunction()

function(perf_prepare_zr_case case_name out_project_dir_var out_project_file_var)
    set(source_dir "${BENCHMARKS_DIR}/cases/${case_name}/zr")
    set(destination_dir "${PERF_SUITE_ROOT}/cases/${case_name}/zr")
    set(project_file "${destination_dir}/benchmark_${case_name}.zrp")

    perf_case_scale("${case_name}" case_scale)
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
            "    return ${case_scale};\n"
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

function(perf_probe_program candidate out_var)
    if (NOT candidate)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif ()

    execute_process(
            COMMAND "${candidate}" ${ARGN}
            RESULT_VARIABLE probe_result
            OUTPUT_QUIET
            ERROR_QUIET
            TIMEOUT 15)
    if (probe_result EQUAL 0)
        set(${out_var} "${candidate}" PARENT_SCOPE)
    else ()
        set(${out_var} "" PARENT_SCOPE)
    endif ()
endfunction()

function(perf_translate_path_for_executable executable_path input_path out_var)
    if (input_path STREQUAL "")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif ()

    if (UNIX AND executable_path MATCHES "\\.exe$")
        execute_process(
                COMMAND wslpath -w "${input_path}"
                RESULT_VARIABLE translate_result
                OUTPUT_VARIABLE translated_path
                ERROR_VARIABLE translate_stderr
                TIMEOUT 15)
        if (NOT translate_result EQUAL 0)
            message(FATAL_ERROR
                    "Failed to translate Linux path for Windows executable '${executable_path}': ${input_path}\n${translate_stderr}")
        endif ()
        perf_normalize_output("${translated_path}" translated_path)
        set(${out_var} "${translated_path}" PARENT_SCOPE)
    else ()
        set(${out_var} "${input_path}" PARENT_SCOPE)
    endif ()
endfunction()

function(perf_case_is_hotspot_representative case_name out_var)
    if (PERF_REQUESTED_TIER STREQUAL "profile")
        list(FIND PERF_HOTSPOT_REPRESENTATIVE_CASES "${case_name}" representative_index)
        if (NOT representative_index EQUAL -1)
            set(${out_var} TRUE PARENT_SCOPE)
            return()
        endif ()
    endif ()

    set(${out_var} FALSE PARENT_SCOPE)
endfunction()

find_program(PERF_PYTHON_EXE_CANDIDATE NAMES python python3)
find_program(PERF_NODE_EXE_CANDIDATE NAMES node)
find_program(PERF_QJS_EXE_CANDIDATE NAMES qjs quickjs)
find_program(PERF_LUA_EXE_CANDIDATE NAMES lua lua54 lua5.4 luajit)
find_program(PERF_CARGO_EXE_CANDIDATE NAMES cargo)
find_program(PERF_DOTNET_EXE_CANDIDATE NAMES dotnet)
if (DEFINED ENV{ZR_VM_JAVA_EXE} AND NOT "$ENV{ZR_VM_JAVA_EXE}" STREQUAL "")
    set(PERF_JAVA_EXE_CANDIDATE "$ENV{ZR_VM_JAVA_EXE}")
else ()
    find_program(PERF_JAVA_EXE_CANDIDATE NAMES java)
endif ()
if (DEFINED ENV{ZR_VM_JAVAC_EXE} AND NOT "$ENV{ZR_VM_JAVAC_EXE}" STREQUAL "")
    set(PERF_JAVAC_EXE_CANDIDATE "$ENV{ZR_VM_JAVAC_EXE}")
else ()
    find_program(PERF_JAVAC_EXE_CANDIDATE NAMES javac)
endif ()
find_program(PERF_LLVM_HOST_TOOL_CANDIDATE NAMES clang-cl clang)
find_program(PERF_VALGRIND_EXE_CANDIDATE NAMES valgrind)
find_program(PERF_CALLGRIND_ANNOTATE_EXE_CANDIDATE NAMES callgrind_annotate)

perf_probe_program("${PERF_PYTHON_EXE_CANDIDATE}" PERF_PYTHON_EXE -c "print(0)")
perf_probe_program("${PERF_NODE_EXE_CANDIDATE}" PERF_NODE_EXE -e "process.exit(0)")
perf_probe_program("${PERF_QJS_EXE_CANDIDATE}" PERF_QJS_EXE -e "0;")
perf_probe_program("${PERF_LUA_EXE_CANDIDATE}" PERF_LUA_EXE -e "os.exit(0)")
perf_probe_program("${PERF_CARGO_EXE_CANDIDATE}" PERF_CARGO_EXE --version)
perf_probe_program("${PERF_DOTNET_EXE_CANDIDATE}" PERF_DOTNET_EXE --version)
perf_probe_program("${PERF_JAVA_EXE_CANDIDATE}" PERF_JAVA_EXE -version)
perf_probe_program("${PERF_JAVAC_EXE_CANDIDATE}" PERF_JAVAC_EXE -version)
perf_probe_program("${PERF_LLVM_HOST_TOOL_CANDIDATE}" PERF_LLVM_HOST_TOOL --version)
perf_probe_program("${PERF_VALGRIND_EXE_CANDIDATE}" PERF_VALGRIND_EXE --version)
perf_probe_program("${PERF_CALLGRIND_ANNOTATE_EXE_CANDIDATE}" PERF_CALLGRIND_ANNOTATE_EXE --version)
if (NOT PERF_VALGRIND_EXE AND PERF_VALGRIND_EXE_CANDIDATE)
    set(PERF_VALGRIND_EXE "${PERF_VALGRIND_EXE_CANDIDATE}")
endif ()
if (NOT PERF_CALLGRIND_ANNOTATE_EXE AND PERF_CALLGRIND_ANNOTATE_EXE_CANDIDATE)
    set(PERF_CALLGRIND_ANNOTATE_EXE "${PERF_CALLGRIND_ANNOTATE_EXE_CANDIDATE}")
endif ()

set(PERF_HOTSPOT_REPRESENTATIVE_CASES
        "numeric_loops"
        "dispatch_loops"
        "matrix_add_2d"
        "map_object_access")
set(PERF_HOTSPOT_SUMMARY_SCRIPT "${BENCHMARKS_DIR}/scripts/hotspot_summary.py")
if (NOT EXISTS "${PERF_HOTSPOT_SUMMARY_SCRIPT}")
    message(FATAL_ERROR "Missing hotspot summary script: ${PERF_HOTSPOT_SUMMARY_SCRIPT}")
endif ()

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
    set(PERF_DOTNET_INTERMEDIATE_DIR "${PERF_TOOLCHAIN_DIR}/dotnet_intermediate")
    file(MAKE_DIRECTORY "${PERF_DOTNET_OUTPUT_DIR}")
    file(MAKE_DIRECTORY "${PERF_DOTNET_INTERMEDIATE_DIR}/obj")
    execute_process(
            COMMAND
            "${PERF_DOTNET_EXE}" build "${BENCHMARKS_DIR}/dotnet_runner/BenchmarkRunner.csproj"
            -c Release
            -o "${PERF_DOTNET_OUTPUT_DIR}"
            "--disable-build-servers"
            "-p:BaseIntermediateOutputPath=${PERF_DOTNET_INTERMEDIATE_DIR}/obj/"
            "-p:MSBuildProjectExtensionsPath=${PERF_DOTNET_INTERMEDIATE_DIR}/obj/"
            RESULT_VARIABLE dotnet_build_result
            OUTPUT_VARIABLE dotnet_build_stdout
            ERROR_VARIABLE dotnet_build_stderr)
    if (NOT dotnet_build_result EQUAL 0)
        message(FATAL_ERROR "Failed to build .NET benchmark runner.\n${dotnet_build_stdout}${dotnet_build_stderr}")
    endif ()
    set(PERF_DOTNET_RUNNER_DLL "${PERF_DOTNET_OUTPUT_DIR}/BenchmarkRunner.dll")
endif ()

set(PERF_JAVA_CLASSES_DIR "")
if (PERF_JAVA_EXE AND PERF_JAVAC_EXE)
    set(PERF_JAVA_OUTPUT_DIR "${PERF_TOOLCHAIN_DIR}/java")
    set(PERF_JAVA_CLASSES_DIR "${PERF_JAVA_OUTPUT_DIR}/classes")
    file(REMOVE_RECURSE "${PERF_JAVA_OUTPUT_DIR}")
    file(MAKE_DIRECTORY "${PERF_JAVA_CLASSES_DIR}")
    file(GLOB_RECURSE PERF_JAVA_SOURCE_FILES
            "${BENCHMARKS_DIR}/java_runner/src/*.java"
            "${BENCHMARKS_DIR}/cases/*/java/*.java")
    list(SORT PERF_JAVA_SOURCE_FILES)
    if (PERF_JAVA_SOURCE_FILES STREQUAL "")
        message(FATAL_ERROR "Java toolchain is available but no Java benchmark sources were found.")
    endif ()
    perf_translate_path_for_executable("${PERF_JAVAC_EXE}" "${PERF_JAVA_CLASSES_DIR}" PERF_JAVA_CLASSES_DIR_ARG)
    set(PERF_JAVA_COMPILE_SOURCE_FILES "")
    foreach (java_source_file IN LISTS PERF_JAVA_SOURCE_FILES)
        perf_translate_path_for_executable("${PERF_JAVAC_EXE}" "${java_source_file}" java_source_file_arg)
        list(APPEND PERF_JAVA_COMPILE_SOURCE_FILES "${java_source_file_arg}")
    endforeach ()
    execute_process(
            COMMAND "${PERF_JAVAC_EXE}" -d "${PERF_JAVA_CLASSES_DIR_ARG}" ${PERF_JAVA_COMPILE_SOURCE_FILES}
            RESULT_VARIABLE java_build_result
            OUTPUT_VARIABLE java_build_stdout
            ERROR_VARIABLE java_build_stderr)
    if (NOT java_build_result EQUAL 0)
        message(FATAL_ERROR "Failed to build Java benchmark runner.\n${java_build_stdout}${java_build_stderr}")
    endif ()
endif ()

message("==========")
message("Running suite: performance_report")
message("Tier: ${PERF_REQUESTED_TIER}")
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
set(PERF_COMPARISON_MARKDOWN_ROWS "")
set(PERF_COMPARISON_JSON_CASES "")
set(PERF_INSTRUCTION_MARKDOWN_ROWS "")
set(PERF_INSTRUCTION_JSON_CASES "")
set(PERF_HOTSPOT_MARKDOWN_CASES "")
set(PERF_HOTSPOT_JSON_CASES "")
set(PERF_GC_BASELINE_CASE "gc_fragment_baseline")
set(PERF_GC_STRESS_CASE "gc_fragment_stress")
set(PERF_GC_OVERHEAD_MARKDOWN_ROWS "")
set(PERF_GC_OVERHEAD_JSON_ROWS "")

foreach (case_name IN LISTS ZR_VM_BENCHMARK_CASE_NAMES)
    if (PERF_ONLY_CASES_FILTER_ACTIVE)
        list(FIND PERF_ONLY_CASE_LIST "${case_name}" PERF_ONLY_CASE_IX)
        if (PERF_ONLY_CASE_IX LESS 0)
            continue()
        endif ()
    endif ()

    perf_case_matches_tier("${case_name}" case_enabled)
    if (NOT case_enabled)
        continue()
    endif ()

    if (PERF_ONLY_FILTER_ACTIVE)
        set(case_has_filtered_impl FALSE)
        foreach (impl IN LISTS ZR_VM_BENCHMARK_IMPLEMENTATIONS_${case_name})
            list(FIND PERF_ONLY_IMPLEMENTATION_LIST "${impl}" PERF_ONLY_CASE_IX)
            if (PERF_ONLY_CASE_IX GREATER_EQUAL 0)
                set(case_has_filtered_impl TRUE)
            endif ()
        endforeach ()
        if (NOT case_has_filtered_impl)
            continue()
        endif ()
    endif ()

    math(EXPR PERF_CASE_COUNT "${PERF_CASE_COUNT} + 1")
    perf_prepare_zr_case("${case_name}" zr_project_dir zr_project_file)
    perf_case_scale("${case_name}" case_scale)

    set(case_description "${ZR_VM_BENCHMARK_DESCRIPTION_${case_name}}")
    set(case_banner "${ZR_VM_BENCHMARK_PASS_BANNER_${case_name}}")
    set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_${PERF_REQUESTED_TIER}}")
    if (case_checksum STREQUAL "")
        set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_core}")
    endif ()
    set(case_expected_output "${case_banner}\n${case_checksum}")
    set(case_impl_jsons "")
    set(case_c_baseline_mean "")
    set(case_interp_mean "")
    set(case_python_mean "")
    set(case_node_mean "")
    set(case_qjs_mean "")
    set(case_lua_mean "")
    set(case_rust_mean "")
    set(case_dotnet_mean "")
    set(case_java_mean "")
    set(case_profile_report_path "")
    set(case_interp_command_list "")
    set(case_interp_working_directory "")
    set(case_interp_ready FALSE)

    foreach (implementation_id IN LISTS ZR_VM_BENCHMARK_IMPLEMENTATIONS_${case_name})
        if (PERF_ONLY_FILTER_ACTIVE)
            list(FIND PERF_ONLY_IMPLEMENTATION_LIST "${implementation_id}" PERF_ONLY_IMPL_IX)
            if (PERF_ONLY_IMPL_IX LESS 0)
                continue()
            endif ()
        endif ()
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
            if (PERF_REQUESTED_TIER STREQUAL "profile")
                list(APPEND command_list "--scale" "${case_scale}")
            endif ()
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
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/python")
                set(should_measure TRUE)
            else ()
                set(note "Python executable unavailable")
            endif ()
        elseif (implementation_id STREQUAL "node")
            set(implementation_name "Node.js")
            set(language "Node.js")
            set(mode "script")
            if (PERF_NODE_EXE)
                set(command_list "${PERF_NODE_EXE};${BENCHMARKS_DIR}/cases/${case_name}/node/main.js;--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/node")
                set(should_measure TRUE)
            else ()
                set(note "Node.js executable unavailable")
            endif ()
        elseif (implementation_id STREQUAL "qjs")
            set(implementation_name "QuickJS")
            set(language "QuickJS")
            set(mode "script")
            if (PERF_QJS_EXE)
                set(command_list "${PERF_QJS_EXE};-m;${BENCHMARKS_DIR}/cases/${case_name}/qjs/main.js;--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/qjs")
                set(should_measure TRUE)
            else ()
                set(note "QuickJS executable unavailable")
            endif ()
        elseif (implementation_id STREQUAL "lua")
            set(implementation_name "Lua")
            set(language "Lua")
            set(mode "script")
            if (PERF_LUA_EXE)
                set(command_list "${PERF_LUA_EXE};${BENCHMARKS_DIR}/cases/${case_name}/lua/main.lua;--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}/cases/${case_name}/lua")
                set(should_measure TRUE)
            else ()
                set(note "Lua executable unavailable")
            endif ()
        elseif (implementation_id STREQUAL "rust")
            set(implementation_name "Rust")
            set(language "Rust")
            set(mode "native")
            if (PERF_RUST_RUNNER_EXE)
                set(command_list "${PERF_RUST_RUNNER_EXE};--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}")
                set(should_measure TRUE)
            else ()
                set(note "Rust toolchain unavailable")
            endif ()
        elseif (implementation_id STREQUAL "dotnet")
            set(implementation_name "C#/.NET")
            set(language "C#/.NET")
            set(mode "native")
            if (PERF_DOTNET_RUNNER_DLL)
                set(command_list "${PERF_DOTNET_EXE};${PERF_DOTNET_RUNNER_DLL};--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}")
                set(should_measure TRUE)
            else ()
                set(note ".NET SDK unavailable")
            endif ()
        elseif (implementation_id STREQUAL "java")
            set(implementation_name "Java")
            set(language "Java")
            set(mode "managed")
            if (PERF_JAVA_CLASSES_DIR)
                perf_translate_path_for_executable("${PERF_JAVA_EXE}" "${PERF_JAVA_CLASSES_DIR}" java_classpath_arg)
                set(command_list "${PERF_JAVA_EXE};-cp;${java_classpath_arg};BenchmarkRunner;--case;${case_name};--tier;${PERF_REQUESTED_TIER}")
                if (PERF_REQUESTED_TIER STREQUAL "profile")
                    list(APPEND command_list "--scale" "${case_scale}")
                endif ()
                set(working_directory "${BENCHMARKS_DIR}")
                set(should_measure TRUE)
            else ()
                set(note "Java toolchain unavailable")
            endif ()
        else ()
            message(FATAL_ERROR "Unknown implementation id in registry: ${implementation_id}")
        endif ()

        perf_implementation_is_core_gated("${case_name}" "${implementation_id}" implementation_is_core_gated)
        set(profile_report_path "")
        if (PERF_REQUESTED_TIER STREQUAL "profile" AND implementation_id STREQUAL "zr_interp")
            set(profile_report_path "${PERF_REPORT_DIR}/${case_name}__${implementation_id}.profile.json")
        endif ()
        if (implementation_id STREQUAL "zr_interp")
            set(case_interp_command_list ${command_list})
            set(case_interp_working_directory "${working_directory}")
            set(case_interp_ready TRUE)
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
                    elseif (NOT implementation_is_core_gated)
                        set(status "SKIP")
                        set(note "out-of-scope follow-up debt: prepare step failed")
                        perf_append_note("skip"
                                "${case_name}"
                                "${implementation_name}"
                                "follow-up debt: prepare step failed.\n${prepare_output}")
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
                perf_normalize_output("${correctness_stdout}" correctness_output)
                perf_strip_contract_noise("${correctness_output}" correctness_output)
                perf_normalize_output("${correctness_stderr}" correctness_error_output)
                if (correctness_output STREQUAL "")
                    set(correctness_combined_output "${correctness_error_output}")
                elseif (correctness_error_output STREQUAL "")
                    set(correctness_combined_output "${correctness_output}")
                else ()
                    set(correctness_combined_output "${correctness_output}\n${correctness_error_output}")
                endif ()
                perf_normalize_output("${case_expected_output}" expected_output_normalized)
                if (NOT correctness_result EQUAL 0)
                    if (implementation_id STREQUAL "zr_binary" AND correctness_combined_output MATCHES "failed to load project entry") 
                        set(status "SKIP")
                        set(note "binary entry loader unavailable for this benchmark")
                        perf_append_note("skip" "${case_name}" "${implementation_name}" "${note}")
                    elseif (NOT implementation_is_core_gated)
                        set(status "SKIP")
                        set(note "out-of-scope follow-up debt: correctness run failed")
                        perf_append_note("skip"
                                "${case_name}"
                                "${implementation_name}"
                                "follow-up debt: correctness run failed with exit code ${correctness_result}.\n${correctness_combined_output}")
                    else ()
                        set(status "FAIL")
                        set(note "correctness run failed")
                        perf_append_note("failure"
                                "${case_name}"
                                "${implementation_name}"
                                "correctness run failed with exit code ${correctness_result}.\n${correctness_combined_output}")
                        set(PERF_HARD_FAILURE TRUE)
                    endif ()
                elseif (NOT correctness_output STREQUAL expected_output_normalized)
                    if (NOT implementation_is_core_gated)
                        set(status "SKIP")
                        set(note "out-of-scope follow-up debt: correctness output mismatch")
                        perf_append_note("skip"
                                "${case_name}"
                                "${implementation_name}"
                                "follow-up debt: expected `${expected_output_normalized}` but got `${correctness_output}`")
                    else ()
                        set(status "FAIL")
                        set(note "correctness output mismatch")
                        perf_append_note("failure"
                                "${case_name}"
                                "${implementation_name}"
                                "expected `${expected_output_normalized}` but got `${correctness_output}`")
                        set(PERF_HARD_FAILURE TRUE)
                    endif ()
                endif ()
            endif ()

            if (NOT status STREQUAL "FAIL" AND NOT status STREQUAL "SKIP")
                set(perf_json_path "${PERF_REPORT_DIR}/${case_name}__${implementation_id}.json")
                if (profile_report_path STREQUAL "")
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
                else ()
                    execute_process(
                            COMMAND
                            "${CMAKE_COMMAND}" -E env
                            "ZR_VM_PROFILE_INSTRUCTIONS=1"
                            "ZR_VM_PROFILE_SLOWPATHS=1"
                            "ZR_VM_PROFILE_HELPERS=1"
                            "ZR_VM_PROFILE_OUT=${profile_report_path}"
                            "ZR_VM_PROFILE_CASE=${case_name}"
                            "ZR_VM_PROFILE_MODE=${mode}"
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
                endif ()
                set(perf_runner_output "${perf_runner_stdout}${perf_runner_stderr}")
                if (NOT perf_runner_result EQUAL 0)
                    if (NOT implementation_is_core_gated)
                        set(status "SKIP")
                        set(note "out-of-scope follow-up debt: measurement failed")
                        perf_append_note("skip"
                                "${case_name}"
                                "${implementation_name}"
                                "follow-up debt: perf runner failed.\n${perf_runner_output}")
                    else ()
                        set(status "FAIL")
                        set(note "measurement failed")
                        perf_append_note("failure"
                                "${case_name}"
                                "${implementation_name}"
                                "perf runner failed.\n${perf_runner_output}")
                        set(PERF_HARD_FAILURE TRUE)
                    endif ()
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
                    elseif (implementation_id STREQUAL "zr_interp")
                        set(case_interp_mean "${mean_wall_ms}")
                        set(case_profile_report_path "${profile_report_path}")
                    elseif (implementation_id STREQUAL "python")
                        set(case_python_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "node")
                        set(case_node_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "qjs")
                        set(case_qjs_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "lua")
                        set(case_lua_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "rust")
                        set(case_rust_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "dotnet")
                        set(case_dotnet_mean "${mean_wall_ms}")
                    elseif (implementation_id STREQUAL "java")
                        set(case_java_mean "${mean_wall_ms}")
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

        if (case_name STREQUAL PERF_GC_BASELINE_CASE OR case_name STREQUAL PERF_GC_STRESS_CASE)
            set("PERF_GC_IMPLEMENTATION_NAME_${implementation_id}" "${implementation_name}")
            set("PERF_GC_LANGUAGE_${implementation_id}" "${language}")
            set("PERF_GC_STATUS_${case_name}_${implementation_id}" "${status}")
            set("PERF_GC_NOTE_${case_name}_${implementation_id}" "${note}")
            if (status STREQUAL "PASS")
                set("PERF_GC_MEAN_WALL_${case_name}_${implementation_id}" "${mean_wall_ms}")
                set("PERF_GC_MEAN_PEAK_${case_name}_${implementation_id}" "${mean_peak_mib}")
            else ()
                set("PERF_GC_MEAN_WALL_${case_name}_${implementation_id}" "")
                set("PERF_GC_MEAN_PEAK_${case_name}_${implementation_id}" "")
            endif ()
        endif ()
    endforeach ()

    perf_escape_json_string("${case_name}" json_case_name)
    perf_escape_json_string("${case_description}" json_case_description)
    perf_escape_json_string("${case_banner}" json_case_banner)
    perf_escape_json_string("${ZR_VM_BENCHMARK_WORKLOAD_TAG_${case_name}}" json_case_workload_tag)

    if (NOT case_interp_mean STREQUAL "")
        perf_relative_to_c("${case_interp_mean}" "${case_c_baseline_mean}" ratio_to_c)
        perf_relative_to_c("${case_interp_mean}" "${case_lua_mean}" ratio_to_lua)
        perf_relative_to_c("${case_interp_mean}" "${case_qjs_mean}" ratio_to_qjs)
        perf_relative_to_c("${case_interp_mean}" "${case_node_mean}" ratio_to_node)
        perf_relative_to_c("${case_interp_mean}" "${case_python_mean}" ratio_to_python)
        perf_relative_to_c("${case_interp_mean}" "${case_dotnet_mean}" ratio_to_dotnet)
        perf_relative_to_c("${case_interp_mean}" "${case_java_mean}" ratio_to_java)
        perf_relative_to_c("${case_interp_mean}" "${case_rust_mean}" ratio_to_rust)
        foreach (ratio_var IN ITEMS ratio_to_c ratio_to_lua ratio_to_qjs ratio_to_node ratio_to_python ratio_to_dotnet ratio_to_java ratio_to_rust)
            if (${ratio_var} STREQUAL "null")
                set(${ratio_var} "-")
            endif ()
        endforeach ()

        string(APPEND PERF_COMPARISON_MARKDOWN_ROWS
                "| ${case_name} | ${ZR_VM_BENCHMARK_WORKLOAD_TAG_${case_name}} | ${ratio_to_c} | ${ratio_to_lua} | ${ratio_to_qjs} | ${ratio_to_node} | ${ratio_to_python} | ${ratio_to_dotnet} | ${ratio_to_java} | ${ratio_to_rust} |\n")
        string(CONCAT comparison_case_json
                "    {\n"
                "      \"name\": \"${json_case_name}\",\n"
                "      \"workload_tag\": \"${json_case_workload_tag}\",\n"
                "      \"relative_to\": {\n"
                "        \"c\": \"${ratio_to_c}\",\n"
                "        \"lua\": \"${ratio_to_lua}\",\n"
                "        \"qjs\": \"${ratio_to_qjs}\",\n"
                "        \"node\": \"${ratio_to_node}\",\n"
                "        \"python\": \"${ratio_to_python}\",\n"
                "        \"dotnet\": \"${ratio_to_dotnet}\",\n"
                "        \"java\": \"${ratio_to_java}\",\n"
                "        \"rust\": \"${ratio_to_rust}\"\n"
                "      }\n"
                "    }")
        if (PERF_COMPARISON_JSON_CASES STREQUAL "")
            set(PERF_COMPARISON_JSON_CASES "${comparison_case_json}")
        else ()
            set(PERF_COMPARISON_JSON_CASES "${PERF_COMPARISON_JSON_CASES},\n${comparison_case_json}")
        endif ()
    endif ()

    perf_case_is_hotspot_representative("${case_name}" case_hotspot_representative)

    if (NOT case_profile_report_path STREQUAL "" AND EXISTS "${case_profile_report_path}")
        string(APPEND PERF_INSTRUCTION_MARKDOWN_ROWS
                "| ${case_name} | available | `${case_profile_report_path}` |\n")
        file(READ "${case_profile_report_path}" case_profile_json_text)
        string(STRIP "${case_profile_json_text}" case_profile_json_text)
        if (PERF_INSTRUCTION_JSON_CASES STREQUAL "")
            set(PERF_INSTRUCTION_JSON_CASES "${case_profile_json_text}")
        else ()
            set(PERF_INSTRUCTION_JSON_CASES "${PERF_INSTRUCTION_JSON_CASES},\n${case_profile_json_text}")
        endif ()
        if (case_hotspot_representative AND
                PERF_VALGRIND_EXE AND
                PERF_CALLGRIND_ANNOTATE_EXE AND
                PERF_PYTHON_EXE AND
                case_interp_ready AND
                NOT case_interp_working_directory STREQUAL "")
            set(case_callgrind_out_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.callgrind.out")
            set(case_callgrind_annotate_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.callgrind.annotate.txt")
            set(case_hotspot_summary_json_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.hotspot.json")
            set(case_hotspot_summary_md_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.hotspot.md")
            set(_perf_callgrind_cmd
                    "${PERF_VALGRIND_EXE}"
                    "--tool=callgrind"
                    "--trace-children=no"
                    "--callgrind-out-file=${case_callgrind_out_path}")
            if (PERF_CALLGRIND_COUNTING_MODE)
                list(APPEND _perf_callgrind_cmd "--cache-sim=no" "--branch-sim=no")
            endif ()
            list(APPEND _perf_callgrind_cmd ${case_interp_command_list})
            execute_process(
                    COMMAND ${_perf_callgrind_cmd}
                    WORKING_DIRECTORY "${case_interp_working_directory}"
                    RESULT_VARIABLE case_callgrind_result
                    OUTPUT_VARIABLE case_callgrind_stdout
                    ERROR_VARIABLE case_callgrind_stderr
                    TIMEOUT 3600)
            if (NOT case_callgrind_result EQUAL 0 OR NOT EXISTS "${case_callgrind_out_path}")
                string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                        "### ${case_name}\n"
                        "- Instruction profile: `${case_profile_report_path}`\n"
                        "- Callgrind: failed during representative workload capture\n\n")
                string(CONCAT hotspot_case_json
                        "    {\n"
                        "      \"name\": \"${json_case_name}\",\n"
                        "      \"status\": \"callgrind_failed\",\n"
                        "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                        "      \"callgrind\": null\n"
                        "    }")
                perf_append_note("failure"
                        "${case_name}"
                        "ZR interp hotspot"
                        "callgrind capture failed.\n${case_callgrind_stdout}${case_callgrind_stderr}")
                set(PERF_HARD_FAILURE TRUE)
            else ()
                execute_process(
                        COMMAND
                        "${PERF_CALLGRIND_ANNOTATE_EXE}"
                        "--auto=no"
                        "--threshold=99"
                        "${case_callgrind_out_path}"
                        RESULT_VARIABLE case_annotate_result
                        OUTPUT_VARIABLE case_annotate_stdout
                        ERROR_VARIABLE case_annotate_stderr
                        TIMEOUT 600)
                if (NOT case_annotate_result EQUAL 0)
                    string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                            "### ${case_name}\n"
                            "- Instruction profile: `${case_profile_report_path}`\n"
                            "- Callgrind: annotate step failed\n\n")
                    string(CONCAT hotspot_case_json
                            "    {\n"
                            "      \"name\": \"${json_case_name}\",\n"
                            "      \"status\": \"callgrind_annotate_failed\",\n"
                            "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                            "      \"callgrind\": null\n"
                            "    }")
                    perf_append_note("failure"
                            "${case_name}"
                            "ZR interp hotspot"
                            "callgrind annotate failed.\n${case_annotate_stdout}${case_annotate_stderr}")
                    set(PERF_HARD_FAILURE TRUE)
                else ()
                    file(WRITE "${case_callgrind_annotate_path}" "${case_annotate_stdout}${case_annotate_stderr}")
                    execute_process(
                            COMMAND
                            "${PERF_PYTHON_EXE}"
                            "${PERF_HOTSPOT_SUMMARY_SCRIPT}"
                            "--case" "${case_name}"
                            "--instruction-profile" "${case_profile_report_path}"
                            "--callgrind-out" "${case_callgrind_out_path}"
                            "--callgrind-annotate" "${case_callgrind_annotate_path}"
                            "--json-out" "${case_hotspot_summary_json_path}"
                            "--markdown-out" "${case_hotspot_summary_md_path}"
                            RESULT_VARIABLE case_hotspot_summary_result
                            OUTPUT_VARIABLE case_hotspot_summary_stdout
                            ERROR_VARIABLE case_hotspot_summary_stderr
                            TIMEOUT 120)
                    if (NOT case_hotspot_summary_result EQUAL 0 OR
                            NOT EXISTS "${case_hotspot_summary_json_path}" OR
                            NOT EXISTS "${case_hotspot_summary_md_path}")
                        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                                "### ${case_name}\n"
                                "- Instruction profile: `${case_profile_report_path}`\n"
                                "- Callgrind: summary generation failed\n\n")
                        string(CONCAT hotspot_case_json
                                "    {\n"
                                "      \"name\": \"${json_case_name}\",\n"
                                "      \"status\": \"hotspot_summary_failed\",\n"
                                "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                                "      \"callgrind\": null\n"
                                "    }")
                        perf_append_note("failure"
                                "${case_name}"
                                "ZR interp hotspot"
                                "hotspot summary generation failed.\n${case_hotspot_summary_stdout}${case_hotspot_summary_stderr}")
                        set(PERF_HARD_FAILURE TRUE)
                    else ()
                        file(READ "${case_hotspot_summary_md_path}" case_hotspot_markdown_text)
                        file(READ "${case_hotspot_summary_json_path}" hotspot_case_json)
                        string(STRIP "${case_hotspot_markdown_text}" case_hotspot_markdown_text)
                        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES "${case_hotspot_markdown_text}\n\n")
                    endif ()
                endif ()
            endif ()
        elseif (case_hotspot_representative)
            string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                    "### ${case_name}\n"
                    "- Instruction profile: `${case_profile_report_path}`\n"
                    "- Callgrind: unavailable (missing valgrind, callgrind_annotate, python, or interp command)\n\n")
            string(CONCAT hotspot_case_json
                    "    {\n"
                    "      \"name\": \"${json_case_name}\",\n"
                    "      \"status\": \"instruction_profile_available\",\n"
                    "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                    "      \"callgrind\": null\n"
                    "    }")
        else ()
            string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                    "### ${case_name}\n"
                    "- Instruction profile: `${case_profile_report_path}`\n"
                    "- Callgrind: not captured in this tier (representative profile cases only)\n\n")
            string(CONCAT hotspot_case_json
                    "    {\n"
                    "      \"name\": \"${json_case_name}\",\n"
                    "      \"status\": \"instruction_profile_available\",\n"
                    "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                    "      \"callgrind\": null\n"
                    "    }")
        endif ()
    else ()
        string(APPEND PERF_INSTRUCTION_MARKDOWN_ROWS
                "| ${case_name} | missing | - |\n")
        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                "### ${case_name}\n"
                "- Instruction profile: unavailable\n"
                "- Callgrind: unavailable in this run\n\n")
        string(CONCAT hotspot_case_json
                "    {\n"
                "      \"name\": \"${json_case_name}\",\n"
                "      \"status\": \"unavailable\",\n"
                "      \"instruction_profile\": null,\n"
                "      \"callgrind\": null\n"
                "    }")
    endif ()
    if (PERF_HOTSPOT_JSON_CASES STREQUAL "")
        set(PERF_HOTSPOT_JSON_CASES "${hotspot_case_json}")
    else ()
        set(PERF_HOTSPOT_JSON_CASES "${PERF_HOTSPOT_JSON_CASES},\n${hotspot_case_json}")
    endif ()

    string(CONCAT case_json
            "    {\n"
            "      \"name\": \"${json_case_name}\",\n"
            "      \"description\": \"${json_case_description}\",\n"
            "      \"workload_tag\": \"${json_case_workload_tag}\",\n"
            "      \"pass_banner\": \"${json_case_banner}\",\n"
            "      \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
            "      \"scale\": ${case_scale},\n"
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

foreach (implementation_id IN LISTS ZR_VM_BENCHMARK_IMPLEMENTATION_ORDER)
    set(base_status_var "PERF_GC_STATUS_${PERF_GC_BASELINE_CASE}_${implementation_id}")
    set(stress_status_var "PERF_GC_STATUS_${PERF_GC_STRESS_CASE}_${implementation_id}")
    set(base_status "${${base_status_var}}")
    set(stress_status "${${stress_status_var}}")

    if (base_status STREQUAL "" AND stress_status STREQUAL "")
        continue()
    endif ()

    set(impl_name_var "PERF_GC_IMPLEMENTATION_NAME_${implementation_id}")
    set(impl_language_var "PERF_GC_LANGUAGE_${implementation_id}")
    set(implementation_name "${${impl_name_var}}")
    set(language "${${impl_language_var}}")
    if (implementation_name STREQUAL "")
        set(implementation_name "${implementation_id}")
    endif ()
    if (language STREQUAL "")
        set(language "-")
    endif ()

    if (base_status STREQUAL "PASS" AND stress_status STREQUAL "PASS")
        set(base_mean_var "PERF_GC_MEAN_WALL_${PERF_GC_BASELINE_CASE}_${implementation_id}")
        set(stress_mean_var "PERF_GC_MEAN_WALL_${PERF_GC_STRESS_CASE}_${implementation_id}")
        set(base_peak_var "PERF_GC_MEAN_PEAK_${PERF_GC_BASELINE_CASE}_${implementation_id}")
        set(stress_peak_var "PERF_GC_MEAN_PEAK_${PERF_GC_STRESS_CASE}_${implementation_id}")
        set(base_mean "${${base_mean_var}}")
        set(stress_mean "${${stress_mean_var}}")
        set(base_peak "${${base_peak_var}}")
        set(stress_peak "${${stress_peak_var}}")

        perf_relative_to_c("${stress_mean}" "${base_mean}" stress_vs_baseline)
        perf_decimal_delta("${stress_mean}" "${base_mean}" wall_delta)
        perf_overhead_percent("${stress_mean}" "${base_mean}" overhead_pct)
        perf_decimal_delta("${stress_peak}" "${base_peak}" peak_delta)
        set(gc_status "PASS")
        set(gc_note "")
    else ()
        set(base_mean "-")
        set(stress_mean "-")
        set(stress_vs_baseline "-")
        set(wall_delta "-")
        set(overhead_pct "-")
        set(base_peak "-")
        set(stress_peak "-")
        set(peak_delta "-")
        set(gc_status "SKIP")
        set(base_note_var "PERF_GC_NOTE_${PERF_GC_BASELINE_CASE}_${implementation_id}")
        set(stress_note_var "PERF_GC_NOTE_${PERF_GC_STRESS_CASE}_${implementation_id}")
        set(base_note "${${base_note_var}}")
        set(stress_note "${${stress_note_var}}")
        set(gc_note "baseline=${base_status}; stress=${stress_status}")
        if (NOT base_note STREQUAL "")
            set(gc_note "${gc_note}; baseline_note=${base_note}")
        endif ()
        if (NOT stress_note STREQUAL "")
            set(gc_note "${gc_note}; stress_note=${stress_note}")
        endif ()
    endif ()

    string(APPEND PERF_GC_OVERHEAD_MARKDOWN_ROWS
            "| ${implementation_name} | ${language} | ${gc_status} | ${base_mean} | ${stress_mean} | ${stress_vs_baseline} | ${wall_delta} | ${overhead_pct} | ${base_peak} | ${stress_peak} | ${peak_delta} |\n")

    if (gc_note STREQUAL "")
        set(gc_note_json "null")
    else ()
        perf_escape_json_string("${gc_note}" gc_note_escaped)
        set(gc_note_json "\"${gc_note_escaped}\"")
    endif ()
    if (stress_vs_baseline STREQUAL "-")
        set(stress_vs_baseline_json "null")
    else ()
        set(stress_vs_baseline_json "\"${stress_vs_baseline}\"")
    endif ()
    if (wall_delta STREQUAL "-")
        set(wall_delta_json "null")
    else ()
        set(wall_delta_json "\"${wall_delta}\"")
    endif ()
    if (overhead_pct STREQUAL "-")
        set(overhead_pct_json "null")
    else ()
        set(overhead_pct_json "\"${overhead_pct}\"")
    endif ()
    if (peak_delta STREQUAL "-")
        set(peak_delta_json "null")
    else ()
        set(peak_delta_json "\"${peak_delta}\"")
    endif ()

    perf_escape_json_string("${implementation_name}" json_impl_name)
    perf_escape_json_string("${language}" json_language)
    string(CONCAT gc_row_json
            "    {\n"
            "      \"implementation_id\": \"${implementation_id}\",\n"
            "      \"implementation_name\": \"${json_impl_name}\",\n"
            "      \"language\": \"${json_language}\",\n"
            "      \"status\": \"${gc_status}\",\n"
            "      \"baseline_mean_wall_ms\": " "\"${base_mean}\"" ",\n"
            "      \"stress_mean_wall_ms\": " "\"${stress_mean}\"" ",\n"
            "      \"stress_vs_baseline\": ${stress_vs_baseline_json},\n"
            "      \"wall_delta_ms\": ${wall_delta_json},\n"
            "      \"overhead_percent\": ${overhead_pct_json},\n"
            "      \"baseline_mean_peak_mib\": " "\"${base_peak}\"" ",\n"
            "      \"stress_mean_peak_mib\": " "\"${stress_peak}\"" ",\n"
            "      \"peak_delta_mib\": ${peak_delta_json},\n"
            "      \"note\": ${gc_note_json}\n"
            "    }")
    if (PERF_GC_OVERHEAD_JSON_ROWS STREQUAL "")
        set(PERF_GC_OVERHEAD_JSON_ROWS "${gc_row_json}")
    else ()
        set(PERF_GC_OVERHEAD_JSON_ROWS "${PERF_GC_OVERHEAD_JSON_ROWS},\n${gc_row_json}")
    endif ()
endforeach ()

if (PERF_GC_OVERHEAD_MARKDOWN_ROWS STREQUAL "")
    set(PERF_GC_OVERHEAD_MARKDOWN_ROWS "| none | - | SKIP | - | - | - | - | - | - | - | - |\n")
endif ()

string(TIMESTAMP PERF_GENERATED_AT_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.md" PERF_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/benchmark_report.json" PERF_JSON_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/comparison_report.md" PERF_COMPARISON_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/comparison_report.json" PERF_COMPARISON_JSON_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/instruction_report.md" PERF_INSTRUCTION_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/instruction_report.json" PERF_INSTRUCTION_JSON_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/hotspot_report.md" PERF_HOTSPOT_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/hotspot_report.json" PERF_HOTSPOT_JSON_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/gc_overhead_report.md" PERF_GC_OVERHEAD_MARKDOWN_PATH_NORMALIZED)
file(TO_CMAKE_PATH "${PERF_REPORT_DIR}/gc_overhead_report.json" PERF_GC_OVERHEAD_JSON_PATH_NORMALIZED)

string(CONCAT PERF_MARKDOWN_REPORT
        "# ZR VM Performance Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Scale Policy: registry tier scale (profile uses per-case profile scale)\n"
        "- Warmup Iterations Per Implementation: ${PERF_WARMUP}\n"
        "- Measured Iterations Per Implementation: ${PERF_ITERATIONS}\n"
        "${PERF_CALLGRIND_DOC_LINE}"
        "- **Wall ms scope:** For **ZR binary**, **ZR aot_c**, and **ZR aot_llvm**, the suite runs a **separate untimed** one-shot `zr_vm_cli --compile ...` (and `--emit-aot-c` / `--emit-aot-llvm` for AOT) **before** `perf_runner`. **Prepare / host compile time is not included** in the table; reported wall ms are **run-only** (`perf_runner` child process for `zr_vm_cli ... --execution-mode ...`). CSV column `one_shot_compile_excluded_from_wall_ms` flags these modes.\n"
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
        "- JSON Report: `${PERF_JSON_PATH_NORMALIZED}`\n"
        "- Comparison Report: `${PERF_COMPARISON_MARKDOWN_PATH_NORMALIZED}`\n"
        "- GC Overhead Report: `${PERF_GC_OVERHEAD_MARKDOWN_PATH_NORMALIZED}`\n"
        "- Instruction Report: `${PERF_INSTRUCTION_MARKDOWN_PATH_NORMALIZED}`\n"
        "- Hotspot Report: `${PERF_HOTSPOT_MARKDOWN_PATH_NORMALIZED}`\n")

file(WRITE "${PERF_REPORT_DIR}/benchmark_report.md" "${PERF_MARKDOWN_REPORT}")
file(WRITE
        "${PERF_REPORT_DIR}/benchmark_report.json"
        "{\n"
        "  \"suite\": \"performance_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"scale_policy\": \"tier_default_or_case_profile\",\n"
        "  \"warmup\": ${PERF_WARMUP},\n"
        "  \"iterations\": ${PERF_ITERATIONS},\n"
        "  \"callgrind_counting_mode\": ${PERF_CALLGRIND_JSON_BOOL},\n"
        "  \"reported_wall_ms_includes_prepare_compile\": false,\n"
        "  \"reported_wall_ms_scope\": \"perf_runner_iterations_only_excludes_cmake_prepare_zr_vm_cli_compile\",\n"
        "  \"cases\": [\n${PERF_JSON_CASES}\n  ]\n"
        "}\n")

if (PERF_COMPARISON_MARKDOWN_ROWS STREQUAL "")
    set(PERF_COMPARISON_MARKDOWN_ROWS "| none | none | - | - | - | - | - | - | - |\n")
endif ()
if (PERF_COMPARISON_JSON_CASES STREQUAL "")
    set(PERF_COMPARISON_JSON_CASES "")
endif ()
file(WRITE
        "${PERF_REPORT_DIR}/comparison_report.md"
        "# ZR VM Comparison Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n\n"
        "| case | workload | vs C | vs Lua | vs QuickJS | vs Node.js | vs Python | vs .NET | vs Java | vs Rust |\n"
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n"
        "${PERF_COMPARISON_MARKDOWN_ROWS}")
file(WRITE
        "${PERF_REPORT_DIR}/comparison_report.json"
        "{\n"
        "  \"suite\": \"comparison_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"cases\": [\n${PERF_COMPARISON_JSON_CASES}\n  ]\n"
        "}\n")

file(WRITE
        "${PERF_REPORT_DIR}/gc_overhead_report.md"
        "# ZR VM GC Overhead Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Baseline Case: `${PERF_GC_BASELINE_CASE}`\n"
        "- Stress Case: `${PERF_GC_STRESS_CASE}`\n"
        "- Overhead compares stress against baseline for the same implementation.\n\n"
        "| implementation | language | status | baseline mean wall ms | stress mean wall ms | stress/baseline | wall delta ms | overhead % | baseline mean peak MiB | stress mean peak MiB | peak delta MiB |\n"
        "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n"
        "${PERF_GC_OVERHEAD_MARKDOWN_ROWS}")
file(WRITE
        "${PERF_REPORT_DIR}/gc_overhead_report.json"
        "{\n"
        "  \"suite\": \"gc_overhead_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"baseline_case\": \"${PERF_GC_BASELINE_CASE}\",\n"
        "  \"stress_case\": \"${PERF_GC_STRESS_CASE}\",\n"
        "  \"rows\": [\n${PERF_GC_OVERHEAD_JSON_ROWS}\n  ]\n"
        "}\n")

file(WRITE
        "${PERF_REPORT_DIR}/instruction_report.md"
        "# ZR VM Instruction Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n\n"
        "| case | status | profile artifact |\n"
        "| --- | --- | --- |\n"
        "${PERF_INSTRUCTION_MARKDOWN_ROWS}")
file(WRITE
        "${PERF_REPORT_DIR}/instruction_report.json"
        "{\n"
        "  \"suite\": \"instruction_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"cases\": [\n${PERF_INSTRUCTION_JSON_CASES}\n  ]\n"
        "}\n")

file(WRITE
        "${PERF_REPORT_DIR}/hotspot_report.md"
        "# ZR VM Hotspot Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "${PERF_CALLGRIND_DOC_LINE}"
        "\n"
        "${PERF_HOTSPOT_MARKDOWN_CASES}")
file(WRITE
        "${PERF_REPORT_DIR}/hotspot_report.json"
        "{\n"
        "  \"suite\": \"hotspot_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"callgrind_counting_mode\": ${PERF_CALLGRIND_JSON_BOOL},\n"
        "  \"cases\": [\n${PERF_HOTSPOT_JSON_CASES}\n  ]\n"
        "}\n")

message("Performance markdown report: ${PERF_REPORT_DIR}/benchmark_report.md")
message("Performance json report: ${PERF_REPORT_DIR}/benchmark_report.json")
message("Comparison markdown report: ${PERF_REPORT_DIR}/comparison_report.md")
message("GC overhead markdown report: ${PERF_REPORT_DIR}/gc_overhead_report.md")
message("Instruction markdown report: ${PERF_REPORT_DIR}/instruction_report.md")
message("Hotspot markdown report: ${PERF_REPORT_DIR}/hotspot_report.md")
if (PERF_CALLGRIND_COUNTING_MODE)
    message("Callgrind counting mode: on (--cache-sim=no --branch-sim=no)")
else ()
    message("Callgrind counting mode: off (set ZR_VM_PERF_CALLGRIND_COUNTING=1 to enable)")
endif ()

if (PERF_HARD_FAILURE)
    message(FATAL_ERROR "performance_report encountered benchmark failures. See generated report for details.")
endif ()
