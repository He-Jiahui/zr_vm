#include <stdio.h>
#include <string.h>

#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"
#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_shift_supported_count_range_query(SZrState *state,
                                                     const TZrChar *caseName,
                                                     const TZrChar *expression,
                                                     TZrInt64 expectedMin,
                                                     TZrInt64 expectedMax) {
    return ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
            state,
            caseName,
            expression,
            expectedMin,
            expectedMax);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(
            ZrVmTest_LspNumericRangeQueryAllocator,
            ZR_NULL,
            12345,
            &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    passed = ZR_TRUE;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_additive_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (unit + unit)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_additive_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (unit + unit)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_additive_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (unit + unit)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_additive_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (unit + unit)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_additive_exact_zero_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (unit + (span - span))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (unit - (span - span))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_additive_exact_zero_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (unit + (span - span))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (unit - (span - span))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & unit)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (unit & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & unit)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (unit & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & (zero - (negativeUnit & negativeUnit)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((zero - (negativeUnit | (negativeUnit - zero))) & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & (zero - (negativeUnit & negativeUnit)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((zero - (negativeUnit | (negativeUnit - zero))) & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & (-(zero - (unit & unit))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((-(zero - (unit | (unit - zero)))) & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_unary_minus_zero_minus_same_identifier_and_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & (-(zero - (unit & unit))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_unary_minus_zero_minus_same_identifier_or_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((-(zero - (unit | (unit - zero)))) & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (allOnes & ((~negativeFour) + (span - span)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (((~(zero - unit)) - (span - span)) & allOnes)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_mask_bitwise_not_direct_zero_identity_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (allOnes & ((~negativeFour) + (span - span)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_mask_bitwise_not_zero_minus_zero_identity_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (((~(zero - unit)) - (span - span)) & allOnes)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((-(zero - allOnes)) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - allOnes)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_all_ones_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((-(zero - allOnes)) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_all_ones_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - allOnes)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_chain_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (zero - (zero - unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_chain_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_chain_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (zero - (zero - unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_chain_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (-negativeUnit)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (-negativeUnit)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~negativeFour)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(zero - unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~negativeFour)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(zero - unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(-unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(-unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((allOnes | unit) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (unit | allOnes))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((allOnes | unit) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (unit | allOnes))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((zero - (zero - (allOnes | unit))) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (unit | allOnes))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((zero - (zero - (allOnes | unit))) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (unit | allOnes))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (((allOnes | unit) + (span - span)) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & ((unit | allOnes) - (span - span)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_all_ones_or_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (((allOnes | unit) + (span - span)) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_all_ones_or_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & ((unit | allOnes) - (span - span)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((~(span - span)) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (~(unit - unit)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((~(span - span)) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (~(unit - unit)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((zero - (zero - (~(span - span)))) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (~(unit - unit)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((zero - (zero - (~(span - span)))) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (zero - (zero - (~(unit - unit)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << ((-(zero - (~(span - span)))) & (~negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - (~(unit - unit)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_direct_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << ((-(zero - (~(span - span)))) & (~negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_side_mask_bitwise_not_zero_minus_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((~(zero - unit)) & (-(zero - (~(unit - unit)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((~(span - span)) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (~(unit - unit))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((~(span - span)) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (~(unit - unit))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(((~(span - span)) + (unit - unit)) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((~(unit - unit)) - (span - span))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(((~(span - span)) + (unit - unit)) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((~(unit - unit)) - (span - span))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - (~(span - span)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (~(unit - unit))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - (~(span - span)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (~(unit - unit))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(((zero - (zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((zero - (zero - (~(unit - unit)))) - (span - span))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(((zero - (zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((zero - (zero - (~(unit - unit)))) - (span - span))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) - (span - span))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((~(unit - unit)) - (span - span))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - (~(span - span)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (~(unit - unit))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - (~(span - span)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (~(unit - unit))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(((-(zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((-(zero - (~(unit - unit)))) - (span - span))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(((-(zero - (~(span - span)))) + (unit - unit)) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_zero_identity_unary_minus_zero_minus_bitwise_not_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & ((-(zero - (~(unit - unit)))) - (span - span))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) - (span - span))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) + (unit - unit)))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_unary_minus_zero_minus_bitwise_not_zero_identity_leaf_exact_zero_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((~(unit - unit)) - (span - span))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(negativeFour & ((span - span) ^ (~(span - span)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeFour & ((unit - unit) | (~(unit - unit)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(negativeFour & ((span - span) ^ (~(span - span)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_exact_zero_bitwise_identity_wrapped_all_ones_mask_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeFour & ((unit - unit) | (~(unit - unit)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(allOnes | unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(unit | allOnes))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(allOnes | unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(unit | allOnes))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_supported_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Minus Shift Supported-Count Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
