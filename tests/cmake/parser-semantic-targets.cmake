    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_minus_rhs_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_rhs_range_inference.c
    )
    target_include_directories(zr_vm_bitwise_zero_minus_rhs_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_minus_rhs_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_minus_shift_exact_zero_count_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_shift_exact_zero_count_range_inference.c
    )
    target_include_directories(zr_vm_bitwise_zero_minus_shift_exact_zero_count_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_minus_shift_exact_zero_count_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_minus_shift_supported_count_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_shift_supported_count_range_inference.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bitwise_zero_minus_shift_supported_count_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_minus_shift_supported_count_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_all_ones_count_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_all_ones_count_range_inference.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_all_ones_count_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_all_ones_count_range_inference_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_all_ones_count_range_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_all_ones_count_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_unary_zm_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_unary_zm_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_unary_zm_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_unary_zm_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_unary_zm_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_unary_zm_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_not_zm_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_not_zm_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_not_zm_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_not_zm_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_not_zm_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_not_zm_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_double_zm_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_double_zm_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_double_zm_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_double_zm_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_double_zm_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_double_zm_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_neg_not_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_neg_not_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_neg_not_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_neg_not_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_neg_not_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_neg_not_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_not_unary_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_not_unary_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_not_unary_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_not_unary_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_not_unary_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_not_unary_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_zm_unary_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_zm_unary_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_zm_unary_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_zm_unary_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_zm_unary_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_zm_unary_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_double_not_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_double_not_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_double_not_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_double_not_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_double_not_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_double_not_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_double_neg_count_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bzms_double_neg_count_range.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_double_neg_count_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_double_neg_count_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_double_neg_count_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_double_neg_count_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bzms_private_not_mask_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_shift_supported_count_private_bitwise_not_mask_operand_range_inference.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_private_not_mask_or_leaf_cases.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_private_not_mask_or_deep_leaf_cases.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bzms_private_not_mask_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bzms_private_not_mask_range_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_private_not_mask_range_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bzms_private_not_mask_range_test)

    zr_vm_add_unity_test_target(
            zr_vm_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference.c
            ${CMAKE_SOURCE_DIR}/tests/parser/bitwise_zero_minus_shift_supported_count_range_test_support.c
    )
    target_include_directories(zr_vm_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    set_target_properties(
            zr_vm_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference_test
            PROPERTIES OUTPUT_NAME zr_vm_bzms_nested_not_range_test)
    zr_vm_link_parser_core_plus_library(zr_vm_bitwise_zero_minus_shift_supported_count_nested_bitwise_not_range_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_branch_refinement_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_branch_refinement.c
    )
    target_include_directories(zr_vm_numeric_branch_refinement_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_branch_refinement_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_branch_segment_sets_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_branch_segment_sets.c
    )
    target_include_directories(zr_vm_numeric_branch_segment_sets_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_branch_segment_sets_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_branch_assignment_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_branch_assignment_dataflow.c
    )
    target_include_directories(zr_vm_numeric_branch_assignment_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_branch_assignment_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_assignment_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_assignment_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_assignment_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_assignment_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_self_dependency_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_self_dependency_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_self_dependency_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_self_dependency_symbolic_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_self_dependency_symbolic_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_self_dependency_symbolic_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_offset_expression_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_offset_expression_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_offset_expression_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_offset_expression_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_target_unary_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_target_unary_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_target_unary_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_target_unary_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_target_positive_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_target_positive_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_target_positive_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_target_positive_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_expression_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_expression_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_expression_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_expression_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_product_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_product_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_product_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_product_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_add_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_additive_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_add_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_add_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_identity_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_identity_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_identity_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_identity_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_sum_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_sum_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_sum_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_sum_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_unary_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_unary_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_unary_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_unary_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_cond_delta_neg_zero_incl_subtract_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_subtractive_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_cond_delta_neg_zero_incl_subtract_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_cond_delta_neg_zero_incl_subtract_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_product_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_product_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_product_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_product_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_selfdep_neg_zero_product_unary_same_diff_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_selfdep_neg_zero_product_unary_same_diff.c
    )
    target_include_directories(zr_vm_selfdep_neg_zero_product_unary_same_diff_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_selfdep_neg_zero_product_unary_same_diff_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_neg_zero_incl_diff_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_negative_offset_zero_inclusive_difference_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_neg_zero_incl_diff_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_neg_zero_incl_diff_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_offset_expression_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_bitwise_offset_expression_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_offset_expression_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_offset_expression_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_selfdep_bitwise_idempotent_wrapper_offset_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_selfdep_bitwise_idempotent_wrapper_offset.c
    )
    target_include_directories(zr_vm_selfdep_bitwise_idempotent_wrapper_offset_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_selfdep_bitwise_idempotent_wrapper_offset_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_arith_offset_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_conditional_delta_bitwise_arithmetic_offset_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_arith_offset_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_selfdep_conditional_delta_bitwise_arith_offset_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_self_dependency_symbolic_coefficient_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coefficient_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_self_dependency_symbolic_coefficient_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_self_dependency_symbolic_coefficient_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_self_dependency_symbolic_coefficient_cross_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coefficient_cross_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_self_dependency_symbolic_coefficient_cross_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_self_dependency_symbolic_coefficient_cross_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_deeper_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_deeper_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_deeper_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_deeper_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_three_additional_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_three_additional_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_three_additional_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_three_additional_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_four_additional_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_reader_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_four_additional_scale_reader_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_reader_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_four_additional_scale_reader_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_five_additional_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_five_additional_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_five_additional_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_five_additional_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_six_additional_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_six_additional_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_six_additional_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_six_additional_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_selfdep_coeff_cross_seven_additional_scale_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_coeff_cross_seven_additional_scale_dataflow.c
    )
    target_include_directories(zr_vm_numeric_selfdep_coeff_cross_seven_additional_scale_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_selfdep_coeff_cross_seven_additional_scale_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_self_dependency_symbolic_product_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_self_dependency_symbolic_product_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_self_dependency_symbolic_product_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_self_dependency_symbolic_product_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_loop_constant_condition_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_loop_constant_condition_dataflow.c
    )
    target_include_directories(zr_vm_numeric_loop_constant_condition_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_loop_constant_condition_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_for_header_assignment_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_for_header_assignment_dataflow.c
    )
    target_include_directories(zr_vm_numeric_for_header_assignment_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_for_header_assignment_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_nested_loop_assignment_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_nested_loop_assignment_dataflow.c
    )
    target_include_directories(zr_vm_numeric_nested_loop_assignment_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_nested_loop_assignment_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_numeric_foreach_cardinality_dataflow_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_numeric_foreach_cardinality_dataflow.c
    )
    target_include_directories(zr_vm_numeric_foreach_cardinality_dataflow_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_numeric_foreach_cardinality_dataflow_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_fact_emission_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_reference_fact_emission.c
    )
    target_include_directories(zr_vm_reference_fact_emission_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_reference_fact_emission_test)

    zr_vm_add_unity_test_target(
            zr_vm_logical_fact_emission_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_logical_fact_emission.c
    )
    target_include_directories(zr_vm_logical_fact_emission_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_logical_fact_emission_test)

    zr_vm_add_unity_test_target(
            zr_vm_object_literal_key_lowering_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_object_literal_key_lowering.c
    )
    target_include_directories(zr_vm_object_literal_key_lowering_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_object_literal_key_lowering_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_pipeline_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_pipeline.c
    )
    target_include_directories(zr_vm_semir_pipeline_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_pipeline_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_static_c_types_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_static_c_types.c
    )
    target_include_directories(zr_vm_semir_static_c_types_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_static_c_types_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_typed_opcode_guardrails_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_typed_opcode_guardrails.c
    )
    target_include_directories(zr_vm_semir_typed_opcode_guardrails_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_typed_opcode_guardrails_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_type_conflict_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_type_conflict_deopt.c
    )
    target_include_directories(zr_vm_semir_type_conflict_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_type_conflict_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_dynamic_arithmetic_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_dynamic_arithmetic_deopt.c
    )
    target_include_directories(zr_vm_semir_dynamic_arithmetic_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_dynamic_arithmetic_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_dynamic_member_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_dynamic_member_deopt.c
    )
    target_include_directories(zr_vm_semir_dynamic_member_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_dynamic_member_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_dynamic_call_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_dynamic_call_deopt.c
    )
    target_include_directories(zr_vm_semir_dynamic_call_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_dynamic_call_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_dynamic_iter_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_dynamic_iter_deopt.c
    )
    target_include_directories(zr_vm_semir_dynamic_iter_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_dynamic_iter_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_dynamic_index_deopt_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_dynamic_index_deopt.c
    )
    target_include_directories(zr_vm_semir_dynamic_index_deopt_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_dynamic_index_deopt_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_source_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_source_contracts.c
    )
    target_include_directories(zr_vm_aot_c_source_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_source_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_reachability_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_reachability.c
    )
    target_include_directories(zr_vm_aot_reachability_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )

    if (MSVC AND CMAKE_GENERATOR STREQUAL "Ninja" AND NOT ZR_VM_TESTS_MACRO_FOUNDATION_ONLY)
        add_test(
                NAME msvc_compile_object_paths
                COMMAND ${CMAKE_COMMAND}
                "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DNINJA_EXE=${CMAKE_MAKE_PROGRAM}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc-object-paths-validation.cmake"
        )
        set_tests_properties(msvc_compile_object_paths PROPERTIES LABELS "build;msvc")
    endif ()
