if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif()

set(CLI_SUITE_ROOT "${GENERATED_DIR}/cli_suite")
file(REMOVE_RECURSE "${CLI_SUITE_ROOT}")
file(MAKE_DIRECTORY "${CLI_SUITE_ROOT}")

function(cli_assert_success case_name result_code output_text)
    if (NOT ${result_code} EQUAL 0)
        message(FATAL_ERROR "CLI case '${case_name}' failed with exit code ${${result_code}}.\nOutput:\n${${output_text}}")
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

message("==========")
message("Running suite: cli_integration")
message("==========")

message("---- help")
cli_run("help" help_output help_result "${CLI_EXE}" "--help")
cli_assert_success("help" help_result help_output)
cli_assert_contains("help" help_output "Usage:")
cli_assert_contains("help" help_output "--compile <project.zrp>")
cli_assert_contains("help" help_output "--intermediate")
cli_assert_contains("help" help_output "--incremental")
cli_assert_contains("help" help_output "--run")

message("---- positional_run")
cli_copy_fixture("hello_world" hello_world_dir)
cli_run("positional_run" positional_output positional_result "${CLI_EXE}" "${hello_world_dir}/hello_world.zrp")
cli_assert_success("positional_run" positional_result positional_output)
cli_assert_contains("positional_run" positional_output "hello world")

message("---- compile_hello_world")
cli_copy_fixture("hello_world" compile_hello_dir)
file(REMOVE_RECURSE "${compile_hello_dir}/bin")
cli_run("compile_hello_world" compile_hello_output compile_hello_result "${CLI_EXE}" "--compile" "${compile_hello_dir}/hello_world.zrp")
cli_assert_success("compile_hello_world" compile_hello_result compile_hello_output)
if (NOT EXISTS "${compile_hello_dir}/bin/main.zro")
    message(FATAL_ERROR "compile_hello_world did not create main.zro")
endif()

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
string(FIND "${repl_runtime_error_output}" "GETTABLE: table must be an object or array" repl_runtime_error_index)
if (repl_runtime_error_index EQUAL -1)
    message(FATAL_ERROR "repl_runtime_error did not surface the runtime error.\nOutput:\n${repl_runtime_error_output}")
endif()
