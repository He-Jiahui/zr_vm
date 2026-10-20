/* RED coverage for syntax-context/table and token-stream budget ledgers. */

#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "runtime_support.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"
#include "syntax/syntax_context_internal.h"

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

static void init_default_limits(SZrParserComptimeBudgetLimits *limits) {
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

static SZrTokenSpan make_span(
        TZrTokenSourceId sourceId, TZrSize start, TZrSize end) {
    SZrTokenSpan span;

    memset(&span, 0, sizeof(span));
    span.sourceId = sourceId;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = 1;
    span.startColumn = (TZrInt32)start + 1;
    span.endLine = 1;
    span.endColumn = (TZrInt32)end + 1;
    return span;
}

static TZrSyntaxExpansionId register_expansion(
        SZrSyntaxContextTable *table, TZrUInt64 stableIdentity) {
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId expansionId = 0u;

    memset(&info, 0, sizeof(info));
    info.stableIdentity = stableIdentity;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, &expansionId));
    return expansionId;
}

/* A sealed source stream owns a nonroot marked child in a distinct table. */
static SZrTokenStream *make_marked_source(
        SZrSyntaxContextTable **outTable) {
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrSyntaxContext marked;
    SZrTokenNativeSemInfo semInfo;
    SZrTokenText text;
    SZrTokenSpan span;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    TZrSyntaxExpansionId expansionId;
    TZrSize index = 0u;

    TEST_ASSERT_NOT_NULL(outTable);
    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    expansionId = register_expansion(table, UINT64_C(0x9001));
    marked = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(table), expansionId,
            ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(marked));
    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "source", 6u, "x", 1u, &sourceId));
    text.sourceId = sourceId;
    text.offset = 0u;
    text.length = 1u;
    span = make_span(sourceId, 0u, 1u);
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = text;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, text, span, marked,
            ZR_NULL, 0u, ZR_NULL, 0u, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    TEST_ASSERT_EQUAL_UINT(1u, (unsigned)ZrParser_TokenStream_Count(stream));
    *outTable = table;
    return stream;
}

static void assert_usage_equal(
        const SZrParserComptimeBudgetUsage *expected,
        const SZrParserComptimeBudgetUsage *actual) {
    TEST_ASSERT_EQUAL_MEMORY(expected, actual, sizeof(*expected));
}

static void test_cross_stream_import_charges_both_ledgers(void) {
    SZrSyntaxContextTable *sourceTable;
    SZrTokenStream *sourceStream;
    SZrSyntaxContextTable *destinationTable;
    SZrTokenStream *destinationStream;
    SZrParserComptimeBudget streamBudget;
    SZrParserComptimeBudgetLimits streamLimits;
    SZrSyntaxContext destinationRoot;
    SZrTokenSpan destinationSpan;
    TZrTokenSourceId destinationSourceId = ZR_TOKEN_SOURCE_INVALID;
    const SZrTokenTree *children[1];
    TZrSize childIndex = 0u;
    TZrSize outIndex = 777u;
    SZrParserComptimeBudgetUsage tableBefore;
    SZrParserComptimeBudgetUsage streamBefore;

    sourceStream = make_marked_source(&sourceTable);
    children[0] = ZrParser_TokenStream_At(sourceStream, childIndex);
    TEST_ASSERT_NOT_NULL(children[0]);

    destinationTable = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(destinationTable);
    /* Establish pre-existing fallback usage before the destination stream. */
    (void)register_expansion(destinationTable, UINT64_C(0xA001));
    tableBefore = destinationTable->fallbackBudget.usage;
    TEST_ASSERT_TRUE(tableBefore.heapBytes > 0u);
    TEST_ASSERT_TRUE(tableBefore.expansionCount > 0u);

    init_default_limits(&streamLimits);
    ZrParser_ComptimeBudget_Init(&streamBudget, &streamLimits);
    destinationStream = ZrParser_TokenStream_Create(
            g_state->global, destinationTable, &streamBudget);
    TEST_ASSERT_NOT_NULL(destinationStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            destinationStream, "destination", 11u, "y", 1u,
            &destinationSourceId));
    streamBefore = streamBudget.usage;
    TEST_ASSERT_TRUE(streamBefore.heapBytes > 0u);
    destinationRoot = ZrParser_SyntaxContext_TableRoot(destinationTable);
    destinationSpan = make_span(destinationSourceId, 0u, 1u);

    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
            destinationStream, ZR_TOKEN_DELIMITER_NONE,
            ZrParser_TokenSpan_None(), ZrParser_TokenSpan_None(),
            children, 1u, destinationSpan, destinationRoot,
            ZR_NULL, 0u, ZR_NULL, 0u, &outIndex));
    TEST_ASSERT_NOT_EQUAL(777u, (unsigned)outIndex);
    /* Context metadata is admitted against its saved fallback ledger. */
    TEST_ASSERT_TRUE(destinationTable->fallbackBudget.usage.heapBytes >
                     tableBefore.heapBytes);
    TEST_ASSERT_TRUE(destinationTable->fallbackBudget.usage.expansionCount >
                     tableBefore.expansionCount);
    TEST_ASSERT_TRUE(destinationTable->markCount > 0u);
    /* The stream ledger still records the complete native + context operation. */
    TEST_ASSERT_TRUE(streamBudget.usage.heapBytes > streamBefore.heapBytes);
    TEST_ASSERT_TRUE(streamBudget.usage.nodeCount > streamBefore.nodeCount);
    TEST_ASSERT_TRUE(streamBudget.usage.tokenCount > streamBefore.tokenCount);
    TEST_ASSERT_TRUE(streamBudget.usage.expansionCount >
                     streamBefore.expansionCount);

    ZrParser_TokenStream_Release(destinationStream);
    ZrParser_SyntaxContextTable_Release(destinationTable);
    ZrParser_TokenStream_Release(sourceStream);
    ZrParser_SyntaxContextTable_Release(sourceTable);
}

