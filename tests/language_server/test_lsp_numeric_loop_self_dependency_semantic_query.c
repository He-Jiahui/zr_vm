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

static TZrBool run_assignment_range_case(SZrState *state,
                                         const TZrChar *label,
                                         const TZrChar *uriText,
                                         const TZrChar *content,
                                         TZrInt64 expectedMin,
                                         TZrInt64 expectedMax) {
    return run_assignment_range_case_at(
            state,
            label,
            uriText,
            content,
            "return narrowed + 0",
            strlen("return narrowed "),
            expectedMin,
            expectedMax);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_positive_expression_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = seed + 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step + 1);\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent positive expression delta assignment dataflow",
            "file:///local_while_self_dependent_positive_expression_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_zero_inclusive_positive_range_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = seed;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent zero-inclusive positive range delta assignment dataflow",
            "file:///local_while_self_dependent_zero_inclusive_positive_range_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_zero_inclusive_negative_range_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = 0 - seed;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent zero-inclusive negative range delta assignment dataflow",
            "file:///local_while_self_dependent_zero_inclusive_negative_range_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            ZR_TYPE_RANGE_INT64_MIN,
            5);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_sign_crossing_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = seed - 128;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent sign-crossing delta assignment dataflow",
            "file:///local_while_self_dependent_sign_crossing_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            ZR_TYPE_RANGE_INT64_MIN,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_same_loop_written_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 2;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent same-loop written delta assignment dataflow",
            "file:///local_while_self_dependent_same_loop_written_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_sequence_with_same_loop_written_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 2;\n"
        "        narrowed = narrowed + 1;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent sequence extends same-loop written delta assignment dataflow",
            "file:///local_while_self_dependent_sequence_same_loop_written_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_multiple_same_loop_written_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 2;\n"
        "        narrowed = narrowed + step;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent multiple same-loop written delta assignment dataflow",
            "file:///local_while_self_dependent_multiple_same_loop_written_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_while_self_dependent_same_loop_written_expression_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 2;\n"
        "        narrowed = narrowed + (step + 1);\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent same-loop written expression delta assignment dataflow",
            "file:///local_while_self_dependent_same_loop_written_expression_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_keeps_other_assignment_with_zero_only_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 0;\n"
        "        other = 10;\n"
        "    }\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent zero-only delta preserves other assignment dataflow",
            "file:///local_while_self_dependent_zero_only_delta_preserves_other_assignment_dataflow_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            0,
            10);
}

static TZrBool test_local_expression_query_widens_zero_only_then_positive_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 0;\n"
        "        narrowed = narrowed + 1;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent zero-only then positive delta assignment dataflow",
            "file:///local_while_self_dependent_zero_only_then_positive_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_keeps_positive_then_negative_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 1;\n"
        "        narrowed = narrowed - 1;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent positive then negative net-zero delta assignment dataflow",
            "file:///local_while_self_dependent_positive_then_negative_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            5);
}

static TZrBool test_local_expression_query_keeps_interleaved_positive_then_negative_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 1;\n"
        "        other = 10;\n"
        "        narrowed = narrowed - 1;\n"
        "    }\n"
        "    return narrowed + 0;\n"
        "}\n";

    return run_assignment_range_case(
            state,
            "while self-dependent interleaved positive then negative net-zero delta assignment dataflow",
            "file:///local_while_self_dependent_interleaved_positive_then_negative_delta_assignment_dataflow_numeric_range_fact.zr",
            content,
            5,
            5);
}

static TZrBool test_local_expression_query_keeps_target_reading_interleaved_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 1;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - 1;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading interleaved net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_interleaved_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading interleaved net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_interleaved_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            0,
            6);
    return narrowedPassed && otherPassed;
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

