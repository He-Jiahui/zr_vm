if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif()

if (DEFINED TIER AND NOT TIER STREQUAL "")
    string(TOLOWER "${TIER}" CLI_REQUESTED_TIER)
elseif (DEFINED ENV{ZR_VM_TEST_TIER} AND NOT "$ENV{ZR_VM_TEST_TIER}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_TEST_TIER}" CLI_REQUESTED_TIER)
else ()
    set(CLI_REQUESTED_TIER "")
endif ()

set(CLI_SUITE_ROOT "${GENERATED_DIR}/cli_suite")
file(REMOVE_RECURSE "${CLI_SUITE_ROOT}")
file(MAKE_DIRECTORY "${CLI_SUITE_ROOT}")

function(cli_assert_success case_name result_code output_text)
    if (NOT ${result_code} EQUAL 0)
        message(FATAL_ERROR "CLI case '${case_name}' failed with exit code ${${result_code}}.\nOutput:\n${${output_text}}")
    endif()
endfunction()

function(cli_assert_failure case_name result_code output_text)
    if (${result_code} EQUAL 0)
        message(FATAL_ERROR "CLI case '${case_name}' unexpectedly succeeded.\nOutput:\n${${output_text}}")
    endif()
endfunction()

function(cli_assert_contains case_name text_value expected)
    string(FIND "${${text_value}}" "${expected}" match_index)
    if (match_index EQUAL -1)
        message(FATAL_ERROR "CLI case '${case_name}' did not contain '${expected}'.\nOutput:\n${${text_value}}")
    endif()
endfunction()

function(cli_copy_fixture project_name destination_var)
    set(destination "${CLI_SUITE_ROOT}/${project_name}")
    file(COPY "${PROJECT_FIXTURES_DIR}/${project_name}" DESTINATION "${CLI_SUITE_ROOT}")
    set(${destination_var} "${destination}" PARENT_SCOPE)
endfunction()

function(cli_run case_name output_var result_var)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE case_result
        OUTPUT_VARIABLE case_stdout
        ERROR_VARIABLE case_stderr
    )

    set(case_output "${case_stdout}${case_stderr}")
    set(${output_var} "${case_output}" PARENT_SCOPE)
    set(${result_var} "${case_result}" PARENT_SCOPE)
endfunction()

function(cli_write_file path content)
    get_filename_component(parent_dir "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent_dir}")
    file(WRITE "${path}" "${content}")
endfunction()

function(cli_prepare_binary_module case_name project_dir source_rel output_name)
    set(temp_project_root "${CLI_SUITE_ROOT}/.prepare/${case_name}")
    set(temp_source_dir "${temp_project_root}/src")
    set(temp_binary_dir "${temp_project_root}/bin")
    set(temp_project_file "${temp_project_root}/${case_name}.zrp")
    set(temp_source_file "${temp_source_dir}/${output_name}.zr")
    set(final_output "${project_dir}/bin/${output_name}.zro")

    file(REMOVE_RECURSE "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_source_dir}")
    file(MAKE_DIRECTORY "${temp_binary_dir}")
    file(MAKE_DIRECTORY "${project_dir}/bin")
    file(COPY_FILE "${project_dir}/${source_rel}" "${temp_source_file}" ONLY_IF_DIFFERENT)
    file(WRITE "${temp_project_file}"
        "{\n"
        "  \"name\": \"${case_name}_prepare\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"${output_name}\"\n"
        "}\n")

    cli_run("${case_name}_prepare_binary" prepare_output prepare_result "${CLI_EXE}" "--compile" "${temp_project_file}")
    cli_assert_success("${case_name}_prepare_binary" prepare_result prepare_output)

    if (NOT EXISTS "${temp_binary_dir}/${output_name}.zro")
        message(FATAL_ERROR "CLI case '${case_name}' did not prepare ${output_name}.zro")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temp_binary_dir}/${output_name}.zro" "${final_output}"
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
        message(FATAL_ERROR "CLI case '${case_name}' failed while copying prepared binary module '${output_name}.zro'.")
    endif()

    file(REMOVE_RECURSE "${temp_project_root}")