static void test_explicit_table_limit_rejects_stream_import_atomically(void) {
    SZrParserComptimeBudget contextBudget;
    SZrParserComptimeBudget streamBudget;
    SZrParserComptimeBudgetLimits contextLimits;
    SZrParserComptimeBudgetLimits streamLimits;
    SZrSyntaxContextTable *sourceTable;
    SZrTokenStream *sourceStream;
    SZrSyntaxContextTable *destinationTable;
    SZrTokenStream *destinationStream;
    SZrSyntaxContext destinationRoot;
    SZrTokenSpan destinationSpan;
    TZrTokenSourceId destinationSourceId = ZR_TOKEN_SOURCE_INVALID;
    const SZrTokenTree *children[1];
    TZrSize childIndex = 0u;
    TZrSize outIndex = 991u;
    TZrSize initialStreamCount;
    TZrSize initialExpansionCount;
    TZrSize initialExpansionCapacity;
    TZrSize initialMarkCount;
    TZrSize initialMarkCapacity;
    SZrParserComptimeBudgetUsage contextBefore;
    SZrParserComptimeBudgetUsage streamBefore;

    init_default_limits(&contextLimits);
    ZrParser_ComptimeBudget_Init(&contextBudget, &contextLimits);
    destinationTable = ZrParser_SyntaxContextTable_Create(
            g_state->global, &contextBudget);
    TEST_ASSERT_NOT_NULL(destinationTable);
    (void)register_expansion(destinationTable, UINT64_C(0xA101));
    /* No metadata beyond this point is admitted by the table ledger. */
    contextBudget.limits.heapBytes = contextBudget.usage.heapBytes;
    contextBudget.limits.expansionCount = contextBudget.usage.expansionCount;
    contextBudget.limits.nodeCount = contextBudget.usage.nodeCount;
    contextBefore = contextBudget.usage;

    init_default_limits(&streamLimits);
    ZrParser_ComptimeBudget_Init(&streamBudget, &streamLimits);
    destinationStream = ZrParser_TokenStream_Create(
            g_state->global, destinationTable, &streamBudget);
    TEST_ASSERT_NOT_NULL(destinationStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            destinationStream, "destination", 11u, "y", 1u,
            &destinationSourceId));
    streamBefore = streamBudget.usage;
    initialStreamCount = ZrParser_TokenStream_Count(destinationStream);
    initialExpansionCount = destinationTable->expansionCount;
    initialExpansionCapacity = destinationTable->expansionCapacity;
    initialMarkCount = destinationTable->markCount;
    initialMarkCapacity = destinationTable->markCapacity;

    sourceStream = make_marked_source(&sourceTable);
    children[0] = ZrParser_TokenStream_At(sourceStream, childIndex);
    TEST_ASSERT_NOT_NULL(children[0]);
    destinationRoot = ZrParser_SyntaxContext_TableRoot(destinationTable);
    destinationSpan = make_span(destinationSourceId, 0u, 1u);

    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendGroup(
            destinationStream, ZR_TOKEN_DELIMITER_NONE,
            ZrParser_TokenSpan_None(), ZrParser_TokenSpan_None(),
            children, 1u, destinationSpan, destinationRoot,
            ZR_NULL, 0u, ZR_NULL, 0u, &outIndex));
    TEST_ASSERT_EQUAL_UINT(991u, (unsigned)outIndex);
    TEST_ASSERT_EQUAL_UINT64(
            initialStreamCount, ZrParser_TokenStream_Count(destinationStream));
    TEST_ASSERT_EQUAL_UINT64(initialExpansionCount,
                             destinationTable->expansionCount);
    TEST_ASSERT_EQUAL_UINT64(initialExpansionCapacity,
                             destinationTable->expansionCapacity);
    TEST_ASSERT_EQUAL_UINT64(initialMarkCount, destinationTable->markCount);
    TEST_ASSERT_EQUAL_UINT64(initialMarkCapacity, destinationTable->markCapacity);
    assert_usage_equal(&contextBefore, &contextBudget.usage);
    assert_usage_equal(&streamBefore, &streamBudget.usage);

    ZrParser_TokenStream_Release(sourceStream);
    ZrParser_SyntaxContextTable_Release(sourceTable);
    ZrParser_TokenStream_Release(destinationStream);
    ZrParser_SyntaxContextTable_Release(destinationTable);
}

