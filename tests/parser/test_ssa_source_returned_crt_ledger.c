#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "support/ssa_source_returned_crt_ledger.h"
#include "zr_vm_core/aot_ir.h"
#include "unity.h"

enum { TEST_ROOTS = 257, TEST_READS = 1024, TEST_STATS = 32, TEST_EVENTS = 512 };
typedef struct SRoot { void *pointer; unsigned kind; } SRoot;
typedef struct SPurity { SZrReturnedHeapStats before[2], after[2]; } SPurity;
typedef struct SStatsRead {
    int result, expectedResult;
    SZrReturnedHeapStats actual, expected;
} SStatsRead;
typedef struct SUintRead { int result, expectedResult; unsigned actual, expected; } SUintRead;
typedef struct SEventRead {
    int result, expectedResult;
    SZrReturnedHeapEvent actual, expected;
} SEventRead;
static SRoot g_roots[TEST_ROOTS];
static SPurity g_purity[TEST_READS];
static SStatsRead g_statsReads[TEST_STATS];
static SUintRead g_uintReads[TEST_READS];
static SEventRead g_eventReads[TEST_EVENTS];
static unsigned g_purityCount, g_statsCount, g_uintCount, g_eventCount;
static unsigned g_setupErrors, g_expectedErrors[2], g_zeroErrors;
static int g_closed;
static SZrReturnedHeapStats g_original[2], g_recovered[2], g_saved[8];
static const size_t g_counts[6] = {3u, 1u, 1u, 3u, 1u, 1u};
static const size_t g_widths[6] = {
    sizeof(SZrAotIrInstruction), sizeof(SZrAotIrBlock), sizeof(SZrAotIrFrameSlot),
    sizeof(SZrAotIrSourceMap), sizeof(SZrExecIrConstant), sizeof(SZrExecIrLayout)
};

