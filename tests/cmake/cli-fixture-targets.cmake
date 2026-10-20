if (TARGET zr_vm_cli_executable AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_executable(
            zr_vm_cli_args_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_args.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/command/command.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
    )
    target_include_directories(zr_vm_cli_args_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    zr_vm_link_core(zr_vm_cli_args_test)

    add_executable(
            zr_vm_cli_zrp_metadata_dump_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_zrp_metadata_dump.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
    )
    target_include_directories(zr_vm_cli_zrp_metadata_dump_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_cli_zrp_metadata_dump_test)
endif ()

if (TARGET zr_vm_reflection_type_surface_test)
    add_test(
            NAME reflection_type_surface
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=reflection_type_surface"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_reflection_type_surface_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_reflection_type_surface_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_reflection_type_surface_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_reflection_type_surface_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_reflection_type_stress_test)
    add_test(
            NAME reflection_type_stress
            COMMAND $<TARGET_FILE:zr_vm_reflection_type_stress_test>
    )
    set_tests_properties(reflection_type_stress PROPERTIES LABELS "stress")
endif ()

if (TARGET zr_vm_aot_c_reflection_construction_shared_library_test)
    add_test(
            NAME aot_c_reflection_construction_shared_library
            COMMAND $<TARGET_FILE:zr_vm_aot_c_reflection_construction_shared_library_test>
    )
    set_tests_properties(
            aot_c_reflection_construction_shared_library
            PROPERTIES LABELS "aot;reflection")
endif ()

if (TARGET zr_vm_call_spread_test)
    add_test(
            NAME call_spread
            COMMAND $<TARGET_FILE:zr_vm_call_spread_test>
    )
endif ()

if (TARGET zr_vm_cli_executable AND
        (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
        (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static) AND
        (TARGET zr_vm_library_shared OR TARGET zr_vm_library_static))
    zr_vm_add_unity_test_target(
            zr_vm_cli_project_incremental_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_project_incremental.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_cli_aot_writer_options_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_aot_writer_options.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_cli_aot_compacted_metadata_sidecar_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_aot_compacted_metadata_sidecar.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_cli_import_basic_fixture_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_import_basic_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_cli_zrm_fixture_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_zrm_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    target_include_directories(zr_vm_cli_import_basic_fixture_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
    )
    target_include_directories(zr_vm_cli_project_incremental_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    target_include_directories(zr_vm_cli_zrm_fixture_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    target_include_directories(zr_vm_cli_aot_writer_options_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    target_include_directories(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE
                  zr_vm_parser_shared
                  zr_vm_core_shared
                  zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                  zr_vm_lib_container_shared
                  zr_vm_lib_iteration_shared
                  zr_vm_lib_ffi_shared
                  zr_vm_lib_network_shared
        )
        target_link_libraries(zr_vm_cli_aot_writer_options_test PRIVATE
                  zr_vm_parser_shared
                  zr_vm_core_shared
                  zr_vm_library_shared
                  zr_vm_lib_math_shared
                  zr_vm_lib_system_shared
                  zr_vm_lib_container_shared
                  zr_vm_lib_iteration_shared
                  zr_vm_lib_ffi_shared
                  zr_vm_lib_network_shared
        )
        target_link_libraries(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE
                  zr_vm_parser_shared
                  zr_vm_core_shared
                  zr_vm_library_shared
                  zr_vm_lib_math_shared
                  zr_vm_lib_system_shared
                  zr_vm_lib_container_shared
                  zr_vm_lib_iteration_shared
                  zr_vm_lib_ffi_shared
                  zr_vm_lib_network_shared
        )
        target_link_libraries(zr_vm_cli_zrm_fixture_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_container_shared
                zr_vm_lib_iteration_shared
                zr_vm_lib_ffi_shared
                zr_vm_lib_network_shared
        )
        target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_container_shared
                zr_vm_lib_iteration_shared
                zr_vm_lib_ffi_shared
                zr_vm_lib_network_shared
        )
        if (TARGET zr_vm_debug_shared)
            target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE zr_vm_debug_shared)
            target_compile_definitions(zr_vm_cli_import_basic_fixture_test PRIVATE ZR_VM_CLI_HAS_DEBUG_AGENT=1)
        endif ()
    else ()
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE
                  zr_vm_parser_static
                  zr_vm_core_static
                  zr_vm_library_static
                  zr_vm_lib_math_static
                zr_vm_lib_system_static
                  zr_vm_lib_container_static
                  zr_vm_lib_iteration_static
                  zr_vm_lib_ffi_static
                  zr_vm_lib_network_static
        )
        target_link_libraries(zr_vm_cli_aot_writer_options_test PRIVATE
                  zr_vm_parser_static
                  zr_vm_core_static
                  zr_vm_library_static
                  zr_vm_lib_math_static
                  zr_vm_lib_system_static
                  zr_vm_lib_container_static
                  zr_vm_lib_iteration_static
                  zr_vm_lib_ffi_static
                  zr_vm_lib_network_static
        )
        target_link_libraries(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE
                  zr_vm_parser_static
                  zr_vm_core_static
                  zr_vm_library_static
                  zr_vm_lib_math_static
                  zr_vm_lib_system_static
                  zr_vm_lib_container_static
                  zr_vm_lib_iteration_static
                  zr_vm_lib_ffi_static
                  zr_vm_lib_network_static
        )
        target_link_libraries(zr_vm_cli_zrm_fixture_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_container_static
                zr_vm_lib_iteration_static
                zr_vm_lib_ffi_static
                zr_vm_lib_network_static
        )
        target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_container_static
                zr_vm_lib_iteration_static
                zr_vm_lib_ffi_static
                zr_vm_lib_network_static
        )
        if (TARGET zr_vm_debug_static)
            target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE zr_vm_debug_static)
            target_compile_definitions(zr_vm_cli_import_basic_fixture_test PRIVATE ZR_VM_CLI_HAS_DEBUG_AGENT=1)
        endif ()
    endif ()
    if (TARGET zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_aot_writer_options_test PRIVATE zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_zrm_fixture_test PRIVATE zr_vm_lib_thread_shared)
        target_compile_definitions(zr_vm_cli_project_incremental_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_aot_writer_options_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_import_basic_fixture_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_zrm_fixture_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
    elseif (TARGET zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_aot_writer_options_test PRIVATE zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_import_basic_fixture_test PRIVATE zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_zrm_fixture_test PRIVATE zr_vm_lib_thread_static)
        target_compile_definitions(zr_vm_cli_project_incremental_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_aot_writer_options_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_aot_compacted_metadata_sidecar_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_import_basic_fixture_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
        target_compile_definitions(zr_vm_cli_zrm_fixture_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
    endif ()
endif ()

if (TARGET zr_vm_cli_executable)
    add_executable(
            zr_vm_cli_repl_e2e_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_repl_e2e.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
    )
    zr_vm_apply_common_test_settings(zr_vm_cli_repl_e2e_test)
    target_include_directories(zr_vm_cli_repl_e2e_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
    )

    add_executable(
            zr_vm_cli_syntax_migration_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_syntax_migration.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
    )
    zr_vm_apply_common_test_settings(zr_vm_cli_syntax_migration_test)
    target_compile_definitions(zr_vm_cli_syntax_migration_test PRIVATE
            ZR_VM_CLI_EXECUTABLE_PATH="$<TARGET_FILE:zr_vm_cli_executable>"
    )
endif ()

if (TARGET zr_vm_cli_executable AND
        ((TARGET zr_vm_lib_network_shared AND TARGET zr_vm_debug_shared) OR
         (TARGET zr_vm_lib_network_static AND TARGET zr_vm_debug_static)))
    add_executable(
            zr_vm_cli_debug_e2e_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_debug_e2e.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
    )
    zr_vm_apply_common_test_settings(zr_vm_cli_debug_e2e_test)
    target_include_directories(zr_vm_cli_debug_e2e_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_cli_debug_e2e_test PRIVATE zr_vm_lib_network_shared)
    else ()
        target_link_libraries(zr_vm_cli_debug_e2e_test PRIVATE zr_vm_lib_network_static)
    endif ()
    zr_link_third_party_for_target(zr_vm_cli_debug_e2e_test "zr_c_json")
endif ()

find_package(Threads REQUIRED)

add_library(
        zr_vm_descriptor_plugin_fixture_int
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/language_server/descriptor_plugin_fixture_int.c
)
target_include_directories(zr_vm_descriptor_plugin_fixture_int PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)
set_target_properties(zr_vm_descriptor_plugin_fixture_int PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(
        zr_vm_descriptor_plugin_fixture_float
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/language_server/descriptor_plugin_fixture_float.c
)
target_include_directories(zr_vm_descriptor_plugin_fixture_float PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)
set_target_properties(zr_vm_descriptor_plugin_fixture_float PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if (TARGET zr_vm_language_server_lsp_project_features_test)
    add_dependencies(zr_vm_language_server_lsp_project_features_test
            zr_vm_descriptor_plugin_fixture_int
            zr_vm_descriptor_plugin_fixture_float)
endif ()

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

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests_generated")
add_custom_command(
        OUTPUT "${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}"
        COMMAND ${CMAKE_COMMAND}
                "-DIN_DLL=$<TARGET_FILE:zr_vm_ffi_fixture>"
                "-DOUT_FILE=${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zr_vm_write_ffi_fixture_path.cmake"
        DEPENDS zr_vm_ffi_fixture
        COMMENT "Generate zr_vm_ffi_fixture_path.h")
add_custom_target(zr_vm_ffi_fixture_path_header DEPENDS "${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}")

if (TARGET zr_vm_libffi_vendor AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_test(
            NAME ffi_win64_asm_dependencies
            COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DNINJA_EXE=${CMAKE_MAKE_PROGRAM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libffi-asm-dependencies-validation.cmake"
    )
    set_tests_properties(ffi_win64_asm_dependencies PROPERTIES LABELS "build;ffi")
endif ()

if (TARGET zr_vm_system_fs_test)
    add_dependencies(zr_vm_system_fs_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()
if (TARGET zr_vm_ffi_test)
    add_dependencies(zr_vm_ffi_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()
if (TARGET zr_vm_ffi_probe)
    add_dependencies(zr_vm_ffi_probe zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_unity_test_target(
            zr_vm_ffi_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_ffi_module.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_native_extern_contract_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_native_extern_contract.c
    )
    target_include_directories(zr_vm_ffi_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_test)
    add_dependencies(zr_vm_ffi_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)

    target_include_directories(zr_vm_native_extern_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_native_extern_contract_test)
    target_compile_definitions(zr_vm_native_extern_contract_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_native_extern_contract_test PRIVATE dl)
    endif ()
    add_dependencies(
            zr_vm_native_extern_contract_test
            zr_vm_ffi_fixture_path_header
            zr_vm_ffi_fixture)

    zr_vm_add_unity_test_target(
            zr_vm_ffi_native_call_pin_contract_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_ffi_native_call_pin_contract.c
    )
    target_include_directories(zr_vm_ffi_native_call_pin_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_ffi_native_call_pin_contract_test)

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
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_probe)
    add_dependencies(zr_vm_ffi_probe zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()