endfunction()

function(cli_case_matches_tier tiers out_var)
    if (CLI_REQUESTED_TIER STREQUAL "")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    set(case_tiers "${tiers}")
    list(FIND case_tiers "${CLI_REQUESTED_TIER}" tier_index)
    if (tier_index EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else ()
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

message("==========")
message("Running suite: cli_integration")
if (NOT CLI_REQUESTED_TIER STREQUAL "")
    message("Tier filter: ${CLI_REQUESTED_TIER}")
endif()
message("==========")

if (DEFINED CMAKE_SHARED_LIBRARY_SUFFIX AND NOT CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL "")
    set(CLI_SHARED_LIB_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
elseif (WIN32)
    set(CLI_SHARED_LIB_SUFFIX ".dll")
elseif (APPLE)
    set(CLI_SHARED_LIB_SUFFIX ".dylib")
else ()
    set(CLI_SHARED_LIB_SUFFIX ".so")
endif ()

find_program(CLI_AOT_LLVM_HOST_TOOL NAMES clang clang-cl)
if (CLI_AOT_LLVM_HOST_TOOL)
    set(CLI_AOT_LLVM_HOST_AVAILABLE ON)
else ()
    set(CLI_AOT_LLVM_HOST_AVAILABLE OFF)
endif ()

cli_case_matches_tier("smoke;core;stress" run_help)
if (run_help)
    message("---- help")
    cli_run("help" help_output help_result "${CLI_EXE}" "--help")
    cli_assert_success("help" help_result help_output)
    cli_assert_contains("help" help_output "Usage:")
    cli_assert_contains("help" help_output "--compile <project.zrp>")
    cli_assert_contains("help" help_output "--intermediate")
    cli_assert_contains("help" help_output "--incremental")
    cli_assert_contains("help" help_output "--run")
endif()

cli_case_matches_tier("smoke;core;stress" run_positional)
if (run_positional)
    message("---- positional_run")
    cli_copy_fixture("hello_world" hello_world_dir)
    cli_run("positional_run" positional_output positional_result "${CLI_EXE}" "${hello_world_dir}/hello_world.zrp")
    cli_assert_success("positional_run" positional_result positional_output)
    cli_assert_contains("positional_run" positional_output "hello world")
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_hello)
if (run_compile_hello)
    message("---- compile_hello_world")
    cli_copy_fixture("hello_world" compile_hello_dir)
    file(REMOVE_RECURSE "${compile_hello_dir}/bin")
    cli_run("compile_hello_world" compile_hello_output compile_hello_result "${CLI_EXE}" "--compile" "${compile_hello_dir}/hello_world.zrp")
    cli_assert_success("compile_hello_world" compile_hello_result compile_hello_output)
    if (NOT EXISTS "${compile_hello_dir}/bin/main.zro")
        message(FATAL_ERROR "compile_hello_world did not create main.zro")
    endif()
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_intermediate)
if (run_compile_intermediate)
    message("---- compile_intermediate")
    cli_copy_fixture("hello_world" compile_intermediate_dir)
    file(REMOVE_RECURSE "${compile_intermediate_dir}/bin")
    cli_run("compile_intermediate" compile_intermediate_output compile_intermediate_result "${CLI_EXE}" "--compile" "${compile_intermediate_dir}/hello_world.zrp" "--intermediate")
    cli_assert_success("compile_intermediate" compile_intermediate_result compile_intermediate_output)
    if (NOT EXISTS "${compile_intermediate_dir}/bin/main.zro")
        message(FATAL_ERROR "compile_intermediate did not create main.zro")
    endif()
    if (NOT EXISTS "${compile_intermediate_dir}/bin/main.zri")
        message(FATAL_ERROR "compile_intermediate did not create main.zri")
    endif()
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_aot_c)
if (run_compile_aot_c)
    message("---- compile_aot_c_and_run")
    cli_copy_fixture("hello_world" compile_aot_c_dir)
    file(REMOVE_RECURSE "${compile_aot_c_dir}/bin")
    cli_run("compile_aot_c"
            compile_aot_c_output
            compile_aot_c_result
            "${CLI_EXE}"
            "--compile"
            "${compile_aot_c_dir}/hello_world.zrp"
            "--emit-aot-c")
    cli_assert_success("compile_aot_c" compile_aot_c_result compile_aot_c_output)
    if (NOT EXISTS "${compile_aot_c_dir}/bin/main.zro")
        message(FATAL_ERROR "compile_aot_c did not create main.zro")
    endif()
    if (NOT EXISTS "${compile_aot_c_dir}/bin/aot_c/src/main.c")
        message(FATAL_ERROR "compile_aot_c did not create AOT C source")
    endif()
    if (NOT EXISTS "${compile_aot_c_dir}/bin/aot_c/lib/zrvm_aot_main${CLI_SHARED_LIB_SUFFIX}")
        message(FATAL_ERROR "compile_aot_c did not create AOT shared library")
    endif()

    cli_run("run_aot_c"
            run_aot_c_output
            run_aot_c_result
            "${CLI_EXE}"
            "--execution-mode"
            "aot_c"
            "--require-aot-path"
            "--emit-executed-via"
            "${compile_aot_c_dir}/hello_world.zrp")
    cli_assert_success("run_aot_c" run_aot_c_result run_aot_c_output)
    cli_assert_contains("run_aot_c" run_aot_c_output "hello world")
    cli_assert_contains("run_aot_c" run_aot_c_output "executed_via=aot_c")
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_aot_llvm)
if (run_compile_aot_llvm)
    message("---- compile_aot_llvm_and_run")
    cli_copy_fixture("hello_world" compile_aot_llvm_dir)
    file(REMOVE_RECURSE "${compile_aot_llvm_dir}/bin")
    cli_run("compile_aot_llvm"
            compile_aot_llvm_output
            compile_aot_llvm_result
            "${CLI_EXE}"
            "--compile"
            "${compile_aot_llvm_dir}/hello_world.zrp"
            "--emit-aot-llvm")
    if (compile_aot_llvm_result EQUAL 0)
        if (NOT EXISTS "${compile_aot_llvm_dir}/bin/main.zro")
            message(FATAL_ERROR "compile_aot_llvm did not create main.zro")
        endif()
        if (NOT EXISTS "${compile_aot_llvm_dir}/bin/aot_llvm/ir/main.ll")
            message(FATAL_ERROR "compile_aot_llvm did not create AOT LLVM IR")
        endif()
        if (NOT EXISTS "${compile_aot_llvm_dir}/bin/aot_llvm/lib/zrvm_aot_main${CLI_SHARED_LIB_SUFFIX}")
            message(FATAL_ERROR "compile_aot_llvm did not create AOT LLVM shared library")
        endif()

        cli_run("run_aot_llvm"
                run_aot_llvm_output
                run_aot_llvm_result
                "${CLI_EXE}"
                "--execution-mode"
                "aot_llvm"
                "--require-aot-path"
                "--emit-executed-via"
                "${compile_aot_llvm_dir}/hello_world.zrp")
        cli_assert_success("run_aot_llvm" run_aot_llvm_result run_aot_llvm_output)
        cli_assert_contains("run_aot_llvm" run_aot_llvm_output "hello world")
        cli_assert_contains("run_aot_llvm" run_aot_llvm_output "executed_via=aot_llvm")
    else ()
        cli_assert_contains("compile_aot_llvm" compile_aot_llvm_output "AOT LLVM host adapter unavailable")
    endif()
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_recursive)
if (run_compile_recursive)
    message("---- compile_recursive_and_run")
    cli_copy_fixture("import_basic" import_basic_dir)
    file(REMOVE_RECURSE "${import_basic_dir}/bin")
    cli_run("compile_recursive_and_run" compile_run_output compile_run_result "${CLI_EXE}" "--compile" "${import_basic_dir}/import_basic.zrp" "--run")
    cli_assert_success("compile_recursive_and_run" compile_run_result compile_run_output)
    if (NOT EXISTS "${import_basic_dir}/bin/main.zro")
        message(FATAL_ERROR "compile_recursive_and_run did not create main.zro")
    endif()
    if (NOT EXISTS "${import_basic_dir}/bin/greet.zro")
        message(FATAL_ERROR "compile_recursive_and_run did not create greet.zro")
    endif()
    cli_assert_contains("compile_recursive_and_run" compile_run_output "hello from import")
