#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

/* 以零包含负偏移的观察者读取和条件增量并置，验证目标与转存值的区间关系；正负增量成对约束方向，防止把含零偏移误判为严格负值。 */
static TZrBool test_local_expression_query_keeps_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var maybeDown: int = (seed % 2) - 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (zero - maybeDown);\n"
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
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

/* 以零包含负偏移的观察者读取和条件增量并置，验证目标与转存值的区间关系；正负增量成对约束方向，防止把含零偏移误判为严格负值。 */
static TZrBool test_local_expression_query_keeps_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_negative_delta_guarded(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var maybeDown: int = (seed % 2) - 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (zero - maybeDown);\n"
        "        mirror = other;\n"
        "        narrowed = narrowed - (choose ? step : step + bias);\n"
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
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero-minus zero-inclusive negative binding offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

/* 此可执行文件由 CTest 的 language_server 套件调用，汇总零包含负偏移与条件增量用例；创建 VM state 后调用本文件场景，并在退出前释放全局状态。 */
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
    passed = test_local_expression_query_keeps_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_positive_delta_range(state) &&
             test_local_expression_query_keeps_subtract_zero_minus_zero_inclusive_negative_binding_offset_conditional_negative_delta_guarded(state);
    printf(
            "%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Zero-Inclusive Negative Offset Conditional Delta Range\n",
            passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
