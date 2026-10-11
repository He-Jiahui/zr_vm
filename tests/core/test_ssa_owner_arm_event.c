#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "support/ssa_exec_ir_owned_module_allocator.h"
#include "unity.h"

enum { TEST_ROOTS = 2, TEST_CASES = 12 };
typedef struct SObserverWitness {
    SZrOwnerHeapStats before, requested, after;
    SZrOwnerHeapEvent original, copied, next;
    int arm, event, nextEvent;
    int beforeResult[TEST_ROOTS];
    unsigned beforeFrees[TEST_ROOTS];
    int freeResult[TEST_ROOTS];
    unsigned recordFree[TEST_ROOTS];
    unsigned priorFailed, priorIgnored, flagsPreserved;
    unsigned support, feature, drained, queries, recordFrees;
} SObserverWitness;
static void *g_roots[TEST_ROOTS];
static SObserverWitness g_caseWitness, g_prior;
static unsigned g_case, g_support, g_feature, g_closed, g_failedRecord;

static int stats_equal(SZrOwnerHeapStats a, SZrOwnerHeapStats b) {
    return a.attempts == b.attempts && a.records == b.records &&
        a.live == b.live && a.frees == b.frees && a.errors == b.errors &&
        a.failures == b.failures;
}
static int event_equal(SZrOwnerHeapEvent a, SZrOwnerHeapEvent b) {
    return a.kind == b.kind && a.record == b.record && a.failed == b.failed &&
        a.count == b.count && a.width == b.width && a.bytes == b.bytes;
}
static SZrOwnerHeapStats expected_stats(unsigned attempts, unsigned records,
        unsigned live, unsigned frees, unsigned failures) {
    SZrOwnerHeapStats s = {attempts, records, live, frees, 0u, failures}; return s;
}
static int event_shape(SZrOwnerHeapEvent e, unsigned kind, size_t count,
        size_t width, unsigned failed) {
    return e.kind == kind && e.count == count && e.width == width &&
        e.bytes == count * width && e.failed == failed;
}
static void support(int condition) { if (!condition) g_support = 0u; }
static void feature(int condition) { if (!condition) g_feature = 0u; }
static void sentinel(SZrOwnerHeapEvent *event) { memset(event, 0xA5, sizeof(*event)); }
static void free_root(unsigned slot) {
    void *pointer = g_roots[slot]; g_roots[slot] = NULL;
    ZrTests_ExecOwner_Free(pointer);
}
static SZrOwnerHeapEvent actual_event(unsigned index) {
    SZrOwnerHeapEvent e = {0};
    if (Unity.CurrentTestFailed || Unity.CurrentTestIgnored ||
            index >= ZrTests_ExecOwner_Stats().attempts) {
        g_support = 0u; return e;
    }
    return ZrTests_ExecOwner_Event(index);
}
static void check_copy(unsigned index, SZrOwnerHeapEvent actual) {
    SZrOwnerHeapEvent copy, unchanged;
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    sentinel(&copy); memcpy(&unchanged, &copy, sizeof(copy));
    int result = ZrTests_ExecOwner_TryEvent(index, &copy);
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    support(result == 0 || result == 1);
    if (result == 0) support(memcmp(&copy, &unchanged, sizeof(copy)) == 0);
    g_caseWitness.event = result; g_caseWitness.original = actual;
    g_caseWitness.copied = copy;
    feature(result == 1 && event_equal(actual, copy));
}
static void drain(void) {
    if (g_closed) return;
    if (g_case != 11u) {
        g_caseWitness.priorFailed = (unsigned)Unity.CurrentTestFailed;
        g_caseWitness.priorIgnored = (unsigned)Unity.CurrentTestIgnored;
    }
    g_caseWitness.requested = ZrTests_ExecOwner_Stats();
    ZrTests_ExecOwner_Disarm();
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) free_root(i);
    g_caseWitness.after = ZrTests_ExecOwner_Stats();
    support(stats_equal(g_caseWitness.after, expected_stats(
        g_caseWitness.requested.attempts, g_caseWitness.requested.records,
        0u, g_caseWitness.requested.records, g_caseWitness.requested.failures)));
    support(g_caseWitness.after.live == 0u && g_caseWitness.after.errors == 0u &&
        g_caseWitness.after.frees == g_caseWitness.after.records);
    for (unsigned i = 0u; i < g_caseWitness.after.records; ++i) {
        unsigned frees = 0u;
        int result = ZrTests_ExecOwner_TryRecordFrees(i, &frees);
        if (i < TEST_ROOTS) {
            g_caseWitness.freeResult[i] = result; g_caseWitness.recordFree[i] = frees;
        } else g_support = 0u;
        g_caseWitness.queries += (unsigned)(result == 1);
        g_caseWitness.recordFrees += frees;
        support(result == 1 && frees == 1u);
    }
    g_caseWitness.flagsPreserved = (unsigned)(
        g_caseWitness.priorFailed == (unsigned)Unity.CurrentTestFailed &&
        g_caseWitness.priorIgnored == (unsigned)Unity.CurrentTestIgnored);
    support(g_caseWitness.flagsPreserved == 1u);
    g_caseWitness.drained = (unsigned)(g_caseWitness.after.live == 0u &&
        g_roots[0] == NULL && g_roots[1] == NULL);
    g_caseWitness.support = g_support; g_caseWitness.feature = g_feature;
    g_closed = 1u;
}
static void finish(void) {
    drain();
    if (g_support != 1u) TEST_FAIL_MESSAGE("OWNER_ARM_EVENT_SUPPORT");
    if (g_feature != 1u) TEST_FAIL_MESSAGE("OWNER_ARM_EVENT_MISSING_FEATURE");
}
static int try_arm(unsigned relative) {
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    int result = ZrTests_ExecOwner_TryArm(relative);
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    support(result == 0 || result == 1);
    g_caseWitness.arm = result; return result;
}