static SZrReturnedHeapStats stats(unsigned attempts, unsigned successes, unsigned frees,
                                 unsigned live, unsigned errors, unsigned failures) {
    SZrReturnedHeapStats value = {attempts, successes, frees, live, errors, failures};
    return value;
}
static SZrReturnedHeapEvent event(size_t count, size_t width, unsigned site,
                                 unsigned record, TZrBool injected) {
    SZrReturnedHeapEvent value = {count, width, count * width, site, record, injected};
    return value;
}
static void stats_equal(SZrReturnedHeapStats a, SZrReturnedHeapStats b) {
    TEST_ASSERT_EQUAL_UINT(a.attempts, b.attempts);
    TEST_ASSERT_EQUAL_UINT(a.successes, b.successes);
    TEST_ASSERT_EQUAL_UINT(a.frees, b.frees);
    TEST_ASSERT_EQUAL_UINT(a.live, b.live);
    TEST_ASSERT_EQUAL_UINT(a.errors, b.errors);
    TEST_ASSERT_EQUAL_UINT(a.failures, b.failures);
}
static void event_equal(SZrReturnedHeapEvent a, SZrReturnedHeapEvent b) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)a.count, (uint64_t)b.count);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)a.width, (uint64_t)b.width);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)a.bytes, (uint64_t)b.bytes);
    TEST_ASSERT_EQUAL_UINT(a.site, b.site);
    TEST_ASSERT_EQUAL_UINT(a.record, b.record);
    TEST_ASSERT_EQUAL_UINT(a.injected, b.injected);
}
/* These internal reads supply purity and health only, never feature credit. */
static SPurity *before_read(void) {
    if (g_purityCount == TEST_READS) { ++g_setupErrors; return NULL; }
    SPurity *p = &g_purity[g_purityCount++];
    p->before[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    p->before[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    return p;
}
static void after_read(SPurity *p) {
    if (p == NULL) return;
    p->after[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    p->after[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
}
static void read_stats(unsigned kind, int expectedResult, SZrReturnedHeapStats expected, int outputPresent) {
    SZrReturnedHeapStats output = {711u, 712u, 713u, 714u, 715u, 716u};
    if (g_statsCount == TEST_STATS) { ++g_setupErrors; return; }
    SStatsRead *q = &g_statsReads[g_statsCount++];
    SPurity *p = before_read();
    q->result = ZrTests_Returned_TryStats(kind, outputPresent ? &output : NULL);
    after_read(p);
    q->actual = output; q->expectedResult = expectedResult;
    q->expected = expectedResult ? expected : stats(711u, 712u, 713u, 714u, 715u, 716u);
}
static void read_frees(unsigned kind, unsigned record, int expectedResult, unsigned expected, int outputPresent) {
    unsigned output = 719u;
    if (g_uintCount == TEST_READS) { ++g_setupErrors; return; }
    SUintRead *q = &g_uintReads[g_uintCount++]; SPurity *p = before_read();
    q->result = ZrTests_Returned_TryRecordFrees(kind, record, outputPresent ? &output : NULL);
    after_read(p);
    q->actual = output; q->expectedResult = expectedResult; q->expected = expectedResult ? expected : 719u;
}
static void read_live(unsigned kind, const void *pointer, int expectedResult, unsigned expected, int outputPresent) {
    unsigned output = 719u;
    if (g_uintCount == TEST_READS) { ++g_setupErrors; return; }
    SUintRead *q = &g_uintReads[g_uintCount++]; SPurity *p = before_read();
    q->result = ZrTests_ReturnedCrt_TryFindLiveRecord(kind, pointer, outputPresent ? &output : NULL);
    after_read(p);
    q->actual = output; q->expectedResult = expectedResult; q->expected = expectedResult ? expected : 719u;
}
static void read_event(unsigned kind, unsigned index, int expectedResult, SZrReturnedHeapEvent expected,
                       int outputPresent) {
    SZrReturnedHeapEvent output = {721u, 722u, 723u, 724u, 725u, (TZrBool)7u};
    if (g_eventCount == TEST_EVENTS) { ++g_setupErrors; return; }
    SEventRead *q = &g_eventReads[g_eventCount++]; SPurity *p = before_read();
    q->result = ZrTests_ReturnedCrt_TryEvent(kind, index, outputPresent ? &output : NULL);
    after_read(p);
    q->actual = output; q->expectedResult = expectedResult;
    q->expected = expectedResult ? expected : (SZrReturnedHeapEvent){721u, 722u, 723u, 724u, 725u, (TZrBool)7u};
}
static void *owner(unsigned slot, size_t bytes, int expectedSuccess) {
    void *p = ZrTests_ReturnedOwner_Malloc(bytes);
    g_roots[slot] = (SRoot){p, ZR_RETURNED_OWNER};
    if ((p != NULL) != expectedSuccess) ++g_setupErrors;
    return p;
}
static void *descriptor(unsigned slot, unsigned site, int expectedSuccess) {
    void *p = ZrTests_ReturnedDescriptor_Calloc(g_counts[site], g_widths[site]);
    g_roots[slot] = (SRoot){p, ZR_RETURNED_DESCRIPTOR};
    if ((p != NULL) != expectedSuccess) ++g_setupErrors;
    if (p != NULL) {
        const unsigned char *bytes = p;
        for (size_t i = 0u; i < g_counts[site] * g_widths[site]; ++i)
            if (bytes[i] != 0u) ++g_zeroErrors;
    }
    return p;
}
static void release(unsigned slot) {
    SRoot r = g_roots[slot];
    g_roots[slot].pointer = NULL;
    if (r.kind == ZR_RETURNED_DESCRIPTOR) ZrTests_ReturnedDescriptor_Free(r.pointer);
    else ZrTests_ReturnedOwner_Free(r.pointer);
}
static void close_owners(void) {
    if (g_closed) return;
    ZrTests_ReturnedCrt_Disarm();
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) release(i);
    g_original[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    g_original[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    ZrTests_ReturnedCrt_Recover();
    g_recovered[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    g_recovered[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    g_closed = 1;
}
static void health(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_setupErrors);
    TEST_ASSERT_EQUAL_UINT(0u, g_zeroErrors);
    for (unsigned k = 0u; k < 2u; ++k) {
        TEST_ASSERT_EQUAL_UINT(g_expectedErrors[k], g_original[k].errors);
        TEST_ASSERT_EQUAL_UINT(0u, g_original[k].live);
        TEST_ASSERT_EQUAL_UINT(g_original[k].successes, g_original[k].frees);
        stats_equal(g_original[k], g_recovered[k]);
    }
}
static void finish(void) {
    close_owners(); health();
    for (unsigned i = 0u; i < g_purityCount; ++i)
        for (unsigned k = 0u; k < 2u; ++k) stats_equal(g_purity[i].before[k], g_purity[i].after[k]);
    for (unsigned i = 0u; i < g_uintCount; ++i) {
        TEST_ASSERT_EQUAL_INT(g_uintReads[i].expectedResult, g_uintReads[i].result);
        TEST_ASSERT_EQUAL_UINT(g_uintReads[i].expected, g_uintReads[i].actual);
    }
    for (unsigned i = 0u; i < g_eventCount; ++i) {
        TEST_ASSERT_EQUAL_INT(g_eventReads[i].expectedResult, g_eventReads[i].result);
        event_equal(g_eventReads[i].expected, g_eventReads[i].actual);
    }
    /* Verify all refusals and supporting reads before claiming TryStats feature credit. */
    for (unsigned i = 0u; i < g_statsCount; ++i) {
        SStatsRead *q = &g_statsReads[i];
        if (!q->result) stats_equal(stats(711u, 712u, 713u, 714u, 715u, 716u), q->actual);
        if (!q->expectedResult) TEST_ASSERT_EQUAL_INT(0, q->result);
    }
    for (unsigned i = 0u; i < g_statsCount; ++i) {
        SStatsRead *q = &g_statsReads[i];
        if (q->expectedResult) {
            if (!q->result) printf("RETURNED_CRT_STATS_FEATURE read=%u\n", i);
            TEST_ASSERT_TRUE_MESSAGE(q->result, "RETURNED_CRT_STATS_FEATURE actual TryStats must succeed");
        }
        TEST_ASSERT_EQUAL_INT(q->expectedResult, q->result);
        stats_equal(q->expected, q->actual);
    }
}
void setUp(void) {
    memset(g_roots, 0, sizeof(g_roots)); memset(g_purity, 0, sizeof(g_purity));
    memset(g_statsReads, 0, sizeof(g_statsReads)); memset(g_uintReads, 0, sizeof(g_uintReads));
    memset(g_eventReads, 0, sizeof(g_eventReads)); memset(g_expectedErrors, 0, sizeof(g_expectedErrors));
    memset(g_saved, 0, sizeof(g_saved));
    g_purityCount = g_statsCount = g_uintCount = g_eventCount = g_setupErrors = g_zeroErrors = 0u;
    g_closed = 0;
    ZrTests_ReturnedCrt_Disarm();
    ZrTests_ReturnedCrt_Reset(); /* Only before the case acquires a resource. */
}
void tearDown(void) { close_owners(); health(); }

static void test_empty_ledgers_report_all_six_zero_stats(void) {
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(0u, 0u, 0u, 0u, 0u, 0u), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(0u, 0u, 0u, 0u, 0u, 0u), 1);
    finish();
}
static void test_owner_live_and_freed_stats_use_actual_requests(void) {
    void *p = owner(0u, 24u, 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(1u, 1u, 0u, 1u, 0u, 0u), 1);
    read_live(ZR_RETURNED_OWNER, p, 1, 0u, 1);
    read_frees(ZR_RETURNED_OWNER, 0u, 1, 0u, 1);
    read_event(ZR_RETURNED_OWNER, 0u, 1, event(1u, 24u, 1u, 0u, ZR_FALSE), 1);
    release(0u);
    read_frees(ZR_RETURNED_OWNER, 0u, 1, 1u, 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(1u, 1u, 1u, 0u, 0u, 0u), 1);
    finish();
}
static void test_six_descriptor_sites_use_typed_sizes_and_zeroed_storage(void) {
    ZrTests_ReturnedCrt_BeginCall(0u, ZR_FALSE);
    for (unsigned i = 0u; i < 6u; ++i) {
        void *p = descriptor(i, i, 1);
        read_live(ZR_RETURNED_DESCRIPTOR, p, 1, i, 1);
        read_event(ZR_RETURNED_DESCRIPTOR, i, 1, event(g_counts[i], g_widths[i], i + 1u, i, ZR_FALSE), 1);
    }
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(6u, 6u, 0u, 6u, 0u, 0u), 1);
    close_owners();
    for (unsigned i = 0u; i < 6u; ++i) read_frees(ZR_RETURNED_DESCRIPTOR, i, 1, 1u, 1);
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(6u, 6u, 6u, 0u, 0u, 0u), 1);
    finish();
}
static void injected_descriptor(unsigned site) {
    ZrTests_ReturnedCrt_BeginCall(site, ZR_FALSE);
    for (unsigned i = 0u; i < site; ++i) {
        (void)descriptor(i, i, i + 1u != site);
        read_event(ZR_RETURNED_DESCRIPTOR, i, 1,
                   event(g_counts[i], g_widths[i], i + 1u,
                         i + 1u == site ? ZR_RETURNED_CRT_ROWS : i,
                         (TZrBool)(i + 1u == site)), 1);
    }
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(site, site - 1u, 0u, site - 1u, 0u, 1u), 1);
    close_owners();
    for (unsigned i = 0u; i + 1u < site; ++i) read_frees(ZR_RETURNED_DESCRIPTOR, i, 1, 1u, 1);
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(site, site - 1u, site - 1u, 0u, 0u, 1u), 1);
    finish();
}
static void test_descriptor_fault_site_1(void) { injected_descriptor(1u); }
static void test_descriptor_fault_site_2(void) { injected_descriptor(2u); }
static void test_descriptor_fault_site_3(void) { injected_descriptor(3u); }
static void test_descriptor_fault_site_4(void) { injected_descriptor(4u); }
static void test_descriptor_fault_site_5(void) { injected_descriptor(5u); }
static void test_descriptor_fault_site_6(void) { injected_descriptor(6u); }

static void test_invalid_public_reads_preserve_outputs_stats_and_events(void) {
    unsigned char foreign[8] = {0};
    void *o = owner(0u, 16u, 1); void *d = descriptor(1u, 0u, 1);
    SZrReturnedHeapEvent oe = event(1u, 16u, 1u, 0u, ZR_FALSE);
    SZrReturnedHeapEvent de = event(g_counts[0], g_widths[0], 1u, 0u, ZR_FALSE);
    read_event(ZR_RETURNED_OWNER, 0u, 1, oe, 1); read_event(ZR_RETURNED_DESCRIPTOR, 0u, 1, de, 1);
    read_stats(2u, 0, stats(0u, 0u, 0u, 0u, 0u, 0u), 1);
    read_stats(UINT_MAX, 0, stats(0u, 0u, 0u, 0u, 0u, 0u), 1);
    read_stats(ZR_RETURNED_OWNER, 0, stats(0u, 0u, 0u, 0u, 0u, 0u), 0);
    read_frees(2u, 0u, 0, 0u, 1); read_frees(ZR_RETURNED_OWNER, 1u, 0, 0u, 1);
    read_frees(ZR_RETURNED_DESCRIPTOR, UINT_MAX, 0, 0u, 1); read_frees(ZR_RETURNED_OWNER, 0u, 0, 0u, 0);
    read_event(2u, 0u, 0, oe, 1); read_event(ZR_RETURNED_OWNER, 1u, 0, oe, 1);
    read_event(ZR_RETURNED_DESCRIPTOR, UINT_MAX, 0, de, 1); read_event(ZR_RETURNED_OWNER, 0u, 0, oe, 0);
    read_live(2u, o, 0, 0u, 1); read_live(ZR_RETURNED_OWNER, NULL, 0, 0u, 1);
    read_live(ZR_RETURNED_OWNER, foreign, 0, 0u, 1); read_live(ZR_RETURNED_DESCRIPTOR, d, 0, 0u, 0);
    read_event(ZR_RETURNED_OWNER, 0u, 1, oe, 1); read_event(ZR_RETURNED_DESCRIPTOR, 0u, 1, de, 1);
    finish();
}
static void test_exact_live_lookup_excludes_interior_foreign_freed_and_other_kind(void) {
    unsigned char foreign[4] = {0};
    unsigned char *a = owner(0u, 16u, 1); void *b = owner(1u, 24u, 1);
    void *d = descriptor(2u, 0u, 1); uintptr_t saved = (uintptr_t)a;
    read_live(ZR_RETURNED_OWNER, a, 1, 0u, 1); read_live(ZR_RETURNED_OWNER, b, 1, 1u, 1);
    read_live(ZR_RETURNED_DESCRIPTOR, d, 1, 0u, 1);
    if (a != NULL) { read_live(ZR_RETURNED_OWNER, a + 1u, 0, 0u, 1); read_live(ZR_RETURNED_OWNER, a + 16u, 0, 0u, 1); }
    read_live(ZR_RETURNED_OWNER, foreign, 0, 0u, 1); read_live(ZR_RETURNED_OWNER, d, 0, 0u, 1);
    read_live(ZR_RETURNED_DESCRIPTOR, b, 0, 0u, 1); release(0u);
    read_live(ZR_RETURNED_OWNER, (const void *)saved, 0, 0u, 1);
    read_frees(ZR_RETURNED_OWNER, 0u, 1, 1u, 1);
    finish();
}
static void test_queries_preserve_pending_owner_fault(void) {
    void *p = owner(0u, 16u, 1);
    ZrTests_ReturnedCrt_BeginCall(0u, ZR_TRUE);
    read_live(ZR_RETURNED_OWNER, p, 1, 0u, 1);
    read_event(ZR_RETURNED_OWNER, 0u, 1, event(1u, 16u, 1u, 0u, ZR_FALSE), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(1u, 1u, 0u, 1u, 0u, 0u), 1);
    (void)owner(1u, 8u, 0);
    read_event(ZR_RETURNED_OWNER, 1u, 1, event(1u, 8u, 1u, ZR_RETURNED_CRT_ROWS, ZR_TRUE), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(2u, 1u, 0u, 1u, 0u, 1u), 1);
    finish();
}
static void test_begin_call_rebases_sites_without_clearing_unarmed_faults(void) {
    (void)descriptor(0u, 0u, 1); (void)owner(1u, 16u, 1);
    ZrTests_ReturnedCrt_BeginCall(2u, ZR_TRUE);
    ZrTests_ReturnedCrt_BeginCall(0u, ZR_FALSE);
    (void)descriptor(2u, 0u, 1); (void)descriptor(3u, 1u, 0); (void)owner(4u, 8u, 0);
    read_event(ZR_RETURNED_DESCRIPTOR, 1u, 1, event(g_counts[0], g_widths[0], 1u, 1u, ZR_FALSE), 1);
    read_event(ZR_RETURNED_DESCRIPTOR, 2u, 1, event(g_counts[1], g_widths[1], 2u, ZR_RETURNED_CRT_ROWS, ZR_TRUE), 1);
    read_event(ZR_RETURNED_OWNER, 1u, 1, event(1u, 8u, 1u, ZR_RETURNED_CRT_ROWS, ZR_TRUE), 1);
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(3u, 2u, 0u, 2u, 0u, 1u), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(2u, 1u, 0u, 1u, 0u, 1u), 1);
    finish();
}
static void test_invalid_geometry_adds_errors_without_attempts_or_events(void) {
    void *a = ZrTests_ReturnedDescriptor_Calloc(0u, g_widths[0]); g_roots[0] = (SRoot){a, ZR_RETURNED_DESCRIPTOR};
    void *b = ZrTests_ReturnedDescriptor_Calloc(g_counts[0], 0u); g_roots[1] = (SRoot){b, ZR_RETURNED_DESCRIPTOR};
    void *c = ZrTests_ReturnedDescriptor_Calloc(SIZE_MAX, 2u); g_roots[2] = (SRoot){c, ZR_RETURNED_DESCRIPTOR};
    void *d = ZrTests_ReturnedDescriptor_Calloc(g_counts[0] + 1u, g_widths[0]); g_roots[3] = (SRoot){d, ZR_RETURNED_DESCRIPTOR};
    void *e = ZrTests_ReturnedDescriptor_Calloc(g_counts[0], g_widths[0] + 1u); g_roots[4] = (SRoot){e, ZR_RETURNED_DESCRIPTOR};
    if (a != NULL || b != NULL || c != NULL || d != NULL || e != NULL) ++g_setupErrors;
    (void)owner(5u, 0u, 0); g_expectedErrors[0] = 5u; g_expectedErrors[1] = 1u;
    read_event(ZR_RETURNED_DESCRIPTOR, 0u, 0, event(1u, 1u, 1u, 0u, ZR_FALSE), 1);
    read_event(ZR_RETURNED_OWNER, 0u, 0, event(1u, 1u, 1u, 0u, ZR_FALSE), 1);
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(0u, 0u, 0u, 0u, 5u, 0u), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(0u, 0u, 0u, 0u, 1u, 0u), 1);
    finish();
}
static void test_real_256_owner_attempts_reject_the_next_without_an_event(void) {
    for (unsigned i = 0u; i < ZR_RETURNED_CRT_ROWS; ++i) {
        (void)owner(i, 8u, 1);
        read_event(ZR_RETURNED_OWNER, i, 1, event(1u, 8u, i + 1u, i, ZR_FALSE), 1);
    }
    (void)owner(ZR_RETURNED_CRT_ROWS, 8u, 0);
    g_expectedErrors[1] = 1u;
    read_event(ZR_RETURNED_OWNER, ZR_RETURNED_CRT_ROWS, 0, event(1u, 8u, 1u, 0u, ZR_FALSE), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(256u, 256u, 0u, 256u, 1u, 0u), 1);
    close_owners();
    for (unsigned i = 0u; i < ZR_RETURNED_CRT_ROWS; ++i) read_frees(ZR_RETURNED_OWNER, i, 1, 1u, 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(256u, 256u, 256u, 0u, 1u, 0u), 1);
    finish();
}
static void recover_registered(void) {
    /* Transfer the registered roots to the actual ledger recovery before it frees them. */
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) g_roots[i].pointer = NULL;
    ZrTests_ReturnedCrt_Recover();
}
static void test_recovery_reports_one_error_per_live_kind_and_releases_records(void) {
    (void)descriptor(0u, 0u, 1); (void)owner(1u, 16u, 1); (void)owner(2u, 8u, 1);
    ZrTests_ReturnedCrt_Disarm(); recover_registered();
    g_expectedErrors[0] = g_expectedErrors[1] = 1u;
    read_frees(ZR_RETURNED_DESCRIPTOR, 0u, 1, 1u, 1);
    read_frees(ZR_RETURNED_OWNER, 0u, 1, 1u, 1); read_frees(ZR_RETURNED_OWNER, 1u, 1, 1u, 1);
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(1u, 1u, 1u, 0u, 1u, 0u), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(2u, 2u, 2u, 0u, 1u, 0u), 1);
    finish();
}
static void test_repeated_recovery_is_idempotent_and_does_not_disarm_fault(void) {
    (void)owner(0u, 16u, 1);
    ZrTests_ReturnedCrt_BeginCall(0u, ZR_TRUE);
    recover_registered();
    g_saved[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    ZrTests_ReturnedCrt_Recover();
    g_saved[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    (void)owner(1u, 8u, 0); g_expectedErrors[1] = 1u;
    read_event(ZR_RETURNED_OWNER, 1u, 1, event(1u, 8u, 1u, ZR_RETURNED_CRT_ROWS, ZR_TRUE), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(2u, 1u, 1u, 0u, 1u, 1u), 1);
    close_owners(); health(); stats_equal(stats(1u, 1u, 1u, 0u, 1u, 0u), g_saved[0]);
    stats_equal(g_saved[0], g_saved[1]); finish();
}
static void test_null_and_foreign_frees_have_explicit_error_accounting(void) {
    unsigned char foreign[8] = {0};
    ZrTests_ReturnedDescriptor_Free(NULL); ZrTests_ReturnedOwner_Free(NULL);
    ZrTests_ReturnedDescriptor_Free(foreign); ZrTests_ReturnedOwner_Free(foreign);
    g_expectedErrors[0] = g_expectedErrors[1] = 1u;
    read_stats(ZR_RETURNED_DESCRIPTOR, 1, stats(0u, 0u, 0u, 0u, 1u, 0u), 1);
    read_stats(ZR_RETURNED_OWNER, 1, stats(0u, 0u, 0u, 0u, 1u, 0u), 1);
    finish();
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_ledgers_report_all_six_zero_stats);
    RUN_TEST(test_owner_live_and_freed_stats_use_actual_requests);
    RUN_TEST(test_six_descriptor_sites_use_typed_sizes_and_zeroed_storage);
    RUN_TEST(test_descriptor_fault_site_1);
    RUN_TEST(test_descriptor_fault_site_2);
    RUN_TEST(test_descriptor_fault_site_3);
    RUN_TEST(test_descriptor_fault_site_4);
    RUN_TEST(test_descriptor_fault_site_5);
    RUN_TEST(test_descriptor_fault_site_6);
    RUN_TEST(test_invalid_public_reads_preserve_outputs_stats_and_events);
    RUN_TEST(test_exact_live_lookup_excludes_interior_foreign_freed_and_other_kind);
    RUN_TEST(test_queries_preserve_pending_owner_fault);
    RUN_TEST(test_begin_call_rebases_sites_without_clearing_unarmed_faults);
    RUN_TEST(test_invalid_geometry_adds_errors_without_attempts_or_events);
    RUN_TEST(test_real_256_owner_attempts_reject_the_next_without_an_event);
    RUN_TEST(test_recovery_reports_one_error_per_live_kind_and_releases_records);
    RUN_TEST(test_repeated_recovery_is_idempotent_and_does_not_disarm_fault);
    RUN_TEST(test_null_and_foreign_frees_have_explicit_error_accounting);
    return UNITY_END();
}
