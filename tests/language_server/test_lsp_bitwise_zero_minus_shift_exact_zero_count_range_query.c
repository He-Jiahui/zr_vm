#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_shift_exact_zero_count_range_query(SZrState *state,
                                                      const TZrChar *caseName,
                                                      const TZrChar *expression,
                                                      TZrInt64 expectedMin,
                                                      TZrInt64 expectedMax) {
    TZrChar content[1024];
    TZrChar label[256];
    TZrChar uri[256];
    int written;

    written = snprintf(
            content,
            sizeof(content),
            "func calc(seed: u8): int {\n"
            "    var unit: int = (seed %% 2) + 2;\n"
            "    var zero: int = 0;\n"
            "    var span: int = seed - 128;\n"
            "    return %s;\n"
            "}\n",
            expression);
    if (written <= 0 || (size_t)written >= sizeof(content)) {
        printf("FAIL: unable to format %s source\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            label,
            sizeof(label),
            "bitwise zero-minus shift exact-zero count %s range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s label\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            uri,
            sizeof(uri),
            "file:///local_%s_bitwise_zero_minus_shift_exact_zero_count_range_fact.zr",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s uri\n", caseName);
        return ZR_FALSE;
    }

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
            content,
            "|",
            0,
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
    passed = run_shift_exact_zero_count_range_query(
                     state,
                     "or_zero_left_shift_left_same_identifier_difference_count_zero_minus_rhs",
                     "(zero + zero) | ((zero << (span - span)) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_exact_zero_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_or_two_exact_zero_count_zero_minus_rhs",
                     "(zero + zero) | ((zero >> ((span - span) | (unit - unit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = run_shift_exact_zero_count_range_query(
                     state,
                     "or_zero_left_shift_left_same_identifier_difference_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero << (span - span)) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = run_shift_exact_zero_count_range_query(
                     state,
                     "or_zero_left_shift_right_bitwise_xor_two_exact_zero_count_zero_minus_unary_rhs",
                     "(zero + zero) | ((zero >> ((span - span) ^ (unit - unit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Minus Shift Exact-Zero Count Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
