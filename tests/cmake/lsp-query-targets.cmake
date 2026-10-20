    foreach (language_server_target
            zr_vm_language_server_symbol_table_test
            zr_vm_language_server_reference_tracker_test
            zr_vm_language_server_semantic_analyzer_test
            zr_vm_language_server_ownership_diagnostics_test
            zr_vm_language_server_union_pattern_diagnostics_test
            zr_vm_language_server_local_semantic_query_test
            zr_vm_language_server_logical_semantic_query_test
            zr_vm_language_server_inlay_semantic_facts_test
            zr_vm_language_server_local_semantic_hover_test
            zr_vm_language_server_computed_member_hover_test
            zr_vm_language_server_expression_fact_hover_test
            zr_vm_language_server_reachability_semantic_query_test
            zr_vm_language_server_semantic_query_diagnostics_test
            zr_vm_language_server_reaching_definition_navigation_test
            zr_vm_language_server_numeric_semantic_query_test
            zr_vm_lsp_bitwise_zero_wrapper_range_query_test
            zr_vm_lsp_bitwise_zero_minus_unary_rhs_range_query_test
            zr_vm_lsp_bitwise_zero_minus_rhs_range_query_test
            zr_vm_lsp_bitwise_zero_minus_shift_exact_zero_count_range_query_test
            zr_vm_lsp_bitwise_zero_minus_shift_supported_count_range_query_test
            zr_vm_lsp_bzms_all_ones_query_test
            zr_vm_lsp_bzms_unary_zm_query_test
            zr_vm_lsp_bzms_not_zm_query_test
            zr_vm_lsp_bzms_double_zm_query_test
            zr_vm_lsp_bzms_neg_not_query_test
            zr_vm_lsp_bzms_not_unary_query_test
            zr_vm_lsp_bzms_zm_unary_query_test
            zr_vm_lsp_bzms_double_not_query_test
            zr_vm_lsp_bzms_double_neg_query_test
            zr_vm_lsp_bzms_supported_count_wrapped_not_query_test
            zr_vm_lsp_bzms_private_not_mask_query_test
            zr_vm_language_server_numeric_branch_semantic_query_test
            zr_vm_language_server_numeric_branch_assignment_semantic_query_test
            zr_vm_language_server_numeric_loop_self_dependency_semantic_query_test
            zr_vm_language_server_numeric_loop_self_dependency_symbolic_semantic_query_test
            zr_vm_lsp_selfdep_conditional_delta_query_test
            zr_vm_lsp_selfdep_conditional_delta_target_unary_query_test
            zr_vm_lsp_selfdep_conditional_delta_target_positive_query_test
            zr_vm_lsp_selfdep_conditional_delta_negative_offset_expr_query_test
            zr_vm_lsp_selfdep_neg_product_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_add_query_test
            zr_vm_lsp_selfdep_conditional_delta_negative_offset_identity_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_identity_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_sum_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_unary_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_subtract_query_test
            zr_vm_lsp_selfdep_neg_zero_incl_product_query_test
            zr_vm_lsp_selfdep_neg_zero_product_unary_same_diff_test
            zr_vm_lsp_selfdep_neg_zero_incl_diff_query_test
            zr_vm_lsp_selfdep_conditional_delta_bitwise_offset_query_test
            zr_vm_lsp_selfdep_bitwise_idempotent_wrapper_offset_test
            zr_vm_lsp_selfdep_conditional_delta_bitwise_unary_offset_query_test
            zr_vm_language_server_numeric_loop_self_dependency_symbolic_coefficient_semantic_query_test
            zr_vm_lsp_selfdep_coeff_cross_query_test
            zr_vm_lsp_selfdep_coeff_cross_deeper_scale_query_test
            zr_vm_lsp_selfdep_coeff_cross_three_additional_scale_query_test
            zr_vm_lsp_selfdep_coeff_cross_four_additional_scale_query_test
            zr_vm_lsp_selfdep_coeff_cross_four_scale_reader_query_test
            zr_vm_lsp_selfdep_coeff_cross_five_additional_scale_query_test
            zr_vm_lsp_selfdep_coeff_cross_six_additional_scale_query_test
            zr_vm_lsp_selfdep_coeff_cross_seven_additional_scale_query_test
            zr_vm_language_server_numeric_loop_self_dependency_symbolic_product_semantic_query_test
            zr_vm_language_server_numeric_loop_constant_condition_semantic_query_test
            zr_vm_language_server_numeric_for_header_assignment_semantic_query_test
            zr_vm_language_server_numeric_nested_loop_assignment_semantic_query_test
            zr_vm_language_server_numeric_foreach_cardinality_semantic_query_test
            zr_vm_language_server_call_member_semantic_query_test
            zr_vm_language_server_semantic_query_parity_test
            zr_vm_language_server_incremental_parser_test
            zr_vm_language_server_lsp_incremental_equivalence_test
            zr_vm_language_server_parser_diagnostics_test
            zr_vm_language_server_declaration_parser_diagnostics_test
            zr_vm_language_server_statement_parser_diagnostics_test
            zr_vm_language_server_class_member_parser_diagnostics_test
            zr_vm_language_server_lsp_position_mapping_test
            zr_vm_language_server_lsp_position_fuzz_test
            zr_vm_language_server_lsp_uri_test
            zr_vm_language_server_lsp_decorator_utf16_ranges_test
            zr_vm_language_server_lsp_code_lens_utf16_ranges_test
            zr_vm_language_server_lsp_super_utf16_ranges_test
            zr_vm_language_server_lsp_token_metadata_utf16_ranges_test
            zr_vm_language_server_lsp_source_contracts_test
            zr_vm_language_server_lsp_interface_test
            zr_vm_language_server_lsp_project_features_test
            zr_vm_language_server_lsp_project_utf16_ranges_test
            zr_vm_language_server_lsp_advanced_editor_features_test
            zr_vm_language_server_lsp_language_feature_matrix_test)
        target_include_directories(${language_server_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
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
    target_include_directories(zr_vm_language_server_ownership_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_union_pattern_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_local_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_logical_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_inlay_semantic_facts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_reachability_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_semantic_query_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_reaching_definition_navigation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bitwise_zero_wrapper_range_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bitwise_zero_minus_unary_rhs_range_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bitwise_zero_minus_rhs_range_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bitwise_zero_minus_shift_exact_zero_count_range_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bitwise_zero_minus_shift_supported_count_range_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_all_ones_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_unary_zm_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_not_zm_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_double_zm_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_neg_not_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_not_unary_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_zm_unary_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_double_not_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_double_neg_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_supported_count_wrapped_not_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_bzms_private_not_mask_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_branch_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_branch_assignment_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_loop_self_dependency_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_loop_self_dependency_symbolic_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_bitwise_offset_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_bitwise_idempotent_wrapper_offset_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_target_unary_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_target_positive_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_negative_offset_expr_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_product_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_add_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_negative_offset_identity_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_identity_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_sum_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_unary_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_subtract_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_product_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_product_unary_same_diff_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_neg_zero_incl_diff_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_conditional_delta_bitwise_unary_offset_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_loop_self_dependency_symbolic_coefficient_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_deeper_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_three_additional_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_four_additional_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_four_scale_reader_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_five_additional_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_six_additional_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_lsp_selfdep_coeff_cross_seven_additional_scale_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_loop_self_dependency_symbolic_product_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_loop_constant_condition_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_for_header_assignment_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_nested_loop_assignment_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_numeric_foreach_cardinality_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_call_member_semantic_query_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    target_include_directories(zr_vm_language_server_semantic_query_parity_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_language_server_semantic_query_parity_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_container_shared
        )
    else ()
        target_link_libraries(zr_vm_language_server_semantic_query_parity_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_container_static
        )
    endif ()

    target_include_directories(zr_vm_language_server_lsp_interface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_project_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_project_utf16_ranges_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )

    target_compile_definitions(zr_vm_language_server_lsp_interface_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )
    target_compile_definitions(zr_vm_language_server_lsp_project_features_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
            ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH="$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_int>"
            ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH="$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_float>"
    )
    target_compile_definitions(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_ownership_diagnostics_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_union_pattern_diagnostics_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_project_utf16_ranges_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
    else ()
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_ownership_diagnostics_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_union_pattern_diagnostics_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_project_utf16_ranges_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
    endif ()

