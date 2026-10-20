    zr_vm_add_unity_test_target(
            zr_vm_compiler_integration_test
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_features.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_regressions.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_escape_metadata_pipeline.c
             ${CMAKE_SOURCE_DIR}/tests/parser/call_chain_polymorphic_compile_fixture.c
             ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_integration_main.c
    )
    target_include_directories(zr_vm_compiler_integration_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_integration_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_compiler_w2_performance_quickening_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening.c
            ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_compiler_w2_performance_quickening_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_w2_performance_quickening_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_compiler_call_lowering_focus_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_regressions.c
            ${CMAKE_SOURCE_DIR}/tests/parser/call_chain_polymorphic_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_call_lowering_focus_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_compiler_call_lowering_focus_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_call_lowering_focus_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(zr_vm_numeric_literals_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_literals.c)
    target_include_directories(zr_vm_numeric_literals_test PRIVATE
            ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core(zr_vm_numeric_literals_test)
    add_test(NAME numeric_literals COMMAND zr_vm_numeric_literals_test)

    zr_vm_add_unity_test_target(zr_vm_lexer_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_lexer_diagnostics.c)
    target_include_directories(zr_vm_lexer_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core(zr_vm_lexer_diagnostics_test)
    add_test(NAME lexer_diagnostics COMMAND ${CMAKE_COMMAND}
            -DTEST_EXECUTABLE=$<TARGET_FILE:zr_vm_lexer_diagnostics_test>
            -P ${CMAKE_SOURCE_DIR}/tests/cmake/run_lexer_diagnostics.cmake)

    zr_vm_add_unity_test_target(
            zr_vm_literal_surface_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_prototype.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_const_keyword.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_char_and_type_cast.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_literal_surface_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_literal_surface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_literal_surface_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_manifest_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/reference/test_reference_manifest_contracts.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_reference_manifest_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_reference_manifest_contracts_test)

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

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/zr_vm_file_list_tests.cmake)

    zr_vm_add_unity_test_target(
            zr_vm_system_fs_test
            ${CMAKE_SOURCE_DIR}/tests/system/test_system_fs_module.c
    )
    target_include_directories(zr_vm_system_fs_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_system_fs_test)

    zr_vm_add_unity_test_target(
            zr_vm_system_assembly_test
            ${CMAKE_SOURCE_DIR}/tests/system/test_system_assembly_module.c
    )
    target_include_directories(zr_vm_system_assembly_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_system_assembly_test)

    zr_vm_add_unity_test_target(
            zr_vm_module_system_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_module_system.c
    )
    target_include_directories(zr_vm_module_system_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_module_system_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_module_system_test PRIVATE zr_vm_lib_iteration_shared)
    else ()
        target_link_libraries(zr_vm_module_system_test PRIVATE zr_vm_lib_iteration_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_metadata_token_model_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_token_model.c
    )
    target_include_directories(zr_vm_metadata_token_model_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_metadata_token_model_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_type_ref_binding_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_type_ref_binding.c
    )
    target_include_directories(zr_vm_metadata_type_ref_binding_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_metadata_type_ref_binding_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_query_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_query.c
    )
    target_include_directories(zr_vm_metadata_runtime_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_query_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_manifest_exports_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_manifest_exports.c
    )
    target_include_directories(zr_vm_metadata_runtime_manifest_exports_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_manifest_exports_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_method_binding_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_method_binding.c
    )
    target_include_directories(zr_vm_metadata_runtime_method_binding_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_method_binding_test)

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/zr_vm_call_binding_tests.cmake)

    zr_vm_add_unity_test_target(
            zr_vm_reflection_token_resolve_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_reflection_token_resolve.c
    )
    target_include_directories(zr_vm_reflection_token_resolve_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_reflection_token_resolve_test)

    zr_vm_add_unity_test_target(
            zr_vm_reflection_method_invoke_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_reflection_method_invoke.c
    )
    target_include_directories(zr_vm_reflection_method_invoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_reflection_method_invoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_binding_compatibility_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_binding_compatibility.c
    )
    target_include_directories(zr_vm_metadata_runtime_binding_compatibility_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_binding_compatibility_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_runtime_typed_direct_call_compatibility_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_aot_runtime_typed_direct_call_compatibility.c
    )
    target_include_directories(zr_vm_aot_runtime_typed_direct_call_compatibility_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_runtime_typed_direct_call_compatibility_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_typespec_layout_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_typespec_layout.c
    )
    target_include_directories(zr_vm_metadata_runtime_typespec_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_typespec_layout_test)

    zr_vm_add_unity_test_target(
            zr_vm_reflection_dynamic_generic_instance_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_reflection_dynamic_generic_instance.c
    )
    target_include_directories(zr_vm_reflection_dynamic_generic_instance_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_reflection_dynamic_generic_instance_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_runtime_type_layout_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_runtime_type_layout.c
    )
    target_include_directories(zr_vm_metadata_runtime_type_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_metadata_runtime_type_layout_test)

    zr_vm_add_unity_test_target(
            zr_vm_zrp_metadata_format_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_zrp_metadata_format.c
    )
    target_include_directories(zr_vm_zrp_metadata_format_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_zrp_metadata_format_test)

    zr_vm_add_unity_test_target(
            zr_vm_metadata_module_hash_golden_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_metadata_module_hash_golden.c
    )
    target_include_directories(zr_vm_metadata_module_hash_golden_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_metadata_module_hash_golden_test)

    zr_vm_add_unity_test_target(
            zr_vm_project_import_resolver_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_import_resolver.c
    )
    target_include_directories(zr_vm_project_import_resolver_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_resolver_test)

    zr_vm_add_unity_test_target(
            zr_vm_project_module_specifier_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_module_specifier.c
    )
    target_include_directories(zr_vm_project_module_specifier_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_module_specifier_test)
    add_test(
            NAME project_module_specifier
            COMMAND $<TARGET_FILE:zr_vm_project_module_specifier_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_project_manifest_v2_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_manifest_v2.c
    )
    target_include_directories(zr_vm_project_manifest_v2_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_manifest_v2_test)
    add_test(
            NAME project_manifest_v2
            COMMAND $<TARGET_FILE:zr_vm_project_manifest_v2_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_project_import_aot_provider_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_import_aot_provider_runtime.c
    )
    target_include_directories(zr_vm_project_import_aot_provider_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_aot_provider_runtime_test)

    zr_vm_add_unity_test_target(
            zr_vm_project_import_provider_version_selection_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_import_provider_version_selection.c
    )
    target_include_directories(zr_vm_project_import_provider_version_selection_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_provider_version_selection_test)

    zr_vm_add_unity_test_target(
            zr_vm_project_manifest_normalization_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_manifest_normalization.c
    )
    target_include_directories(zr_vm_project_manifest_normalization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_manifest_normalization_test)

    zr_vm_add_unity_test_target(
            zr_vm_zrm_container_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_zrm_container.c
    )
    target_include_directories(zr_vm_zrm_container_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/third_party/zr_miniz/miniz
            ${CMAKE_BINARY_DIR}/zr_vm_library/third_party/zr_miniz
    )
    zr_vm_link_core(zr_vm_zrm_container_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_zrm_container_test PRIVATE zr_vm_library_shared zr_third_party_zr_miniz_static)
    else ()
        target_link_libraries(zr_vm_zrm_container_test PRIVATE zr_vm_library_static zr_third_party_zr_miniz_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_native_binding_direct_call_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_native_binding_direct_call.c
    )
    target_include_directories(zr_vm_native_binding_direct_call_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_native_binding_direct_call_test)

    if ((TARGET zr_vm_debug_shared OR TARGET zr_vm_debug_static) AND
            (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static))
        zr_vm_add_unity_test_target(
                zr_vm_debug_library_test
                ${CMAKE_SOURCE_DIR}/tests/library/test_debug_library.c
        )
        target_include_directories(zr_vm_debug_library_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core_plus_library(zr_vm_debug_library_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_library_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_library_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_library_test "zr_c_json")
        target_link_libraries(zr_vm_debug_library_test PRIVATE Threads::Threads)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_native_registry_descriptor_invalidation_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_native_registry_descriptor_invalidation.c
    )
    target_include_directories(zr_vm_native_registry_descriptor_invalidation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_native_registry_descriptor_invalidation_test PRIVATE
            ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH="$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_int>"
    )
    add_dependencies(zr_vm_native_registry_descriptor_invalidation_test
            zr_vm_descriptor_plugin_fixture_int
    )
    zr_vm_link_parser_core_plus_library(zr_vm_native_registry_descriptor_invalidation_test)

    zr_vm_add_unity_test_target(
            zr_vm_official_provider_convergence_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_official_provider_convergence.c
    )
    target_include_directories(zr_vm_official_provider_convergence_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_testing/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_official_provider_convergence_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE
                zr_vm_lib_container_shared
                zr_vm_lib_iteration_shared
                zr_vm_lib_testing_shared
        )
    else ()
        target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE
                zr_vm_lib_container_static
                zr_vm_lib_iteration_static
                zr_vm_lib_testing_static
        )
    endif ()
    if (TARGET zr_vm_thread_shared OR TARGET zr_vm_thread_static)
        target_compile_definitions(zr_vm_official_provider_convergence_test PRIVATE
                ZR_TEST_HAS_THREAD_PROVIDER=1)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE zr_vm_thread_shared)
        else ()
            target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE zr_vm_thread_static)
        endif ()
    endif ()
    if (TARGET zr_vm_debug_shared OR TARGET zr_vm_debug_static)
        target_compile_definitions(zr_vm_official_provider_convergence_test PRIVATE
                ZR_TEST_HAS_DEBUG_PROVIDER=1)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE zr_vm_debug_shared)
        else ()
            target_link_libraries(zr_vm_official_provider_convergence_test PRIVATE zr_vm_debug_static)
        endif ()
    endif ()

    if ((BUILD_SHARED_LIB AND TARGET zr_vm_rust_binding_shared) OR
            (NOT BUILD_SHARED_LIB AND TARGET zr_vm_rust_binding_static))
        zr_vm_add_unity_test_target(
                zr_vm_rust_binding_api_test
                ${CMAKE_SOURCE_DIR}/tests/rust_binding/test_rust_binding_api.c
        )
        target_include_directories(zr_vm_rust_binding_api_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_rust_binding/include
                ${CMAKE_SOURCE_DIR}/zr_vm_rust_binding/src/zr_vm_rust_binding
        )
        if (BUILD_SHARED_LIB)
            target_compile_definitions(zr_vm_rust_binding_api_test PRIVATE
                    ZR_VM_RUST_BINDING_TEST_LINK_SHARED=1
                    ZR_VM_RUST_BINDING_TEST_LINK_STATIC=0
            )
            target_link_libraries(zr_vm_rust_binding_api_test PRIVATE
                    zr_vm_parser_shared
                    zr_vm_core_shared
                    zr_vm_library_shared
                    zr_vm_rust_binding_shared
            )
        else ()
            target_compile_definitions(zr_vm_rust_binding_api_test PRIVATE
                    ZR_VM_RUST_BINDING_TEST_LINK_STATIC=1
                    ZR_VM_RUST_BINDING_TEST_LINK_SHARED=0
            )
            target_link_libraries(zr_vm_rust_binding_api_test PRIVATE
                    zr_vm_parser_static
                    zr_vm_core_static
                    zr_vm_library_static
                    zr_vm_rust_binding_static
            )
        endif ()
    endif ()
        if (TARGET zr_vm_rust_binding_api_test)
            add_test(
                    NAME rust_binding_api
                    COMMAND $<TARGET_FILE:zr_vm_rust_binding_api_test>
            )
        endif ()

    zr_vm_add_unity_test_target(
            zr_vm_project_import_canonicalization_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_project_import_canonicalization.c
    )
    target_include_directories(zr_vm_project_import_canonicalization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_canonicalization_test)

    zr_vm_add_unity_test_target(
            zr_vm_compile_tool_project_import_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compile_tool_project_import.c
    )
    target_include_directories(zr_vm_compile_tool_project_import_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compile_tool_project_import_test)

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
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compile_time_test)

    zr_vm_add_unity_test_target(
            zr_vm_comptime_contract_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_contract.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_comptime_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_comptime_contract_test)

