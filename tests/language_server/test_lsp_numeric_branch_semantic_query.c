#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
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

static TZrBool find_position_for_substring(const TZrChar *content,
                                           const TZrChar *needle,
                                           TZrSize occurrence,
                                           SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
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

    outPosition->line = line;
    outPosition->character = character;
    return ZR_TRUE;
}

static TZrBool run_branch_range_case(SZrState *state,
                                     const TZrChar *label,
                                     const TZrChar *uriText,
                                     const TZrChar *content,
                                     TZrSize plusOccurrence,
                                     TZrInt64 expectedMin,
                                     TZrInt64 expectedMax) {
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", plusOccurrence, &position)) {
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

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact != ZR_NULL &&
             query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION &&
             query.numericFact->targetType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact->hasRange &&
             query.numericFact->minValue == expectedMin &&
             query.numericFact->maxValue == expectedMax &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == (TZrUInt64)expectedMin &&
             query.numericFact->maxUnsignedValue == (TZrUInt64)expectedMax &&
             !query.numericFact->mayOverflow;

    if (!passed) {
        printf("FAIL: expected %s interval range fact; status=%d expr=%p exprKind=%d exprType=%d numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld hasUnsigned=%d umin=%llu umax=%llu mayOverflow=%d\n",
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

static TZrBool run_branch_segment_set_case(SZrState *state,
                                           const TZrChar *label,
                                           const TZrChar *uriText,
                                           const TZrChar *content,
                                           TZrSize plusOccurrence,
                                           TZrInt64 expectedMin,
                                           TZrInt64 expectedMax,
                                           const SZrNumericRangeSegment *expectedSegments,
                                           TZrSize expectedSegmentCount) {
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;
    TZrSize index;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", plusOccurrence, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare %s local segment query fixture\n", label);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for %s segment range\n", label);
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.expressionFact->inferredType.hasRangeConstraint &&
             query.expressionFact->inferredType.minValue == expectedMin &&
             query.expressionFact->inferredType.maxValue == expectedMax &&
             query.expressionFact->inferredType.rangeSegmentCount == expectedSegmentCount &&
             query.numericFact != ZR_NULL &&
             query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION &&
             query.numericFact->targetType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact->hasRange &&
             query.numericFact->minValue == expectedMin &&
             query.numericFact->maxValue == expectedMax &&
             query.numericFact->rangeSegmentCount == expectedSegmentCount &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == (TZrUInt64)expectedMin &&
             query.numericFact->maxUnsignedValue == (TZrUInt64)expectedMax &&
             !query.numericFact->mayOverflow;

    if (passed) {
        for (index = 0; index < expectedSegmentCount; index++) {
            const SZrNumericRangeSegment *expressionSegment =
                ZrParser_InferredType_RangeSegmentAt(&query.expressionFact->inferredType, index);
            const SZrNumericRangeSegment *numericSegment =
                ZrParser_SemanticNumericFact_RangeSegmentAt(query.numericFact, index);
            if (expressionSegment == ZR_NULL ||
                numericSegment == ZR_NULL ||
                expressionSegment->minValue != expectedSegments[index].minValue ||
                expressionSegment->maxValue != expectedSegments[index].maxValue ||
                numericSegment->minValue != expectedSegments[index].minValue ||
                numericSegment->maxValue != expectedSegments[index].maxValue) {
                passed = ZR_FALSE;
                break;
            }
        }
    }

    if (!passed) {
        printf("FAIL: expected %s segmented range fact; status=%d expr=%p exprKind=%d exprType=%d "
               "exprHasRange=%d exprMin=%lld exprMax=%lld exprSegments=%llu "
               "numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld segments=%llu "
               "expectedSegments=%llu hasUnsigned=%d umin=%llu umax=%llu mayOverflow=%d\n",
               label,
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.hasRangeConstraint : -1,
               query.expressionFact != ZR_NULL ? (long long)query.expressionFact->inferredType.minValue : 0LL,
               query.expressionFact != ZR_NULL ? (long long)query.expressionFact->inferredType.maxValue : 0LL,
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->inferredType.rangeSegmentCount
                   : 0ULL,
               (void *)query.numericFact,
               query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->targetType : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : 0LL,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : 0LL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->rangeSegmentCount : 0ULL,
               (unsigned long long)expectedSegmentCount,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->minUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->maxUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool run_branch_segment_range_case(SZrState *state,
                                             const TZrChar *label,
                                             const TZrChar *uriText,
                                             const TZrChar *content,
                                             TZrSize plusOccurrence,
                                             TZrInt64 expectedMin,
                                             TZrInt64 expectedMax,
                                             TZrInt64 firstMin,
                                             TZrInt64 firstMax,
                                             TZrInt64 secondMin,
                                             TZrInt64 secondMax) {
    const SZrNumericRangeSegment expectedSegments[] = {
        {firstMin, firstMax},
        {secondMin, secondMax},
    };

    return run_branch_segment_set_case(state,
                                       label,
                                       uriText,
                                       content,
                                       plusOccurrence,
                                       expectedMin,
                                       expectedMax,
                                       expectedSegments,
                                       2);
}

static TZrBool test_local_expression_query_refines_true_branch_less_than_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch less-than",
                                 "file:///local_true_branch_less_than_numeric_range_fact.zr",
                                 content,
                                 0,
                                 1,
                                 10);
}

static TZrBool test_local_expression_query_refines_true_branch_equal_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed == 10) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch equal",
                                 "file:///local_true_branch_equal_numeric_range_fact.zr",
                                 content,
                                 0,
                                 11,
                                 11);
}

