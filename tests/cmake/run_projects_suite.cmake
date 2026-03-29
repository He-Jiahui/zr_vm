if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED FIXTURE_BUILDER_EXE OR FIXTURE_BUILDER_EXE STREQUAL "")
    message(FATAL_ERROR "FIXTURE_BUILDER_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED REPO_ROOT OR REPO_ROOT STREQUAL "")
    message(FATAL_ERROR "REPO_ROOT is required.")
endif()

set(COMMON_FAIL_REGEX "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error")
set(PROJECT_CASE_NAMES "")

function(register_project_case name project_rel pass_regex fail_regex prepare_source_rel prepare_output_rel required_file_rel required_file_regex)
    set(PROJECT_CASE_NAMES "${PROJECT_CASE_NAMES};${name}" PARENT_SCOPE)
    set("PROJECT_CASE_PROJECT_${name}" "${project_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_PASS_${name}" "${pass_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_FAIL_${name}" "${fail_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_SOURCE_${name}" "${prepare_source_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_OUTPUT_${name}" "${prepare_output_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRED_FILE_${name}" "${required_file_rel}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRED_FILE_REGEX_${name}" "${required_file_regex}" PARENT_SCOPE)
endfunction()

function(run_fixture_prepare case_name prepare_source_rel prepare_output_rel)
    set(prepare_source "${PROJECT_FIXTURES_DIR}/${prepare_source_rel}")
    set(prepare_output "${PROJECT_FIXTURES_DIR}/${prepare_output_rel}")

    get_filename_component(prepare_output_dir "${prepare_output}" DIRECTORY)
    file(MAKE_DIRECTORY "${prepare_output_dir}")

    execute_process(
        COMMAND "${FIXTURE_BUILDER_EXE}" "${prepare_source}" "${prepare_output}"
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
        message(FATAL_ERROR "Project case '${case_name}' failed while preparing fixture '${prepare_source_rel}'.")
    endif()
endfunction()

function(run_project_case case_name)
    set(project_rel "${PROJECT_CASE_PROJECT_${case_name}}")
    set(pass_regex "${PROJECT_CASE_PASS_${case_name}}")
    set(fail_regex "${PROJECT_CASE_FAIL_${case_name}}")
    set(prepare_source_rel "${PROJECT_CASE_PREPARE_SOURCE_${case_name}}")
    set(prepare_output_rel "${PROJECT_CASE_PREPARE_OUTPUT_${case_name}}")
    set(required_file_rel "${PROJECT_CASE_REQUIRED_FILE_${case_name}}")
    set(required_file_regex "${PROJECT_CASE_REQUIRED_FILE_REGEX_${case_name}}")
    set(project_file "${PROJECT_FIXTURES_DIR}/${project_rel}")

    message("---- ${case_name}")

    if (NOT prepare_source_rel STREQUAL "" AND NOT prepare_output_rel STREQUAL "")
        run_fixture_prepare("${case_name}" "${prepare_source_rel}" "${prepare_output_rel}")
    endif()

    if (NOT required_file_rel STREQUAL "")
        file(REMOVE "${REPO_ROOT}/${required_file_rel}")
    endif()

    execute_process(
        COMMAND "${CLI_EXE}" "${project_file}"
        WORKING_DIRECTORY "${REPO_ROOT}"
        RESULT_VARIABLE project_result
        OUTPUT_VARIABLE project_stdout
        ERROR_VARIABLE project_stderr
    )

    set(project_output "${project_stdout}${project_stderr}")

    if (NOT project_stdout STREQUAL "")
        message("${project_stdout}")
    endif()

    if (NOT project_stderr STREQUAL "")
        message("${project_stderr}")
    endif()

    if (NOT project_result EQUAL 0)
        message(FATAL_ERROR "Project case '${case_name}' failed with exit code ${project_result}.")
    endif()

    if (NOT fail_regex STREQUAL "" AND project_output MATCHES "${fail_regex}")
        message(FATAL_ERROR "Project case '${case_name}' matched fail regex '${fail_regex}'.")
    endif()

    if (NOT pass_regex STREQUAL "" AND NOT project_output MATCHES "${pass_regex}")
        message(FATAL_ERROR "Project case '${case_name}' did not match pass regex '${pass_regex}'.")
    endif()

    if (NOT required_file_rel STREQUAL "")
        set(required_file "${REPO_ROOT}/${required_file_rel}")
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
register_project_case("classes" "classes/classes.zrp" "61" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_full" "classes_full/classes_full.zrp" "127" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_static" "classes_static/classes_static.zrp" "82" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_super" "classes_super/classes_super.zrp" "42" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("classes_properties" "classes_properties/classes_properties.zrp" "40" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("import_binary" "import_binary/import_binary.zrp" "hello from import" "${COMMON_FAIL_REGEX}" "import_binary/fixtures/greet_binary_source.zr" "import_binary/bin/greet.zro" "" "")
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

message("==========")
message("Running suite: projects")
message("==========")

foreach(case_name IN LISTS PROJECT_CASE_NAMES)
    if (NOT case_name STREQUAL "")
        run_project_case("${case_name}")
    endif()
endforeach()
