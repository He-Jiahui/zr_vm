/* RED coverage for the native immutable macro token stream contract. */

#include "unity.h"

#include "test_support.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/token_stream.h"

void setUp(void) {
}

void tearDown(void) {
}

static void init_test_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};

    limits.fuel = 100000u;
    limits.callDepth = 128u;
    limits.heapBytes = 1u << 20;
    limits.aggregateCount = 100000u;
    limits.generatedDeclarationCount = 10000u;
    limits.diagnosticCount = 1024u;
    limits.tokenCount = 100000u;
    limits.nodeCount = 100000u;
    limits.matchingCount = 100000u;
    limits.expansionCount = 10000u;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static void test_token_stream_copies_source_and_preserves_raw_spelling(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenText decoded;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenSpan span = {0};
    TZrSize identIndex = 0;
    const SZrTokenTree *ident;
    TZrChar source[] = "r#name /* kept */";
    const TZrChar decodedName[] = "name";
    const TZrChar *rawBytes;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    stream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "input.zr", 8u, source, sizeof(source) - 1u, &sourceId));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_TOKEN_SOURCE_INVALID, sourceId);
    source[0] = 'X';
    TEST_ASSERT_TRUE(ZrParser_TokenStream_CopyText(
            stream, decodedName, sizeof(decodedName) - 1u, &decoded));

    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = decoded;
    semInfo.isRawIdentifier = ZR_TRUE;
    span.sourceId = sourceId;
    span.startOffset = 0u;
    span.endOffset = 6u;
    span.startLine = 1;
    span.startColumn = 1;
    span.endLine = 1;
    span.endColumn = 7;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 0u, 6u},
            span,
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &identIndex));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    ident = ZrParser_TokenStream_At(stream, identIndex);
    TEST_ASSERT_NOT_NULL(ident);
    TEST_ASSERT_EQUAL(ZR_TOKEN_TREE_IDENT, ZrParser_TokenTree_Kind(ident));
    TEST_ASSERT_EQUAL_UINT32(6u, ZrParser_TokenTree_Text(ident).length);
    rawBytes = ZrParser_TokenStream_TextBytes(stream, ZrParser_TokenTree_Text(ident));
    TEST_ASSERT_NOT_NULL(rawBytes);
    TEST_ASSERT_EQUAL_STRING_LEN("r#name", rawBytes, 6u);
    TEST_ASSERT_EQUAL(ZR_TK_IDENTIFIER,
                      ZrParser_TokenTree_NativeSemInfo(ident, &semInfo) ? semInfo.token : ZR_TK_EOS);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_token_stream_groups_and_trivia_are_immutable_views(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenSpan span = {0};
    SZrTokenTrivia trivia = {ZR_TOKEN_TRIVIA_BLOCK_COMMENT, {0u, 1u, 5u}};
    TZrSize childIndex = 0;
    TZrSize groupIndex = 0;
    const SZrTokenTree *children[1];
    const SZrTokenTree *group;
    const TZrChar source[] = "(/*x*/name)";
    SZrTokenStream *childStream;
    TZrTokenSourceId childSourceId = ZR_TOKEN_SOURCE_INVALID;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    childStream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(childStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            childStream, "group.zr", 8u, source, sizeof(source) - 1u, &childSourceId));
    stream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "group.zr", 8u, source, sizeof(source) - 1u, &sourceId));
    trivia.text.sourceId = childSourceId;
    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = (SZrTokenText){childSourceId, 6u, 4u};
    span = (SZrTokenSpan){childSourceId, 6u, 10u, 1, 7, 1, 11};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            childStream, semInfo, semInfo.text, span,
            ZrParser_SyntaxContext_TableRoot(table),
            &trivia, 1u, ZR_NULL, 0u, &childIndex));
    /* A group snapshots/imports children from an already sealed child stream;
     * it must not embed an unfinished node owned by the destination builder. */
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(childStream));
    children[0] = ZrParser_TokenStream_At(childStream, childIndex);
    TEST_ASSERT_NOT_NULL(children[0]);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
            stream,
            ZR_TOKEN_DELIMITER_PAREN,
            (SZrTokenSpan){sourceId, 0u, 1u, 1, 1, 1, 2},
            (SZrTokenSpan){sourceId, 10u, 11u, 1, 11, 1, 12},
            children,
            1u,
            (SZrTokenSpan){sourceId, 0u, 11u, 1, 1, 1, 12},
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL, 0u, ZR_NULL, 0u,
            &groupIndex));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_UINT(0u, groupIndex);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    group = ZrParser_TokenStream_At(stream, groupIndex);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL(ZR_TOKEN_TREE_GROUP, ZrParser_TokenTree_Kind(group));
    TEST_ASSERT_EQUAL(ZR_TOKEN_DELIMITER_PAREN,
                      ZrParser_TokenTree_GroupDelimiter(group));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenTree_GroupChildCount(group));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenTree_LeadingTriviaCount(
            ZrParser_TokenTree_GroupChildAt(group, 0u)));

    ZrParser_TokenStream_Retain(stream);
    ZrParser_TokenStream_Release(stream);
    ZrParser_TokenStream_Release(stream);
    ZrParser_TokenStream_Release(childStream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_token_stream_copies_source_and_preserves_raw_spelling);
    RUN_TEST(test_token_stream_groups_and_trivia_are_immutable_views);
    return UNITY_END();
}
