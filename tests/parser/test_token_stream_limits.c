/* RED coverage for token-stream ownership, source parsing, and resource limits. */
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/token_stream.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrSyntaxContextTable *create_context_table(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    return table;
}

static SZrTokenStream *from_text_named(const TZrChar *source,
                                       TZrSize sourceLength,
                                       SZrSyntaxContextTable *table,
                                       const TZrChar *sourceName,
                                       TZrSize sourceNameLength) {
    SZrTokenStream *stream = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, table, sourceName, sourceNameLength,
            source, sourceLength, ZR_NULL, &stream));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(stream));
    return stream;
}

static SZrTokenStream *from_text(const TZrChar *source,
                                 TZrSize sourceLength,
                                 SZrSyntaxContextTable *table) {
    static const TZrChar sourceName[] = "token_stream_limits.zr";

    return from_text_named(source, sourceLength, table,
                           sourceName, sizeof(sourceName) - 1U);
}

static void init_default_budget_limits(SZrParserComptimeBudgetLimits *limits) {
    memset(limits, 0, sizeof(*limits));
    limits->fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits->callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits->heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits->aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits->generatedDeclarationCount =
            ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits->diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits->tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits->nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits->matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits->expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
}

static void assert_token_text(const SZrTokenStream *stream,
                              const SZrTokenText text,
                              const TZrChar *expected,
                              TZrSize expectedLength) {
    const TZrChar *bytes;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)expectedLength, (uint64_t)text.length);
    bytes = ZrParser_TokenStream_TextBytes(stream, text);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, expectedLength);
}

static void assert_raw_text(const SZrTokenStream *stream,
                            const SZrTokenTree *tree,
                            const TZrChar *expected) {
    SZrTokenText raw = ZrParser_TokenTree_Text(tree);

    assert_token_text(stream, raw, expected, strlen(expected));
}

static SZrTokenSpan make_span(TZrTokenSourceId sourceId,
                              TZrSize startOffset,
                              TZrSize endOffset) {
    SZrTokenSpan span;

    memset(&span, 0, sizeof(span));
    span.sourceId = sourceId;
    span.startOffset = startOffset;
    span.endOffset = endOffset;
    span.startLine = 1;
    span.endLine = 1;
    span.startColumn = (TZrInt32)startOffset + 1;
    span.endColumn = (TZrInt32)endOffset + 1;
    return span;
}

static void assert_rejected_without_output(const TZrChar *source,
                                           TZrSize sourceLength) {
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *stream = ZR_NULL;

    TEST_ASSERT_FALSE(ZrParser_TokenStream_FromText(
            g_state, table, "invalid.zr", sizeof("invalid.zr") - 1U,
            source, sourceLength, ZR_NULL, &stream));
    TEST_ASSERT_NULL(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Empty and trivia-only input should seal successfully without publishing a tree. */
static void test_from_text_empty_and_comments(void) {
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *empty = from_text("", 0U, table);
    SZrTokenStream *comments = from_text(
            "// line comment\n/* block comment */",
            sizeof("// line comment\n/* block comment */") - 1U, table);

    TEST_ASSERT_EQUAL_UINT(0U, (unsigned)ZrParser_TokenStream_Count(empty));
    TEST_ASSERT_EQUAL_UINT(0U, (unsigned)ZrParser_TokenStream_Count(comments));
    ZrParser_TokenStream_Release(empty);
    ZrParser_TokenStream_Release(comments);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Comments are trivia on adjacent trees, while comment-only source bytes are
 * retained even when there is no published token tree. */
static void test_comments_preserve_trivia_and_owned_source(void) {
    static const TZrChar source[] = "/*before*/name//after";
    static const TZrChar commentOnly[] = "/*comment-only*/";
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *stream = from_text(source, sizeof(source) - 1U, table);
    const SZrTokenTree *tree;
    SZrTokenTrivia leading;
    SZrTokenTrivia trailing;
    SZrTokenStream *owned = ZrParser_TokenStream_Create(
            g_state->global, table, ZR_NULL);
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    TZrSize sourceLength = 0U;
    const TZrChar *sourceBytes;

    TEST_ASSERT_NOT_NULL(owned);
    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)ZrParser_TokenStream_Count(stream));
    tree = ZrParser_TokenStream_At(stream, 0U);
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT_EQUAL_UINT(1U,
                           (unsigned)ZrParser_TokenTree_LeadingTriviaCount(tree));
    TEST_ASSERT_EQUAL_UINT(1U,
                           (unsigned)ZrParser_TokenTree_TrailingTriviaCount(tree));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_LeadingTriviaAt(tree, 0U, &leading));
    TEST_ASSERT_TRUE(ZrParser_TokenTree_TrailingTriviaAt(tree, 0U, &trailing));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TRIVIA_BLOCK_COMMENT, (int)leading.kind);
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TRIVIA_LINE_COMMENT, (int)trailing.kind);
    assert_token_text(stream, leading.text, "/*before*/",
                      sizeof("/*before*/") - 1U);
    assert_token_text(stream, trailing.text, "//after",
                      sizeof("//after") - 1U);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            owned, "comments-only.zr", sizeof("comments-only.zr") - 1U,
            commentOnly, sizeof(commentOnly) - 1U, &sourceId));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(owned));
    TEST_ASSERT_EQUAL_UINT(0U, (unsigned)ZrParser_TokenStream_Count(owned));
    sourceBytes = ZrParser_TokenStream_SourceBytes(owned, sourceId,
                                                   &sourceLength);
    TEST_ASSERT_NOT_NULL(sourceBytes);
    TEST_ASSERT_EQUAL_UINT64(sizeof(commentOnly) - 1U,
                             (uint64_t)sourceLength);
    TEST_ASSERT_EQUAL_MEMORY(commentOnly, sourceBytes, sourceLength);

    ZrParser_TokenStream_Release(stream);
    ZrParser_TokenStream_Release(owned);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Raw spelling stays available while NativeSemInfo exposes decoded semantics. */
