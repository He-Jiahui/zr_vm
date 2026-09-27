#ifndef ZR_VM_TESTS_PERFORMANCE_PERF_BACKEND_METRICS_H
#define ZR_VM_TESTS_PERFORMANCE_PERF_BACKEND_METRICS_H

#include <stddef.h>
#include <stdint.h>

#include "zr_vm_common/zr_common_conf.h"

/* SSA 后端对比的单次样本上限与门槛；可用性位只解释显式提供的指标。 */
#define ZR_PERF_METRICS_MAX_SAMPLES 20U
#define ZR_PERF_METRICS_TEXT_CAPACITY 128U
#define ZR_PERF_METRICS_MIN_COMPARISON_SAMPLES 3U
#define ZR_PERF_METRICS_MAX_COEFFICIENT_OF_VARIATION 0.05
#define ZR_PERF_METRICS_MIN_IMPROVEMENT 0.03

/** @brief 区分请求的后端与实际执行后端，以便把回退单独报告。 */
typedef enum EZrPerfBackend {
    ZR_PERF_BACKEND_UNKNOWN = 0,
    ZR_PERF_BACKEND_INTERPRETER,
    ZR_PERF_BACKEND_EXEC_BC,
    ZR_PERF_BACKEND_AOT_C,
    ZR_PERF_BACKEND_AOT_LLVM,
    ZR_PERF_BACKEND_HOST_JIT,
    ZR_PERF_BACKEND_COUNT
} EZrPerfBackend;

/* 后端短别名保留对应枚举值；当前仓库没有直接引用。 */
#define ZR_PERF_BACKEND_AOT ZR_PERF_BACKEND_AOT_C
#define ZR_PERF_BACKEND_JIT ZR_PERF_BACKEND_HOST_JIT

/** @brief 标识一次测量覆盖的阶段，跨阶段样本不能直接比较。 */
typedef enum EZrPerfMeasurementPhase {
    ZR_PERF_MEASUREMENT_PHASE_UNKNOWN = 0,
    ZR_PERF_MEASUREMENT_PHASE_COMPILE,
    ZR_PERF_MEASUREMENT_PHASE_LINK,
    ZR_PERF_MEASUREMENT_PHASE_LOAD,
    ZR_PERF_MEASUREMENT_PHASE_STARTUP,
    ZR_PERF_MEASUREMENT_PHASE_STEADY_STATE,
    ZR_PERF_MEASUREMENT_PHASE_COUNT
} EZrPerfMeasurementPhase;

/** @brief 样本校验结果保留可见回退这一非错误状态。 */
typedef enum EZrPerfMetricsStatus {
    ZR_PERF_METRICS_STATUS_INVALID = 0,
    ZR_PERF_METRICS_STATUS_VALID,
    ZR_PERF_METRICS_STATUS_FALLBACK_VISIBLE
} EZrPerfMetricsStatus;

/** @brief availableMetrics 中的位决定相应可选字段是否有意义。 */
typedef enum EZrPerfMetricAvailability {
    ZR_PERF_METRIC_INSTRUCTION_COUNT = 1u << 0u,
    ZR_PERF_METRIC_HELPER_COUNT = 1u << 1u,
    ZR_PERF_METRIC_ALLOCATION_COUNT = 1u << 2u,
    ZR_PERF_METRIC_GC_WORK = 1u << 3u,
    ZR_PERF_METRIC_GC_PAUSE = 1u << 4u,
    ZR_PERF_METRIC_FRAME_LATENCY = 1u << 5u,
    ZR_PERF_METRIC_RSS = 1u << 6u,
    ZR_PERF_METRIC_CODE_SIZE = 1u << 7u,
    ZR_PERF_METRIC_COMPILE_TIME = 1u << 8u,
    ZR_PERF_METRIC_LINK_TIME = 1u << 9u,
    ZR_PERF_METRIC_LOAD_TIME = 1u << 10u,
    ZR_PERF_METRIC_STARTUP_TIME = 1u << 11u,
    ZR_PERF_METRIC_STEADY_STATE_TIME = 1u << 12u,
    ZR_PERF_METRIC_NATIVE_COVERAGE = 1u << 13u,
    ZR_PERF_METRIC_FALLBACK_COUNT = 1u << 14u,
    ZR_PERF_METRIC_DEOPT_COUNT = 1u << 15u
} EZrPerfMetricAvailability;