static TZrBool test_local_expression_query_refines_true_branch_edge_not_equal_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed != 0) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch edge-not-equal",
                                 "file:///local_true_branch_edge_not_equal_numeric_range_fact.zr",
                                 content,
                                 0,
                                 2,
                                 256);
}

static TZrBool test_local_expression_query_refines_true_branch_logical_and_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed > 2 && seed < 10) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch logical-and",
                                 "file:///local_true_branch_logical_and_numeric_range_fact.zr",
                                 content,
                                 0,
                                 4,
                                 10);
}

static TZrBool test_local_expression_query_refines_true_branch_logical_or_same_direction_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10 || seed < 20) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch logical-or same-direction",
                                 "file:///local_true_branch_logical_or_same_direction_numeric_range_fact.zr",
                                 content,
                                 0,
                                 1,
                                 20);
}

static TZrBool test_local_expression_query_refines_true_branch_logical_or_disjoint_segment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10 || seed > 20) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_segment_range_case(
        state,
        "true-branch logical-or disjoint",
        "file:///local_true_branch_logical_or_disjoint_segment_numeric_range_fact.zr",
        content,
        0,
        1,
        256,
        1,
        10,
        22,
        256);
}

static TZrBool test_local_expression_query_refines_true_branch_logical_or_nested_and_segment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if ((seed > 2 && seed < 10) || seed == 20) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 30;\n"
        "}\n";

    return run_branch_segment_range_case(
        state,
        "true-branch logical-or nested-and",
        "file:///local_true_branch_logical_or_nested_and_segment_numeric_range_fact.zr",
        content,
        0,
        4,
        21,
        4,
        10,
        21,
        21);
}

static TZrBool test_local_expression_query_refines_true_branch_logical_or_three_segment_range(SZrState *state) {
    const SZrNumericRangeSegment expectedSegments[] = {
        {1, 5},
        {11, 11},
        {22, 256},
    };
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 5 || seed == 10 || seed > 20) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 30;\n"
        "}\n";

    return run_branch_segment_set_case(
        state,
        "true-branch logical-or three-segment",
        "file:///local_true_branch_logical_or_three_segment_numeric_range_fact.zr",
        content,
        0,
        1,
        256,
        expectedSegments,
        3);
}

static TZrBool test_local_expression_query_refines_true_branch_unary_not_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (!(seed < 10)) {\n"
        "        return seed + 1;\n"
        "    }\n"
        "    return seed + 20;\n"
        "}\n";

    return run_branch_range_case(state,
                                 "true-branch unary-not",
                                 "file:///local_true_branch_unary_not_numeric_range_fact.zr",
                                 content,
                                 0,
                                 11,
                                 256);
}

