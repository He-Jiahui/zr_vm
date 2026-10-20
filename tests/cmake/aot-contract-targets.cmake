    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_reachability_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
        )
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_gc_domain_bridge_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_gc_domain_bridge.c
    )
    target_include_directories(zr_vm_gc_domain_bridge_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_gc_domain_bridge_test)
    zr_vm_add_unity_test_target(
            zr_vm_gc_domain_multimutator_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_gc_domain_multimutator.c
    )
    target_include_directories(zr_vm_gc_domain_multimutator_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_gc_domain_multimutator_test)
    target_link_libraries(zr_vm_gc_domain_multimutator_test PRIVATE Threads::Threads)
    zr_vm_add_unity_test_target(
            zr_vm_gc_concurrent_major_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_gc_concurrent_major.c
    )
    target_include_directories(zr_vm_gc_concurrent_major_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_gc_concurrent_major_test)
    target_link_libraries(zr_vm_gc_concurrent_major_test PRIVATE Threads::Threads)
    zr_vm_add_unity_test_target(
            zr_vm_resource_same_domain_handoff_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_resource_same_domain_handoff.c
    )
    target_include_directories(zr_vm_resource_same_domain_handoff_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_resource_same_domain_handoff_test)
    target_link_libraries(zr_vm_resource_same_domain_handoff_test PRIVATE Threads::Threads)
    zr_vm_add_unity_test_target(
            zr_vm_resource_cross_domain_transfer_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_resource_cross_domain_transfer.c
    )
    target_include_directories(zr_vm_resource_cross_domain_transfer_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_resource_cross_domain_transfer_test)
    zr_vm_add_unity_test_target(
            zr_vm_resource_cross_domain_transfer_race_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_resource_cross_domain_transfer_races.c
    )
    target_include_directories(zr_vm_resource_cross_domain_transfer_race_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_resource_cross_domain_transfer_race_test)
    target_link_libraries(zr_vm_resource_cross_domain_transfer_race_test PRIVATE Threads::Threads)
    zr_vm_link_parser_core(zr_vm_aot_reachability_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_guardrail_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_guardrail_contracts.c
    )
    target_include_directories(zr_vm_aot_c_guardrail_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_guardrail_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_code_stripping_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_code_stripping.c
    )
    target_include_directories(zr_vm_aot_c_code_stripping_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_code_stripping_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_publication_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_publication.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_publication_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_zrp_metadata_publication_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_reflection_annotation_preserve_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_reflection_annotation_preserve.c
    )
    target_include_directories(zr_vm_aot_c_reflection_annotation_preserve_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_reflection_annotation_preserve_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_methodspec_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_methodspec_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_methodspec_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_methodspec_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_typedef_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_typedef_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_typedef_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_typedef_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_typespec_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_typespec_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_typespec_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_typespec_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_typespec_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_module_ref_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_module_ref_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_module_ref_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_module_ref_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_module_ref_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_export_token_remap_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_export_token_remap.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_export_token_remap_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_export_token_remap_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_export_token_remap_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_size_deltas_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_size_deltas.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_size_deltas_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_size_deltas_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_size_deltas_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_zrp_metadata_pool_pruning_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_zrp_metadata_pool_pruning.c
    )
    target_include_directories(zr_vm_aot_c_zrp_metadata_pool_pruning_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_aot_c_zrp_metadata_pool_pruning_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c
        )
    endif ()
    zr_vm_link_parser_core(zr_vm_aot_c_zrp_metadata_pool_pruning_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_llvm_symbol_stripping_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_llvm_symbol_stripping.c
    )
    target_include_directories(zr_vm_aot_llvm_symbol_stripping_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_llvm_symbol_stripping_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_type_layout_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_type_layout_contracts.c
    )
    target_include_directories(zr_vm_aot_c_type_layout_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_type_layout_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_golden_scalar_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_golden_scalar_smoke.c
    )
    target_include_directories(zr_vm_aot_c_golden_scalar_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_golden_scalar_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_golden_scalar_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_golden_scalar_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_scalar_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_scalar.c
    )
    target_include_directories(zr_vm_aot_c_typed_scalar_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_scalar_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_C_FLAGS="${CMAKE_C_FLAGS}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_scalar_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_scalar_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_load_const_scalar_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_load_const_scalar.c
    )
    target_include_directories(zr_vm_aot_c_load_const_scalar_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_load_const_scalar_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_load_const_scalar_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_load_const_scalar_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_method_info_signature_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_method_info_signature.c
    )
    target_include_directories(zr_vm_aot_c_method_info_signature_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_method_info_signature_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_value_semir_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_value_semir_contracts.c
    )
    target_include_directories(zr_vm_aot_c_value_semir_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_value_semir_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_control_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_control_contracts.c
    )
    target_include_directories(zr_vm_aot_c_control_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_control_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_return_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_return_contracts.c
    )
    target_include_directories(zr_vm_aot_c_return_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_return_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_frame_setup_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_frame_setup_contracts.c
    )
    target_include_directories(zr_vm_aot_c_frame_setup_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_frame_setup_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_logical_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_logical_contracts.c
    )
    target_include_directories(zr_vm_aot_c_logical_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_logical_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_global_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_global_contracts.c
    )
    target_include_directories(zr_vm_aot_c_global_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_global_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_constant_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_constant_contracts.c
    )
    target_include_directories(zr_vm_aot_c_constant_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_constant_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_ownership_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_ownership_contracts.c
    )
    target_include_directories(zr_vm_aot_c_ownership_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_ownership_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_super_array_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_super_array_contracts.c
    )
    target_include_directories(zr_vm_aot_c_super_array_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_super_array_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_iterator_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_iterator_contracts.c
    )
    target_include_directories(zr_vm_aot_c_iterator_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_iterator_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_scope_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_scope_contracts.c
    )
    target_include_directories(zr_vm_aot_c_scope_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_scope_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_shift_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_shift_contracts.c
    )
    target_include_directories(zr_vm_aot_c_shift_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_shift_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_float_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_float_contracts.c
    )
    target_include_directories(zr_vm_aot_c_float_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_float_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_numeric_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_numeric_contracts.c
    )
    target_include_directories(zr_vm_aot_c_generic_numeric_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_generic_numeric_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_power_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_power_contracts.c
    )
    target_include_directories(zr_vm_aot_c_power_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_power_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_call_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_call_contracts.c
    )
    target_include_directories(zr_vm_aot_c_call_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_call_contracts_test)

    set(zr_vm_aot_c_typed_call_contract_sources
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_call_contracts.c
    )
    foreach (zr_vm_aot_c_typed_call_contract_optional_source IN ITEMS
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_call_i64_contracts.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_call_bool_contracts.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_call_u64_contracts.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_call_f64_contracts.c)
        if (EXISTS "${zr_vm_aot_c_typed_call_contract_optional_source}")
            list(APPEND zr_vm_aot_c_typed_call_contract_sources
                    ${zr_vm_aot_c_typed_call_contract_optional_source})
        endif ()
    endforeach ()
