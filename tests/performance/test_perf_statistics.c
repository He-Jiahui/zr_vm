#include "perf_statistics.h"

#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ZR_TEST_TOLERANCE 1.0e-12

static void zr_test_fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void zr_test_expect_close(double actual, double expected, const char *message) {
    if (fabs(actual - expected) > ZR_TEST_TOLERANCE) {
        fprintf(stderr, "FAIL: %s (expected %.15g, got %.15g)\n", message, expected, actual);
        exit(1);
    }
}

static void zr_test_fixed_statistics(void) {
    const double values[] = {1.0, 2.0, 3.0, 4.0};
    SZrPerfStatistics statistics = {0};

    if (!ZrPerfStatistics_Compute(values, sizeof(values) / sizeof(values[0]), &statistics)) {
        zr_test_fail("fixed statistics computation failed");
    }
    zr_test_expect_close(statistics.mean, 2.5, "mean uses all values");
    zr_test_expect_close(statistics.median, 2.5, "median averages the two central values");
    zr_test_expect_close(statistics.mad, 1.0, "MAD is the median absolute deviation");
    zr_test_expect_close(statistics.sampleStddev, sqrt(5.0 / 3.0), "stddev uses n - 1");
    zr_test_expect_close(statistics.coefficientOfVariation,
                         sqrt(5.0 / 3.0) / 2.5,
                         "CV is sample stddev divided by mean");
}

static void zr_test_zero_mean_cv(void) {
    const double values[] = {0.0, 0.0, 0.0};
    SZrPerfStatistics statistics = {0};

    if (!ZrPerfStatistics_Compute(values, sizeof(values) / sizeof(values[0]), &statistics)) {
        zr_test_fail("zero-mean statistics computation failed");
    }
    zr_test_expect_close(statistics.coefficientOfVariation, 0.0, "constant zero samples have zero CV");
}

static void zr_test_large_finite_statistics(void) {
    const double equalValues[] = {DBL_MAX, DBL_MAX};
    const double spreadValues[] = {DBL_MAX / 2.0, DBL_MAX};
    SZrPerfStatistics statistics = {0};

    if (!ZrPerfStatistics_Compute(equalValues, 2U, &statistics) ||
        !isfinite(statistics.mean) || !isfinite(statistics.median) ||
        statistics.mean != DBL_MAX || statistics.median != DBL_MAX || statistics.sampleStddev != 0.0) {
        zr_test_fail("equal DBL_MAX timing samples overflowed");
    }
    if (!ZrPerfStatistics_Compute(spreadValues, 2U, &statistics) ||
        !isfinite(statistics.mean) || !isfinite(statistics.median) || !isfinite(statistics.sampleStddev)) {
        zr_test_fail("large finite timing statistics overflowed");
    }
}

static void zr_test_large_deviation_norm(void) {
    double values[ZR_PERF_MAX_TOTAL_SAMPLES];
    SZrPerfStatistics statistics = {0};
    const double expected = DBL_MAX * sqrt(5.0 / 19.0);
    size_t index;

    for (index = 0U; index < ZR_PERF_MAX_TOTAL_SAMPLES; index++) {
        values[index] = index < ZR_PERF_MAX_TOTAL_SAMPLES / 2U ? 0.0 : DBL_MAX;
    }
    if (!ZrPerfStatistics_Compute(values, ZR_PERF_MAX_TOTAL_SAMPLES, &statistics) ||
        !isfinite(statistics.sampleStddev) ||
        fabs(statistics.sampleStddev - expected) / expected > ZR_TEST_TOLERANCE) {
        zr_test_fail("large finite deviations overflowed before normalization");
    }
}

static void zr_test_subnormal_midpoint(void) {
    const double smallest = nextafter(0.0, 1.0);
    const double values[] = {smallest, smallest * 2.0};
    SZrPerfStatistics statistics = {0};

    if (smallest == 0.0 || !ZrPerfStatistics_Compute(values, 2U, &statistics)) {
        zr_test_fail("subnormal statistics computation failed");
    }
    if (statistics.median != smallest * 2.0) {
        zr_test_fail("adjacent subnormal midpoint collapsed to the lower endpoint");
    }
}

