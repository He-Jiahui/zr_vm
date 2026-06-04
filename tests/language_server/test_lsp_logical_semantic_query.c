//
// Focused LSP local semantic query tests for logical facts.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_local_semantic_query.h"
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

static void test_local_expression_query_returns_constant_comparison_logical_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Constant Comparison Logical Fact";
    const TZrChar *uriText = "file:///local_constant_comparison_logical_fact.zr";
    const TZrChar *content =
        "logic() {\n"
        "    var known = 1 < 2;\n"
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
        !lsp_find_position_for_substring(content, "<", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local comparison logical query fixture");
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
        query.expressionFact->kind != ZR_SEMANTIC_EXPRESSION_FACT_BINARY ||
        query.expressionFact->inferredType.baseType != ZR_VALUE_TYPE_BOOL ||
        !query.expressionFact->hasConstant ||
        !query.expressionFact->constantValue.boolValue ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected comparison true logical fact; status=%d query=(%lld,%d,%d) expr=%p exprKind=%d exprType=%d hasConst=%d const=%d logical=%p logicalKind=%d hasKnown=%d known=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasConstant : -1,
                 query.expressionFact != ZR_NULL && query.expressionFact->hasConstant
                     ? (int)query.expressionFact->constantValue.boolValue
                     : -1,
                 (void *)query.logicalFact,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->kind : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->hasKnownValue : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->knownValue : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_expression_query_returns_composed_comparison_logical_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Expression Query Returns Composed Comparison Logical Fact";
    const TZrChar *uriText = "file:///local_composed_comparison_logical_fact.zr";
    const TZrChar *content =
        "logic() {\n"
        "    var known = (1 < 2) && (3 < 4);\n"
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
        !lsp_find_position_for_substring(content, "&&", 0, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare composed logical query fixture");
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
        query.expressionFact->kind != ZR_SEMANTIC_EXPRESSION_FACT_BINARY ||
        query.expressionFact->inferredType.baseType != ZR_VALUE_TYPE_BOOL ||
        !query.expressionFact->hasConstant ||
        !query.expressionFact->constantValue.boolValue ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected composed true logical fact; status=%d query=(%lld,%d,%d) expr=%p exprKind=%d exprType=%d hasConst=%d const=%d logical=%p logicalKind=%d hasKnown=%d known=%d",
                 (int)query.status,
                 (long long)query.queryRange.start.offset,
                 (int)query.queryRange.start.line,
                 (int)query.queryRange.start.column,
                 (void *)query.expressionFact,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
                 query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasConstant : -1,
                 query.expressionFact != ZR_NULL && query.expressionFact->hasConstant
                     ? (int)query.expressionFact->constantValue.boolValue
                     : -1,
                 (void *)query.logicalFact,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->kind : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->hasKnownValue : -1,
                 query.logicalFact != ZR_NULL ? (int)query.logicalFact->knownValue : -1);
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

    printf("ZR VM LSP Logical Semantic Query Tests\n");
    printf("======================================\n\n");

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

    test_local_expression_query_returns_constant_comparison_logical_fact(state);
    TEST_DIVIDER();
    test_local_expression_query_returns_composed_comparison_logical_fact(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Logical Semantic Query Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
