//
// Focused LSP reachability semantic query regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static void test_local_query_returns_exhaustive_branch_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Exhaustive Branch Reachability Fact";
    const TZrChar *uriText = "file:///local_exhaustive_branch_reachability.zr";
    const TZrChar *content =
        "func choose(flag: bool): int {\n"
        "    if (flag) {\n"
        "        return 1;\n"
        "    } else {\n"
        "        return 2;\n"
        "    }\n"
        "    var deadAfterBranches = 3;\n"
        "    return deadAfterBranches;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "deadAfterBranches", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare exhaustive branch reachability fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_IF_EXPRESSION) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unreachable fact caused by the preceding exhaustive if/else; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_query_returns_constant_conditional_branch_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Constant Conditional Branch Reachability Fact";
    const TZrChar *uriText = "file:///local_constant_conditional_reachability.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    return true ? 1 : 2;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[640];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "2", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare constant conditional branch fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_BOOLEAN_LITERAL ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected skipped ternary branch fact; status=%d expr=%p reachability=%p cause=%d causeNodeType=%d logical=%p logicalKind=%d hasKnown=%d known=%d",
                 (int)query.status,
                 (void *)query.expressionFact,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
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

static void test_local_query_returns_exhaustive_switch_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Exhaustive Switch Reachability Fact";
    const TZrChar *uriText = "file:///local_exhaustive_switch_reachability.zr";
    const TZrChar *content =
        "func choose(value: int): int {\n"
        "    switch (value) {\n"
        "        (0) {\n"
        "            return 0;\n"
        "        }\n"
        "        (1) {\n"
        "            return 1;\n"
        "        }\n"
        "        () {\n"
        "            return 2;\n"
        "        }\n"
        "    }\n"
        "    var deadAfterSwitch = 3;\n"
        "    return deadAfterSwitch;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "deadAfterSwitch", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare exhaustive switch reachability fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_SWITCH_EXPRESSION) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unreachable fact caused by the preceding exhaustive switch; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_query_returns_constant_true_loop_exit_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Constant True Loop Exit Reachability Fact";
    const TZrChar *uriText = "file:///local_constant_true_loop_exit_reachability.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    while (true) {\n"
        "        return 1;\n"
        "    }\n"
        "    var deadAfterLoop = 2;\n"
        "    return deadAfterLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[640];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "deadAfterLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare constant-true loop reachability fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_WHILE_LOOP ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unreachable fact caused by a non-fallthrough constant-true loop; status=%d fact=%p cause=%d causeNodeType=%d logical=%p logicalKind=%d hasKnown=%d known=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
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

static void test_local_query_returns_infinite_for_loop_exit_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Infinite For Loop Exit Reachability Fact";
    const TZrChar *uriText = "file:///local_infinite_for_loop_exit_reachability.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    for (;;) {\n"
        "        return 1;\n"
        "    }\n"
        "    var deadAfterForLoop = 2;\n"
        "    return deadAfterForLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "deadAfterForLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare infinite for-loop reachability fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_FOR_LOOP) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unreachable fact caused by a non-fallthrough infinite for-loop; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_query_keeps_constant_true_loop_with_break_reachable(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Keeps Constant True Loop With Break Reachable";
    const TZrChar *uriText = "file:///local_constant_true_loop_break_reachable.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    while (true) {\n"
        "        break;\n"
        "        return 1;\n"
        "    }\n"
        "    var afterLoop = 2;\n"
        "    return afterLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "afterLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare constant-true loop break fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.reachabilityFact != ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected no reachability fact after a breaking constant-true loop; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 (int)query.reachabilityFact->cause,
                 query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_query_keeps_infinite_for_loop_with_break_reachable(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Keeps Infinite For Loop With Break Reachable";
    const TZrChar *uriText = "file:///local_infinite_for_loop_break_reachable.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    for (;;) {\n"
        "        break;\n"
        "        return 1;\n"
        "    }\n"
        "    var afterForLoop = 2;\n"
        "    return afterForLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "afterForLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare infinite for-loop break fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.reachabilityFact != ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected no reachability fact after a breaking infinite for-loop; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 (int)query.reachabilityFact->cause,
                 query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_local_query_returns_nested_loop_exit_reachability_fact(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Returns Nested Loop Exit Reachability Fact";
    const TZrChar *uriText = "file:///local_nested_loop_exit_reachability.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    while (true) {\n"
        "        while (true) {\n"
        "            return 1;\n"
        "        }\n"
        "    }\n"
        "    var deadAfterNestedLoop = 2;\n"
        "    return deadAfterNestedLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[640];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "deadAfterNestedLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare nested loop reachability fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT ||
        query.reachabilityFact == ZR_NULL ||
        query.reachabilityFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        query.reachabilityFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP ||
        query.reachabilityFact->causeNode == ZR_NULL ||
        query.reachabilityFact->causeNode->type != ZR_AST_WHILE_LOOP ||
        query.logicalFact == ZR_NULL ||
        query.logicalFact->kind != ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE ||
        !query.logicalFact->hasKnownValue ||
        !query.logicalFact->knownValue) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected unreachable fact caused by a nested non-fallthrough loop; status=%d fact=%p cause=%d causeNodeType=%d logical=%p logicalKind=%d hasKnown=%d known=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
                 query.reachabilityFact != ZR_NULL && query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
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

static void test_local_query_keeps_nested_breaking_loop_reachable(SZrState *state) {
    const TZrChar *summary = "LSP Local Query Keeps Nested Breaking Loop Reachable";
    const TZrChar *uriText = "file:///local_nested_breaking_loop_reachable.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    while (true) {\n"
        "        while (true) {\n"
        "            break;\n"
        "            return 1;\n"
        "        }\n"
        "        break;\n"
        "    }\n"
        "    var afterNestedLoop = 2;\n"
        "    return afterNestedLoop;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "afterNestedLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare nested breaking loop fixture");
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "ExpressionAt returned false");
        return;
    }

    if (query.reachabilityFact != ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected no reachability fact after a nested breaking loop; status=%d fact=%p cause=%d causeNodeType=%d",
                 (int)query.status,
                 (void *)query.reachabilityFact,
                 (int)query.reachabilityFact->cause,
                 query.reachabilityFact->causeNode != ZR_NULL
                     ? (int)query.reachabilityFact->causeNode->type
                     : -1);
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

    printf("ZR VM Language Server Reachability Semantic Query Tests\n");
    printf("=====================================================\n\n");

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

    test_local_query_returns_exhaustive_branch_reachability_fact(state);
    test_local_query_returns_constant_conditional_branch_reachability_fact(state);
    test_local_query_returns_exhaustive_switch_reachability_fact(state);
    test_local_query_returns_constant_true_loop_exit_reachability_fact(state);
    test_local_query_returns_infinite_for_loop_exit_reachability_fact(state);
    test_local_query_keeps_constant_true_loop_with_break_reachable(state);
    test_local_query_keeps_infinite_for_loop_with_break_reachable(state);
    test_local_query_returns_nested_loop_exit_reachability_fact(state);
    test_local_query_keeps_nested_breaking_loop_reachable(state);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Reachability Semantic Query Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
