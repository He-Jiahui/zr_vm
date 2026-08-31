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
include("${CMAKE_CURRENT_LIST_DIR}/benchmark_measurement_contract.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/benchmark_persistent_commands.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/benchmark_task3_suite.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/benchmark_task4_environment.cmake")

file(TO_CMAKE_PATH "${CLI_EXE}" CLI_EXE)
file(TO_CMAKE_PATH "${PERF_RUNNER_EXE}" PERF_RUNNER_EXE)
file(TO_CMAKE_PATH "${NATIVE_BENCHMARK_EXE}" NATIVE_BENCHMARK_EXE)
file(TO_CMAKE_PATH "${BENCHMARKS_DIR}" BENCHMARKS_DIR)
file(TO_CMAKE_PATH "${GENERATED_DIR}" GENERATED_DIR)
if (DEFINED HOST_BINARY_DIR AND NOT HOST_BINARY_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${HOST_BINARY_DIR}" HOST_BINARY_DIR)
endif ()

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

set(PERF_SCOPE_MODE "process")
if (DEFINED ENV{ZR_VM_PERF_SCOPE} AND NOT "$ENV{ZR_VM_PERF_SCOPE}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_PERF_SCOPE}" PERF_SCOPE_MODE)
endif ()
if (NOT PERF_SCOPE_MODE STREQUAL "process" AND NOT PERF_SCOPE_MODE STREQUAL "steady")
    message(FATAL_ERROR "Unsupported ZR_VM_PERF_SCOPE: ${PERF_SCOPE_MODE}; expected process or steady")
endif ()
if (PERF_SCOPE_MODE STREQUAL "steady" AND PERF_REQUESTED_TIER STREQUAL "profile")
    message(FATAL_ERROR "ZR_VM_PERF_SCOPE=steady cannot be combined with profile/Callgrind mode")
endif ()
if (PERF_SCOPE_MODE STREQUAL "steady")
    if (NOT DEFINED ZR_BENCHMARK_SERVER_EXE OR ZR_BENCHMARK_SERVER_EXE STREQUAL "")
        message(FATAL_ERROR "ZR_VM_PERF_SCOPE=steady requires the built zr_vm_zr_benchmark_server target.")
    endif ()
    file(TO_CMAKE_PATH "${ZR_BENCHMARK_SERVER_EXE}" ZR_BENCHMARK_SERVER_EXE)
    if (NOT EXISTS "${ZR_BENCHMARK_SERVER_EXE}")
        message(FATAL_ERROR "ZR_VM_PERF_SCOPE=steady requires an existing ZR benchmark server: ${ZR_BENCHMARK_SERVER_EXE}")
    endif ()
endif ()

set(PERF_REQUESTED_WARMUP "")
if (DEFINED ENV{ZR_VM_PERF_WARMUP} AND NOT "$ENV{ZR_VM_PERF_WARMUP}" STREQUAL "")
    set(PERF_REQUESTED_WARMUP "$ENV{ZR_VM_PERF_WARMUP}")
endif ()

set(PERF_REQUESTED_ITERATIONS "")
if (DEFINED ENV{ZR_VM_PERF_ITERATIONS} AND NOT "$ENV{ZR_VM_PERF_ITERATIONS}" STREQUAL "")
    set(PERF_REQUESTED_ITERATIONS "$ENV{ZR_VM_PERF_ITERATIONS}")
endif ()

zr_benchmark_task3_resolve_policy(
        "${PERF_SCOPE_MODE}"
        "${PERF_REQUESTED_TIER}"
        "${PERF_DEFAULT_WARMUP}"
        "${PERF_DEFAULT_ITERATIONS}"
        "${PERF_REQUESTED_WARMUP}"
        "${PERF_REQUESTED_ITERATIONS}"
        PERF_WARMUP
        PERF_ITERATIONS
        PERF_MAX_EXTRA_SAMPLES
        PERF_PROFILE_MODE
        PERF_MINIMUM_SAMPLE_MODE)

set(PERF_EXECUTION_SEED 0)
if (DEFINED ENV{ZR_VM_PERF_SEED} AND NOT "$ENV{ZR_VM_PERF_SEED}" STREQUAL "")
    set(PERF_EXECUTION_SEED "$ENV{ZR_VM_PERF_SEED}")
endif ()
zr_benchmark_task3_validate_seed("${PERF_EXECUTION_SEED}" PERF_EXECUTION_SEED_VALID)
if (NOT PERF_EXECUTION_SEED_VALID)
    message(FATAL_ERROR
            "Invalid ZR_VM_PERF_SEED: ${PERF_EXECUTION_SEED}; expected an unsigned 64-bit decimal integer")
endif ()
set(PERF_BOOTSTRAP_SEED "${PERF_EXECUTION_SEED}")

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

# Optional: ZR_VM_PERF_ONLY_IMPLEMENTATIONS=comma-separated ids (e.g. zr_interp,zr_binary) to run a subset for diagnosis.
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

if (PERF_SCOPE_MODE STREQUAL "steady")
    set(PERF_SUITE_ROOT "${GENERATED_DIR}/performance_suite_steady")
    set(PERF_REPORT_DIR "${GENERATED_DIR}/performance_steady")
else ()
    set(PERF_SUITE_ROOT "${GENERATED_DIR}/performance_suite")
    set(PERF_REPORT_DIR "${GENERATED_DIR}/performance")
endif ()
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

set(PERF_TASK4_ENVIRONMENT_REPORT "$ENV{ZR_VM_BENCHMARK_ENVIRONMENT_REPORT}")
zr_benchmark_task4_resolve_environment(
        "${CMAKE_SYSTEM_NAME}"
        "${PERF_TASK4_ENVIRONMENT_REPORT}"
        PERF_TASK4_ENVIRONMENT_VALID
        PERF_TASK4_PROVISIONAL_COMPARABLE
        PERF_TASK4_ENVIRONMENT_JSON
        PERF_TASK4_ENVIRONMENT_ISSUE)
if (NOT PERF_TASK4_ENVIRONMENT_VALID AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "Task4 environment evidence is unavailable: ${PERF_TASK4_ENVIRONMENT_ISSUE}")
endif ()

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

function(perf_decimal_to_milli value out_var)
    string(REGEX MATCH "^([0-9]+)(\\.([0-9]+))?$" matched "${value}")
    if (matched STREQUAL "")
        message(FATAL_ERROR "Expected a non-negative decimal, got: ${value}")
    endif ()
    set(whole "${CMAKE_MATCH_1}")
    set(fraction "${CMAKE_MATCH_3}")
    string(APPEND fraction "0000")
    string(SUBSTRING "${fraction}" 0 3 fraction_milli)
    string(SUBSTRING "${fraction}" 3 1 round_digit)
    math(EXPR milli "${whole} * 1000 + ${fraction_milli}")
    if (round_digit GREATER_EQUAL 5)
        math(EXPR milli "${milli} + 1")
    endif ()
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

function(perf_bytes_to_mib byte_value out_var)
    if (byte_value STREQUAL "")
        set(${out_var} "-" PARENT_SCOPE)
        return()
    endif ()
    if (NOT byte_value MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Expected byte count as an unsigned integer, got: ${byte_value}")
    endif ()
    math(EXPR mib_milli "((${byte_value} * 1000) + 524288) / 1048576")
    perf_format_milli_decimal("${mib_milli}" mib_text)
    set(${out_var} "${mib_text}" PARENT_SCOPE)
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
            "pub fn scale(): int {\n"
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
perf_probe_program("${PERF_VALGRIND_EXE_CANDIDATE}" PERF_VALGRIND_EXE --version)
perf_probe_program("${PERF_CALLGRIND_ANNOTATE_EXE_CANDIDATE}" PERF_CALLGRIND_ANNOTATE_EXE --version)

set(PERF_LUA_IS_LUAJIT FALSE)
if (PERF_LUA_EXE)
    get_filename_component(perf_lua_executable_name "${PERF_LUA_EXE}" NAME)
    string(TOLOWER "${perf_lua_executable_name}" perf_lua_executable_name)
    if (perf_lua_executable_name MATCHES "luajit")
        set(PERF_LUA_IS_LUAJIT TRUE)
    endif ()
endif ()

if (NOT PERF_PYTHON_EXE)
    message(FATAL_ERROR "The performance suite requires Python for deterministic execution planning.")
endif ()
set(PERF_EXECUTION_PLAN_SCRIPT
        "${CMAKE_CURRENT_LIST_DIR}/../../scripts/benchmark/benchmark_execution_plan.py")
file(TO_CMAKE_PATH "${PERF_EXECUTION_PLAN_SCRIPT}" PERF_EXECUTION_PLAN_SCRIPT)

set(PERF_CANDIDATE_JOBS_JSON "[")
set(PERF_CANDIDATE_JOB_NEEDS_COMMA FALSE)
foreach (candidate_case IN LISTS ZR_VM_BENCHMARK_CASE_NAMES)
    perf_case_matches_tier("${candidate_case}" candidate_case_enabled)
    if (NOT candidate_case_enabled)
        continue()
    endif ()
    foreach (candidate_implementation IN LISTS ZR_VM_BENCHMARK_IMPLEMENTATIONS_${candidate_case})
        perf_escape_json_string("${candidate_case}" candidate_case_json)
        perf_escape_json_string("${candidate_implementation}" candidate_implementation_json)
        if (PERF_CANDIDATE_JOB_NEEDS_COMMA)
            string(APPEND PERF_CANDIDATE_JOBS_JSON ",")
        endif ()
        string(APPEND PERF_CANDIDATE_JOBS_JSON
                "{\"case\":\"${candidate_case_json}\",\"implementation\":\"${candidate_implementation_json}\"}")
        set(PERF_CANDIDATE_JOB_NEEDS_COMMA TRUE)
    endforeach ()
endforeach ()
string(APPEND PERF_CANDIDATE_JOBS_JSON "]")

set(PERF_CASE_FILTER_JSON null)
if (PERF_ONLY_CASES_FILTER_ACTIVE)
    perf_json_array_from_list(PERF_CASE_FILTER_JSON ${PERF_ONLY_CASE_LIST})
endif ()
set(PERF_IMPLEMENTATION_FILTER_JSON null)
if (PERF_ONLY_FILTER_ACTIVE)
    perf_json_array_from_list(PERF_IMPLEMENTATION_FILTER_JSON ${PERF_ONLY_IMPLEMENTATION_LIST})
endif ()
set(PERF_EXECUTION_PLAN_PATH "${PERF_REPORT_DIR}/execution_plan.json")
zr_benchmark_task3_create_execution_plan(
        "${PERF_PYTHON_EXE}"
        "${PERF_EXECUTION_PLAN_SCRIPT}"
        "${PERF_CANDIDATE_JOBS_JSON}"
        "${PERF_EXECUTION_SEED}"
        "${PERF_CASE_FILTER_JSON}"
        "${PERF_IMPLEMENTATION_FILTER_JSON}"
        "${PERF_EXECUTION_PLAN_PATH}"
        PERF_EXECUTION_PLAN_JSON)
string(JSON PERF_EXECUTION_JOB_COUNT GET "${PERF_EXECUTION_PLAN_JSON}" job_count)
math(EXPR PERF_EXECUTION_LAST_JOB "${PERF_EXECUTION_JOB_COUNT} - 1")
set(PERF_CASE_ORDER "")
foreach (plan_index RANGE 0 ${PERF_EXECUTION_LAST_JOB})
    string(JSON plan_case GET "${PERF_EXECUTION_PLAN_JSON}" jobs ${plan_index} case)
    string(JSON plan_implementation GET "${PERF_EXECUTION_PLAN_JSON}" jobs ${plan_index} implementation)
    list(FIND PERF_CASE_ORDER "${plan_case}" plan_case_index)
    if (plan_case_index LESS 0)
        list(APPEND PERF_CASE_ORDER "${plan_case}")
    endif ()
    list(APPEND "PERF_PLANNED_IMPLEMENTATIONS_${plan_case}" "${plan_implementation}")
endforeach ()
list(LENGTH PERF_CASE_ORDER PERF_CASE_COUNT)
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

foreach (PERF_EXECUTION_INDEX RANGE 0 ${PERF_EXECUTION_LAST_JOB})
    string(JSON case_name GET "${PERF_EXECUTION_PLAN_JSON}" jobs ${PERF_EXECUTION_INDEX} case)
    string(JSON implementation_id GET "${PERF_EXECUTION_PLAN_JSON}" jobs ${PERF_EXECUTION_INDEX} implementation)
    set(case_project_dir_var "PERF_CASE_PROJECT_DIR_${case_name}")
    set(case_project_file_var "PERF_CASE_PROJECT_FILE_${case_name}")
    if (NOT DEFINED ${case_project_dir_var})
        perf_prepare_zr_case("${case_name}" prepared_project_dir prepared_project_file)
        set("${case_project_dir_var}" "${prepared_project_dir}")
        set("${case_project_file_var}" "${prepared_project_file}")
    endif ()
    set(zr_project_dir "${${case_project_dir_var}}")
    set(zr_project_file "${${case_project_file_var}}")
    perf_case_scale("${case_name}" case_scale)

    set(case_description "${ZR_VM_BENCHMARK_DESCRIPTION_${case_name}}")
    set(case_banner "${ZR_VM_BENCHMARK_PASS_BANNER_${case_name}}")
    set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_${PERF_REQUESTED_TIER}}")
    if (case_checksum STREQUAL "")
        set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_core}")
    endif ()
    set(case_expected_output "${case_banner}\n${case_checksum}")
    if (PERF_PROFILE_MODE)
        set(case_min_sample_ms 0)
    else ()
        set(case_min_sample_ms "${ZR_VM_BENCHMARK_MIN_SAMPLE_MS_${case_name}}")
        if (NOT case_min_sample_ms MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "Benchmark case '${case_name}' has invalid MIN_SAMPLE_MS: ${case_min_sample_ms}")
        endif ()
    endif ()
    set(case_profile_report_path "")
    set(case_interp_command_list "")
    set(case_interp_working_directory "")
    set(case_interp_ready FALSE)

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
        set(perf_json_text "")
        set(relative_to_c "null")
        set(mean_wall_ms "")
        set(median_wall_ms "")
        set(min_wall_ms "")
        set(max_wall_ms "")
        set(stddev_wall_ms "")
        set(mad_wall_ms "")
        set(coefficient_of_variation "")
        set(bootstrap_low "")
        set(bootstrap_high "")
        set(sample_count "")
        set(extra_sample_count "")
        set(repetitions "")
        set(stability "")
        set(comparable FALSE)
        set(gate_eligible FALSE)
        set(calibration_enabled FALSE)
        set(calibration_aggregate_wall_ms "")
        set(mean_peak_mib "")
        set(max_peak_mib "")

        zr_benchmark_measurement_contract_get(
                "${implementation_id}"
                measurement_scope
                prepare_scope
                runtime_reused
                compiler_reused
                jit_state_reused)
        zr_benchmark_measurement_contract_validate(
                "${measurement_scope}"
                "${prepare_scope}"
                "${runtime_reused}"
                "${compiler_reused}"
                "${jit_state_reused}"
                measurement_contract_valid)
        if (NOT measurement_contract_valid)
            message(FATAL_ERROR "Invalid benchmark measurement contract for implementation '${implementation_id}'.")
        endif ()

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
                    if (NOT implementation_is_core_gated)
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
                set(measurement_command_list ${command_list})
                set(measurement_scope_for_runner "${measurement_scope}")
                set(prepare_scope_for_runner "${prepare_scope}")
                set(runtime_reused_for_runner "${runtime_reused}")
                set(compiler_reused_for_runner "${compiler_reused}")
                set(jit_state_reused_for_runner "${jit_state_reused}")
                set(persistent_runner_args "")
                if (PERF_SCOPE_MODE STREQUAL "steady")
                    zr_benchmark_persistent_command_get(
                            "${implementation_id}"
                            "${case_name}"
                            "${PERF_REQUESTED_TIER}"
                            persistent_supported
                            measurement_command_list
                            persistent_prepare_scope
                            persistent_runtime_reused
                            persistent_compiler_reused
                            persistent_jit_state_reused)
                    if (persistent_supported)
                        set(measurement_scope_for_runner "persistent_runtime")
                        set(prepare_scope_for_runner "${persistent_prepare_scope}")
                        set(runtime_reused_for_runner "${persistent_runtime_reused}")
                        set(compiler_reused_for_runner "${persistent_compiler_reused}")
                        set(jit_state_reused_for_runner "${persistent_jit_state_reused}")
                        if (implementation_id STREQUAL "dotnet")
                            set(dotnet_calibration_enabled FALSE)
                            if (case_min_sample_ms GREATER 0)
                                set(dotnet_calibration_enabled TRUE)
                            endif ()
                            zr_benchmark_task3_dotnet_jit_state_reused(
                                    TRUE
                                    "${PERF_WARMUP}"
                                    "${dotnet_calibration_enabled}"
                                    jit_state_reused_for_runner)
                        endif ()
                        set(persistent_runner_args
                                --persistent
                                --checksum-contract "benchmark-checksum-v1:${case_name}:${PERF_REQUESTED_TIER}"
                                --expected-checksum "${case_checksum}"
                                --ready-timeout-ms 5000
                                --request-timeout-ms 60000
                                --stop-timeout-ms 5000)
                    else ()
                        set(status "SKIP")
                        set(note "steady scope unavailable for this implementation")
                        perf_append_note("skip" "${case_name}" "${implementation_name}" "${note}")
                    endif ()
                endif ()
                set(measurement_scope "${measurement_scope_for_runner}")
                set(prepare_scope "${prepare_scope_for_runner}")
                set(runtime_reused "${runtime_reused_for_runner}")
                set(compiler_reused "${compiler_reused_for_runner}")
                set(jit_state_reused "${jit_state_reused_for_runner}")
            endif ()

            if (NOT status STREQUAL "FAIL" AND NOT status STREQUAL "SKIP")
                set(perf_json_path "${PERF_REPORT_DIR}/${case_name}__${implementation_id}.json")
                set(measurement_policy_runner_args
                        --max-extra-samples "${PERF_MAX_EXTRA_SAMPLES}"
                        --bootstrap-seed "${PERF_BOOTSTRAP_SEED}")
                if (PERF_PROFILE_MODE)
                    list(APPEND measurement_policy_runner_args --profile)
                else ()
                    list(APPEND measurement_policy_runner_args --min-sample-ms "${case_min_sample_ms}")
                endif ()
                if (profile_report_path STREQUAL "")
                    execute_process(
                            COMMAND
                            "${PERF_RUNNER_EXE}"
                            "--name" "${implementation_name}"
                            "--iterations" "${PERF_ITERATIONS}"
                            "--warmup" "${PERF_WARMUP}"
                            "--json-out" "${perf_json_path}"
                            "--working-directory" "${working_directory}"
                            "--measurement-scope" "${measurement_scope_for_runner}"
                            "--prepare-scope" "${prepare_scope_for_runner}"
                            "--runtime-reused" "${runtime_reused_for_runner}"
                            "--compiler-reused" "${compiler_reused_for_runner}"
                            "--jit-state-reused" "${jit_state_reused_for_runner}"
                            ${measurement_policy_runner_args}
                            ${persistent_runner_args}
                            "--"
                            ${measurement_command_list}
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
                            "--measurement-scope" "${measurement_scope_for_runner}"
                            "--prepare-scope" "${prepare_scope_for_runner}"
                            "--runtime-reused" "${runtime_reused_for_runner}"
                            "--compiler-reused" "${compiler_reused_for_runner}"
                            "--jit-state-reused" "${jit_state_reused_for_runner}"
                            ${measurement_policy_runner_args}
                            ${persistent_runner_args}
                            "--"
                            ${measurement_command_list}
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
                    if (NOT EXISTS "${perf_json_path}")
                        message(FATAL_ERROR
                                "Performance runner succeeded without writing its JSON report: ${perf_json_path}")
                    endif ()
                    set(status "PASS")
                    file(READ "${perf_json_path}" perf_json_text)
                    zr_benchmark_task3_parse_runner_report("${perf_json_text}" run)
                    set(mean_wall_ms "${run_mean_wall_ms}")
                    set(median_wall_ms "${run_median_wall_ms}")
                    set(min_wall_ms "${run_min_wall_ms}")
                    set(max_wall_ms "${run_max_wall_ms}")
                    set(stddev_wall_ms "${run_stddev_wall_ms}")
                    set(mad_wall_ms "${run_mad_wall_ms}")
                    set(coefficient_of_variation "${run_coefficient_of_variation}")
                    set(bootstrap_low "${run_bootstrap_low}")
                    set(bootstrap_high "${run_bootstrap_high}")
                    set(sample_count "${run_sample_count}")
                    set(extra_sample_count "${run_extra_sample_count}")
                    set(repetitions "${run_repetitions}")
                    set(stability "${run_stability}")
                    set(comparable "${run_comparable}")
                    set(gate_eligible "${run_gate_eligible}")
                    set(calibration_enabled "${run_calibration_enabled}")
                    set(calibration_aggregate_wall_ms "${run_calibration_aggregate_wall_ms}")
                    if (measurement_scope STREQUAL "persistent_runtime")
                        perf_bytes_to_mib("${run_persistent_peak_working_set_bytes}" max_peak_mib)
                        set(mean_peak_mib "-")
                    else ()
                        perf_bytes_to_mib("${run_mean_peak_working_set_bytes}" mean_peak_mib)
                        perf_bytes_to_mib("${run_max_peak_working_set_bytes}" max_peak_mib)
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

        set(result_prefix "PERF_RESULT_${case_name}_${implementation_id}")
        foreach (result_field IN ITEMS
                implementation_name language mode status note working_directory
                measurement_scope prepare_scope runtime_reused compiler_reused jit_state_reused
                mean_wall_ms median_wall_ms min_wall_ms max_wall_ms stddev_wall_ms mad_wall_ms
                coefficient_of_variation bootstrap_low bootstrap_high sample_count extra_sample_count
                repetitions stability comparable gate_eligible calibration_enabled calibration_aggregate_wall_ms
                mean_peak_mib max_peak_mib profile_report_path perf_json_text)
            set("${result_prefix}_${result_field}" "${${result_field}}")
        endforeach ()
        set("${result_prefix}_command_list" "${command_list}")
        set("${result_prefix}_interp_command_list" "${case_interp_command_list}")
        set("${result_prefix}_interp_working_directory" "${case_interp_working_directory}")
        set("${result_prefix}_interp_ready" "${case_interp_ready}")

        if (case_name STREQUAL PERF_GC_BASELINE_CASE OR case_name STREQUAL PERF_GC_STRESS_CASE)
            set("PERF_GC_IMPLEMENTATION_NAME_${implementation_id}" "${implementation_name}")
            set("PERF_GC_LANGUAGE_${implementation_id}" "${language}")
            set("PERF_GC_STATUS_${case_name}_${implementation_id}" "${status}")
            set("PERF_GC_NOTE_${case_name}_${implementation_id}" "${note}")
            if (status STREQUAL "PASS" AND comparable AND gate_eligible AND
                stability STREQUAL "STABLE")
                set("PERF_GC_MEAN_WALL_${case_name}_${implementation_id}" "${mean_wall_ms}")
                set("PERF_GC_MEAN_PEAK_${case_name}_${implementation_id}" "${mean_peak_mib}")
            else ()
                set("PERF_GC_MEAN_WALL_${case_name}_${implementation_id}" "")
                set("PERF_GC_MEAN_PEAK_${case_name}_${implementation_id}" "")
            endif ()
        endif ()
    endforeach ()

include("${CMAKE_CURRENT_LIST_DIR}/benchmark_task3_case_assembly.cmake")

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

if (PERF_SCOPE_MODE STREQUAL "steady")
    set(PERF_SCOPE_REPORT_LINE
            "- **Measurement scope:** Supported numeric/dispatch rows use `persistent_runtime`; one process serves all warmup and measured requests. Per-sample RSS is unavailable, and the final memory column is the session peak.\n")
    set(PERF_ZR_PREP_REPORT_LINE
            "- **ZR binary prepare:** A fresh untimed `zr_vm_cli --compile ...` precedes correctness and measurement; the persistent server then loads generated bytecode once. Compiler state is not reused.\n")
    set(PERF_MEAN_PEAK_COLUMN "per-sample peak MiB")
    set(PERF_MAX_PEAK_COLUMN "session peak MiB")
else ()
    set(PERF_SCOPE_REPORT_LINE
            "- **Measurement scope:** Rows use `process_end_to_end`; each warmup or sample starts a fresh child and all reuse flags are false.\n")
    set(PERF_ZR_PREP_REPORT_LINE
            "- **ZR binary prepare:** The suite runs a separate untimed one-shot `zr_vm_cli --compile ...`; samples include child CLI startup, bytecode load, runtime setup, and execution.\n")
    set(PERF_MEAN_PEAK_COLUMN "mean peak MiB")
    set(PERF_MAX_PEAK_COLUMN "max peak MiB")
endif ()

string(CONCAT PERF_MARKDOWN_REPORT
        "# ZR VM Performance Report\n\n"
        "- Generated At (UTC): ${PERF_GENERATED_AT_UTC}\n"
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Scale Policy: registry tier scale (profile uses per-case profile scale)\n"
        "- Warmup Iterations Per Implementation: ${PERF_WARMUP}\n"
        "- Initial Measured Samples Per Implementation: ${PERF_ITERATIONS}\n"
        "- Maximum Extra Samples: ${PERF_MAX_EXTRA_SAMPLES}\n"
        "- Execution Seed: ${PERF_EXECUTION_SEED} (`fisher_yates_splitmix64` v1)\n"
        "- Minimum Sample Policy: ${PERF_MINIMUM_SAMPLE_MODE}\n"
        "- Profile Wall-Time Comparable: false\n"
        "${PERF_CALLGRIND_DOC_LINE}"
        "${PERF_SCOPE_REPORT_LINE}"
        "${PERF_ZR_PREP_REPORT_LINE}"
        "- Benchmarks Root: `${BENCHMARKS_DIR}`\n"
        "- Cases: ${PERF_CASE_COUNT}\n\n"
        "| case | implementation | language | status | measurement scope | prepare scope | runtime reused | compiler reused | JIT state reused | mean wall ms | median wall ms | min wall ms | max wall ms | stddev wall ms | MAD ms | CV | bootstrap median 95% CI | samples (+extra) x reps | calibrated:aggregate ms | stability | comparable | gate eligible | ${PERF_MEAN_PEAK_COLUMN} | ${PERF_MAX_PEAK_COLUMN} | relative_to_c |\n"
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | --- | --- | --- | --- | ---: | ---: | ---: |\n"
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
        "  \"schema_version\": 3,\n"
        "  \"suite\": \"performance_report\",\n"
        "  \"generated_at_utc\": \"${PERF_GENERATED_AT_UTC}\",\n"
        "  \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
        "  \"scope_mode\": \"${PERF_SCOPE_MODE}\",\n"
        "  \"environment\": ${PERF_TASK4_ENVIRONMENT_JSON},\n"
        "  \"scale_policy\": \"tier_default_or_case_profile\",\n"
        "  \"warmup\": ${PERF_WARMUP},\n"
        "  \"iterations\": ${PERF_ITERATIONS},\n"
        "  \"execution_plan\": ${PERF_EXECUTION_PLAN_JSON},\n"
        "  \"measurement_policy\": {\n"
        "    \"profile\": ${PERF_PROFILE_MODE},\n"
        "    \"warmup\": ${PERF_WARMUP},\n"
        "    \"initial_sample_count\": ${PERF_ITERATIONS},\n"
        "    \"max_extra_sample_count\": ${PERF_MAX_EXTRA_SAMPLES},\n"
        "    \"minimum_sample_ms_source\": \"${PERF_MINIMUM_SAMPLE_MODE}\",\n"
        "    \"calibration_strategy\": \"power_of_two_repetition_doubling\",\n"
        "    \"stability\": {\"metric\": \"coefficient_of_variation\", \"maximum\": 0.05},\n"
        "    \"bootstrap\": {\"statistic\": \"median\", \"confidence\": 0.95, \"seed\": \"${PERF_BOOTSTRAP_SEED}\"},\n"
        "    \"profile_comparable\": false\n"
        "  },\n"
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
        "- Tier: ${PERF_REQUESTED_TIER}\n"
        "- Ratios are emitted only when both records have the same non-empty measurement scope.\n\n"
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
