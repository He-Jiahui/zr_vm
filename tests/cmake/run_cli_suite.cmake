if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required.")
endif()

if (NOT DEFINED PROJECT_FIXTURES_DIR OR PROJECT_FIXTURES_DIR STREQUAL "")
    message(FATAL_ERROR "PROJECT_FIXTURES_DIR is required.")
endif()

if (NOT DEFINED GENERATED_DIR OR GENERATED_DIR STREQUAL "")
    message(FATAL_ERROR "GENERATED_DIR is required.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

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

function(cli_assert_matches case_name text_value expected_regex)
    if (NOT "${${text_value}}" MATCHES "${expected_regex}")
        message(FATAL_ERROR "CLI case '${case_name}' did not match /${expected_regex}/.\nOutput:\n${${text_value}}")
    endif()
endfunction()

function(cli_assert_file_contains case_name path expected)
    if (NOT EXISTS "${path}")
        message(FATAL_ERROR "CLI case '${case_name}' expected file '${path}' to exist.")
    endif()

    file(READ "${path}" file_text)
    string(FIND "${file_text}" "${expected}" match_index)
    if (match_index EQUAL -1)
        message(FATAL_ERROR "CLI case '${case_name}' expected file '${path}' to contain '${expected}'.")
    endif()
endfunction()

function(cli_copy_fixture project_name destination_var)
    set(destination "${CLI_SUITE_ROOT}/${project_name}")
    file(REMOVE_RECURSE "${destination}")
    file(MAKE_DIRECTORY "${destination}")
    file(GLOB fixture_entries RELATIVE "${PROJECT_FIXTURES_DIR}/${project_name}" "${PROJECT_FIXTURES_DIR}/${project_name}/*")
    foreach(fixture_entry IN LISTS fixture_entries)
        if (fixture_entry STREQUAL "bin")
            continue()
        endif()

        set(source_path "${PROJECT_FIXTURES_DIR}/${project_name}/${fixture_entry}")
        if (IS_DIRECTORY "${source_path}")
            file(COPY "${source_path}" DESTINATION "${destination}")
        else()
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_path}" "${destination}/${fixture_entry}"
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
                message(FATAL_ERROR "Failed to copy fixture entry '${fixture_entry}' for project '${project_name}'.")
            endif()
        endif()
    endforeach()
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

function(cli_run_split case_name stdout_var stderr_var result_var)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE case_result
        OUTPUT_VARIABLE case_stdout
        ERROR_VARIABLE case_stderr
    )

    set(${stdout_var} "${case_stdout}" PARENT_SCOPE)
    set(${stderr_var} "${case_stderr}" PARENT_SCOPE)
    set(${result_var} "${case_result}" PARENT_SCOPE)
endfunction()

function(cli_assert_empty case_name text_value label)
    if (NOT "${${text_value}}" STREQUAL "")
        message(FATAL_ERROR "CLI case '${case_name}' expected ${label} to be empty.\nOutput:\n${${text_value}}")
    endif()
endfunction()

function(cli_write_file path content)
    get_filename_component(parent_dir "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent_dir}")
    file(WRITE "${path}" "${content}")
endfunction()

function(cli_copy_file_or_fail case_name source_path destination_path description)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_path}" "${destination_path}"
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
        message(FATAL_ERROR "CLI case '${case_name}' failed while copying ${description}.")
    endif()
endfunction()