void setUp(void) {
    ++g_case;
    g_support = 1u; g_feature = 1u; g_closed = 0u;
    memset(&g_caseWitness, 0, sizeof(g_caseWitness));
    /* The witness case observes case11's actual ledger without resetting it. */
    if (g_case == 12u) return;
    SZrOwnerHeapStats old = ZrTests_ExecOwner_Stats();
    if (old.live != 0u || old.errors != 0u || g_roots[0] || g_roots[1]) {
        g_support = 0u; return;
    }
    ZrTests_ExecOwner_Disarm(); ZrTests_ExecOwner_Reset();
    g_caseWitness.before = ZrTests_ExecOwner_Stats();
    support(stats_equal(g_caseWitness.before, expected_stats(0u, 0u, 0u, 0u, 0u)));
}
static void prior_failure_observe(void) {
    g_caseWitness.priorFailed = (unsigned)Unity.CurrentTestFailed;
    g_caseWitness.priorIgnored = (unsigned)Unity.CurrentTestIgnored;
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    support(stats_equal(before, expected_stats(1u, 1u, 1u, 0u, 0u)));
    int arm = try_arm(1u);
    SZrOwnerHeapEvent output, untouched;
    sentinel(&output); memcpy(&untouched, &output, sizeof(output));
    int result = ZrTests_ExecOwner_TryEvent(0u, &output);
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    support(result == 0 || result == 1);
    if (result == 0) support(memcmp(&output, &untouched, sizeof(output)) == 0);
    g_caseWitness.event = result; g_caseWitness.copied = output;
    feature(arm == 1 && result == 1 && event_equal(g_caseWitness.original, output));
    g_roots[1] = ZrTests_ExecOwner_Malloc(7u);
    unsigned refused = (unsigned)(g_roots[1] == NULL);
    support(stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 2u - refused, 2u - refused, 0u, refused)));
    feature(refused == 1u);
    SZrOwnerHeapStats live = ZrTests_ExecOwner_Stats();
    support(live.records <= TEST_ROOTS);
    for (unsigned i = 0u; i < live.records && i < TEST_ROOTS; ++i) {
        unsigned frees = 99u;
        int read = ZrTests_ExecOwner_TryRecordFrees(i, &frees);
        g_caseWitness.beforeResult[i] = read;
        g_caseWitness.beforeFrees[i] = frees;
        support(read == 1 && frees == 0u);
    }
    support(stats_equal(live, ZrTests_ExecOwner_Stats()));
    sentinel(&output); memcpy(&untouched, &output, sizeof(output));
    before = ZrTests_ExecOwner_Stats();
    result = ZrTests_ExecOwner_TryEvent(1u, &output);
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    support(result == 0 || result == 1);
    if (result == 0) support(memcmp(&output, &untouched, sizeof(output)) == 0);
    g_caseWitness.nextEvent = result; g_caseWitness.next = output;
    feature(result == 1 && event_shape(output, 1u, 1u, 7u, refused) &&
        output.record == (refused ? g_failedRecord : 1u));
}
void tearDown(void) {
    if (g_case == 11u && !g_closed) prior_failure_observe();
    drain();
    if (g_case == 11u) g_prior = g_caseWitness;
    /* No Unity assertions, reset, recovery, allocation hiding or flag changes. */
}

