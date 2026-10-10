#include <stdio.h>
#include <string.h>
#include "support/ssa_source_returned_execir_cleanup.h"
#include "zr_vm_core/aot_ir.h"
#include "unity.h"

/* Exactly one actual allocation in each of three ledgers per lifecycle. These
 * are finite fixture bounds, not allocator capacities or opaque owner storage.
 */
enum { TEST_LEDGERS = 3, TEST_CASES = 6 };
typedef struct SCloseWitness {
    SZrReturnedHeapStats crtBefore[2], crtAfter[2];
    SZrOwnerHeapStats ownerBefore, ownerAfter;
    int beforeResult[TEST_LEDGERS], afterResult[TEST_LEDGERS];
    unsigned beforeFrees[TEST_LEDGERS], afterFrees[TEST_LEDGERS];
    unsigned priorFailed, priorIgnored, flagsPreserved;
    unsigned lifecycle, residual, closed, support;
    SZrReturnedExecCleanupReport report;
    SZrReturnedExecCleanupRecord records[TEST_LEDGERS];
} SCloseWitness;

static void *g_roots[TEST_LEDGERS];
static unsigned g_record[TEST_LEDGERS];
static SZrReturnedExecCleanupRecord g_records[TEST_LEDGERS];
static SZrReturnedExecCleanupReport g_report;
static SCloseWitness g_close, g_saved[TEST_CASES];
static unsigned g_case, g_lifecycle, g_setupErrors, g_closed, g_residual;
static int g_priorWitnessesVerified = 1;

