#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_keeps_target_reading_target_side_positive_range_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + unit;\n"
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
            "while self-dependent target-reading target-side positive range-offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side positive range-offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side positive range-offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_target_side_positive_range_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + unit;\n"
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
            "while self-dependent target-reading target-side positive range-offset conditional positive delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_positive_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side positive range-offset conditional positive delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_positive_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side positive range-offset conditional positive delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_positive_range_offset_conditional_positive_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_commuted_positive_range_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = unit + (+narrowed);\n"
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
            "while self-dependent target-reading commuted positive range-offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted positive range-offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted positive range-offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_commuted_positive_range_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = unit + (+narrowed);\n"
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
            "while self-dependent target-reading commuted positive range-offset conditional positive delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_positive_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted positive range-offset conditional positive delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_positive_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted positive range-offset conditional positive delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_positive_range_offset_conditional_positive_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_side_negative_range_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + down;\n"
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
            "while self-dependent target-reading target-side negative range-offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side negative range-offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side negative range-offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_negative_delta_target_side_negative_range_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + down;\n"
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
            "while self-dependent target-reading target-side negative range-offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side negative range-offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading target-side negative range-offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_target_side_negative_range_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_commuted_negative_range_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = down + (+narrowed);\n"
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
            "while self-dependent target-reading commuted negative range-offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted negative range-offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted negative range-offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_negative_delta_commuted_negative_range_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = down + (+narrowed);\n"
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
            "while self-dependent target-reading commuted negative range-offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted negative range-offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading commuted negative range-offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_commuted_negative_range_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_unary_negative_range_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (-unit);\n"
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
            "while self-dependent target-reading unary negative range-offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary negative range-offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary negative range-offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_negative_delta_unary_negative_range_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (-unit);\n"
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
            "while self-dependent target-reading unary negative range-offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary negative range-offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary negative range-offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_negative_range_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_unary_minus_negative_binding_positive_offset_conditional_negative_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (-down);\n"
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
            "while self-dependent target-reading unary-minus negative binding positive offset conditional negative delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_negative_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary-minus negative binding positive offset conditional negative delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_negative_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary-minus negative binding positive offset conditional negative delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_negative_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            ZR_TYPE_RANGE_INT64_MIN + 1,
            7);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_positive_delta_unary_minus_negative_binding_positive_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var down: int = -((seed % 2) + 1);\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (-down);\n"
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
            "while self-dependent target-reading unary-minus negative binding positive offset conditional positive delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_positive_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary-minus negative binding positive offset conditional positive delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_positive_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading unary-minus negative binding positive offset conditional positive delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_unary_minus_negative_binding_positive_offset_conditional_positive_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_zero_minus_positive_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (zero - unit);\n"
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
            "while self-dependent target-reading zero-minus positive offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-minus positive offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-minus positive offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_negative_delta_zero_minus_positive_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (zero - unit);\n"
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
            "while self-dependent target-reading zero-minus positive offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-minus positive offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-minus positive offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_minus_positive_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            0);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_bounded_negative_subtractive_offset_conditional_positive_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var pad: int = 1;\n"
        "    var larger: int = (seed % 2) + 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (pad - larger);\n"
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
            "while self-dependent target-reading bounded strict negative subtractive offset conditional positive delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_positive_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading bounded strict negative subtractive offset conditional positive delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_positive_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading bounded strict negative subtractive offset conditional positive delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_positive_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_negative_delta_bounded_negative_subtractive_offset_guarded(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var pad: int = 1;\n"
        "    var larger: int = (seed % 2) + 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (+narrowed) + (pad - larger);\n"
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
            "while self-dependent target-reading bounded strict negative subtractive offset conditional negative delta guarded target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_negative_delta_guarded_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading bounded strict negative subtractive offset conditional negative delta guarded observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_negative_delta_guarded_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            0);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading bounded strict negative subtractive offset conditional negative delta guarded mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_bounded_strict_negative_subtractive_offset_conditional_negative_delta_guarded_mirror_numeric_range_fact.zr",
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
            test_local_expression_query_keeps_target_reading_target_side_positive_range_offset_conditional_negative_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_positive_delta_target_side_positive_range_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_commuted_positive_range_offset_conditional_negative_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_positive_delta_commuted_positive_range_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_target_side_negative_range_offset_conditional_positive_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_negative_delta_target_side_negative_range_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_commuted_negative_range_offset_conditional_positive_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_negative_delta_commuted_negative_range_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_unary_negative_range_offset_conditional_positive_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_negative_delta_unary_negative_range_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_unary_minus_negative_binding_positive_offset_conditional_negative_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_positive_delta_unary_minus_negative_binding_positive_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_zero_minus_positive_offset_conditional_positive_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_negative_delta_zero_minus_positive_offset_guarded(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_bounded_negative_subtractive_offset_conditional_positive_delta_range(
                    state);
    passed =
            passed &&
            test_local_expression_query_keeps_negative_delta_bounded_negative_subtractive_offset_guarded(
                    state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Target-Side Positive Conditional Negative Delta Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
