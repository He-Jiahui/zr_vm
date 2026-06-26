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

static const TZrChar *associative_commutative_product_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    var scale: int = (seed % 3) - 1;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * scale));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((scale * step) * factor);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *folded_constant_product_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 3) - 1;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * (2 * 3)));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((6 * step) * factor);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *unary_negative_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + ((-step) * factor);\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((-1 * factor) * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *unary_positive_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + ((+step) * factor);\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - (factor * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *double_negative_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + ((-(-step)) * factor);\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - (factor * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *divided_constant_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * (6 / 2)));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((3 * factor) * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *modulo_constant_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * (7 % 4)));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((3 * factor) * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *additive_constant_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * (1 + 2)));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((3 * factor) * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static const TZrChar *subtractive_constant_product_factor_content(void) {
    return "func calc(flag: bool, seed: u8): int {\n"
           "    var narrowed: int = 5;\n"
           "    var other: int = 0;\n"
           "    var step: int = (seed % 3) - 1;\n"
           "    var factor: int = (seed % 5) - 2;\n"
           "    while (flag) {\n"
           "        narrowed = narrowed + (step * (factor * (5 - 2)));\n"
           "        other = narrowed;\n"
           "        narrowed = narrowed - ((3 * factor) * step);\n"
           "    }\n"
           "    narrowed + 0;\n"
           "    return other + 0;\n"
           "}\n";
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_associative_commutative_product_exact_cancel(
        SZrState *state) {
    const TZrChar *content = associative_commutative_product_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic associative-commutative product exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_associative_commutative_product_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic associative-commutative product exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_associative_commutative_product_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            7);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_folded_constant_product_exact_cancel(
        SZrState *state) {
    const TZrChar *content = folded_constant_product_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded constant-product exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_constant_product_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic folded constant-product exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_folded_constant_product_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            11);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_unary_negative_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = unary_negative_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-negative product factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_negative_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-negative product factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_negative_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            7);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_unary_positive_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = unary_positive_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-positive product factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_positive_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic unary-positive product factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_unary_positive_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            7);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_double_negative_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = double_negative_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic double-negative product factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_double_negative_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic double-negative product factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_double_negative_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            0,
            7);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_divided_constant_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = divided_constant_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic divided constant product-factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_divided_constant_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic divided constant product-factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_divided_constant_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            11);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_modulo_constant_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = modulo_constant_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic modulo constant product-factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_modulo_constant_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic modulo constant product-factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_modulo_constant_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            11);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_additive_constant_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = additive_constant_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic additive constant product-factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_additive_constant_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic additive constant product-factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_additive_constant_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            11);
    return narrowedPassed && otherPassed;
}

static TZrBool
test_local_expression_query_keeps_target_reading_symbolic_subtractive_constant_product_factor_exact_cancel(
        SZrState *state) {
    const TZrChar *content = subtractive_constant_product_factor_content();
    TZrBool narrowedPassed;
    TZrBool otherPassed;

    narrowedPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic subtractive constant product-factor exact-cancel target assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_subtractive_constant_product_factor_exact_cancel_target_numeric_range_fact.zr",
            content,
            "narrowed + 0",
            strlen("narrowed "),
            5,
            5);
    otherPassed = run_assignment_range_case_at(
            state,
            "while self-dependent target-reading symbolic subtractive constant product-factor exact-cancel observer assignment dataflow",
            "file:///local_while_self_dependent_target_reading_symbolic_subtractive_constant_product_factor_exact_cancel_observer_numeric_range_fact.zr",
            content,
            "other + 0",
            strlen("other "),
            -1,
            11);
    return narrowedPassed && otherPassed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool targetReadingSymbolicAssociativeCommutativeProductExactCancelPassed;
    TZrBool targetReadingSymbolicFoldedConstantProductExactCancelPassed;
    TZrBool targetReadingSymbolicUnaryNegativeProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicUnaryPositiveProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicDoubleNegativeProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicDividedConstantProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicModuloConstantProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicAdditiveConstantProductFactorExactCancelPassed;
    TZrBool targetReadingSymbolicSubtractiveConstantProductFactorExactCancelPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Loop Self-Dependency Symbolic Product Semantic Query Tests\n");
    printf("=============================================================================\n");
    targetReadingSymbolicAssociativeCommutativeProductExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_associative_commutative_product_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Associative-Commutative Product Exact Cancel\n",
           targetReadingSymbolicAssociativeCommutativeProductExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicFoldedConstantProductExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_folded_constant_product_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Folded Constant Product Exact Cancel\n",
           targetReadingSymbolicFoldedConstantProductExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicUnaryNegativeProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_unary_negative_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Unary Negative Product Factor Exact Cancel\n",
           targetReadingSymbolicUnaryNegativeProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicUnaryPositiveProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_unary_positive_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Unary Positive Product Factor Exact Cancel\n",
           targetReadingSymbolicUnaryPositiveProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicDoubleNegativeProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_double_negative_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Double Negative Product Factor Exact Cancel\n",
           targetReadingSymbolicDoubleNegativeProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicDividedConstantProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_divided_constant_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Divided Constant Product Factor Exact Cancel\n",
           targetReadingSymbolicDividedConstantProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicModuloConstantProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_modulo_constant_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Modulo Constant Product Factor Exact Cancel\n",
           targetReadingSymbolicModuloConstantProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicAdditiveConstantProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_additive_constant_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Additive Constant Product Factor Exact Cancel\n",
           targetReadingSymbolicAdditiveConstantProductFactorExactCancelPassed ? "PASS" : "FAIL");
    targetReadingSymbolicSubtractiveConstantProductFactorExactCancelPassed =
        test_local_expression_query_keeps_target_reading_symbolic_subtractive_constant_product_factor_exact_cancel(
                state);
    printf("%s: LSP Local Expression Query Keeps Self-Dependent Target-Reading Symbolic Subtractive Constant Product Factor Exact Cancel\n",
           targetReadingSymbolicSubtractiveConstantProductFactorExactCancelPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return targetReadingSymbolicAssociativeCommutativeProductExactCancelPassed &&
                   targetReadingSymbolicFoldedConstantProductExactCancelPassed &&
                   targetReadingSymbolicUnaryNegativeProductFactorExactCancelPassed &&
                   targetReadingSymbolicUnaryPositiveProductFactorExactCancelPassed &&
                   targetReadingSymbolicDoubleNegativeProductFactorExactCancelPassed &&
                   targetReadingSymbolicDividedConstantProductFactorExactCancelPassed &&
                   targetReadingSymbolicModuloConstantProductFactorExactCancelPassed &&
                   targetReadingSymbolicAdditiveConstantProductFactorExactCancelPassed &&
                   targetReadingSymbolicSubtractiveConstantProductFactorExactCancelPassed
           ? 0
           : 1;
}