function(cli_prepare_binary_module case_name project_dir source_rel output_name)
    set(temp_project_root "${CLI_SUITE_ROOT}/.prepare/${case_name}")
    set(temp_source_dir "${temp_project_root}/src")
    set(temp_binary_dir "${temp_project_root}/bin")
    set(temp_project_file "${temp_project_root}/${case_name}.zrp")
    set(temp_source_file "${temp_source_dir}/${output_name}.zr")
    set(final_output "${project_dir}/bin/${output_name}.zro")
    set(prepare_source "${project_dir}/${source_rel}")
    set(copy_all_outputs OFF)

    file(REMOVE_RECURSE "${temp_project_root}")
    file(MAKE_DIRECTORY "${temp_source_dir}")
    file(MAKE_DIRECTORY "${temp_binary_dir}")
    file(MAKE_DIRECTORY "${project_dir}/bin")
    if (IS_DIRECTORY "${prepare_source}")
        set(copy_all_outputs ON)
        file(REMOVE_RECURSE "${temp_source_dir}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${prepare_source}" "${temp_source_dir}"
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
            message(FATAL_ERROR "CLI case '${case_name}' failed while copying prepared source directory '${source_rel}'.")
        endif()
    else()
        file(COPY_FILE "${prepare_source}" "${temp_source_file}" ONLY_IF_DIFFERENT)
    endif()
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

    if (copy_all_outputs)
        file(GLOB prepared_outputs "${temp_binary_dir}/*.zro")
        foreach(prepared_output IN LISTS prepared_outputs)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${prepared_output}" "${project_dir}/bin"
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
                message(FATAL_ERROR "CLI case '${case_name}' failed while copying prepared binary module set.")
            endif()
        endforeach()
    else()
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

cli_case_matches_tier("smoke;core;stress" run_help)
if (run_help)
    message("---- help")
    cli_run_split("help" help_stdout help_stderr help_result "${CLI_EXE}" "--help")
    set(help_output "${help_stdout}${help_stderr}")
    cli_assert_success("help" help_result help_output)
    cli_assert_empty("help" help_stderr "stderr")
    cli_assert_contains("help" help_stdout "Usage:")
    cli_assert_contains("help" help_stdout "--compile <project.zrp>")
    cli_assert_contains("help" help_stdout "--intermediate")
    cli_assert_contains("help" help_stdout "--incremental")
    cli_assert_contains("help" help_stdout "--run")
endif()

cli_case_matches_tier("smoke;core;stress" run_help_alias)
if (run_help_alias)
    message("---- help_alias")
    cli_run_split("help_alias" help_alias_stdout help_alias_stderr help_alias_result "${CLI_EXE}" "-h")
    set(help_alias_output "${help_alias_stdout}${help_alias_stderr}")
    cli_assert_success("help_alias" help_alias_result help_alias_output)
    cli_assert_empty("help_alias" help_alias_stderr "stderr")
    cli_assert_contains("help_alias" help_alias_stdout "--project <project.zrp> -m <module>")
    cli_assert_contains("help_alias" help_alias_stdout "Use -- to stop CLI parsing")
endif()

cli_case_matches_tier("smoke;core;stress" run_help_question_alias)
if (run_help_question_alias)
    message("---- help_question_alias")
    cli_run_split("help_question_alias" help_question_stdout help_question_stderr help_question_result "${CLI_EXE}" "-?")
    set(help_question_output "${help_question_stdout}${help_question_stderr}")
    cli_assert_success("help_question_alias" help_question_result help_question_output)
    cli_assert_empty("help_question_alias" help_question_stderr "stderr")
    cli_assert_contains("help_question_alias" help_question_stdout "Main Modes:")
    cli_assert_contains("help_question_alias" help_question_stdout "-V | --version")
endif()

cli_case_matches_tier("smoke;core;stress" run_version)
if (run_version)
    message("---- version")
    cli_run_split("version" version_stdout version_stderr version_result "${CLI_EXE}" "-V")
    set(version_output "${version_stdout}${version_stderr}")
    cli_assert_success("version" version_result version_output)
    cli_assert_empty("version" version_stderr "stderr")
    cli_assert_matches("version" version_stdout "^[0-9]+\\.[0-9]+\\.[0-9]+-[^\\r\\n]+")
endif()

cli_case_matches_tier("smoke;core;stress" run_version_long)
if (run_version_long)
    message("---- version_long")
    cli_run_split("version_long" version_long_stdout version_long_stderr version_long_result "${CLI_EXE}" "--version")
    set(version_long_output "${version_long_stdout}${version_long_stderr}")
    cli_assert_success("version_long" version_long_result version_long_output)
    cli_assert_empty("version_long" version_long_stderr "stderr")
    cli_assert_matches("version_long" version_long_stdout "^[0-9]+\\.[0-9]+\\.[0-9]+-[^\\r\\n]+")
endif()

cli_case_matches_tier("smoke;core;stress" run_missing_project)
if (run_missing_project)
    message("---- run_missing_project")
    cli_run_split("run_missing_project" missing_project_stdout missing_project_stderr missing_project_result "${CLI_EXE}" "--run")
    set(missing_project_output "${missing_project_stdout}${missing_project_stderr}")
    cli_assert_failure("run_missing_project" missing_project_result missing_project_output)
    cli_assert_empty("run_missing_project" missing_project_stdout "stdout")
    cli_assert_contains("run_missing_project"
                        missing_project_stderr
                        "require --compile <project.zrp>")
endif()

cli_case_matches_tier("smoke;core;stress" run_positional)
if (run_positional)
    message("---- positional_run")
    cli_copy_fixture("hello_world" hello_world_dir)
    cli_run("positional_run" positional_output positional_result "${CLI_EXE}" "${hello_world_dir}/hello_world.zrp")
    cli_assert_success("positional_run" positional_result positional_output)
    cli_assert_contains("positional_run" positional_output "hello world")
endif()

cli_case_matches_tier("smoke;core;stress" run_positional_args)
if (run_positional_args)
    message("---- positional_run_with_passthrough")
    cli_copy_fixture("cli_args" cli_args_run_dir)
    cli_run("positional_run_with_passthrough"
            cli_args_run_output
            cli_args_run_result
            "${CLI_EXE}"
            "${cli_args_run_dir}/cli_args.zrp"
            "--"
            "alpha"
            "--debug")
    cli_assert_success("positional_run_with_passthrough" cli_args_run_result cli_args_run_output)
    cli_assert_contains("positional_run_with_passthrough" cli_args_run_output "CLI_ARGS_MAIN")
    cli_assert_contains("positional_run_with_passthrough"
                        cli_args_run_output
                        "main_arg0=${cli_args_run_dir}/cli_args.zrp")
    cli_assert_contains("positional_run_with_passthrough" cli_args_run_output "main_arg1=alpha")
    cli_assert_contains("positional_run_with_passthrough" cli_args_run_output "main_arg2=--debug")
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

cli_case_matches_tier("smoke;core;stress" run_compile_run_args)
if (run_compile_run_args)
    message("---- compile_run_with_passthrough")
    cli_copy_fixture("cli_args" cli_args_compile_run_dir)
    file(REMOVE_RECURSE "${cli_args_compile_run_dir}/bin")
    cli_run("compile_run_with_passthrough"
            cli_args_compile_run_output
            cli_args_compile_run_result
            "${CLI_EXE}"
            "--compile"
            "${cli_args_compile_run_dir}/cli_args.zrp"
            "--run"
            "--"
            "seed"
            "42")
    cli_assert_success("compile_run_with_passthrough" cli_args_compile_run_result cli_args_compile_run_output)
    cli_assert_contains("compile_run_with_passthrough" cli_args_compile_run_output "CLI_ARGS_MAIN")
    cli_assert_contains("compile_run_with_passthrough"
                        cli_args_compile_run_output
                        "main_arg0=${cli_args_compile_run_dir}/cli_args.zrp")
    cli_assert_contains("compile_run_with_passthrough" cli_args_compile_run_output "main_arg1=seed")
    cli_assert_contains("compile_run_with_passthrough" cli_args_compile_run_output "main_arg2=42")
    if (NOT EXISTS "${cli_args_compile_run_dir}/bin/main.zro")
        message(FATAL_ERROR "compile_run_with_passthrough did not create main.zro")
    endif()
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_run_default_binary)
if (run_compile_run_default_binary)
    message("---- compile_run_default_binary")
    cli_copy_fixture("hello_world" compile_default_binary_dir)
    file(REMOVE_RECURSE "${compile_default_binary_dir}/bin")
    cli_run("compile_run_default_binary"
            compile_default_binary_output
            compile_default_binary_result
            "${CLI_EXE}"
            "--compile"
            "${compile_default_binary_dir}/hello_world.zrp"
            "--run"
            "--emit-executed-via")
    cli_assert_success("compile_run_default_binary" compile_default_binary_result compile_default_binary_output)
    cli_assert_contains("compile_run_default_binary" compile_default_binary_output "hello world")
    cli_assert_contains("compile_run_default_binary" compile_default_binary_output "executed_via=binary")