/** @brief 区分无效、不可比、噪声不足、未达收益及真实退化。 */
typedef enum EZrPerfComparisonStatus {
    ZR_PERF_COMPARISON_INVALID = 0,
    ZR_PERF_COMPARISON_COMPARABLE,
    ZR_PERF_COMPARISON_FALLBACK_VISIBLE,
    ZR_PERF_COMPARISON_INCOMPARABLE,
    ZR_PERF_COMPARISON_INCONCLUSIVE,
    ZR_PERF_COMPARISON_NO_GAIN,
    ZR_PERF_COMPARISON_REGRESSION
} EZrPerfComparisonStatus;

/** @brief 一组后端测量及其正确性、环境和可用指标的值对象。 */
typedef struct SZrPerfBackendMetrics {
    EZrPerfBackend requestedBackend;
    EZrPerfBackend actualBackend;
    EZrPerfMeasurementPhase phase;
    int processExitCode;

    TZrUInt32 sampleCount;
    double samples[ZR_PERF_METRICS_MAX_SAMPLES];
    double elapsedMs;

    /* Phase costs are optional. A value is meaningful only when its bit is set. */
    double compileMs;
    double linkMs;
    double loadMs;
    double startupMs;
    double steadyStateMs;

    TZrBool checksumValid;
    TZrBool environmentValid;
    TZrChar checksum[ZR_PERF_METRICS_TEXT_CAPACITY];
    TZrChar expectedChecksum[ZR_PERF_METRICS_TEXT_CAPACITY];
    TZrChar environmentFingerprint[ZR_PERF_METRICS_TEXT_CAPACITY];
    TZrChar workload[ZR_PERF_METRICS_TEXT_CAPACITY];

    TZrUInt32 availableMetrics;
    TZrUInt64 instructionCount;
    TZrUInt64 helperCount;
    TZrUInt64 allocationCount;
    TZrUInt64 gcWork;
    TZrUInt64 gcPauseUs;
    TZrUInt64 frameP50Us;
    TZrUInt64 frameP95Us;
    TZrUInt64 frameP99Us;
    TZrUInt64 frameMaxUs;
    TZrUInt64 rssBytes;
    TZrUInt64 codeSizeBytes;
    double nativeCoverage;
    TZrUInt64 fallbackCount;
    TZrUInt64 deoptCount;

    EZrPerfMetricsStatus status;
} SZrPerfBackendMetrics;

/** @brief 成对样本比较结果；只有 Bootstrap 区间和收益界有效时 improvementLow 才是置信下界，否则保留点估计。 */
typedef struct SZrPerfComparison {
    EZrPerfComparisonStatus status;
    TZrBool comparable;
    TZrBool gateEligible;
    double baselineMedian;
    double candidateMedian;
    double improvement;
    double improvementLow;
    double improvementHigh;
    double baselineCoefficientOfVariation;
    double candidateCoefficientOfVariation;
    TZrUInt32 baselineSampleCount;
    TZrUInt32 candidateSampleCount;
    TZrUInt64 fallbackCount;
    TZrUInt64 deoptCount;
    TZrChar reason[ZR_PERF_METRICS_TEXT_CAPACITY];
} SZrPerfComparison;

/** @brief 初始化供 benchmark 构造的可写样本及未知状态。 */
void ZrTests_Perf_MetricsInit(SZrPerfBackendMetrics *sample);
/** @brief 将后端枚举映射到报告使用的稳定名称。 */
const TZrChar *ZrTests_Perf_BackendName(EZrPerfBackend backend);
/** @brief 将样本校验状态映射到报告名称。 */
const TZrChar *ZrTests_Perf_MetricsStatusName(EZrPerfMetricsStatus status);
/** @brief 将比较分类映射到报告名称。 */
const TZrChar *ZrTests_Perf_ComparisonStatusName(EZrPerfComparisonStatus status);

/** @brief 校验样本结构与语义；合法回退标为可见。
 * @note 当前实现还会写入 sample->status，实参必须指向可写对象。 */
TZrBool ZrTests_Perf_ValidateMetrics(const SZrPerfBackendMetrics *sample);

/** @brief 比较同环境、同工作负载的成对样本并分类。
 * @return 输入畸形时为假；其余分类由 result->status 表示。 */
TZrBool ZrTests_Perf_ComparePaired(const SZrPerfBackendMetrics *baseline,
                                   const SZrPerfBackendMetrics *candidate,
                                   SZrPerfComparison *result);

#endif
