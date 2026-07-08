#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero - (unit + unit)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((span - span) - ((unit + unit) | (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero | (zero - (unit + unit))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((span - span) - ((unit + unit) | (span - span))) ^ (span - span)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero - (unit + unit)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((span - span) - ((unit + unit) | (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero | (zero - (unit + unit))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((span - span) - ((unit + unit) | (span - span))) ^ (span - span)))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_zero_minus_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_minus_exact_zero_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_or_inner_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_inner_zero_minus_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