endif()

cli_case_matches_tier("smoke;core;stress" run_project_module)
if (run_project_module)
    message("---- project_module_run")
    cli_copy_fixture("cli_args" cli_args_module_dir)
    cli_run("project_module_run"
            cli_args_module_output
            cli_args_module_result
            "${CLI_EXE}"
            "--project"
            "${cli_args_module_dir}/cli_args.zrp"
            "-m"
            "tools.seed"
            "--execution-mode"
            "binary"
            "--"
            "foo"
            "bar")
    cli_assert_success("project_module_run" cli_args_module_result cli_args_module_output)
    cli_assert_contains("project_module_run" cli_args_module_output "CLI_ARGS_MODULE")
    cli_assert_contains("project_module_run" cli_args_module_output "module_arg0=tools.seed")
    cli_assert_contains("project_module_run" cli_args_module_output "module_arg1=foo")
    cli_assert_contains("project_module_run" cli_args_module_output "module_arg2=bar")
endif()

cli_case_matches_tier("smoke;core;stress" run_project_module_interp)
if (run_project_module_interp)
    message("---- project_module_interp_run")
    cli_copy_fixture("cli_args" cli_args_module_interp_dir)
    cli_run("project_module_interp_run"
            cli_args_module_interp_output
            cli_args_module_interp_result
            "${CLI_EXE}"
            "--project"
            "${cli_args_module_interp_dir}/cli_args.zrp"
            "-m"
            "tools.seed"
            "--"
            "interp")
    cli_assert_success("project_module_interp_run" cli_args_module_interp_result cli_args_module_interp_output)
    cli_assert_contains("project_module_interp_run" cli_args_module_interp_output "CLI_ARGS_MODULE")
    cli_assert_contains("project_module_interp_run" cli_args_module_interp_output "module_arg0=tools.seed")
    cli_assert_contains("project_module_interp_run" cli_args_module_interp_output "module_arg1=interp")
endif()

cli_case_matches_tier("smoke;core;stress" run_project_module_unknown_execution_mode)
if (run_project_module_unknown_execution_mode)
    message("---- project_module_unknown_execution_mode")
    cli_copy_fixture("cli_args" cli_args_module_unknown_mode_dir)
    cli_run_split("project_module_unknown_execution_mode"
                  cli_args_module_unknown_mode_stdout
                  cli_args_module_unknown_mode_stderr
                  cli_args_module_unknown_mode_result
                  "${CLI_EXE}"
                  "--project"
                  "${cli_args_module_unknown_mode_dir}/cli_args.zrp"
                  "-m"
                  "tools.seed"
                  "--execution-mode"
                  "aot_c")
    set(cli_args_module_unknown_mode_output "${cli_args_module_unknown_mode_stdout}${cli_args_module_unknown_mode_stderr}")
    cli_assert_failure("project_module_unknown_execution_mode" cli_args_module_unknown_mode_result cli_args_module_unknown_mode_output)
    cli_assert_empty("project_module_unknown_execution_mode" cli_args_module_unknown_mode_stdout "stdout")
    cli_assert_contains("project_module_unknown_execution_mode" cli_args_module_unknown_mode_stderr "Unknown execution mode: aot_c")
