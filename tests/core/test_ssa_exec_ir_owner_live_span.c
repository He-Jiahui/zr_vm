#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "support/ssa_exec_ir_owned_module_allocator.h"
#include "unity.h"

enum { LIVE_ROOTS = 4, LIVE_QUERIES = 32 };
typedef struct SLiveQuery {
    int result, expected;
    unsigned record, expectedRecord;
    uintptr_t allocation, expectedAllocation;
    size_t bytes, expectedBytes;
    SZrOwnerHeapStats before, after;
} SLiveQuery;
static void *g_roots[LIVE_ROOTS];
static SLiveQuery g_queries[LIVE_QUERIES];
static unsigned g_queryCount, g_setupErrors;
static int g_closed;
static SZrOwnerHeapStats g_original, g_recovered;
static unsigned char g_sentinel;

static void stats_equal(SZrOwnerHeapStats a, SZrOwnerHeapStats b) {
    TEST_ASSERT_EQUAL_UINT(a.attempts, b.attempts);
    TEST_ASSERT_EQUAL_UINT(a.records, b.records);
    TEST_ASSERT_EQUAL_UINT(a.live, b.live);
    TEST_ASSERT_EQUAL_UINT(a.frees, b.frees);
    TEST_ASSERT_EQUAL_UINT(a.errors, b.errors);
    TEST_ASSERT_EQUAL_UINT(a.failures, b.failures);
}
static void *allocate(unsigned slot, size_t count, size_t width, int callocRequest) {
    void *p = callocRequest ? ZrTests_ExecOwner_Calloc(count, width) : ZrTests_ExecOwner_Malloc(count * width);
    g_roots[slot] = p;
    if (p == NULL)
        ++g_setupErrors;
    return p;
}
/* All output pointer fields become integers while the queried owner is live. */
static void query(unsigned first, unsigned last, const void *pointer, size_t count, size_t width, int expected,
                  unsigned record, uintptr_t base, size_t bytes, int outputPresent) {
    SZrOwnerLiveSpan output = {713u, &g_sentinel, 991u};
    SLiveQuery *q;
    if (g_queryCount == LIVE_QUERIES) {
        ++g_setupErrors;
        return;
    }
    q = &g_queries[g_queryCount++];
    q->expected = expected;
    q->expectedRecord = expected ? record : 713u;
    q->expectedAllocation = expected ? base : (uintptr_t) &g_sentinel;
    q->expectedBytes = expected ? bytes : 991u;
    q->before = ZrTests_ExecOwner_Stats();
    q->result = ZrTests_ExecOwner_TryLiveSpan(first, last, pointer, count, width, outputPresent ? &output : NULL);
    q->after = ZrTests_ExecOwner_Stats();
    q->record = output.record;
    q->allocation = (uintptr_t) output.allocation;
    q->bytes = output.bytes;
}
static void release(unsigned slot) {
    void *p = g_roots[slot];
    g_roots[slot] = NULL;
    ZrTests_ExecOwner_Free(p);
}
static void close_owners(void) {
    if (g_closed)
        return;
    ZrTests_ExecOwner_Disarm();
    for (unsigned i = 0u; i < LIVE_ROOTS; ++i)
        release(i);
    g_original = ZrTests_ExecOwner_Stats();
    ZrTests_ExecOwner_Recover();
    g_recovered = ZrTests_ExecOwner_Stats();
    g_closed = 1;
}
static void verify_health(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_setupErrors);
    TEST_ASSERT_EQUAL_UINT(0u, g_original.errors);
    TEST_ASSERT_EQUAL_UINT(0u, g_original.live);
    TEST_ASSERT_EQUAL_UINT(g_original.records, g_original.frees);
    stats_equal(g_original, g_recovered);
    ZrTests_ExecOwner_AssertBalanced();
}
static void finish(void) {
    close_owners();
    verify_health();
    /* Check every purity/refusal observation before feature credit. */
    for (unsigned i = 0u; i < g_queryCount; ++i) {
        SLiveQuery *q = &g_queries[i];
        stats_equal(q->before, q->after);
        if (!q->result) {
            TEST_ASSERT_EQUAL_UINT(713u, q->record);
            TEST_ASSERT_EQUAL_UINT64((uint64_t) (uintptr_t) &g_sentinel, (uint64_t) q->allocation);
            TEST_ASSERT_EQUAL_UINT64(991u, (uint64_t) q->bytes);
        }
        if (!q->expected)
            TEST_ASSERT_EQUAL_INT(0, q->result);
    }
    for (unsigned i = 0u; i < g_queryCount; ++i) {
        SLiveQuery *q = &g_queries[i];
        if (q->expected) {
            if (!q->result)
                printf("OWNER_LIVE_SPAN_FEATURE query=%u attempts_delta=%u frees_delta=%u\n", i,
                       q->after.attempts - q->before.attempts, q->after.frees - q->before.frees);
            TEST_ASSERT_TRUE_MESSAGE(q->result, "OWNER_LIVE_SPAN_FEATURE actual live span query must succeed");
            TEST_ASSERT_EQUAL_INT(1, q->result);
        } else {
            TEST_ASSERT_EQUAL_INT(0, q->result);
        }
        TEST_ASSERT_EQUAL_UINT(q->expectedRecord, q->record);
        TEST_ASSERT_EQUAL_UINT64((uint64_t) q->expectedAllocation, (uint64_t) q->allocation);
        TEST_ASSERT_EQUAL_UINT64((uint64_t) q->expectedBytes, (uint64_t) q->bytes);
    }
}
void setUp(void) {
    memset(g_roots, 0, sizeof(g_roots));
    memset(g_queries, 0, sizeof(g_queries));
    g_queryCount = g_setupErrors = 0u;
    g_closed = 0;
    /* Reset asserts only here, before any resource is acquired. */
    ZrTests_ExecOwner_Reset();
}
void tearDown(void) {
    close_owners();
    verify_health();
}

