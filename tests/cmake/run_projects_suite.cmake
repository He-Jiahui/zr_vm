if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED REPO_ROOT OR REPO_ROOT STREQUAL "")
    message(FATAL_ERROR "REPO_ROOT is required.")
endif()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

if (DEFINED TIER AND NOT TIER STREQUAL "")
    string(TOLOWER "${TIER}" PROJECT_REQUESTED_TIER)
elseif (DEFINED ENV{ZR_VM_TEST_TIER} AND NOT "$ENV{ZR_VM_TEST_TIER}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_TEST_TIER}" PROJECT_REQUESTED_TIER)
else ()
    set(PROJECT_REQUESTED_TIER "")
endif ()

set(COMMON_FAIL_REGEX "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error")
set(PROJECT_CASE_NAMES "")
set(PROJECT_FIXTURES_REL_PREFIX "tests/fixtures/projects/")

set(PROJECT_SOURCE_FIXTURES_DIR "${PROJECT_FIXTURES_DIR}")
set(PROJECT_SUITE_ROOT "${GENERATED_DIR}/projects_suite")
set(PROJECT_FIXTURES_DIR "${PROJECT_SUITE_ROOT}/fixtures")
file(REMOVE_RECURSE "${PROJECT_SUITE_ROOT}")
file(MAKE_DIRECTORY "${PROJECT_SUITE_ROOT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_FIXTURES_DIR}" "${PROJECT_FIXTURES_DIR}"
    RESULT_VARIABLE project_fixture_copy_result
    OUTPUT_VARIABLE project_fixture_copy_stdout
    ERROR_VARIABLE project_fixture_copy_stderr
)

if (NOT project_fixture_copy_stdout STREQUAL "")
    message("${project_fixture_copy_stdout}")
endif()

if (NOT project_fixture_copy_stderr STREQUAL "")
    message("${project_fixture_copy_stderr}")
endif()

if (NOT project_fixture_copy_result EQUAL 0)
    message(FATAL_ERROR "Failed while copying project fixtures into '${PROJECT_SUITE_ROOT}'.")
endif()

file(REMOVE_RECURSE "${PROJECT_FIXTURES_DIR}/.fixture_prepare")

set(native_numeric_pipeline_output_dir "${PROJECT_FIXTURES_DIR}/native_numeric_pipeline/bin/out")
file(TO_CMAKE_PATH "${native_numeric_pipeline_output_dir}" native_numeric_pipeline_output_dir)
set(native_numeric_pipeline_source_file "${PROJECT_FIXTURES_DIR}/native_numeric_pipeline/src/system_io.zr")
if (EXISTS "${native_numeric_pipeline_source_file}")
    file(READ "${native_numeric_pipeline_source_file}" native_numeric_pipeline_source_contents)
    string(REPLACE
            "tests/fixtures/projects/native_numeric_pipeline/bin/out"
            "${native_numeric_pipeline_output_dir}"
            native_numeric_pipeline_source_contents
            "${native_numeric_pipeline_source_contents}")
    file(WRITE "${native_numeric_pipeline_source_file}" "${native_numeric_pipeline_source_contents}")
endif()

file(GLOB native_numeric_pipeline_bytecode
        "${PROJECT_FIXTURES_DIR}/native_numeric_pipeline/bin/*.zro"
        "${PROJECT_FIXTURES_DIR}/native_numeric_pipeline/bin/*.zri")
if (NOT native_numeric_pipeline_bytecode STREQUAL "")
    file(REMOVE ${native_numeric_pipeline_bytecode})
endif()

file(REMOVE_RECURSE "${native_numeric_pipeline_output_dir}")
file(MAKE_DIRECTORY "${native_numeric_pipeline_output_dir}")

set(gc_fragment_stress_output_dir "${PROJECT_FIXTURES_DIR}/gc_fragment_stress/bin/out")
file(TO_CMAKE_PATH "${gc_fragment_stress_output_dir}" gc_fragment_stress_output_dir)
set(gc_fragment_stress_source_file "${PROJECT_FIXTURES_DIR}/gc_fragment_stress/src/main.zr")
if (EXISTS "${gc_fragment_stress_source_file}")
    file(READ "${gc_fragment_stress_source_file}" gc_fragment_stress_source_contents)
    string(REPLACE
            "tests/fixtures/projects/gc_fragment_stress/bin/out"
            "${gc_fragment_stress_output_dir}"
            gc_fragment_stress_source_contents
            "${gc_fragment_stress_source_contents}")
    file(WRITE "${gc_fragment_stress_source_file}" "${gc_fragment_stress_source_contents}")
