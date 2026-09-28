#include <stdio.h>
#include <string.h>

#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"
#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

/* 双重按位取反的计数树在范围分析中应恢复受支持计数；左右移与两种
 * 右侧符号成对运行，验证最外层位或查询仍得到同一窄范围。 */
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
                     "or_zero_left_shift_left_double_bitwise_not_additive_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(~(unit + unit)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_double_bitwise_not_exact_zero_or_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~(~((unit + unit) | (span - span))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_double_bitwise_not_exact_zero_or_inner_bitwise_not_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (~(zero | (~(unit + unit))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_double_bitwise_not_inner_bitwise_not_count_xor_exact_zero_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (~((~((unit + unit) | (span - span))) ^ (span - span)))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_double_bitwise_not_additive_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(~(unit + unit)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_double_bitwise_not_exact_zero_or_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~(~((unit + unit) | (span - span))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_left_double_bitwise_not_exact_zero_or_inner_bitwise_not_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (~(zero | (~(unit + unit))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zero_left_shift_right_double_bitwise_not_inner_bitwise_not_count_xor_exact_zero_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> (~((~((unit + unit) | (span - span))) ^ (span - span)))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Minus Shift Double Bitwise-Not Count Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
