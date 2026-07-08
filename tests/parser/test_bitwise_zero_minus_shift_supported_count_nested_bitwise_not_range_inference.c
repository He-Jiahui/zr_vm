#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(zero - (zero - (~negativeFour)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(zero - (zero - (~negativeFour)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(zero - (zero - (~negativeFour)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(zero - (zero - (~negativeFour)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(-(zero - (~negativeFour)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(-(zero - (~negativeFour)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(-(zero - (~negativeFour)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(-(zero - (~negativeFour)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~((~negativeFour) + (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((~negativeFour) - (unit - unit))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~((~negativeFour) + (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((~negativeFour) - (unit - unit))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(zero - (~(~negativeFour)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(zero - (~(zero - (zero - (~negativeFour)))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(zero - (~(~negativeFour)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~(zero - (~(zero - (zero - (~negativeFour)))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero | (~(zero - (~(~negativeFour))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - ((~(zero - (~(zero - (zero - (~negativeFour)))))) ^ (span - span)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero | (~(zero - (~(~negativeFour))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - ((~(zero - (~(zero - (zero - (~negativeFour)))))) ^ (span - span)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~((zero - (~(~negativeFour))) | (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((unit - unit) & (zero - (~(zero - (zero - (~negativeFour))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~((zero - (~(~negativeFour))) | (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((unit - unit) & (zero - (~(zero - (zero - (~negativeFour))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (~negativeFour))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((~(zero - unit)) & allOnes)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (~negativeFour))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((~(zero - unit)) & allOnes)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (zero - (zero - (~negativeFour))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((-(zero - (~negativeFour))) & allOnes)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (zero - (zero - (~negativeFour))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((-(zero - (~negativeFour))) & allOnes)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (zero - (~(~negativeFour))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((zero - (~(zero - (zero - (~negativeFour))))) & allOnes)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (~(allOnes & (zero - (~(~negativeFour))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (~((zero - (~(zero - (zero - (~negativeFour))))) & allOnes)))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_or_wrapped_rhs_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_rhs_zero_minus_zero_minus_bitwise_not_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_wrapped_supported_count_bitwise_or_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_wrapped_supported_count_bitwise_and_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_unary_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_wrapped_direct_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_wrapped_zero_minus_zero_minus_bitwise_not_count_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
