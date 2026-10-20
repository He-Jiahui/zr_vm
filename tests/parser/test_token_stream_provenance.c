/* RED coverage for token-source provenance and lossless punctuation/trivia. */

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

static SZrTokenSpan test_span(
        TZrTokenSourceId sourceId,
        TZrSize startOffset,
        TZrSize endOffset) {
    SZrTokenSpan span;

    span.sourceId = sourceId;
    span.startOffset = startOffset;
    span.endOffset = endOffset;
    span.startLine = 1;
    span.startColumn = (TZrInt32)(startOffset + 1u);
    span.endLine = 1;
    span.endColumn = (TZrInt32)(endOffset + 1u);
    return span;
}

static TZrBool append_test_ident(
        SZrTokenStream *stream,
        SZrSyntaxContextTable *table,
        TZrTokenSourceId sourceId,
        const TZrChar *name,
        TZrSize nameLength,
        TZrSize *outIndex) {
    SZrTokenText semanticText;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText rawText = {sourceId, 0u, nameLength};

    if (!ZrParser_TokenStream_CopyText(
                stream, name, nameLength, &semanticText)) {
        return ZR_FALSE;
    }
    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = semanticText;
    return ZrParser_TokenStream_AppendIdent(
            stream,
            semInfo,
            rawText,
            test_span(sourceId, 0u, nameLength),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            outIndex);
}