endif()

cli_case_matches_tier("smoke;core;stress" run_inline_code)
if (run_inline_code)
    message("---- inline_code_run")
    set(cli_inline_code [=[
var system = %import("zr.system");
var index = 0;
for (var item in system.process.arguments) {
    if (index == 0) {
        system.console.printLine("inline_arg0=" + item);
    } else if (index == 1) {
        system.console.printLine("inline_arg1=" + item);
    } else if (index == 2) {
        system.console.printLine("inline_arg2=" + item);
    }
    index = index + 1;
}
return index;
]=])
    string(REPLACE ";" "\\;" cli_inline_code_escaped "${cli_inline_code}")
    cli_run("inline_code_run"
            cli_inline_output
            cli_inline_result
            "${CLI_EXE}"
            "-c"
            "${cli_inline_code_escaped}"
            "--"
            "foo"
            "bar")
    cli_assert_success("inline_code_run" cli_inline_result cli_inline_output)
    cli_assert_contains("inline_code_run" cli_inline_output "inline_arg0=-c")
    cli_assert_contains("inline_code_run" cli_inline_output "inline_arg1=foo")
    cli_assert_contains("inline_code_run" cli_inline_output "inline_arg2=bar")
endif()

cli_case_matches_tier("smoke;core;stress" run_inline_code_eval_alias)
if (run_inline_code_eval_alias)
    message("---- inline_code_eval_alias")
    set(cli_inline_eval_code [=[
var system = %import("zr.system");
var index = 0;
for (var item in system.process.arguments) {
    if (index == 0) {
        system.console.printLine("inline_eval_arg0=" + item);
    } else if (index == 1) {
        system.console.printLine("inline_eval_arg1=" + item);
    }
    index = index + 1;
}
return index;
]=])
    string(REPLACE ";" "\\;" cli_inline_eval_code_escaped "${cli_inline_eval_code}")
    cli_run("inline_code_eval_alias"
            cli_inline_eval_output
            cli_inline_eval_result
            "${CLI_EXE}"
            "-e"
            "${cli_inline_eval_code_escaped}"
            "--"
            "foo")
    cli_assert_success("inline_code_eval_alias" cli_inline_eval_result cli_inline_eval_output)
    cli_assert_contains("inline_code_eval_alias" cli_inline_eval_output "inline_eval_arg0=-e")
    cli_assert_contains("inline_code_eval_alias" cli_inline_eval_output "inline_eval_arg1=foo")
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_passthrough_error)
if (run_compile_passthrough_error)
    message("---- compile_passthrough_error")
    cli_copy_fixture("cli_args" cli_args_compile_only_dir)
    cli_run_split("compile_passthrough_error"
                  compile_passthrough_stdout
                  compile_passthrough_stderr
                  compile_passthrough_result
                  "${CLI_EXE}"
                  "--compile"
                  "${cli_args_compile_only_dir}/cli_args.zrp"
                  "--"
                  "arg1")
    set(compile_passthrough_output "${compile_passthrough_stdout}${compile_passthrough_stderr}")
    cli_assert_failure("compile_passthrough_error" compile_passthrough_result compile_passthrough_output)
    cli_assert_empty("compile_passthrough_error" compile_passthrough_stdout "stdout")
    cli_assert_contains("compile_passthrough_error" compile_passthrough_stderr "active run path")
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

cli_case_matches_tier("smoke;core;stress" run_decorator_import_recursive)
if (run_decorator_import_recursive)
    message("---- decorator_import_compile_recursive_and_run")
    cli_copy_fixture("decorator_import" decorator_import_dir)
    file(REMOVE_RECURSE "${decorator_import_dir}/bin")
    cli_run("decorator_import_compile_recursive_and_run"
            decorator_import_output
            decorator_import_result
            "${CLI_EXE}"
            "--compile"
            "${decorator_import_dir}/decorator_import.zrp"
            "--intermediate"
            "--run")
    cli_assert_success("decorator_import_compile_recursive_and_run" decorator_import_result decorator_import_output)
    if (NOT EXISTS "${decorator_import_dir}/bin/main.zro")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create main.zro")
    endif()
    if (NOT EXISTS "${decorator_import_dir}/bin/decorated_user.zro")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create decorated_user.zro")
    endif()
    if (NOT EXISTS "${decorator_import_dir}/bin/main.zri")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create main.zri")
    endif()
    if (NOT EXISTS "${decorator_import_dir}/bin/decorated_user.zri")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create decorated_user.zri")
    endif()
    if (NOT EXISTS "${decorator_import_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create decorators.zro")
    endif()
    if (NOT EXISTS "${decorator_import_dir}/bin/decorators.zri")
        message(FATAL_ERROR "decorator_import_compile_recursive_and_run did not create decorators.zri")
    endif()
    cli_assert_contains("decorator_import_compile_recursive_and_run" decorator_import_output "31")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_import_binary)
