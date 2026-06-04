//
// Focused LSP local semantic query regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_local_semantic_query.h"
#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static int g_failures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timerValue, summary) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
    g_failures++; \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

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

static TZrBool lsp_find_position_for_substring(const TZrChar *content,
                                               const TZrChar *needle,
                                               TZrSize occurrence,
                                               TZrInt32 extraCharacterOffset,
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
    outPosition->character = character + extraCharacterOffset;
    return ZR_TRUE;
}

static const TZrChar *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_diagnostic_array_contains_code(SZrArray *diagnostics,
                                                  const TZrChar *expectedCode) {
    if (diagnostics == ZR_NULL || expectedCode == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
            (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            (*diagnosticPtr)->code != ZR_NULL &&
            strcmp(test_string_ptr((*diagnosticPtr)->code), expectedCode) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_local_expression_query_returns_numeric_range_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Numeric Range Fact";
    const TZrChar *uriText = "file:///local_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    return 1 + 2;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local numeric range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minValue != 3 ||
        query.numericFact->maxValue != 3 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected exact range; status=%d query=(%lld,%d,%d) expr=%p nodeType=%d exprRange=(%lld,%d,%d)-(%lld,%d,%d) numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL && query.expressionFact->node != ZR_NULL
                     ? (int)query.expressionFact->node->type
                     : -1,
                 query.expressionFact != ZR_NULL ? (long long)query.expressionFact->range.start.offset : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.start.line : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.start.column : -1,
                 query.expressionFact != ZR_NULL ? (long long)query.expressionFact->range.end.offset : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.end.line : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.end.column : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_propagates_variable_numeric_range_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Propagates Variable Numeric Range Fact";
    const TZrChar *uriText = "file:///local_variable_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    var seed = 2;\n"
        "    return seed + 3;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local variable numeric range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minValue != 5 ||
        query.numericFact->maxValue != 5 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected variable exact range; status=%d query=(%lld,%d,%d) expr=%p exprType=%d numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_propagates_expression_statement_numeric_range_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Propagates Expression Statement Numeric Range Fact";
    const TZrChar *uriText = "file:///local_expression_statement_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(): void {\n"
        "    var seed = 2;\n"
        "    seed + 3;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local expression statement numeric range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minValue != 5 ||
        query.numericFact->maxValue != 5 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected expression statement exact range; status=%d query=(%lld,%d,%d) expr=%p exprType=%d numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_propagates_integer_interval_range_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Propagates Integer Interval Range Fact";
    const TZrChar *uriText = "file:///local_integer_interval_range_fact.zr";
    const TZrChar *content =
        "func calc(seed: i8): int {\n"
        "    return seed + 3;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local integer interval range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minValue != -125 ||
        query.numericFact->maxValue != 130 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected interval range; status=%d query=(%lld,%d,%d) expr=%p exprType=%d numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_keeps_unsigned_numeric_range_payload(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Keeps Unsigned Numeric Range Payload";
    const TZrChar *uriText = "file:///local_unsigned_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(seed: u8): int {\n"
        "    return seed + 3;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local unsigned numeric range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minValue != 3 ||
        query.numericFact->maxValue != 258 ||
        !query.numericFact->hasUnsignedRange ||
        query.numericFact->minUnsignedValue != 3 ||
        query.numericFact->maxUnsignedValue != 258 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unsigned range payload; status=%d query=(%lld,%d,%d) expr=%p exprType=%d numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld hasUnsigned=%d unsignedMin=%llu unsignedMax=%llu mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
                 query.numericFact != ZR_NULL
                     ? (unsigned long long)query.numericFact->minUnsignedValue
                     : 0ULL,
                 query.numericFact != ZR_NULL
                     ? (unsigned long long)query.numericFact->maxUnsignedValue
                     : 0ULL,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_numeric_overflow_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Numeric Overflow Fact";
    const TZrChar *uriText = "file:///local_numeric_overflow_fact.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    return 9223372036854775807 + 1;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local numeric overflow query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        query.numericFact->hasRange ||
        !query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected overflow fact; status=%d query=(%lld,%d,%d) expr=%p nodeType=%d exprRange=(%lld,%d,%d)-(%lld,%d,%d) numeric=%p numericKind=%d hasRange=%d min=%lld max=%lld mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL && query.expressionFact->node != ZR_NULL
                     ? (int)query.expressionFact->node->type
                     : -1,
                 query.expressionFact != ZR_NULL ? (long long)query.expressionFact->range.start.offset : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.start.line : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.start.column : -1,
                 query.expressionFact != ZR_NULL ? (long long)query.expressionFact->range.end.offset : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.end.line : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->range.end.column : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : -1,
                 query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_float_numeric_range_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Float Numeric Range Fact";
    const TZrChar *uriText = "file:///local_float_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(): double {\n"
        "    return 1.5 + 2.25;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local float numeric range query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.expressionFact->inferredType.baseType != ZR_VALUE_TYPE_DOUBLE ||
        query.numericFact == ZR_NULL ||
        query.numericFact->kind != ZR_SEMANTIC_NUMERIC_FACT_PROMOTION ||
        !query.numericFact->hasRange ||
        query.numericFact->minDoubleValue != 3.75 ||
        query.numericFact->maxDoubleValue != 3.75 ||
        query.numericFact->mayOverflow) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected exact float range; status=%d query=(%lld,%d,%d) expr=%p exprType=%d numeric=%p numericKind=%d hasRange=%d minDouble=%.17g maxDouble=%.17g mayOverflow=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.numericFact,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
                 query.numericFact != ZR_NULL ? query.numericFact->minDoubleValue : -1.0,
                 query.numericFact != ZR_NULL ? query.numericFact->maxDoubleValue : -1.0,
                 query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_logical_short_circuit_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Logical Short Circuit Fact";
    const TZrChar *uriText = "file:///local_logical_short_circuit_fact.zr";
    const TZrChar *content =
        "shorts() {\n"
        "    var skippedOr = true || false;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "||", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local logical short-circuit query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.expressionFact->inferredType.baseType != ZR_VALUE_TYPE_BOOL ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected short-circuit fact; status=%d query=(%lld,%d,%d) expr=%p exprType=%d logical=%p logicalKind=%d hasKnown=%d known=%d reachability=%p cause=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.logicalFact,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->kind : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->hasKnownValue : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->knownValue : -1,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_unary_logical_not_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Unary Logical Not Fact";
    const TZrChar *uriText = "file:///local_unary_logical_not_fact.zr";
    const TZrChar *content =
        "logic() {\n"
        "    var inverted = !true;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "!true", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local unary logical-not query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.expressionFact == ZR_NULL ||
        query.expressionFact->inferredType.baseType != ZR_VALUE_TYPE_BOOL ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE ||
        !query.logicalFact->hasKnownValue ||
        query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unary logical-not false fact; status=%d query=(%lld,%d,%d) expr=%p exprNode=%d exprKind=%d exprType=%d logical=%p logicalKind=%d hasKnown=%d known=%d range=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL && query.expressionFact->node != ZR_NULL
                     ? (int)query.expressionFact->node->type
                     : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 (void *)query.logicalFact,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->kind : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->hasKnownValue : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->knownValue : -1,
                 query.logicalFact != ZR_NULL ? (long long)query.logicalFact->range.start.offset : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->range.start.line : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->range.start.column : -1,
                 query.logicalFact != ZR_NULL ? (long long)query.logicalFact->range.end.offset : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->range.end.line : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->range.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_reference_query_returns_write_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Reference Query Returns Write Fact";
    const TZrChar *uriText = "file:///local_reference_write_fact.zr";
    const TZrChar *content =
        "var counter = 0;\n"
        "bump() {\n"
        "    counter = counter + 1;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "counter", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local reference write query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_WRITE ||
        !query.referenceFact->isResolved ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset ||
        query.referenceFact->declarationRange.start.offset == query.referenceFact->declarationRange.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected write reference fact; status=%d reference=%p kind=%d resolved=%d range=(%lld,%d,%d)-(%lld,%d,%d) declaration=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_reference_query_returns_member_write_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Reference Query Returns Member Write Fact";
    const TZrChar *uriText = "file:///local_reference_member_write_fact.zr";
    const TZrChar *content =
        "var seed = { value: 1 };\n"
        "mutate(): int {\n"
        "    seed.value = 3;\n"
        "    return seed.value;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "value", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local member write reference query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_WRITE ||
        query.referenceFact->isResolved ||
        query.referenceFact->name == ZR_NULL ||
        strcmp(test_string_ptr(query.referenceFact->name), "value") != 0 ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected member write reference fact; status=%d reference=%p kind=%d resolved=%d name=%s range=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL && query.referenceFact->name != ZR_NULL
                         ? test_string_ptr(query.referenceFact->name)
                         : "<null>",
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_reference_query_returns_member_access_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Reference Query Returns Member Access Fact";
    const TZrChar *uriText = "file:///local_reference_member_access_fact.zr";
    const TZrChar *content =
        "var seed = { value: 1 };\n"
        "inspect() {\n"
        "    seed.value;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "value", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local member access reference query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
        query.referenceFact->isResolved ||
        query.referenceFact->name == ZR_NULL ||
        strcmp(test_string_ptr(query.referenceFact->name), "value") != 0 ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected member access reference fact; status=%d reference=%p kind=%d resolved=%d name=%s range=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL && query.referenceFact->name != ZR_NULL
                         ? test_string_ptr(query.referenceFact->name)
                         : "<null>",
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_reference_query_returns_computed_member_access_and_index_read_facts(SZrState *state) {
    const TZrChar *summary = "LSP Local Reference Query Returns Computed Member Access And Index Read Facts";
    const TZrChar *uriText = "file:///local_reference_computed_member_access_fact.zr";
    const TZrChar *content =
        "var seed = { value: 1 };\n"
        "var index = 0;\n"
        "inspect() {\n"
        "    seed[index];\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition bracketPosition;
    SZrLspPosition indexPosition;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "[", 0, 0, &bracketPosition) ||
        !lsp_find_position_for_substring(content, "index", 1, 0, &indexPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local computed member access reference query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, bracketPosition, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false for computed member bracket");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
        query.referenceFact->isResolved ||
        query.referenceFact->name == ZR_NULL ||
        strcmp(test_string_ptr(query.referenceFact->name), "index") != 0 ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected computed member access reference fact; status=%d reference=%p kind=%d resolved=%d name=%s range=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL && query.referenceFact->name != ZR_NULL
                         ? test_string_ptr(query.referenceFact->name)
                         : "<null>",
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, indexPosition, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false for computed member index");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_READ ||
        !query.referenceFact->isResolved ||
        query.referenceFact->name == ZR_NULL ||
        strcmp(test_string_ptr(query.referenceFact->name), "index") != 0 ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset ||
        query.referenceFact->declarationRange.start.offset == query.referenceFact->declarationRange.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected computed member index read reference fact; status=%d reference=%p kind=%d resolved=%d name=%s range=(%lld,%d,%d)-(%lld,%d,%d) declaration=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL && query.referenceFact->name != ZR_NULL
                         ? test_string_ptr(query.referenceFact->name)
                         : "<null>",
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_reference_query_returns_call_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Reference Query Returns Call Fact";
    const TZrChar *uriText = "file:///local_reference_call_fact.zr";
    const TZrChar *content =
        "target(): int {\n"
        "    return 1;\n"
        "}\n"
        "run(): int {\n"
        "    return target();\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "target", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local reference call query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ReferenceAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_CALL ||
        !query.referenceFact->isResolved ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset ||
        query.referenceFact->declarationRange.start.offset == query.referenceFact->declarationRange.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected call reference fact; status=%d reference=%p kind=%d resolved=%d range=(%lld,%d,%d)-(%lld,%d,%d) declaration=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_includes_reference_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Includes Reference Fact";
    const TZrChar *uriText = "file:///local_expression_reference_fact.zr";
    const TZrChar *content =
        "var total = 2;\n"
        "read(): int {\n"
        "    return total;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "total", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local expression reference query fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.referenceFact == ZR_NULL ||
        query.referenceFact->kind != ZR_SEMANTIC_REFERENCE_READ ||
        !query.referenceFact->isResolved ||
        query.referenceFact->range.start.offset == query.referenceFact->range.end.offset ||
        query.referenceFact->declarationRange.start.offset == query.referenceFact->declarationRange.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected expression query to include read reference fact; status=%d expr=%p reference=%p kind=%d resolved=%d range=(%lld,%d,%d)-(%lld,%d,%d) declaration=(%lld,%d,%d)-(%lld,%d,%d)",
                 (int)query.status,
                 (void *)query.expressionFact,
                 (void *)query.referenceFact,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->range.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->range.end.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.start.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.start.column : -1,
                 query.referenceFact != ZR_NULL ? (long long)query.referenceFact->declarationRange.end.offset : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.line : -1,
                 query.referenceFact != ZR_NULL ? (int)query.referenceFact->declarationRange.end.column : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_ownership_violation_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Ownership Violation Fact";
    const TZrChar *uriText = "file:///local_ownership_violation_fact.zr";
    const TZrChar *content =
        "class Resource {\n"
        "}\n"
        "leak(resource: %unique Resource): %loaned Resource {\n"
        "    return %loan(resource);\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    SZrArray diagnostics;
    SZrSemanticAnalyzer *analyzer;
    TZrSize ownershipFactCount = 0;
    const SZrSemanticOwnershipFact *firstOwnershipFact = ZR_NULL;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "%loan", 1, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local ownership query fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        !lsp_diagnostic_array_contains_code(&diagnostics, "loan_escape")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected LSP diagnostics to include loan_escape before local ownership query; count=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.ownershipFact == ZR_NULL ||
        query.ownershipFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
        query.ownershipFact->qualifier != ZR_OWNERSHIP_QUALIFIER_LOANED ||
        !query.ownershipFact->isViolation ||
        query.ownershipFact->diagnosticMessage == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        if (analyzer != ZR_NULL &&
            analyzer->semanticContext != ZR_NULL &&
            analyzer->semanticContext->ownershipFacts.isValid) {
            ownershipFactCount = analyzer->semanticContext->ownershipFacts.length;
            if (ownershipFactCount > 0) {
                firstOwnershipFact =
                    (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->ownershipFacts,
                        0);
            }
        }
        snprintf(reason,
                 sizeof(reason),
                 "Expected ownership violation fact; status=%d expr=%p ownership=%p facts=%llu query=(%lld,%d,%d)-(%lld,%d,%d) first=(%lld,%d,%d)-(%lld,%d,%d) kind=%d qualifier=%d violation=%d message=%p",
                 (int)query.status,
                 (void *)query.expressionFact,
                 (void *)query.ownershipFact,
                 (unsigned long long)ownershipFactCount,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (long long)query.queryRange.end.offset,
                 (int)query.queryRange.end.line,
                 (int)query.queryRange.end.column,
                 firstOwnershipFact != ZR_NULL ? (long long)firstOwnershipFact->range.start.offset : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->range.start.line : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->range.start.column : -1,
                 firstOwnershipFact != ZR_NULL ? (long long)firstOwnershipFact->range.end.offset : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->range.end.line : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->range.end.column : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->kind : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->qualifier : -1,
                 firstOwnershipFact != ZR_NULL ? (int)firstOwnershipFact->isViolation : -1,
                 firstOwnershipFact != ZR_NULL ? (void *)firstOwnershipFact->diagnosticMessage : ZR_NULL);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM LSP Local Semantic Query Tests\n");
    printf("====================================\n\n");

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Failed to get main state\n");
        return 1;
    }
    ZrCore_GlobalState_InitRegistry(state, global);

    test_local_expression_query_returns_numeric_range_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_propagates_variable_numeric_range_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_propagates_expression_statement_numeric_range_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_propagates_integer_interval_range_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_keeps_unsigned_numeric_range_payload(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_numeric_overflow_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_float_numeric_range_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_logical_short_circuit_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_unary_logical_not_fact(state);
    TEST_DIVIDER();
    test_local_reference_query_returns_write_fact(state);
    TEST_DIVIDER();
    test_local_reference_query_returns_member_write_fact(state);
    TEST_DIVIDER();
    test_local_reference_query_returns_member_access_fact(state);
    TEST_DIVIDER();
    test_local_reference_query_returns_computed_member_access_and_index_read_facts(state);
    TEST_DIVIDER();
    test_local_reference_query_returns_call_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_includes_reference_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_ownership_violation_fact(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Local Semantic Query Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