static void zr_test_sample_limit(void) {
    double values[ZR_PERF_MAX_TOTAL_SAMPLES + 1U] = {0.0};
    SZrPerfStatistics statistics = {0};

    if (ZrPerfStatistics_Compute(values, ZR_PERF_MAX_TOTAL_SAMPLES + 1U, &statistics)) {
        zr_test_fail("statistics API accepted more than 20 total samples");
    }
}

static void zr_test_deterministic_bootstrap(void) {
    const double values[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    const uint64_t seed = UINT64_C(0x123456789abcdef0);
    SZrPerfBootstrapInterval first = {0};
    SZrPerfBootstrapInterval second = {0};

    if (!ZrPerfStatistics_BootstrapMedian95(values,
                                            sizeof(values) / sizeof(values[0]),
                                            seed,
                                            10000U,
                                            &first) ||
        !ZrPerfStatistics_BootstrapMedian95(values,
                                            sizeof(values) / sizeof(values[0]),
                                            seed,
                                            10000U,
                                            &second)) {
        zr_test_fail("bootstrap computation failed");
    }
    if (first.seed != seed || first.resampleCount != 10000U) {
        zr_test_fail("bootstrap metadata did not preserve seed and resample count");
    }
    zr_test_expect_close(first.low, 3.0, "bootstrap lower percentile is deterministic");
    zr_test_expect_close(first.high, 8.0, "bootstrap upper percentile is deterministic");
    zr_test_expect_close(second.low, first.low, "same seed reproduces lower percentile");
    zr_test_expect_close(second.high, first.high, "same seed reproduces upper percentile");
}

static void zr_test_stability_classification(void) {
    const double stableValues[] = {100.0, 101.0, 99.0, 100.0, 101.0,
                                   99.0, 100.0, 101.0, 99.0, 100.0};
    const double convergingValues[] = {94.0, 106.0, 94.0, 106.0, 94.0, 106.0, 94.0, 106.0,
                                       94.0, 106.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0};
    const double unstableValues[] = {50.0, 150.0, 50.0, 150.0, 50.0, 150.0, 50.0, 150.0, 50.0, 150.0,
                                     50.0, 150.0, 50.0, 150.0, 50.0, 150.0, 50.0, 150.0, 50.0, 150.0};
    SZrPerfStatistics statistics = {0};

    if (!ZrPerfStatistics_Compute(stableValues, 10U, &statistics) ||
        ZrPerfStatistics_Classify(&statistics, 10U, 10U, 10U, 0.05) != ZR_PERF_STABILITY_STABLE) {
        zr_test_fail("stable initial samples were not accepted");
    }

    if (!ZrPerfStatistics_Compute(convergingValues, 10U, &statistics) ||
        ZrPerfStatistics_Classify(&statistics, 10U, 10U, 10U, 0.05) != ZR_PERF_STABILITY_NEEDS_MORE) {
        zr_test_fail("high initial CV did not request another sample");
    }
    if (!ZrPerfStatistics_Compute(convergingValues, 16U, &statistics) ||
        ZrPerfStatistics_Classify(&statistics, 16U, 10U, 10U, 0.05) != ZR_PERF_STABILITY_STABLE) {
        zr_test_fail("additional samples did not converge to stable");
    }

    if (!ZrPerfStatistics_Compute(unstableValues, 20U, &statistics) ||
        ZrPerfStatistics_Classify(&statistics, 20U, 10U, 10U, 0.05) != ZR_PERF_STABILITY_UNSTABLE) {
        zr_test_fail("20 noisy samples were not marked unstable");
    }
}

int main(void) {
    zr_test_fixed_statistics();
    zr_test_zero_mean_cv();
    zr_test_large_finite_statistics();
    zr_test_large_deviation_norm();
    zr_test_subnormal_midpoint();
    zr_test_sample_limit();
    zr_test_deterministic_bootstrap();
    zr_test_stability_classification();
    puts("perf statistics PASS");
    return 0;
}