static int crt_equal(SZrReturnedHeapStats a, SZrReturnedHeapStats b) {
    return a.attempts == b.attempts && a.successes == b.successes &&
        a.frees == b.frees && a.live == b.live && a.errors == b.errors &&
        a.failures == b.failures;
}
static int owner_equal(SZrOwnerHeapStats a, SZrOwnerHeapStats b) {
    return a.attempts == b.attempts && a.records == b.records &&
        a.live == b.live && a.frees == b.frees && a.errors == b.errors &&
        a.failures == b.failures;
}
static SZrReturnedHeapStats expected_crt(unsigned frees, unsigned live, unsigned errors) {
    SZrReturnedHeapStats s = {1u, 1u, frees, live, errors, 0u}; return s;
}
static SZrOwnerHeapStats expected_owner(unsigned frees, unsigned live, unsigned errors) {
    SZrOwnerHeapStats s = {1u, 1u, live, frees, errors, 0u}; return s;
}
static void release_registered(void) {
    void *pointer = g_roots[0]; g_roots[0] = NULL;
    ZrTests_ReturnedDescriptor_Free(pointer);
    pointer = g_roots[1]; g_roots[1] = NULL;
    ZrTests_ReturnedOwner_Free(pointer);
    pointer = g_roots[2]; g_roots[2] = NULL;
    ZrTests_ExecOwner_Free(pointer);
}
static void record_reads(SCloseWitness *w, int after) {
    int *result = after ? w->afterResult : w->beforeResult;
    unsigned *frees = after ? w->afterFrees : w->beforeFrees;
    result[0] = ZrTests_Returned_TryRecordFrees(ZR_RETURNED_DESCRIPTOR, g_record[0], &frees[0]);
    result[1] = ZrTests_Returned_TryRecordFrees(ZR_RETURNED_OWNER, g_record[1], &frees[1]);
    result[2] = ZrTests_ExecOwner_TryRecordFrees(g_record[2], &frees[2]);
}
static unsigned support_valid(const SCloseWitness *w) {
    unsigned beforeFree = w->residual ? 0u : 1u;
    unsigned beforeLive = w->residual ? 1u : 0u;
    unsigned afterError = w->residual ? 1u : 0u;
    if (g_setupErrors != 0u || w->closed != 1u || w->flagsPreserved != 1u ||
        w->priorIgnored != 0u) return 0u;
    for (unsigned k = 0u; k < 2u; ++k) {
        if (!crt_equal(w->crtBefore[k], expected_crt(beforeFree, beforeLive, 0u)) ||
            !crt_equal(w->crtAfter[k], expected_crt(1u, 0u, afterError))) return 0u;
    }
    if (!owner_equal(w->ownerBefore, expected_owner(beforeFree, beforeLive, 0u)) ||
        !owner_equal(w->ownerAfter, expected_owner(1u, 0u, afterError))) return 0u;
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) {
        if (w->beforeResult[k] != 1 || w->afterResult[k] != 1 ||
            w->beforeFrees[k] != beforeFree || w->afterFrees[k] != 1u ||
            g_roots[k] != NULL) return 0u;
    }
    return 1u;
}
static void close_capture(void) {
    if (g_closed) return;
    g_close.priorFailed = (unsigned)Unity.CurrentTestFailed;
    g_close.priorIgnored = (unsigned)Unity.CurrentTestIgnored;
    g_close.lifecycle = g_lifecycle;
    g_close.residual = g_residual;
    ZrTests_ReturnedCrt_Disarm(); ZrTests_ExecOwner_Disarm();
    release_registered();
    g_close.crtBefore[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    g_close.crtBefore[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    g_close.ownerBefore = ZrTests_ExecOwner_Stats();
    record_reads(&g_close, 0);
    ZrTests_ReturnedExec_CleanupLedgers(&g_report);
    g_close.crtAfter[0] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_DESCRIPTOR);
    g_close.crtAfter[1] = ZrTests_ReturnedCrt_Stats(ZR_RETURNED_OWNER);
    g_close.ownerAfter = ZrTests_ExecOwner_Stats();
    record_reads(&g_close, 1);
    g_close.flagsPreserved = (unsigned)(g_close.priorFailed == (unsigned)Unity.CurrentTestFailed &&
        g_close.priorIgnored == (unsigned)Unity.CurrentTestIgnored);
    g_close.report = g_report;
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) g_close.records[k] = g_records[k];
    g_closed = 1u; g_close.closed = 1u;
    g_close.support = support_valid(&g_close);
}
static int prior_drain_valid(void) {
    if (g_lifecycle == 0u) {
        return ZrTests_ReturnedCrt_Stats(0u).live == 0u &&
            ZrTests_ReturnedCrt_Stats(1u).live == 0u && ZrTests_ExecOwner_Stats().live == 0u;
    }
    if (!g_closed || !g_priorWitnessesVerified || g_close.support != 1u) return 0;
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) if (g_roots[k] != NULL) return 0;
    return crt_equal(ZrTests_ReturnedCrt_Stats(0u), g_close.crtAfter[0]) &&
        crt_equal(ZrTests_ReturnedCrt_Stats(1u), g_close.crtAfter[1]) &&
        owner_equal(ZrTests_ExecOwner_Stats(), g_close.ownerAfter);
}
static void begin_lifecycle(void) {
    SZrReturnedExecCleanupStorage storage = {{&g_records[0], &g_records[1]}, {1u, 1u}, &g_records[2], 1u};
    /* This numerical boundary is independent of completed, so RED's deliberate
     * feature refusal cannot prevent final drain or later root registration.
     */
    int verified = prior_drain_valid();
    if (!verified) { ++g_setupErrors; return; }
    ZrTests_ReturnedCrt_Disarm(); ZrTests_ExecOwner_Disarm();
    ZrTests_ReturnedCrt_Reset(); ZrTests_ExecOwner_Reset();
    if (ZrTests_ReturnedExec_CleanupBegin(&g_report, &storage, verified) != 1) {
        ++g_setupErrors; return;
    }
    memset(g_records, 0, sizeof(g_records)); memset(&g_close, 0, sizeof(g_close));
    memset(g_record, 0, sizeof(g_record));
    ++g_lifecycle; g_closed = 0u; g_residual = 0u; g_priorWitnessesVerified = 0;
}
static void acquire_registered(void) {
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    SZrOwnerLiveSpan span = {0};
    ZrTests_ReturnedCrt_BeginCall(0u, ZR_FALSE);
    g_roots[0] = ZrTests_ReturnedDescriptor_Calloc(3u, sizeof(SZrAotIrInstruction));
    g_roots[1] = ZrTests_ReturnedOwner_Malloc(8u);
    g_roots[2] = ZrTests_ExecOwner_Malloc(16u);
    if (g_roots[0] == NULL || g_roots[1] == NULL || g_roots[2] == NULL) { ++g_setupErrors; return; }
    if (ZrTests_ReturnedCrt_TryFindLiveRecord(ZR_RETURNED_DESCRIPTOR, g_roots[0], &g_record[0]) != 1 ||
        ZrTests_ReturnedCrt_TryFindLiveRecord(ZR_RETURNED_OWNER, g_roots[1], &g_record[1]) != 1 ||
        ZrTests_ExecOwner_TryLiveSpan(before.records, ZrTests_ExecOwner_Stats().records,
            g_roots[2], 16u, 1u, &span) != 1) { ++g_setupErrors; return; }
    g_record[2] = span.record;
    if (span.allocation != g_roots[2] || span.bytes != 16u) ++g_setupErrors;
}
static void assert_support(void) {
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, g_close.support, "RETURNED_EXEC_CLEANUP_SUPPORT");
    TEST_ASSERT_EQUAL_UINT(0u, g_setupErrors);
}
static void assert_completed(void) {
    if (g_report.completed != 1u) TEST_FAIL_MESSAGE("RETURNED_EXEC_CLEANUP_MISSING_COMPLETION");
    /* Independent real-ledger observations prevent a flag-only GREEN report.
     * Every check follows completion, preserving the accepted RED protocol.
     */
    TEST_ASSERT_EQUAL_UINT(1u, g_report.begun);
    TEST_ASSERT_EQUAL_UINT(1u, g_report.closed);
    TEST_ASSERT_EQUAL_UINT(0u, g_report.refusedQueries);
    TEST_ASSERT_EQUAL_UINT(0u, g_report.observationErrors);
    TEST_ASSERT_EQUAL_UINT(g_close.residual ? 0u : 1u, g_report.healthyBeforeRecovery);
    TEST_ASSERT_TRUE(owner_equal(g_close.ownerBefore, g_report.ownerBefore));
    TEST_ASSERT_TRUE(owner_equal(g_close.ownerAfter, g_report.ownerAfter));
    TEST_ASSERT_EQUAL_UINT(1u, g_report.ownerRecordCount);
    TEST_ASSERT_EQUAL_UINT(g_close.ownerBefore.records, g_report.ownerRecordCount);
    TEST_ASSERT_TRUE(g_report.storage.owner == &g_records[2]);
    TEST_ASSERT_EQUAL_UINT(1u, g_report.storage.ownerCapacity);
    for (unsigned k = 0u; k < 2u; ++k) {
        TEST_ASSERT_EQUAL_INT(1, g_report.crtBeforeResult[k]);
        TEST_ASSERT_EQUAL_INT(1, g_report.crtAfterResult[k]);
        TEST_ASSERT_TRUE(crt_equal(g_close.crtBefore[k], g_report.crtBefore[k]));
        TEST_ASSERT_TRUE(crt_equal(g_close.crtAfter[k], g_report.crtAfter[k]));
        TEST_ASSERT_EQUAL_UINT(1u, g_report.crtRecordCount[k]);
        TEST_ASSERT_EQUAL_UINT(g_close.crtBefore[k].successes, g_report.crtRecordCount[k]);
        TEST_ASSERT_TRUE(g_report.storage.crt[k] == &g_records[k]);
        TEST_ASSERT_EQUAL_UINT(1u, g_report.storage.crtCapacity[k]);
    }
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) {
        TEST_ASSERT_EQUAL_INT(1, g_records[k].beforeResult);
        TEST_ASSERT_EQUAL_INT(1, g_records[k].afterResult);
        TEST_ASSERT_EQUAL_INT(g_close.beforeResult[k], g_records[k].beforeResult);
        TEST_ASSERT_EQUAL_INT(g_close.afterResult[k], g_records[k].afterResult);
        TEST_ASSERT_EQUAL_UINT(g_close.residual ? 0u : 1u, g_records[k].beforeFrees);
        TEST_ASSERT_EQUAL_UINT(1u, g_records[k].afterFrees);
        TEST_ASSERT_EQUAL_UINT(g_close.beforeFrees[k], g_records[k].beforeFrees);
        TEST_ASSERT_EQUAL_UINT(g_close.afterFrees[k], g_records[k].afterFrees);
        TEST_ASSERT_EQUAL_INT(g_close.records[k].beforeResult, g_records[k].beforeResult);
        TEST_ASSERT_EQUAL_INT(g_close.records[k].afterResult, g_records[k].afterResult);
        TEST_ASSERT_EQUAL_UINT(g_close.records[k].beforeFrees, g_records[k].beforeFrees);
        TEST_ASSERT_EQUAL_UINT(g_close.records[k].afterFrees, g_records[k].afterFrees);
    }
}
static void assert_prehealth(void) {
    if (g_report.healthyBeforeRecovery != 1u) TEST_FAIL_MESSAGE("RETURNED_EXEC_CLEANUP_PREHEALTH");
}