static void test_from_text_preserves_string_and_numeric_spelling(void) {
    static const TZrChar source[] = "\"a\\n\" 0x2A";
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *stream = from_text(source, sizeof(source) - 1U, table);
    const SZrTokenTree *stringTree = ZrParser_TokenStream_At(stream, 0U);
    const SZrTokenTree *numberTree = ZrParser_TokenStream_At(stream, 1U);
    SZrTokenNativeSemInfo stringSem;
    SZrTokenNativeSemInfo numberSem;

    TEST_ASSERT_NOT_NULL(stringTree);
    TEST_ASSERT_NOT_NULL(numberTree);
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TREE_LITERAL,
                          (int)ZrParser_TokenTree_Kind(stringTree));
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TREE_LITERAL,
                          (int)ZrParser_TokenTree_Kind(numberTree));
    assert_raw_text(stream, stringTree, "\"a\\n\"");
    assert_raw_text(stream, numberTree, "0x2A");

    TEST_ASSERT_TRUE(ZrParser_TokenTree_NativeSemInfo(stringTree, &stringSem));
    TEST_ASSERT_EQUAL_INT(ZR_TK_STRING, (int)stringSem.token);
    assert_token_text(stream, stringSem.text, "a\n", 2U);
    TEST_ASSERT_TRUE(ZrParser_TokenTree_NativeSemInfo(numberTree, &numberSem));
    TEST_ASSERT_EQUAL_INT(ZR_TK_INTEGER, (int)numberSem.token);
    TEST_ASSERT_EQUAL_INT64(42, numberSem.intValue);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Delimiter and lexical failures must consume the complete source and publish nothing. */
static void test_from_text_rejects_mismatch_and_invalid_lexical_input(void) {
    static const TZrChar mismatched[] = "(value";
    static const TZrChar unterminated[] = "\"unterminated";
    static const TZrChar trailingInvalid[] = "value \"unterminated";

    assert_rejected_without_output(mismatched, sizeof(mismatched) - 1U);
    assert_rejected_without_output(unterminated, sizeof(unterminated) - 1U);
    assert_rejected_without_output(trailingInvalid,
                                   sizeof(trailingInvalid) - 1U);
}

