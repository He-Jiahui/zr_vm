#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static TZrBool fixture_runner(const SZrSsaFixture *fixture,
                              TZrUInt32 backend,
                              SZrSsaObservation *observation) {
    (void)fixture;
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    observation->resultBits = UINT64_C(42);
    observation->completed = ZR_TRUE;
    expect_true(ZrTests_Ssa_ObservationAppendEvent(
                        observation,
                        ZR_SSA_EVENT_GET,
                        10u,
                        1u,
                        0u),
                "runner could not append get event");
    expect_true(ZrTests_Ssa_ObservationAppendEvent(
                        observation,
                        ZR_SSA_EVENT_WRITEBACK,
                        11u,
                        2u,
                        0u),
                "runner could not append writeback event");
    expect_true(ZrTests_Ssa_ObservationAppendEvent(
                        observation,
                        ZR_SSA_EVENT_DROP,
                        12u,
                        3u,
                        0u),
                "runner could not append drop event");
    return ZR_TRUE;
}

static TZrBool fallback_fixture_runner(const SZrSsaFixture *fixture,
                                       TZrUInt32 backend,
                                       SZrSsaObservation *observation) {
    if (!fixture_runner(fixture, backend, observation)) return ZR_FALSE;
    observation->backend = 0u;
    observation->fallbackVisible = ZR_TRUE;
    return ZR_TRUE;
}

static void test_observation_compare_checks_event_order(void) {
    SZrSsaObservation expected;
    SZrSsaObservation actual;
    SZrSsaDiffDiagnostic diagnostic;

    expect_true(fixture_runner(ZR_NULL, 1u, &expected), "expected runner failed");
    actual = expected;
    actual.events[1u].kind = ZR_SSA_EVENT_DROP;
    actual.events[2u].kind = ZR_SSA_EVENT_WRITEBACK;
    expect_true(!ZrTests_Ssa_Compare(&expected, &actual, &diagnostic),
                "event reorder was accepted");
    expect_true(diagnostic.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
                    diagnostic.eventIndex == 1u,
                "event reorder diagnostic did not identify the first mismatch");
}

static void test_observation_compare_checks_exception_and_bits(void) {
    SZrSsaObservation expected;
    SZrSsaObservation actual;
    SZrSsaDiffDiagnostic diagnostic;

    expect_true(fixture_runner(ZR_NULL, 1u, &expected), "expected runner failed");
    actual = expected;
    actual.resultBits++;
    expect_true(!ZrTests_Ssa_Compare(&expected, &actual, &diagnostic),
                "result bit pattern mismatch was accepted");
    expect_true(diagnostic.reason == ZR_SSA_DIFF_RESULT_MISMATCH,
                "result mismatch diagnostic was not specific");

    actual = expected;
    actual.hasException = ZR_TRUE;
    actual.exceptionType = 99u;
    actual.exceptionSourceId = 77u;
    expect_true(!ZrTests_Ssa_Compare(&expected, &actual, &diagnostic),
                "exception mismatch was accepted");
    expect_true(diagnostic.reason == ZR_SSA_DIFF_EXCEPTION_MISMATCH,
                "exception mismatch diagnostic was not specific");
}

static void test_runner_and_coverage_fail_closed(void) {
    SZrSsaFixture fixture;
    SZrSsaObservation observation;
    SZrSsaCoverage coverage;
    SZrSsaDiffDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.name = "minimal";
    fixture.requiredBackends = ((TZrUInt32)1u << 0u) | ((TZrUInt32)1u << 1u);
    fixture.runner = fixture_runner;
    expect_true(ZrTests_Ssa_RunFixture(&fixture, 1u, &observation, &diagnostic),
                "fixture runner did not produce an observation");
    expect_true(observation.backend == 1u, "runner backend identity was lost");

    ZrTests_Ssa_CoverageInit(&coverage, fixture.requiredBackends);
    observation.backend = 0u;
    ZrTests_Ssa_CoverageRecord(&coverage, 0u, &observation, ZR_TRUE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage),
                "incomplete backend coverage was accepted");
    observation.backend = 1u;
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &observation, ZR_FALSE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage),
                "missing required backend was accepted");
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &observation, ZR_TRUE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage),
                "a previously failed backend was silently cleared");

    fixture.runner = ZR_NULL;
    expect_true(!ZrTests_Ssa_RunFixture(&fixture, 1u, &observation, &diagnostic),
                "fixture without runner returned fake success");
    expect_true(diagnostic.reason == ZR_SSA_DIFF_BACKEND_UNSUPPORTED,
                "unsupported backend diagnostic was lost");
}