void setUp(void) {
    ++g_case;
    if (g_case == 1u || g_case == 2u || g_case == 4u) begin_lifecycle();
}
void tearDown(void) {
    close_capture();
    /* Actual prior failure/ignore is preserved; cleanup made no Unity call.
     * Genuine UnityIgnore execution remains OPEN in this six-case protocol.
     */
    if (Unity.CurrentTestFailed || Unity.CurrentTestIgnored) return;
    assert_support(); assert_completed();
    if (g_case <= 4u) assert_prehealth();
}

static void test_healthy_registered_ledger_close(void) {
    acquire_registered(); close_capture(); assert_support();
    g_priorWitnessesVerified = 1;
    assert_completed(); assert_prehealth();
}
static void test_prior_unity_failure_still_finishes_cleanup(void) {
    acquire_registered();
    TEST_FAIL_MESSAGE("RETURNED_EXEC_CLEANUP_PRIOR_FAILURE");
}
static void test_prior_failure_saved_witnesses_are_complete(void) {
    assert_support();
    TEST_ASSERT_EQUAL_UINT(1u, g_close.priorFailed);
    TEST_ASSERT_EQUAL_UINT(0u, g_close.priorIgnored);
    TEST_ASSERT_EQUAL_UINT(g_saved[1].lifecycle, g_close.lifecycle);
    TEST_ASSERT_EQUAL_UINT(g_saved[1].support, g_close.support);
    TEST_ASSERT_EQUAL_UINT(g_saved[1].priorFailed, g_close.priorFailed);
    g_priorWitnessesVerified = 1;
    assert_completed(); assert_prehealth();
}
static void test_residual_cleanup_cannot_become_healthy(void) {
    acquire_registered();
    /* Transfer exactly these genuine live raw allocations to their authoritative
     * ledgers. No forgotten opaque owner or fabricated runtime object exists.
     */
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) g_roots[k] = NULL;
    g_residual = 1u;
    close_capture(); assert_support(); assert_completed(); assert_prehealth();
}
static void test_residual_saved_health_and_failure_remain_visible(void) {
    assert_support();
    TEST_ASSERT_EQUAL_UINT(1u, g_close.residual);
    TEST_ASSERT_EQUAL_UINT(0u, g_close.priorFailed);
    TEST_ASSERT_EQUAL_UINT(g_saved[3].lifecycle, g_close.lifecycle);
    TEST_ASSERT_EQUAL_UINT(g_saved[3].support, g_close.support);
    TEST_ASSERT_TRUE(crt_equal(g_saved[3].crtBefore[0], g_close.crtBefore[0]));
    TEST_ASSERT_TRUE(owner_equal(g_saved[3].ownerAfter, g_close.ownerAfter));
    g_priorWitnessesVerified = 1;
    assert_completed();
    TEST_ASSERT_EQUAL_UINT(0u, g_report.healthyBeforeRecovery);
}
static int same_report(const SZrReturnedExecCleanupReport *a, const SZrReturnedExecCleanupReport *b) {
    if (a->begun != b->begun || a->closed != b->closed || a->completed != b->completed ||
        a->healthyBeforeRecovery != b->healthyBeforeRecovery || a->refusedQueries != b->refusedQueries ||
        a->observationErrors != b->observationErrors || a->ownerRecordCount != b->ownerRecordCount ||
        !owner_equal(a->ownerBefore, b->ownerBefore) || !owner_equal(a->ownerAfter, b->ownerAfter) ||
        a->storage.owner != b->storage.owner || a->storage.ownerCapacity != b->storage.ownerCapacity) return 0;
    for (unsigned k = 0u; k < 2u; ++k) {
        if (a->crtBeforeResult[k] != b->crtBeforeResult[k] || a->crtAfterResult[k] != b->crtAfterResult[k] ||
            a->crtRecordCount[k] != b->crtRecordCount[k] || !crt_equal(a->crtBefore[k], b->crtBefore[k]) ||
            !crt_equal(a->crtAfter[k], b->crtAfter[k]) || a->storage.crt[k] != b->storage.crt[k] ||
            a->storage.crtCapacity[k] != b->storage.crtCapacity[k]) return 0;
    }
    return 1;
}
static void test_repeated_close_preserves_original_failure_report(void) {
    SZrReturnedExecCleanupReport before = g_report;
    SZrReturnedExecCleanupRecord records[TEST_LEDGERS];
    SZrReturnedHeapStats crt[2] = {ZrTests_ReturnedCrt_Stats(0u), ZrTests_ReturnedCrt_Stats(1u)};
    SZrOwnerHeapStats owner = ZrTests_ExecOwner_Stats();
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) records[k] = g_records[k];
    ZrTests_ReturnedExec_CleanupLedgers(&g_report); close_capture();
    assert_support();
    TEST_ASSERT_TRUE(same_report(&before, &g_report));
    TEST_ASSERT_TRUE(crt_equal(crt[0], ZrTests_ReturnedCrt_Stats(0u)));
    TEST_ASSERT_TRUE(crt_equal(crt[1], ZrTests_ReturnedCrt_Stats(1u)));
    TEST_ASSERT_TRUE(owner_equal(owner, ZrTests_ExecOwner_Stats()));
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) {
        TEST_ASSERT_EQUAL_INT(records[k].beforeResult, g_records[k].beforeResult);
        TEST_ASSERT_EQUAL_INT(records[k].afterResult, g_records[k].afterResult);
        TEST_ASSERT_EQUAL_UINT(records[k].beforeFrees, g_records[k].beforeFrees);
        TEST_ASSERT_EQUAL_UINT(records[k].afterFrees, g_records[k].afterFrees);
    }
    assert_completed();
    TEST_ASSERT_EQUAL_UINT(0u, g_report.healthyBeforeRecovery);
}
static void emit_case(void) {
    unsigned queries = 0u, frees = 0u;
    for (unsigned k = 0u; k < TEST_LEDGERS; ++k) {
        queries += (unsigned)(g_close.beforeResult[k] == 1) + (unsigned)(g_close.afterResult[k] == 1);
        frees += g_close.afterFrees[k];
    }
    if (g_case <= TEST_CASES) g_saved[g_case - 1u] = g_close;
    printf("RETURNED_EXEC_CLEANUP_CASE case=%u lifecycle=%u completed=%u healthy=%u prior_failed=%u prior_ignored=%u flags_preserved=%u closed=%u support=%u before_live=%u after_live=%u before_errors=%u after_errors=%u queries=%u record_frees=%u\n",
        g_case, g_close.lifecycle, g_close.report.completed, g_close.report.healthyBeforeRecovery,
        g_close.priorFailed, g_close.priorIgnored, g_close.flagsPreserved, g_close.closed, g_close.support,
        g_close.crtBefore[0].live + g_close.crtBefore[1].live + g_close.ownerBefore.live,
        g_close.crtAfter[0].live + g_close.crtAfter[1].live + g_close.ownerAfter.live,
        g_close.crtBefore[0].errors + g_close.crtBefore[1].errors + g_close.ownerBefore.errors,
        g_close.crtAfter[0].errors + g_close.crtAfter[1].errors + g_close.ownerAfter.errors,
        queries, frees);
}
static void final_drain(void) {
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    unsigned frees = 0u, query = 0u;
    ZrTests_ReturnedCrt_Disarm(); ZrTests_ExecOwner_Disarm(); release_registered();
    /* Independent of report completion/closed: failed RED always gets a drain. */
    ZrTests_ReturnedCrt_Recover();
    /* Recover's diagnostic repeats for historical errors even at live zero.
     * A verified empty owner ledger is already drained; recover real residue.
     */
    if (ZrTests_ExecOwner_Stats().live != 0u) ZrTests_ExecOwner_Recover();
    SZrReturnedHeapStats a = ZrTests_ReturnedCrt_Stats(0u), b = ZrTests_ReturnedCrt_Stats(1u);
    SZrOwnerHeapStats o = ZrTests_ExecOwner_Stats();
    int reads = ZrTests_Returned_TryRecordFrees(0u, g_record[0], &query); frees += query;
    query = 0u; reads += ZrTests_Returned_TryRecordFrees(1u, g_record[1], &query); frees += query;
    query = 0u; reads += ZrTests_ExecOwner_TryRecordFrees(g_record[2], &query); frees += query;
    unsigned live = a.live + b.live + o.live;
    unsigned flags = (unsigned)(failed == (unsigned)Unity.CurrentTestFailed && ignored == (unsigned)Unity.CurrentTestIgnored);
    unsigned drained = (unsigned)(live == 0u && reads == TEST_LEDGERS && frees == TEST_LEDGERS &&
        g_roots[0] == NULL && g_roots[1] == NULL && g_roots[2] == NULL && flags == 1u);
    printf("RETURNED_EXEC_CLEANUP_FINAL drain=%u live=%u record_frees=%u errors=%u flags_preserved=%u\n",
        drained, live, frees, a.errors + b.errors + o.errors, flags);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_healthy_registered_ledger_close);
    emit_case();
    RUN_TEST(test_prior_unity_failure_still_finishes_cleanup);
    emit_case();
    RUN_TEST(test_prior_failure_saved_witnesses_are_complete);
    emit_case();
    RUN_TEST(test_residual_cleanup_cannot_become_healthy);
    emit_case();
    RUN_TEST(test_residual_saved_health_and_failure_remain_visible);
    emit_case();
    RUN_TEST(test_repeated_close_preserves_original_failure_report);
    emit_case();
    final_drain();
    return UNITY_END();
}
