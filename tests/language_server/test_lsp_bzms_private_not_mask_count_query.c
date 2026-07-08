#include <stdio.h>
#include <string.h>

#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"
#include "lsp_numeric_range_query_test_support.h"
#include "test_lsp_bzms_private_not_mask_or_counterpart_deep_query_cases.h"
#include "test_lsp_bzms_private_not_mask_or_counterpart_query_cases.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

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
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_wrapped_left_all_ones_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(((span - span) ^ (~(span - span))) & negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_wrapped_left_all_ones_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(((unit - unit) | (~(unit - unit))) & negativeFour))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_wrapped_left_all_ones_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(((span - span) ^ (~(span - span))) & negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_wrapped_left_all_ones_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(((unit - unit) | (~(unit - unit))) & negativeFour))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_wrapped_count_side_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~((~(span - span)) & (negativeFour | (unit - unit))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_wrapped_count_side_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(((negativeFour ^ (span - span)) & (~(unit - unit)))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_private_not_mask_wrapped_count_side_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~((~(span - span)) & (negativeFour | (unit - unit))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_private_not_mask_wrapped_count_side_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(((negativeFour ^ (span - span)) & (~(unit - unit)))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartQueries(state) && passed;
    passed = ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartDeepQueries(state) && passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Minus Shift Private Bitwise-Not Mask Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
