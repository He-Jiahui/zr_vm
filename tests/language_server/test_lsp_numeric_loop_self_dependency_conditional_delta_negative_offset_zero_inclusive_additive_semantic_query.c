#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_zero_inclusive_negative_additive_offset_query_case(
        SZrState *state,
        const char *label,
        const char *uriStem,
        const char *prefixExpression,
        const char *deltaExpression,
        TZrInt64 expectedTargetMin,
        TZrInt64 expectedTargetMax,
        TZrInt64 expectedObserverMin,
        TZrInt64 expectedObserverMax,
        TZrInt64 expectedMirrorMin,
        TZrInt64 expectedMirrorMax) {
    char content[2048];
    char targetDescription[256];
    char observerDescription[256];
    char mirrorDescription[256];
    char targetUri[256];
    char observerUri[256];
    char mirrorUri[256];
    int written;
    TZrBool narrowedPassed;
    TZrBool otherPassed;
    TZrBool mirrorPassed;

    written = snprintf(
            content,
            sizeof(content),
            "fn calc(flag: bool, choose: bool, seed: u8): int {\n"
            "    var narrowed: int = 5;\n"
            "    var other: int = 0;\n"
            "    var mirror: int = 0;\n"
            "    var zero: int = 0;\n"
            "    var maybeDown: int = (seed %% 2) - 1;\n"
            "    var down: int = (seed %% 2) - 2;\n"
            "    var zeroFactor: int = 0;\n"
            "    var maybeUnit: int = seed %% 2;\n"
            "    var span: int = (seed %% 3) - 1;\n"
            "    var unit: int = (seed %% 2) + 2;\n"
            "    var step: int = (seed %% 3) + 1;\n"
            "    var bias: int = (seed %% 2) + 1;\n"
            "    while (flag) {\n"
            "        other = %s;\n"
            "        mirror = other;\n"
            "        narrowed = narrowed %s;\n"
            "    }\n"
            "    narrowed + 0;\n"
            "    other + 0;\n"
            "    return mirror + 0;\n"
            "}\n",
            prefixExpression,
            deltaExpression);
    if (written <= 0 || (size_t)written >= sizeof(content)) {
        printf("FAIL: unable to build test content for %s\n", label);
        return ZR_FALSE;
    }
    written = snprintf(targetDescription, sizeof(targetDescription), "%s target assignment dataflow", label);
    if (written <= 0 || (size_t)written >= sizeof(targetDescription)) {
        return ZR_FALSE;
    }
    written = snprintf(observerDescription, sizeof(observerDescription), "%s observer assignment dataflow", label);
    if (written <= 0 || (size_t)written >= sizeof(observerDescription)) {
        return ZR_FALSE;
    }
    written = snprintf(mirrorDescription, sizeof(mirrorDescription), "%s mirror assignment dataflow", label);
    if (written <= 0 || (size_t)written >= sizeof(mirrorDescription)) {
        return ZR_FALSE;
    }
    written = snprintf(targetUri, sizeof(targetUri), "file:///local_%s_target_numeric_range_fact.zr", uriStem);
    if (written <= 0 || (size_t)written >= sizeof(targetUri)) {
        return ZR_FALSE;
    }
    written = snprintf(observerUri, sizeof(observerUri), "file:///local_%s_observer_numeric_range_fact.zr", uriStem);
    if (written <= 0 || (size_t)written >= sizeof(observerUri)) {
        return ZR_FALSE;
    }
    written = snprintf(mirrorUri, sizeof(mirrorUri), "file:///local_%s_mirror_numeric_range_fact.zr", uriStem);
    if (written <= 0 || (size_t)written >= sizeof(mirrorUri)) {
        return ZR_FALSE;
    }

    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            targetDescription,
            targetUri,
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            expectedTargetMin,
            expectedTargetMax);
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            observerDescription,
            observerUri,
            content,
            "other + 0",
            strlen("other "),
            expectedObserverMin,
            expectedObserverMax);
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            mirrorDescription,
            mirrorUri,
            content,
            "return mirror + 0",
            strlen("return mirror "),
            expectedMirrorMin,
            expectedMirrorMax);
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

    passed = run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add zero-inclusive negative offset conditional positive delta",
                     "while_self_dependent_target_reading_add_zero_inclusive_negative_offset_conditional_positive_delta",
                     "(+narrowed) + maybeDown",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add commuted zero-inclusive negative offset conditional positive delta",
                     "while_self_dependent_target_reading_add_commuted_zero_inclusive_negative_offset_conditional_positive_delta",
                     "maybeDown + (+narrowed)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add zero-inclusive zero-product offset conditional positive delta",
                     "while_self_dependent_target_reading_add_zero_inclusive_zero_product_offset_conditional_positive_delta",
                     "(+narrowed) + (maybeDown * zeroFactor)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add commuted zero-inclusive zero-product offset conditional positive delta",
                     "while_self_dependent_target_reading_add_commuted_zero_inclusive_zero_product_offset_conditional_positive_delta",
                     "(zeroFactor * maybeDown) + (+narrowed)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add zero-inclusive nonnegative zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_zero_inclusive_nonnegative_zero_product_offset_conditional_positive_delta",
                      "(+narrowed) + (maybeUnit * zeroFactor)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add commuted zero-inclusive nonnegative zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_commuted_zero_inclusive_nonnegative_zero_product_offset_conditional_positive_delta",
                      "(zeroFactor * maybeUnit) + (+narrowed)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add sign-crossing zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_sign_crossing_zero_product_offset_conditional_positive_delta",
                      "(+narrowed) + (span * zeroFactor)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add commuted sign-crossing zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_commuted_sign_crossing_zero_product_offset_conditional_positive_delta",
                      "(zeroFactor * span) + (+narrowed)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add sign-crossing unary identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_sign_crossing_unary_identity_zero_product_offset_conditional_positive_delta",
                      "(+narrowed) + ((+span) * zeroFactor)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add commuted sign-crossing unary identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_commuted_sign_crossing_unary_identity_zero_product_offset_conditional_positive_delta",
                      "(zeroFactor * (+span)) + (+narrowed)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add sign-crossing identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_sign_crossing_identity_zero_product_offset_conditional_positive_delta",
                      "(+narrowed) + ((span + zero) * zeroFactor)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add commuted sign-crossing identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_commuted_sign_crossing_identity_zero_product_offset_conditional_positive_delta",
                      "(zeroFactor * (zero + span)) + (+narrowed)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add sign-crossing subtractive identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_sign_crossing_subtractive_identity_zero_product_offset_conditional_positive_delta",
                      "(+narrowed) + ((span - zero) * zeroFactor)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add commuted sign-crossing subtractive identity zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_commuted_sign_crossing_subtractive_identity_zero_product_offset_conditional_positive_delta",
                      "(zeroFactor * (span - zero)) + (+narrowed)",
                      "+ (choose ? step : step + bias)",
                      5,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX,
                      0,
                      ZR_TYPE_RANGE_INT64_MAX) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading add bounded nonnegative zero-product offset conditional positive delta",
                      "while_self_dependent_target_reading_add_bounded_nonnegative_zero_product_offset_conditional_positive_delta",
                     "(+narrowed) + (unit * zeroFactor)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add commuted bounded nonnegative zero-product offset conditional positive delta",
                     "while_self_dependent_target_reading_add_commuted_bounded_nonnegative_zero_product_offset_conditional_positive_delta",
                     "(zeroFactor * unit) + (+narrowed)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add strict-negative zero-product offset conditional positive delta",
                     "while_self_dependent_target_reading_add_strict_negative_zero_product_offset_conditional_positive_delta",
                     "(+narrowed) + (down * zeroFactor)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading add commuted strict-negative zero-product offset conditional positive delta",
                     "while_self_dependent_target_reading_add_commuted_strict_negative_zero_product_offset_conditional_positive_delta",
                     "(zeroFactor * down) + (+narrowed)",
                     "+ (choose ? step : step + bias)",
                     5,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX,
                     0,
                     ZR_TYPE_RANGE_INT64_MAX) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add zero-inclusive negative offset guarded",
                     "while_self_dependent_target_reading_negative_delta_add_zero_inclusive_negative_offset_guarded",
                     "(+narrowed) + maybeDown",
                     "- (choose ? step : step + bias)",
                     5,
                     5,
                     0,
                     0,
                     0,
                     0) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add commuted zero-inclusive negative offset guarded",
                     "while_self_dependent_target_reading_negative_delta_add_commuted_zero_inclusive_negative_offset_guarded",
                     "maybeDown + (+narrowed)",
                     "- (choose ? step : step + bias)",
                     5,
                     5,
                     0,
                     0,
                     0,
                     0) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add zero-inclusive zero-product offset",
                     "while_self_dependent_target_reading_negative_delta_add_zero_inclusive_zero_product_offset",
                     "(+narrowed) + (maybeDown * zeroFactor)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add zero-inclusive nonnegative zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_zero_inclusive_nonnegative_zero_product_offset",
                      "(+narrowed) + (maybeUnit * zeroFactor)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add commuted zero-inclusive nonnegative zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_commuted_zero_inclusive_nonnegative_zero_product_offset",
                      "(zeroFactor * maybeUnit) + (+narrowed)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add sign-crossing zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_sign_crossing_zero_product_offset",
                      "(+narrowed) + (span * zeroFactor)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add commuted sign-crossing zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_commuted_sign_crossing_zero_product_offset",
                      "(zeroFactor * span) + (+narrowed)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add sign-crossing unary identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_sign_crossing_unary_identity_zero_product_offset",
                      "(+narrowed) + ((+span) * zeroFactor)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add commuted sign-crossing unary identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_commuted_sign_crossing_unary_identity_zero_product_offset",
                      "(zeroFactor * (+span)) + (+narrowed)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add sign-crossing identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_sign_crossing_identity_zero_product_offset",
                      "(+narrowed) + ((span + zero) * zeroFactor)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add commuted sign-crossing identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_commuted_sign_crossing_identity_zero_product_offset",
                      "(zeroFactor * (zero + span)) + (+narrowed)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add sign-crossing subtractive identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_sign_crossing_subtractive_identity_zero_product_offset",
                      "(+narrowed) + ((span - zero) * zeroFactor)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add commuted sign-crossing subtractive identity zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_commuted_sign_crossing_subtractive_identity_zero_product_offset",
                      "(zeroFactor * (span - zero)) + (+narrowed)",
                      "- (choose ? step : step + bias)",
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5,
                      ZR_TYPE_RANGE_INT64_MIN,
                      5) &&
              run_zero_inclusive_negative_additive_offset_query_case(
                      state,
                      "while self-dependent target-reading negative delta add bounded nonnegative zero-product offset",
                      "while_self_dependent_target_reading_negative_delta_add_bounded_nonnegative_zero_product_offset",
                      "(+narrowed) + (unit * zeroFactor)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add commuted bounded nonnegative zero-product offset",
                     "while_self_dependent_target_reading_negative_delta_add_commuted_bounded_nonnegative_zero_product_offset",
                     "(zeroFactor * unit) + (+narrowed)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add strict-negative zero-product offset",
                     "while_self_dependent_target_reading_negative_delta_add_strict_negative_zero_product_offset",
                     "(+narrowed) + (down * zeroFactor)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add commuted strict-negative zero-product offset",
                     "while_self_dependent_target_reading_negative_delta_add_commuted_strict_negative_zero_product_offset",
                     "(zeroFactor * down) + (+narrowed)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5) &&
             run_zero_inclusive_negative_additive_offset_query_case(
                     state,
                     "while self-dependent target-reading negative delta add commuted zero-inclusive zero-product offset",
                     "while_self_dependent_target_reading_negative_delta_add_commuted_zero_inclusive_zero_product_offset",
                     "(zeroFactor * maybeDown) + (+narrowed)",
                     "- (choose ? step : step + bias)",
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5,
                     ZR_TYPE_RANGE_INT64_MIN,
                     5);
    printf(
            "%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Zero-Inclusive Negative Additive Offset Conditional Delta Range\n",
            passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