static void test_group_imports_children_from_two_sealed_owners(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *destination;
    TZrTokenSourceId leftSource = ZR_TOKEN_SOURCE_INVALID;
    TZrTokenSourceId rightSource = ZR_TOKEN_SOURCE_INVALID;
    TZrTokenSourceId destinationSource = ZR_TOKEN_SOURCE_INVALID;
    TZrSize leftIndex = 0u;
    TZrSize rightIndex = 0u;
    TZrSize groupIndex = 0u;
    const SZrTokenTree *children[2];
    const SZrTokenTree *group;
    const SZrTokenTree *child;
    SZrTokenText text;
    const TZrChar *bytes;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    left = ZrParser_TokenStream_Create(state->global, table, &budget);
    right = ZrParser_TokenStream_Create(state->global, table, &budget);
    destination = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_NOT_NULL(right);
    TEST_ASSERT_NOT_NULL(destination);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            left, "left.zr", 7u, "a", 1u, &leftSource));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            right, "right.zr", 8u, "b", 1u, &rightSource));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            destination, "group.zr", 8u, "()", 2u, &destinationSource));
    TEST_ASSERT_TRUE(append_test_ident(
            left, table, leftSource, "a", 1u, &leftIndex));
    TEST_ASSERT_TRUE(append_test_ident(
            right, table, rightSource, "b", 1u, &rightIndex));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(left));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(right));
    children[0] = ZrParser_TokenStream_At(left, leftIndex);
    children[1] = ZrParser_TokenStream_At(right, rightIndex);
    TEST_ASSERT_NOT_NULL(children[0]);
    TEST_ASSERT_NOT_NULL(children[1]);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
            destination,
            ZR_TOKEN_DELIMITER_PAREN,
            test_span(destinationSource, 0u, 1u),
            test_span(destinationSource, 1u, 2u),
            children,
            2u,
            test_span(destinationSource, 0u, 2u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &groupIndex));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(destination));
    group = ZrParser_TokenStream_At(destination, groupIndex);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_TokenTree_GroupChildCount(group));

    child = ZrParser_TokenTree_GroupChildAt(group, 0u);
    TEST_ASSERT_NOT_NULL(child);
    text = ZrParser_TokenTree_Text(child);
    bytes = ZrParser_TokenStream_TextBytes(destination, text);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_STRING_LEN("a", bytes, 1u);
    child = ZrParser_TokenTree_GroupChildAt(group, 1u);
    TEST_ASSERT_NOT_NULL(child);
    text = ZrParser_TokenTree_Text(child);
    bytes = ZrParser_TokenStream_TextBytes(destination, text);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_STRING_LEN("b", bytes, 1u);

    ZrParser_TokenStream_Release(destination);
    ZrParser_TokenStream_Release(right);
    ZrParser_TokenStream_Release(left);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_empty_group_retains_interior_comment_trivia(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream = ZR_NULL;
    const SZrTokenTree *group;
    SZrTokenTrivia observed;
    const TZrChar *bytes;
    const TZrChar source[] = "(/*x*/)";

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "empty.zr", 8u, source, sizeof(source) - 1u,
            &budget, &stream));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenStream_Count(stream));
    group = ZrParser_TokenStream_At(stream, 0u);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL(ZR_TOKEN_TREE_GROUP, ZrParser_TokenTree_Kind(group));
    TEST_ASSERT_EQUAL(ZR_TOKEN_DELIMITER_PAREN,
                      ZrParser_TokenTree_GroupDelimiter(group));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenTree_GroupChildCount(group));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenTree_LeadingTriviaCount(group));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenTree_GroupInnerTriviaCount(group));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_GroupInnerTriviaAt(
            group, 0u, &observed));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TRIVIA_BLOCK_COMMENT, observed.kind);
    bytes = ZrParser_TokenStream_TextBytes(stream, observed.text);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_STRING_LEN("/*x*/", bytes, 5u);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_from_text_keeps_adjacent_punctuation_spacing(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream = ZR_NULL;
    const SZrTokenTree *first;
    const SZrTokenTree *second;
    SZrTokenText firstText;
    SZrTokenText secondText;
    const TZrChar *firstBytes;
    const TZrChar *secondBytes;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state,
            table,
            "punct.zr",
            8u,
            "##",
            2u,
            &budget,
            &stream));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_TokenStream_Count(stream));
    first = ZrParser_TokenStream_At(stream, 0u);
    second = ZrParser_TokenStream_At(stream, 1u);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL(ZR_TOKEN_TREE_PUNCT, ZrParser_TokenTree_Kind(first));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TREE_PUNCT, ZrParser_TokenTree_Kind(second));
    TEST_ASSERT_EQUAL(ZR_TOKEN_SPACING_JOINT,
                      ZrParser_TokenTree_PunctSpacing(first));
    TEST_ASSERT_EQUAL(ZR_TOKEN_SPACING_ALONE,
                      ZrParser_TokenTree_PunctSpacing(second));
    firstText = ZrParser_TokenTree_Text(first);
    secondText = ZrParser_TokenTree_Text(second);
    firstBytes = ZrParser_TokenStream_TextBytes(stream, firstText);
    secondBytes = ZrParser_TokenStream_TextBytes(stream, secondText);
    TEST_ASSERT_EQUAL_STRING_LEN("#", firstBytes, 1u);
    TEST_ASSERT_EQUAL_STRING_LEN("#", secondBytes, 1u);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_punct_constructor_rejects_ascii_identifier(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    TZrSize index = 0u;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    stream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "invalid.zr", 10u, "a", 1u, &sourceId));
    semInfo.token = (EZrToken)'a';
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendPunct(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 0u, 1u},
            (TZrUInt32)'a',
            ZR_TOKEN_SPACING_ALONE,
            test_span(sourceId, 0u, 1u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &index));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(stream));

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_textual_semantics_require_decoded_owned_text(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText missingText = {ZR_TOKEN_SOURCE_INVALID, 0u, 0u};
    TZrSize index = 0u;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    stream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "ident.zr", 8u, "name", 4u, &sourceId));

    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = missingText;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendIdent(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 0u, 4u},
            test_span(sourceId, 0u, 4u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &index));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(stream));

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_string_semantics_need_text_but_integer_does_not(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenNativeSemInfo observed = {0};
    SZrTokenText missingText = {ZR_TOKEN_SOURCE_INVALID, 0u, 0u};
    TZrSize index = 0u;
    const TZrChar source[] = "\"x\" `y` 42";

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    stream = ZrParser_TokenStream_Create(state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "literal.zr", 10u, source, sizeof(source) - 1u,
            &sourceId));

    semInfo.token = ZR_TK_STRING;
    semInfo.text = missingText;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendLiteral(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 0u, 3u},
            test_span(sourceId, 0u, 3u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &index));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(stream));

    semInfo.token = ZR_TK_TEMPLATE_STRING;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendLiteral(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 4u, 3u},
            test_span(sourceId, 4u, 7u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &index));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(stream));

    semInfo.token = ZR_TK_INTEGER;
    semInfo.intValue = 42;
    semInfo.text = missingText;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendLiteral(
            stream,
            semInfo,
            (SZrTokenText){sourceId, 8u, 2u},
            test_span(sourceId, 8u, 10u),
            ZrParser_SyntaxContext_TableRoot(table),
            ZR_NULL,
            0u,
            ZR_NULL,
            0u,
            &index));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_UINT(0u, index);
    TEST_ASSERT_TRUE(ZrParser_TokenTree_NativeSemInfo(
            ZrParser_TokenStream_At(stream, index), &observed));
    TEST_ASSERT_EQUAL_INT64(42, observed.intValue);
    TEST_ASSERT_EQUAL_UINT(ZR_TOKEN_SOURCE_INVALID, observed.text.sourceId);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

