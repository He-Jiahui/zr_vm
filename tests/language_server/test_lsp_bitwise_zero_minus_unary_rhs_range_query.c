#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_bitwise_zero_minus_unary_rhs_range_query_at(SZrState *state,
                                                               const TZrChar *caseName,
                                                               const TZrChar *expression,
                                                               const TZrChar *operatorNeedle,
                                                               int offset,
                                                               TZrInt64 expectedMin,
                                                               TZrInt64 expectedMax);

static TZrBool run_bitwise_zero_minus_unary_rhs_range_query(SZrState *state,
                                                            const TZrChar *caseName,
                                                            const TZrChar *expression,
                                                            const TZrChar *operatorNeedle,
                                                            TZrInt64 expectedMin,
                                                            TZrInt64 expectedMax) {
    return run_bitwise_zero_minus_unary_rhs_range_query_at(
            state,
            caseName,
            expression,
            operatorNeedle,
            0,
            expectedMin,
            expectedMax);
}

static TZrBool run_bitwise_zero_minus_unary_rhs_range_query_at(SZrState *state,
                                                               const TZrChar *caseName,
                                                               const TZrChar *expression,
                                                               const TZrChar *operatorNeedle,
                                                               int offset,
                                                               TZrInt64 expectedMin,
                                                               TZrInt64 expectedMax) {
    TZrChar content[1024];
    TZrChar label[256];
    TZrChar uri[256];
    int written;

    written = snprintf(
            content,
            sizeof(content),
            "fn calc(seed: u8): int {\n"
            "    var unit: int = (seed %% 2) + 2;\n"
            "    var zero: int = 0;\n"
            "    var allOnes: int = 0 - 1;\n"
            "    var span: int = seed - 128;\n"
            "    return %s;\n"
            "}\n",
            expression);
    if (written <= 0 || (size_t)written >= sizeof(content)) {
        printf("FAIL: unable to format %s source\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            label,
            sizeof(label),
            "bitwise zero-minus unary-rhs %s range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s label\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            uri,
            sizeof(uri),
            "file:///local_%s_bitwise_zero_minus_unary_rhs_range_fact.zr",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s uri\n", caseName);
        return ZR_FALSE;
    }

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
            content,
            operatorNeedle,
            offset,
            expectedMin,
            expectedMax);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(
            ZrVmTest_LspNumericRangeQueryAllocator,
            ZR_NULL,
            12345,
            &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    passed = ZR_TRUE;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_direct_rhs",
                     "(zero + zero) | (zero - (-unit))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_additive_rhs",
                     "(zero - zero) ^ (zero - (-(unit + zero)))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_subtract_rhs",
                     "(zero - (-(unit - zero))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-span))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_rhs",
                     "allOnes & (zero - (-unit))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(unit + zero))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_rhs",
                     "allOnes | (zero - (-(unit - zero)))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_basic_zero_minus_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (unit + zero))))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_basic_zero_minus_subtract_rhs",
                     "(zero - (-(zero - (unit - zero)))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - span)))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_basic_zero_minus_rhs",
                     "allOnes & (zero - (-(zero - unit)))",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (unit + zero)))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_basic_zero_minus_rhs",
                     "allOnes | (zero - (-(zero - (unit - zero))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (-unit))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_basic_zero_minus_unary_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (-(unit + zero)))))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_basic_zero_minus_unary_subtract_rhs",
                     "(zero - (-(zero - (-(unit - zero))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (-span))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_basic_zero_minus_unary_rhs",
                     "allOnes & (zero - (-(zero - (-unit))))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (-(unit + zero))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_basic_zero_minus_unary_rhs",
                     "allOnes | (zero - (-(zero - (-(unit - zero)))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - unit))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_nested_basic_zero_minus_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (unit + zero)))))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_nested_basic_zero_minus_subtract_rhs",
                     "(zero - (-(zero - (zero - (unit - zero))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_nested_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - span))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_nested_basic_zero_minus_rhs",
                     "allOnes & (zero - (-(zero - (zero - unit))))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (unit + zero))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_nested_basic_zero_minus_rhs",
                     "allOnes | (zero - (-(zero - (zero - (unit - zero)))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (-unit)))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_nested_basic_zero_minus_unary_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (-(unit + zero))))))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_nested_basic_zero_minus_unary_subtract_rhs",
                     "(zero - (-(zero - (zero - (-(unit - zero)))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_nested_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (-span)))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_nested_basic_zero_minus_unary_rhs",
                     "allOnes & (zero - (-(zero - (zero - (-unit)))))",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (-(unit + zero)))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_nested_basic_zero_minus_unary_rhs",
                     "allOnes | (zero - (-(zero - (zero - (-(unit - zero))))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - unit)))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (zero - (unit + zero))))))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_triple_nested_basic_zero_minus_subtract_rhs",
                     "(zero - (-(zero - (zero - (zero - (unit - zero)))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - span)))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_triple_nested_basic_zero_minus_rhs",
                     "allOnes & (zero - (-(zero - (zero - (zero - unit)))))",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_triple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (zero - (unit + zero)))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_triple_nested_basic_zero_minus_rhs",
                     "allOnes | (zero - (-(zero - (zero - (zero - (unit - zero))))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (-unit))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_unary_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (zero - (-(unit + zero)))))))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_triple_nested_basic_zero_minus_unary_subtract_rhs",
                     "(zero - (-(zero - (zero - (zero - (-(unit - zero))))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_triple_nested_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (-span))))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_triple_nested_basic_zero_minus_unary_rhs",
                     "allOnes & (zero - (-(zero - (zero - (zero - (-unit))))))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_triple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (zero - (-(unit + zero))))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_triple_nested_basic_zero_minus_unary_rhs",
                     "allOnes | (zero - (-(zero - (zero - (zero - (-(unit - zero)))))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - unit))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (zero - (zero - (unit + zero)))))))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_quadruple_nested_basic_zero_minus_subtract_rhs",
                     "(zero - (-(zero - (zero - (zero - (zero - (unit - zero))))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - span))))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_quadruple_nested_basic_zero_minus_rhs",
                     "allOnes & (zero - (-(zero - (zero - (zero - (zero - unit))))))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_quadruple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (zero - (zero - (unit + zero))))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_quadruple_nested_basic_zero_minus_rhs",
                     "allOnes | (zero - (-(zero - (zero - (zero - (zero - (unit - zero)))))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (-unit)))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "xor_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_additive_rhs",
                     "(zero - zero) ^ (zero - (-(zero - (zero - (zero - (zero - (-(unit + zero))))))))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_zero_right_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_subtract_rhs",
                     "(zero - (-(zero - (zero - (zero - (zero - (-(unit - zero)))))))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (-span)))))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "and_signed_all_ones_left_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_rhs",
                     "allOnes & (zero - (-(zero - (zero - (zero - (zero - (-unit)))))))",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_signed_all_ones_and_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (allOnes & (zero - (-(zero - (zero - (zero - (zero - (-(unit + zero)))))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_unary_quadruple_nested_basic_zero_minus_unary_rhs",
                     "allOnes | (zero - (-(zero - (zero - (zero - (zero - (-(unit - zero))))))))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quintuple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - unit)))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quintuple_nested_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - span)))))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quintuple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (-unit))))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_quintuple_nested_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (-span))))))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_sextuple_nested_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (zero - unit))))))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_sextuple_nested_basic_zero_minus_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (zero - span))))))))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_sextuple_nested_basic_zero_minus_unary_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (zero - (-unit)))))))))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_sextuple_nested_basic_zero_minus_unary_sign_crossing_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero - (zero - (zero - (zero - (zero - (-span)))))))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_signed_all_ones_mask_leaf",
                     "(zero + zero) | (zero - (-(zero - (allOnes & (span + zero)))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_same_identifier_leaf",
                     "(zero + zero) | (zero - (-(zero - (span | (span - zero)))))",
                     "|",
                     -127,
                     128) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_signed_all_ones_or_left_leaf",
                     "(zero + zero) | (zero - (-(zero - (allOnes | span))))",
                     "|",
                     1,
                     1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_basic_zero_minus_signed_all_ones_or_right_leaf",
                     "(zero + zero) | (zero - (-(zero - (span | allOnes))))",
                     "|",
                     1,
                     1) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_bitwise_and_zero_right_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((span & zero) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_bitwise_and_zero_left_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero & span) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_same_identifier_difference_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((span - span) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_same_identifier_difference_right_wrapper_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((span - (span - zero)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_same_identifier_modulo_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((unit % unit) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_same_identifier_modulo_right_wrapper_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((unit % (unit - zero)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_product_zero_right_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((span * zero) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_product_zero_left_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero * span) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_division_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero / unit) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_division_right_wrapper_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero / (unit - zero)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_modulo_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero % unit) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_modulo_right_wrapper_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero % (unit - zero)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_shift_left_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero << unit) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_shift_right_right_wrapper_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | ((zero >> (unit - zero)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_exact_zero_shift_left_leaf_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero << unit))))",
                     "|",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_zero_minus_unary_exact_zero_shift_right_right_wrapper_leaf_basic_zero_minus_rhs",
                     "(zero + zero) | (zero - (-(zero - (zero >> (unit - zero)))))",
                     "|",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_product_two_exact_zero_leaves_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (((span - span) * (unit - unit)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_bitwise_and_two_exact_zero_leaves_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (((span - span) & (unit - unit)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_bitwise_or_two_exact_zero_leaves_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (((span - span) | (unit - unit)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_minus_unary_rhs_range_query(
                     state,
                     "or_zero_left_bitwise_xor_two_exact_zero_leaves_zero_minus_unary_basic_zero_minus_rhs",
                     "(zero + zero) | (((span - span) ^ (unit - unit)) - (-(zero - unit)))",
                     "|",
                     -3,
                     -2) &&
             passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Minus Unary-RHS Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
