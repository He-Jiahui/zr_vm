#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_propagates_reader_for_target_reading_symbolic_deeper_four_additional_level_sign_crossing_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (cover * (mask * gate)))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper four-additional-level sign-crossing scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_four_additional_level_sign_crossing_scale_product_coefficient_residual_reader_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper four-additional-level sign-crossing scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_four_additional_level_sign_crossing_scale_product_coefficient_residual_reader_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed;
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
        test_local_expression_query_propagates_reader_for_target_reading_symbolic_deeper_four_additional_level_sign_crossing_scale_product_coefficient_residual(
                state);
    printf("%s: LSP Local Expression Query Propagates Self-Dependent Target-Reading Symbolic Four-Additional Sign-Crossing Reader Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
