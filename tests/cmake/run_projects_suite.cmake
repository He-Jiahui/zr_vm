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

if (DEFINED CMAKE_SHARED_LIBRARY_SUFFIX AND NOT CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL "")
    set(PROJECT_SHARED_LIB_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
elseif (WIN32)
    set(PROJECT_SHARED_LIB_SUFFIX ".dll")
elseif (APPLE)
    set(PROJECT_SHARED_LIB_SUFFIX ".dylib")
else ()
    set(PROJECT_SHARED_LIB_SUFFIX ".so")
endif ()

find_program(PROJECT_AOT_LLVM_HOST_TOOL NAMES clang clang-cl)
if (PROJECT_AOT_LLVM_HOST_TOOL)
    set(PROJECT_AOT_LLVM_HOST_AVAILABLE ON)
else ()
    set(PROJECT_AOT_LLVM_HOST_AVAILABLE OFF)
endif ()

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
    set("PROJECT_CASE_COMPILE_ARGS_${name}" "" PARENT_SCOPE)
    set("PROJECT_CASE_RUN_ARGS_${name}" "" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_AOT_C_${name}" "OFF" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_AOT_LLVM_${name}" "OFF" PARENT_SCOPE)
endfunction()

function(set_project_case_metadata name tiers executed_via_regex require_aot_path)
    set("PROJECT_CASE_TIERS_${name}" "${tiers}" PARENT_SCOPE)
    set("PROJECT_CASE_EXECUTED_VIA_REGEX_${name}" "${executed_via_regex}" PARENT_SCOPE)
    set("PROJECT_CASE_REQUIRE_AOT_PATH_${name}" "${require_aot_path}" PARENT_SCOPE)
endfunction()

function(set_project_case_execution name compile_args run_args prepare_aot_c prepare_aot_llvm)
    set("PROJECT_CASE_COMPILE_ARGS_${name}" "${compile_args}" PARENT_SCOPE)
    set("PROJECT_CASE_RUN_ARGS_${name}" "${run_args}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_AOT_C_${name}" "${prepare_aot_c}" PARENT_SCOPE)
    set("PROJECT_CASE_PREPARE_AOT_LLVM_${name}" "${prepare_aot_llvm}" PARENT_SCOPE)
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

function(run_fixture_prepare case_name prepare_source_rel prepare_output_rel prepare_aot_c prepare_aot_llvm)
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
    if (prepare_aot_c)
        list(APPEND compile_command "--emit-aot-c")
    endif()
    if (prepare_aot_llvm)
        list(APPEND compile_command "--emit-aot-llvm")
    endif()

    file(REMOVE_RECURSE "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_source_dir}")
    file(MAKE_DIRECTORY "${temp_binary_dir}")
    if (prepare_aot_c)
        file(MAKE_DIRECTORY "${temp_binary_dir}/aot_c/src")
        file(MAKE_DIRECTORY "${temp_binary_dir}/aot_c/lib")
    endif()
    if (prepare_aot_llvm)
        file(MAKE_DIRECTORY "${temp_binary_dir}/aot_llvm/ir")
        file(MAKE_DIRECTORY "${temp_binary_dir}/aot_llvm/lib")
    endif()
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

    if (prepare_aot_c)
        set(temp_aot_source "${temp_binary_dir}/aot_c/src/${module_name}.c")
        set(temp_aot_library "${temp_binary_dir}/aot_c/lib/zrvm_aot_${module_name}${PROJECT_SHARED_LIB_SUFFIX}")
        set(final_aot_source "${prepare_output_dir}/aot_c/src/${module_name}.c")
        set(final_aot_library "${prepare_output_dir}/aot_c/lib/zrvm_aot_${module_name}${PROJECT_SHARED_LIB_SUFFIX}")

        if (NOT EXISTS "${temp_aot_source}" OR NOT EXISTS "${temp_aot_library}")
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' did not produce AOT artifacts for prepared fixture '${module_name}'.")
        endif()

        file(MAKE_DIRECTORY "${prepare_output_dir}/aot_c/src")
        file(MAKE_DIRECTORY "${prepare_output_dir}/aot_c/lib")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_aot_source}" "${final_aot_source}"
            RESULT_VARIABLE copy_aot_source_result
            OUTPUT_VARIABLE copy_aot_source_stdout
            ERROR_VARIABLE copy_aot_source_stderr
        )
        if (NOT copy_aot_source_stdout STREQUAL "")
            message("${copy_aot_source_stdout}")
        endif()
        if (NOT copy_aot_source_stderr STREQUAL "")
            message("${copy_aot_source_stderr}")
        endif()
        if (NOT copy_aot_source_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared AOT source for '${module_name}'.")
        endif()

        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_aot_library}" "${final_aot_library}"
            RESULT_VARIABLE copy_aot_library_result
            OUTPUT_VARIABLE copy_aot_library_stdout
            ERROR_VARIABLE copy_aot_library_stderr
        )
        if (NOT copy_aot_library_stdout STREQUAL "")
            message("${copy_aot_library_stdout}")
        endif()
        if (NOT copy_aot_library_stderr STREQUAL "")
            message("${copy_aot_library_stderr}")
        endif()
        if (NOT copy_aot_library_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared AOT library for '${module_name}'.")
        endif()
    endif()

    if (prepare_aot_llvm)
        set(temp_aot_llvm_ir "${temp_binary_dir}/aot_llvm/ir/${module_name}.ll")
        set(temp_aot_llvm_library "${temp_binary_dir}/aot_llvm/lib/zrvm_aot_${module_name}${PROJECT_SHARED_LIB_SUFFIX}")
        set(final_aot_llvm_ir "${prepare_output_dir}/aot_llvm/ir/${module_name}.ll")
        set(final_aot_llvm_library "${prepare_output_dir}/aot_llvm/lib/zrvm_aot_${module_name}${PROJECT_SHARED_LIB_SUFFIX}")

        if (NOT EXISTS "${temp_aot_llvm_ir}" OR NOT EXISTS "${temp_aot_llvm_library}")
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' did not produce AOT LLVM artifacts for prepared fixture '${module_name}'.")
        endif()

        file(MAKE_DIRECTORY "${prepare_output_dir}/aot_llvm/ir")
        file(MAKE_DIRECTORY "${prepare_output_dir}/aot_llvm/lib")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_aot_llvm_ir}" "${final_aot_llvm_ir}"
            RESULT_VARIABLE copy_aot_llvm_ir_result
            OUTPUT_VARIABLE copy_aot_llvm_ir_stdout
            ERROR_VARIABLE copy_aot_llvm_ir_stderr
        )
        if (NOT copy_aot_llvm_ir_stdout STREQUAL "")
            message("${copy_aot_llvm_ir_stdout}")
        endif()
        if (NOT copy_aot_llvm_ir_stderr STREQUAL "")
            message("${copy_aot_llvm_ir_stderr}")
        endif()
        if (NOT copy_aot_llvm_ir_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared AOT LLVM IR for '${module_name}'.")
        endif()

        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_aot_llvm_library}" "${final_aot_llvm_library}"
            RESULT_VARIABLE copy_aot_llvm_library_result
            OUTPUT_VARIABLE copy_aot_llvm_library_stdout
            ERROR_VARIABLE copy_aot_llvm_library_stderr
        )
        if (NOT copy_aot_llvm_library_stdout STREQUAL "")
            message("${copy_aot_llvm_library_stdout}")
        endif()
        if (NOT copy_aot_llvm_library_stderr STREQUAL "")
            message("${copy_aot_llvm_library_stderr}")
        endif()
        if (NOT copy_aot_llvm_library_result EQUAL 0)
            file(REMOVE_RECURSE "${temp_project_root}")
            message(FATAL_ERROR "Project case '${case_name}' failed while copying prepared AOT LLVM library for '${module_name}'.")
        endif()
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
    set(compile_args "${PROJECT_CASE_COMPILE_ARGS_${case_name}}")
    set(run_args "${PROJECT_CASE_RUN_ARGS_${case_name}}")
    set(prepare_aot_c "${PROJECT_CASE_PREPARE_AOT_C_${case_name}}")
    set(prepare_aot_llvm "${PROJECT_CASE_PREPARE_AOT_LLVM_${case_name}}")
    set(project_file "${PROJECT_FIXTURES_DIR}/${project_rel}")
    project_case_matches_tier("${case_tiers}" run_case)

    if (NOT run_case)
        return()
    endif()

    message("---- ${case_name}")

    if (NOT prepare_source_rel STREQUAL "" AND NOT prepare_output_rel STREQUAL "")
        run_fixture_prepare("${case_name}" "${prepare_source_rel}" "${prepare_output_rel}" "${prepare_aot_c}" "${prepare_aot_llvm}")
    endif()

    if (NOT required_file_rel STREQUAL "")
        file(REMOVE "${REPO_ROOT}/${required_file_rel}")
    endif()

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
            message(FATAL_ERROR "Project case '${case_name}' compile step failed with exit code ${compile_result}.")
        endif()
    endif()

    set(run_command "${CLI_EXE}")
    if (NOT run_args STREQUAL "")
        list(APPEND run_command ${run_args})
    endif()
    list(APPEND run_command "${project_file}")

    execute_process(
        COMMAND ${run_command}
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
register_project_case("container_matrix_result" "container_matrix/container_matrix.zrp" "635" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
register_project_case("hello_world_aot_c" "hello_world/hello_world.zrp" "hello world" "${COMMON_FAIL_REGEX}" "" "" "" "")
register_project_case("import_basic_aot_c" "import_basic/import_basic.zrp" "hello from import" "${COMMON_FAIL_REGEX}" "" "" "" "")
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

register_project_case("aot_module_graph_pipeline_aot_c" "aot_module_graph_pipeline/aot_module_graph_pipeline.zrp" "AOT_MODULE_GRAPH_PIPELINE_PASS[
\n]+102" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "aot_module_graph_pipeline/fixtures/graph_binary_stage_source.zr" "aot_module_graph_pipeline/bin/graph_binary_stage.zro" "" "")
set_project_case_metadata("aot_module_graph_pipeline_aot_c" "smoke;core;stress" "executed_via=aot_c" "ON")
set_project_case_execution("aot_module_graph_pipeline_aot_c" "--emit-aot-c" "--execution-mode;aot_c;--require-aot-path;--emit-executed-via" "ON" "OFF")
register_project_case("aot_dynamic_meta_ownership_lab_aot_c" "aot_dynamic_meta_ownership_lab/aot_dynamic_meta_ownership_lab.zrp" "AOT_DYNAMIC_META_OWNERSHIP_LAB_PASS" "${COMMON_FAIL_REGEX}|Function value is NULL" "" "" "" "")
register_project_case("aot_eh_tail_gc_stress_aot_c" "aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp" "AOT_EH_TAIL_GC_STRESS_PASS" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
set_project_case_metadata("aot_dynamic_meta_ownership_lab_aot_c" "core;stress" "executed_via=aot_c" "ON")
set_project_case_metadata("aot_eh_tail_gc_stress_aot_c" "stress" "executed_via=aot_c" "ON")
set_project_case_execution("aot_dynamic_meta_ownership_lab_aot_c" "--emit-aot-c" "--execution-mode;aot_c;--require-aot-path;--emit-executed-via" "OFF" "OFF")
set_project_case_execution("aot_eh_tail_gc_stress_aot_c" "--emit-aot-c" "--execution-mode;aot_c;--require-aot-path;--emit-executed-via" "OFF" "OFF")

set_project_case_metadata("hello_world_aot_c" "smoke;core;stress" "executed_via=aot_c" "ON")
set_project_case_metadata("import_basic_aot_c" "smoke;core;stress" "executed_via=aot_c" "ON")
set_project_case_execution("hello_world_aot_c" "--emit-aot-c" "--execution-mode;aot_c;--require-aot-path;--emit-executed-via" "OFF" "OFF")
set_project_case_execution("import_basic_aot_c" "--emit-aot-c" "--execution-mode;aot_c;--require-aot-path;--emit-executed-via" "OFF" "OFF")

if (PROJECT_AOT_LLVM_HOST_AVAILABLE)
    register_project_case("hello_world_aot_llvm" "hello_world/hello_world.zrp" "hello world" "${COMMON_FAIL_REGEX}" "" "" "" "")
    register_project_case("import_basic_aot_llvm" "import_basic/import_basic.zrp" "hello from import" "${COMMON_FAIL_REGEX}" "" "" "" "")
    register_project_case("aot_module_graph_pipeline_aot_llvm" "aot_module_graph_pipeline/aot_module_graph_pipeline.zrp" "AOT_MODULE_GRAPH_PIPELINE_PASS[
\n]+102" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "aot_module_graph_pipeline/fixtures/graph_binary_stage_source.zr" "aot_module_graph_pipeline/bin/graph_binary_stage.zro" "" "")
    set_project_case_metadata("hello_world_aot_llvm" "smoke;core;stress" "executed_via=aot_llvm" "ON")
    set_project_case_metadata("import_basic_aot_llvm" "smoke;core;stress" "executed_via=aot_llvm" "ON")
    set_project_case_metadata("aot_module_graph_pipeline_aot_llvm" "smoke;core;stress" "executed_via=aot_llvm" "ON")
    set_project_case_execution("hello_world_aot_llvm" "--emit-aot-llvm" "--execution-mode;aot_llvm;--require-aot-path;--emit-executed-via" "OFF" "OFF")
    set_project_case_execution("import_basic_aot_llvm" "--emit-aot-llvm" "--execution-mode;aot_llvm;--require-aot-path;--emit-executed-via" "OFF" "OFF")
    set_project_case_execution("aot_module_graph_pipeline_aot_llvm" "--emit-aot-llvm" "--execution-mode;aot_llvm;--require-aot-path;--emit-executed-via" "OFF" "ON")
    register_project_case("aot_dynamic_meta_ownership_lab_aot_llvm" "aot_dynamic_meta_ownership_lab/aot_dynamic_meta_ownership_lab.zrp" "AOT_DYNAMIC_META_OWNERSHIP_LAB_PASS" "${COMMON_FAIL_REGEX}|Function value is NULL" "" "" "" "")
    register_project_case("aot_eh_tail_gc_stress_aot_llvm" "aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp" "AOT_EH_TAIL_GC_STRESS_PASS" "${COMMON_FAIL_REGEX}|null|Function value is NULL" "" "" "" "")
    set_project_case_metadata("aot_dynamic_meta_ownership_lab_aot_llvm" "core;stress" "executed_via=aot_llvm" "ON")
    set_project_case_metadata("aot_eh_tail_gc_stress_aot_llvm" "stress" "executed_via=aot_llvm" "ON")
    set_project_case_execution("aot_dynamic_meta_ownership_lab_aot_llvm" "--emit-aot-llvm" "--execution-mode;aot_llvm;--require-aot-path;--emit-executed-via" "OFF" "OFF")
    set_project_case_execution("aot_eh_tail_gc_stress_aot_llvm" "--emit-aot-llvm" "--execution-mode;aot_llvm;--require-aot-path;--emit-executed-via" "OFF" "OFF")
endif ()

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
