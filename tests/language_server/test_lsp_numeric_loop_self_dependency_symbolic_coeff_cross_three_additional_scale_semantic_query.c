#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_positive_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level positive singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_positive_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_positive_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 2) + 2;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level positive non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_positive_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = seed % 2;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level zero-inclusive positive scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 2) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level zero-inclusive negative scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_negative_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 2) - 2;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level negative non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_negative_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_sign_crossing_scale_product_coefficient_residual(
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
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level sign-crossing scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_sign_crossing_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_preserves_target_reading_symbolic_deeper_three_additional_level_zero_only_scale_product_coefficient_noop(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = 0;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (scale * (outer * (span * (mask * gate))))));\n"
        "        other = narrowed;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper three-additional-level zero-only scale-product coefficient no-op target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_three_additional_level_zero_only_scale_product_coefficient_noop_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
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
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_positive_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_positive_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_negative_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_three_additional_level_sign_crossing_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_preserves_target_reading_symbolic_deeper_three_additional_level_zero_only_scale_product_coefficient_noop(
                state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Target-Reading Symbolic Three-Additional Scale-Product Sign-Crossing Coefficient Residual\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
