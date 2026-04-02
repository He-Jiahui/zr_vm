include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

add_subdirectory(third_party)

set(ZR_VM_TESTS_SOURCE_DIR_NORMALIZED "${CMAKE_SOURCE_DIR}/tests")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}" ZR_VM_TESTS_SOURCE_DIR_NORMALIZED)

set(ZR_VM_TESTS_BINARY_DIR_NORMALIZED "${CMAKE_BINARY_DIR}/tests_generated")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}" ZR_VM_TESTS_BINARY_DIR_NORMALIZED)
file(MAKE_DIRECTORY "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}")

set(ZR_VM_TESTS_HARNESS_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/harness/runtime_support.c
        ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
)

set(ZR_VM_SUITE_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_executable_suite.cmake)
set(ZR_VM_PROJECTS_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_projects_suite.cmake)
find_program(ZR_VM_NODE_EXECUTABLE node)

function(zr_vm_apply_common_test_settings target_name)
    target_include_directories(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )

    target_compile_definitions(${target_name} PRIVATE
            UNITY_INCLUDE_DOUBLE
            UNITY_DOUBLE_PRECISION=1e-6
            UNITY_USE_FLUSH_STDOUT
            ZR_VM_TESTS_SOURCE_DIR="${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}"
            ZR_VM_TESTS_BINARY_DIR="${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
    )
endfunction()

function(zr_vm_add_unity_test_target target_name)
    add_executable(${target_name}
            ${ARGN}
            ${ZR_VM_TESTS_HARNESS_SOURCES}
    )
    zr_vm_apply_common_test_settings(${target_name})
    zr_link_third_party_for_target(${target_name} "zr_unity")
endfunction()

function(zr_vm_add_support_target target_name)
    add_executable(${target_name}
            ${ARGN}
            ${ZR_VM_TESTS_HARNESS_SOURCES}
    )
    zr_vm_apply_common_test_settings(${target_name})
endfunction()

function(zr_vm_link_core target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE zr_vm_core_shared)
    else ()
        target_link_libraries(${target_name} PRIVATE zr_vm_core_static)
    endif ()
endfunction()

function(zr_vm_link_parser_core target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    else ()
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_static zr_vm_core_static)
    endif ()
endfunction()

function(zr_vm_link_parser_core_plus_library target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_container_shared
                zr_vm_lib_ffi_shared
        )
    else ()
        target_link_libraries(${target_name} PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_container_static
                zr_vm_lib_ffi_static
        )
    endif ()
endfunction()

function(zr_vm_link_language_server target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE
                zr_vm_language_server_shared
                zr_vm_parser_shared
                zr_vm_core_shared
        )
    else ()
        target_link_libraries(${target_name} PRIVATE
                zr_vm_language_server_static
                zr_vm_parser_static
                zr_vm_core_static
        )
    endif ()
endfunction()

if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(
            zr_vm_gc_test
            ${CMAKE_SOURCE_DIR}/tests/gc/gc_tests.c
            ${CMAKE_SOURCE_DIR}/tests/gc/gc_test_utils.c
    )
    target_include_directories(zr_vm_gc_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_gc_test)

    zr_vm_add_unity_test_target(
            zr_vm_instructions_test
            ${CMAKE_SOURCE_DIR}/tests/instructions/test_instructions.c
    )
    target_include_directories(zr_vm_instructions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_instructions_test)

    zr_vm_add_unity_test_target(
            zr_vm_meta_test
            ${CMAKE_SOURCE_DIR}/tests/meta/test_meta.c
    )
    target_include_directories(zr_vm_meta_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_meta_test)

    zr_vm_add_unity_test_target(
            zr_vm_native_closure_value_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_native_closure_value.c
    )
    target_include_directories(zr_vm_native_closure_value_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_native_closure_value_test)
