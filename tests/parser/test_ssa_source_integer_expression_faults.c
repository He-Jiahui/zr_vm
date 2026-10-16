#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_source_integer_expression_faults_fixture.h"

static const char g_keeperSource[] = "return 6 + 7;";
static const char g_subjectSource[] = "return 2 + 3 * 4;";
static SZrIntegerExprEvent g_baseline[ZR_INTEGER_EXPR_TRACE];
static SZrIntegerExprObservation g_failure;
static unsigned g_baselineCount, g_baselineReady;
static unsigned g_phaseSites[ZR_INTEGER_EXPR_PHASE_COUNT];
static unsigned g_swept, g_refused, g_retried, g_accepted;

static int diagnostic_same(SZrExecIrDiagnostic a, SZrExecIrDiagnostic b) {
    return a.code == b.code && a.functionToken == b.functionToken && a.blockId == b.blockId &&
            a.instructionId == b.instructionId && a.sourceId == b.sourceId &&
            a.expectedVersion == b.expectedVersion && a.actualVersion == b.actualVersion &&
            a.expectedHash == b.expectedHash && a.actualHash == b.actualHash;
}
static int event_geometry_same(SZrIntegerExprEvent a, SZrIntegerExprEvent b) {
    return a.phase == b.phase && a.actual.kind == b.actual.kind &&
            a.actual.count == b.actual.count && a.actual.width == b.actual.width &&
            a.actual.bytes == b.actual.bytes;
}
static int aot_clear(SZrAotIrDiagnostic d) {
    return !d.status && !d.functionId && !d.blockId && !d.instructionId && !d.index && !d.expected && !d.actual;
}
static int source_clear(const SZrSourceCompileDiagnostic *d) {
    SZrExecIrDiagnostic lower = {0};
    return d->stage == ZR_SOURCE_STAGE_NONE && d->result == ZR_SOURCE_RESULT_OK &&
            !d->hasExecIr && !d->hasAotIr && !d->hasRange && diagnostic_same(d->execIr, lower) &&
            aot_clear(d->aotIr) && !d->startOffset && !d->endOffset && !d->startLine &&
            !d->startColumn && !d->endLine && !d->endColumn;
}
static int success_protocol(const SZrIntegerExprObservation *o) {
    if (o->errors || o->after.errors || o->before.failures != o->after.failures ||
            o->after.attempts <= o->before.attempts || o->callCount > ZR_INTEGER_EXPR_CALLS ||
            !source_clear(&o->source) || o->frameValuesMax <= 1u || !o->packedFrees) return 0;
    for (unsigned kind = ZR_INTEGER_EXPR_SOURCE_CALL; kind <= ZR_INTEGER_EXPR_OWNER_CALL; ++kind)
        if (o->calls[kind] != 1u) return 0;
    if (!o->calls[ZR_INTEGER_EXPR_FRAME_VALUES_CALL] || !o->calls[ZR_INTEGER_EXPR_FRAME_HEADER_CALL]) return 0;
    for (unsigned i = 0u; i < o->callCount; ++i) {
        SZrExecIrDiagnostic clear = {0};
        if (!o->call[i].result ||
                (o->call[i].hasDiagnostic && !diagnostic_same(o->call[i].diagnostic, clear))) return 0;
    }
    return 1;
}
static int failed_trace(unsigned site, const SZrIntegerExprObservation *o,
        SZrIntegerExprEvent *failure, unsigned *failedCount) {
    unsigned count;
    int good = 1;
    *failedCount = 0u;
    if (o->after.attempts < o->before.attempts) return 0;
    count = o->after.attempts - o->before.attempts;
    if (!site || site > g_baselineCount || count < site || count > ZR_INTEGER_EXPR_TRACE) return 0;
    for (unsigned i = 0u; i < count; ++i) {
        SZrIntegerExprEvent event = {{0}, 0u};
        if (ZrTests_IntegerExpr_TryTrace(i, &event) != 1) { good = 0; continue; }
        if (i < site && !event_geometry_same(event, g_baseline[i])) good = 0;
        if (event.actual.failed) {
            ++*failedCount; *failure = event;
            if (i + 1u != site) good = 0;
        }
        if (i + 1u < site && event.actual.failed) good = 0;
    }
    return good && *failedCount == 1u && failure->actual.failed == 1u &&
            o->after.failures == o->before.failures + 1u;
}
static int failure_protocol(unsigned site, const SZrIntegerExprObservation *o) {
    const SZrSourceCompileDiagnostic *d = &o->source;
    unsigned attempt, expectedStage = ZR_SOURCE_STAGE_NONE, lowerRefusals = 0u;
    if (!site || site > g_baselineCount || o->errors || o->after.errors ||
            o->calls[ZR_INTEGER_EXPR_SOURCE_CALL] != 1u || o->callCount > ZR_INTEGER_EXPR_CALLS ||
            d->result != ZR_SOURCE_RESULT_OUT_OF_MEMORY || !d->hasExecIr || d->hasAotIr ||
            d->execIr.code != ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY || !aot_clear(d->aotIr) ||
            (!d->hasRange && (d->startOffset || d->endOffset || d->startLine || d->startColumn || d->endLine || d->endColumn))) return 0;
    attempt = o->before.attempts + site - 1u;
    for (unsigned i = 0u; i < o->callCount; ++i) {
        const SZrIntegerExprCall *call = &o->call[i];
        if (call->before.attempts > attempt || call->after.attempts <= attempt) continue;
        if (call->kind == ZR_INTEGER_EXPR_BRIDGE_CALL && !expectedStage) expectedStage = ZR_SOURCE_STAGE_CONSTANT;
        if (call->kind == ZR_INTEGER_EXPR_COMPACTOR_CALL && !expectedStage) expectedStage = ZR_SOURCE_STAGE_COMPACTION;
        if (call->kind == ZR_INTEGER_EXPR_ATTACH_CALL && !expectedStage) expectedStage = ZR_SOURCE_STAGE_FRAME;
        if (call->kind == ZR_INTEGER_EXPR_FINALIZER_CALL && !expectedStage) expectedStage = ZR_SOURCE_STAGE_EXECIR_OWNER;
        if (call->hasDiagnostic && !call->result) {
            ++lowerRefusals;
            if (!diagnostic_same(call->diagnostic, d->execIr)) return 0;
        }
    }
    if (expectedStage) return (unsigned)d->stage == expectedStage && lowerRefusals != 0u;
    /* Unwrapped real Build/Binder and bool Append allocations remain visible
     * as producer-other. They still require an actual injected failure and
     * real Source/lower OOM; a generic PRODUCER_FAILED is never promoted here. */
    return g_baseline[site - 1u].phase == ZR_INTEGER_EXPR_PRODUCER_OTHER &&
            (d->stage == ZR_SOURCE_STAGE_BUILD || d->stage == ZR_SOURCE_STAGE_MODULE_CONTRACT ||
             d->stage == ZR_SOURCE_STAGE_LAYOUT || d->stage == ZR_SOURCE_STAGE_FRAME);
}
static void print_windows(const SZrIntegerExprObservation *o) {
    fprintf(stderr, " source=%u bridge=%u compactor=%u append=%u attach=%u finalizer=%u candidate=%u owner=%u frame_values=%u frame_header=%u frame_max=%u packed_free=%u observer=%u",
            o->calls[ZR_INTEGER_EXPR_SOURCE_CALL], o->calls[ZR_INTEGER_EXPR_BRIDGE_CALL],
            o->calls[ZR_INTEGER_EXPR_COMPACTOR_CALL], o->calls[ZR_INTEGER_EXPR_APPEND_CALL],
            o->calls[ZR_INTEGER_EXPR_ATTACH_CALL], o->calls[ZR_INTEGER_EXPR_FINALIZER_CALL],
            o->calls[ZR_INTEGER_EXPR_CANDIDATE_CALL], o->calls[ZR_INTEGER_EXPR_OWNER_CALL],
            o->calls[ZR_INTEGER_EXPR_FRAME_VALUES_CALL], o->calls[ZR_INTEGER_EXPR_FRAME_HEADER_CALL],
            o->frameValuesMax, o->packedFrees, o->errors);
}
static void print_calls(unsigned site, const SZrIntegerExprObservation *o) {
    /* Real before/after ledger windows and all nine diagnostics are retained in
     * raw output for independent review, including a bool API's absent payload. */
    for (unsigned i = 0u; i < o->callCount && i < ZR_INTEGER_EXPR_CALLS; ++i) {
        const SZrIntegerExprCall *c = &o->call[i];
        fprintf(stderr, "INTEGER_EXPR_WINDOW site=%u call=%u kind=%u phase=%s depth=%u result=%u has_diag=%u before_attempts=%u after_attempts=%u before_records=%u after_records=%u before_live=%u after_live=%u before_frees=%u after_frees=%u before_errors=%u after_errors=%u before_failures=%u after_failures=%u code=%u token=%u block=%u instruction=%u source_id=%u expected_version=%u actual_version=%u expected_hash=%llu actual_hash=%llu\n",
                site, i, c->kind, ZrTests_IntegerExpr_PhaseName(c->phase), c->depth,
                (unsigned)c->result, c->hasDiagnostic, c->before.attempts, c->after.attempts,
                c->before.records, c->after.records, c->before.live, c->after.live,
                c->before.frees, c->after.frees, c->before.errors, c->after.errors,
                c->before.failures, c->after.failures, (unsigned)c->diagnostic.code,
                (unsigned)c->diagnostic.functionToken, (unsigned)c->diagnostic.blockId,
                (unsigned)c->diagnostic.instructionId, (unsigned)c->diagnostic.sourceId,
                (unsigned)c->diagnostic.expectedVersion, (unsigned)c->diagnostic.actualVersion,
                (unsigned long long)c->diagnostic.expectedHash, (unsigned long long)c->diagnostic.actualHash);
    }
}
void setUp(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ZrTests_IntegerExpr_Init(),
            "unverified previous lifecycle; retain its cleanup evidence and ledger state");
}
void tearDown(void) {
    const SZrIntegerExprClose *close;
    /* Always save real pre-recovery health and drain ownership before Unity can
     * longjmp. Repeated Close preserves the first receipt and record witnesses. */
    ZrTests_IntegerExpr_Close(); close = ZrTests_IntegerExpr_CloseReport();
    TEST_ASSERT_EQUAL_UINT(1u, close->closed);
    TEST_ASSERT_EQUAL_UINT(1u, close->ledgers.closed);
    TEST_ASSERT_EQUAL_UINT(1u, close->ledgers.completed);
    TEST_ASSERT_EQUAL_UINT(1u, close->ledgers.healthyBeforeRecovery);
    TEST_ASSERT_EQUAL_UINT(0u, close->observerErrors);
    TEST_ASSERT_EQUAL_INT(1, ZrTests_IntegerExpr_ClosedHealthy());
}
static void test_integer_expression_measures_real_allocation_windows(void) {
    SZrSourceCompileDiagnostic diagnostic;
    const SZrIntegerExprObservation *o;
    SZrFunction *keeper, *subject;
    unsigned attempts;
    int complete = 1;
    ZrTests_IntegerExpr_Open();
    keeper = ZrTests_IntegerExpr_Compile(0u, g_keeperSource, &diagnostic);
    TEST_ASSERT_NOT_NULL(keeper);
    TEST_ASSERT_TRUE(success_protocol(ZrTests_IntegerExpr_Observation()));
    TEST_ASSERT_TRUE(ZrTests_IntegerExpr_CaptureKeeper());
    subject = ZrTests_IntegerExpr_Compile(1u, g_subjectSource, &diagnostic);
    TEST_ASSERT_NOT_NULL(subject);
    o = ZrTests_IntegerExpr_Observation();
    TEST_ASSERT_TRUE(success_protocol(o));
    TEST_ASSERT_TRUE(ZrTests_IntegerExpr_Published(subject, o));
    TEST_ASSERT_TRUE(ZrTests_IntegerExpr_KeeperUnchanged());
    TEST_ASSERT_TRUE(ZrTests_IntegerExpr_CrtQuiet());
    attempts = o->after.attempts - o->before.attempts;
    TEST_ASSERT_TRUE(attempts > 0u && attempts <= ZR_INTEGER_EXPR_TRACE);
    for (unsigned i = 0u; i < attempts; ++i) {
        if (ZrTests_IntegerExpr_TryTrace(i, &g_baseline[i]) != 1 || g_baseline[i].actual.failed ||
                g_baseline[i].phase == ZR_INTEGER_EXPR_NONE || g_baseline[i].phase >= ZR_INTEGER_EXPR_PHASE_COUNT) {
            complete = 0; break;
        }
        ++g_phaseSites[g_baseline[i].phase];
    }
    for (unsigned p = 1u; p < ZR_INTEGER_EXPR_PHASE_COUNT; ++p) if (!g_phaseSites[p]) complete = 0;
    fprintf(stderr, "INTEGER_EXPR_BASELINE attempts=%u keeper_live=%u complete=%d", attempts,
            ZrTests_IntegerExpr_KeeperLive(), complete);
    print_windows(o); fputc('\n', stderr);
    for (unsigned p = 1u; p < ZR_INTEGER_EXPR_PHASE_COUNT; ++p)
        fprintf(stderr, "INTEGER_EXPR_PHASE phase=%s attempts=%u\n", ZrTests_IntegerExpr_PhaseName(p), g_phaseSites[p]);
    for (unsigned i = 0u; complete && i < attempts; ++i)
        fprintf(stderr, "INTEGER_EXPR_TRACE site=%u phase=%s kind=%u count=%llu width=%llu bytes=%llu failed=%u\n",
                i + 1u, ZrTests_IntegerExpr_PhaseName(g_baseline[i].phase), g_baseline[i].actual.kind,
                (unsigned long long)g_baseline[i].actual.count, (unsigned long long)g_baseline[i].actual.width,
                (unsigned long long)g_baseline[i].actual.bytes, g_baseline[i].actual.failed);
    print_calls(0u, o);
    ZrTests_IntegerExpr_Close();
    TEST_ASSERT_EQUAL_INT(1, complete);
    TEST_ASSERT_EQUAL_INT(1, ZrTests_IntegerExpr_ClosedHealthy());
    g_baselineCount = attempts; g_baselineReady = 1u;
}
static int run_fault(unsigned site) {
    SZrSourceCompileDiagnostic diagnostic;
    SZrOwnerHeapStats before, after;
    SZrIntegerExprEvent event = {{0}, 0u};
    SZrFunction *keeper, *failed, *retry = ZR_NULL;
    unsigned failedCount = 0u;
    int armed, trace, protocol, released, kept, quiet, retryGood = 0, retryKept = 0, healthy, accepted;
    ZrTests_IntegerExpr_Open();
    keeper = ZrTests_IntegerExpr_Compile(0u, g_keeperSource, &diagnostic);
    if (!keeper || !success_protocol(ZrTests_IntegerExpr_Observation()) ||
            !ZrTests_IntegerExpr_CaptureKeeper() || !ZrTests_IntegerExpr_CrtQuiet()) {
        fprintf(stderr, "INTEGER_EXPR_SETUP site=%u keeper=0 stage=%u result=%u\n", site,
                (unsigned)diagnostic.stage, (unsigned)diagnostic.result);
        ZrTests_IntegerExpr_Close(); return 0;
    }
    before = ZrTests_ExecOwner_Stats();
    armed = ZrTests_ExecOwner_TryArm(site);
    if (armed != 1) {
        fprintf(stderr, "INTEGER_EXPR_SETUP site=%u arm=%d\n", site, armed);
        ZrTests_IntegerExpr_Close(); return 0;
    }
    failed = ZrTests_IntegerExpr_Compile(1u, g_subjectSource, &diagnostic);
    ZrTests_ExecOwner_Disarm(); /* Every retry is explicitly unarmed. */
    g_failure = *ZrTests_IntegerExpr_Observation(); after = ZrTests_ExecOwner_Stats();
    trace = failed_trace(site, &g_failure, &event, &failedCount);
    protocol = failure_protocol(site, &g_failure);
    released = ZrTests_IntegerExpr_Released(before.records, after.records);
    kept = ZrTests_IntegerExpr_KeeperUnchanged();
    quiet = ZrTests_IntegerExpr_CrtQuiet();
    if (!failed) ++g_refused;
    /* Unexpected publication remains a failure, but its genuine Function must
     * still be released before testing the healthy retry in the same Global. */
    if (failed) ZrTests_Returned_FreeFunction(1u);
    if (kept && !after.errors) {
        retry = ZrTests_IntegerExpr_Compile(1u, g_subjectSource, &diagnostic);
        retryGood = retry && success_protocol(ZrTests_IntegerExpr_Observation()) &&
                ZrTests_IntegerExpr_Published(retry, ZrTests_IntegerExpr_Observation()) &&
                ZrTests_IntegerExpr_CrtQuiet();
        retryKept = ZrTests_IntegerExpr_KeeperUnchanged();
    }
    if (retryGood) ++g_retried;
    accepted = !failed && trace && protocol && released && kept && quiet &&
            after.live == ZrTests_IntegerExpr_KeeperLive() && after.errors == 0u &&
            retryGood && retryKept;
    ZrTests_IntegerExpr_Close(); healthy = ZrTests_IntegerExpr_ClosedHealthy();
    accepted = accepted && healthy; ++g_swept; if (accepted) ++g_accepted;
    fprintf(stderr, "INTEGER_EXPR_FAULT site=%u phase=%s kind=%u count=%llu width=%llu bytes=%llu failed=%u failures=%u attempts=%u stage=%u result=%u has_exec=%u has_aot=%u has_range=%u code=%u token=%u block=%u instruction=%u source_id=%u expected_version=%u actual_version=%u expected_hash=%llu actual_hash=%llu start=%llu end=%llu start_line=%ld start_column=%ld end_line=%ld end_column=%ld null=%u trace=%d protocol=%d released=%d live=%u keeper_live=%u keeper=%d retry=%d retry_keeper=%d quiet=%d healthy=%d accepted=%d",
            site, ZrTests_IntegerExpr_PhaseName(event.phase), event.actual.kind,
            (unsigned long long)event.actual.count, (unsigned long long)event.actual.width,
            (unsigned long long)event.actual.bytes, failedCount, after.failures - before.failures,
            after.attempts - before.attempts, (unsigned)g_failure.source.stage, (unsigned)g_failure.source.result,
            (unsigned)g_failure.source.hasExecIr, (unsigned)g_failure.source.hasAotIr, (unsigned)g_failure.source.hasRange,
            (unsigned)g_failure.source.execIr.code, (unsigned)g_failure.source.execIr.functionToken,
            (unsigned)g_failure.source.execIr.blockId, (unsigned)g_failure.source.execIr.instructionId,
            (unsigned)g_failure.source.execIr.sourceId, (unsigned)g_failure.source.execIr.expectedVersion,
            (unsigned)g_failure.source.execIr.actualVersion, (unsigned long long)g_failure.source.execIr.expectedHash,
            (unsigned long long)g_failure.source.execIr.actualHash, (unsigned long long)g_failure.source.startOffset,
            (unsigned long long)g_failure.source.endOffset, (long)g_failure.source.startLine,
            (long)g_failure.source.startColumn, (long)g_failure.source.endLine, (long)g_failure.source.endColumn,
            (unsigned)(failed == ZR_NULL), trace, protocol, released, after.live,
            g_failure.before.live, kept, retryGood, retryKept, quiet, healthy, accepted);
    print_windows(&g_failure); fputc('\n', stderr); print_calls(site, &g_failure);
    return healthy && kept && retryKept;
}
static void test_integer_expression_sweeps_each_actual_allocation_and_retries(void) {
    int continued = 1;
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, g_baselineReady, "successful measured baseline is required before injection");
    for (unsigned site = 1u; site <= g_baselineCount; ++site) {
        if (site != 1u && ZrTests_IntegerExpr_Init() != 1) { continued = 0; break; }
        if (!run_fault(site)) { continued = 0; break; }
    }
    fprintf(stderr, "INTEGER_EXPR_SUMMARY baseline=%u swept=%u refused=%u retried=%u accepted=%u continued=%d\n",
            g_baselineCount, g_swept, g_refused, g_retried, g_accepted, continued);
    TEST_ASSERT_EQUAL_INT(1, continued);
    TEST_ASSERT_EQUAL_UINT(g_baselineCount, g_swept);
    TEST_ASSERT_EQUAL_UINT(g_baselineCount, g_refused);
    TEST_ASSERT_EQUAL_UINT(g_baselineCount, g_retried);
    TEST_ASSERT_EQUAL_UINT(g_baselineCount, g_accepted);
}
int main(void) {
    int result;
    SZrIntegerExprFinal final;
    UNITY_BEGIN();
    RUN_TEST(test_integer_expression_measures_real_allocation_windows);
    RUN_TEST(test_integer_expression_sweeps_each_actual_allocation_and_retries);
    result = UNITY_END(); final = ZrTests_IntegerExpr_Final();
    fprintf(stderr, "INTEGER_EXPR_FINAL lifecycles=%u closed=%u healthy=%u unhealthy=%u observer=%u owner_live=%u owner_errors=%u vm_result=%d vm_live=%u vm_errors=%u vm_diagnostics=%u vm_dropped=%u crt0_result=%d crt0_live=%u crt0_errors=%u crt1_result=%d crt1_live=%u crt1_errors=%u pre_owner_live=%u pre_owner_errors=%u pre_owner_records=%u pre_owner_frees=%u pre_crt0_result=%d pre_crt0_live=%u pre_crt0_errors=%u pre_crt1_result=%d pre_crt1_live=%u pre_crt1_errors=%u\n",
            final.lifecycles, final.closed, final.healthy, final.unhealthyLifecycles, final.observerErrors,
            final.owner.live, final.owner.errors, final.vmResult, final.vm.live, final.vm.errors,
            final.vm.diagnosticCount, final.vm.dropped, final.crtResult[0], final.crt[0].live, final.crt[0].errors,
            final.crtResult[1], final.crt[1].live, final.crt[1].errors,
            final.ownerBeforeRecovery.live, final.ownerBeforeRecovery.errors,
            final.ownerBeforeRecovery.records, final.ownerBeforeRecovery.frees,
            final.crtBeforeResult[0], final.crtBeforeRecovery[0].live, final.crtBeforeRecovery[0].errors,
            final.crtBeforeResult[1], final.crtBeforeRecovery[1].live, final.crtBeforeRecovery[1].errors);
    if (!final.closed || !final.healthy || final.unhealthyLifecycles || final.observerErrors ||
            final.owner.live || final.owner.errors || final.vmResult != 1 || final.vm.live || final.vm.errors ||
            final.vm.diagnosticCount || final.vm.dropped || final.crtResult[0] != 1 || final.crtResult[1] != 1 ||
            final.crt[0].live || final.crt[0].errors || final.crt[1].live || final.crt[1].errors ||
            final.ownerBeforeRecovery.live || final.ownerBeforeRecovery.errors ||
            final.crtBeforeResult[0] != 1 || final.crtBeforeResult[1] != 1 ||
            final.crtBeforeRecovery[0].live || final.crtBeforeRecovery[0].errors ||
            final.crtBeforeRecovery[1].live || final.crtBeforeRecovery[1].errors) return 1;
    return result;
}