static TZrBool test_local_expression_query_refines_else_if_inner_true_branch_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10) {\n"
        "        return seed + 100;\n"
        "    } else if (seed < 20) {\n"
        "        return seed + 1;\n"
        "    } else {\n"
        "        return seed + 200;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "else-if inner true-branch",
                                 "file:///local_else_if_inner_true_branch_numeric_range_fact.zr",
                                 content,
                                 1,
                                 11,
                                 20);
}

static TZrBool test_local_expression_query_refines_false_branch_less_than_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10) {\n"
        "        return seed + 1;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "false-branch less-than",
                                 "file:///local_false_branch_less_than_numeric_range_fact.zr",
                                 content,
                                 1,
                                 11,
                                 256);
}

static TZrBool test_local_expression_query_refines_false_branch_edge_equal_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed == 0) {\n"
        "        return seed + 100;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "false-branch edge-equal",
                                 "file:///local_false_branch_edge_equal_numeric_range_fact.zr",
                                 content,
                                 1,
                                 2,
                                 256);
}

static TZrBool test_local_expression_query_refines_false_branch_not_equal_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed != 10) {\n"
        "        return seed + 100;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "false-branch not-equal",
                                 "file:///local_false_branch_not_equal_numeric_range_fact.zr",
                                 content,
                                 1,
                                 11,
                                 11);
}

static TZrBool test_local_expression_query_refines_false_branch_logical_or_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed <= 2 || seed >= 10) {\n"
        "        return seed + 100;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "false-branch logical-or",
                                 "file:///local_false_branch_logical_or_numeric_range_fact.zr",
                                 content,
                                 1,
                                 4,
                                 10);
}

static TZrBool test_local_expression_query_refines_false_branch_logical_and_same_direction_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed < 10 && seed < 20) {\n"
        "        return seed + 100;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_range_case(state,
                                 "false-branch logical-and same-direction",
                                 "file:///local_false_branch_logical_and_same_direction_numeric_range_fact.zr",
                                 content,
                                 1,
                                 11,
                                 256);
}