endif()

file(GLOB gc_fragment_stress_bytecode
        "${PROJECT_FIXTURES_DIR}/gc_fragment_stress/bin/*.zro"
        "${PROJECT_FIXTURES_DIR}/gc_fragment_stress/bin/*.zri")
if (NOT gc_fragment_stress_bytecode STREQUAL "")
    file(REMOVE ${gc_fragment_stress_bytecode})
endif()

file(REMOVE_RECURSE "${gc_fragment_stress_output_dir}")
file(MAKE_DIRECTORY "${gc_fragment_stress_output_dir}")

function(register_project_case name project_rel pass_regex fail_regex prepare_source_rel prepare_output_rel required_file_rel required_file_regex)
    list(APPEND PROJECT_CASE_NAMES "${name}")
    set(PROJECT_CASE_NAMES "${PROJECT_CASE_NAMES}" PARENT_SCOPE)
    set("PROJECT_CASE_PROJECT_${name}" "${project_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_PASS_${name}" "${pass_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_FAIL_${name}" "${fail_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_SOURCE_${name}" "${prepare_source_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_OUTPUT_${name}" "${prepare_output_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRED_FILE_${name}" "${required_file_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRED_FILE_REGEX_${name}" "${required_file_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_TIERS_${name}" "core" PARENT_SCOPE)
    set("PROJECT_CASE_EXECUTED_VIA_REGEX_${name}" "" PARENT_SCOPE)
    set("PROJECT_CASE_COMPILE_ARGS_${name}" "" PARENT_SCOPE)
    set("PROJECT_CASE_RUN_ARGS_${name}" "" PARENT_SCOPE)
endfunction()

function(set_project_case_metadata name tiers executed_via_regex)
    set("PROJECT_CASE_TIERS_${name}" "${tiers}" PARENT_SCOPE)
    set("PROJECT_CASE_EXECUTED_VIA_REGEX_${name}" "${executed_via_regex}" PARENT_SCOPE)
endfunction()

function(set_project_case_execution name compile_args run_args)
    set("PROJECT_CASE_COMPILE_ARGS_${name}" "${compile_args}" PARENT_SCOPE)
    set("PROJECT_CASE_RUN_ARGS_${name}" "${run_args}" PARENT_SCOPE)
endfunction()

