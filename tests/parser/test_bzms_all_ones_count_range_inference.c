#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_all_ones_mask_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit + unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (((unit + unit) | (span - span)) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero | (~zero)) & (unit + unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit + unit) & (zero ^ (~zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit + unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (((unit + unit) | (span - span)) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero | (~zero)) & (unit + unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit + unit) & (zero ^ (~zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_or_identity_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_exact_zero_or_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_exact_zero_xor_bitwise_not_all_ones_side_count_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
