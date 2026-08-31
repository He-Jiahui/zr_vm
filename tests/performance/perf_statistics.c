#include "perf_statistics.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int zr_perf_statistics_compare_double(const void *left, const void *right) {
    const double leftValue = *(const double *)left;
    const double rightValue = *(const double *)right;
    return leftValue < rightValue ? -1 : (leftValue > rightValue ? 1 : 0);
}

static double zr_perf_statistics_sorted_median(const double *sortedValues, size_t count) {
    const size_t middle = count / 2U;
    if ((count % 2U) == 0U) {
        const double sum = sortedValues[middle - 1U] + sortedValues[middle];
        return isfinite(sum) ? sum / 2.0
                             : sortedValues[middle - 1U] / 2.0 + sortedValues[middle] / 2.0;
    }
    return sortedValues[middle];
}

static uint64_t zr_perf_statistics_splitmix64_next(uint64_t *state) {
    uint64_t value;

    *state += UINT64_C(0x9e3779b97f4a7c15);
    value = *state;
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

int ZrPerfStatistics_Compute(const double *values, size_t count, SZrPerfStatistics *statistics) {
    double *scratch;
    double maximumValue = 0.0;
    double normalizedSum = 0.0;
    double deviationScale = 0.0;
    double scaledDeviationSquares = 0.0;
    size_t index;

    if (values == NULL || count == 0U || count > ZR_PERF_MAX_TOTAL_SAMPLES || statistics == NULL ||
        count > SIZE_MAX / sizeof(*scratch)) {
        return 0;
    }
    memset(statistics, 0, sizeof(*statistics));
    scratch = (double *)malloc(count * sizeof(*scratch));
    if (scratch == NULL) {
        return 0;
    }
    for (index = 0U; index < count; index++) {
        if (!isfinite(values[index]) || values[index] < 0.0) {
            free(scratch);
            return 0;
        }
        if (values[index] > maximumValue) {
            maximumValue = values[index];
        }
        scratch[index] = values[index];
    }
    if (maximumValue > 0.0) {
        for (index = 0U; index < count; index++) {
            normalizedSum += values[index] / maximumValue;
        }
        statistics->mean = (normalizedSum / (double)count) * maximumValue;
    }
    qsort(scratch, count, sizeof(*scratch), zr_perf_statistics_compare_double);
    statistics->median = zr_perf_statistics_sorted_median(scratch, count);

    for (index = 0U; index < count; index++) {
        const double deviation = fabs(values[index] - statistics->mean);
        if (deviation > 0.0) {
            if (deviationScale < deviation) {
                const double ratio = deviationScale / deviation;
                scaledDeviationSquares = 1.0 + scaledDeviationSquares * ratio * ratio;
                deviationScale = deviation;
            } else {
                const double ratio = deviation / deviationScale;
                scaledDeviationSquares += ratio * ratio;
            }
        }
        scratch[index] = fabs(values[index] - statistics->median);
    }
    qsort(scratch, count, sizeof(*scratch), zr_perf_statistics_compare_double);
    statistics->mad = zr_perf_statistics_sorted_median(scratch, count);
    statistics->sampleStddev = count > 1U && deviationScale > 0.0
                                       ? deviationScale * sqrt(scaledDeviationSquares / (double)(count - 1U))
                                       : 0.0;
    if (statistics->mean != 0.0) {
        statistics->coefficientOfVariation = statistics->sampleStddev / statistics->mean;
    } else if (statistics->sampleStddev != 0.0) {
        statistics->coefficientOfVariation = INFINITY;
    }
    free(scratch);
    return 1;
}

int ZrPerfStatistics_BootstrapMedian95(const double *values,
                                       size_t count,
                                       uint64_t seed,
                                       size_t resampleCount,
                                       SZrPerfBootstrapInterval *interval) {
    double *resample;
    double *medians;
    uint64_t state = seed;
    size_t bootstrapIndex;
    size_t valueIndex;
    size_t lowIndex;
    size_t highIndex;

    if (values == NULL || count == 0U || count > ZR_PERF_MAX_TOTAL_SAMPLES || resampleCount == 0U || interval == NULL ||
        count > SIZE_MAX / sizeof(*resample) || resampleCount > SIZE_MAX / sizeof(*medians)) {
        return 0;
    }
    for (valueIndex = 0U; valueIndex < count; valueIndex++) {
        if (!isfinite(values[valueIndex]) || values[valueIndex] < 0.0) {
            return 0;
        }
    }
    resample = (double *)malloc(count * sizeof(*resample));
    medians = (double *)malloc(resampleCount * sizeof(*medians));
    if (resample == NULL || medians == NULL) {
        free(resample);
        free(medians);
        return 0;
    }
    for (bootstrapIndex = 0U; bootstrapIndex < resampleCount; bootstrapIndex++) {
        for (valueIndex = 0U; valueIndex < count; valueIndex++) {
            const size_t sourceIndex = (size_t)(zr_perf_statistics_splitmix64_next(&state) % (uint64_t)count);
            resample[valueIndex] = values[sourceIndex];
        }
        qsort(resample, count, sizeof(*resample), zr_perf_statistics_compare_double);
        medians[bootstrapIndex] = zr_perf_statistics_sorted_median(resample, count);
    }
    qsort(medians, resampleCount, sizeof(*medians), zr_perf_statistics_compare_double);
    lowIndex = (size_t)floor((double)(resampleCount - 1U) * 0.025);
    highIndex = (size_t)floor((double)(resampleCount - 1U) * 0.975);
    interval->seed = seed;
    interval->resampleCount = resampleCount;
    interval->low = medians[lowIndex];
    interval->high = medians[highIndex];
    free(resample);
    free(medians);
    return 1;
}

EZrPerfStability ZrPerfStatistics_Classify(const SZrPerfStatistics *statistics,
                                           size_t sampleCount,
                                           size_t initialSampleCount,
                                           size_t maxExtraSampleCount,
                                           double maximumCoefficientOfVariation) {
    size_t maximumSampleCount;

    if (statistics == NULL || initialSampleCount == 0U || sampleCount < initialSampleCount ||
        sampleCount > ZR_PERF_MAX_TOTAL_SAMPLES || initialSampleCount > ZR_PERF_MAX_TOTAL_SAMPLES ||
        maxExtraSampleCount > ZR_PERF_MAX_TOTAL_SAMPLES - initialSampleCount ||
        maximumCoefficientOfVariation < 0.0 || !isfinite(maximumCoefficientOfVariation)) {
        return ZR_PERF_STABILITY_INVALID;
    }
    if (statistics->coefficientOfVariation <= maximumCoefficientOfVariation) {
        return ZR_PERF_STABILITY_STABLE;
    }
    maximumSampleCount = initialSampleCount > SIZE_MAX - maxExtraSampleCount
                                 ? SIZE_MAX
                                 : initialSampleCount + maxExtraSampleCount;
    return sampleCount < maximumSampleCount ? ZR_PERF_STABILITY_NEEDS_MORE : ZR_PERF_STABILITY_UNSTABLE;
}
