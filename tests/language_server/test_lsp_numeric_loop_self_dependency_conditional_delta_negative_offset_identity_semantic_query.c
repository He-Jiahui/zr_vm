#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_keeps_subtract_negative_binding_plus_zero_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (down + zero);\n"
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
            "while self-dependent target-reading subtract negative binding plus zero offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_plus_zero_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract negative binding plus zero offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_plus_zero_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract negative binding plus zero offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_plus_zero_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_subtract_negative_binding_plus_zero_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (down + zero);\n"
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
            "while self-dependent target-reading positive delta subtract negative binding plus zero offset guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_plus_zero_offset_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract negative binding plus zero offset guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_plus_zero_offset_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract negative binding plus zero offset guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_plus_zero_offset_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_subtract_zero_plus_negative_binding_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (zero + down);\n"
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
            "while self-dependent target-reading subtract zero plus negative binding offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_plus_negative_binding_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero plus negative binding offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_plus_negative_binding_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract zero plus negative binding offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_zero_plus_negative_binding_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_subtract_zero_plus_negative_binding_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (zero + down);\n"
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
            "while self-dependent target-reading positive delta subtract zero plus negative binding offset guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_zero_plus_negative_binding_offset_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract zero plus negative binding offset guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_zero_plus_negative_binding_offset_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract zero plus negative binding offset guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_zero_plus_negative_binding_offset_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_subtract_negative_binding_minus_zero_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (down - zero);\n"
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
            "while self-dependent target-reading subtract negative binding minus zero offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_minus_zero_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract negative binding minus zero offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_minus_zero_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading subtract negative binding minus zero offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_subtract_negative_binding_minus_zero_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_subtract_negative_binding_minus_zero_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var down: int = (seed % 2) - 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) - (down - zero);\n"
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
            "while self-dependent target-reading positive delta subtract negative binding minus zero offset guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_minus_zero_offset_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract negative binding minus zero offset guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_minus_zero_offset_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive delta subtract negative binding minus zero offset guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_delta_subtract_negative_binding_minus_zero_offset_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
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
            test_local_expression_query_keeps_subtract_negative_binding_plus_zero_offset_conditional_negative_delta_range(
                    state) &&
            test_local_expression_query_keeps_positive_delta_subtract_negative_binding_plus_zero_offset_guarded(
                    state) &&
            test_local_expression_query_keeps_subtract_zero_plus_negative_binding_offset_conditional_negative_delta_range(
                    state) &&
            test_local_expression_query_keeps_positive_delta_subtract_zero_plus_negative_binding_offset_guarded(
                    state) &&
            test_local_expression_query_keeps_subtract_negative_binding_minus_zero_offset_conditional_negative_delta_range(
                    state) &&
            test_local_expression_query_keeps_positive_delta_subtract_negative_binding_minus_zero_offset_guarded(
                    state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Negative Offset Identity Conditional Delta Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