/* A failed append is atomic: existing trees and committed usage remain unchanged. */
static void test_tiny_budget_preserves_preexisting_stream(void) {
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenText raw;
    SZrTokenNativeSemInfo semInfo;
    SZrSyntaxContext context;
    TZrSize index = 0U;
    TZrSize retainedIndex;
    TZrUInt64 tokenUsage;
    TZrUInt64 nodeUsage;

    init_default_budget_limits(&limits);
    limits.tokenCount = 1U;
    limits.nodeCount = 1U;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = ZrParser_SyntaxContextTable_Create(g_state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "budget.zr", sizeof("budget.zr") - 1U,
            "keep next", sizeof("keep next") - 1U, &sourceId));

    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_IDENTIFIER;
    raw.sourceId = sourceId;
    raw.offset = 0U;
    raw.length = 4U;
    semInfo.text = raw;
    context = ZrParser_SyntaxContext_TableRoot(table);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, raw, make_span(sourceId, 0U, 4U), context,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)ZrParser_TokenStream_Count(stream));
    tokenUsage = budget.usage.tokenCount;
    nodeUsage = budget.usage.nodeCount;
    retainedIndex = index;

    raw.offset = 5U;
    raw.length = 4U;
    semInfo.text = raw;
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, raw, make_span(sourceId, 5U, 9U), context,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)tokenUsage,
                             (uint64_t)budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)nodeUsage,
                             (uint64_t)budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT((unsigned)retainedIndex, (unsigned)index);
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TREE_IDENT,
                          (int)ZrParser_TokenTree_Kind(
                                  ZrParser_TokenStream_At(stream, 0U)));
    assert_raw_text(stream, ZrParser_TokenStream_At(stream, 0U), "keep");
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Concat imports source/context ownership instead of borrowing either input. */
static void test_concat_imports_owned_sources_and_contexts(void) {
    static const TZrChar leftSource[] = "left";
    static const TZrChar rightSource[] = "right";
    static const TZrChar leftName[] = "left.zr";
    static const TZrChar rightName[] = "right.zr";
    SZrSyntaxContextTable *leftTable = create_context_table();
    SZrSyntaxContextTable *rightTable = create_context_table();
    SZrTokenStream *left = from_text_named(
            leftSource, sizeof(leftSource) - 1U, leftTable,
            leftName, sizeof(leftName) - 1U);
    SZrTokenStream *right = from_text_named(
            rightSource, sizeof(rightSource) - 1U, rightTable,
            rightName, sizeof(rightName) - 1U);
    SZrTokenStream *joined = ZR_NULL;
    const SZrTokenTree *leftTree;
    const SZrTokenTree *rightTree;
    SZrSyntaxContext joinedContext;
    TZrSize leftSourceLength = 0U;
    TZrSize rightSourceLength = 0U;
    TZrSize leftNameLength = 0U;
    TZrSize rightNameLength = 0U;
    const TZrChar *leftBytes;
    const TZrChar *rightBytes;
    const TZrChar *leftSourceName;
    const TZrChar *rightSourceName;

    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(
            left, right, ZR_NULL, &joined));
    TEST_ASSERT_NOT_NULL(joined);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(joined));
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)ZrParser_TokenStream_Count(joined));
    leftTree = ZrParser_TokenStream_At(joined, 0U);
    rightTree = ZrParser_TokenStream_At(joined, 1U);
    TEST_ASSERT_NOT_NULL(leftTree);
    TEST_ASSERT_NOT_NULL(rightTree);
    assert_raw_text(joined, leftTree, "left");
    assert_raw_text(joined, rightTree, "right");

    joinedContext = ZrParser_TokenTree_Context(leftTree);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(joinedContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(
            joinedContext, ZrParser_TokenTree_Context(rightTree)));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(
            joinedContext, ZrParser_SyntaxContext_TableRoot(
                    ZrParser_TokenStream_ContextTable(joined))));

    leftBytes = ZrParser_TokenStream_SourceBytes(joined,
                                                  ZrParser_TokenTree_Text(leftTree).sourceId,
                                                  &leftSourceLength);
    rightBytes = ZrParser_TokenStream_SourceBytes(joined,
                                                   ZrParser_TokenTree_Text(rightTree).sourceId,
                                                   &rightSourceLength);
    TEST_ASSERT_NOT_NULL(leftBytes);
    TEST_ASSERT_NOT_NULL(rightBytes);
    TEST_ASSERT_EQUAL_UINT64(sizeof(leftSource) - 1U,
                             (uint64_t)leftSourceLength);
    TEST_ASSERT_EQUAL_UINT64(sizeof(rightSource) - 1U,
                             (uint64_t)rightSourceLength);
    TEST_ASSERT_EQUAL_MEMORY(leftSource, leftBytes, leftSourceLength);
    TEST_ASSERT_EQUAL_MEMORY(rightSource, rightBytes, rightSourceLength);
    leftSourceName = ZrParser_TokenStream_SourceName(
            joined, ZrParser_TokenTree_Text(leftTree).sourceId,
            &leftNameLength);
    rightSourceName = ZrParser_TokenStream_SourceName(
            joined, ZrParser_TokenTree_Text(rightTree).sourceId,
            &rightNameLength);
    TEST_ASSERT_NOT_NULL(leftSourceName);
    TEST_ASSERT_NOT_NULL(rightSourceName);
    TEST_ASSERT_EQUAL_UINT64(sizeof(leftName) - 1U,
                             (uint64_t)leftNameLength);
    TEST_ASSERT_EQUAL_UINT64(sizeof(rightName) - 1U,
                             (uint64_t)rightNameLength);
    TEST_ASSERT_EQUAL_MEMORY(leftName, leftSourceName, leftNameLength);
    TEST_ASSERT_EQUAL_MEMORY(rightName, rightSourceName, rightNameLength);

    ZrParser_TokenStream_Release(left);
    ZrParser_TokenStream_Release(right);
    ZrParser_SyntaxContextTable_Release(leftTable);
    ZrParser_SyntaxContextTable_Release(rightTable);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_raw_text(joined, ZrParser_TokenStream_At(joined, 0U), "left");
    assert_raw_text(joined, ZrParser_TokenStream_At(joined, 1U), "right");
    ZrParser_TokenStream_Release(joined);
}