static void test_comment_only_and_concat_trivia_are_lossless(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *commentOnly = ZR_NULL;
    SZrTokenStream *prefix = ZR_NULL;
    SZrTokenStream *suffix = ZR_NULL;
    SZrTokenStream *joined = ZR_NULL;
    SZrTokenStream *emptyPrefix = ZR_NULL;
    SZrTokenStream *emptySuffix = ZR_NULL;
    SZrTokenStream *emptyJoined = ZR_NULL;
    const SZrTokenTree *first;
    const SZrTokenTree *second;
    SZrTokenTrivia trivia;
    const TZrChar *bytes;

    TEST_ASSERT_NOT_NULL(state);
    init_test_budget(&budget);
    table = ZrParser_SyntaxContextTable_Create(state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "comment.zr", 10u,
            "/*only*/", 8u, &budget, &commentOnly));
    TEST_ASSERT_NOT_NULL(commentOnly);
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(commentOnly));
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenStream_EmptyTriviaCount(commentOnly));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_EmptyTriviaAt(
            commentOnly, 0u, &trivia));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TRIVIA_BLOCK_COMMENT, trivia.kind);
    bytes = ZrParser_TokenStream_TextBytes(commentOnly, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("/*only*/", bytes, 8u);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "prefix.zr", 9u,
            "/*pre*/a", 8u, &budget, &prefix));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "suffix.zr", 9u,
            "b/*post*/", 9u, &budget, &suffix));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(
            prefix, suffix, &budget, &joined));
    TEST_ASSERT_NOT_NULL(joined);
    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_TokenStream_Count(joined));
    first = ZrParser_TokenStream_At(joined, 0u);
    second = ZrParser_TokenStream_At(joined, 1u);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenTree_LeadingTriviaCount(first));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_LeadingTriviaAt(first, 0u, &trivia));
    bytes = ZrParser_TokenStream_TextBytes(joined, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("/*pre*/", bytes, 7u);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_TokenTree_TrailingTriviaCount(second));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_TrailingTriviaAt(second, 0u, &trivia));
    bytes = ZrParser_TokenStream_TextBytes(joined, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("/*post*/", bytes, 8u);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "empty-left.zr", 13u,
            "//left\n", 7u, &budget, &emptyPrefix));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            state, table, "empty-right.zr", 14u,
            "/*right*/", 9u, &budget, &emptySuffix));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(
            emptyPrefix, emptySuffix, &budget, &emptyJoined));
    TEST_ASSERT_EQUAL_UINT(0u, ZrParser_TokenStream_Count(emptyJoined));
    TEST_ASSERT_EQUAL_UINT(3u, ZrParser_TokenStream_EmptyTriviaCount(emptyJoined));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_EmptyTriviaAt(emptyJoined, 0u, &trivia));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TRIVIA_LINE_COMMENT, trivia.kind);
    bytes = ZrParser_TokenStream_TextBytes(emptyJoined, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("//left", bytes, 6u);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_EmptyTriviaAt(emptyJoined, 1u, &trivia));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TRIVIA_NEWLINE, trivia.kind);
    bytes = ZrParser_TokenStream_TextBytes(emptyJoined, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("\n", bytes, 1u);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_EmptyTriviaAt(emptyJoined, 2u, &trivia));
    TEST_ASSERT_EQUAL(ZR_TOKEN_TRIVIA_BLOCK_COMMENT, trivia.kind);
    bytes = ZrParser_TokenStream_TextBytes(emptyJoined, trivia.text);
    TEST_ASSERT_EQUAL_STRING_LEN("/*right*/", bytes, 9u);

    ZrParser_TokenStream_Release(emptyJoined);
    ZrParser_TokenStream_Release(emptySuffix);
    ZrParser_TokenStream_Release(emptyPrefix);
    ZrParser_TokenStream_Release(joined);
    ZrParser_TokenStream_Release(suffix);
    ZrParser_TokenStream_Release(prefix);
    ZrParser_TokenStream_Release(commentOnly);
    ZrParser_SyntaxContextTable_Release(table);
    ZrTests_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_group_imports_children_from_two_sealed_owners);
    RUN_TEST(test_empty_group_retains_interior_comment_trivia);
    RUN_TEST(test_from_text_keeps_adjacent_punctuation_spacing);
    RUN_TEST(test_punct_constructor_rejects_ascii_identifier);
    RUN_TEST(test_textual_semantics_require_decoded_owned_text);
    RUN_TEST(test_string_semantics_need_text_but_integer_does_not);
    RUN_TEST(test_comment_only_and_concat_trivia_are_lossless);
    return UNITY_END();
}