function(project_case_matches_tier tiers out_var)
    if (PROJECT_REQUESTED_TIER STREQUAL "")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    set(case_tiers "${tiers}")
    if (case_tiers STREQUAL "")
        set(case_tiers "core")
    endif()

    list(FIND case_tiers "${PROJECT_REQUESTED_TIER}" tier_index)
    if (tier_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

function(project_output_matches_pass_regex project_output pass_regex out_var)
    if ("${project_output}" MATCHES "${pass_regex}")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    set(normalized_output "${project_output}")
    set(normalized_regex "${pass_regex}")
    string(REPLACE "\r\n" "__ZR_NL__" normalized_output "${normalized_output}")
    string(REPLACE "\n" "__ZR_NL__" normalized_output "${normalized_output}")
    string(REPLACE "\r" "__ZR_NL__" normalized_output "${normalized_output}")

    string(REPLACE "\r" "" normalized_regex "${normalized_regex}")
    string(REPLACE "\n" "" normalized_regex "${normalized_regex}")
    string(REPLACE "[\\r\\n]+" "__ZR_NL__" normalized_regex "${normalized_regex}")
    string(REPLACE "[\\n]+" "__ZR_NL__" normalized_regex "${normalized_regex}")
    string(REPLACE "[\\r]+" "__ZR_NL__" normalized_regex "${normalized_regex}")

    if ("${normalized_output}" MATCHES "${normalized_regex}")
        set(${out_var} TRUE PARENT_SCOPE)
    else ()
        set(${out_var} FALSE PARENT_SCOPE)
    endif ()
endfunction()

function(project_case_resolve_required_file required_file_rel out_var)
    if (required_file_rel STREQUAL "")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    string(FIND "${required_file_rel}" "${PROJECT_FIXTURES_REL_PREFIX}" fixtures_prefix_index)
    if (fixtures_prefix_index EQUAL 0)
        string(LENGTH "${PROJECT_FIXTURES_REL_PREFIX}" fixtures_prefix_length)
        string(SUBSTRING "${required_file_rel}" "${fixtures_prefix_length}" -1 required_fixture_rel)
        set(${out_var} "${PROJECT_FIXTURES_DIR}/${required_fixture_rel}" PARENT_SCOPE)
        return()
    endif()

    set(${out_var} "${REPO_ROOT}/${required_file_rel}" PARENT_SCOPE)
endfunction()

function(project_run_args_request_binary run_args out_var)
    set(args ${run_args})
    list(FIND args "--execution-mode" execution_mode_index)
    list(FIND args "binary" binary_index)
    if (execution_mode_index EQUAL -1 OR binary_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

function(run_fixture_prepare case_name prepare_source_rel prepare_output_rel)
    set(prepare_source "${PROJECT_FIXTURES_DIR}/${prepare_source_rel}")
    set(prepare_output "${PROJECT_FIXTURES_DIR}/${prepare_output_rel}")
    get_filename_component(module_name "${prepare_output}" NAME_WE)
    get_filename_component(prepare_output_ext "${prepare_output}" EXT)
    get_filename_component(prepare_output_dir "${prepare_output}" DIRECTORY)
    set(temp_project_root "${PROJECT_FIXTURES_DIR}/.fixture_prepare/${case_name}")
    set(temp_source_dir "${temp_project_root}/src")
    set(temp_binary_dir "${temp_project_root}/bin")
    set(temp_project_file "${temp_project_root}/${case_name}.zrp")
    set(temp_source_file "${temp_source_dir}/${module_name}.zr")
    set(temp_compiled_output "${temp_binary_dir}/${module_name}${prepare_output_ext}")
    set(compile_command "${CLI_EXE}" "--compile" "${temp_project_file}")
    set(copy_all_outputs OFF)

    if (prepare_output_ext STREQUAL ".zri")
        list(APPEND compile_command "--intermediate")
    endif()

    file(REMOVE_RECURSE "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_source_dir}")
    file(MAKE_DIRECTORY "${temp_binary_dir}")
    file(MAKE_DIRECTORY "${prepare_output_dir}")

    if (IS_DIRECTORY "${prepare_source}")
        set(copy_all_outputs ON)
        file(REMOVE_RECURSE "${temp_source_dir}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${prepare_source}" "${temp_source_dir}"
            RESULT_VARIABLE copy_source_result
            OUTPUT_VARIABLE copy_source_stdout
            ERROR_VARIABLE copy_source_stderr
        )
        if (NOT copy_source_stdout STREQUAL "")
            message("${copy_source_stdout}")
        endif()
        if (NOT copy_source_stderr STREQUAL "")
            message("${copy_source_stderr}")
        endif()
        if (NOT copy_source_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying fixture directory '${prepare_source_rel}'.")
        endif()
    else ()
        file(COPY_FILE "${prepare_source}" "${temp_source_file}" ONLY_IF_DIFFERENT)
    endif()

    file(WRITE "${temp_project_file}"
            "{\n"
            "  \"name\": \"${case_name}_fixture\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"${module_name}\"\n"
            "}\n")

    execute_process(
        COMMAND ${compile_command}
        WORKING_DIRECTORY "${REPO_ROOT}"
        RESULT_VARIABLE prepare_result
        OUTPUT_VARIABLE prepare_stdout
        ERROR_VARIABLE prepare_stderr
    )
    if (NOT prepare_stdout STREQUAL "")
        message("${prepare_stdout}")
    endif()
    if (NOT prepare_stderr STREQUAL "")
        message("${prepare_stderr}")
    endif()
    if (NOT prepare_result EQUAL 0)
        file(REMOVE_RECURSE "${temp_project_root}")
        message(FATAL_ERROR "Project case '${case_name}' failed while preparing fixture '${prepare_source_rel}'.")
    endif()
    if (NOT EXISTS "${temp_compiled_output}")
        file(REMOVE_RECURSE "${temp_project_root}")
        message(FATAL_ERROR "Project case '${case_name}' did not produce fixture output '${temp_compiled_output}'.")
    endif()

    if (copy_all_outputs)
        file(GLOB prepared_outputs "${temp_binary_dir}/*${prepare_output_ext}")
        foreach(prepared_output IN LISTS prepared_outputs)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${prepared_output}" "${prepare_output_dir}"
                RESULT_VARIABLE copy_result
                OUTPUT_VARIABLE copy_stdout
                ERROR_VARIABLE copy_stderr
            )
            if (NOT copy_stdout STREQUAL "")
                message("${copy_stdout}")
            endif()
            if (NOT copy_stderr STREQUAL "")
                message("${copy_stderr}")
            endif()
            if (NOT copy_result EQUAL 0)
                file(REMOVE_RECURSE "${temp_project_root}")
                message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared fixture set '${prepare_source_rel}'.")
            endif()
        endforeach()
    else ()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_compiled_output}" "${prepare_output}"
            RESULT_VARIABLE copy_result
            OUTPUT_VARIABLE copy_stdout
            ERROR_VARIABLE copy_stderr
        )
        if (NOT copy_stdout STREQUAL "")
            message("${copy_stdout}")
        endif()
        if (NOT copy_stderr STREQUAL "")
            message("${copy_stderr}")
        endif()
        if (NOT copy_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared fixture '${prepare_output_rel}'.")
        endif()
    endif()

    file(REMOVE_RECURSE "${temp_project_root}")
endfunction()

function(run_project_case case_name)
    set(project_rel "${PROJECT_CASE_PROJECT_${case_name}}")
    set(pass_regex "${PROJECT_CASE_PASS_${case_name}}")
    set(fail_regex "${PROJECT_CASE_FAIL_${case_name}}")
    set(prepare_source_rel "${PROJECT_CASE_PREPARE_SOURCE_${case_name}}")
    set(prepare_output_rel "${PROJECT_CASE_PREPARE_OUTPUT_${case_name}}")
    set(required_file_rel "${PROJECT_CASE_REQUIRED_FILE_${case_name}}")
    set(required_file_regex "${PROJECT_CASE_REQUIRED_FILE_REGEX_${case_name}}")
    set(case_tiers "${PROJECT_CASE_TIERS_${case_name}}")
    set(executed_via_regex "${PROJECT_CASE_EXECUTED_VIA_REGEX_${case_name}}")
    set(compile_args "${PROJECT_CASE_COMPILE_ARGS_${case_name}}")
    set(run_args "${PROJECT_CASE_RUN_ARGS_${case_name}}")
    set(project_file "${PROJECT_FIXTURES_DIR}/${project_rel}")

    project_case_matches_tier("${case_tiers}" run_case)
    if (NOT run_case)
        return()
    endif()

    message("---- ${case_name}")

    if (NOT prepare_source_rel STREQUAL "" AND NOT prepare_output_rel STREQUAL "")
        run_fixture_prepare("${case_name}" "${prepare_source_rel}" "${prepare_output_rel}")
    endif()

    if (NOT required_file_rel STREQUAL "")
        project_case_resolve_required_file("${required_file_rel}" required_file)
        file(REMOVE "${required_file}")
    endif()

    set(compile_requested FALSE)
    if (NOT compile_args STREQUAL "")
        set(compile_command "${CLI_EXE}" "--compile" "${project_file}")
        list(APPEND compile_command ${compile_args})
        execute_process(
            COMMAND ${compile_command}
            WORKING_DIRECTORY "${REPO_ROOT}"
            RESULT_VARIABLE compile_result
            OUTPUT_VARIABLE compile_stdout
            ERROR_VARIABLE compile_stderr
        )
        if (NOT compile_stdout STREQUAL "")
            message("${compile_stdout}")
        endif()
        if (NOT compile_stderr STREQUAL "")
            message("${compile_stderr}")
        endif()
        if (NOT compile_result EQUAL 0)
            message(FATAL_ERROR "Project case '${case_name}' failed during compile step.")
        endif()
        set(compile_requested TRUE)
    endif()

    project_run_args_request_binary("${run_args}" run_requires_binary)
    if (run_requires_binary AND NOT compile_requested)
        execute_process(
            COMMAND "${CLI_EXE}" "--compile" "${project_file}"
            WORKING_DIRECTORY "${REPO_ROOT}"
            RESULT_VARIABLE binary_prepare_result
            OUTPUT_VARIABLE binary_prepare_stdout
            ERROR_VARIABLE binary_prepare_stderr
        )
        if (NOT binary_prepare_stdout STREQUAL "")
            message("${binary_prepare_stdout}")
        endif()
        if (NOT binary_prepare_stderr STREQUAL "")
            message("${binary_prepare_stderr}")
        endif()
        if (NOT binary_prepare_result EQUAL 0)
            message(FATAL_ERROR "Project case '${case_name}' failed while preparing binary artifacts.")
        endif()
    endif()

    set(run_command "${CLI_EXE}" "${project_file}")
    if (NOT run_args STREQUAL "")
        list(APPEND run_command ${run_args})
    endif()

    execute_process(
        COMMAND ${run_command}
        WORKING_DIRECTORY "${REPO_ROOT}"
        RESULT_VARIABLE project_result
        OUTPUT_VARIABLE project_stdout
        ERROR_VARIABLE project_stderr
    )

    if (NOT project_stdout STREQUAL "")
        message("${project_stdout}")
    endif()
    if (NOT project_stderr STREQUAL "")
        message("${project_stderr}")
    endif()
    if (NOT project_result EQUAL 0)
        message(FATAL_ERROR "Project case '${case_name}' failed with exit code ${project_result}.")
    endif()

    set(project_output "${project_stdout}${project_stderr}")
    if (NOT fail_regex STREQUAL "" AND project_output MATCHES "${fail_regex}")
        message(FATAL_ERROR "Project case '${case_name}' matched fail regex '${fail_regex}'.\nOutput:\n${project_output}")
    endif()

    project_output_matches_pass_regex("${project_output}" "${pass_regex}" project_passed)
    if (NOT project_passed)
        message(FATAL_ERROR "Project case '${case_name}' did not match pass regex '${pass_regex}'.\nOutput:\n${project_output}")
    endif()

    if (NOT executed_via_regex STREQUAL "" AND NOT project_output MATCHES "${executed_via_regex}")
        message(FATAL_ERROR "Project case '${case_name}' did not match executed_via regex '${executed_via_regex}'.\nOutput:\n${project_output}")
    endif()

    if (NOT required_file_rel STREQUAL "")
        project_case_resolve_required_file("${required_file_rel}" required_file)
        if (NOT EXISTS "${required_file}")
            message(FATAL_ERROR "Project case '${case_name}' did not create required file '${required_file_rel}'.")
        endif()
        if (NOT required_file_regex STREQUAL "")
            file(READ "${required_file}" required_file_contents)
            if (NOT required_file_contents MATCHES "${required_file_regex}")
                message(FATAL_ERROR "Project case '${case_name}' created '${required_file_rel}', but contents did not match '${required_file_regex}'.")
            endif()
        endif()
    endif()
endfunction()

register_project_case("hello_world" "hello_world/hello_world.zrp" "hello world" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("import_basic" "import_basic/import_basic.zrp" "hello from import" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_import" "decorator_import/decorator_import.zrp" "31" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_compile_time" "decorator_compile_time/decorator_compile_time.zrp" "31" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_compile_time_import" "decorator_compile_time_import/decorator_compile_time_import.zrp" "31" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_compile_time_import_binary" "decorator_compile_time_import_binary/decorator_compile_time_import_binary.zrp" "62" "${COMMON_FAIL_REGEX}" "decorator_compile_time_import_binary/fixtures/provider_module" "decorator_compile_time_import_binary/bin/provider.zro" "" "")
register_project_case("decorator_compile_time_parameter_import" "decorator_compile_time_parameter_import/decorator_compile_time_parameter_import.zrp" "62" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_compile_time_parameter_import_binary" "decorator_compile_time_parameter_import_binary/decorator_compile_time_parameter_import_binary.zrp" "62" "${COMMON_FAIL_REGEX}" "decorator_compile_time_parameter_import_binary/fixtures/provider_module" "decorator_compile_time_parameter_import_binary/bin/provider.zro" "" "")
register_project_case("decorator_compile_time_deep_import" "decorator_compile_time_deep_import/decorator_compile_time_deep_import.zrp" "43" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("decorator_compile_time_provider_import" "decorator_compile_time_provider_import/decorator_compile_time_provider_import.zrp" "71" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes" "classes/classes.zrp" "61" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_full" "classes_full/classes_full.zrp" "127" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_static" "classes_static/classes_static.zrp" "82" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_super" "classes_super/classes_super.zrp" "42" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_properties" "classes_properties/classes_properties.zrp" "40" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("import_binary" "import_binary/import_binary.zrp" "hello from import" "${COMMON_FAIL_REGEX}" "import_binary/fixtures/greet_binary_source.zr" "import_binary/bin/greet.zro" "" "")
register_project_case("import_binary_const" "import_binary_const/import_binary_const.zrp" "7" "${COMMON_FAIL_REGEX}" "import_binary_const/fixtures/greet_binary_source.zr" "import_binary_const/bin/greet.zro" "" "")
register_project_case("decorator_import_binary" "decorator_import_binary/decorator_import_binary.zrp" "31" "${COMMON_FAIL_REGEX}" "decorator_import_binary/fixtures/decorated_user_module" "decorator_import_binary/bin/decorated_user.zro" "" "")
register_project_case("decorator_compile_time_binary" "decorator_compile_time_binary/decorator_compile_time_binary.zrp" "31" "${COMMON_FAIL_REGEX}" "decorator_compile_time_binary/fixtures/decorated_user_module" "decorator_compile_time_binary/bin/decorated_user.zro" "" "")
register_project_case("decorator_compile_time_deep_import_binary" "decorator_compile_time_deep_import_binary/decorator_compile_time_deep_import_binary.zrp" "43" "${COMMON_FAIL_REGEX}" "decorator_compile_time_deep_import_binary/fixtures/decorated_user_module" "decorator_compile_time_deep_import_binary/bin/decorated_user.zro" "" "")
register_project_case("decorator_compile_time_provider_import_binary" "decorator_compile_time_provider_import_binary/decorator_compile_time_provider_import_binary.zrp" "71" "${COMMON_FAIL_REGEX}" "decorator_compile_time_provider_import_binary/fixtures/decorated_user_module" "decorator_compile_time_provider_import_binary/bin/decorated_user.zro" "" "")
register_project_case("import_pub_function" "import_pub_function/import_pub_function.zrp" "7123" "${COMMON_FAIL_REGEX}|null" "" "" "" "")
register_project_case("import_capture_native" "import_capture_native/import_capture_native.zrp" "7005" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("import_capture_vector3" "import_capture_vector3/import_capture_vector3.zrp" "5" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("native_chain_calls" "native_chain_calls/native_chain_calls.zrp" "5008" "${COMMON_FAIL_REGEX}|null" "" "" "" "")
register_project_case("native_vector3_export_probe" "native_vector3_export_probe/native_vector3_export_probe.zrp" "1" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("native_vector3_capture_probe" "native_vector3_capture_probe/native_vector3_capture_probe.zrp" "6" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("native_math_export_probe" "native_math_export_probe/native_math_export_probe.zrp" "9" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("native_complex_capture_probe" "native_complex_capture_probe/native_complex_capture_probe.zrp" "18" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("native_numeric_pipeline_banner" "native_numeric_pipeline/native_numeric_pipeline.zrp" "NATIVE_NUMERIC_PIPELINE_PASS" "${COMMON_FAIL_REGEX}|\\[closure\\]" "" "" "tests/fixtures/projects/native_numeric_pipeline/bin/out/report.txt" "NATIVE_NUMERIC_PIPELINE_PASS")
register_project_case("native_numeric_pipeline_result" "native_numeric_pipeline/native_numeric_pipeline.zrp" "214" "${COMMON_FAIL_REGEX}|\\[closure\\]|null" "" "" "tests/fixtures/projects/native_numeric_pipeline/bin/out/report.txt" "checksum=214")
register_project_case("native_numeric_pipeline_probe" "native_numeric_pipeline/native_numeric_pipeline_probe.zrp" "probe:vmState" "${COMMON_FAIL_REGEX}|\\[closure\\]" "" "" "" "")
set(gc_fragment_stress_report_regex
        "GC_FRAGMENT_STRESS_PASS
checksum=[0-9]+
minorSamples=[1-9][0-9]*
minorMaxStepUs=[1-9][0-9]*
fullSamples=[1-9][0-9]*
fullMaxStepUs=[1-9][0-9]*
peakRememberedObjectCount=[0-9]+
peakRegionCount=[1-9][0-9]*
peakOldLiveBytes=[0-9]+
peakManagedMemoryBytes=[1-9][0-9]*
peakGcDebtBytes=[0-9]+")
register_project_case("gc_fragment_stress" "gc_fragment_stress/gc_fragment_stress.zrp" "GC_FRAGMENT_STRESS_PASS" "${COMMON_FAIL_REGEX}|Function value is NULL" "" "" "tests/fixtures/projects/gc_fragment_stress/bin/out/report.txt" "${gc_fragment_stress_report_regex}")
register_project_case("container_matrix_banner" "container_matrix/container_matrix.zrp" "CONTAINER_MATRIX_PASS" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("container_matrix_result" "container_matrix/container_matrix.zrp" "635" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("network_loopback" "network_loopback/network_loopback.zrp" "NETWORK_LOOPBACK_PASS ping pong echo" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("network_loopback_binary" "network_loopback/network_loopback.zrp" "NETWORK_LOOPBACK_PASS ping pong echo" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")

set_project_case_metadata("hello_world" "smoke;core;stress" "")
set_project_case_metadata("import_basic" "smoke;core;stress" "")
set_project_case_metadata("decorator_import" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_import" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_import_binary" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_parameter_import" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_parameter_import_binary" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_deep_import" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_provider_import" "smoke;core;stress" "")
set_project_case_metadata("classes" "core;stress" "")
set_project_case_metadata("classes_full" "core;stress" "")
set_project_case_metadata("classes_static" "core;stress" "")
set_project_case_metadata("classes_super" "core;stress" "")
set_project_case_metadata("classes_properties" "core;stress" "")
set_project_case_metadata("import_binary" "smoke;core;stress" "")
set_project_case_metadata("import_binary_const" "core;stress" "")
set_project_case_metadata("decorator_import_binary" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_binary" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_deep_import_binary" "smoke;core;stress" "")
set_project_case_metadata("decorator_compile_time_provider_import_binary" "smoke;core;stress" "")
set_project_case_metadata("import_pub_function" "core;stress" "")
set_project_case_metadata("import_capture_native" "core;stress" "")
set_project_case_metadata("import_capture_vector3" "core;stress" "")
set_project_case_metadata("native_chain_calls" "core;stress" "")
set_project_case_metadata("native_vector3_export_probe" "core;stress" "")
set_project_case_metadata("native_vector3_capture_probe" "core;stress" "")
set_project_case_metadata("native_math_export_probe" "core;stress" "")
set_project_case_metadata("native_complex_capture_probe" "core;stress" "")
set_project_case_metadata("native_numeric_pipeline_banner" "core;stress" "")
set_project_case_metadata("native_numeric_pipeline_result" "core;stress" "")
set_project_case_metadata("native_numeric_pipeline_probe" "core;stress" "")
set_project_case_metadata("gc_fragment_stress" "core;stress" "")
set_project_case_metadata("container_matrix_banner" "smoke;core;stress" "")
set_project_case_metadata("container_matrix_result" "smoke;core;stress" "")
set_project_case_metadata("network_loopback" "core;stress" "")
set_project_case_metadata("network_loopback_binary" "core;stress" "executed_via=binary")

set_project_case_execution("network_loopback_binary" "" "--execution-mode;binary;--emit-executed-via")

message("==========")
message("Running suite: projects")
if (NOT PROJECT_REQUESTED_TIER STREQUAL "")
    message("Tier filter: ${PROJECT_REQUESTED_TIER}")
endif()
message("==========")

foreach(case_name IN LISTS PROJECT_CASE_NAMES)
    run_project_case("${case_name}")
endforeach()