endif()

cli_case_matches_tier("smoke;core;stress" run_aot_module_graph_roundtrip)
if (run_aot_module_graph_roundtrip)
    message("---- aot_module_graph_pipeline_roundtrip")
    cli_copy_fixture("aot_module_graph_pipeline" aot_graph_dir)
    file(REMOVE_RECURSE "${aot_graph_dir}/bin")
    cli_prepare_binary_module("aot_module_graph_pipeline_roundtrip"
                              "${aot_graph_dir}"
                              "fixtures/graph_binary_stage_source.zr"
                              "graph_binary_stage")

    cli_run("aot_module_graph_pipeline_compile"
            aot_graph_compile_output
            aot_graph_compile_result
            "${CLI_EXE}"
            "--compile"
            "${aot_graph_dir}/aot_module_graph_pipeline.zrp"
            "--intermediate")
    cli_assert_success("aot_module_graph_pipeline_compile" aot_graph_compile_result aot_graph_compile_output)
    if (NOT EXISTS "${aot_graph_dir}/bin/main.zro")
        message(FATAL_ERROR "aot_module_graph_pipeline_compile did not create main.zro")
    endif()
    if (NOT EXISTS "${aot_graph_dir}/bin/main.zri")
        message(FATAL_ERROR "aot_module_graph_pipeline_compile did not create main.zri")
    endif()
    if (NOT EXISTS "${aot_graph_dir}/bin/graph_binary_stage.zro")
        message(FATAL_ERROR "aot_module_graph_pipeline_compile removed prepared graph_binary_stage.zro")
    endif()

    cli_run("aot_module_graph_pipeline_run" aot_graph_run_output aot_graph_run_result "${CLI_EXE}" "${aot_graph_dir}/aot_module_graph_pipeline.zrp")
    cli_assert_success("aot_module_graph_pipeline_run" aot_graph_run_result aot_graph_run_output)
    cli_assert_contains("aot_module_graph_pipeline_run" aot_graph_run_output "AOT_MODULE_GRAPH_PIPELINE_PASS")
    cli_assert_contains("aot_module_graph_pipeline_run" aot_graph_run_output "102")