static TZrBool test_local_expression_query_keeps_target_reading_replay_resolved_net_zero_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 1;\n"
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
            "while self-dependent target-reading replay-resolved net-zero target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_replay_resolved_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading replay-resolved net-zero observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_replay_resolved_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            0,
            6);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_replay_resolved_net_negative_delta(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = 0;\n"
        "    while (flag) {\n"
        "        step = 1;\n"
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
            "while self-dependent target-reading replay-resolved net-negative target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_replay_resolved_net_negative_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading replay-resolved net-negative observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_replay_resolved_net_negative_observer_numeric_range_fact.zr",
            content,
            "return other + 0",
            strlen("return other "),
            ZR_TYPE_RANGE_INT64_MIN,
            6);
    return narrowedPassed && otherPassed;
}

static TZrBool test_local_expression_query_widens_target_reading_interleaved_net_negative_delta(
        SZrState *state) {
    const TZrChar *content =
        "fn main() -> int {\n"
        "    var flag: bool = true;\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + 1;\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed - 2;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading interleaved net-negative target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_interleaved_net_negative_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading interleaved net-negative observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_interleaved_net_negative_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            ZR_TYPE_RANGE_INT64_MIN,
            6);
    return narrowedPassed && otherPassed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool positiveExpressionDeltaPassed;
    TZrBool zeroInclusivePositiveRangeDeltaPassed;
    TZrBool zeroInclusiveNegativeRangeDeltaPassed;
    TZrBool signCrossingDeltaPassed;
    TZrBool sameLoopWrittenDeltaPassed;
    TZrBool sequenceSameLoopWrittenDeltaPassed;
    TZrBool multipleSameLoopWrittenDeltaPassed;
    TZrBool sameLoopWrittenExpressionDeltaPassed;
    TZrBool zeroOnlyDeltaOtherAssignmentPassed;
    TZrBool zeroOnlyThenPositiveDeltaPassed;
    TZrBool positiveThenNegativeDeltaPassed;
    TZrBool interleavedPositiveThenNegativeDeltaPassed;
    TZrBool targetReadingInterleavedDeltaPassed;
    TZrBool targetReadingSymbolicNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed;
    TZrBool targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed;
    TZrBool targetReadingReplayResolvedNetZeroDeltaPassed;
    TZrBool targetReadingReplayResolvedNetNegativeDeltaPassed;
    TZrBool targetReadingInterleavedNetNegativeDeltaPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Loop Self-Dependency Semantic Query Tests\n");
    printf("==========================================================\n");
    positiveExpressionDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_positive_expression_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Positive Expression Delta\n",
           positiveExpressionDeltaPassed ? "PASS" : "FAIL");
    zeroInclusivePositiveRangeDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_zero_inclusive_positive_range_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Zero-Inclusive Positive Range Delta\n",
           zeroInclusivePositiveRangeDeltaPassed ? "PASS" : "FAIL");
    zeroInclusiveNegativeRangeDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_zero_inclusive_negative_range_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Zero-Inclusive Negative Range Delta\n",
           zeroInclusiveNegativeRangeDeltaPassed ? "PASS" : "FAIL");
    signCrossingDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_sign_crossing_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Sign-Crossing Delta\n",
           signCrossingDeltaPassed ? "PASS" : "FAIL");
    sameLoopWrittenDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_same_loop_written_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Same-Loop Written Delta\n",
           sameLoopWrittenDeltaPassed ? "PASS" : "FAIL");
    sequenceSameLoopWrittenDeltaPassed =
        test_local_expression_query_widens_sequence_with_same_loop_written_delta(state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Sequence With Same-Loop Written Delta\n",
           sequenceSameLoopWrittenDeltaPassed ? "PASS" : "FAIL");
    multipleSameLoopWrittenDeltaPassed =
        test_local_expression_query_widens_multiple_same_loop_written_delta(state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Multiple Same-Loop Written Delta\n",
           multipleSameLoopWrittenDeltaPassed ? "PASS" : "FAIL");
    sameLoopWrittenExpressionDeltaPassed =
        test_local_expression_query_widens_while_self_dependent_same_loop_written_expression_delta(state);
    printf("%s: LSP Local Expression Query Widens While Self-Dependent Same-Loop Written Expression Delta\n",
           sameLoopWrittenExpressionDeltaPassed ? "PASS" : "FAIL");
    zeroOnlyDeltaOtherAssignmentPassed =
        test_local_expression_query_keeps_other_assignment_with_zero_only_delta(state);
    printf("%s: LSP Local Expression Query Keeps Other Assignment With Self-Dependent Zero-Only Delta\n",
           zeroOnlyDeltaOtherAssignmentPassed ? "PASS" : "FAIL");
    zeroOnlyThenPositiveDeltaPassed =
        test_local_expression_query_widens_zero_only_then_positive_delta(state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Zero-Only Then Positive Delta\n",
           zeroOnlyThenPositiveDeltaPassed ? "PASS" : "FAIL");
    positiveThenNegativeDeltaPassed =
        test_local_expression_query_keeps_positive_then_negative_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Positive Then Negative Net-Zero Delta\n",
           positiveThenNegativeDeltaPassed ? "PASS" : "FAIL");
    interleavedPositiveThenNegativeDeltaPassed =
        test_local_expression_query_keeps_interleaved_positive_then_negative_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Interleaved Positive Then Negative Net-Zero Delta\n",
           interleavedPositiveThenNegativeDeltaPassed ? "PASS" : "FAIL");
    targetReadingInterleavedDeltaPassed =
        test_local_expression_query_keeps_target_reading_interleaved_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Interleaved Net-Zero Delta\n",
           targetReadingInterleavedDeltaPassed ? "PASS" : "FAIL");
    targetReadingSymbolicNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_symbolic_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Net-Zero Delta\n",
           targetReadingSymbolicNetZeroDeltaPassed ? "PASS" : "FAIL");
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
    targetReadingReplayResolvedNetZeroDeltaPassed =
        test_local_expression_query_keeps_target_reading_replay_resolved_net_zero_delta(state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Replay-Resolved Net-Zero Delta\n",
           targetReadingReplayResolvedNetZeroDeltaPassed ? "PASS" : "FAIL");
    targetReadingReplayResolvedNetNegativeDeltaPassed =
        test_local_expression_query_widens_target_reading_replay_resolved_net_negative_delta(state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Target-Reading Replay-Resolved Net-Negative Delta\n",
           targetReadingReplayResolvedNetNegativeDeltaPassed ? "PASS" : "FAIL");
    targetReadingInterleavedNetNegativeDeltaPassed =
        test_local_expression_query_widens_target_reading_interleaved_net_negative_delta(state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Target-Reading Interleaved Net-Negative Delta\n",
           targetReadingInterleavedNetNegativeDeltaPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return positiveExpressionDeltaPassed &&
                   zeroInclusivePositiveRangeDeltaPassed &&
                   zeroInclusiveNegativeRangeDeltaPassed &&
                   signCrossingDeltaPassed &&
                   sameLoopWrittenDeltaPassed &&
                   sequenceSameLoopWrittenDeltaPassed &&
                   multipleSameLoopWrittenDeltaPassed &&
                   sameLoopWrittenExpressionDeltaPassed &&
                   zeroOnlyDeltaOtherAssignmentPassed &&
                   zeroOnlyThenPositiveDeltaPassed &&
                   positiveThenNegativeDeltaPassed &&
                   interleavedPositiveThenNegativeDeltaPassed &&
                   targetReadingInterleavedDeltaPassed &&
                   targetReadingSymbolicNetZeroDeltaPassed &&
                   targetReadingSymbolicExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicCommutedExpressionNetZeroDeltaPassed &&
                   targetReadingSymbolicAssociativeExpressionNetZeroDeltaPassed &&
                   targetReadingReplayResolvedNetZeroDeltaPassed &&
                   targetReadingReplayResolvedNetNegativeDeltaPassed &&
                   targetReadingInterleavedNetNegativeDeltaPassed
                ? 0
                : 1;
}