endif ()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_unity_test_target(
            zr_vm_parser_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_parser.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_extern.c
    )
    target_include_directories(zr_vm_parser_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_parser_test)

    zr_vm_add_unity_test_target(
            zr_vm_instruction_execution_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_instruction_execution.c
    )
    target_include_directories(zr_vm_instruction_execution_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_instruction_execution_test)

    zr_vm_add_unity_test_target(
            zr_vm_type_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_type_inference.c
    )
    target_include_directories(zr_vm_type_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_type_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_features_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_features.c
    )
    target_include_directories(zr_vm_compiler_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_compiler_features_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_regressions_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_regressions.c
    )
    target_include_directories(zr_vm_compiler_regressions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_compiler_regressions_test)

    zr_vm_add_unity_test_target(
            zr_vm_char_and_type_cast_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_char_and_type_cast.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_char_and_type_cast_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_char_and_type_cast_test)

    zr_vm_add_unity_test_target(
            zr_vm_lexer_parser_compiler_execution_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_lexer_parser_compiler_execution.c
    )
    target_include_directories(zr_vm_lexer_parser_compiler_execution_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_lexer_parser_compiler_execution_test)

    zr_vm_add_unity_test_target(
            zr_vm_prototype_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_prototype.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_prototype_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_prototype_test)

    zr_vm_add_unity_test_target(
            zr_vm_const_keyword_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_const_keyword.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_const_keyword_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_const_keyword_test)

    zr_vm_add_unity_test_target(
            zr_vm_exceptions_test
            ${CMAKE_SOURCE_DIR}/tests/exceptions/test_exceptions.c
    )
    target_include_directories(zr_vm_exceptions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_exceptions_test)

    zr_vm_add_unity_test_target(
            zr_vm_module_system_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_module_system.c
    )
    target_include_directories(zr_vm_module_system_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_module_system_test)

    zr_vm_add_unity_test_target(
            zr_vm_named_arguments_test
            ${CMAKE_SOURCE_DIR}/tests/function/test_named_arguments.c
    )
    target_include_directories(zr_vm_named_arguments_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_named_arguments_test)

    zr_vm_add_unity_test_target(
            zr_vm_compile_time_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_compile_time_execution.c
    )
    target_include_directories(zr_vm_compile_time_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_compile_time_test)

    zr_vm_add_unity_test_target(
            zr_vm_scripts_test
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_comprehensive.c
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_utils.c
    )
    target_include_directories(zr_vm_scripts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/scripts
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_scripts_test)
    zr_link_third_party_for_target(zr_vm_scripts_test "zr_c_json")

    zr_vm_add_unity_test_target(
            zr_vm_artifact_golden_test
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_artifact_golden.c
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_utils.c
    )
    target_include_directories(zr_vm_artifact_golden_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/scripts
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_artifact_golden_test)
    zr_link_third_party_for_target(zr_vm_artifact_golden_test "zr_c_json")

    zr_vm_add_unity_test_target(
            zr_vm_container_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_runtime.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_container_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_metadata.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_container_type_inference_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_type_inference.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )

    foreach (container_target
            zr_vm_container_runtime_test
            zr_vm_container_metadata_test
            zr_vm_container_type_inference_test)
        target_include_directories(${container_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/container
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
        )
        zr_vm_link_parser_core_plus_library(${container_target})
    endforeach ()
endif ()

if ((TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static) AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_support_target(
            zr_vm_language_server_symbol_table_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_symbol_table.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_reference_tracker_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_reference_tracker.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_semantic_analyzer_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_semantic_analyzer.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_incremental_parser_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_incremental_parser.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_interface_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_interface.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_project_features_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_project_features.c
    )

    foreach (language_server_target
            zr_vm_language_server_symbol_table_test
            zr_vm_language_server_reference_tracker_test
            zr_vm_language_server_semantic_analyzer_test
            zr_vm_language_server_incremental_parser_test
            zr_vm_language_server_lsp_interface_test
            zr_vm_language_server_lsp_project_features_test)
        target_include_directories(${language_server_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
        )
        zr_vm_link_language_server(${language_server_target})
    endforeach ()

    target_include_directories(zr_vm_language_server_semantic_analyzer_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )

    target_include_directories(zr_vm_language_server_lsp_interface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_lsp_project_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )

    target_compile_definitions(zr_vm_language_server_lsp_interface_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )
    target_compile_definitions(zr_vm_language_server_lsp_project_features_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
        )
    else ()
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
        )
    endif ()
endif ()

if (TARGET zr_vm_cli_executable AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_executable(
            zr_vm_project_fixture_builder
            ${CMAKE_SOURCE_DIR}/tests/projects/project_fixture_builder.c
    )
    target_include_directories(zr_vm_project_fixture_builder PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    zr_vm_link_parser_core(zr_vm_project_fixture_builder)

    add_executable(
            zr_vm_cli_args_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_args.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/command.c
    )
    target_include_directories(zr_vm_cli_args_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
endif ()

find_package(Threads REQUIRED)

add_library(
        zr_vm_ffi_fixture
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/ffi/ffi_fixture.c
)
target_include_directories(zr_vm_ffi_fixture PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/ffi
)
target_link_libraries(zr_vm_ffi_fixture PRIVATE Threads::Threads)
set_target_properties(zr_vm_ffi_fixture PROPERTIES
        OUTPUT_NAME zr_vm_ffi_fixture
)

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_unity_test_target(
            zr_vm_ffi_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_ffi_module.c
    )
    target_include_directories(zr_vm_ffi_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_compile_definitions(zr_vm_ffi_test PRIVATE
            ZR_VM_FFI_FIXTURE_PATH="$<TARGET_FILE:zr_vm_ffi_fixture>"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_test)
    add_dependencies(zr_vm_ffi_test zr_vm_ffi_fixture)

    zr_vm_add_support_target(
            zr_vm_ffi_probe
            ${CMAKE_SOURCE_DIR}/tests/ffi/debug_ffi_close_probe.c
    )
    target_include_directories(zr_vm_ffi_probe PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_compile_definitions(zr_vm_ffi_probe PRIVATE
            ZR_VM_FFI_FIXTURE_PATH="$<TARGET_FILE:zr_vm_ffi_fixture>"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_probe)
    add_dependencies(zr_vm_ffi_probe zr_vm_ffi_fixture)
endif ()

set(core_runtime_executables "")
if (TARGET zr_vm_gc_test AND TARGET zr_vm_instructions_test AND TARGET zr_vm_meta_test AND TARGET zr_vm_native_closure_value_test)
    set(core_runtime_executables
            "$<TARGET_FILE:zr_vm_gc_test>;$<TARGET_FILE:zr_vm_instructions_test>;$<TARGET_FILE:zr_vm_meta_test>;$<TARGET_FILE:zr_vm_native_closure_value_test>"
    )
    add_test(
            NAME core_runtime
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=core_runtime"
            "-DEXECUTABLES=${core_runtime_executables}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_pipeline_executables "")
if (TARGET zr_vm_parser_test AND
        TARGET zr_vm_instruction_execution_test AND
        TARGET zr_vm_type_inference_test AND
        TARGET zr_vm_compiler_features_test AND
        TARGET zr_vm_compiler_regressions_test AND
        TARGET zr_vm_char_and_type_cast_test AND
        TARGET zr_vm_lexer_parser_compiler_execution_test AND
        TARGET zr_vm_prototype_test AND
        TARGET zr_vm_const_keyword_test AND
        TARGET zr_vm_exceptions_test AND
        TARGET zr_vm_module_system_test AND
        TARGET zr_vm_named_arguments_test AND
        TARGET zr_vm_compile_time_test AND
        TARGET zr_vm_scripts_test)
    set(language_pipeline_executables
            "$<TARGET_FILE:zr_vm_parser_test>;$<TARGET_FILE:zr_vm_instruction_execution_test>;$<TARGET_FILE:zr_vm_type_inference_test>;$<TARGET_FILE:zr_vm_compiler_features_test>;$<TARGET_FILE:zr_vm_compiler_regressions_test>;$<TARGET_FILE:zr_vm_char_and_type_cast_test>;$<TARGET_FILE:zr_vm_lexer_parser_compiler_execution_test>;$<TARGET_FILE:zr_vm_prototype_test>;$<TARGET_FILE:zr_vm_const_keyword_test>;$<TARGET_FILE:zr_vm_exceptions_test>;$<TARGET_FILE:zr_vm_module_system_test>;$<TARGET_FILE:zr_vm_named_arguments_test>;$<TARGET_FILE:zr_vm_compile_time_test>;$<TARGET_FILE:zr_vm_scripts_test>"
    )
    add_test(
            NAME language_pipeline
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=language_pipeline"
            "-DEXECUTABLES=${language_pipeline_executables}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(container_executables "")
if (TARGET zr_vm_container_runtime_test AND
        TARGET zr_vm_container_metadata_test AND
        TARGET zr_vm_container_type_inference_test)
    set(container_executables
            "$<TARGET_FILE:zr_vm_container_runtime_test>;$<TARGET_FILE:zr_vm_container_metadata_test>;$<TARGET_FILE:zr_vm_container_type_inference_test>"
    )
    add_test(
            NAME containers
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=containers"
            "-DEXECUTABLES=${container_executables}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_server_executables "")
if (TARGET zr_vm_language_server_symbol_table_test AND
        TARGET zr_vm_language_server_reference_tracker_test AND
        TARGET zr_vm_language_server_semantic_analyzer_test AND
        TARGET zr_vm_language_server_incremental_parser_test AND
        TARGET zr_vm_language_server_lsp_interface_test AND
        TARGET zr_vm_language_server_lsp_project_features_test)
    set(language_server_executables
            "$<TARGET_FILE:zr_vm_language_server_symbol_table_test>;$<TARGET_FILE:zr_vm_language_server_reference_tracker_test>;$<TARGET_FILE:zr_vm_language_server_semantic_analyzer_test>;$<TARGET_FILE:zr_vm_language_server_incremental_parser_test>;$<TARGET_FILE:zr_vm_language_server_lsp_interface_test>;$<TARGET_FILE:zr_vm_language_server_lsp_project_features_test>"
    )
    add_test(
            NAME language_server
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=language_server"
            "-DEXECUTABLES=${language_server_executables}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_language_server_stdio AND ZR_VM_NODE_EXECUTABLE)
    add_test(
            NAME language_server_stdio_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_smoke.js
            $<TARGET_FILE:zr_vm_language_server_stdio>
    )
endif ()

if (TARGET zr_vm_project_fixture_builder AND TARGET zr_vm_cli_executable)
    add_test(
            NAME projects
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DFIXTURE_BUILDER_EXE=$<TARGET_FILE:zr_vm_project_fixture_builder>"
            "-DREPO_ROOT=${CMAKE_SOURCE_DIR}"
            "-DPROJECT_FIXTURES_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects"
            -P ${ZR_VM_PROJECTS_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_args_test)
    add_test(
            NAME cli_args
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_args"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_args_test>"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_executable)
    add_test(
            NAME cli_integration
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DPROJECT_FIXTURES_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects"
            "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_cli_suite.cmake
    )
endif ()

if (TARGET zr_vm_artifact_golden_test)
    add_test(
            NAME golden_regression
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=golden_regression"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_artifact_golden_test>"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

add_executable(zr_vm_test_runner ${CMAKE_SOURCE_DIR}/tests/test_runner.c)