endif()

cli_case_matches_tier("smoke;core;stress" run_aot_module_graph_llvm_missing_import)
if (run_aot_module_graph_llvm_missing_import AND CLI_AOT_LLVM_HOST_AVAILABLE)
    message("---- aot_module_graph_pipeline_llvm_missing_import")
    cli_copy_fixture("aot_module_graph_pipeline" aot_graph_llvm_missing_dir)
    file(REMOVE_RECURSE "${aot_graph_llvm_missing_dir}/bin")
    cli_prepare_binary_module("aot_module_graph_pipeline_llvm_missing_import"
                              "${aot_graph_llvm_missing_dir}"
                              "fixtures/graph_binary_stage_source.zr"
                              "graph_binary_stage")

    cli_run("aot_module_graph_pipeline_llvm_missing_compile"
            aot_graph_llvm_missing_compile_output
            aot_graph_llvm_missing_compile_result
            "${CLI_EXE}"
            "--compile"
            "${aot_graph_llvm_missing_dir}/aot_module_graph_pipeline.zrp"
            "--emit-aot-llvm")
    cli_assert_success("aot_module_graph_pipeline_llvm_missing_compile"
                       aot_graph_llvm_missing_compile_result
                       aot_graph_llvm_missing_compile_output)

    cli_run("aot_module_graph_pipeline_llvm_missing_run"
            aot_graph_llvm_missing_run_output
            aot_graph_llvm_missing_run_result
            "${CLI_EXE}"
            "--execution-mode"
            "aot_llvm"
            "--require-aot-path"
            "--emit-executed-via"
            "${aot_graph_llvm_missing_dir}/aot_module_graph_pipeline.zrp")
    cli_assert_failure("aot_module_graph_pipeline_llvm_missing_run"
                       aot_graph_llvm_missing_run_result
                       aot_graph_llvm_missing_run_output)
    cli_assert_contains("aot_module_graph_pipeline_llvm_missing_run"
                        aot_graph_llvm_missing_run_output
                        "missing AOT artifacts for module 'graph_binary_stage'")
