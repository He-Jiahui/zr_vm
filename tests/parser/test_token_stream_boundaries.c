#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/token_stream.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static void init_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};
    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits.generatedDeclarationCount = ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits.diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits.tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits.expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static SZrTokenStream *from_text(SZrSyntaxContextTable *table, const char *text) {
    SZrTokenStream *stream = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, table, "boundary.zr", sizeof("boundary.zr") - 1u,
            text, strlen(text), ZR_NULL, &stream));
    return stream;
}

static void test_group_has_only_top_level_roots_and_nested_children(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = from_text(table, "outer({[\"\"]}) //end");
    const SZrTokenTree *paren = ZrParser_TokenStream_At(stream, 1u);
    const SZrTokenTree *brace = ZrParser_TokenTree_GroupChildAt(paren, 0u);
    const SZrTokenTree *bracket = ZrParser_TokenTree_GroupChildAt(brace, 0u);
    const SZrTokenTree *literal = ZrParser_TokenTree_GroupChildAt(bracket, 0u);
    SZrTokenNativeSemInfo semantic;

    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_DELIMITER_PAREN, ZrParser_TokenTree_GroupDelimiter(paren));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_DELIMITER_BRACE, ZrParser_TokenTree_GroupDelimiter(brace));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_DELIMITER_BRACKET, ZrParser_TokenTree_GroupDelimiter(bracket));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_NativeSemInfo(literal, &semantic));
    TEST_ASSERT_EQUAL_INT(ZR_TK_STRING, semantic.token);
    TEST_ASSERT_EQUAL_UINT(0u, semantic.text.length);
    TEST_ASSERT_NOT_NULL(ZrParser_TokenStream_TextBytes(stream, semantic.text));
    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_TokenTree_TrailingTriviaCount(paren));
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_failed_group_restores_sources_trees_and_budget(void) {
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *child = from_text(table, "child");
    SZrTokenStream *parent;
    TZrTokenSourceId sourceId;
    SZrTokenSpan span;
    const SZrTokenTree *children[] = {ZrParser_TokenStream_At(child, 0u)};
    SZrParserComptimeBudgetUsage before;
    TZrSize index = 777u;

    init_budget(&budget);
    parent = ZrParser_TokenStream_Create(g_state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            parent, "group.zr", 8u, "(child)", 7u, &sourceId));
    span = (SZrTokenSpan){sourceId, 0u, 7u, 1, 1, 1, 8};
    budget.limits.nodeCount = 1u;
    before = budget.usage;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendGroup(
            parent, ZR_TOKEN_DELIMITER_NONE, ZrParser_TokenSpan_None(),
            ZrParser_TokenSpan_None(), children, 1u, span,
            ZrParser_SyntaxContext_TableRoot(table), ZR_NULL, 0u, ZR_NULL, 0u, &index));
    TEST_ASSERT_EQUAL_UINT(777u, index);
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(parent));
    TEST_ASSERT_EQUAL_MEMORY(&before, &budget.usage, sizeof(before));
    TEST_ASSERT_NULL(ZrParser_TokenStream_SourceBytes(parent, sourceId + 1u, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, budget.exceededResource);
    ZrParser_TokenStream_Release(parent);
    ZrParser_TokenStream_Release(child);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_failed_from_text_and_concat_leave_live_budget_unchanged(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetUsage before;
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *left = from_text(table, "left");
    SZrTokenStream *right = from_text(table, "right");
    SZrTokenStream *result = ZR_NULL;

    init_budget(&budget);
    before = budget.usage;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_FromText(
            g_state, table, "bad.zr", 6u, "{[)}", 4u, &budget, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_MEMORY(&before, &budget.usage, sizeof(before));
    budget.limits.nodeCount = 0u;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_Concat(left, right, &budget, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_MEMORY(&before, &budget.usage, sizeof(before));
    ZrParser_TokenStream_Release(left);
    ZrParser_TokenStream_Release(right);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_native_constructor_rejects_forged_payload_and_context(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TZrTokenSourceId sourceId;
    SZrTokenText raw;
    SZrTokenSpan span;
    SZrTokenNativeSemInfo semantic = {0};
    SZrSyntaxContext context = ZrParser_SyntaxContext_Root();
    TZrSize index = 999u;

    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(stream, "bad.zr", 6u, "+", 1u, &sourceId));
    raw = (SZrTokenText){sourceId, 0u, 1u};
    span = (SZrTokenSpan){sourceId, 0u, 1u, 1, 1, 1, 2};
    semantic.token = ZR_TK_INTEGER;
    semantic.intValue = 42;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendIdent(
            stream, semantic, raw, span, context, ZR_NULL, 0u, ZR_NULL, 0u, &index));
    semantic.token = ZR_TK_PLUS;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendPunct(
            stream, semantic, raw, '+', (EZrTokenSpacing)9, span, context,
            ZR_NULL, 0u, ZR_NULL, 0u, &index));
    context.markId = 1u;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendPunct(
            stream, semantic, raw, '+', ZR_TOKEN_SPACING_ALONE, span, context,
            ZR_NULL, 0u, ZR_NULL, 0u, &index));
    TEST_ASSERT_EQUAL_UINT(999u, index);
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(stream));
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_deep_group_clone_and_token_cursor_are_iterative(void) {
    enum {DEPTH = 4096};
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    char *text = malloc(2u * DEPTH + 2u);
    SZrTokenStream *stream;
    SZrTokenStream *empty;
    SZrTokenStream *copy = ZR_NULL;
    SZrLexState lexer;
    TZrSize count = 0u;

    TEST_ASSERT_NOT_NULL(text);
    memset(text, '(', DEPTH);
    text[DEPTH] = 'x';
    memset(text + DEPTH + 1u, ')', DEPTH);
    text[2u * DEPTH + 1u] = '\0';
    stream = from_text(table, text);
    free(text);
    empty = from_text(table, "");
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(stream, empty, ZR_NULL, &copy));
    ZrParser_TokenStream_Release(stream);
    ZrParser_TokenStream_Release(empty);
    ZrParser_SyntaxContextTable_Release(table);
    ZrParser_Lexer_InitTokenStream(&lexer, g_state, copy, ZR_NULL);
    for (; lexer.t.token != ZR_TK_EOS && count <= 2u * DEPTH + 1u; count++) {
        TEST_ASSERT_FALSE(lexer.t.hasLexError);
        ZrParser_Lexer_Next(&lexer);
    }
    TEST_ASSERT_EQUAL_UINT(2u * DEPTH + 1u, count);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(copy);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_group_has_only_top_level_roots_and_nested_children);
    RUN_TEST(test_failed_group_restores_sources_trees_and_budget);
    RUN_TEST(test_failed_from_text_and_concat_leave_live_budget_unchanged);
    RUN_TEST(test_native_constructor_rejects_forged_payload_and_context);
    RUN_TEST(test_deep_group_clone_and_token_cursor_are_iterative);
    return UNITY_END();
}
