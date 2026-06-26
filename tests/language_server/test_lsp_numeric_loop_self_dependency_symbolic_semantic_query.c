#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static TZrBool find_position_for_substring_offset(const TZrChar *content,
                                                  const TZrChar *needle,
                                                  TZrSize offset,
                                                  SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize remainingOffset = offset;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }
    while (*cursor != '\0' && remainingOffset > 0) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
        remainingOffset--;
    }

    outPosition->line = line;
    outPosition->character = character;
    return remainingOffset == 0;
}

static TZrBool run_assignment_range_case_at(SZrState *state,
                                            const TZrChar *label,
                                            const TZrChar *uriText,
                                            const TZrChar *content,
                                            const TZrChar *needle,
                                            TZrSize offset,
                                            TZrInt64 expectedMin,
                                            TZrInt64 expectedMax) {
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool expectUnsignedRange;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring_offset(
                content,
                needle,
                offset,
                &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare %s local query fixture\n", label);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for %s range\n", label);
        return ZR_FALSE;
    }

    expectUnsignedRange = expectedMin >= 0 && expectedMax >= 0;
    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact != ZR_NULL &&
             (query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
              query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_RANGE) &&
             query.numericFact->targetType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact->hasRange &&
             query.numericFact->minValue == expectedMin &&
             query.numericFact->maxValue == expectedMax &&
             query.numericFact->hasUnsignedRange == expectUnsignedRange &&
             (!expectUnsignedRange ||
              (query.numericFact->minUnsignedValue == (TZrUInt64)expectedMin &&
               query.numericFact->maxUnsignedValue == (TZrUInt64)expectedMax)) &&
             !query.numericFact->mayOverflow;

    if (!passed) {
        printf("FAIL: expected %s range fact; status=%d expr=%p exprKind=%d exprType=%d "
               "numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld hasUnsigned=%d "
               "umin=%llu umax=%llu mayOverflow=%d\n",
               label,
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               (void *)query.numericFact,
               query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->targetType : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : 0LL,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : 0LL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->minUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->maxUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -123,
            132);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_additive_zero_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + 0);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic additive-zero expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_additive_zero_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic additive-zero expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_additive_zero_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -123,
            132);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step + 1);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + 1);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -122,
            133);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_commuted_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step + 1);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (1 + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic commuted expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_commuted_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic commuted expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_commuted_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -122,
            133);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_associative_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + ((step + 1) + 2);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + (1 + 2));\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic associative expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_associative_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic associative expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_associative_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -120,
            135);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_folded_integer_sum_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + ((step + 1) + 2);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + 3);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded integer-sum expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_integer_sum_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded integer-sum expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_integer_sum_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -120,
            135);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_signed_additive_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + ((step + 5) - 2);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + 3);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic signed-additive expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_signed_additive_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic signed-additive expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_signed_additive_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -120,
            135);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_same_side_term_cancellation_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    var bias: int = seed + 3;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + ((step + bias) - bias);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic same-side term cancellation expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_same_side_term_cancellation_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic same-side term cancellation expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_same_side_term_cancellation_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -123,
            132);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_unary_negative_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (-step);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-negative expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_negative_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-negative expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_negative_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -122,
            133);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_unary_positive_expression_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (+step);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-positive expression net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_positive_expression_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-positive expression net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_positive_expression_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -123,
            132);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_residual_net_negative_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + 1);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic residual net-negative target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_residual_net_negative_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic residual net-negative observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_residual_net_negative_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            ZR_TYPE_RANGE_INT64_MIN,
            132);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_multi_residual_net_positive_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    var bias: int = seed + 3;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + bias);\n"
        "        narrowed = narrowed + (bias + 1);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multi-residual net-positive target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multi_residual_net_positive_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multi-residual net-positive observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multi_residual_net_positive_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -123,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_multi_residual_prefix_range(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 100;\n"
        "    var step: int = seed - 128;\n"
        "    var bias: int = seed + 3;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "        narrowed = narrowed - (step + bias);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (bias + 1);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multi-residual prefix target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multi_residual_prefix_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multi-residual prefix observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multi_residual_prefix_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -253,
            ZR_TYPE_RANGE_INT64_MAX);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_literal_coefficient_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * 2);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic literal-coefficient target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_literal_coefficient_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic literal-coefficient observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_literal_coefficient_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -251,
            259);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_folded_coefficient_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (1 + 1));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded-coefficient target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_coefficient_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded-coefficient observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_coefficient_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -251,
            259);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_multiplicative_coefficient_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (2 * 1));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multiplicative-coefficient target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multiplicative_coefficient_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic multiplicative-coefficient observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_multiplicative_coefficient_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -251,
            259);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_keeps_target_reading_symbolic_negative_literal_coefficient_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * -2);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic negative literal-coefficient target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_negative_literal_coefficient_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic negative literal-coefficient observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_negative_literal_coefficient_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            -249,
            261);
    return narrowedPassed && otherPassed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool targetReadingSymbolicNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicAdditiveZeroExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicFoldedIntegerSumExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicSignedAdditiveExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicSameSideTermCancellationExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicUnaryNegativeExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicUnaryPositiveExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicResidualNetNegativeDeltaPassed;
    TZrBool targetReadingSymbolicMultiResidualNetPositiveDeltaPassed;
    TZrBool targetReadingSymbolicMultiResidualPrefixRangePassed;
    TZrBool targetReadingSymbolicLiteralCoefficientNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicFoldedCoefficientNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicMultiplicativeCoefficientNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicNegativeLiteralCoefficientNetZeroDeltaPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Loop Self-Dependency Symbolic Semantic Query Tests\n");
    printf("===================================================================\n");
    targetReadingSymbolicNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Net-Zero Delta\n",
           targetReadingSymbolicNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicAdditiveZeroExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_additive_zero_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Additive-Zero Expression Net-Zero Delta\n",
           targetReadingSymbolicAdditiveZeroExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Expression Net-Zero Delta\n",
           targetReadingSymbolicExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_commuted_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Commuted Expression Net-Zero Delta\n",
           targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_associative_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Associative Expression Net-Zero Delta\n",
           targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicFoldedIntegerSumExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_folded_integer_sum_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Folded Integer-Sum Expression Net-Zero Delta\n",
           targetReadingSymbolicFoldedIntegerSumExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicSignedAdditiveExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_signed_additive_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Signed-Additive Expression Net-Zero Delta\n",
           targetReadingSymbolicSignedAdditiveExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicSameSideTermCancellationExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_same_side_term_cancellation_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Same-Side Term Cancellation Expression Net-Zero Delta\n",
           targetReadingSymbolicSameSideTermCancellationExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicUnaryNegativeExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_unary_negative_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Unary-Negative Expression Net-Zero Delta\n",
           targetReadingSymbolicUnaryNegativeExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicUnaryPositiveExpressionNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_unary_positive_expression_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Unary-Positive Expression Net-Zero Delta\n",
           targetReadingSymbolicUnaryPositiveExpressionNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicResidualNetNegativeDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_residual_net_negative_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Residual Net-Negative Delta\n",
           targetReadingSymbolicResidualNetNegativeDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicMultiResidualNetPositiveDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_multi_residual_net_positive_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Multi-Residual Net-Positive Delta\n",
           targetReadingSymbolicMultiResidualNetPositiveDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicMultiResidualPrefixRangePassed =
        test_local_expression_query_keeps_target_reading_symbolic_multi_residual_prefix_range(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Multi-Residual Prefix Range\n",
           targetReadingSymbolicMultiResidualPrefixRangePassed ? "PASS" : "FAIL");
    targetReadingSymbolicLiteralCoefficientNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_literal_coefficient_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Literal-Coefficient Net-Zero Delta\n",
           targetReadingSymbolicLiteralCoefficientNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicFoldedCoefficientNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_folded_coefficient_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Folded-Coefficient Net-Zero Delta\n",
           targetReadingSymbolicFoldedCoefficientNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicMultiplicativeCoefficientNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_multiplicative_coefficient_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Multiplicative-Coefficient Net-Zero Delta\n",
           targetReadingSymbolicMultiplicativeCoefficientNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicNegativeLiteralCoefficientNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_negative_literal_coefficient_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Negative Literal-Coefficient Net-Zero Delta\n",
           targetReadingSymbolicNegativeLiteralCoefficientNetZeroDeltaPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return targetReadingSymbolicNetZeroDeltaPassed &&
                   targetReadingSymbolicAdditiveZeroExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicFoldedIntegerSumExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicSignedAdditiveExpressionNetZeroDeltaPassed &&
                    targetReadingSymbolicSameSideTermCancellationExpressionNetZeroDeltaPassed &&
                    targetReadingSymbolicUnaryNegativeExpressionNetZeroDeltaPassed &&
                     targetReadingSymbolicUnaryPositiveExpressionNetZeroDeltaPassed &&
                      targetReadingSymbolicResidualNetNegativeDeltaPassed &&
                      targetReadingSymbolicMultiResidualNetPositiveDeltaPassed &&
                       targetReadingSymbolicMultiResidualPrefixRangePassed &&
                       targetReadingSymbolicLiteralCoefficientNetZeroDeltaPassed &&
                       targetReadingSymbolicFoldedCoefficientNetZeroDeltaPassed &&
                       targetReadingSymbolicMultiplicativeCoefficientNetZeroDeltaPassed &&
                       targetReadingSymbolicNegativeLiteralCoefficientNetZeroDeltaPassed
                   ? 0
                  : 1;
}
