#include "aot_coverage.h"
#include "aot_runner.h"
#include "perf_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(TZrBool condition, const char *message) {
    if (condition == ZR_FALSE) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

typedef struct SZrFixtureEntryState {
    TZrUInt64 checksum;
    TZrBool fail;
    SZrAotCoverageCounts coverage;
} SZrFixtureEntryState;

static TZrBool fixture_entry_invoke(void *context,
                                    TZrUInt64 *checksum,
                                    SZrAotCoverageCounts *coverage) {
    SZrFixtureEntryState *state = (SZrFixtureEntryState *)context;

    if (state == ZR_NULL || checksum == ZR_NULL || coverage == ZR_NULL || state->fail != ZR_FALSE) {
        return ZR_FALSE;
    }
    *checksum = state->checksum;
    *coverage = state->coverage;
    return ZR_TRUE;
}

static void test_coverage_keeps_semantic_denominator_and_classes(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageReport report;

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 80u;
    counts.nativeHelperSites = 10u;
    counts.interpreterSites = 10u;
    counts.deoptCount = 3u;
    counts.semanticSites = 100u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;

    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "representative coverage counts were rejected");
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "representative coverage report failed");
    expect_true(report.available != ZR_FALSE && report.semanticSites == 100u,
                "coverage denominator was not retained");
    expect_true(report.nativeCoverage > 0.799 && report.nativeCoverage < 0.801,
                "native coverage ratio was not 80 percent");
    expect_true(report.nativeExecutionCoverage > 0.899 &&
                    report.nativeExecutionCoverage < 0.901,
                "native helper execution was not included in the native execution ratio");
    expect_true(report.interpreterShare > 0.099 && report.interpreterShare < 0.101,
                "interpreter fallback share was not reported");
    expect_true(report.mixedExecution != ZR_FALSE,
                "mixed native/interpreter execution was hidden");

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 1u;
    counts.semanticSites = 5u;
    counts.executedSemanticSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "partial semantic-site coverage report failed");
    expect_true(report.semanticSites == 5u && report.executedSemanticSites == 1u &&
                    report.nativeCoverage > 0.999 && report.nativeStaticCoverage > 0.199 &&
                    report.nativeStaticCoverage < 0.201,
                "static and dynamic semantic denominators were conflated");
}

static void test_zero_coverage_is_unavailable_not_perfect(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageReport report;

    memset(&counts, 0, sizeof(counts));
    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "zero counts should be structurally valid");
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "zero counts should produce an unavailable report");
    expect_true(report.available == ZR_FALSE && report.nativeCoverage < 0.0,
                "missing coverage data was reported as 100 percent");
}

static void test_coverage_rejects_inconsistent_denominator(void) {
    SZrAotCoverageCounts counts;

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 2u;
    counts.semanticSites = 1u;
    expect_true(!ZrTests_AotCoverage_Validate(&counts),
                "coverage accepted a denominator smaller than class counts");

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 1u;
    counts.semanticSites = 5u;
    counts.executedSemanticSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "unexecuted semantic sites were rejected");
}

static void test_coverage_record_and_merge_are_overflow_safe(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageCounts other;
    TZrUInt64 original;

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = UINT64_MAX;
    original = counts.nativeSites;
    expect_true(!ZrTests_AotCoverage_Record(&counts,
                                            ZR_AOT_COVERAGE_CLASS_NATIVE,
                                            1u),
                "coverage counter overflow was accepted");
    expect_true(counts.nativeSites == original,
                "failed coverage increment partially mutated the counter");

    ZrTests_AotCoverage_Init(&counts);
    ZrTests_AotCoverage_Init(&other);
    counts.nativeSites = UINT64_MAX;
    other.nativeSites = 1u;
    expect_true(!ZrTests_AotCoverage_Merge(&counts, &other),
                "coverage merge overflow was accepted");
    expect_true(counts.nativeSites == UINT64_MAX && counts.semanticSites == 0u,
                "failed coverage merge partially mutated the destination");

    ZrTests_AotCoverage_Init(&counts);
    ZrTests_AotCoverage_Init(&other);
    counts.nativeSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    other.nativeSites = 1u;
    other.sampleRatePermille = 0u;
    expect_true(ZrTests_AotCoverage_Merge(&counts, &other),
                "mixed-rate coverage merge failed");
    expect_true(counts.sampleRatePermille == 0u,
                "unknown sample rate was incorrectly reported as exact");

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 1u;
    counts.semanticSites = 1u;
    counts.executedSemanticSites = 1u;
    expect_true(!ZrTests_AotCoverage_Record(&counts,
                                            ZR_AOT_COVERAGE_CLASS_NATIVE,
                                            1u),
                "record exceeded the declared static denominator");
    expect_true(counts.nativeSites == 1u && counts.executedSemanticSites == 1u,
                "failed denominator record partially mutated the counters");
}

static void test_runner_reports_actual_backend_and_checksum(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = UINT64_C(0x12345678);
    state.coverage.nativeSites = 4u;
    state.coverage.semanticSites = 4u;
    state.coverage.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, UINT64_C(7), "fixture-c",
                        fixture_entry_invoke, &state),
                "compiled C entry could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.entryToken = UINT64_C(7);
    request.expectedChecksum = state.checksum;
    request.checksumRequired = ZR_TRUE;
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "registered C entry did not run");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_RAN &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.entryToken == UINT64_C(7) &&
                    result.checksum == state.checksum,
                "runner lost requested/actual backend or entry token");
    expect_true(result.coverage.available != ZR_FALSE,
                "runner discarded entry coverage");
    expect_true(ZrTests_AotRunner_ValidateResult(&result),
                "valid AOT runner result failed structural validation");
}