elseif (run_aot_module_graph_llvm_missing_import)
    message("---- aot_module_graph_pipeline_llvm_missing_import (skipped: AOT LLVM host adapter unavailable)")
endif()

cli_case_matches_tier("core;stress" run_incremental)
if (run_incremental)
    message("---- incremental")
    cli_copy_fixture("import_basic" incremental_dir)
    file(REMOVE_RECURSE "${incremental_dir}/bin")
    cli_run("incremental_first" incremental_first_output incremental_first_result "${CLI_EXE}" "--compile" "${incremental_dir}/import_basic.zrp" "--incremental")
    cli_assert_success("incremental_first" incremental_first_result incremental_first_output)
    cli_assert_contains("incremental_first" incremental_first_output "compile summary: compiled=2 skipped=0 removed=0")
    if (NOT EXISTS "${incremental_dir}/bin/.zr_cli_manifest")
        message(FATAL_ERROR "incremental_first did not create manifest")
    endif()

    cli_run("incremental_second" incremental_second_output incremental_second_result "${CLI_EXE}" "--compile" "${incremental_dir}/import_basic.zrp" "--incremental")
    cli_assert_success("incremental_second" incremental_second_result incremental_second_output)
    cli_assert_contains("incremental_second" incremental_second_output "compile summary: compiled=0 skipped=2 removed=0")

    cli_write_file("${incremental_dir}/src/greet.zr" "return \"hello from import v2\";\n")
    cli_run("incremental_changed_dependency" incremental_changed_output incremental_changed_result "${CLI_EXE}" "--compile" "${incremental_dir}/import_basic.zrp" "--incremental")
    cli_assert_success("incremental_changed_dependency" incremental_changed_result incremental_changed_output)
    cli_assert_contains("incremental_changed_dependency" incremental_changed_output "compile summary: compiled=2 skipped=0 removed=0")
endif()

cli_case_matches_tier("core;stress" run_incremental_cleanup)
if (run_incremental_cleanup)
    message("---- incremental_cleanup")
    cli_copy_fixture("import_basic" cleanup_dir)
    cli_write_file("${cleanup_dir}/src/old.zr" "return 11;\n")
    cli_write_file("${cleanup_dir}/src/main.zr" "var greetModule = %import(\"greet\");\nvar oldModule = %import(\"old\");\nreturn greetModule() + oldModule();\n")
    file(REMOVE_RECURSE "${cleanup_dir}/bin")
    cli_run("cleanup_initial" cleanup_initial_output cleanup_initial_result "${CLI_EXE}" "--compile" "${cleanup_dir}/import_basic.zrp" "--incremental")
    cli_assert_success("cleanup_initial" cleanup_initial_result cleanup_initial_output)
    if (NOT EXISTS "${cleanup_dir}/bin/old.zro")
        message(FATAL_ERROR "cleanup_initial did not create old.zro")
    endif()
    cli_write_file("${cleanup_dir}/src/main.zr" "var greetModule = %import(\"greet\");\nreturn greetModule();\n")
    cli_run("cleanup_second" cleanup_second_output cleanup_second_result "${CLI_EXE}" "--compile" "${cleanup_dir}/import_basic.zrp" "--incremental")
    cli_assert_success("cleanup_second" cleanup_second_result cleanup_second_output)
    cli_assert_contains("cleanup_second" cleanup_second_output "removed=1")
    if (EXISTS "${cleanup_dir}/bin/old.zro")
        message(FATAL_ERROR "cleanup_second did not remove stale old.zro")
    endif()
endif()

