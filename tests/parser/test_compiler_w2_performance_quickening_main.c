#include "unity.h"

void test_matrix_add_2d_compile_binds_super_array_items_for_hot_typed_int_paths(void);
void test_w2_load_typed_arithmetic_probe_reports_residual_candidates(void);
void test_w2_dispatch_loops_materialized_constant_signed_arithmetic_fuses(void);
void test_w2_signed_equality_branch_fuses_slot_operands(void);
void test_w2_signed_greater_equal_branch_reuses_greater_signed_jump(void);
void test_w2_left_constant_add_mul_fold_to_existing_const_opcodes(void);
void test_w2_right_constant_mod_fold_uses_cfg_liveness_across_branch(void);
void test_w2_late_forward_get_stack_after_member_call_specialization(void);

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_matrix_add_2d_compile_binds_super_array_items_for_hot_typed_int_paths);
    RUN_TEST(test_w2_load_typed_arithmetic_probe_reports_residual_candidates);
    RUN_TEST(test_w2_dispatch_loops_materialized_constant_signed_arithmetic_fuses);
    RUN_TEST(test_w2_signed_equality_branch_fuses_slot_operands);
    RUN_TEST(test_w2_signed_greater_equal_branch_reuses_greater_signed_jump);
    RUN_TEST(test_w2_left_constant_add_mul_fold_to_existing_const_opcodes);
    RUN_TEST(test_w2_right_constant_mod_fold_uses_cfg_liveness_across_branch);
    RUN_TEST(test_w2_late_forward_get_stack_after_member_call_specialization);

    return UNITY_END();
}
