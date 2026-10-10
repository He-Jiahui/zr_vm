#include <stdio.h>
#include <string.h>
#include "ssa_source_finalized_module_fixture.h"
#include "unity.h"
/* The six real lower phases are called with the same live inputs and normalized
 * candidate storage as the finalizer. No profile admission is substituted here. */
enum { FM_PHASES = 6 };
static unsigned g_directAttempts[FM_PHASES];
TZrBool Fm_Direct(unsigned *phase, SZrExecIrDiagnostic *d) {
    memset(g_directAttempts, 0, sizeof(g_directAttempts));
    /* Only fixed numeric bookkeeping while armed: no heap calls or assertions. */
#define PHASE(number, call)                                                                                            \
    do {                                                                                                               \
        *phase = (number);                                                                                             \
        unsigned start = ZrTests_ExecOwner_Stats().attempts;                                                           \
        TZrBool success = (call);                                                                                      \
        g_directAttempts[(number) - 1] = ZrTests_ExecOwner_Stats().attempts - start;                                   \
        if (!success)                                                                                                  \
            return ZR_FALSE;                                                                                           \
    } while (0)
    PHASE(1, ZrCore_ExecIr_VerifyModule(&g_fm.original, d));
    PHASE(2,
          ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_fm.finalized, &g_fm.context, g_fm.original.layouts, 1, 0, d));
    PHASE(3, ZrCore_ExecIr_CloneModuleReplacingFunction(&g_fm.original, 1, &g_fm.finalized, &g_fm.candidate, d));
    PHASE(4, ZrCore_ExecIr_VerifyModule(&g_fm.candidate, d));
    PHASE(5, ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_fm.candidate.functions[0], &g_fm.context,
                                                          g_fm.candidate.layouts, 1, 0, d));
    PHASE(6, ZrCore_ExecIr_CloneOwnedModule(&g_fm.candidate, &g_fm.directOwner, d));
