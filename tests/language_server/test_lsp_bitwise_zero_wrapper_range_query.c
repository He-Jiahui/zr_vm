#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"

static TZrBool run_bitwise_zero_wrapper_range_query_at(SZrState *state,
                                                       const TZrChar *caseName,
                                                       const TZrChar *expression,
                                                       const TZrChar *operatorNeedle,
                                                       int offset,
                                                       TZrInt64 expectedMin,
                                                       TZrInt64 expectedMax);

static TZrBool run_bitwise_zero_wrapper_range_query(SZrState *state,
                                                    const TZrChar *caseName,
                                                    const TZrChar *expression,
                                                    const TZrChar *operatorNeedle,
                                                    TZrInt64 expectedMin,
                                                    TZrInt64 expectedMax) {
    return run_bitwise_zero_wrapper_range_query_at(
            state,
            caseName,
            expression,
            operatorNeedle,
            0,
            expectedMin,
            expectedMax);
}

static TZrBool run_bitwise_zero_wrapper_range_query_at(SZrState *state,
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
            "bitwise zero-wrapper %s range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s label\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            uri,
            sizeof(uri),
            "file:///local_%s_bitwise_zero_wrapper_range_fact.zr",
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
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_unary_plus_zero_left",
                     "(+zero) & unit",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_additive_zero_right",
                     "unit & (zero + zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_subtract_zero_left",
                     "(zero - zero) & unit",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_unary_plus_zero_left",
                     "(+zero) | unit",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_additive_zero_right",
                     "unit | (zero + zero)",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_subtract_zero_left",
                     "(zero - zero) | unit",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_unary_plus_zero_right",
                     "unit ^ (+zero)",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_additive_zero_left",
                     "(zero + zero) ^ unit",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_subtract_zero_right",
                     "unit ^ (zero - zero)",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_unary_plus_zero_right_sign_crossing",
                     "span & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_additive_zero_right_sign_crossing",
                     "span & (zero + zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_subtract_zero_right_sign_crossing",
                     "span & (zero - zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_unary_plus_zero_left_sign_crossing",
                     "(+zero) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_additive_zero_left_sign_crossing",
                     "(zero + zero) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_subtract_zero_left_sign_crossing",
                     "(zero - zero) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_unary_plus_zero_left_sign_crossing",
                     "(+zero) ^ span",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_additive_zero_left_sign_crossing",
                     "(zero + zero) ^ span",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_subtract_zero_left_sign_crossing",
                     "(zero - zero) ^ span",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_unary_minus_zero_left",
                     "(-zero) & unit",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_unary_minus_zero_right_sign_crossing",
                     "span & (-zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_unary_minus_zero_left_sign_crossing",
                     "(-zero) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_unary_minus_zero_right_sign_crossing",
                     "span ^ (-zero)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_unary_minus_additive_zero_left_sign_crossing",
                     "(-(zero + zero)) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_unary_minus_direct_range_left_zero_right",
                     "(-unit) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_unary_minus_direct_range",
                     "(zero + zero) | (-unit)",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_unary_minus_direct_range",
                     "(zero - zero) ^ (-unit)",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_unary_minus_additive_identity_operand",
                     "(zero + zero) | (-(unit + zero))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_unary_minus_subtract_zero_operand",
                     "(zero - zero) ^ (-(unit - zero))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_right_unary_minus_additive_identity_operand",
                     "(-(zero + unit)) | (zero + zero)",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_zero_minus_direct_range",
                     "(zero + zero) | (zero - unit)",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_zero_minus_additive_identity_operand",
                     "(zero - zero) ^ (zero - (unit + zero))",
                     "^",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_zero_right_zero_minus_subtract_zero_operand",
                     "((+zero) - (unit - zero)) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_unary_minus_zero_minus_direct_operand",
                     "(zero + zero) | (-(zero - unit))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_unary_minus_zero_minus_additive_operand",
                     "(zero - zero) ^ (-(zero - (unit + zero)))",
                     "^",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_zero_right_unary_minus_zero_minus_subtract_operand",
                     "(-(zero - (unit - zero))) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_unary_minus_zero_minus_sign_crossing_operand",
                     "(zero + zero) | (-(zero - span))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_nested_unary_minus_zero_or",
                     "((-zero) | span) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_nested_unary_minus_zero_and",
                     "(zero + zero) | (span & (-zero))",
                     "|",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_nested_unary_minus_zero_or",
                     "(zero - zero) ^ ((-zero) | span)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "or_zero_right_nested_unary_minus_zero_xor_and",
                     "((span ^ (-zero)) & (+zero)) | (zero + zero)",
                     ")) | (",
                     3,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_same_identifier_and_sign_crossing",
                     "(span & span) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_wrapped_same_identifier_and_sign_crossing",
                     "((span + zero) & span) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_zero_right_wrapped_same_identifier_or_sign_crossing",
                     "(span | (span - zero)) & (+zero)",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_same_identifier_and_sign_crossing",
                     "(zero + zero) | (span & span)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_wrapped_same_identifier_and_sign_crossing",
                     "(+zero) | ((span + zero) & span)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_wrapped_same_identifier_or_sign_crossing",
                     "(zero - zero) ^ (span | (span - zero))",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_zero_left_same_identifier_xor_sign_crossing",
                     "(span ^ span) & span",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_zero_right_wrapped_same_identifier_xor_sign_crossing",
                     "span & (span ^ (span + zero))",
                     "&",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_wrapped_same_identifier_xor_sign_crossing",
                     "(span ^ (span - zero)) | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_right_wrapped_same_identifier_xor_sign_crossing",
                     "span ^ ((span + zero) ^ span)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_right_wrapped_same_identifier_xor_sign_crossing",
                     "(zero + zero) | (span ^ (span - zero))",
                     "|",
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "xor_both_zero_wrapped_same_identifier_xor_sign_crossing",
                     "(span ^ span) ^ (span ^ (span + zero))",
                     ") ^ (",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_sign_crossing",
                     "allOnes & span",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_mask_left_sign_crossing",
                     "(allOnes & span) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_sign_crossing",
                     "(zero + zero) | (allOnes & span)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_signed_all_ones_mask_left_sign_crossing",
                     "(zero - zero) ^ (allOnes & span)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_mask_right_sign_crossing",
                     "(span & allOnes) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_right_sign_crossing",
                     "(zero + zero) | (span & allOnes)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_signed_all_ones_mask_right_sign_crossing",
                     "(zero - zero) ^ (span & allOnes)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_additive_identity_mask_left",
                     "(allOnes + zero) & span",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_subtract_zero_identity_mask_right",
                     "span & (allOnes - zero)",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_additive_identity_mask",
                     "((zero + allOnes) & span) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_subtract_zero_identity_mask",
                     "(zero + zero) | ((allOnes - zero) & span)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_signed_all_ones_additive_identity_mask_right",
                     "(zero - zero) ^ (span & (zero + allOnes))",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_additive_identity_operand",
                     "allOnes & (span + zero)",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_mask_right_subtract_zero_operand",
                     "((span - zero) & allOnes) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_additive_identity_operand",
                     "(zero + zero) | (allOnes & (zero + span))",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "xor_zero_left_signed_all_ones_mask_right_subtract_zero_operand",
                     "(zero - zero) ^ ((span - zero) & allOnes)",
                     "^",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_unary_minus_direct_operand",
                     "allOnes & (-unit)",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_mask_right_unary_minus_direct_operand",
                     "((-unit) & allOnes) & (+zero)",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_unary_minus_direct_operand",
                     "(zero + zero) | (allOnes & (-unit))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_unary_minus_additive_identity_operand",
                     "allOnes & (-(unit + zero))",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_unary_minus_subtract_zero_operand",
                     "(zero + zero) | (allOnes & (-(unit - zero)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_zero_minus_direct_operand",
                     "allOnes & (zero - unit)",
                     "&",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_zero_minus_additive_identity_operand",
                     "(zero + zero) | (allOnes & (zero - (unit + zero)))",
                     "|",
                     -3,
                     -2) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_signed_all_ones_mask_left_unary_minus_zero_minus_direct_operand",
                     "allOnes & (-(zero - unit))",
                     "&",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_mask_left_unary_minus_zero_minus_operand",
                     "(zero + zero) | (allOnes & (-(zero - (unit + zero))))",
                     "|",
                     2,
                     3) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_same_identifier_sign_crossing",
                     "span & span",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "and_wrapped_same_identifier_sign_crossing",
                     "(span + zero) & span",
                     "&",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_same_identifier_sign_crossing",
                     "span | span",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_wrapped_same_identifier_sign_crossing",
                     "span | (span - zero)",
                     "|",
                     -128,
                     127) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_left",
                     "allOnes | span",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_right",
                     "span | allOnes",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_additive_identity_left",
                     "(allOnes + zero) | span",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_left_unary_minus_direct_operand",
                     "allOnes | (-unit)",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_left_unary_minus_additive_identity_operand",
                     "allOnes | (-(zero + unit))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_left_zero_minus_subtract_zero_operand",
                     "allOnes | (zero - (unit - zero))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_signed_all_ones_left_unary_minus_zero_minus_operand",
                     "allOnes | (-(zero - (unit - zero)))",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_or_unary_minus_direct_operand",
                     "((allOnes | (-unit)) & (+zero))",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_or_unary_minus_direct_operand",
                     "(zero + zero) | ((-unit) | allOnes)",
                     "|",
                     -1,
                     -1) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query_at(
                     state,
                     "and_zero_right_signed_all_ones_or_annihilator",
                     "((allOnes | span) & (+zero))",
                     ") & (+zero)",
                     2,
                     0,
                     0) &&
             passed;
    passed = run_bitwise_zero_wrapper_range_query(
                     state,
                     "or_zero_left_signed_all_ones_or_annihilator",
                     "(zero + zero) | (span | (zero + allOnes))",
                     "|",
                     -1,
                     -1) &&
             passed;

    printf("%s: LSP Local Expression Query Keeps Bitwise Zero-Wrapper Range\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
