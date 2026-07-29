#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_keeps_target_reading_nested_commuted_bitwise_identity_zero_over_unary_plus_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var unit: int = (seed % 2) + 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (zero | (+unit));\n"
        "        mirror = other;\n"
        "        narrowed = narrowed + (choose ? step : step + bias);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    return mirror + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading nested commuted bitwise identity-zero over unary-plus range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_nested_commuted_bitwise_identity_zero_over_unary_plus_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading nested commuted bitwise identity-zero over unary-plus range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_nested_commuted_bitwise_identity_zero_over_unary_plus_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading nested commuted bitwise identity-zero over unary-plus range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_nested_commuted_bitwise_identity_zero_over_unary_plus_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(ZrVmTest_LspNumericRangeQueryAllocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    passed =
            test_local_expression_query_keeps_target_reading_nested_commuted_bitwise_identity_zero_over_unary_plus_range_offset_conditional_delta_range(
                    state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Bitwise Unary Offset Conditional Delta Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
