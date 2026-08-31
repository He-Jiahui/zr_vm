#ifndef ZR_VM_TESTS_PERFORMANCE_PERF_STATISTICS_H
#define ZR_VM_TESTS_PERFORMANCE_PERF_STATISTICS_H

#include <stddef.h>
#include <stdint.h>

#define ZR_PERF_MAX_TOTAL_SAMPLES 20U
#define ZR_PERF_MIN_GATE_SAMPLES 10U

typedef struct SZrPerfStatistics {
    double mean;
    double median;
    double mad;
    double sampleStddev;
    double coefficientOfVariation;
} SZrPerfStatistics;

typedef struct SZrPerfBootstrapInterval {
    uint64_t seed;
    size_t resampleCount;
    double low;
    double high;
} SZrPerfBootstrapInterval;

typedef enum EZrPerfStability {
    ZR_PERF_STABILITY_INVALID = 0,
    ZR_PERF_STABILITY_NEEDS_MORE,
    ZR_PERF_STABILITY_STABLE,
    ZR_PERF_STABILITY_UNSTABLE
} EZrPerfStability;

int ZrPerfStatistics_Compute(const double *values, size_t count, SZrPerfStatistics *statistics);

int ZrPerfStatistics_BootstrapMedian95(const double *values,
                                       size_t count,
                                       uint64_t seed,
                                       size_t resampleCount,
                                       SZrPerfBootstrapInterval *interval);

EZrPerfStability ZrPerfStatistics_Classify(const SZrPerfStatistics *statistics,
                                           size_t sampleCount,
                                           size_t initialSampleCount,
                                           size_t maxExtraSampleCount,
                                           double maximumCoefficientOfVariation);

#endif
