#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var pulse: int = 0;\n"
        "    var phase: int = 0;\n"
        "    var relay: int = 0;\n"
        "    var signal: int = 0;\n"
        "    var trace: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = 1;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        pulse = echo;\n"
        "        phase = pulse;\n"
        "        relay = phase;\n"
        "        signal = relay;\n"
        "        trace = signal;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    echo + 0;\n"
        "    pulse + 0;\n"
        "    return trace + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;
    TZrBool pulsePassed;
    TZrBool tracePassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "echo + 0",
            strlen("echo "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    pulsePassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual pulse assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_pulse_numeric_range_fact.zr",
            content,
            "pulse + 0",
            strlen("pulse "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    tracePassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive singleton scale-product coefficient residual trace assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual_trace_numeric_range_fact.zr",
            content,
            "return trace + 0",
            strlen("return trace "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed && pulsePassed && tracePassed;
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = (seed % 2) + 2;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        narrowed = narrowed + (step + step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive non-singleton scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -4,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive non-singleton scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            -4,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level positive non-singleton scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            -4,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = seed % 2;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive positive scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive positive scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive positive scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive positive scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = (seed % 2) - 1;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive negative scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive negative scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive negative scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-inclusive negative scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = (seed % 2) - 2;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level negative non-singleton scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level negative non-singleton scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level negative non-singleton scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            -1,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level negative non-singleton scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            -1,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = (seed % 3) - 1;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level sign-crossing scale-product coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level sign-crossing scale-product coefficient residual observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level sign-crossing scale-product coefficient residual mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level sign-crossing scale-product coefficient residual echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            0,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
}

static TZrBool test_local_expression_query_preserves_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var mirror: int = 0;\n"
        "    var echo: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var crown: int = 0;\n"
        "    var crest: int = (seed % 3) - 1;\n"
        "    var shell: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    var outer: int = 1;\n"
        "    var span: int = 1;\n"
        "    var cover: int = 1;\n"
        "    var mask: int = (seed % 3) - 1;\n"
        "    var gate: int = 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * (crown * (crest * (shell * (scale * (outer * (span * (cover * (mask * gate))))))))));\n"
        "        other = narrowed;\n"
        "        mirror = other;\n"
        "        echo = mirror;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    other + 0;\n"
        "    mirror + 0;\n"
        "    return echo + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;
    TZrBool echoPassed;

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-only scale-product coefficient no-op target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-only scale-product coefficient no-op observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            5);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-only scale-product coefficient no-op mirror assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop_mirror_numeric_range_fact.zr",
            content,
            "mirror + 0",
            strlen("mirror "),
            0,
            5);
    echoPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            "while self-dependent target-reading symbolic deeper seven-additional-level zero-only scale-product coefficient no-op echo assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop_echo_numeric_range_fact.zr",
            content,
            "return echo + 0",
            strlen("return echo "),
            0,
            5);

    return narrowedPassed && otherPassed && mirrorPassed && echoPassed;
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
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_positive_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_positive_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_positive_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_zero_inclusive_negative_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_negative_non_singleton_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_deeper_seven_additional_level_sign_crossing_scale_product_coefficient_residual(
                state) &&
        test_local_expression_query_preserves_target_reading_symbolic_deeper_seven_additional_level_zero_only_scale_product_coefficient_noop(
                state);
    printf("%s: LSP Local Expression Query Self-Dependent Target-Reading Symbolic Seven-Additional Bounded Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
