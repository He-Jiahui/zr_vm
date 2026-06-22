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

static TZrBool run_assignment_range_case(SZrState *state,
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

static TZrBool run_assignment_segment_range_case(SZrState *state,
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
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    const SZrNumericRangeSegment *expressionSegment;
    const SZrNumericRangeSegment *numericSegment;
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
        printf("FAIL: unable to prepare %s local segmented query fixture\n", label);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for %s segmented range\n", label);
        return ZR_FALSE;
    }

    expressionSegment = query.expressionFact != ZR_NULL
                            ? ZrParser_InferredType_RangeSegmentAt(&query.expressionFact->inferredType, 0)
                            : ZR_NULL;
    numericSegment = query.numericFact != ZR_NULL
                         ? ZrParser_SemanticNumericFact_RangeSegmentAt(query.numericFact, 0)
                         : ZR_NULL;

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.expressionFact->inferredType.hasRangeConstraint &&
             query.expressionFact->inferredType.minValue == expectedMin &&
             query.expressionFact->inferredType.maxValue == expectedMax &&
             query.expressionFact->inferredType.rangeSegmentCount == 2 &&
             query.numericFact != ZR_NULL &&
             query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION &&
             query.numericFact->targetType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact->hasRange &&
             query.numericFact->minValue == expectedMin &&
             query.numericFact->maxValue == expectedMax &&
             query.numericFact->rangeSegmentCount == 2 &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == (TZrUInt64)expectedMin &&
             query.numericFact->maxUnsignedValue == (TZrUInt64)expectedMax &&
             !query.numericFact->mayOverflow &&
             expressionSegment != ZR_NULL &&
             numericSegment != ZR_NULL &&
             expressionSegment->minValue == firstMin &&
             expressionSegment->maxValue == firstMax &&
             numericSegment->minValue == firstMin &&
             numericSegment->maxValue == firstMax;

    if (passed) {
        expressionSegment = ZrParser_InferredType_RangeSegmentAt(&query.expressionFact->inferredType, 1);
        numericSegment = ZrParser_SemanticNumericFact_RangeSegmentAt(query.numericFact, 1);
        passed = expressionSegment != ZR_NULL &&
                 numericSegment != ZR_NULL &&
                 expressionSegment->minValue == secondMin &&
                 expressionSegment->maxValue == secondMax &&
                 numericSegment->minValue == secondMin &&
                 numericSegment->maxValue == secondMax;
    }

    if (!passed) {
        printf("FAIL: expected %s segmented assignment range fact; status=%d expr=%p exprKind=%d exprType=%d "
               "exprHasRange=%d exprMin=%lld exprMax=%lld exprSegments=%llu "
               "numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld segments=%llu "
               "hasUnsigned=%d umin=%llu umax=%llu mayOverflow=%d\n",
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
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->minUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->maxUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_expression_query_joins_if_else_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        narrowed = 1;\n"
        "    } else {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else assignment dataflow",
                                     "file:///local_if_else_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     2,
                                     11);
}

static TZrBool test_local_expression_query_preserves_if_else_assignment_segments(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        narrowed = 1;\n"
        "    } else {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_segment_range_case(
        state,
        "if/else assignment dataflow segments",
        "file:///local_if_else_assignment_dataflow_segment_numeric_range_fact.zr",
        content,
        0,
        2,
        11,
        2,
        2,
        11,
        11);
}

static TZrBool test_local_expression_query_joins_if_else_multi_statement_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        flag;\n"
        "        narrowed = 1;\n"
        "    } else {\n"
        "        flag;\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else multi-statement assignment dataflow",
                                     "file:///local_if_else_multi_statement_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     2,
                                     11);
}

static TZrBool test_local_expression_query_joins_if_else_nonterminal_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        narrowed = 1;\n"
        "        flag;\n"
        "    } else {\n"
        "        narrowed = 10;\n"
        "        flag;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else nonterminal assignment dataflow",
                                     "file:///local_if_else_nonterminal_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     2,
                                     11);
}

static TZrBool test_local_expression_query_joins_if_else_rhs_dependent_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        narrowed = 1;\n"
        "        narrowed = narrowed + 1;\n"
        "    } else {\n"
        "        narrowed = 10;\n"
        "        narrowed = narrowed + 1;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else rhs-dependent assignment dataflow",
                                     "file:///local_if_else_rhs_dependent_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     2,
                                     3,
                                     12);
}

static TZrBool test_local_expression_query_joins_if_else_multi_target_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var low: int = 0;\n"
        "    var high: int = 0;\n"
        "    if (flag) {\n"
        "        low = 1;\n"
        "        high = low + 1;\n"
        "    } else {\n"
        "        low = 10;\n"
        "        high = low + 10;\n"
        "    }\n"
        "    return high + low;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else multi-target assignment dataflow",
                                     "file:///local_if_else_multi_target_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     2,
                                     3,
                                     30);
}

static TZrBool test_local_expression_query_joins_nested_if_else_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, inner: bool): int {\n"
        "    var narrowed: int = 0;\n"
        "    if (flag) {\n"
        "        if (inner) {\n"
        "            narrowed = 1;\n"
        "        } else {\n"
        "            narrowed = 2;\n"
        "        }\n"
        "    } else {\n"
        "        if (inner) {\n"
        "            narrowed = 10;\n"
        "        } else {\n"
        "            narrowed = 20;\n"
        "        }\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "nested if/else assignment dataflow",
                                     "file:///local_nested_if_else_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     2,
                                     21);
}