static void test_arm_first_malloc(void) {
    int result = try_arm(1u);
    g_roots[0] = ZrTests_ExecOwner_Malloc(9u);
    unsigned refused = (unsigned)(g_roots[0] == NULL);
    support(stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u - refused, 1u - refused, 0u, refused)));
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 9u, refused));
    if (!refused) support(e.record == 0u);
    g_caseWitness.original = e;
    feature(result == 1 && refused == 1u); finish();
}
static void test_arm_second_calloc(void) {
    int result = try_arm(2u);
    g_roots[0] = ZrTests_ExecOwner_Calloc(3u, sizeof(unsigned));
    support(g_roots[0] != NULL);
    if (g_roots[0]) {
        unsigned *values = g_roots[0];
        support(values[0] == 0u && values[1] == 0u && values[2] == 0u);
    }
    SZrOwnerHeapEvent first = actual_event(0u);
    support(event_shape(first, 2u, 3u, sizeof(unsigned), 0u) && first.record == 0u);
    free_root(0u);
    g_roots[1] = ZrTests_ExecOwner_Calloc(3u, sizeof(unsigned));
    unsigned refused = (unsigned)(g_roots[1] == NULL);
    support(stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 2u - refused, 1u - refused, 1u, refused)));
    SZrOwnerHeapEvent second = actual_event(1u);
    support(event_shape(second, 2u, 3u, sizeof(unsigned), refused));
    if (!refused) support(second.record == 1u);
    g_caseWitness.original = second;
    feature(result == 1 && refused == 1u); finish();
}
static void test_malloc_event_copy(void) {
    g_roots[0] = ZrTests_ExecOwner_Malloc(13u);
    support(g_roots[0] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u, 1u, 0u, 0u)));
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 13u, 0u) && e.record == 0u);
    check_copy(0u, e); finish();
}
static void test_calloc_event_copy(void) {
    g_roots[0] = ZrTests_ExecOwner_Calloc(3u, sizeof(unsigned));
    support(g_roots[0] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u, 1u, 0u, 0u)));
    if (g_roots[0]) {
        unsigned *values = g_roots[0];
        support(values[0] == 0u && values[1] == 0u && values[2] == 0u);
    }
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 2u, 3u, sizeof(unsigned), 0u) && e.record == 0u);
    check_copy(0u, e); finish();
}
static void test_failed_realloc_event(void) {
    g_roots[0] = ZrTests_ExecOwner_Malloc(11u);
    support(g_roots[0] != NULL);
    if (g_roots[0]) memset(g_roots[0], 0x39, 11u);
    ZrTests_ExecOwner_Arm(1u);
    void *replacement = ZrTests_ExecOwner_Realloc(g_roots[0], 23u);
    /* A successful replacement must replace its registration immediately. */
    if (replacement != NULL) g_roots[0] = replacement;
    support(replacement == NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 1u, 1u, 0u, 1u)));
    if (g_roots[0]) {
        const unsigned char *bytes = g_roots[0];
        for (unsigned i = 0u; i < 11u; ++i) support(bytes[i] == 0x39u);
    }
    unsigned frees = 99u;
    support(ZrTests_ExecOwner_TryRecordFrees(0u, &frees) == 1 && frees == 0u);
    SZrOwnerHeapEvent e = actual_event(1u);
    support(event_shape(e, 3u, 1u, 23u, 1u) && e.record == 0u);
    check_copy(1u, e); finish();
}
static void test_event_query_keeps_armed_fault(void) {
    g_roots[0] = ZrTests_ExecOwner_Malloc(5u);
    support(g_roots[0] != NULL);
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 5u, 0u) && e.record == 0u);
    ZrTests_ExecOwner_Arm(1u);
    check_copy(0u, e); check_copy(0u, e);
    g_roots[1] = ZrTests_ExecOwner_Malloc(6u);
    support(g_roots[1] == NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 1u, 1u, 0u, 1u)));
    SZrOwnerHeapEvent next = actual_event(1u);
    support(event_shape(next, 1u, 1u, 6u, 1u));
    g_caseWitness.next = next; finish();
}
static void arm_refusal(unsigned relative) {
    int result = try_arm(relative);
    g_roots[0] = ZrTests_ExecOwner_Malloc(4u);
    support(g_roots[0] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u, 1u, 0u, 0u)));
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 4u, 0u) && e.record == 0u);
    g_caseWitness.original = e; feature(result == 0); finish();
}
static void test_arm_zero_refusal(void) { arm_refusal(0u); }
static void test_arm_uintmax_refusal(void) {
    /* A real prior request makes attempts nonzero: UINT_MAX must be refused
     * before scheduling arithmetic can wrap, while its actual event survives. */
    g_roots[0] = ZrTests_ExecOwner_Malloc(2u);
    support(g_roots[0] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u, 1u, 0u, 0u)));
    SZrOwnerHeapEvent original = actual_event(0u);
    support(event_shape(original, 1u, 1u, 2u, 0u) && original.record == 0u);
    int result = try_arm(UINT_MAX);
    support(event_equal(original, actual_event(0u)));
    g_roots[1] = ZrTests_ExecOwner_Malloc(4u);
    support(g_roots[1] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 2u, 2u, 0u, 0u)));
    SZrOwnerHeapEvent next = actual_event(1u);
    support(event_shape(next, 1u, 1u, 4u, 0u) && next.record == 1u);
    g_caseWitness.original = original; g_caseWitness.next = next;
    feature(result == 0); finish();
}
static void test_second_arm_refusal(void) {
    ZrTests_ExecOwner_Arm(1u);
    int result = try_arm(2u);
    g_roots[0] = ZrTests_ExecOwner_Malloc(3u);
    support(g_roots[0] == NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 0u, 0u, 0u, 1u)));
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 3u, 1u));
    /* Actual existing Event supplies this private sentinel; never guess it. */
    g_failedRecord = e.record; g_caseWitness.original = e;
    feature(result == 0); finish();
}
static void test_event_invalid_preserves_output(void) {
    g_roots[0] = ZrTests_ExecOwner_Malloc(8u);
    support(g_roots[0] != NULL);
    ZrTests_ExecOwner_Arm(1u);
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    SZrOwnerHeapEvent output, saved;
    sentinel(&output); memcpy(&saved, &output, sizeof(output));
    int a = ZrTests_ExecOwner_TryEvent(0u, NULL);
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    int b = ZrTests_ExecOwner_TryEvent(before.attempts, &output);
    int same1 = memcmp(&saved, &output, sizeof(output)) == 0;
    support(stats_equal(before, ZrTests_ExecOwner_Stats()));
    int c = ZrTests_ExecOwner_TryEvent(UINT_MAX, &output);
    int same2 = memcmp(&saved, &output, sizeof(output)) == 0;
    SZrOwnerHeapStats afterInvalid = ZrTests_ExecOwner_Stats();
    g_roots[1] = ZrTests_ExecOwner_Malloc(6u);
    support(stats_equal(before, afterInvalid) &&
        stats_equal(before, expected_stats(1u, 1u, 1u, 0u, 0u)));
    SZrOwnerHeapEvent e = actual_event(0u);
    support(event_shape(e, 1u, 1u, 8u, 0u) && e.record == 0u);
    support(g_roots[1] == NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(2u, 1u, 1u, 0u, 1u)));
    SZrOwnerHeapEvent next = actual_event(1u);
    support(event_shape(next, 1u, 1u, 6u, 1u) && next.record == g_failedRecord);
    g_caseWitness.next = next;
    g_caseWitness.event = b; g_caseWitness.original = e;
    feature(a == 0 && b == 0 && c == 0 && same1 && same2); finish();
}
static void test_prior_unity_failure(void) {
    g_roots[0] = ZrTests_ExecOwner_Malloc(10u);
    support(g_roots[0] != NULL && stats_equal(ZrTests_ExecOwner_Stats(),
        expected_stats(1u, 1u, 1u, 0u, 0u)));
    g_caseWitness.original = actual_event(0u);
    support(event_shape(g_caseWitness.original, 1u, 1u, 10u, 0u) &&
        g_caseWitness.original.record == 0u);
    /* Only this expected failure precedes cleanup. Teardown stays nonasserting. */
    TEST_FAIL_MESSAGE("OWNER_ARM_EVENT_PRIOR_FAILURE");
}
static void test_prior_failure_witness(void) {
    support(g_prior.support == 1u && g_prior.drained == 1u &&
        g_prior.priorFailed == 1u && g_prior.priorIgnored == 0u &&
        g_prior.flagsPreserved == 1u);
    unsigned refused = g_prior.requested.failures;
    support(refused <= 1u && stats_equal(g_prior.requested,
        expected_stats(2u, 2u - refused, 2u - refused, 0u, refused)) &&
        stats_equal(g_prior.after, expected_stats(2u, 2u - refused, 0u, 2u - refused, refused)) &&
        stats_equal(g_prior.after, ZrTests_ExecOwner_Stats()) &&
        g_prior.queries == 2u - refused && g_prior.recordFrees == 2u - refused);
    for (unsigned i = 0u; i < g_prior.after.records && i < TEST_ROOTS; ++i) {
        unsigned frees = 0u;
        int result = ZrTests_ExecOwner_TryRecordFrees(i, &frees);
        support(g_prior.beforeResult[i] == 1 && g_prior.beforeFrees[i] == 0u &&
            g_prior.freeResult[i] == 1 && g_prior.recordFree[i] == 1u &&
            result == g_prior.freeResult[i] && frees == g_prior.recordFree[i]);
    }
    support(event_shape(g_prior.original, 1u, 1u, 10u, 0u) && g_prior.original.record == 0u);
    /* Unity has actually concluded case11; the clean existing observer now
     * independently identifies both real saved event instances and all fields. */
    SZrOwnerHeapEvent first = actual_event(0u), second = actual_event(1u);
    support(event_equal(first, g_prior.original) &&
        event_shape(second, 1u, 1u, 7u, refused) &&
        second.record == (refused ? g_failedRecord : 1u));
    feature(g_prior.arm == 1 && g_prior.event == 1 && g_prior.nextEvent == 1 &&
        g_prior.feature == 1u && refused == 1u &&
        event_equal(first, g_prior.copied) && event_equal(second, g_prior.next) &&
        event_shape(g_prior.next, 1u, 1u, 7u, 1u) && g_prior.next.record == g_failedRecord);
    g_caseWitness.before = g_prior.after;
    g_caseWitness.arm = g_prior.arm; g_caseWitness.event = g_prior.event;
    g_caseWitness.nextEvent = g_prior.nextEvent;
    finish();
}
static void emit_case(void) {
    printf("OWNER_ARM_EVENT_CASE case=%u support=%u feature=%u drained=%u arm=%d event=%d next_event=%d attempts=%u records=%u live=%u frees=%u errors=%u failures=%u queries=%u record_frees=%u prior_failed=%u prior_ignored=%u flags_preserved=%u\n",
        g_case, g_caseWitness.support, g_caseWitness.feature, g_caseWitness.drained,
        g_caseWitness.arm, g_caseWitness.event, g_caseWitness.nextEvent,
        g_caseWitness.after.attempts, g_caseWitness.after.records, g_caseWitness.after.live,
        g_caseWitness.after.frees, g_caseWitness.after.errors, g_caseWitness.after.failures,
        g_caseWitness.queries, g_caseWitness.recordFrees, g_caseWitness.priorFailed,
        g_caseWitness.priorIgnored, g_caseWitness.flagsPreserved);
}
static void final_drain(void) {
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    ZrTests_ExecOwner_Disarm();
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) free_root(i);
    SZrOwnerHeapStats s = ZrTests_ExecOwner_Stats();
    unsigned queries = 0u, frees = 0u;
    for (unsigned i = 0u; i < s.records; ++i) {
        unsigned count = 0u;
        queries += (unsigned)(ZrTests_ExecOwner_TryRecordFrees(i, &count) == 1);
        frees += count;
    }
    unsigned flags = (unsigned)(failed == (unsigned)Unity.CurrentTestFailed &&
        ignored == (unsigned)Unity.CurrentTestIgnored);
    unsigned drained = (unsigned)(s.live == 0u && s.errors == 0u &&
        s.frees == s.records && queries == s.records && frees == s.records && flags == 1u);
    printf("OWNER_ARM_EVENT_FINAL drain=%u live=%u errors=%u records=%u frees=%u queries=%u record_frees=%u flags_preserved=%u\n",
        drained, s.live, s.errors, s.records, s.frees, queries, frees, flags);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arm_first_malloc); emit_case();
    RUN_TEST(test_arm_second_calloc); emit_case();
    RUN_TEST(test_malloc_event_copy); emit_case();
    RUN_TEST(test_calloc_event_copy); emit_case();
    RUN_TEST(test_failed_realloc_event); emit_case();
    RUN_TEST(test_event_query_keeps_armed_fault); emit_case();
    RUN_TEST(test_arm_zero_refusal); emit_case();
    RUN_TEST(test_arm_uintmax_refusal); emit_case();
    RUN_TEST(test_second_arm_refusal); emit_case();
    RUN_TEST(test_event_invalid_preserves_output); emit_case();
    RUN_TEST(test_prior_unity_failure); emit_case();
    RUN_TEST(test_prior_failure_witness); emit_case();
    final_drain();
    return UNITY_END();
}