#undef PHASE
    return ZR_TRUE;
}
static void direct_cleanup(void) {
    ZrCore_ExecIr_OwnedModuleFree(&g_fm.directOwner);
    ZrCore_ExecIr_FreeModule(&g_fm.candidate);
}
static SZrOwnerHeapEvent failed(unsigned first, unsigned last) {
    SZrOwnerHeapEvent result = {0};
    unsigned failures = 0;
    for (unsigned i = first; i < last; ++i) {
        SZrOwnerHeapEvent e = ZrTests_ExecOwner_Event(i);
        if (e.failed) {
            result = e;
            ++failures;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1, failures);
    return result;
}
static void geometry(const SZrOwnerHeapEvent *e, const SZrOwnerHeapEvent *a) {
    TEST_ASSERT_EQUAL_UINT(e->kind, a->kind);
    TEST_ASSERT_EQUAL_UINT64(e->count, a->count);
    TEST_ASSERT_EQUAL_UINT64(e->width, a->width);
    TEST_ASSERT_EQUAL_UINT64(e->bytes, a->bytes);
}
void Fm_Faults(void) {
    SZrExecIrDiagnostic d, zero = {0};
    SZrOwnerHeapStats first, done, measured;
    SZrOwnerHeapEvent trace[512];
    unsigned phase, n, completed = 0, baselinePhases[FM_PHASES], sum = 0;
    Fm_Feature();
    first = ZrTests_ExecOwner_Stats();
    memset(&d, 0xA5, sizeof(d));
    TZrBool ok = Fm_Direct(&phase, &d);
    ZrTests_ExecOwner_Disarm();
    done = ZrTests_ExecOwner_Stats();
    memcpy(baselinePhases, g_directAttempts, sizeof(baselinePhases));
    direct_cleanup();
    TEST_ASSERT_TRUE(ok);
    Fm_Diagnostic(&zero, &d);
    ZrTests_ExecOwner_AssertReleased(first.records, done.records);
    n = done.attempts - first.attempts;
    for (unsigned i = 0; i < FM_PHASES; ++i)
        sum += baselinePhases[i];
    TEST_ASSERT_EQUAL_UINT(FM_PHASES, phase);
    TEST_ASSERT_EQUAL_UINT(n, sum);
    TEST_ASSERT_GREATER_THAN_UINT(0, n);
    /* A bounded trace is admitted only if the actual complete count fits it. */
    TEST_ASSERT_LESS_OR_EQUAL_UINT(ZR_ARRAY_COUNT(trace), n);
    for (unsigned i = 0; i < n; ++i)
        trace[i] = ZrTests_ExecOwner_Event(first.attempts + i);
    first = ZrTests_ExecOwner_Stats();
    static SFmResult success, r, retry;
    static SFmObservation before, after;
    success = Fm_Call();
    measured = ZrTests_ExecOwner_Stats();
    TEST_ASSERT_TRUE(success.success);
    Fm_Diagnostic(&zero, &success.diagnostic);
    TEST_ASSERT_EQUAL_UINT(n, measured.attempts - first.attempts);
    for (unsigned i = 0; i < n; ++i) {
        SZrOwnerHeapEvent e = ZrTests_ExecOwner_Event(first.attempts + i);
        geometry(&trace[i], &e);
    }
    /* Each pair failure costs at most two complete traces; each healthy retry one.
     * Existing producer/prerequisite records/events are included in the bound.
     * malloc/calloc/realloc callbacks create at most one record per attempt. */
    TZrUInt64 extra = (TZrUInt64) 3 * n * n + n; /* final actual copy comparison is bounded by one trace */
    TEST_ASSERT_TRUE((TZrUInt64) measured.records + extra <= 32768u);
    TEST_ASSERT_TRUE((TZrUInt64) measured.attempts + extra <= 65536u);
    for (unsigned site = 1; site <= n; ++site) {
        SZrExecIrDiagnostic witness;
        SZrOwnerHeapEvent lowerEvent, actualEvent;
        Fm_Observe(&before);
        first = ZrTests_ExecOwner_Stats();
        memset(&witness, 0xA5, sizeof(witness));
        ZrTests_ExecOwner_Arm(site);
        ok = Fm_Direct(&phase, &witness);
        ZrTests_ExecOwner_Disarm();
        done = ZrTests_ExecOwner_Stats();
        direct_cleanup();
        Fm_Observe(&after);
        TEST_ASSERT_LESS_OR_EQUAL_UINT(n, done.attempts - first.attempts);
        TEST_ASSERT_FALSE(ok);
        unsigned intervalEnd = 0, expectedPhase = 0, observedAttempts = 0;
        for (unsigned i = 0; i < FM_PHASES; ++i) {
            unsigned intervalStart = intervalEnd;
            intervalEnd += baselinePhases[i];
            if (site > intervalStart && site <= intervalEnd)
                expectedPhase = i + 1;
            observedAttempts += g_directAttempts[i];
        }
        TEST_ASSERT_GREATER_THAN_UINT(0, expectedPhase);
        TEST_ASSERT_EQUAL_UINT(expectedPhase, phase);
        TEST_ASSERT_EQUAL_UINT(done.attempts - first.attempts, observedAttempts);
        for (unsigned i = 0; i < FM_PHASES; ++i) {
            if (i + 1 < expectedPhase)
                TEST_ASSERT_EQUAL_UINT(baselinePhases[i], g_directAttempts[i]);
            if (i + 1 > expectedPhase)
                TEST_ASSERT_EQUAL_UINT(0, g_directAttempts[i]);
        }
        TEST_ASSERT_EQUAL_UINT(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, witness.code);
        Fm_Unchanged(&before, &after, ZR_TRUE);
        ZrTests_ExecOwner_AssertReleased(first.records, done.records);
        lowerEvent = failed(first.attempts, done.attempts);
        r = Fm_CallWithFault(site);
        TEST_ASSERT_LESS_OR_EQUAL_UINT(n, r.after.heap.attempts - r.before.heap.attempts);
        actualEvent = failed(r.before.heap.attempts, r.after.heap.attempts);
        Fm_Refused(&r, &witness);
        geometry(&lowerEvent, &actualEvent);
        geometry(&trace[site - 1], &actualEvent);
        TEST_ASSERT_EQUAL_UINT(0, r.after.heap.errors);
        first = ZrTests_ExecOwner_Stats();
        retry = Fm_Call();
        TEST_ASSERT_TRUE_MESSAGE(retry.success, "actual finalized primitive source module snapshot must succeed");
        Fm_Diagnostic(&zero, &retry.diagnostic);
        Fm_Unchanged(&retry.before, &retry.after, ZR_FALSE);
        TEST_ASSERT_EQUAL_UINT(n, retry.after.heap.attempts - first.attempts);
        ++completed;
    }
    TEST_ASSERT_EQUAL_UINT(n, completed);
    first = ZrTests_ExecOwner_Stats();
    Fm_Published();
    TEST_ASSERT_LESS_OR_EQUAL_UINT(n, ZrTests_ExecOwner_Stats().attempts - first.attempts);
    fprintf(stderr,
            "SOURCE_FINALIZED_FAULT_SWEEP measured=%u completed=%u original_verify=%u finalized_frame=%u whole_copy=%u "
            "candidate_verify=%u candidate_frame=%u snapshot=%u\n",
            n, completed, baselinePhases[0], baselinePhases[1], baselinePhases[2], baselinePhases[3], baselinePhases[4],
            baselinePhases[5]);
}
