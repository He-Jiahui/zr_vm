#ifndef ZR_VM_TESTS_PERFORMANCE_PERF_STATISTICS_H
#define ZR_VM_TESTS_PERFORMANCE_PERF_STATISTICS_H

#include <stddef.h>
#include <stdint.h>

/* runner 的初始样本与追加样本共享此上限；gate 至少需要十个稳定样本。 */
#define ZR_PERF_MAX_TOTAL_SAMPLES 20U
#define ZR_PERF_MIN_GATE_SAMPLES 10U

/** @brief 非负有限耗时样本的中心、离散度和变异系数。 */
typedef struct SZrPerfStatistics {
    double mean;
    double median;
    double mad;
    double sampleStddev;
    double coefficientOfVariation;
} SZrPerfStatistics;

/** @brief 保存固定种子 Bootstrap 中位数区间及可复现参数。 */
typedef struct SZrPerfBootstrapInterval {
    uint64_t seed;
    size_t resampleCount;
    double low;
    double high;
} SZrPerfBootstrapInterval;

/** @brief 指示 runner 继续采样、接受稳定性或耗尽预算。 */
typedef enum EZrPerfStability {
    ZR_PERF_STABILITY_INVALID = 0,
    ZR_PERF_STABILITY_NEEDS_MORE,
    ZR_PERF_STABILITY_STABLE,
    ZR_PERF_STABILITY_UNSTABLE
} EZrPerfStability;

/** @brief 计算最多 20 个非负有限样本的均值、中位数、MAD 和样本标准差。 */
int ZrPerfStatistics_Compute(const double *values, size_t count, SZrPerfStatistics *statistics);

/** @brief 用固定种子重采样产生可复现的 95% 中位数区间。 */
int ZrPerfStatistics_BootstrapMedian95(const double *values,
                                       size_t count,
                                       uint64_t seed,
                                       size_t resampleCount,
                                       SZrPerfBootstrapInterval *interval);

/** @brief 按 CV 门槛和剩余样本预算给出采样状态。 */
EZrPerfStability ZrPerfStatistics_Classify(const SZrPerfStatistics *statistics,
                                           size_t sampleCount,
                                           size_t initialSampleCount,
                                           size_t maxExtraSampleCount,
                                           double maximumCoefficientOfVariation);

#endif
