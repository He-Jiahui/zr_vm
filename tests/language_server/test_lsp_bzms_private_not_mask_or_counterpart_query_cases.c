#include "test_lsp_bzms_private_not_mask_or_counterpart_query_cases.h"

#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"

TZrBool ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartQueries(SZrState *state) {
    TZrBool passed;

    passed = ZR_TRUE;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_xor_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) ^ (span - span))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_xor_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) ^ (span - span))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_xor_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) ^ (span - span))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit - unit)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_xor_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) ^ (span - span))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((zero - (zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (zero - (zero - (~(unit - unit)))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((-(zero - (~(span - span)))) & allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_unary_zero_minus_all_ones_mask_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes & (-(zero - (~(unit - unit)))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (allOnes | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | allOnes))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | allOnes))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (allOnes | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | unit))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (unit | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | unit))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (unit | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | unit))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (unit | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | unit))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (unit | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit | (span - span))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((unit ^ (unit - unit)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (unit | (span - span))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((unit ^ (unit - unit)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit | (span - span))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((unit ^ (unit - unit)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (unit | (span - span))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_bitwise_identity_wrapped_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((unit ^ (unit - unit)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (span & span)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((span | (span - zero)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (span & span)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((span | (span - zero)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (span & span)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((span | (span - zero)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (span & span)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_same_identifier_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((span | (span - zero)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (~negativeFour)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~negativeFour) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (~negativeFour)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~negativeFour) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (~negativeFour)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~negativeFour) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (~negativeFour)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~negativeFour) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(~negativeFour))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(~negativeFour)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(~negativeFour))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(~negativeFour)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(~negativeFour))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(~negativeFour)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(~negativeFour))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(~negativeFour)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (~negativeFour))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (~negativeFour)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (~negativeFour))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (~negativeFour)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (~negativeFour))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (~negativeFour)) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (~negativeFour))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (~negativeFour)) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (~negativeFour)))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (~negativeFour)))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (~negativeFour)))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (~negativeFour)))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (~negativeFour)))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (~negativeFour)))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (~negativeFour)))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (~negativeFour)))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (zero - (~negativeFour)))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (zero - (~negativeFour))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (zero - (~negativeFour)))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (zero - (~negativeFour))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (zero - (~negativeFour)))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (zero - (~negativeFour))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (zero - (~negativeFour)))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (zero - (~negativeFour))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (zero - (~negativeFour))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (zero - (~negativeFour))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (zero - (~negativeFour))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (zero - (~negativeFour))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (zero - (~negativeFour)))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (zero - (~negativeFour)))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (zero - (~negativeFour)))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (zero - (~negativeFour)))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (zero - (~negativeFour))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (zero - (~negativeFour))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (zero - (~negativeFour)))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (zero - (~negativeFour))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (zero - (~negativeFour)))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (zero - (~negativeFour))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (zero - (~negativeFour)))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (zero - (~negativeFour))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_unary_zero_minus_bitwise_not_all_ones_or_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (zero - (~negativeFour)))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (zero - (~negativeFour))))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (zero - (~negativeFour))))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (zero - (~negativeFour))))))) | (~(unit - unit)))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (zero - (~negativeFour))))))) | (~(unit - unit)))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    return passed;
}