static void test_shared_budget_charges_import_once(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudgetUsage before;
    SZrSyntaxContextTable *sourceTable;
    SZrTokenStream *sourceStream;
    SZrSyntaxContextTable *destinationTable;
    SZrTokenStream *destinationStream;
    const SZrTokenTree *children[1];
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    TZrSize outIndex = 0u;

    sourceStream = make_marked_source(&sourceTable);
    children[0] = ZrParser_TokenStream_At(sourceStream, 0u);
    init_default_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    destinationTable = ZrParser_SyntaxContextTable_Create(
            g_state->global, &budget);
    TEST_ASSERT_NOT_NULL(destinationTable);
    destinationStream = ZrParser_TokenStream_Create(
            g_state->global, destinationTable, &budget);
    TEST_ASSERT_NOT_NULL(destinationStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            destinationStream, "destination", 11u, "y", 1u, &sourceId));
    before = budget.usage;
    /* One imported mark, its identifier, and the enclosing group. */
    budget.limits.nodeCount = before.nodeCount + 3u;
    budget.limits.expansionCount = before.expansionCount + 1u;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
            destinationStream, ZR_TOKEN_DELIMITER_NONE,
            ZrParser_TokenSpan_None(), ZrParser_TokenSpan_None(),
            children, 1u, make_span(sourceId, 0u, 1u),
            ZrParser_SyntaxContext_TableRoot(destinationTable),
            ZR_NULL, 0u, ZR_NULL, 0u, &outIndex));
    TEST_ASSERT_EQUAL_UINT64(before.nodeCount + 3u, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(before.expansionCount + 1u,
                             budget.usage.expansionCount);
    TEST_ASSERT_EQUAL_UINT64(before.tokenCount + 1u, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_UINT64(1u, destinationTable->markCount);
    TEST_ASSERT_EQUAL_UINT64(1u, destinationTable->expansionCount);
    TEST_ASSERT_EQUAL_PTR(&budget, destinationTable->budget);

    ZrParser_TokenStream_Release(destinationStream);
    ZrParser_SyntaxContextTable_Release(destinationTable);
    ZrParser_TokenStream_Release(sourceStream);
    ZrParser_SyntaxContextTable_Release(sourceTable);
}

static void test_concat_owned_table_detaches_caller_budget(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudgetUsage callerUsage;
    SZrSyntaxContextTable *sourceTable;
    SZrSyntaxContextTable *resultTable;
    SZrTokenStream *sourceStream;
    SZrTokenStream *result = ZR_NULL;

    sourceStream = make_marked_source(&sourceTable);
    init_default_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(
            sourceStream, sourceStream, &budget, &result));
    TEST_ASSERT_NOT_NULL(result);
    resultTable = ZrParser_TokenStream_ContextTable(result);
    TEST_ASSERT_NOT_NULL(resultTable);
    TEST_ASSERT_NULL(resultTable->budget);
    TEST_ASSERT_TRUE(resultTable->fallbackBudget.usage.heapBytes > 0u);
    TEST_ASSERT_TRUE(resultTable->fallbackBudget.usage.expansionCount > 0u);
    TEST_ASSERT_TRUE(resultTable->fallbackBudget.usage.nodeCount > 0u);
    callerUsage = budget.usage;
    budget.limits.expansionCount = callerUsage.expansionCount;
    (void)register_expansion(resultTable, UINT64_C(0xC001));
    assert_usage_equal(&callerUsage, &budget.usage);

    ZrParser_TokenStream_Release(result);
    ZrParser_TokenStream_Release(sourceStream);
    ZrParser_SyntaxContextTable_Release(sourceTable);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cross_stream_import_charges_both_ledgers);
    RUN_TEST(test_explicit_table_limit_rejects_stream_import_atomically);
    RUN_TEST(test_shared_budget_charges_import_once);
    RUN_TEST(test_concat_owned_table_detaches_caller_budget);
    return UNITY_END();
}
