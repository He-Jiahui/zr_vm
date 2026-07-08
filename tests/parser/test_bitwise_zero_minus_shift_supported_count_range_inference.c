#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit + unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit + unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + (span - span))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit - (span - span))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + (span - span))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit - (span - span))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (zero - (negativeUnit & negativeUnit)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((zero - (negativeUnit | (negativeUnit - zero))) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (zero - (negativeUnit & negativeUnit)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((zero - (negativeUnit | (negativeUnit - zero))) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (-(zero - (unit & unit))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((-(zero - (unit | (unit - zero)))) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (-(zero - (unit & unit))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((-(zero - (unit | (unit - zero)))) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & ((~negativeFour) + (span - span)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (((~(zero - unit)) - (span - span)) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & ((~negativeFour) + (span - span)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (((~(zero - unit)) - (span - span)) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((-(zero - allOnes)) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - allOnes)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((-(zero - allOnes)) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - allOnes)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_chain_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_chain_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (-negativeUnit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (-negativeUnit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~negativeFour)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(zero - unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~negativeFour)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(zero - unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - (~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - (~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (-(zero - (~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (~negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (-(zero - (~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (~negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((allOnes | unit) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (unit | allOnes))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((allOnes | unit) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (unit | allOnes))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero - (zero - (allOnes | unit))) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (unit | allOnes))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero - (zero - (allOnes | unit))) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (unit | allOnes))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (((allOnes | unit) + (span - span)) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & ((unit | allOnes) - (span - span)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (((allOnes | unit) + (span - span)) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & ((unit | allOnes) - (span - span)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((~(span - span)) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (~(unit - unit)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((~(span - span)) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (~(unit - unit)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero - (zero - (~(span - span)))) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (~(unit - unit)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((zero - (zero - (~(span - span)))) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (~(unit - unit)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << ((-(zero - (~(span - span)))) & (~negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - (~(unit - unit)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << ((-(zero - (~(span - span)))) & (~negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - (~(unit - unit)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes | unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(unit | allOnes))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes | unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(unit | allOnes))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
