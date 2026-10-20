    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_execbc_aot_pipeline_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
    )

    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_execbc_aot_manual_opcode_sync_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
    )

    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_dynamic_iteration_pipeline_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c
    )

    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_known_call_pipeline_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_known_call_pipeline.c
    )

    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_meta_call_pipeline_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_meta_call_pipeline.c
    )

    zr_vm_add_execbc_aot_pipeline_test(
            zr_vm_tail_call_pipeline_test
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/tests/parser/test_tail_call_pipeline.c
    )

    zr_vm_add_unity_test_target(
            zr_vm_generic_instantiation_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_generic_instantiation.c
    )
    target_include_directories(zr_vm_generic_instantiation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_generic_instantiation_test)

    zr_vm_add_unity_test_target(
            zr_vm_generic_constraints_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_generic_constraints.c
    )
    target_include_directories(zr_vm_generic_constraints_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_generic_constraints_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_facts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_facts.c
    )
    target_include_directories(zr_vm_semantic_facts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_facts_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query.c
    )
    target_include_directories(zr_vm_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_symbols_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query_symbols.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
    )
    target_include_directories(zr_vm_semantic_query_symbols_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_symbols_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_relations_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query_relations.c
    )
    target_include_directories(zr_vm_semantic_query_relations_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_relations_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_calls_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query_calls.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
    )
    target_include_directories(zr_vm_semantic_query_calls_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_calls_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_display_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_display.c
    )
    target_include_directories(zr_vm_semantic_display_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_display_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_contract_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query_contract.c
    )
    target_include_directories(zr_vm_semantic_query_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_contract_test)

    zr_vm_add_unity_test_target(
            zr_vm_semantic_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_query_diagnostics.c
    )
    target_include_directories(zr_vm_semantic_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_semantic_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_semantic_query_diagnostics.c
    )
    target_include_directories(zr_vm_compiler_semantic_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_semantic_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_return_ownership_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_return_ownership_diagnostics.c
    )
    target_include_directories(zr_vm_compiler_return_ownership_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_return_ownership_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_variance_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_variance_query_diagnostics.c
    )
    target_include_directories(zr_vm_compiler_variance_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_variance_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_const_assignment_query_producer_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_const_assignment_query_producer.c
    )
    target_include_directories(zr_vm_compiler_const_assignment_query_producer_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_const_assignment_query_producer_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_interface_const_query_producer_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_interface_const_query_producer.c
    )
    target_include_directories(zr_vm_compiler_interface_const_query_producer_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_interface_const_query_producer_test)

    zr_vm_add_unity_test_target(
            zr_vm_extern_struct_decorator_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_extern_struct_decorator_query_diagnostics.c
    )
    target_include_directories(zr_vm_extern_struct_decorator_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_extern_struct_decorator_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_extern_enum_decorator_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_extern_enum_decorator_query_diagnostics.c
    )
    target_include_directories(zr_vm_extern_enum_decorator_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_extern_enum_decorator_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_ffi_wrapper_decorator_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ffi_wrapper_decorator_query_diagnostics.c
    )
    target_include_directories(zr_vm_ffi_wrapper_decorator_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_wrapper_decorator_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_extern_parameter_decorator_query_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_extern_parameter_decorator_query_diagnostics.c
    )
    target_include_directories(zr_vm_extern_parameter_decorator_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_extern_parameter_decorator_query_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_reachability_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_reachability.c
    )
    target_include_directories(zr_vm_cfg_reachability_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_reachability_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_union_exhaustiveness_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_union_exhaustiveness.c
    )
    target_include_directories(zr_vm_cfg_union_exhaustiveness_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_union_exhaustiveness_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_constant_conditions_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_constant_conditions.c
    )
    target_include_directories(zr_vm_cfg_constant_conditions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_constant_conditions_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_integer_bitwise_conditions_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_integer_bitwise_conditions.c
    )
    target_include_directories(zr_vm_cfg_integer_bitwise_conditions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_integer_bitwise_conditions_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_float_arithmetic_conditions_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_float_arithmetic_conditions.c
    )
    target_include_directories(zr_vm_cfg_float_arithmetic_conditions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_float_arithmetic_conditions_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_switch_constants_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_switch_constants.c
    )
    target_include_directories(zr_vm_cfg_switch_constants_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_switch_constants_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_finally_abrupt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_finally_abrupt.c
    )
    target_include_directories(zr_vm_cfg_finally_abrupt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_finally_abrupt_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_try_catch_edges_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_try_catch_edges.c
    )
    target_include_directories(zr_vm_cfg_try_catch_edges_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_try_catch_edges_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_throw_effects_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_throw_effects.c
    )
    target_include_directories(zr_vm_cfg_throw_effects_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_throw_effects_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_typed_catch_flow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_typed_catch_flow.c
    )
    target_include_directories(zr_vm_cfg_typed_catch_flow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_typed_catch_flow_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_typed_catch_branch_flow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_typed_catch_branch_flow.c
    )
    target_include_directories(zr_vm_cfg_typed_catch_branch_flow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_typed_catch_branch_flow_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_typed_catch_loop_flow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_typed_catch_loop_flow.c
    )
    target_include_directories(zr_vm_cfg_typed_catch_loop_flow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_typed_catch_loop_flow_test)

    zr_vm_add_unity_test_target(
            zr_vm_cfg_typed_catch_switch_flow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_cfg_typed_catch_switch_flow.c
    )
    target_include_directories(zr_vm_cfg_typed_catch_switch_flow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_cfg_typed_catch_switch_flow_test)

    zr_vm_add_unity_test_target(
            zr_vm_dataflow_engine_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_dataflow_engine.c
    )
    target_include_directories(zr_vm_dataflow_engine_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_dataflow_engine_test)

    zr_vm_add_unity_test_target(
            zr_vm_expression_fact_emission_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_expression_fact_emission.c
    )
    target_include_directories(zr_vm_expression_fact_emission_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_expression_fact_emission_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_range_inference.c
    )
    target_include_directories(zr_vm_numeric_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_wrapper_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_wrapper_range_inference.c
    )
    target_include_directories(zr_vm_bitwise_zero_wrapper_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_wrapper_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_minus_unary_rhs_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_unary_rhs_range_inference.c
    )
    target_include_directories(zr_vm_bitwise_zero_minus_unary_rhs_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_minus_unary_rhs_range_inference_test)