static void test_runner_matrix_and_unsupported_backend_diagnostics(void) {
    SZrAotRunner runner;
    SZrAotRunnerMatrix matrix;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 1u;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, 12u, "c-entry",
                        fixture_entry_invoke, &state),
                "matrix C entry could not be registered");
    expect_true(ZrTests_AotRunner_DescribeMatrix(&runner, 12u, &matrix),
                "backend matrix could not be described");
    expect_true(matrix.rowCount == 3u && matrix.rows[0u].available != ZR_FALSE &&
                    matrix.rows[1u].available == ZR_FALSE,
                "backend matrix did not expose C/LLVM availability");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_UNKNOWN;
    request.entryToken = 12u;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "unknown backend returned fake success");
    expect_true(result.failure == ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND &&
                    result.status == ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                "unknown backend diagnostic was not specific");
}

static void test_runner_makes_interpreter_fallback_explicit(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 99u;
    state.coverage.interpreterSites = 2u;
    state.coverage.semanticSites = 2u;
    state.coverage.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_INTERPRETER, UINT64_C(9), "fixture-interp",
                        fixture_entry_invoke, &state),
                "interpreter fixture could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_LLVM;
    request.entryToken = UINT64_C(9);
    request.allowInterpreterFallback = ZR_TRUE;
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "explicit interpreter fallback was rejected");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FALLBACK &&
                    result.actualBackend == ZR_AOT_BACKEND_INTERPRETER &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK &&
                    result.coverage.interpreterSites == 2u,
                "interpreter fallback was not visible in runner result");
}

static void test_runner_rejects_missing_backend_and_checksum_mismatch(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 10u;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, UINT64_C(4), "fixture-c",
                        fixture_entry_invoke, &state),
                "checksum fixture could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_LLVM;
    request.entryToken = UINT64_C(4);
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "missing AOT backend returned fake success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_UNAVAILABLE &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE &&
                    result.actualBackend == ZR_AOT_BACKEND_UNKNOWN,
                "unsupported AOT backend diagnostic was lost");

    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.expectedChecksum = 11u;
    request.checksumRequired = ZR_TRUE;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "checksum mismatch returned success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FAILED &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH &&
                    result.actualBackend == ZR_AOT_BACKEND_C,
                "checksum mismatch did not preserve actual backend");
}

static void test_runner_reports_invocation_failure_without_fallback(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.fail = ZR_TRUE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, 17u, "failing-c",
                        fixture_entry_invoke, &state),
                "failing entry could not be registered");
    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.entryToken = 17u;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "failed AOT invocation returned success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FAILED &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_INVOCATION &&
                    ZrTests_AotRunner_ValidateResult(&result),
                "AOT invocation failure was not visible or valid");
}

static void test_aot_phase_report_keeps_costs_and_unavailable_values_explicit(void) {
    SZrPerfAotPhaseReport report;
    SZrPerfAotPhaseReport invalid;

    memset(&report, 0, sizeof(report));
    report.status = ZR_PERF_AOT_REPORT_RAN;
    report.processExitCode = 0;
    snprintf(report.requestedBackend, sizeof(report.requestedBackend), "aot_c");
    snprintf(report.actualBackend, sizeof(report.actualBackend), "aot_c");
    snprintf(report.entryToken, sizeof(report.entryToken), "fixture-7");
    snprintf(report.artifactHash, sizeof(report.artifactHash), "sha256:fixture");
    snprintf(report.toolchain, sizeof(report.toolchain), "gcc-11");
    report.compileMs = 2.0;
    report.linkMs = -1.0; /* unavailable, not an invented zero */
    report.loadMs = 1.0;
    report.startupMs = 3.0;
    report.runMs = 4.0;
    report.hasCodeSizeBytes = ZR_TRUE;
    report.codeSizeBytes = 128u;
    report.coverageAvailable = ZR_TRUE;
    report.semanticSites = 10u;
    report.executedSemanticSites = 10u;
    report.nativeSites = 8u;
    report.nativeHelperSites = 1u;
    report.interpreterSites = 1u;
    report.nativeCoverage = 0.8;
    expect_true(ZrPerfReport_ValidateAotPhase(&report),
                "valid AOT phase report was rejected");
    expect_true(ZrPerfReport_WriteAotJson("ssa_aot_runner_coverage_report.json", &report),
                "AOT phase report was not serialized");
    remove("ssa_aot_runner_coverage_report.json");

    invalid = report;
    invalid.actualBackend[0] = 'x';
    expect_true(!ZrPerfReport_ValidateAotPhase(&invalid),
                "requested/actual mismatch was hidden in AOT report");
}

int main(void) {
    test_coverage_keeps_semantic_denominator_and_classes();
    test_zero_coverage_is_unavailable_not_perfect();
    test_coverage_rejects_inconsistent_denominator();
    test_coverage_record_and_merge_are_overflow_safe();
    test_runner_reports_actual_backend_and_checksum();
    test_runner_matrix_and_unsupported_backend_diagnostics();
    test_runner_makes_interpreter_fallback_explicit();
    test_runner_rejects_missing_backend_and_checksum_mismatch();
    test_runner_reports_invocation_failure_without_fallback();
    test_aot_phase_report_keeps_costs_and_unavailable_values_explicit();
    puts("ssa aot runner coverage PASS");
    return EXIT_SUCCESS;
}
