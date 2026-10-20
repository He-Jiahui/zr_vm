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

    include(${CMAKE_SOURCE_DIR}/tests/cmake/zr_vm_parser_recovery_tests.cmake)
    include(${CMAKE_SOURCE_DIR}/tests/cmake/zr_vm_binary_metadata_source_tests.cmake)
    include(${CMAKE_SOURCE_DIR}/tests/cmake/zr_vm_compile_time_import_ownership_tests.cmake)

    zr_vm_add_unity_test_target(
            zr_vm_expression_fragment_parser_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_expression_fragment_parser.c
    )
    target_include_directories(zr_vm_expression_fragment_parser_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_expression_fragment_parser_test)

    zr_vm_add_unity_test_target(
            zr_vm_reflection_type_surface_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reflection_type_surface.c
    )
    target_include_directories(zr_vm_reflection_type_surface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_reflection_type_surface_test)

    zr_vm_add_unity_test_target(
            zr_vm_reflection_type_stress_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reflection_type_stress.c
    )
    target_include_directories(zr_vm_reflection_type_stress_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_reflection_type_stress_test)

    zr_vm_add_unity_test_target(
            zr_vm_call_spread_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_call_spread.c
    )
    target_include_directories(zr_vm_call_spread_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_call_spread_test)

    zr_vm_add_unity_test_target(
            zr_vm_union_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_union.c
    )
    target_include_directories(zr_vm_union_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_union_test)

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
            zr_vm_closure_capture_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_closure_capture_runtime.c
    )
    target_include_directories(zr_vm_closure_capture_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_closure_capture_runtime_test)

    zr_vm_add_unity_test_target(
            zr_vm_repl_submission_bindings_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_repl_submission_bindings.c
    )
    target_include_directories(zr_vm_repl_submission_bindings_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_repl_submission_bindings_test)

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
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_type_inference_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_type_inference_test PRIVATE zr_vm_lib_iteration_shared)
    else ()
        target_link_libraries(zr_vm_type_inference_test PRIVATE zr_vm_lib_iteration_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_canonical_type_graph_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_canonical_type_graph.c
    )
    target_include_directories(zr_vm_canonical_type_graph_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_canonical_type_graph_test)

    zr_vm_add_unity_test_target(
            zr_vm_place_cfg_graph_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_place_cfg_graph.c
    )
    target_include_directories(zr_vm_place_cfg_graph_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_place_cfg_graph_test)

    zr_vm_add_unity_test_target(
            zr_vm_pre_semantic_ir_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_pre_semantic_ir.c
    )
    target_include_directories(zr_vm_pre_semantic_ir_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_pre_semantic_ir_test)

    zr_vm_add_unity_test_target(
            zr_vm_iterator_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_iterator_runtime.c
    )
    target_include_directories(zr_vm_iterator_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_iterator_runtime_test)

    zr_vm_add_unity_test_target(
            zr_vm_iterator_gc_drop_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_iterator_gc_drop.c
    )
    target_include_directories(zr_vm_iterator_gc_drop_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_iterator_gc_drop_test)

    zr_vm_add_unity_test_target(
            zr_vm_yield_syntax_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_yield_syntax.c
    )
    target_include_directories(zr_vm_yield_syntax_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_yield_syntax_test)

    zr_vm_add_unity_test_target(
            zr_vm_iterator_semantic_ir_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_iterator_semantic_ir.c
    )
    target_include_directories(zr_vm_iterator_semantic_ir_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_iterator_semantic_ir_test)

    zr_vm_add_unity_test_target(
            zr_vm_yield_semantic_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_yield_semantic.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    target_include_directories(zr_vm_yield_semantic_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/container
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_yield_semantic_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_yield_semantic_test PRIVATE zr_vm_lib_iteration_shared)
    else ()
        target_link_libraries(zr_vm_yield_semantic_test PRIVATE zr_vm_lib_iteration_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_artifact_schema_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_artifact_schema.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_artifact_schema_metadata_graph.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_artifact_schema_source_roundtrip.c
    )
    target_include_directories(zr_vm_artifact_schema_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_artifact_schema_test)
    if (BUILD_SHARED_LIB AND TARGET zr_vm_thread_shared)
        target_link_libraries(zr_vm_artifact_schema_test PRIVATE zr_vm_thread_shared)
    elseif (TARGET zr_vm_thread_static)
        target_link_libraries(zr_vm_artifact_schema_test PRIVATE zr_vm_thread_static)
    endif ()
    add_test(
            NAME artifact_schema
            COMMAND $<TARGET_FILE:zr_vm_artifact_schema_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_canonical_consumers_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_canonical_consumers.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_callable_consumers.c
    )
    target_include_directories(zr_vm_canonical_consumers_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    zr_vm_link_parser_core_plus_library(zr_vm_canonical_consumers_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_syntax_contract_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_syntax_contract.c
    )
    target_include_directories(zr_vm_reference_syntax_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_syntax_contract_test)

    zr_vm_add_unity_test_target(
            zr_vm_struct_value_init_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_struct_value_init.c
    )
    target_include_directories(zr_vm_struct_value_init_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_struct_value_init_test)

    zr_vm_add_unity_test_target(
            zr_vm_property_unified_ast_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_property_unified_ast.c
    )
    target_include_directories(zr_vm_property_unified_ast_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_property_unified_ast_test)

    zr_vm_add_unity_test_target(
            zr_vm_property_explicit_field_init_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_property_explicit_field_init.c
    )
    target_include_directories(zr_vm_property_explicit_field_init_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_property_explicit_field_init_test)

    zr_vm_add_unity_test_target(
            zr_vm_property_access_lowering_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_property_access_lowering.c
    )
    target_include_directories(zr_vm_property_access_lowering_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_property_access_lowering_test)

    zr_vm_add_unity_test_target(
            zr_vm_property_ref_return_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_property_ref_return.c
    )
    target_include_directories(zr_vm_property_ref_return_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    target_compile_definitions(zr_vm_property_ref_return_test PRIVATE
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_property_ref_return_test)

    zr_vm_add_unity_test_target(
            zr_vm_property_consumer_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_property_consumer_contracts.c
    )
    target_include_directories(zr_vm_property_consumer_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_property_consumer_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_legacy_migration_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_legacy_migration.c
    )
    target_include_directories(zr_vm_legacy_migration_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_legacy_migration_test)

    zr_vm_add_unity_test_target(
            zr_vm_percent_test_migration_test
            ${CMAKE_SOURCE_DIR}/tests/migration/test_percent_test_migration.c
    )
    target_include_directories(zr_vm_percent_test_migration_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_percent_test_migration_test)
    add_test(
            NAME percent_test_migration
            COMMAND $<TARGET_FILE:zr_vm_percent_test_migration_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_percent_syntax_cutover_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_percent_syntax_cutover.c
    )
    target_include_directories(zr_vm_percent_syntax_cutover_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_percent_syntax_cutover_test)
    add_test(
            NAME percent_syntax_cutover
            COMMAND $<TARGET_FILE:zr_vm_percent_syntax_cutover_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_syntax_reference_v1_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_reference_v1.c
            ${CMAKE_SOURCE_DIR}/tests/fixtures/projects/syntax_reference_v1/native/syntax_reference_native.c
    )
    target_include_directories(zr_vm_syntax_reference_v1_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_syntax_reference_v1_test)
    add_test(
            NAME syntax_reference_v1
            COMMAND $<TARGET_FILE:zr_vm_syntax_reference_v1_test>
    )

    zr_vm_add_unity_test_target(
            zr_vm_reference_place_out_flow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_place_out_flow.c
    )
    target_include_directories(zr_vm_reference_place_out_flow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_place_out_flow_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_loan_nll_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_loan_nll.c
    )
    target_include_directories(zr_vm_reference_loan_nll_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_loan_nll_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_receiver_call_boundary_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_receiver_call_boundary.c
    )
    target_include_directories(zr_vm_reference_receiver_call_boundary_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_receiver_call_boundary_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_escape_closure_suspension_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_escape_closure_suspension.c
    )
    target_include_directories(zr_vm_reference_escape_closure_suspension_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_escape_closure_suspension_test)

    zr_vm_add_unity_test_target(
            zr_vm_ref_struct_restrictions_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ref_struct_restrictions.c
    )
    target_include_directories(zr_vm_ref_struct_restrictions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ref_struct_restrictions_test)

    zr_vm_add_unity_test_target(
            zr_vm_resource_unique_drop_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_resource_unique_drop.c
    )
    target_include_directories(zr_vm_resource_unique_drop_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_resource_unique_drop_test)

    zr_vm_add_unity_test_target(
            zr_vm_resource_shared_weak_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_resource_shared_weak.c
    )
    target_include_directories(zr_vm_resource_shared_weak_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_resource_shared_weak_test)

    zr_vm_add_unity_test_target(
            zr_vm_ownership_intrinsic_member_separation_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ownership_intrinsic_member_separation.c
    )
    target_include_directories(zr_vm_ownership_intrinsic_member_separation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ownership_intrinsic_member_separation_test)

    zr_vm_add_unity_test_target(
            zr_vm_ownership_receiver_guard_performance_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ownership_receiver_guard_performance.c
    )
    target_include_directories(zr_vm_ownership_receiver_guard_performance_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ownership_receiver_guard_performance_test)

    zr_vm_add_unity_test_target(
            zr_vm_resource_owner_borrow_receiver_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_resource_owner_borrow_receiver.c
    )
    target_include_directories(zr_vm_resource_owner_borrow_receiver_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_resource_owner_borrow_receiver_test)

