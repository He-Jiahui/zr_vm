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

int main(void) {
    test_observation_compare_checks_event_order();
    test_observation_compare_checks_exception_and_bits();
    test_runner_and_coverage_fail_closed();
    test_fallback_preserves_semantics_but_not_native_coverage();
    puts("ssa differential harness PASS");
    return EXIT_SUCCESS;
}
