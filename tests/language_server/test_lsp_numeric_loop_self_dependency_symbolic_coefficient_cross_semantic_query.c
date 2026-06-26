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
        !find_position_for_substring_offset(content, needle, offset, &position)) {
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

static TZrBool test_local_expression_query_widens_target_reading_symbolic_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 5) - 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * factor);\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_nested_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 5) - 1;\n"
        "    var scale: int = 2;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic nested sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_nested_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_non_singleton_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 5) - 1;\n"
        "    var scale: int = (seed % 2) + 2;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic non-singleton scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_non_singleton_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_negative_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 5) - 1;\n"
        "    var scale: int = -1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic negative scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_negative_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_negative_non_singleton_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 2) - 2;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + (step + step);\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic negative non-singleton scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_negative_non_singleton_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_sign_crossing_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 3) - 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic sign-crossing scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_sign_crossing_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_zero_inclusive_positive_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = seed % 2;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic zero-inclusive positive scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_zero_inclusive_positive_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static TZrBool test_local_expression_query_widens_target_reading_symbolic_zero_inclusive_negative_scale_sign_crossing_coefficient_residual(
        SZrState *state) {
    const TZrChar *content =
        "func calc(flag: bool, seed: u8): int {\n"
        "    var narrowed: int = 5;\n"
        "    var other: int = 0;\n"
        "    var step: int = (seed % 3) + 1;\n"
        "    var factor: int = (seed % 3) - 1;\n"
        "    var scale: int = (seed % 2) - 1;\n"
        "    while (flag) {\n"
        "        narrowed = narrowed + (step * (factor * scale));\n"
        "        other = narrowed;\n"
        "        narrowed = narrowed + step;\n"
        "    }\n"
        "    narrowed + 0;\n"
        "    return other + 0;\n"
        "}\n";

    return run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic zero-inclusive negative scale sign-crossing coefficient residual target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_zero_inclusive_negative_scale_sign_crossing_coefficient_residual_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    passed =
        test_local_expression_query_widens_target_reading_symbolic_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_nested_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_non_singleton_scale_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_negative_scale_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_negative_non_singleton_scale_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_sign_crossing_scale_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_zero_inclusive_positive_scale_sign_crossing_coefficient_residual(
                state) &&
        test_local_expression_query_widens_target_reading_symbolic_zero_inclusive_negative_scale_sign_crossing_coefficient_residual(
                state);
    printf("%s: LSP Local Expression Query Widens Self-Dependent Target-Reading Symbolic Sign-Crossing Coefficient Residual\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