static TZrBool test_local_expression_query_joins_while_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    while (flag) {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "while assignment dataflow",
                                     "file:///local_while_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     6,
                                     11);
}

static TZrBool test_local_expression_query_joins_while_multi_statement_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    while (flag) {\n"
        "        flag;\n"
        "        narrowed = 10;\n"
        "        flag;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "while multi-statement assignment dataflow",
                                     "file:///local_while_multi_statement_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     6,
                                     11);
}

static TZrBool test_local_expression_query_joins_while_multi_target_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var low: int = 5;\n"
        "    var high: int = 20;\n"
        "    while (flag) {\n"
        "        low = 1;\n"
        "        high = low + 10;\n"
        "    }\n"
        "    return high + low;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "while multi-target assignment dataflow",
                                     "file:///local_while_multi_target_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     1,
                                     12,
                                     25);
}

static TZrBool test_local_expression_query_joins_if_then_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    if (flag) {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/then assignment dataflow",
                                     "file:///local_if_then_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     6,
                                     11);
}

static TZrBool test_local_expression_query_joins_if_else_only_assignment_range(SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    if (flag) {\n"
        "        flag;\n"
        "    } else {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_assignment_range_case(state,
                                     "if/else-only assignment dataflow",
                                     "file:///local_if_else_only_assignment_dataflow_numeric_range_fact.zr",
                                     content,
                                     0,
                                     6,
                                     11);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool ifElseAssignmentDataflowTestPassed;
    TZrBool ifElseAssignmentSegmentDataflowTestPassed;
    TZrBool ifElseMultiStatementAssignmentDataflowTestPassed;
    TZrBool ifElseNonterminalAssignmentDataflowTestPassed;
    TZrBool ifElseRhsDependentAssignmentDataflowTestPassed;
    TZrBool ifElseMultiTargetAssignmentDataflowTestPassed;
    TZrBool nestedIfElseAssignmentDataflowTestPassed;
    TZrBool whileAssignmentDataflowTestPassed;
    TZrBool whileMultiStatementAssignmentDataflowTestPassed;
    TZrBool whileMultiTargetAssignmentDataflowTestPassed;
    TZrBool ifThenAssignmentDataflowTestPassed;
    TZrBool ifElseOnlyAssignmentDataflowTestPassed;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Branch Assignment Semantic Query Tests\n");
    printf("========================================================\n");
    ifElseAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else Assignment Range\n",
           ifElseAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseAssignmentSegmentDataflowTestPassed =
        test_local_expression_query_preserves_if_else_assignment_segments(state);
    printf("%s: LSP Local Expression Query Preserves If/Else Assignment Segment Range\n",
           ifElseAssignmentSegmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseMultiStatementAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_multi_statement_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else Multi-Statement Assignment Range\n",
           ifElseMultiStatementAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseNonterminalAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_nonterminal_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else Nonterminal Assignment Range\n",
           ifElseNonterminalAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseRhsDependentAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_rhs_dependent_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else RHS-Dependent Assignment Range\n",
           ifElseRhsDependentAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseMultiTargetAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_multi_target_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else Multi-Target Assignment Range\n",
           ifElseMultiTargetAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    nestedIfElseAssignmentDataflowTestPassed =
        test_local_expression_query_joins_nested_if_else_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins Nested If/Else Assignment Range\n",
           nestedIfElseAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    whileAssignmentDataflowTestPassed =
        test_local_expression_query_joins_while_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins While Assignment Range\n",
           whileAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    whileMultiStatementAssignmentDataflowTestPassed =
        test_local_expression_query_joins_while_multi_statement_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins While Multi-Statement Assignment Range\n",
           whileMultiStatementAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    whileMultiTargetAssignmentDataflowTestPassed =
        test_local_expression_query_joins_while_multi_target_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins While Multi-Target Assignment Range\n",
           whileMultiTargetAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifThenAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_then_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Then Assignment Range\n",
           ifThenAssignmentDataflowTestPassed ? "PASS" : "FAIL");
    ifElseOnlyAssignmentDataflowTestPassed =
        test_local_expression_query_joins_if_else_only_assignment_range(state);
    printf("%s: LSP Local Expression Query Joins If/Else-Only Assignment Range\n",
           ifElseOnlyAssignmentDataflowTestPassed ? "PASS" : "FAIL");

    passed = ifElseAssignmentDataflowTestPassed &&
             ifElseAssignmentSegmentDataflowTestPassed &&
             ifElseMultiStatementAssignmentDataflowTestPassed &&
             ifElseNonterminalAssignmentDataflowTestPassed &&
             ifElseRhsDependentAssignmentDataflowTestPassed &&
              ifElseMultiTargetAssignmentDataflowTestPassed &&
              nestedIfElseAssignmentDataflowTestPassed &&
              whileAssignmentDataflowTestPassed &&
              whileMultiStatementAssignmentDataflowTestPassed &&
              whileMultiTargetAssignmentDataflowTestPassed &&
              ifThenAssignmentDataflowTestPassed &&
             ifElseOnlyAssignmentDataflowTestPassed;

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