static void test_fallback_preserves_semantics_but_not_native_coverage(void) {
    SZrSsaFixture fixture;
    SZrSsaObservation baseline, fallback, native;
    SZrSsaCoverage coverage;
    SZrSsaDiffDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.name = "fallback";
    fixture.requiredBackends = ((TZrUInt32)1u << 1u);
    fixture.runner = fallback_fixture_runner;
    expect_true(fixture_runner(&fixture, 0u, &baseline), "baseline runner failed");
    expect_true(ZrTests_Ssa_RunFixture(&fixture, 1u, &fallback, &diagnostic) &&
                    fallback.fallbackVisible && fallback.backend == 0u &&
                    ZrTests_Ssa_Compare(&baseline, &fallback, &diagnostic),
                "visible fallback lost its valid semantic observation");
    ZrTests_Ssa_CoverageInit(&coverage, fixture.requiredBackends);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &fallback, ZR_TRUE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage) &&
                    coverage.executedBackends == ((TZrUInt32)1u << 1u) &&
                    coverage.failedBackends == ((TZrUInt32)1u << 1u),
                "fallback was counted as native coverage");

    fixture.runner = fixture_runner;
    expect_true(ZrTests_Ssa_RunFixture(&fixture, 1u, &native, &diagnostic),
                "native fixture failed");
    ZrTests_Ssa_CoverageInit(&coverage, fixture.requiredBackends);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &native, ZR_TRUE);
    expect_true(ZrTests_Ssa_CoverageComplete(&coverage),
                "matching actual backend was not counted");
    native.fallbackVisible = ZR_TRUE;
    ZrTests_Ssa_CoverageInit(&coverage, fixture.requiredBackends);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &native, ZR_TRUE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage),
                "a declared fallback to the requested backend was counted as native");
    native.fallbackVisible = ZR_FALSE;
    native.backend = 0u;
    ZrTests_Ssa_CoverageInit(&coverage, fixture.requiredBackends);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &native, ZR_TRUE);
    expect_true(!ZrTests_Ssa_CoverageComplete(&coverage),
                "a different actual backend was counted as requested coverage");
}

static void expect_diagnostic(const SZrSsaDiffDiagnostic *expected,
                              const SZrSsaDiffDiagnostic *actual,
                              const char *message) {
    expect_true(actual->reason == expected->reason &&
                    actual->eventIndex == expected->eventIndex &&
                    actual->expectedEvent == expected->expectedEvent &&
                    actual->actualEvent == expected->actualEvent &&
                    actual->expectedSourceId == expected->expectedSourceId &&
                    actual->actualSourceId == expected->actualSourceId,
                message);
}

static void expect_difference(const SZrSsaObservation *expected,
                              const SZrSsaObservation *actual,
                              const SZrSsaDiffDiagnostic *wanted,
                              const char *message) {
    SZrSsaDiffDiagnostic diagnostic;

    expect_true(ZrTests_Ssa_ObservationValidate(expected) &&
                    ZrTests_Ssa_ObservationValidate(actual),
                "field injection produced a malformed observation");
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    expect_true(!ZrTests_Ssa_Compare(expected, actual, &diagnostic), message);
    expect_diagnostic(wanted, &diagnostic, message);
}

static void comparable_observation(SZrSsaObservation *observation) {
    const SZrSsaDiffDiagnostic clear = {
            ZR_SSA_DIFF_NONE, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u};
    SZrSsaDiffDiagnostic diagnostic;

    expect_true(fixture_runner(ZR_NULL, 1u, observation), "baseline runner failed");
    /* Zero exception metadata permits toggling each exception field alone. */
    observation->hasException = ZR_TRUE;
    expect_true(ZrTests_Ssa_ObservationValidate(observation), "invalid baseline");
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    expect_true(ZrTests_Ssa_Compare(observation, observation, &diagnostic),
                "equal observations were rejected");
    expect_diagnostic(&clear, &diagnostic, "equal observation left a diagnostic");
}

