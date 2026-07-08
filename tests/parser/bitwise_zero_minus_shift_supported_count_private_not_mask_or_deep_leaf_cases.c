#include "bitwise_zero_minus_shift_supported_count_private_not_mask_or_deep_leaf_cases.h"

#include "unity.h"

#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)));",
            -3,
            -2);
}

void register_bitwise_zero_minus_shift_supported_count_private_not_mask_or_deep_leaf_cases(void) {
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
}
