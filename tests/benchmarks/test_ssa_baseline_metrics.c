#include "perf_backend_metrics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrPerfBackendMetrics make_sample(EZrPerfBackend backend,
                                         const double *values,
                                         size_t count) {
    SZrPerfBackendMetrics sample;
    ZrTests_Perf_MetricsInit(&sample);
    sample.requestedBackend = backend;
    sample.actualBackend = backend;
    sample.phase = ZR_PERF_MEASUREMENT_PHASE_STEADY_STATE;
    sample.processExitCode = 0;
    sample.checksumValid = 1u;
    sample.environmentValid = 1u;
    snprintf(sample.checksum, sizeof(sample.checksum), "48943705");
    snprintf(sample.expectedChecksum, sizeof(sample.expectedChecksum), "48943705");
    snprintf(sample.environmentFingerprint, sizeof(sample.environmentFingerprint), "env-a");
    snprintf(sample.workload, sizeof(sample.workload), "numeric_loops");
    sample.sampleCount = (TZrUInt32)count;
    for (size_t index = 0u; index < count; ++index) {
        sample.samples[index] = values[index];
    }
    sample.elapsedMs = values[0];
    return sample;
}

static void test_valid_sample_preserves_unavailable_counters(void) {
    const double values[] = {100.0, 101.0, 99.0};
    SZrPerfBackendMetrics sample = make_sample(ZR_PERF_BACKEND_INTERPRETER, values, 3u);

    expect_true(ZrTests_Perf_ValidateMetrics(&sample), "valid sample rejected");
    expect_true(sample.status == ZR_PERF_METRICS_STATUS_VALID,
                "valid sample status is not VALID");
    expect_true((sample.availableMetrics & ZR_PERF_METRIC_INSTRUCTION_COUNT) == 0u,
                "unavailable instruction count was fabricated");
}

static void test_crash_and_checksum_mismatch_are_invalid(void) {
    const double values[] = {100.0, 101.0, 99.0};
    SZrPerfBackendMetrics sample = make_sample(ZR_PERF_BACKEND_INTERPRETER, values, 3u);

    sample.processExitCode = 7;
    expect_true(!ZrTests_Perf_ValidateMetrics(&sample),
                "non-zero process exit was accepted");
    expect_true(sample.status == ZR_PERF_METRICS_STATUS_INVALID,
                "crashed sample did not become INVALID");

    sample.processExitCode = 0;
    sample.checksum[0] = 'x';
    expect_true(!ZrTests_Perf_ValidateMetrics(&sample),
                "checksum mismatch was accepted");
    expect_true(sample.status == ZR_PERF_METRICS_STATUS_INVALID,
                "checksum mismatch did not become INVALID");
}

static void test_fallback_is_visible_and_not_gate_eligible(void) {
    const double values[] = {100.0, 101.0, 99.0};
    SZrPerfBackendMetrics sample = make_sample(ZR_PERF_BACKEND_AOT_C, values, 3u);
    SZrPerfComparison comparison;

    sample.actualBackend = ZR_PERF_BACKEND_EXEC_BC;
    expect_true(ZrTests_Perf_ValidateMetrics(&sample),
                "well-formed fallback sample rejected");
    expect_true(sample.status == ZR_PERF_METRICS_STATUS_FALLBACK_VISIBLE,
                "fallback was not visible in sample status");

    expect_true(ZrTests_Perf_ComparePaired(&sample, &sample, &comparison),
                "fallback comparison could not be classified");
    expect_true(comparison.status == ZR_PERF_COMPARISON_FALLBACK_VISIBLE,
                "fallback comparison status is not visible");
    expect_true(comparison.gateEligible == 0u,
                "fallback comparison was incorrectly gate eligible");
}

static void test_stable_paired_improvement_requires_three_percent(void) {
    const double baselineValues[] = {100.0, 100.0, 100.0};
    const double candidateValues[] = {96.0, 96.0, 96.0};
    SZrPerfBackendMetrics baseline = make_sample(ZR_PERF_BACKEND_INTERPRETER,
                                                 baselineValues,
                                                 3u);
    SZrPerfBackendMetrics candidate = make_sample(ZR_PERF_BACKEND_AOT_C,
                                                  candidateValues,
                                                  3u);
    SZrPerfComparison comparison;

    expect_true(ZrTests_Perf_ComparePaired(&baseline, &candidate, &comparison),
                "stable paired comparison failed");
    expect_true(comparison.status == ZR_PERF_COMPARISON_COMPARABLE,
                "stable paired comparison was not comparable");
    expect_true(comparison.gateEligible != 0u,
                "stable improvement did not pass gate");
    expect_true(comparison.improvement >= 0.03,
                "reported improvement is below the required threshold");
    expect_true(comparison.improvementLow >= 0.03,
                "confidence lower bound is below the required threshold");
}