static void test_malloc_exact_live_span_reports_record_allocation_and_bytes(void) {
    void *p = allocate(0u, 1u, 32u, 0);
    query(0u, 1u, p, 32u, 1u, 1, 0u, (uintptr_t) p, 32u, 1);
    finish();
}
static void test_calloc_readable_subspans_include_last_byte(void) {
    unsigned char *p = allocate(0u, 4u, 8u, 1);
    if (p != NULL) {
        query(0u, 1u, p + 8u, 2u, 8u, 1, 0u, (uintptr_t) p, 32u, 1);
        query(0u, 1u, p + 31u, 1u, 1u, 1, 0u, (uintptr_t) p, 32u, 1);
        query(0u, 1u, p + 31u, 2u, 1u, 0, 0u, 0u, 0u, 1);
        query(0u, 1u, p + 32u, 1u, 1u, 0, 0u, 0u, 0u, 1);
    }
    finish();
}
static void test_realloc_null_creates_real_live_record(void) {
    void *p = ZrTests_ExecOwner_Realloc(NULL, 24u);
    g_roots[0] = p;
    if (p == NULL)
        ++g_setupErrors;
    query(0u, 1u, p, 3u, 8u, 1, 0u, (uintptr_t) p, 24u, 1);
    finish();
}
static void test_successful_realloc_uses_current_pointer_and_size(void) {
    void *p = allocate(0u, 1u, 16u, 0);
    if (p != NULL) {
        void *next = ZrTests_ExecOwner_Realloc(p, 48u);
        if (next != NULL) {
            g_roots[0] = next;
            p = next;
            query(0u, 1u, p, 48u, 1u, 1, 0u, (uintptr_t) p, 48u, 1);
            next = ZrTests_ExecOwner_Realloc(p, 8u);
            if (next != NULL) {
                g_roots[0] = next;
                p = next;
                query(0u, 1u, p, 8u, 1u, 1, 0u, (uintptr_t) p, 8u, 1);
                query(0u, 1u, p, 9u, 1u, 0, 0u, 0u, 0u, 1);
            } else {
                ++g_setupErrors;
            }
        } else {
            ++g_setupErrors;
        }
    }
    finish();
}
static void test_released_record_is_not_a_live_span(void) {
    void *p = allocate(0u, 1u, 16u, 0);
    uintptr_t address = (uintptr_t) p;
    query(0u, 1u, p, 16u, 1u, 1, 0u, address, 16u, 1);
    release(0u);
    /* Only the previously saved integer is used after the free. */
    query(0u, 1u, (const void *) address, 1u, 1u, 0, 0u, 0u, 0u, 1);
    finish();
}
static void test_valid_half_open_windows_select_exact_record(void) {
    void *a = allocate(0u, 1u, 16u, 0);
    void *b = allocate(1u, 1u, 24u, 0);
    query(0u, 1u, a, 1u, 1u, 1, 0u, (uintptr_t) a, 16u, 1);
    query(1u, 2u, b, 1u, 1u, 1, 1u, (uintptr_t) b, 24u, 1);
    query(0u, 2u, b, 3u, 8u, 1, 1u, (uintptr_t) b, 24u, 1);
    query(0u, 1u, b, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(1u, 2u, a, 1u, 1u, 0, 0u, 0u, 0u, 1);
    finish();
}
static void test_empty_reversed_and_out_of_inventory_windows_preserve_output(void) {
    void *p = allocate(0u, 1u, 16u, 0);
    query(0u, 0u, p, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(1u, 1u, p, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(1u, 0u, p, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(0u, 2u, p, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(UINT32_MAX, UINT32_MAX, p, 1u, 1u, 0, 0u, 0u, 0u, 1);
    finish();
}
static void test_null_and_zero_query_arguments_preserve_output(void) {
    void *p = allocate(0u, 1u, 16u, 0);
    query(0u, 1u, p, 1u, 1u, 0, 0u, 0u, 0u, 0);
    query(0u, 1u, NULL, 1u, 1u, 0, 0u, 0u, 0u, 1);
    query(0u, 1u, p, 0u, 1u, 0, 0u, 0u, 0u, 1);
    query(0u, 1u, p, 1u, 0u, 0, 0u, 0u, 0u, 1);
    finish();
}
static void test_multiplication_and_address_overflow_preserve_output(void) {
    void *p = allocate(0u, 1u, 16u, 0);
    query(0u, 1u, p, SIZE_MAX, 2u, 0, 0u, 0u, 0u, 1);
    query(0u, 1u, (const void *) (uintptr_t) (UINTPTR_MAX - 1u), 4u, 1u, 0, 0u, 0u, 0u, 1);
    finish();
}
static void test_readable_foreign_and_outside_spans_preserve_output(void) {
    unsigned char foreign[8] = {0};
    void *p = allocate(0u, 1u, 16u, 0);
    uintptr_t address = (uintptr_t) p;
    query(0u, 1u, foreign, sizeof(foreign), 1u, 0, 0u, 0u, 0u, 1);
    if (address != 0u && address <= UINTPTR_MAX - 16u) {
        query(0u, 1u, (const void *) (address - 1u), 1u, 1u, 0, 0u, 0u, 0u, 1);
        query(0u, 1u, (const void *) (address + 16u), 1u, 1u, 0, 0u, 0u, 0u, 1);
    }
    finish();
}
static void test_independent_live_allocations_are_not_cross_matched(void) {
    unsigned char *a = allocate(0u, 1u, 24u, 0);
    unsigned char *b = allocate(1u, 3u, 8u, 1);
    if (a != NULL && b != NULL) {
        query(0u, 2u, a + 4u, 2u, 8u, 1, 0u, (uintptr_t) a, 24u, 1);
        query(0u, 2u, b + 4u, 2u, 8u, 1, 1u, (uintptr_t) b, 24u, 1);
        query(0u, 1u, b + 4u, 1u, 1u, 0, 0u, 0u, 0u, 1);
        query(1u, 2u, a + 4u, 1u, 1u, 0, 0u, 0u, 0u, 1);
    }
    finish();
}
static void test_armed_queries_preserve_stats_events_and_pending_fault(void) {
    /* Arm asserts while no resource is live. Queries must not consume attempt 3. */
    ZrTests_ExecOwner_Arm(3u);
    void *a = allocate(0u, 1u, 16u, 0);
    void *b = allocate(1u, 2u, 8u, 1);
    query(0u, 2u, a, 16u, 1u, 1, 0u, (uintptr_t) a, 16u, 1);
    query(0u, 2u, b, 16u, 1u, 1, 1u, (uintptr_t) b, 16u, 1);
    query(0u, 1u, b, 1u, 1u, 0, 0u, 0u, 0u, 1);
    void *fault = ZrTests_ExecOwner_Malloc(8u);
    g_roots[2] = fault;
    int failed = fault == NULL;
    SZrOwnerHeapStats beforeCleanup = ZrTests_ExecOwner_Stats();
    close_owners();
    verify_health();
    TEST_ASSERT_TRUE(failed);
    TEST_ASSERT_EQUAL_UINT(3u, beforeCleanup.attempts);
    TEST_ASSERT_EQUAL_UINT(2u, beforeCleanup.records);
    TEST_ASSERT_EQUAL_UINT(1u, beforeCleanup.failures);
    SZrOwnerHeapEvent e0 = ZrTests_ExecOwner_Event(0u);
    SZrOwnerHeapEvent e1 = ZrTests_ExecOwner_Event(1u);
    SZrOwnerHeapEvent e2 = ZrTests_ExecOwner_Event(2u);
    TEST_ASSERT_EQUAL_UINT(1u, e0.kind);
    TEST_ASSERT_EQUAL_UINT(0u, e0.record);
    TEST_ASSERT_EQUAL_UINT(0u, e0.failed);
    TEST_ASSERT_EQUAL_UINT64(1u, e0.count);
    TEST_ASSERT_EQUAL_UINT64(16u, e0.width);
    TEST_ASSERT_EQUAL_UINT64(16u, e0.bytes);
    TEST_ASSERT_EQUAL_UINT(2u, e1.kind);
    TEST_ASSERT_EQUAL_UINT(1u, e1.record);
    TEST_ASSERT_EQUAL_UINT(0u, e1.failed);
    TEST_ASSERT_EQUAL_UINT64(2u, e1.count);
    TEST_ASSERT_EQUAL_UINT64(8u, e1.width);
    TEST_ASSERT_EQUAL_UINT64(16u, e1.bytes);
    TEST_ASSERT_EQUAL_UINT(1u, e2.kind);
    TEST_ASSERT_EQUAL_UINT(32768u, e2.record);
    TEST_ASSERT_EQUAL_UINT(1u, e2.failed);
    TEST_ASSERT_EQUAL_UINT64(1u, e2.count);
    TEST_ASSERT_EQUAL_UINT64(8u, e2.width);
    TEST_ASSERT_EQUAL_UINT64(8u, e2.bytes);
    finish();
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_malloc_exact_live_span_reports_record_allocation_and_bytes);
    RUN_TEST(test_calloc_readable_subspans_include_last_byte);
    RUN_TEST(test_realloc_null_creates_real_live_record);
    RUN_TEST(test_successful_realloc_uses_current_pointer_and_size);
    RUN_TEST(test_released_record_is_not_a_live_span);
    RUN_TEST(test_valid_half_open_windows_select_exact_record);
    RUN_TEST(test_empty_reversed_and_out_of_inventory_windows_preserve_output);
    RUN_TEST(test_null_and_zero_query_arguments_preserve_output);
    RUN_TEST(test_multiplication_and_address_overflow_preserve_output);
    RUN_TEST(test_readable_foreign_and_outside_spans_preserve_output);
    RUN_TEST(test_independent_live_allocations_are_not_cross_matched);
    RUN_TEST(test_armed_queries_preserve_stats_events_and_pending_fault);
    return UNITY_END();
}
