#include "perf_backend_metrics.h"

#include "perf_statistics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static size_t zr_perf_metrics_bounded_length(const TZrChar *text, size_t capacity) {
    size_t length = 0u;

    if (text == ZR_NULL) {
        return capacity;
    }
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static TZrBool zr_perf_metrics_text_is_valid(const TZrChar *text, size_t capacity,
                                             TZrBool required) {
    size_t index;
    const size_t length = zr_perf_metrics_bounded_length(text, capacity);

    if (length == capacity || (required != ZR_FALSE && length == 0u)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < length; ++index) {
        const unsigned char value = (unsigned char)text[index];
        if (value < 0x20u || value > 0x7eu) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void zr_perf_metrics_set_status(const SZrPerfBackendMetrics *sample,
                                       EZrPerfMetricsStatus status) {
    /* The public validation API is const so callers can validate read-only reports. The
     * status is a derived diagnostic field and is the only field changed by validation. */
    if (sample != ZR_NULL) {
        ((SZrPerfBackendMetrics *)(void *)sample)->status = status;
    }
}

static void zr_perf_comparison_set_status(SZrPerfComparison *result,
                                          EZrPerfComparisonStatus status,
                                          const TZrChar *reason) {
    if (result == ZR_NULL) {
        return;
    }
    result->status = status;
    result->comparable = (status == ZR_PERF_COMPARISON_COMPARABLE ||
                          status == ZR_PERF_COMPARISON_NO_GAIN ||
                          status == ZR_PERF_COMPARISON_REGRESSION)
                             ? ZR_TRUE
                             : ZR_FALSE;
    result->gateEligible = ZR_FALSE;
    if (reason != ZR_NULL) {
        (void)snprintf(result->reason, sizeof(result->reason), "%s", reason);
    }
}

void ZrTests_Perf_MetricsInit(SZrPerfBackendMetrics *sample) {
    if (sample == ZR_NULL) {
        return;
    }
    memset(sample, 0, sizeof(*sample));
    sample->requestedBackend = ZR_PERF_BACKEND_UNKNOWN;
    sample->actualBackend = ZR_PERF_BACKEND_UNKNOWN;
    sample->phase = ZR_PERF_MEASUREMENT_PHASE_UNKNOWN;
    sample->status = ZR_PERF_METRICS_STATUS_INVALID;
}

const TZrChar *ZrTests_Perf_BackendName(EZrPerfBackend backend) {
    switch (backend) {
        case ZR_PERF_BACKEND_INTERPRETER:
            return "zr_interp";
        case ZR_PERF_BACKEND_EXEC_BC:
            return "execbc";
        case ZR_PERF_BACKEND_AOT_C:
            return "aot_c";
        case ZR_PERF_BACKEND_AOT_LLVM:
            return "aot_llvm";
        case ZR_PERF_BACKEND_HOST_JIT:
            return "host_jit";
        case ZR_PERF_BACKEND_UNKNOWN:
        case ZR_PERF_BACKEND_COUNT:
        default:
            return "unknown";
    }
}

const TZrChar *ZrTests_Perf_MetricsStatusName(EZrPerfMetricsStatus status) {
    switch (status) {
        case ZR_PERF_METRICS_STATUS_VALID:
            return "VALID";
        case ZR_PERF_METRICS_STATUS_FALLBACK_VISIBLE:
            return "FALLBACK_VISIBLE";
        case ZR_PERF_METRICS_STATUS_INVALID:
        default:
            return "INVALID";
    }
}

const TZrChar *ZrTests_Perf_ComparisonStatusName(EZrPerfComparisonStatus status) {
    switch (status) {
        case ZR_PERF_COMPARISON_COMPARABLE:
            return "COMPARABLE";
        case ZR_PERF_COMPARISON_FALLBACK_VISIBLE:
            return "FALLBACK_VISIBLE";
        case ZR_PERF_COMPARISON_INCOMPARABLE:
            return "INCOMPARABLE";
        case ZR_PERF_COMPARISON_INCONCLUSIVE:
            return "INCONCLUSIVE";
        case ZR_PERF_COMPARISON_NO_GAIN:
            return "NO_GAIN";
        case ZR_PERF_COMPARISON_REGRESSION:
            return "REGRESSION";
        case ZR_PERF_COMPARISON_INVALID:
        default:
            return "INVALID";
    }
}

static TZrBool zr_perf_metrics_optional_time_is_valid(double value) {
    return (TZrBool)(isfinite(value) && value >= 0.0);
}

TZrBool ZrTests_Perf_ValidateMetrics(const SZrPerfBackendMetrics *sample) {
    TZrUInt32 metric;
    TZrUInt32 knownMetrics;
    TZrUInt32 index;

    if (sample == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_perf_metrics_set_status(sample, ZR_PERF_METRICS_STATUS_INVALID);
    if (sample->requestedBackend <= ZR_PERF_BACKEND_UNKNOWN ||
        sample->requestedBackend >= ZR_PERF_BACKEND_COUNT ||
        sample->actualBackend <= ZR_PERF_BACKEND_UNKNOWN ||
        sample->actualBackend >= ZR_PERF_BACKEND_COUNT ||
        sample->phase <= ZR_PERF_MEASUREMENT_PHASE_UNKNOWN ||
        sample->phase >= ZR_PERF_MEASUREMENT_PHASE_COUNT ||
        sample->processExitCode != 0 ||
        sample->sampleCount == 0u ||
        sample->sampleCount > ZR_PERF_METRICS_MAX_SAMPLES ||
        sample->checksumValid == ZR_FALSE ||
        sample->environmentValid == ZR_FALSE ||
        !zr_perf_metrics_text_is_valid(sample->checksum,
                                        sizeof(sample->checksum), ZR_TRUE) ||
        !zr_perf_metrics_text_is_valid(sample->expectedChecksum,
                                        sizeof(sample->expectedChecksum), ZR_TRUE) ||
        !zr_perf_metrics_text_is_valid(sample->environmentFingerprint,
                                        sizeof(sample->environmentFingerprint), ZR_TRUE) ||
        !zr_perf_metrics_text_is_valid(sample->workload,
                                        sizeof(sample->workload), ZR_TRUE) ||
        strcmp(sample->checksum, sample->expectedChecksum) != 0 ||
        !zr_perf_metrics_optional_time_is_valid(sample->elapsedMs)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < sample->sampleCount; ++index) {
        if (!zr_perf_metrics_optional_time_is_valid(sample->samples[index])) {
            return ZR_FALSE;
        }
    }

    knownMetrics = 0u;
    for (metric = 0u; metric < 16u; ++metric) {
        knownMetrics |= (TZrUInt32)1u << metric;
    }
    if ((sample->availableMetrics & ~knownMetrics) != 0u) {
        return ZR_FALSE;
    }

    if ((sample->availableMetrics & ZR_PERF_METRIC_COMPILE_TIME) != 0u &&
        !zr_perf_metrics_optional_time_is_valid(sample->compileMs)) {
        return ZR_FALSE;
    }
    if ((sample->availableMetrics & ZR_PERF_METRIC_LINK_TIME) != 0u &&
        !zr_perf_metrics_optional_time_is_valid(sample->linkMs)) {
        return ZR_FALSE;
    }
    if ((sample->availableMetrics & ZR_PERF_METRIC_LOAD_TIME) != 0u &&
        !zr_perf_metrics_optional_time_is_valid(sample->loadMs)) {
        return ZR_FALSE;
    }
    if ((sample->availableMetrics & ZR_PERF_METRIC_STARTUP_TIME) != 0u &&
        !zr_perf_metrics_optional_time_is_valid(sample->startupMs)) {
        return ZR_FALSE;
    }
    if ((sample->availableMetrics & ZR_PERF_METRIC_STEADY_STATE_TIME) != 0u &&
        !zr_perf_metrics_optional_time_is_valid(sample->steadyStateMs)) {
        return ZR_FALSE;
    }
    if ((sample->availableMetrics & ZR_PERF_METRIC_NATIVE_COVERAGE) != 0u &&
        (!isfinite(sample->nativeCoverage) || sample->nativeCoverage < 0.0 ||
         sample->nativeCoverage > 1.0)) {
        return ZR_FALSE;
    }

    if (sample->actualBackend != sample->requestedBackend) {
        zr_perf_metrics_set_status(sample, ZR_PERF_METRICS_STATUS_FALLBACK_VISIBLE);
    } else {
        zr_perf_metrics_set_status(sample, ZR_PERF_METRICS_STATUS_VALID);
    }
    return ZR_TRUE;
}

static void zr_perf_comparison_set_inconclusive(SZrPerfComparison *result,
                                                const TZrChar *reason) {
    zr_perf_comparison_set_status(result, ZR_PERF_COMPARISON_INCONCLUSIVE, reason);
}

TZrBool ZrTests_Perf_ComparePaired(const SZrPerfBackendMetrics *baseline,
                                   const SZrPerfBackendMetrics *candidate,
                                   SZrPerfComparison *result) {
    SZrPerfStatistics baselineStatistics;
    SZrPerfStatistics candidateStatistics;
    SZrPerfBootstrapInterval baselineInterval;
    SZrPerfBootstrapInterval candidateInterval;
    const double *baselineValues;
    const double *candidateValues;
    double lowImprovement;
    double highImprovement;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(result, 0, sizeof(*result));
    result->status = ZR_PERF_COMPARISON_INVALID;
    if (!ZrTests_Perf_ValidateMetrics(baseline) ||
        !ZrTests_Perf_ValidateMetrics(candidate)) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_INVALID,
                                      "invalid sample");
        return ZR_FALSE;
    }

    result->baselineSampleCount = baseline->sampleCount;
    result->candidateSampleCount = candidate->sampleCount;
    result->fallbackCount = baseline->actualBackend != baseline->requestedBackend
                                ? 1u
                                : 0u;
    if (candidate->actualBackend != candidate->requestedBackend) {
        result->fallbackCount++;
    }
    result->deoptCount = baseline->deoptCount + candidate->deoptCount;

    if (strcmp(baseline->environmentFingerprint, candidate->environmentFingerprint) != 0) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_INCOMPARABLE,
                                      "environment fingerprint mismatch");
        return ZR_TRUE;
    }
    if (strcmp(baseline->workload, candidate->workload) != 0 ||
        strcmp(baseline->checksum, candidate->checksum) != 0) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_INCOMPARABLE,
                                      "workload or checksum mismatch");
        return ZR_TRUE;
    }
    if (result->fallbackCount != 0u) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_FALLBACK_VISIBLE,
                                      "actual backend differs from requested backend");
        return ZR_TRUE;
    }
    if (baseline->sampleCount != candidate->sampleCount ||
        baseline->sampleCount < ZR_PERF_METRICS_MIN_COMPARISON_SAMPLES) {
        zr_perf_comparison_set_inconclusive(result,
                                            "insufficient or unpaired samples");
        return ZR_TRUE;
    }

    baselineValues = baseline->samples;
    candidateValues = candidate->samples;
    if (!ZrPerfStatistics_Compute(baselineValues,
                                  baseline->sampleCount,
                                  &baselineStatistics) ||
        !ZrPerfStatistics_Compute(candidateValues,
                                  candidate->sampleCount,
                                  &candidateStatistics)) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_INVALID,
                                      "statistics computation failed");
        return ZR_FALSE;
    }
    result->baselineMedian = baselineStatistics.median;
    result->candidateMedian = candidateStatistics.median;
    result->baselineCoefficientOfVariation =
            baselineStatistics.coefficientOfVariation;
    result->candidateCoefficientOfVariation =
            candidateStatistics.coefficientOfVariation;
    if (baselineStatistics.median <= 0.0 ||
        !isfinite(baselineStatistics.median) ||
        !isfinite(candidateStatistics.median) ||
        baselineStatistics.coefficientOfVariation >
                ZR_PERF_METRICS_MAX_COEFFICIENT_OF_VARIATION ||
        candidateStatistics.coefficientOfVariation >
                ZR_PERF_METRICS_MAX_COEFFICIENT_OF_VARIATION) {
        zr_perf_comparison_set_inconclusive(result,
                                            "variance too high or zero baseline");
        return ZR_TRUE;
    }

    result->improvement =
            (result->baselineMedian - result->candidateMedian) /
            result->baselineMedian;
    result->improvementLow = result->improvement;
    result->improvementHigh = result->improvement;
    if (ZrPerfStatistics_BootstrapMedian95(baselineValues,
                                           baseline->sampleCount,
                                           UINT64_C(0x5a17),
                                           1000u,
                                           &baselineInterval) &&
        ZrPerfStatistics_BootstrapMedian95(candidateValues,
                                           candidate->sampleCount,
                                           UINT64_C(0xa51c),
                                           1000u,
                                           &candidateInterval) &&
        baselineInterval.low > 0.0) {
        lowImprovement =
                (baselineInterval.low - candidateInterval.high) /
                baselineInterval.low;
        highImprovement =
                (baselineInterval.high - candidateInterval.low) /
                baselineInterval.high;
        if (isfinite(lowImprovement) && isfinite(highImprovement)) {
            result->improvementLow = lowImprovement;
            result->improvementHigh = highImprovement;
        }
    }

    /* A median improvement is only a candidate signal.  Promote it to the
     * merge gate when the conservative lower confidence bound also clears the
     * threshold; this prevents a small, noisy sample from being reported as a
     * real gain.  If bootstrap was unavailable, improvementLow is the raw
     * estimate and the caller still gets a deterministic result. */
    if (result->improvementLow >= ZR_PERF_METRICS_MIN_IMPROVEMENT) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_COMPARABLE,
                                      "stable lower confidence bound meets 3 percent gate");
        result->gateEligible = ZR_TRUE;
    } else if (result->improvement < 0.0) {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_REGRESSION,
                                      "candidate regressed");
    } else {
        zr_perf_comparison_set_status(result,
                                      ZR_PERF_COMPARISON_NO_GAIN,
                                      "improvement is below 3 percent gate");
    }
    return ZR_TRUE;
}