cli_case_matches_tier("smoke;core" run_repl)
if (run_repl)
    message("---- repl")
    set(repl_input_file "${CLI_SUITE_ROOT}/repl_input.txt")
    file(WRITE "${repl_input_file}" ":help\nreturn 2;\n\n:reset\nreturn 3;\n\n:quit\n")
    execute_process(
        COMMAND "${CLI_EXE}"
        INPUT_FILE "${repl_input_file}"
        RESULT_VARIABLE repl_result
        OUTPUT_VARIABLE repl_stdout
        ERROR_VARIABLE repl_stderr
    )
    set(repl_output "${repl_stdout}${repl_stderr}")
    if (NOT repl_result EQUAL 0)
        message(FATAL_ERROR "repl failed with exit code ${repl_result}.\nOutput:\n${repl_output}")
    endif()
    string(FIND "${repl_output}" "ZR VM REPL" repl_banner_index)
    if (repl_banner_index EQUAL -1)
        message(FATAL_ERROR "repl did not print banner.\nOutput:\n${repl_output}")
    endif()
    string(FIND "${repl_output}" "Available commands" repl_help_index)
    if (repl_help_index EQUAL -1)
        message(FATAL_ERROR "repl did not print help text.\nOutput:\n${repl_output}")
    endif()
    string(FIND "${repl_output}" "3" repl_value_index)
    if (repl_value_index EQUAL -1)
        message(FATAL_ERROR "repl did not evaluate submitted code after :reset.\nOutput:\n${repl_output}")
    endif()
endif()

cli_case_matches_tier("core;stress" run_repl_native_import)
if (run_repl_native_import)
    message("---- repl_native_import")
    set(repl_native_import_input_file "${CLI_SUITE_ROOT}/repl_native_import_input.txt")
    file(WRITE "${repl_native_import_input_file}" "var s = %import(\"zr.system\");\ns.console.print(\"xxx\");\n\n:quit\n")
    execute_process(
        COMMAND "${CLI_EXE}"
        INPUT_FILE "${repl_native_import_input_file}"
        RESULT_VARIABLE repl_native_import_result
        OUTPUT_VARIABLE repl_native_import_stdout
        ERROR_VARIABLE repl_native_import_stderr
    )
    set(repl_native_import_output "${repl_native_import_stdout}${repl_native_import_stderr}")
    if (NOT repl_native_import_result EQUAL 0)
        message(FATAL_ERROR "repl_native_import failed with exit code ${repl_native_import_result}.\nOutput:\n${repl_native_import_output}")
    endif()
    string(FIND "${repl_native_import_output}" "xxx" repl_native_import_index)
    if (repl_native_import_index EQUAL -1)
        message(FATAL_ERROR "repl_native_import did not print console output.\nOutput:\n${repl_native_import_output}")
    endif()
endif()

cli_case_matches_tier("core;stress" run_repl_runtime_error)
if (run_repl_runtime_error)
    message("---- repl_runtime_error")
    set(repl_runtime_error_input_file "${CLI_SUITE_ROOT}/repl_runtime_error_input.txt")
    file(WRITE "${repl_runtime_error_input_file}" "var s = %import(\"zr.system\");\n\ns.console.print(\"xxx\");\n\n:quit\n")
    execute_process(
        COMMAND "${CLI_EXE}"
        INPUT_FILE "${repl_runtime_error_input_file}"
        RESULT_VARIABLE repl_runtime_error_result
        OUTPUT_VARIABLE repl_runtime_error_stdout
        ERROR_VARIABLE repl_runtime_error_stderr
    )
    set(repl_runtime_error_output "${repl_runtime_error_stdout}${repl_runtime_error_stderr}")
    if (NOT repl_runtime_error_result EQUAL 0)
        message(FATAL_ERROR "repl_runtime_error failed with exit code ${repl_runtime_error_result}.\nOutput:\n${repl_runtime_error_output}")
    endif()
    string(FIND "${repl_runtime_error_output}" "GET_MEMBER: missing member 's'" repl_runtime_error_index)
    if (repl_runtime_error_index EQUAL -1)
        message(FATAL_ERROR "repl_runtime_error did not surface the runtime error.\nOutput:\n${repl_runtime_error_output}")
    endif()
endif()