static void test_observation_compare_isolates_scalar_fields(void) {
    enum Field {
        COMPLETED, RESULT_TYPE, RESULT_BITS, RESULT_AUXILIARY,
        HAS_EXCEPTION, EXCEPTION_TYPE, EXCEPTION_SOURCE, DROPPED, WRITEBACK
    };
    static const struct {
        const char *name;
        enum Field field;
        SZrSsaDiffDiagnostic diagnostic;
    } cases[] = {
            {"completed", COMPLETED,
             {ZR_SSA_DIFF_RESULT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"resultType", RESULT_TYPE,
             {ZR_SSA_DIFF_RESULT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"resultBits", RESULT_BITS,
             {ZR_SSA_DIFF_RESULT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"resultAuxiliary", RESULT_AUXILIARY,
             {ZR_SSA_DIFF_RESULT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"hasException", HAS_EXCEPTION,
             {ZR_SSA_DIFF_EXCEPTION_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"exceptionType", EXCEPTION_TYPE,
             {ZR_SSA_DIFF_EXCEPTION_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"exceptionSourceId", EXCEPTION_SOURCE,
             {ZR_SSA_DIFF_EXCEPTION_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 1u}},
            {"droppedCount", DROPPED,
             {ZR_SSA_DIFF_EFFECT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}},
            {"writebackCount", WRITEBACK,
             {ZR_SSA_DIFF_EFFECT_MISMATCH, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u}}
    };
    SZrSsaObservation expected, actual;
    size_t index;

    comparable_observation(&expected);
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        actual = expected;
        switch (cases[index].field) {
            case COMPLETED: actual.completed = ZR_FALSE; break;
            case RESULT_TYPE: actual.resultType = ZR_SSA_RESULT_FLOAT; break;
            case RESULT_BITS: actual.resultBits++; break;
            case RESULT_AUXILIARY: actual.resultAuxiliary++; break;
            case HAS_EXCEPTION: actual.hasException = ZR_FALSE; break;
            case EXCEPTION_TYPE: actual.exceptionType++; break;
            case EXCEPTION_SOURCE: actual.exceptionSourceId++; break;
            case DROPPED: actual.droppedCount++; break;
            case WRITEBACK: actual.writebackCount++; break;
        }
        expect_difference(&expected, &actual, &cases[index].diagnostic, cases[index].name);
    }
}

static void test_observation_compare_checks_event_count_boundaries(void) {
    const SZrSsaDiffDiagnostic shorter = {
            ZR_SSA_DIFF_EVENT_MISMATCH, 2u,
            ZR_SSA_EVENT_DROP, ZR_SSA_EVENT_INVALID, 0u, 0u};
    const SZrSsaDiffDiagnostic longer = {
            ZR_SSA_DIFF_EVENT_MISMATCH, 3u,
            ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_RETURN, 0u, 0u};
    SZrSsaObservation expected, actual;

    comparable_observation(&expected);
    actual = expected;
    actual.eventCount--;
    expect_difference(&expected, &actual, &shorter, "shorter event list");

    actual = expected;
    expect_true(ZrTests_Ssa_ObservationAppendEvent(
                        &actual, ZR_SSA_EVENT_RETURN, 13u, UINT64_C(42), 0u),
                "could not append valid extra event");
    expect_difference(&expected, &actual, &longer, "longer event list");
}

static void test_observation_compare_isolates_event_fields(void) {
    enum Field { KIND, SOURCE, VALUE_BITS, AUXILIARY };
    static const struct {
        const char *name;
        enum Field field;
        SZrSsaDiffDiagnostic diagnostic;
    } cases[] = {
            {"event kind", KIND,
             {ZR_SSA_DIFF_EVENT_MISMATCH, 1u, ZR_SSA_EVENT_WRITEBACK, ZR_SSA_EVENT_GET, 11u, 11u}},
            {"event sourceId", SOURCE,
             {ZR_SSA_DIFF_EVENT_MISMATCH, 1u, ZR_SSA_EVENT_WRITEBACK, ZR_SSA_EVENT_WRITEBACK, 11u, 12u}},
            {"event valueBits", VALUE_BITS,
             {ZR_SSA_DIFF_EVENT_MISMATCH, 1u, ZR_SSA_EVENT_WRITEBACK, ZR_SSA_EVENT_WRITEBACK, 11u, 11u}},
            {"event auxiliary", AUXILIARY,
             {ZR_SSA_DIFF_EVENT_MISMATCH, 1u, ZR_SSA_EVENT_WRITEBACK, ZR_SSA_EVENT_WRITEBACK, 11u, 11u}}
    };
    SZrSsaObservation expected, actual;
    size_t index;

    comparable_observation(&expected);
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        actual = expected;
        switch (cases[index].field) {
            case KIND: actual.events[1u].kind = ZR_SSA_EVENT_GET; break;
            case SOURCE: actual.events[1u].sourceId++; break;
            case VALUE_BITS: actual.events[1u].valueBits++; break;
            case AUXILIARY: actual.events[1u].auxiliary++; break;
        }
        expect_difference(&expected, &actual, &cases[index].diagnostic, cases[index].name);
    }
}

static TZrBool failed_fixture_runner(const SZrSsaFixture *fixture,
                                     TZrUInt32 backend,
                                     SZrSsaObservation *observation) {
    (void)fixture_runner(fixture, backend, observation);
    return ZR_FALSE;
}

static TZrBool malformed_fixture_runner(const SZrSsaFixture *fixture,
                                        TZrUInt32 backend,
                                        SZrSsaObservation *observation) {
    (void)fixture_runner(fixture, backend, observation);
    observation->events[1u].kind = ZR_SSA_EVENT_INVALID;
    return ZR_TRUE;
}

static void test_runner_rejects_failure_and_malformed_observations(void) {
    const SZrSsaDiffDiagnostic malformed = {
            ZR_SSA_DIFF_MALFORMED_OBSERVATION, 0u,
            ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u};
    const SZrSsaDiffDiagnostic clear = {
            ZR_SSA_DIFF_NONE, 0u, ZR_SSA_EVENT_INVALID, ZR_SSA_EVENT_INVALID, 0u, 0u};
    SZrSsaFixture fixture;
    SZrSsaObservation observation;
    SZrSsaDiffDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.name = "runner rejection";
    fixture.runner = failed_fixture_runner;
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    expect_true(!ZrTests_Ssa_RunFixture(&fixture, 1u, &observation, &diagnostic),
                "runner false was accepted");
    expect_true(ZrTests_Ssa_ObservationValidate(&observation),
                "runner false case also produced malformed output");
    expect_diagnostic(&malformed, &diagnostic, "runner false diagnostic");

    fixture.runner = malformed_fixture_runner;
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    expect_true(!ZrTests_Ssa_RunFixture(&fixture, 1u, &observation, &diagnostic),
                "runner malformed output was accepted");
    expect_true(!ZrTests_Ssa_ObservationValidate(&observation),
                "malformed runner injection was ineffective");
    expect_diagnostic(&malformed, &diagnostic, "runner malformed diagnostic");

    fixture.runner = fixture_runner;
    expect_true(ZrTests_Ssa_RunFixture(&fixture, 1u, &observation, &diagnostic),
                "valid runner retry failed");
    expect_diagnostic(&clear, &diagnostic, "valid runner retry left a diagnostic");
}

int main(void) {
    test_observation_compare_checks_event_order();
    test_observation_compare_checks_exception_and_bits();
    test_runner_and_coverage_fail_closed();
    test_fallback_preserves_semantics_but_not_native_coverage();
    test_observation_compare_isolates_scalar_fields();
    test_observation_compare_checks_event_count_boundaries();
    test_observation_compare_isolates_event_fields();
    test_runner_rejects_failure_and_malformed_observations();
    puts("ssa differential harness PASS");
    return EXIT_SUCCESS;
}