static TZrBool test_local_expression_query_refines_false_branch_logical_and_disjoint_segment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(seed: u8): uint {\n"
        "    if (seed > 10 && seed < 20) {\n"
        "        return seed + 100;\n"
        "    } else {\n"
        "        return seed + 1;\n"
        "    }\n"
        "}\n";

    return run_branch_segment_range_case(
        state,
        "false-branch logical-and disjoint",
        "file:///local_false_branch_logical_and_disjoint_segment_numeric_range_fact.zr",
        content,
        1,
        1,
        256,
        1,
        11,
        21,
        256);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool branchRefinementTestPassed;
    TZrBool equalBranchRefinementTestPassed;
    TZrBool edgeNotEqualBranchRefinementTestPassed;
    TZrBool logicalAndBranchRefinementTestPassed;
    TZrBool logicalOrSameDirectionBranchRefinementTestPassed;
    TZrBool logicalOrDisjointSegmentBranchRefinementTestPassed;
    TZrBool logicalOrNestedAndSegmentBranchRefinementTestPassed;
    TZrBool logicalOrThreeSegmentBranchRefinementTestPassed;
    TZrBool unaryNotBranchRefinementTestPassed;
    TZrBool elseIfInnerTrueBranchRefinementTestPassed;
    TZrBool falseBranchRefinementTestPassed;
    TZrBool falseBranchEdgeEqualRefinementTestPassed;
    TZrBool falseBranchNotEqualRefinementTestPassed;
    TZrBool falseBranchLogicalOrRefinementTestPassed;
    TZrBool falseBranchLogicalAndSameDirectionRefinementTestPassed;
    TZrBool falseBranchLogicalAndDisjointSegmentRefinementTestPassed;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Branch Semantic Query Tests\n");
    printf("=============================================\n");
    branchRefinementTestPassed = test_local_expression_query_refines_true_branch_less_than_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Less-Than Range\n",
           branchRefinementTestPassed ? "PASS" : "FAIL");
    equalBranchRefinementTestPassed = test_local_expression_query_refines_true_branch_equal_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Equal Range\n",
           equalBranchRefinementTestPassed ? "PASS" : "FAIL");
    edgeNotEqualBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_edge_not_equal_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Edge-Not-Equal Range\n",
           edgeNotEqualBranchRefinementTestPassed ? "PASS" : "FAIL");
    logicalAndBranchRefinementTestPassed = test_local_expression_query_refines_true_branch_logical_and_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Logical-And Range\n",
           logicalAndBranchRefinementTestPassed ? "PASS" : "FAIL");
    logicalOrSameDirectionBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_logical_or_same_direction_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Logical-Or Same-Direction Range\n",
           logicalOrSameDirectionBranchRefinementTestPassed ? "PASS" : "FAIL");
    logicalOrDisjointSegmentBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_logical_or_disjoint_segment_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Logical-Or Disjoint Segment Range\n",
           logicalOrDisjointSegmentBranchRefinementTestPassed ? "PASS" : "FAIL");
    logicalOrNestedAndSegmentBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_logical_or_nested_and_segment_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Logical-Or Nested-And Segment Range\n",
           logicalOrNestedAndSegmentBranchRefinementTestPassed ? "PASS" : "FAIL");
    logicalOrThreeSegmentBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_logical_or_three_segment_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Logical-Or Three-Segment Range\n",
           logicalOrThreeSegmentBranchRefinementTestPassed ? "PASS" : "FAIL");
    unaryNotBranchRefinementTestPassed =
        test_local_expression_query_refines_true_branch_unary_not_range(state);
    printf("%s: LSP Local Expression Query Refines True-Branch Unary-Not Range\n",
           unaryNotBranchRefinementTestPassed ? "PASS" : "FAIL");
    elseIfInnerTrueBranchRefinementTestPassed =
        test_local_expression_query_refines_else_if_inner_true_branch_range(state);
    printf("%s: LSP Local Expression Query Refines Else-If Inner True-Branch Range\n",
           elseIfInnerTrueBranchRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchRefinementTestPassed = test_local_expression_query_refines_false_branch_less_than_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Less-Than Range\n",
           falseBranchRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchEdgeEqualRefinementTestPassed =
        test_local_expression_query_refines_false_branch_edge_equal_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Edge-Equal Range\n",
           falseBranchEdgeEqualRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchNotEqualRefinementTestPassed =
        test_local_expression_query_refines_false_branch_not_equal_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Not-Equal Range\n",
           falseBranchNotEqualRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchLogicalOrRefinementTestPassed = test_local_expression_query_refines_false_branch_logical_or_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Logical-Or Range\n",
           falseBranchLogicalOrRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchLogicalAndSameDirectionRefinementTestPassed =
        test_local_expression_query_refines_false_branch_logical_and_same_direction_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Logical-And Same-Direction Range\n",
           falseBranchLogicalAndSameDirectionRefinementTestPassed ? "PASS" : "FAIL");
    falseBranchLogicalAndDisjointSegmentRefinementTestPassed =
        test_local_expression_query_refines_false_branch_logical_and_disjoint_segment_range(state);
    printf("%s: LSP Local Expression Query Refines False-Branch Logical-And Disjoint Segment Range\n",
           falseBranchLogicalAndDisjointSegmentRefinementTestPassed ? "PASS" : "FAIL");
    passed = branchRefinementTestPassed &&
            equalBranchRefinementTestPassed &&
            edgeNotEqualBranchRefinementTestPassed &&
            logicalAndBranchRefinementTestPassed &&
            logicalOrSameDirectionBranchRefinementTestPassed &&
            logicalOrDisjointSegmentBranchRefinementTestPassed &&
            logicalOrNestedAndSegmentBranchRefinementTestPassed &&
            logicalOrThreeSegmentBranchRefinementTestPassed &&
            unaryNotBranchRefinementTestPassed &&
            elseIfInnerTrueBranchRefinementTestPassed &&
            falseBranchRefinementTestPassed &&
            falseBranchEdgeEqualRefinementTestPassed &&
            falseBranchNotEqualRefinementTestPassed &&
            falseBranchLogicalOrRefinementTestPassed &&
            falseBranchLogicalAndSameDirectionRefinementTestPassed &&
            falseBranchLogicalAndDisjointSegmentRefinementTestPassed;

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