if (run_decorator_import_binary)
    message("---- decorator_import_binary_run")
    cli_copy_fixture("decorator_import_binary" decorator_import_binary_dir)
    file(REMOVE_RECURSE "${decorator_import_binary_dir}/bin")
    cli_prepare_binary_module("decorator_import_binary_prepare"
                              "${decorator_import_binary_dir}"
                              "fixtures/decorated_user_module"
                              "decorated_user")
    if (NOT EXISTS "${decorator_import_binary_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_import_binary_run did not prepare decorators.zro")
    endif()
    cli_run("decorator_import_binary_run"
            decorator_import_binary_output
            decorator_import_binary_result
            "${CLI_EXE}"
            "${decorator_import_binary_dir}/decorator_import_binary.zrp")
    cli_assert_success("decorator_import_binary_run" decorator_import_binary_result decorator_import_binary_output)
    cli_assert_contains("decorator_import_binary_run" decorator_import_binary_output "31")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_compile_time_deep_import_recursive)
if (run_decorator_compile_time_deep_import_recursive)
    message("---- decorator_compile_time_deep_import_compile_recursive_and_run")
    cli_copy_fixture("decorator_compile_time_deep_import" decorator_compile_time_deep_import_dir)
    file(REMOVE_RECURSE "${decorator_compile_time_deep_import_dir}/bin")
    cli_run("decorator_compile_time_deep_import_compile_recursive_and_run"
            decorator_compile_time_deep_import_output
            decorator_compile_time_deep_import_result
            "${CLI_EXE}"
            "--compile"
            "${decorator_compile_time_deep_import_dir}/decorator_compile_time_deep_import.zrp"
            "--intermediate"
            "--run")
    cli_assert_success("decorator_compile_time_deep_import_compile_recursive_and_run"
                       decorator_compile_time_deep_import_result
                       decorator_compile_time_deep_import_output)
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/main.zro")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create main.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/decorated_user.zro")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create decorated_user.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create decorators.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/main.zri")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create main.zri")
    endif()
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/decorated_user.zri")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create decorated_user.zri")
    endif()
    if (NOT EXISTS "${decorator_compile_time_deep_import_dir}/bin/decorators.zri")
        message(FATAL_ERROR "decorator_compile_time_deep_import_compile_recursive_and_run did not create decorators.zri")
    endif()
    cli_assert_contains("decorator_compile_time_deep_import_compile_recursive_and_run"
                        decorator_compile_time_deep_import_output
                        "43")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_compile_time_import_binary)
if (run_decorator_compile_time_import_binary)
    message("---- decorator_compile_time_import_binary_run")
    cli_copy_fixture("decorator_compile_time_import_binary" decorator_compile_time_import_binary_dir)
    file(REMOVE_RECURSE "${decorator_compile_time_import_binary_dir}/bin")
    cli_prepare_binary_module("decorator_compile_time_import_binary_prepare"
                              "${decorator_compile_time_import_binary_dir}"
                              "fixtures/provider_module"
                              "provider")
    if (NOT EXISTS "${decorator_compile_time_import_binary_dir}/bin/provider.zro")
        message(FATAL_ERROR "decorator_compile_time_import_binary_run did not prepare provider.zro")
    endif()
    cli_run("decorator_compile_time_import_binary_run"
            decorator_compile_time_import_binary_output
            decorator_compile_time_import_binary_result
            "${CLI_EXE}"
            "${decorator_compile_time_import_binary_dir}/decorator_compile_time_import_binary.zrp")
    cli_assert_success("decorator_compile_time_import_binary_run"
                       decorator_compile_time_import_binary_result
                       decorator_compile_time_import_binary_output)
    cli_assert_contains("decorator_compile_time_import_binary_run"
                        decorator_compile_time_import_binary_output
                        "62")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_compile_time_deep_import_binary)
if (run_decorator_compile_time_deep_import_binary)
    message("---- decorator_compile_time_deep_import_binary_run")
    cli_copy_fixture("decorator_compile_time_deep_import_binary" decorator_compile_time_deep_import_binary_dir)
    file(REMOVE_RECURSE "${decorator_compile_time_deep_import_binary_dir}/bin")
    cli_prepare_binary_module("decorator_compile_time_deep_import_binary_prepare"
                              "${decorator_compile_time_deep_import_binary_dir}"
                              "fixtures/decorated_user_module"
                              "decorated_user")
    if (NOT EXISTS "${decorator_compile_time_deep_import_binary_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_compile_time_deep_import_binary_run did not prepare decorators.zro")
    endif()
    cli_run("decorator_compile_time_deep_import_binary_run"
            decorator_compile_time_deep_import_binary_output
            decorator_compile_time_deep_import_binary_result
            "${CLI_EXE}"
            "${decorator_compile_time_deep_import_binary_dir}/decorator_compile_time_deep_import_binary.zrp")
    cli_assert_success("decorator_compile_time_deep_import_binary_run"
                       decorator_compile_time_deep_import_binary_result
                       decorator_compile_time_deep_import_binary_output)
    cli_assert_contains("decorator_compile_time_deep_import_binary_run"
                        decorator_compile_time_deep_import_binary_output
                        "43")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_compile_time_provider_import_recursive)
