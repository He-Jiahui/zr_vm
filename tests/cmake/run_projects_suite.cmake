if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED REPO_ROOT OR REPO_ROOT STREQUAL "")
    message(FATAL_ERROR "REPO_ROOT is required.")
endif()

if (DEFINED TIER AND NOT TIER STREQUAL "")
    string(TOLOWER "${TIER}" PROJECT_REQUESTED_TIER)
elseif (DEFINED ENV{ZR_VM_TEST_TIER} AND NOT "$ENV{ZR_VM_TEST_TIER}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_TEST_TIER}" PROJECT_REQUESTED_TIER)
else ()
    set(PROJECT_REQUESTED_TIER "")
endif ()

if (DEFINED ENV{ZR_VM_REQUIRE_AOT_PATH} AND NOT "$ENV{ZR_VM_REQUIRE_AOT_PATH}" STREQUAL "")
    set(PROJECT_REQUIRE_AOT_PATH "$ENV{ZR_VM_REQUIRE_AOT_PATH}")
else ()
    set(PROJECT_REQUIRE_AOT_PATH "")
endif ()

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
    set("PROJECT_CASE_TIERS_${name}" "core" PARENT_SCOPE)
    set("PROJECT_CASE_EXECUTED_VIA_REGEX_${name}" "" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRE_AOT_PATH_${name}" "OFF" PARENT_SCOPE)
endfunction()

function(set_project_case_metadata name tiers executed_via_regex require_aot_path)
    set("PROJECT_CASE_TIERS_${name}" "${tiers}" PARENT_SCOPE)
    set("PROJECT_CASE_EXECUTED_VIA_REGEX_${name}" "${executed_via_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRE_AOT_PATH_${name}" "${require_aot_path}" PARENT_SCOPE)
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

    if (prepare_output_ext STREQUAL ".zri")
        list(APPEND compile_command "--intermediate")
    endif()

    file(REMOVE_RECURSE "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_source_dir}")
    file(MAKE_DIRECTORY "${prepare_output_dir}")
    file(COPY_FILE "${prepare_source}" "${temp_source_file}" ONLY_IF_DIFFERENT)
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

    file(REMOVE_RECURSE "${temp_project_root}")

    if (NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared fixture '${prepare_output_rel}'.")
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
    set(case_tiers "${PROJECT_CASE_TIERS_${case_name}}")
    set(executed_via_regex "${PROJECT_CASE_EXECUTED_VIA_REGEX_${case_name}}")
    set(require_aot_path "${PROJECT_CASE_REQUIRE_AOT_PATH_${case_name}}")
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

    if (NOT executed_via_regex STREQUAL "" AND NOT project_output MATCHES "${executed_via_regex}")
        message(FATAL_ERROR "Project case '${case_name}' did not expose executed_via output matching '${executed_via_regex}'.")
    endif()

    if (require_aot_path AND executed_via_regex STREQUAL "")
        message(FATAL_ERROR "Project case '${case_name}' requested require_aot_path but has no executed_via regex.")
    endif()

    if (NOT PROJECT_REQUIRE_AOT_PATH STREQUAL "" AND
            NOT PROJECT_REQUIRE_AOT_PATH STREQUAL "0" AND
            require_aot_path AND
            executed_via_regex STREQUAL "")
        message(FATAL_ERROR "Project case '${case_name}' was gated by ZR_VM_REQUIRE_AOT_PATH but does not define an executed_via contract.")
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
register_project_case("container_matrix_banner" "container_matrix/container_matrix.zrp" "CONTAINER_MATRIX_PASS" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("container_matrix_result" "container_matrix/container_matrix.zrp" "633" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("aot_module_graph_pipeline" "aot_module_graph_pipeline/aot_module_graph_pipeline.zrp" "AOT_MODULE_GRAPH_PIPELINE_PASS[\n]+102" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "aot_module_graph_pipeline/fixtures/graph_binary_stage_source.zr" "aot_module_graph_pipeline/bin/graph_binary_stage.zro" "" "")
register_project_case("aot_dynamic_meta_ownership_lab" "aot_dynamic_meta_ownership_lab/aot_dynamic_meta_ownership_lab.zrp" "AOT_DYNAMIC_META_OWNERSHIP_LAB_PASS[\n]+29" "${COMMON_FAIL_REGEX}|Function value is NULL" "" "" "" "")
register_project_case("aot_eh_tail_gc_stress" "aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp" "AOT_EH_TAIL_GC_STRESS_PASS[\n]+31" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")

set_project_case_metadata("hello_world" "smoke;core;stress" "" "OFF")
set_project_case_metadata("import_basic" "smoke;core;stress" "" "OFF")
set_project_case_metadata("classes" "core;stress" "" "OFF")
set_project_case_metadata("classes_full" "core;stress" "" "OFF")
set_project_case_metadata("classes_static" "core;stress" "" "OFF")
set_project_case_metadata("classes_super" "core;stress" "" "OFF")
set_project_case_metadata("classes_properties" "core;stress" "" "OFF")
set_project_case_metadata("import_binary" "smoke;core;stress" "" "OFF")
set_project_case_metadata("import_pub_function" "core;stress" "" "OFF")
set_project_case_metadata("import_capture_native" "core;stress" "" "OFF")
set_project_case_metadata("import_capture_vector3" "core;stress" "" "OFF")
set_project_case_metadata("native_chain_calls" "core;stress" "" "OFF")
set_project_case_metadata("native_vector3_export_probe" "core;stress" "" "OFF")
set_project_case_metadata("native_vector3_capture_probe" "core;stress" "" "OFF")
set_project_case_metadata("native_math_export_probe" "core;stress" "" "OFF")
set_project_case_metadata("native_complex_capture_probe" "core;stress" "" "OFF")
set_project_case_metadata("native_numeric_pipeline_banner" "core;stress" "" "OFF")
set_project_case_metadata("native_numeric_pipeline_result" "core;stress" "" "OFF")
set_project_case_metadata("native_numeric_pipeline_probe" "core;stress" "" "OFF")
set_project_case_metadata("container_matrix_banner" "smoke;core;stress" "" "OFF")
set_project_case_metadata("container_matrix_result" "smoke;core;stress" "" "OFF")
set_project_case_metadata("aot_module_graph_pipeline" "smoke;core;stress" "" "OFF")
set_project_case_metadata("aot_dynamic_meta_ownership_lab" "core;stress" "" "OFF")
set_project_case_metadata("aot_eh_tail_gc_stress" "stress" "" "OFF")

message("==========")
message("Running suite: projects")
if (NOT PROJECT_REQUESTED_TIER STREQUAL "")
    message("Tier filter: ${PROJECT_REQUESTED_TIER}")
endif()
message("==========")

foreach(case_name IN LISTS PROJECT_CASE_NAMES)
    if (NOT case_name STREQUAL "")
        run_project_case("${case_name}")
    endif()
endforeach()