/* An early allocation failure reports its budget cause without consuming inputs. */
static void test_concat_reports_initial_heap_failure_without_mutating_inputs(void) {
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *left = from_text("left", 4U, table);
    SZrTokenStream *right = from_text("right", 5U, table);
    const SZrTokenTree *leftTree = ZrParser_TokenStream_At(left, 0U);
    const SZrTokenTree *rightTree = ZrParser_TokenStream_At(right, 0U);
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudget failedBudget;
    SZrParserComptimeBudgetUsage before;
    SZrTokenStream *joined = left;
    TZrBool rejected;
    TZrBool noOutput;
    TZrBool inputsUnchanged;
    TZrBool retried;
    TZrSize joinedCount;

    init_default_budget_limits(&limits);
    limits.heapBytes = 0U;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    before = budget.usage;
    rejected = !ZrParser_TokenStream_Concat(left, right, &budget, &joined);
    noOutput = joined == ZR_NULL;
    failedBudget = budget;
    inputsUnchanged = ZrParser_TokenStream_IsSealed(left) &&
                      ZrParser_TokenStream_IsSealed(right) &&
                      ZrParser_TokenStream_Count(left) == 1U &&
                      ZrParser_TokenStream_Count(right) == 1U &&
                      ZrParser_TokenStream_At(left, 0U) == leftTree &&
                      ZrParser_TokenStream_At(right, 0U) == rightTree;
    if (joined != left && joined != right) {
        ZrParser_TokenStream_Release(joined);
    }
    init_default_budget_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    retried = ZrParser_TokenStream_Concat(left, right, &budget, &joined);
    joinedCount = ZrParser_TokenStream_Count(joined);
    ZrParser_TokenStream_Release(joined);
    ZrParser_TokenStream_Release(left);
    ZrParser_TokenStream_Release(right);
    ZrParser_SyntaxContextTable_Release(table);

    TEST_ASSERT_TRUE(rejected);
    TEST_ASSERT_TRUE(noOutput);
    TEST_ASSERT_EQUAL_MEMORY(&before, &failedBudget.usage, sizeof(before));
    TEST_ASSERT_TRUE(inputsUnchanged);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                          failedBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(0U, failedBudget.exceededLimit);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, failedBudget.requestedUsage);
    TEST_ASSERT_TRUE(retried);
    TEST_ASSERT_EQUAL_UINT64(2U, joinedCount);
}

/* Stream arenas and context tables stay alive after caller storage is released and GC runs. */
static void test_stream_source_and_context_survive_release_and_gc(void) {
    static const TZrChar sourceName[] = "owned_source.zr";
    const TZrSize sourceLength = sizeof("owned text") - 1U;
    TZrChar *source = (TZrChar *)malloc(sourceLength + 1U);
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    const SZrTokenTree *tree;
    SZrTokenText raw;
    SZrSyntaxContext context;

    TEST_ASSERT_NOT_NULL(source);
    memcpy(source, "owned text", sourceLength + 1U);
    table = create_context_table();
    stream = from_text_named(source, sourceLength, table,
                             sourceName, sizeof(sourceName) - 1U);
    free(source);
    ZrParser_SyntaxContextTable_Release(table);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);

    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)ZrParser_TokenStream_Count(stream));
    tree = ZrParser_TokenStream_At(stream, 0U);
    TEST_ASSERT_NOT_NULL(tree);
    raw = ZrParser_TokenTree_Text(tree);
    assert_raw_text(stream, tree, "owned");
    context = ZrParser_TokenTree_Context(tree);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(context));
    TEST_ASSERT_NOT_NULL(ZrParser_TokenStream_SourceName(
            stream, raw.sourceId, ZR_NULL));
    TEST_ASSERT_EQUAL_MEMORY(sourceName,
                             ZrParser_TokenStream_SourceName(
                                     stream, raw.sourceId, ZR_NULL),
                             sizeof(sourceName) - 1U);
    ZrParser_TokenStream_Release(stream);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_from_text_empty_and_comments);
    RUN_TEST(test_comments_preserve_trivia_and_owned_source);
    RUN_TEST(test_from_text_preserves_string_and_numeric_spelling);
    RUN_TEST(test_from_text_rejects_mismatch_and_invalid_lexical_input);
    RUN_TEST(test_tiny_budget_preserves_preexisting_stream);
    RUN_TEST(test_concat_imports_owned_sources_and_contexts);
    RUN_TEST(test_stream_source_and_context_survive_release_and_gc);
    RUN_TEST(test_concat_reports_initial_heap_failure_without_mutating_inputs);
    return UNITY_END();
}
