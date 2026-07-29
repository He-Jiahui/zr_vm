#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_positive_bitwise_idempotent_wrapper_offset_query(
        SZrState *state,
        const TZrChar *caseName,
        const TZrChar *offsetExpression) {
    TZrChar content[1024];
    TZrChar label[256];
    TZrChar uri[256];
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
            "    var unit: int = (seed %% 2) + 2;\n"
            "    var zero: int = 0;\n"
            "    var step: int = (seed %% 3) + 1;\n"
            "    var bias: int = (seed %% 2) + 1;\n"
            "    while (flag) {\n"
            "        other = narrowed - (%s);\n"
            "        mirror = other;\n"
            "        narrowed = narrowed + (choose ? step : step + bias);\n"
            "    }\n"
            "    narrowed + 0;\n"
            "    other + 0;\n"
            "    return mirror + 0;\n"
            "}\n",
            offsetExpression);
    if (written <= 0 || (size_t)written >= sizeof(content)) {
        printf("FAIL: unable to format %s source\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            label,
            sizeof(label),
            "while self-dependent target-reading %s target range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s target label\n", caseName);
        return ZR_FALSE;
    }
    written = snprintf(uri, sizeof(uri), "file:///local_%s_target_numeric_range_fact.zr", caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s target uri\n", caseName);
        return ZR_FALSE;
    }
    narrowedPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
            content,
            "    narrowed + 0;",
            strlen("    narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);

    written = snprintf(
            label,
            sizeof(label),
            "while self-dependent target-reading %s observer range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s observer label\n", caseName);
        return ZR_FALSE;
    }
    written = snprintf(uri, sizeof(uri), "file:///local_%s_observer_numeric_range_fact.zr", caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s observer uri\n", caseName);
        return ZR_FALSE;
    }
    otherPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
            content,
            "other + 0",
            strlen("other "),
            0,
            ZR_TYPE_RANGE_INT64_MAX - 2);

    written = snprintf(
            label,
            sizeof(label),
            "while self-dependent target-reading %s mirror range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s mirror label\n", caseName);
        return ZR_FALSE;
    }
    written = snprintf(uri, sizeof(uri), "file:///local_%s_mirror_numeric_range_fact.zr", caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s mirror uri\n", caseName);
        return ZR_FALSE;
    }
    mirrorPassed = ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
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
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_and_add_zero_left_wrapper_offset",
                    "(unit + zero) & unit") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_and_add_zero_right_wrapper_offset",
                    "unit & (zero + unit)") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_and_subtract_zero_left_wrapper_offset",
                    "(unit - zero) & unit") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_and_subtract_zero_right_wrapper_offset",
                    "unit & (unit - zero)") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_or_add_zero_left_wrapper_offset",
                    "(zero + unit) | unit") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_or_add_zero_right_wrapper_offset",
                    "unit | (unit + zero)") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_or_subtract_zero_left_wrapper_offset",
                    "(unit - zero) | unit") &&
            run_positive_bitwise_idempotent_wrapper_offset_query(
                    state,
                    "bitwise_or_subtract_zero_right_wrapper_offset",
                    "unit | (unit - zero)");
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Bitwise Idempotent Wrapper Offset Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
