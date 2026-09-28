#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

/* 以额外六层乘积的正单点因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_positive_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level positive singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_positive_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的正非单点因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_positive_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = (seed % 2) + 2;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level positive non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_positive_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的含零正因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = seed % 2;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level zero-inclusive positive scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的含零负因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = (seed % 2) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level zero-inclusive negative scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的负非单点因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_negative_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = (seed % 2) - 2;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level negative non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_negative_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的跨零因子留下循环残差，验证目标的范围沿可达迭代拓宽。 */
static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_sign_crossing_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level sign-crossing scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_sign_crossing_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

/* 以额外六层乘积的纯零因子形成无效更新，验证目标初始区间不会因表达式深度被误拓宽。 */
static TZrBool test_local_expression_query_preserves_target_reading_symbolic_deeper_six_additional_level_zero_only_scale_product_coefficient_noop(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crest: int = 0;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate)))))))));\n"
        "        other = narrowed;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper six-additional-level zero-only scale-product coefficient no-op target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_six_additional_level_zero_only_scale_product_coefficient_noop_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
}

/* 此可执行文件由 CTest 的 language_server 套件调用，汇总额外六级乘积系数的自依赖循环用例；创建 VM state 后运行场景并释放全局状态。 */
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

    /* TODO: 用 && 串接场景会在首个失败后跳过其余变体；核查是否应逐项
     * 执行并汇总结果，使一次回归能显示完整失败范围。 */
    passed =
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_positive_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_positive_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_negative_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_six_additional_level_sign_crossing_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_preserves_target_reading_symbolic_deeper_six_additional_level_zero_only_scale_product_coefficient_noop(
                state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Target-Reading Symbolic Six-Additional Scale-Product Coefficient Residuals\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