if (run_decorator_compile_time_provider_import_recursive)
    message("---- decorator_compile_time_provider_import_compile_recursive_and_run")
    cli_copy_fixture("decorator_compile_time_provider_import" decorator_compile_time_provider_import_dir)
    file(REMOVE_RECURSE "${decorator_compile_time_provider_import_dir}/bin")
    cli_run("decorator_compile_time_provider_import_compile_recursive_and_run"
            decorator_compile_time_provider_import_output
            decorator_compile_time_provider_import_result
            "${CLI_EXE}"
            "--compile"
            "${decorator_compile_time_provider_import_dir}/decorator_compile_time_provider_import.zrp"
            "--intermediate"
            "--run")
    cli_assert_success("decorator_compile_time_provider_import_compile_recursive_and_run"
                       decorator_compile_time_provider_import_result
                       decorator_compile_time_provider_import_output)
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/main.zro")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create main.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/decorated_user.zro")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create decorated_user.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create decorators.zro")
    endif()
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/main.zri")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create main.zri")
    endif()
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/decorated_user.zri")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create decorated_user.zri")
    endif()
    if (NOT EXISTS "${decorator_compile_time_provider_import_dir}/bin/decorators.zri")
        message(FATAL_ERROR "decorator_compile_time_provider_import_compile_recursive_and_run did not create decorators.zri")
    endif()
    cli_assert_contains("decorator_compile_time_provider_import_compile_recursive_and_run"
                        decorator_compile_time_provider_import_output
                        "71")
endif()

cli_case_matches_tier("smoke;core;stress" run_decorator_compile_time_provider_import_binary)
if (run_decorator_compile_time_provider_import_binary)
    message("---- decorator_compile_time_provider_import_binary_run")
    cli_copy_fixture("decorator_compile_time_provider_import_binary" decorator_compile_time_provider_import_binary_dir)
    file(REMOVE_RECURSE "${decorator_compile_time_provider_import_binary_dir}/bin")
    cli_prepare_binary_module("decorator_compile_time_provider_import_binary_prepare"
                              "${decorator_compile_time_provider_import_binary_dir}"
                              "fixtures/decorated_user_module"
                              "decorated_user")
    if (NOT EXISTS "${decorator_compile_time_provider_import_binary_dir}/bin/decorators.zro")
        message(FATAL_ERROR "decorator_compile_time_provider_import_binary_run did not prepare decorators.zro")
    endif()
    cli_run("decorator_compile_time_provider_import_binary_run"
            decorator_compile_time_provider_import_binary_output
            decorator_compile_time_provider_import_binary_result
            "${CLI_EXE}"
            "${decorator_compile_time_provider_import_binary_dir}/decorator_compile_time_provider_import_binary.zrp")
    cli_assert_success("decorator_compile_time_provider_import_binary_run"
                       decorator_compile_time_provider_import_binary_result
                       decorator_compile_time_provider_import_binary_output)
    cli_assert_contains("decorator_compile_time_provider_import_binary_run"
                        decorator_compile_time_provider_import_binary_output
                        "71")
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

cli_case_matches_tier("core;stress" run_compile_run_incremental_binary_parity)
if (run_compile_run_incremental_binary_parity)
    message("---- compile_run_incremental_binary_parity")
    cli_copy_fixture("import_basic" compile_run_incremental_dir)
    file(REMOVE_RECURSE "${compile_run_incremental_dir}/bin")

    cli_run("compile_run_incremental_first"
            compile_run_incremental_first_output
            compile_run_incremental_first_result
            "${CLI_EXE}"
            "--compile"
            "${compile_run_incremental_dir}/import_basic.zrp"
            "--run"
            "--incremental"
            "--emit-executed-via")
    cli_assert_success("compile_run_incremental_first"
                       compile_run_incremental_first_result
                       compile_run_incremental_first_output)
    cli_assert_contains("compile_run_incremental_first"
                        compile_run_incremental_first_output
                        "compile summary: compiled=2 skipped=0 removed=0")
    cli_assert_contains("compile_run_incremental_first"
                        compile_run_incremental_first_output
                        "hello from import")
    cli_assert_contains("compile_run_incremental_first"
                        compile_run_incremental_first_output
                        "executed_via=binary")

    cli_write_file("${compile_run_incremental_dir}/src/greet.zr"
                   "pub var greet = () => {\n    return \"hello from import v2\";\n};\n")
    cli_run("compile_run_incremental_changed_dependency"
            compile_run_incremental_changed_output
            compile_run_incremental_changed_result
            "${CLI_EXE}"
            "--compile"
            "${compile_run_incremental_dir}/import_basic.zrp"
            "--run"
            "--incremental"
            "--emit-executed-via")
    cli_assert_success("compile_run_incremental_changed_dependency"
                       compile_run_incremental_changed_result
                       compile_run_incremental_changed_output)
    cli_assert_contains("compile_run_incremental_changed_dependency"
                        compile_run_incremental_changed_output
                        "compile summary: compiled=2 skipped=0 removed=0")
    cli_assert_contains("compile_run_incremental_changed_dependency"
                        compile_run_incremental_changed_output
                        "hello from import v2")
    cli_assert_contains("compile_run_incremental_changed_dependency"
                        compile_run_incremental_changed_output
                        "executed_via=binary")

    cli_run("compile_run_incremental_noop"
            compile_run_incremental_noop_output
            compile_run_incremental_noop_result
            "${CLI_EXE}"
            "--compile"
            "${compile_run_incremental_dir}/import_basic.zrp"
            "--run"
            "--incremental"
            "--emit-executed-via")
    cli_assert_success("compile_run_incremental_noop"
                       compile_run_incremental_noop_result
                       compile_run_incremental_noop_output)
    cli_assert_contains("compile_run_incremental_noop"
                        compile_run_incremental_noop_output
                        "compile summary: compiled=0 skipped=2 removed=0")
    cli_assert_contains("compile_run_incremental_noop"
                        compile_run_incremental_noop_output
                        "hello from import v2")
    cli_assert_contains("compile_run_incremental_noop"
                        compile_run_incremental_noop_output
                        "executed_via=binary")
