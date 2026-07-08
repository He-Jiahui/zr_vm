#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_private_not_mask_or_deep_leaf_cases.h"
#include "bitwise_zero_minus_shift_supported_count_private_not_mask_or_leaf_cases.h"
#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((~(span - span)) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (~(unit - unit))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((~(span - span)) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (~(unit - unit))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((~(span - span)) + (unit - unit)) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((~(unit - unit)) - (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((~(span - span)) + (unit - unit)) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((~(unit - unit)) - (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & ((span - span) ^ (~(span - span)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour & ((unit - unit) | (~(unit - unit)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & ((span - span) ^ (~(span - span)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour & ((unit - unit) | (~(unit - unit)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((span - span) ^ (~(span - span))) & negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((unit - unit) | (~(unit - unit))) & negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((span - span) ^ (~(span - span))) & negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((unit - unit) | (~(unit - unit))) & negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((~(span - span)) & (negativeFour | (unit - unit))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((negativeFour ^ (span - span)) & (~(unit - unit)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((~(span - span)) & (negativeFour | (unit - unit))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(((negativeFour ^ (span - span)) & (~(unit - unit)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - (~(span - span)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (~(unit - unit))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - (~(span - span)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (~(unit - unit))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((zero - (zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((zero - (zero - (~(unit - unit)))) - (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((zero - (zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((zero - (zero - (~(unit - unit)))) - (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) - (span - span))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) - (span - span))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) ^ (span - span))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) ^ (span - span))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - (~(span - span)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (~(unit - unit))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - (~(span - span)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (~(unit - unit))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((-(zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((-(zero - (~(unit - unit)))) - (span - span))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(((-(zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & ((-(zero - (~(unit - unit)))) - (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) - (span - span))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) - (span - span))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) ^ (span - span))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) ^ (span - span))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (~(unit - unit)))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (~(unit - unit)))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (~(unit - unit)))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (~(unit - unit)))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_left_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_mask_count_side_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_bitwise_identity_leaf_all_ones_mask_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_identity_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    register_bitwise_zero_minus_shift_supported_count_private_not_mask_or_leaf_cases();
    register_bitwise_zero_minus_shift_supported_count_private_not_mask_or_deep_leaf_cases();
    return UNITY_END();
}