static void test_marginal_gain_is_not_promoted_by_point_estimate(void) {
    const double baselineValues[] = {100.0, 101.0, 99.0};
    const double candidateValues[] = {96.0, 97.0, 95.0};
    SZrPerfBackendMetrics baseline = make_sample(ZR_PERF_BACKEND_INTERPRETER,
                                                 baselineValues,
                                                 3u);
    SZrPerfBackendMetrics candidate = make_sample(ZR_PERF_BACKEND_AOT_C,
                                                  candidateValues,
                                                  3u);
    SZrPerfComparison comparison;

    expect_true(ZrTests_Perf_ComparePaired(&baseline, &candidate, &comparison),
                "marginal paired comparison failed");
    expect_true(comparison.improvement > 0.03,
                "fixture does not exercise a point estimate above the gate");
    expect_true(comparison.improvementLow < 0.03,
                "fixture confidence bound unexpectedly clears the gate");
    expect_true(comparison.gateEligible == 0u,
                "noisy point estimate was promoted to gate eligible");
    expect_true(comparison.status == ZR_PERF_COMPARISON_NO_GAIN,
                "marginal gain was not classified as NO_GAIN");
}

static void test_unavailable_and_malformed_optional_metrics(void) {
    const double values[] = {100.0, 100.0, 100.0};
    SZrPerfBackendMetrics sample = make_sample(ZR_PERF_BACKEND_INTERPRETER,
                                               values,
                                               3u);

    sample.rssBytes = 0u;
    expect_true(ZrTests_Perf_ValidateMetrics(&sample),
                "unavailable RSS was rejected");
    sample.availableMetrics = ZR_PERF_METRIC_RSS;
    sample.rssBytes = 4096u;
    expect_true(ZrTests_Perf_ValidateMetrics(&sample),
                "available RSS was rejected");
    sample.nativeCoverage = 1.1;
    sample.availableMetrics |= ZR_PERF_METRIC_NATIVE_COVERAGE;
    expect_true(!ZrTests_Perf_ValidateMetrics(&sample),
                "out-of-range native coverage was accepted");

    ZrTests_Perf_MetricsInit(&sample);
    sample.requestedBackend = ZR_PERF_BACKEND_INTERPRETER;
    sample.actualBackend = ZR_PERF_BACKEND_INTERPRETER;
    sample.phase = ZR_PERF_MEASUREMENT_PHASE_STEADY_STATE;
    sample.processExitCode = 0;
    sample.checksumValid = 1u;
    sample.environmentValid = 1u;
    snprintf(sample.checksum, sizeof(sample.checksum), "1");
    snprintf(sample.expectedChecksum, sizeof(sample.expectedChecksum), "1");
    snprintf(sample.environmentFingerprint,
             sizeof(sample.environmentFingerprint),
             "env-a");
    snprintf(sample.workload, sizeof(sample.workload), "fixture");
    sample.sampleCount = 1u;
    sample.samples[0] = NAN;
    sample.elapsedMs = 1.0;
    expect_true(!ZrTests_Perf_ValidateMetrics(&sample),
                "non-finite sample was accepted");
}

static void test_environment_mismatch_and_noise_are_not_promoted(void) {
    const double baselineValues[] = {100.0, 101.0, 99.0};
    const double candidateValues[] = {96.0, 97.0, 95.0};
    SZrPerfBackendMetrics baseline = make_sample(ZR_PERF_BACKEND_INTERPRETER,
                                                 baselineValues,
                                                 3u);
    SZrPerfBackendMetrics candidate = make_sample(ZR_PERF_BACKEND_AOT_C,
                                                  candidateValues,
                                                  3u);
    SZrPerfComparison comparison;

    snprintf(candidate.environmentFingerprint,
             sizeof(candidate.environmentFingerprint),
             "env-b");
    expect_true(ZrTests_Perf_ComparePaired(&baseline, &candidate, &comparison),
                "environment mismatch could not be classified");
    expect_true(comparison.status == ZR_PERF_COMPARISON_INCOMPARABLE,
                "environment mismatch was promoted");
    expect_true(comparison.gateEligible == 0u,
                "incomparable samples were gate eligible");

    snprintf(candidate.environmentFingerprint,
             sizeof(candidate.environmentFingerprint),
             "env-a");
    candidate.samples[0] = 1.0;
    candidate.samples[1] = 1000.0;
    candidate.samples[2] = 1.0;
    expect_true(ZrTests_Perf_ComparePaired(&baseline, &candidate, &comparison),
                "noisy paired comparison failed");
    expect_true(comparison.status == ZR_PERF_COMPARISON_INCONCLUSIVE,
                "noisy samples were promoted");
    expect_true(comparison.gateEligible == 0u,
                "inconclusive samples were gate eligible");
}

int main(void) {
    test_valid_sample_preserves_unavailable_counters();
    test_crash_and_checksum_mismatch_are_invalid();
    test_fallback_is_visible_and_not_gate_eligible();
    test_stable_paired_improvement_requires_three_percent();
    test_marginal_gain_is_not_promoted_by_point_estimate();
    test_environment_mismatch_and_noise_are_not_promoted();
    test_unavailable_and_malformed_optional_metrics();
    puts("ssa baseline metrics PASS");
    return EXIT_SUCCESS;
}
