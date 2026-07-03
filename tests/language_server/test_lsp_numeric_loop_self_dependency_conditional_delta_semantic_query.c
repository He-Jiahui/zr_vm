#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_keeps_target_reading_identity_expression_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed + 0;\n"
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
            "while self-dependent target-reading identity-expression conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_identity_expression_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading identity-expression conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_identity_expression_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading identity-expression conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_identity_expression_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_zero_binding_identity_expression_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var zero: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed + zero;\n"
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
            "while self-dependent target-reading zero-binding identity-expression conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_binding_identity_expression_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-binding identity-expression conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_binding_identity_expression_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-binding identity-expression conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_binding_identity_expression_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_constant_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - 1;\n"
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
            "while self-dependent target-reading constant-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading constant-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading constant-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_constant_offset_identity_wrapper_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = (narrowed - 1) + 0;\n"
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
            "while self-dependent target-reading constant-offset identity-wrapper conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_identity_wrapper_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading constant-offset identity-wrapper conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_identity_wrapper_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading constant-offset identity-wrapper conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_constant_offset_identity_wrapper_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_singleton_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - unit;\n"
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
            "while self-dependent target-reading singleton-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading singleton-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading singleton-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_range_offset_conditional_delta_range(
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
        "        other = narrowed - unit;\n"
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
            "while self-dependent target-reading positive range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_additive_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var pad: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit + pad);\n"
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
            "while self-dependent target-reading positive additive range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_additive_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive additive range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_additive_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive additive range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_additive_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_subtractive_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 2;\n"
        "    var pad: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit - pad);\n"
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
            "while self-dependent target-reading positive subtractive range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_subtractive_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive subtractive range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_subtractive_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive subtractive range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_subtractive_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_multiplicative_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var factor: int = 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit * factor);\n"
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
            "while self-dependent target-reading positive multiplicative range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_multiplicative_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive multiplicative range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_multiplicative_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive multiplicative range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_multiplicative_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_divided_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 3) + 6;\n"
        "    var factor: int = 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit / factor);\n"
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
            "while self-dependent target-reading positive divided range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_divided_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive divided range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_divided_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 3);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive divided range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_divided_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 3);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_zero_inclusive_modulo_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 3) + 6;\n"
        "    var factor: int = 3;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit % factor);\n"
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
            "while self-dependent target-reading zero-inclusive modulo range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_modulo_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-inclusive modulo range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_modulo_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-inclusive modulo range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_modulo_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_left_shift_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 1;\n"
        "    var shift: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit << shift);\n"
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
            "while self-dependent target-reading positive left-shift range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_left_shift_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive left-shift range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_left_shift_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive left-shift range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_left_shift_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_right_shift_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 5) + 4;\n"
        "    var shift: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit >> shift);\n"
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
            "while self-dependent target-reading positive right-shift range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_right_shift_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive right-shift range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_right_shift_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive right-shift range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_right_shift_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_bitwise_and_identity_mask_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 2;\n"
        "    var mask: int = 3;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit & mask);\n"
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
            "while self-dependent target-reading positive bitwise-and identity-mask range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_and_identity_mask_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive bitwise-and identity-mask range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_and_identity_mask_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive bitwise-and identity-mask range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_and_identity_mask_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_positive_bitwise_or_identity_zero_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = (seed % 2) + 2;\n"
        "    var zero: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit | zero);\n"
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
            "while self-dependent target-reading positive bitwise-or identity-zero range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_or_identity_zero_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive bitwise-or identity-zero range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_or_identity_zero_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading positive bitwise-or identity-zero range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_positive_bitwise_or_identity_zero_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_zero_inclusive_positive_range_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = seed % 2;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - unit;\n"
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
            "while self-dependent target-reading zero-inclusive positive range-offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_positive_range_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-inclusive positive range-offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_positive_range_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-inclusive positive range-offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_inclusive_positive_range_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_zero_only_offset_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - unit;\n"
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
            "while self-dependent target-reading zero-only offset conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_only_offset_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-only offset conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_only_offset_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading zero-only offset conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_zero_only_offset_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_singleton_offset_identity_wrapper_conditional_delta_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, choose: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var unit: int = 1;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var bias: int = (seed % 2) + 1;\n"
        "    while (flag) {\n"
        "        other = narrowed - (unit + 0);\n"
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
            "while self-dependent target-reading singleton-offset identity-wrapper conditional delta target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_identity_wrapper_conditional_delta_target_numeric_range_fact.zr",
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading singleton-offset identity-wrapper conditional delta observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_identity_wrapper_conditional_delta_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading singleton-offset identity-wrapper conditional delta mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_singleton_offset_identity_wrapper_conditional_delta_mirror_numeric_range_fact.zr",
            content,
            "return mirror + 0",
            strlen("return mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 1);
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

    passed = test_local_expression_query_keeps_target_reading_identity_expression_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_zero_binding_identity_expression_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_constant_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_constant_offset_identity_wrapper_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_singleton_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_additive_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_subtractive_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_multiplicative_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_divided_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_zero_inclusive_modulo_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_left_shift_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_right_shift_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_bitwise_and_identity_mask_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_positive_bitwise_or_identity_zero_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_zero_inclusive_positive_range_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_zero_only_offset_conditional_delta_range(state) &&
             test_local_expression_query_keeps_target_reading_singleton_offset_identity_wrapper_conditional_delta_range(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Bitwise Identity Range-Offset Conditional Delta Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
