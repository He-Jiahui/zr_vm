#ifndef ZR_VM_PERF_REPORT_H
#define ZR_VM_PERF_REPORT_H

#include <stdint.h>
#include <stdio.h>

#include "persistent_protocol.h"
#include "perf_statistics.h"
#include "zr_vm_common/zr_common_conf.h"

/** @brief 单次逻辑样本；aggregateWallMs 可包含多次启动或协议重复。 */
typedef struct SZrPerfRunSample {
    double wallMs;
    double aggregateWallMs;
    uint64_t peakWorkingSetBytes;
    uint64_t processId;
    int exitCode;
} SZrPerfRunSample;

/** @brief 对测量样本汇总的时间分布与内存峰值统计。 */
typedef struct SZrPerfSummary {
    double meanWallMs;
    double medianWallMs;
    double minWallMs;
    double maxWallMs;
    double stddevWallMs;
    double madWallMs;
    double coefficientOfVariation;
    SZrPerfBootstrapInterval bootstrapMedian95;
    double meanPeakWorkingSetBytes;
    double medianPeakWorkingSetBytes;
    uint64_t minPeakWorkingSetBytes;
    uint64_t maxPeakWorkingSetBytes;
} SZrPerfSummary;

/** @brief 报告消费方判断可比性和 gate 资格所需的采样策略。 */
typedef struct SZrPerfMeasurementMetadata {
    int initialSampleCount;
    int sampleCount;
    int extraSampleCount;
    uint32_t repetitions;
    int calibrationEnabled;
    double minimumSampleMs;
    double calibrationAggregateWallMs;
    int comparable;
    int gateEligible;
    const char *stability;
    uint64_t bootstrapSeed;
    size_t bootstrapResampleCount;
} SZrPerfMeasurementMetadata;

/* 独立 AOT 阶段报告不把编译、链接、加载或启动混入稳态样本。
 * 阶段值 -1 表示不可用；零仍是有效测量。 */
#define ZR_PERF_AOT_REPORT_TEXT_CAPACITY 128U

/** @brief AOT 产物运行、回退、不可用及失败的报告状态。 */
typedef enum EZrPerfAotReportStatus {
    ZR_PERF_AOT_REPORT_INVALID = 0,
    ZR_PERF_AOT_REPORT_RAN,
    ZR_PERF_AOT_REPORT_FALLBACK,
    ZR_PERF_AOT_REPORT_UNAVAILABLE,
    ZR_PERF_AOT_REPORT_FAILED,
    ZR_PERF_AOT_REPORT_STATUS_COUNT
} EZrPerfAotReportStatus;

/** @brief AOT 阶段成本和覆盖率快照，文本字段必须 NUL 终止且为可打印 ASCII。 */
typedef struct SZrPerfAotPhaseReport {
    EZrPerfAotReportStatus status;
    int processExitCode;
    TZrChar requestedBackend[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar actualBackend[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar entryToken[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar artifactHash[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar toolchain[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar failureReason[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];

    double compileMs;
    double linkMs;
    double loadMs;
    double startupMs;
    double runMs;

    TZrBool hasRssBytes;
    TZrUInt64 rssBytes;
    TZrBool hasCodeSizeBytes;
    TZrUInt64 codeSizeBytes;

    TZrBool coverageAvailable;
    TZrUInt64 semanticSites;
    TZrUInt64 executedSemanticSites;
    TZrUInt64 nativeSites;
    TZrUInt64 nativeHelperSites;
    TZrUInt64 interpreterSites;
    TZrUInt64 fallbackCount;
    TZrUInt64 deoptCount;
    double nativeCoverage;
} SZrPerfAotPhaseReport;

/** @brief 返回 AOT 报告状态的固定 JSON 文本值。 */
const TZrChar *ZrPerfReport_AotStatusName(EZrPerfAotReportStatus status);
/** @brief 验证状态、阶段值和覆盖率分项的一致性。 */
int ZrPerfReport_ValidateAotPhase(const SZrPerfAotPhaseReport *report);
/** @brief 将已验证的 AOT 阶段快照写成独立 JSON 文件。 */
int ZrPerfReport_WriteAotJson(const char *jsonPath,
                              const SZrPerfAotPhaseReport *report);

/** @brief 汇总 1 至 20 个样本并生成确定性 Bootstrap 中位数区间。 */
int ZrPerfReport_ComputeSummary(const SZrPerfRunSample *samples,
                                int count,
                                uint64_t bootstrapSeed,
                                size_t bootstrapResampleCount,
                                SZrPerfSummary *summary);

/** @brief 将 runner 的采样与作用域元数据写成跨语言 suite 消费的 JSON。
 * @note persistentMode 下每个样本 PID 必须等于已退出会话的 PID。 */
int ZrPerfReport_WriteJson(const char *jsonPath,
                           const char *caseName,
                           const char *workingDirectory,
                           const char *measurementScope,
                           const char *prepareScope,
                           int runtimeReused,
                           int compilerReused,
                           int jitStateReused,
                           char *const *command,
                           const SZrPerfMeasurementMetadata *metadata,
                           int warmup,
                           const SZrPerfRunSample *samples,
                           const SZrPerfSummary *summary,
                           int persistentMode,
                           const SZrPerfPersistentSessionInfo *persistentSession,
                           const char *checksumContract,
                           const char *expectedChecksum);

#endif
