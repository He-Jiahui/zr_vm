#ifndef ZR_VM_PERF_REPORT_H
#define ZR_VM_PERF_REPORT_H

#include <stdint.h>
#include <stdio.h>

#include "persistent_protocol.h"
#include "perf_statistics.h"
#include "zr_vm_common/zr_common_conf.h"

/**
 * @brief 一批固定 repetitions 的逻辑测量，wallMs 为 aggregateWallMs/repetitions。
 * @note process 模式的峰值取该批各新进程峰值的最大值；持久模式只保存
 * 会话 PID，逐样本内存无独立测量，报告时写 null。校准和预热不进入汇总。
 */
typedef struct SZrPerfRunSample {
    double wallMs;
    double aggregateWallMs;
    uint64_t peakWorkingSetBytes;
    uint64_t processId;
    int exitCode;
} SZrPerfRunSample;

/**
 * @brief 同一批逻辑样本的耗时分布、Bootstrap 中位数区间与进程峰值分布。
 * @note stddevWallMs 使用 n-1 的样本标准差，单样本为 0；均值非零时
 * CV 为样本标准差/均值，全零耗时样本的 CV 为 0。
 * MAD 为相对中位数的绝对偏差中位数。内存统计不除以 repetitions；
 * 持久模式的这些内存字段不表示会话峰值，WriteJson 将它们写为 null。
 * 本结构不持有样本或工作数组；可比性和 gate 资格由 runner 的 metadata 表达。
 */
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

/**
 * @brief runner 已决定的采样策略和资格，供 suite 按同一口径消费统计。
 * @note sampleCount=initialSampleCount+extraSampleCount，合计不超过 20；
 * iterations 字段保存初始数，runs 与 summary 覆盖实际总数。
 * 当前 runner 用 CV<=0.05 判稳定，至少 10 个稳定样本才给 gateEligible；
 * profile 单样本设为 NOT_COMPARABLE。WriteJson 保存这些决定而不重新判定。
 * stability 借用 runner 的固定状态文本，须在同步写入返回前有效；
 * Bootstrap seed/resampleCount 须与本批 summary 的计算参数一致。
 */
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

/** @brief AOT 文本字段固定容量，包含 NUL；最多承载 127 个可打印 ASCII 字节。
 * @note 验证必须在字段数组内找到终止符，容量不是动态字符串长度。 */
#define ZR_PERF_AOT_REPORT_TEXT_CAPACITY 128U

/**
 * @brief AOT 快照的运行、回退、不可用或失败状态；INVALID/COUNT 不能写入。
 * @note RAN 要求退出码为 0 且 requested/actual backend 一致。
 * FALLBACK 要求成功退出；同名 backend 还须有可用的解释器执行计数。
 * UNAVAILABLE 要求非零退出码，actual backend 可为空。
 * FAILED 仍须满足文本、阶段与覆盖率约束，没有额外退出码限制。
 */
typedef enum EZrPerfAotReportStatus {
    ZR_PERF_AOT_REPORT_INVALID = 0,
    ZR_PERF_AOT_REPORT_RAN,
    ZR_PERF_AOT_REPORT_FALLBACK,
    ZR_PERF_AOT_REPORT_UNAVAILABLE,
    ZR_PERF_AOT_REPORT_FAILED,
    ZR_PERF_AOT_REPORT_STATUS_COUNT
} EZrPerfAotReportStatus;

/**
 * @brief 独立 AOT 阶段成本与覆盖率快照，不并入逻辑样本的耗时分布。
 * @note 文本须在固定数组内 NUL 终止且为可打印 ASCII；requestedBackend/
 * entryToken 必填，actualBackend 仅 UNAVAILABLE 可为空，其余文本允许为空。
 * 阶段为非负有限毫秒数或 -1；-1 写 null，0 仍可用。内存值由 has* 标志
 * 决定是否写 null，零字节不代表不可用。
 * coverageAvailable 为假时 nativeCoverage=-1 且五个语义计数为 0；
 * 为真时 native/helper/interpreter 的和须等于 executedSemanticSites，
 * 且 0<executedSemanticSites<=semanticSites。nativeCoverage 必须精确等于
 * (double)nativeSites/(double)executedSemanticSites，不含 helper。
 * fallbackCount/deoptCount 单独保存，不进入此分母分项；结构不持有外部资源。
 */
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

/**
 * @brief 返回 AOT 状态的固定 JSON 文本，未知值映射为 INVALID。
 * @return 非 NULL 静态字符串，无需释放，不能作为状态验证结果。
 */
const TZrChar *ZrPerfReport_AotStatusName(EZrPerfAotReportStatus status);
/**
 * @brief 在序列化前验证 AOT 快照的状态、阶段和覆盖率一致性。
 * @return 1 表示满足本报告结构约束，0 表示拒绝；不修改 report。
 * @note 不运行 backend、不鉴定 hash/toolchain；比例按 producer 的 double
 * 除法精确核对，不接受从 JSON 九位显示小数回填的近似值。
 */
int ZrPerfReport_ValidateAotPhase(const SZrPerfAotPhaseReport *report);
/**
 * @brief 验证快照后覆盖写入 schema_version=1 的独立 AOT JSON。
 * @pre report 及其内嵌字段在同步调用返回前保持有效且不被改写。
 * @return 全部写入、flush 与 close 成功返回 1；参数、验证或 I/O 失败返回 0。
 * @note 验证失败发生在打开目标之前，原文件保留；打开后失败可能留下
 * 截断或部分文件。不接管 report，阶段与可选字段的不可用值写 null。
 */
int ZrPerfReport_WriteAotJson(const char *jsonPath,
                              const SZrPerfAotPhaseReport *report);

/**
 * @brief 汇总 1 至 20 个逻辑测量样本，生成确定性 95% Bootstrap 中位数区间。
 * @pre samples 含 count 项，wallMs 为非负有限的归一化耗时；校准、预热和
 * 失败运行已由采样方排除。bootstrapResampleCount 必须非零。
 * @return 检出的参数、工作数组或时间统计失败返回 0，成功路径返回 1。
 * @note 不改变或持有 samples；内部临时数组在返回前释放。参数检查失败
 * 不写 summary，后续已检出的失败清零 summary。内存中位数的单独分配
 * 失败尚未被上报，参见实现 BUG；返回 1 不授予稳定性或 gate 资格。
 */
int ZrPerfReport_ComputeSummary(const SZrPerfRunSample *samples,
                                int count,
                                uint64_t bootstrapSeed,
                                size_t bootstrapResampleCount,
                                SZrPerfSummary *summary);

/**
 * @brief 将 runner 的测量批次、作用域和资格写为跨语言 suite 消费的 JSON。
 * @pre command 为 NULL 终止的参数数组，samples 至少有 metadata->sampleCount
 * 项；所有借用参数在同步调用返回前有效。stability 使用 runner 的固定文本。
 * @return 参数或 I/O 失败返回 0，写入、flush 与 close 全部成功返回 1。
 * @note 不重算 summary、校验 checksum 或重判资格，调用方负责样本、
 * 汇总及 Bootstrap 参数一致。persistentMode 下必须提供已完成 STOP 的会话
 * 快照，所有样本 PID 与之相同；只发布会话峰值，样本和 summary 内存写 null。
 * 元数据或 PID 被拒绝时尚未打开目标；打开后 I/O 失败可能留下部分文件。
 * 不接管借用参数，workingDirectory 为 NULL 时写空字符串。
 */
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