endif()

cli_case_matches_tier("core;stress" run_compile_run_incremental_intermediate_toggle_cleanup)
if (run_compile_run_incremental_intermediate_toggle_cleanup)
    message("---- compile_run_incremental_intermediate_toggle_cleanup")
    cli_copy_fixture("import_basic" compile_run_incremental_intermediate_dir)
    file(REMOVE_RECURSE "${compile_run_incremental_intermediate_dir}/bin")

    cli_run("compile_run_incremental_intermediate_first"
            compile_run_incremental_intermediate_first_output
            compile_run_incremental_intermediate_first_result
            "${CLI_EXE}"
            "--compile"
            "${compile_run_incremental_intermediate_dir}/import_basic.zrp"
            "--run"
            "--incremental"
            "--intermediate"
            "--emit-executed-via")
    cli_assert_success("compile_run_incremental_intermediate_first"
                       compile_run_incremental_intermediate_first_result
                       compile_run_incremental_intermediate_first_output)
    cli_assert_contains("compile_run_incremental_intermediate_first"
                        compile_run_incremental_intermediate_first_output
                        "compile summary: compiled=2 skipped=0 removed=0")
    cli_assert_contains("compile_run_incremental_intermediate_first"
                        compile_run_incremental_intermediate_first_output
                        "hello from import")
    cli_assert_contains("compile_run_incremental_intermediate_first"
                        compile_run_incremental_intermediate_first_output
                        "executed_via=binary")
    if (NOT EXISTS "${compile_run_incremental_intermediate_dir}/bin/main.zri")
        message(FATAL_ERROR "compile_run_incremental_intermediate_first did not create main.zri")
    endif()
    if (NOT EXISTS "${compile_run_incremental_intermediate_dir}/bin/greet.zri")
        message(FATAL_ERROR "compile_run_incremental_intermediate_first did not create greet.zri")
    endif()

    cli_run("compile_run_incremental_intermediate_second"
            compile_run_incremental_intermediate_second_output
            compile_run_incremental_intermediate_second_result
            "${CLI_EXE}"
            "--compile"
            "${compile_run_incremental_intermediate_dir}/import_basic.zrp"
            "--run"
            "--incremental"
            "--emit-executed-via")
    cli_assert_success("compile_run_incremental_intermediate_second"
                       compile_run_incremental_intermediate_second_result
                       compile_run_incremental_intermediate_second_output)
    cli_assert_contains("compile_run_incremental_intermediate_second"
                        compile_run_incremental_intermediate_second_output
                        "compile summary: compiled=0 skipped=2 removed=0")
    cli_assert_contains("compile_run_incremental_intermediate_second"
                        compile_run_incremental_intermediate_second_output
                        "hello from import")
    cli_assert_contains("compile_run_incremental_intermediate_second"
                        compile_run_incremental_intermediate_second_output
                        "executed_via=binary")
    if (EXISTS "${compile_run_incremental_intermediate_dir}/bin/main.zri")
        message(FATAL_ERROR "compile_run_incremental_intermediate_second did not remove stale main.zri")
    endif()
    if (EXISTS "${compile_run_incremental_intermediate_dir}/bin/greet.zri")
        message(FATAL_ERROR "compile_run_incremental_intermediate_second did not remove stale greet.zri")
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

