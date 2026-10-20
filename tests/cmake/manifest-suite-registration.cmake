set(core_runtime_executables "")
if (TARGET zr_vm_gc_test AND TARGET zr_vm_instructions_test AND TARGET zr_vm_meta_test AND TARGET zr_vm_native_closure_value_test AND TARGET zr_vm_runtime_crash_recovery_test AND TARGET zr_vm_value_copy_fast_paths_test AND TARGET zr_vm_type_layout_inline_copy_test AND TARGET zr_vm_execution_member_access_fast_paths_test AND TARGET zr_vm_precall_frame_slot_reset_test AND TARGET zr_vm_tail_reuse_callinfo_reset_test AND TARGET zr_vm_vm_closure_precall_test)
    set(core_runtime_executables
            "$<TARGET_FILE:zr_vm_gc_test>;$<TARGET_FILE:zr_vm_instructions_test>;$<TARGET_FILE:zr_vm_meta_test>;$<TARGET_FILE:zr_vm_native_closure_value_test>;$<TARGET_FILE:zr_vm_runtime_crash_recovery_test>;$<TARGET_FILE:zr_vm_value_copy_fast_paths_test>;$<TARGET_FILE:zr_vm_type_layout_inline_copy_test>;$<TARGET_FILE:zr_vm_execution_member_access_fast_paths_test>;$<TARGET_FILE:zr_vm_precall_frame_slot_reset_test>;$<TARGET_FILE:zr_vm_postcall_fast_paths_test>;$<TARGET_FILE:zr_vm_tail_reuse_callinfo_reset_test>;$<TARGET_FILE:zr_vm_vm_closure_precall_test>"
    )
    set(core_runtime_smoke_executables
            "$<TARGET_FILE:zr_vm_instructions_test>;$<TARGET_FILE:zr_vm_meta_test>;$<TARGET_FILE:zr_vm_runtime_crash_recovery_test>;$<TARGET_FILE:zr_vm_value_copy_fast_paths_test>;$<TARGET_FILE:zr_vm_type_layout_inline_copy_test>;$<TARGET_FILE:zr_vm_execution_member_access_fast_paths_test>;$<TARGET_FILE:zr_vm_precall_frame_slot_reset_test>;$<TARGET_FILE:zr_vm_postcall_fast_paths_test>;$<TARGET_FILE:zr_vm_tail_reuse_callinfo_reset_test>;$<TARGET_FILE:zr_vm_vm_closure_precall_test>"
    )
    set(core_runtime_core_executables "${core_runtime_executables}")
    set(core_runtime_stress_executables "${core_runtime_executables}")
    add_test(
            NAME core_runtime
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=core_runtime"
            "-DEXECUTABLES=${core_runtime_executables}"
            "-DEXECUTABLES_SMOKE=${core_runtime_smoke_executables}"
            "-DEXECUTABLES_CORE=${core_runtime_core_executables}"
            "-DEXECUTABLES_STRESS=${core_runtime_stress_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_pipeline_executables "")
if (TARGET zr_vm_parser_test AND
        TARGET zr_vm_instruction_execution_test AND
        TARGET zr_vm_type_inference_test AND
        TARGET zr_vm_semantic_facts_test AND
        TARGET zr_vm_semantic_query_test AND
        TARGET zr_vm_semantic_query_symbols_test AND
        TARGET zr_vm_compiler_semantic_query_diagnostics_test AND
        TARGET zr_vm_cfg_reachability_test AND
        TARGET zr_vm_cfg_union_exhaustiveness_test AND
        TARGET zr_vm_cfg_constant_conditions_test AND
        TARGET zr_vm_cfg_integer_bitwise_conditions_test AND
        TARGET zr_vm_cfg_float_arithmetic_conditions_test AND
        TARGET zr_vm_cfg_switch_constants_test AND
        TARGET zr_vm_cfg_finally_abrupt_test AND
        TARGET zr_vm_cfg_try_catch_edges_test AND
        TARGET zr_vm_cfg_throw_effects_test AND
        TARGET zr_vm_cfg_typed_catch_flow_test AND
        TARGET zr_vm_cfg_typed_catch_branch_flow_test AND
        TARGET zr_vm_cfg_typed_catch_loop_flow_test AND
        TARGET zr_vm_cfg_typed_catch_switch_flow_test AND
        TARGET zr_vm_dataflow_engine_test AND
        TARGET zr_vm_reference_fact_emission_test AND
        TARGET zr_vm_compiler_integration_test AND
        TARGET zr_vm_semir_pipeline_test AND
        TARGET zr_vm_execbc_aot_pipeline_test AND
        TARGET zr_vm_execbc_aot_manual_opcode_sync_test AND
        TARGET zr_vm_dynamic_iteration_pipeline_test AND
        TARGET zr_vm_known_call_pipeline_test AND
        TARGET zr_vm_meta_call_pipeline_test AND
        TARGET zr_vm_tail_call_pipeline_test AND
        TARGET zr_vm_aot_c_source_contracts_test AND
        TARGET zr_vm_aot_c_guardrail_contracts_test AND
        TARGET zr_vm_aot_c_value_semir_contracts_test AND
        TARGET zr_vm_aot_c_control_contracts_test AND
        TARGET zr_vm_aot_c_return_contracts_test AND
        TARGET zr_vm_aot_c_logical_contracts_test AND
        TARGET zr_vm_aot_c_global_contracts_test AND
        TARGET zr_vm_aot_c_constant_contracts_test AND
        TARGET zr_vm_aot_c_ownership_contracts_test AND
        TARGET zr_vm_aot_c_super_array_contracts_test AND
        TARGET zr_vm_aot_c_iterator_contracts_test AND
        TARGET zr_vm_aot_c_scope_contracts_test AND
        TARGET zr_vm_aot_c_shift_contracts_test AND
        TARGET zr_vm_aot_c_float_contracts_test AND
        TARGET zr_vm_aot_c_generic_numeric_contracts_test AND
        TARGET zr_vm_aot_c_power_contracts_test AND
        TARGET zr_vm_aot_c_call_contracts_test AND
        TARGET zr_vm_aot_c_typed_call_contracts_test AND
        TARGET zr_vm_aot_c_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_control_shared_library_smoke_test AND
        TARGET zr_vm_aot_receiver_guard_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_call_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_dynamic_deopt_bridge_smoke_test AND
        TARGET zr_vm_aot_c_value_type_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_logical_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_global_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_ownership_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_super_array_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_iterator_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_scope_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_float_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_generic_numeric_shared_library_smoke_test AND
        TARGET zr_vm_aot_c_power_shared_library_smoke_test AND
        TARGET zr_vm_value_type_runtime_test AND
        TARGET zr_vm_literal_surface_test AND
        TARGET zr_vm_reference_manifest_contracts_test AND
        TARGET zr_vm_lexer_parser_compiler_execution_test AND
        TARGET zr_vm_exceptions_test AND
        TARGET zr_vm_module_system_test AND
        TARGET zr_vm_named_arguments_test AND
        TARGET zr_vm_compile_time_test AND
        TARGET zr_vm_decorator_pipeline_test)
    set(language_pipeline_executables
            "$<TARGET_FILE:zr_vm_parser_test>;$<TARGET_FILE:zr_vm_instruction_execution_test>;$<TARGET_FILE:zr_vm_type_inference_test>;$<TARGET_FILE:zr_vm_semantic_facts_test>;$<TARGET_FILE:zr_vm_semantic_query_test>;$<TARGET_FILE:zr_vm_compiler_semantic_query_diagnostics_test>;$<TARGET_FILE:zr_vm_cfg_reachability_test>;$<TARGET_FILE:zr_vm_cfg_union_exhaustiveness_test>;$<TARGET_FILE:zr_vm_cfg_constant_conditions_test>;$<TARGET_FILE:zr_vm_cfg_switch_constants_test>;$<TARGET_FILE:zr_vm_cfg_finally_abrupt_test>;$<TARGET_FILE:zr_vm_cfg_try_catch_edges_test>;$<TARGET_FILE:zr_vm_cfg_typed_catch_flow_test>;$<TARGET_FILE:zr_vm_cfg_typed_catch_loop_flow_test>;$<TARGET_FILE:zr_vm_dataflow_engine_test>;$<TARGET_FILE:zr_vm_reference_fact_emission_test>;$<TARGET_FILE:zr_vm_compiler_integration_test>;$<TARGET_FILE:zr_vm_semir_pipeline_test>;$<TARGET_FILE:zr_vm_aot_c_source_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_value_semir_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_control_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_return_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_logical_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_global_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_constant_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_ownership_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_super_array_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_iterator_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_scope_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_shift_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_float_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_generic_numeric_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_power_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_call_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_typed_call_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_control_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_receiver_guard_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_call_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_value_type_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_logical_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_global_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_ownership_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_super_array_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_iterator_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_scope_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_float_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_generic_numeric_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_power_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_value_type_runtime_test>;$<TARGET_FILE:zr_vm_literal_surface_test>;$<TARGET_FILE:zr_vm_reference_manifest_contracts_test>;$<TARGET_FILE:zr_vm_lexer_parser_compiler_execution_test>;$<TARGET_FILE:zr_vm_exceptions_test>;$<TARGET_FILE:zr_vm_module_system_test>;$<TARGET_FILE:zr_vm_named_arguments_test>;$<TARGET_FILE:zr_vm_compile_time_test>;$<TARGET_FILE:zr_vm_decorator_pipeline_test>"
    )
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_execbc_aot_pipeline_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_execbc_aot_manual_opcode_sync_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_dynamic_iteration_pipeline_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_known_call_pipeline_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_meta_call_pipeline_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_tail_call_pipeline_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_semantic_query_symbols_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_cfg_integer_bitwise_conditions_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_cfg_float_arithmetic_conditions_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_wrapper_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_minus_unary_rhs_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_minus_rhs_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_minus_shift_exact_zero_count_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_minus_shift_supported_count_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_all_ones_count_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_unary_zm_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_not_zm_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_double_zm_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_neg_not_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_not_unary_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_zm_unary_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_double_not_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_double_neg_count_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bzms_private_not_mask_range_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_branch_refinement_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_branch_segment_sets_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_branch_assignment_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_assignment_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_self_dependency_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_self_dependency_symbolic_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_offset_expression_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_target_unary_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_target_positive_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_expression_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_product_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_add_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_identity_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_sum_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_unary_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_cond_delta_neg_zero_incl_subtract_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_product_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_selfdep_neg_zero_product_unary_same_diff_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_neg_zero_incl_diff_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_offset_expression_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_selfdep_bitwise_idempotent_wrapper_offset_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_arith_offset_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_self_dependency_symbolic_coefficient_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_self_dependency_symbolic_coefficient_cross_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_deeper_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_three_additional_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_reader_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_five_additional_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_six_additional_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_selfdep_coeff_cross_seven_additional_scale_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_self_dependency_symbolic_product_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_loop_constant_condition_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_for_header_assignment_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_nested_loop_assignment_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_numeric_foreach_cardinality_dataflow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_aot_c_dynamic_deopt_bridge_smoke_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_cfg_throw_effects_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_cfg_typed_catch_branch_flow_test>")
    list(APPEND language_pipeline_executables
            "$<TARGET_FILE:zr_vm_cfg_typed_catch_switch_flow_test>")
    set(language_pipeline_smoke_executables
            "$<TARGET_FILE:zr_vm_instruction_execution_test>;$<TARGET_FILE:zr_vm_compiler_integration_test>;$<TARGET_FILE:zr_vm_semir_pipeline_test>;$<TARGET_FILE:zr_vm_aot_c_source_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_value_semir_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_control_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_return_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_logical_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_global_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_constant_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_ownership_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_super_array_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_iterator_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_scope_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_shift_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_float_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_generic_numeric_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_power_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_call_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_typed_call_contracts_test>;$<TARGET_FILE:zr_vm_aot_c_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_control_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_receiver_guard_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_call_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_value_type_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_logical_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_global_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_ownership_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_super_array_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_iterator_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_scope_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_float_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_generic_numeric_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_aot_c_power_shared_library_smoke_test>;$<TARGET_FILE:zr_vm_value_type_runtime_test>;$<TARGET_FILE:zr_vm_literal_surface_test>;$<TARGET_FILE:zr_vm_reference_manifest_contracts_test>;$<TARGET_FILE:zr_vm_module_system_test>;$<TARGET_FILE:zr_vm_decorator_pipeline_test>"
    )
    list(APPEND language_pipeline_smoke_executables
            "$<TARGET_FILE:zr_vm_aot_c_dynamic_deopt_bridge_smoke_test>")
    set(language_pipeline_core_executables "${language_pipeline_executables}")
    set(language_pipeline_stress_executables "${language_pipeline_executables}")
    zr_vm_add_manifest_executable_suite(
            NAME language_pipeline
            RUNNER_SCRIPT "${ZR_VM_SUITE_RUNNER_SCRIPT}"
            EXECUTABLES "${language_pipeline_executables}"
            EXECUTABLES_SMOKE "${language_pipeline_smoke_executables}"
            EXECUTABLES_CORE "${language_pipeline_core_executables}"
            EXECUTABLES_STRESS "${language_pipeline_stress_executables}"
            HOST_BINARY_DIR "${CMAKE_BINARY_DIR}"
            RUN_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif ()

set(container_executables "")
if (TARGET zr_vm_container_runtime_test AND
        TARGET zr_vm_container_metadata_test AND
        TARGET zr_vm_container_type_inference_test AND
        TARGET zr_vm_container_temp_value_root_test AND
        TARGET zr_vm_generational_pool_test AND
        TARGET zr_vm_generational_pool_gc_stress_test AND
        TARGET zr_vm_generational_pool_performance_matrix_test AND
        TARGET zr_vm_generational_pool_type_layout_test AND
        TARGET zr_vm_pooling_closed_type_runtime_test AND
        TARGET zr_vm_generational_pool_artifact_test)
    set(container_executables
            "$<TARGET_FILE:zr_vm_container_runtime_test>;$<TARGET_FILE:zr_vm_container_metadata_test>;$<TARGET_FILE:zr_vm_container_type_inference_test>;$<TARGET_FILE:zr_vm_container_temp_value_root_test>;$<TARGET_FILE:zr_vm_generational_pool_test>;$<TARGET_FILE:zr_vm_generational_pool_gc_stress_test>;$<TARGET_FILE:zr_vm_generational_pool_performance_matrix_test>;$<TARGET_FILE:zr_vm_generational_pool_type_layout_test>;$<TARGET_FILE:zr_vm_pooling_closed_type_runtime_test>;$<TARGET_FILE:zr_vm_generational_pool_artifact_test>"
    )
    set(container_smoke_executables
            "$<TARGET_FILE:zr_vm_container_runtime_test>;$<TARGET_FILE:zr_vm_container_type_inference_test>;$<TARGET_FILE:zr_vm_container_temp_value_root_test>"
    )
    add_test(
            NAME containers
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=containers"
            "-DEXECUTABLES=${container_executables}"
            "-DEXECUTABLES_SMOKE=${container_smoke_executables}"
            "-DEXECUTABLES_CORE=${container_executables}"
            "-DEXECUTABLES_STRESS=${container_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_server_executables "")
if (TARGET zr_vm_language_server_symbol_table_test AND
        TARGET zr_vm_language_server_reference_tracker_test AND
        TARGET zr_vm_language_server_semantic_analyzer_test AND
        TARGET zr_vm_language_server_ownership_diagnostics_test AND
        TARGET zr_vm_language_server_union_pattern_diagnostics_test AND
        TARGET zr_vm_language_server_local_semantic_query_test AND
        TARGET zr_vm_language_server_logical_semantic_query_test AND
        TARGET zr_vm_language_server_inlay_semantic_facts_test AND
        TARGET zr_vm_language_server_local_semantic_hover_test AND
        TARGET zr_vm_language_server_computed_member_hover_test AND
        TARGET zr_vm_language_server_expression_fact_hover_test AND
        TARGET zr_vm_language_server_reachability_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_semantic_query_test AND
        TARGET zr_vm_lsp_bitwise_zero_wrapper_range_query_test AND
        TARGET zr_vm_lsp_bitwise_zero_minus_unary_rhs_range_query_test AND
        TARGET zr_vm_lsp_bitwise_zero_minus_rhs_range_query_test AND
        TARGET zr_vm_lsp_bitwise_zero_minus_shift_exact_zero_count_range_query_test AND
        TARGET zr_vm_lsp_bitwise_zero_minus_shift_supported_count_range_query_test AND
        TARGET zr_vm_lsp_bzms_all_ones_query_test AND
        TARGET zr_vm_lsp_bzms_unary_zm_query_test AND
        TARGET zr_vm_lsp_bzms_not_zm_query_test AND
        TARGET zr_vm_lsp_bzms_double_zm_query_test AND
        TARGET zr_vm_lsp_bzms_neg_not_query_test AND
        TARGET zr_vm_lsp_bzms_not_unary_query_test AND
        TARGET zr_vm_lsp_bzms_zm_unary_query_test AND
        TARGET zr_vm_lsp_bzms_double_not_query_test AND
        TARGET zr_vm_lsp_bzms_double_neg_query_test AND
        TARGET zr_vm_lsp_bzms_supported_count_wrapped_not_query_test AND
        TARGET zr_vm_lsp_bzms_private_not_mask_query_test AND
        TARGET zr_vm_language_server_numeric_branch_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_branch_assignment_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_loop_self_dependency_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_loop_self_dependency_symbolic_semantic_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_target_unary_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_target_positive_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_negative_offset_expr_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_product_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_add_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_negative_offset_identity_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_identity_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_sum_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_unary_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_subtract_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_product_query_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_product_unary_same_diff_test AND
        TARGET zr_vm_lsp_selfdep_neg_zero_incl_diff_query_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_bitwise_offset_query_test AND
        TARGET zr_vm_lsp_selfdep_bitwise_idempotent_wrapper_offset_test AND
        TARGET zr_vm_lsp_selfdep_conditional_delta_bitwise_unary_offset_query_test AND
        TARGET zr_vm_language_server_numeric_loop_self_dependency_symbolic_coefficient_semantic_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_deeper_scale_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_three_additional_scale_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_four_additional_scale_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_four_scale_reader_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_five_additional_scale_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_six_additional_scale_query_test AND
        TARGET zr_vm_lsp_selfdep_coeff_cross_seven_additional_scale_query_test AND
        TARGET zr_vm_language_server_numeric_loop_self_dependency_symbolic_product_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_loop_constant_condition_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_for_header_assignment_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_nested_loop_assignment_semantic_query_test AND
        TARGET zr_vm_language_server_numeric_foreach_cardinality_semantic_query_test AND
        TARGET zr_vm_language_server_call_member_semantic_query_test AND
        TARGET zr_vm_language_server_semantic_query_parity_test AND
        TARGET zr_vm_language_server_incremental_parser_test AND
        TARGET zr_vm_language_server_parser_diagnostics_test AND
        TARGET zr_vm_language_server_declaration_parser_diagnostics_test AND
        TARGET zr_vm_language_server_statement_parser_diagnostics_test AND
        TARGET zr_vm_language_server_lsp_position_mapping_test AND
        TARGET zr_vm_language_server_lsp_position_fuzz_test AND
        TARGET zr_vm_language_server_lsp_uri_test AND
        TARGET zr_vm_language_server_lsp_decorator_utf16_ranges_test AND
        TARGET zr_vm_language_server_lsp_code_lens_utf16_ranges_test AND
        TARGET zr_vm_language_server_lsp_super_utf16_ranges_test AND
        TARGET zr_vm_language_server_lsp_token_metadata_utf16_ranges_test AND
        TARGET zr_vm_language_server_lsp_source_contracts_test AND
        TARGET zr_vm_language_server_lsp_interface_test AND
        TARGET zr_vm_language_server_lsp_project_features_test AND
        TARGET zr_vm_language_server_lsp_project_utf16_ranges_test AND
        TARGET zr_vm_language_server_lsp_advanced_editor_features_test)
    set(language_server_executables
            "$<TARGET_FILE:zr_vm_language_server_symbol_table_test>;$<TARGET_FILE:zr_vm_language_server_reference_tracker_test>;$<TARGET_FILE:zr_vm_language_server_semantic_analyzer_test>;$<TARGET_FILE:zr_vm_language_server_ownership_diagnostics_test>;$<TARGET_FILE:zr_vm_language_server_union_pattern_diagnostics_test>;$<TARGET_FILE:zr_vm_language_server_local_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_logical_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_inlay_semantic_facts_test>;$<TARGET_FILE:zr_vm_language_server_local_semantic_hover_test>;$<TARGET_FILE:zr_vm_language_server_computed_member_hover_test>;$<TARGET_FILE:zr_vm_language_server_expression_fact_hover_test>;$<TARGET_FILE:zr_vm_language_server_reachability_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_branch_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_branch_assignment_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_loop_self_dependency_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_loop_self_dependency_symbolic_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_loop_self_dependency_symbolic_coefficient_semantic_query_test>;$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_query_test>;$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_deeper_scale_query_test>;$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_three_additional_scale_query_test>;$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_four_additional_scale_query_test>;$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_four_scale_reader_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_loop_self_dependency_symbolic_product_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_loop_constant_condition_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_for_header_assignment_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_nested_loop_assignment_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_numeric_foreach_cardinality_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_call_member_semantic_query_test>;$<TARGET_FILE:zr_vm_language_server_incremental_parser_test>;$<TARGET_FILE:zr_vm_language_server_parser_diagnostics_test>;$<TARGET_FILE:zr_vm_language_server_declaration_parser_diagnostics_test>;$<TARGET_FILE:zr_vm_language_server_statement_parser_diagnostics_test>;$<TARGET_FILE:zr_vm_language_server_lsp_position_mapping_test>;$<TARGET_FILE:zr_vm_language_server_lsp_position_fuzz_test>;$<TARGET_FILE:zr_vm_language_server_lsp_decorator_utf16_ranges_test>;$<TARGET_FILE:zr_vm_language_server_lsp_code_lens_utf16_ranges_test>;$<TARGET_FILE:zr_vm_language_server_lsp_super_utf16_ranges_test>;$<TARGET_FILE:zr_vm_language_server_lsp_token_metadata_utf16_ranges_test>;$<TARGET_FILE:zr_vm_language_server_lsp_source_contracts_test>;$<TARGET_FILE:zr_vm_language_server_lsp_interface_test>;$<TARGET_FILE:zr_vm_language_server_lsp_project_features_test>;$<TARGET_FILE:zr_vm_language_server_lsp_project_utf16_ranges_test>;$<TARGET_FILE:zr_vm_language_server_lsp_advanced_editor_features_test>"
    )
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_language_server_semantic_query_parity_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bitwise_zero_wrapper_range_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_language_server_lsp_uri_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bitwise_zero_minus_unary_rhs_range_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bitwise_zero_minus_rhs_range_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bitwise_zero_minus_shift_exact_zero_count_range_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bitwise_zero_minus_shift_supported_count_range_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_all_ones_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_unary_zm_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_not_zm_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_double_zm_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_neg_not_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_not_unary_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_zm_unary_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_double_not_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_double_neg_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_supported_count_wrapped_not_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_bzms_private_not_mask_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_five_additional_scale_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_six_additional_scale_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_coeff_cross_seven_additional_scale_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_target_unary_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_target_positive_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_negative_offset_expr_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_product_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_add_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_negative_offset_identity_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_identity_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_sum_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_unary_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_subtract_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_product_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_product_unary_same_diff_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_neg_zero_incl_diff_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_bitwise_offset_query_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_bitwise_idempotent_wrapper_offset_test>")
    string(APPEND language_server_executables
            ";$<TARGET_FILE:zr_vm_lsp_selfdep_conditional_delta_bitwise_unary_offset_query_test>")
    add_test(
            NAME language_server
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=language_server"
            "-DEXECUTABLES=${language_server_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()
