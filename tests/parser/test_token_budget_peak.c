#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "runtime_support.h"
#include "syntax/token_stream_internal.h"

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

/* A context transaction's reservation must be visible to native stream
 * allocations; otherwise the batch can exceed its heap limit before Prepare. */
static void test_batch_rejects_peak_heap_overcommit(void) {
    const TZrSize contextReservation = 64u;
    const TZrSize nativeRequest = 64u;
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrTokenStreamBatch batch;
    TZrUInt64 initialHeap;
    void *memory;

    init_default_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = ZrParser_SyntaxContextTable_Create(g_state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    initialHeap = budget.usage.heapBytes;

    /* Leave room for the reservation, but not for both allocations together. */
    budget.limits.heapBytes = initialHeap + contextReservation + nativeRequest - 1u;

    token_stream_batch_begin(stream, &batch);
    TEST_ASSERT_TRUE(batch.ownsContextBatch);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &batch.contextBatch.transaction,
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            contextReservation));

    memory = token_stream_alloc(
            stream, nativeRequest, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (memory != ZR_NULL) {
        token_stream_free(
                stream, memory, nativeRequest, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    TEST_ASSERT_FALSE(token_stream_batch_finish(&batch, ZR_FALSE));

    TEST_ASSERT_NULL(memory);
    TEST_ASSERT_EQUAL_UINT64(initialHeap, budget.usage.heapBytes);

    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* A caller-owned context transaction must also participate in the stream
 * batch's peak check.  The stream batch cannot safely publish native storage
 * while an enclosing context transaction still has a pending reservation. */
static void test_nested_context_reservation_is_visible_to_stream(void) {
    const TZrSize contextReservation = 64u;
    const TZrSize nativeRequest = 64u;
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrSyntaxContextBatch contextBatch;
    SZrTokenStreamBatch batch;
    TZrUInt64 initialHeap;
    void *memory;

    init_default_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = ZrParser_SyntaxContextTable_Create(g_state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    initialHeap = budget.usage.heapBytes;
    budget.limits.heapBytes = initialHeap + contextReservation + nativeRequest - 1u;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_BeginBatch(table, &contextBatch));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &contextBatch.transaction,
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            contextReservation));

    token_stream_batch_begin(stream, &batch);
    TEST_ASSERT_FALSE(batch.ownsContextBatch);
    memory = token_stream_alloc(
            stream, nativeRequest, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (memory != ZR_NULL) {
        token_stream_free(
                stream, memory, nativeRequest, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    TEST_ASSERT_FALSE(token_stream_batch_finish(&batch, ZR_FALSE));
    TEST_ASSERT_NULL(memory);

    ZrParser_SyntaxContext_AbortBatch(&contextBatch);
    TEST_ASSERT_EQUAL_UINT64(initialHeap, budget.usage.heapBytes);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_public_append_rejects_external_context_batch(void) {
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrSyntaxContextBatch contextBatch;
    SZrTokenNativeSemInfo semInfo;
    SZrTokenText text;
    SZrTokenSpan span;
    SZrSyntaxContext context;
    TZrTokenSourceId sourceId;
    TZrSize initialCount;
    TZrSize outIndex = 123u;
    TZrUInt64 initialHeap;

    init_default_limits(&limits);
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = ZrParser_SyntaxContextTable_Create(g_state->global, &budget);
    TEST_ASSERT_NOT_NULL(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, &budget);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "test", 4u, "x", 1u, &sourceId));
    text.sourceId = sourceId;
    text.offset = 0u;
    text.length = 1u;
    span.sourceId = sourceId;
    span.startOffset = 0u;
    span.endOffset = 1u;
    span.startLine = 1;
    span.startColumn = 1;
    span.endLine = 1;
    span.endColumn = 2;
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = text;
    context = ZrParser_SyntaxContext_TableRoot(table);
    initialCount = ZrParser_TokenStream_Count(stream);
    initialHeap = budget.usage.heapBytes;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_BeginBatch(table, &contextBatch));
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, text, span, context, ZR_NULL, 0u,
            ZR_NULL, 0u, &outIndex));
    TEST_ASSERT_EQUAL_UINT64(initialCount, ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_UINT64(initialHeap, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(123u, outIndex);
    TEST_ASSERT_FALSE(ZrParser_TokenStream_AppendGroup(
            stream, ZR_TOKEN_DELIMITER_NONE,
            ZrParser_TokenSpan_None(), ZrParser_TokenSpan_None(),
            ZR_NULL, 0u, span, context, ZR_NULL, 0u, ZR_NULL, 0u,
            &outIndex));
    TEST_ASSERT_EQUAL_UINT64(initialCount, ZrParser_TokenStream_Count(stream));
    TEST_ASSERT_EQUAL_UINT64(initialHeap, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(123u, outIndex);

    ZrParser_SyntaxContext_AbortBatch(&contextBatch);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_batch_rejects_peak_heap_overcommit);
    RUN_TEST(test_nested_context_reservation_is_visible_to_stream);
    RUN_TEST(test_public_append_rejects_external_context_batch);
    return UNITY_END();
}