cli_case_matches_tier("smoke;core;stress" run_interactive_after_run)
if (run_interactive_after_run)
    message("---- interactive_after_run")
    cli_copy_fixture("cli_args" cli_args_interactive_dir)
    set(interactive_after_run_input_file "${CLI_SUITE_ROOT}/interactive_after_run_input.txt")
    file(WRITE "${interactive_after_run_input_file}"
        "var system = %import(\"zr.system\");\n"
        "var first = \"\";\n"
        "for (var item in system.process.arguments) {\n"
        "    first = item;\n"
        "}\n"
        "return first;\n"
        "\n"
        ":quit\n")
    execute_process(
        COMMAND "${CLI_EXE}" "${cli_args_interactive_dir}/cli_args.zrp" "-i" "--" "tail"
        INPUT_FILE "${interactive_after_run_input_file}"
        RESULT_VARIABLE interactive_after_run_result
        OUTPUT_VARIABLE interactive_after_run_stdout
        ERROR_VARIABLE interactive_after_run_stderr
    )
    set(interactive_after_run_output "${interactive_after_run_stdout}${interactive_after_run_stderr}")
    if (NOT interactive_after_run_result EQUAL 0)
        message(FATAL_ERROR "interactive_after_run failed with exit code ${interactive_after_run_result}.\nOutput:\n${interactive_after_run_output}")
    endif()
    cli_assert_contains("interactive_after_run" interactive_after_run_output "CLI_ARGS_MAIN")
    cli_assert_contains("interactive_after_run"
                        interactive_after_run_output
                        "main_arg0=${cli_args_interactive_dir}/cli_args.zrp")
    cli_assert_contains("interactive_after_run" interactive_after_run_output "main_arg1=tail")
    cli_assert_contains("interactive_after_run" interactive_after_run_output "ZR VM REPL")
    cli_assert_contains("interactive_after_run" interactive_after_run_output "<repl>")
endif()

cli_case_matches_tier("smoke;core;stress" run_compile_interactive_after_run)
if (run_compile_interactive_after_run)
    message("---- compile_interactive_after_run")
    cli_copy_fixture("cli_args" cli_args_compile_interactive_dir)
    set(compile_interactive_after_run_input_file "${CLI_SUITE_ROOT}/compile_interactive_after_run_input.txt")
    file(WRITE "${compile_interactive_after_run_input_file}"
        "var system = %import(\"zr.system\");\n"
        "var first = \"\";\n"
        "for (var item in system.process.arguments) {\n"
        "    first = item;\n"
        "}\n"
        "return first;\n"
        "\n"
        ":quit\n")
    execute_process(
        COMMAND "${CLI_EXE}" "--compile" "${cli_args_compile_interactive_dir}/cli_args.zrp" "--run" "--emit-executed-via" "-i" "--" "tail"
        INPUT_FILE "${compile_interactive_after_run_input_file}"
        RESULT_VARIABLE compile_interactive_after_run_result
        OUTPUT_VARIABLE compile_interactive_after_run_stdout
        ERROR_VARIABLE compile_interactive_after_run_stderr
    )
    set(compile_interactive_after_run_output "${compile_interactive_after_run_stdout}${compile_interactive_after_run_stderr}")
    if (NOT compile_interactive_after_run_result EQUAL 0)
        message(FATAL_ERROR "compile_interactive_after_run failed with exit code ${compile_interactive_after_run_result}.\nOutput:\n${compile_interactive_after_run_output}")
    endif()
    cli_assert_contains("compile_interactive_after_run" compile_interactive_after_run_output "CLI_ARGS_MAIN")
    cli_assert_contains("compile_interactive_after_run"
                        compile_interactive_after_run_output
                        "main_arg0=${cli_args_compile_interactive_dir}/cli_args.zrp")
    cli_assert_contains("compile_interactive_after_run" compile_interactive_after_run_output "main_arg1=tail")
    cli_assert_contains("compile_interactive_after_run" compile_interactive_after_run_output "executed_via=binary")
    cli_assert_contains("compile_interactive_after_run" compile_interactive_after_run_output "ZR VM REPL")
    cli_assert_contains("compile_interactive_after_run" compile_interactive_after_run_output "<repl>")
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
    cli_assert_empty("repl_native_import" repl_native_import_stderr "stderr")
    string(FIND "${repl_native_import_stdout}" "xxx" repl_native_import_index)
    if (repl_native_import_index EQUAL -1)
        message(FATAL_ERROR "repl_native_import did not print console output to stdout.\nOutput:\n${repl_native_import_output}")
    endif()
endif()

cli_case_matches_tier("core;stress" run_repl_runtime_error)
if (run_repl_runtime_error)
    message("---- repl_runtime_error")
    set(repl_runtime_error_input_file "${CLI_SUITE_ROOT}/repl_runtime_error_input.txt")
    file(WRITE "${repl_runtime_error_input_file}"
        "var system = %import(\"zr.system\");\n"
        "system.s.console.print(\"xxx\");\n"
        "\n"
        ":quit\n")
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
    string(FIND "${repl_runtime_error_stderr}" "GET_MEMBER: missing member 's'" repl_runtime_error_index)
    if (repl_runtime_error_index EQUAL -1)
        message(FATAL_ERROR "repl_runtime_error did not surface the runtime error on stderr.\nOutput:\n${repl_runtime_error_output}")
    endif()
    string(FIND "${repl_runtime_error_stdout}" "GET_MEMBER: missing member 's'" repl_runtime_error_stdout_index)
    if (NOT repl_runtime_error_stdout_index EQUAL -1)
        message(FATAL_ERROR "repl_runtime_error unexpectedly wrote the runtime error to stdout.\nOutput:\n${repl_runtime_error_output}")
    endif()
endif()
