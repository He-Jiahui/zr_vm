#include "perf_report.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 内存中位数的 qsort 回调按完整 uint64_t 排序；不用相减再转 int，
 * 避免大峰值或相邻峰值被截断后破坏中间元素的次序。 */
static int zr_perf_report_compare_u64(const void *left, const void *right) {
    const uint64_t leftValue = *(const uint64_t *)left;
    const uint64_t rightValue = *(const uint64_t *)right;
    return leftValue < rightValue ? -1 : (leftValue > rightValue ? 1 : 0);
}

/* 对每个逻辑样本的进程峰值取均值，不按 repetitions 再除一次。
 * 先转 double 再求和避免整数累加溢出；返回值是统计近似值。 */
static double zr_perf_report_mean_u64(const uint64_t *values, int count) {
    double sum = 0.0;
    int index;
    if (values == NULL || count <= 0) {
        return 0.0;
    }
    for (index = 0; index < count; index++) {
        sum += (double)values[index];
    }
    return sum / (double)count;
}

/* 临时副本承载排序，保留调用方样本与命令执行次序的对应关系；
 * 偶数样本先转 double 再平均两个中间峰值，避免 uint64_t 加法溢出。 */
static double zr_perf_report_median_u64(const uint64_t *values, int count) {
    uint64_t *sortedValues;
    double result;
    if (values == NULL || count <= 0) {
        return 0.0;
    }
    sortedValues = (uint64_t *)malloc((size_t)count * sizeof(*sortedValues));
    /* BUG: 前面的数组和 Bootstrap 分配成功、但本次 malloc 失败时，返回的
     * 0 会被 ComputeSummary 当成有效中位数并返回 1。process 模式中真实峰值
     * 中位数为正（例如全部样本峰值为正）时，runner 继续 WriteJson，
     * median_peak_working_set_bytes 被误报为 0。
     * 静态可达链为 runner 的 ComputeSummary -> 此处 -> WriteJson -> suite 读取；
     * 尚未故障注入复现，后续修复须让此分配失败向上返回独立失败状态。 */
    if (sortedValues == NULL) {
        return 0.0;
    }
    memcpy(sortedValues, values, (size_t)count * sizeof(*sortedValues));
    qsort(sortedValues, (size_t)count, sizeof(*sortedValues), zr_perf_report_compare_u64);
    result = (count % 2) == 0 ?
                     ((double)sortedValues[(count / 2) - 1] + (double)sortedValues[count / 2]) / 2.0 :
                     (double)sortedValues[count / 2];
    free(sortedValues);
    return result;
}

/* runner 已排除校准、预热和失败运行；此处只汇总保留的逻辑样本。
 * wallMs 已由采样方除以 repetitions，aggregateWallMs 不参与分布统计。
 * 排序与 Bootstrap 使用临时数组，summary 返回后不持有样本或临时内存。 */
int ZrPerfReport_ComputeSummary(const SZrPerfRunSample *samples,
                                int count,
                                uint64_t bootstrapSeed,
                                size_t bootstrapResampleCount,
                                SZrPerfSummary *summary) {
    double *wallValues;
    uint64_t *peakValues;
    SZrPerfStatistics statistics;
    int index;
    if (samples == NULL || count <= 0 || count > (int)ZR_PERF_MAX_TOTAL_SAMPLES || summary == NULL ||
        (size_t)count > SIZE_MAX / sizeof(*wallValues) ||
        (size_t)count > SIZE_MAX / sizeof(*peakValues)) {
        return 0;
    }
    wallValues = (double *)malloc((size_t)count * sizeof(*wallValues));
    peakValues = (uint64_t *)malloc((size_t)count * sizeof(*peakValues));
    /* 任一工作数组失败都清空输出并释放另一数组，避免调用方消费半份统计。
     * 参数检查失败则尚未写 summary；两个失败阶段的输出状态并不相同。 */
    if (wallValues == NULL || peakValues == NULL) {
        free(wallValues);
        free(peakValues);
        memset(summary, 0, sizeof(*summary));
        return 0;
    }
    summary->minWallMs = samples[0].wallMs;
    summary->maxWallMs = samples[0].wallMs;
    summary->minPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;
    summary->maxPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;
    for (index = 0; index < count; index++) {
        wallValues[index] = samples[index].wallMs;
        peakValues[index] = samples[index].peakWorkingSetBytes;
        if (samples[index].wallMs < summary->minWallMs) summary->minWallMs = samples[index].wallMs;
        if (samples[index].wallMs > summary->maxWallMs) summary->maxWallMs = samples[index].wallMs;
        if (samples[index].peakWorkingSetBytes < summary->minPeakWorkingSetBytes) {
            summary->minPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
        if (samples[index].peakWorkingSetBytes > summary->maxPeakWorkingSetBytes) {
            summary->maxPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
    }
    /* 描述统计和中位数区间必须来自同一组 wallMs 且共同成功。
     * 这里不授予 comparable/gate 资格；runner 另按 profile、样本数和 CV 判定。 */
    if (!ZrPerfStatistics_Compute(wallValues, (size_t)count, &statistics) ||
        !ZrPerfStatistics_BootstrapMedian95(wallValues,
                                           (size_t)count,
                                           bootstrapSeed,
                                           bootstrapResampleCount,
                                           &summary->bootstrapMedian95)) {
        free(wallValues);
        free(peakValues);
        memset(summary, 0, sizeof(*summary));
        return 0;
    }
    summary->meanWallMs = statistics.mean;
    summary->medianWallMs = statistics.median;
    summary->stddevWallMs = statistics.sampleStddev;
    summary->madWallMs = statistics.mad;
    summary->coefficientOfVariation = statistics.coefficientOfVariation;
    summary->meanPeakWorkingSetBytes = zr_perf_report_mean_u64(peakValues, count);
    summary->medianPeakWorkingSetBytes = zr_perf_report_median_u64(peakValues, count);
    free(wallValues);
    free(peakValues);
    return 1;
}

/* 两类报告共用 JSON 字符串转义，保护命令、作用域及 AOT 身份字段。
 * NULL 写成空字符串；哪些可选字段要写 null 由外层先判断。
 * 普通字节原样输出，此处不验证 UTF-8；I/O 失败由外层最终检查。 */
static void zr_perf_report_json_escaped(FILE *file, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    fputc('"', file);
    while (cursor != NULL && *cursor != '\0') {
        switch (*cursor) {
            case '\\': fputs("\\\\", file); break;
            case '"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*cursor < 0x20) fprintf(file, "\\u%04x", (unsigned int)*cursor);
                else fputc((int)*cursor, file);
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

/* 将 runner 已完成的测量批次交给 suite，不执行命令、校验 checksum 或
 * 重算稳定性。结构与同会话 PID 先受检，打开目标后才按固定字段发布 JSON。 */
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
                           const char *expectedChecksum) {
    FILE *file;
    int index;
    /* iterations 保留初始预算，sample_count 包含追加样本；两者不能互换。
     * 先按总上限检查加法两项再核对恒等式，拒绝矛盾元数据后才允许截断目标。 */
    if (jsonPath == NULL || caseName == NULL || measurementScope == NULL || prepareScope == NULL ||
        command == NULL || metadata == NULL || samples == NULL || summary == NULL || metadata->sampleCount <= 0 ||
        metadata->sampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES || metadata->initialSampleCount <= 0 ||
        metadata->initialSampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES || metadata->extraSampleCount < 0 ||
        metadata->extraSampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES - metadata->initialSampleCount ||
        metadata->sampleCount != metadata->initialSampleCount + metadata->extraSampleCount ||
        metadata->repetitions == 0U || metadata->stability == NULL ||
        (persistentMode && persistentSession == NULL)) {
        return 0;
    }
    /* 持久会话先由 runner 完成 STOP；快照应与每个 RUN 来自同一 PID。
     * 此处只核对 PID，一致不代表重新验证退出码或 checksum。
     * 会话峰值覆盖整个服务器生命周期，不可充当各 RUN 的独立峰值。 */
    if (persistentMode) {
        for (index = 0; index < metadata->sampleCount; index++) {
            if (samples[index].processId != persistentSession->processId) return 0;
        }
    }
    file = fopen(jsonPath, "wb");
    if (file == NULL) return 0;
    fputs("{\n  \"name\": ", file); zr_perf_report_json_escaped(file, caseName);
    fputs(",\n  \"working_directory\": ", file);
    zr_perf_report_json_escaped(file, workingDirectory != NULL ? workingDirectory : "");
    fprintf(file,
            ",\n  \"iterations\": %d,\n  \"sample_count\": %d,\n  \"extra_sample_count\": %d,"
            "\n  \"repetitions\": %" PRIu32 ",\n  \"warmup\": %d,\n  \"measurement_scope\": ",
            metadata->initialSampleCount,
            metadata->sampleCount,
            metadata->extraSampleCount,
            metadata->repetitions,
            warmup);
    zr_perf_report_json_escaped(file, measurementScope);
    fputs(",\n  \"prepare_scope\": ", file); zr_perf_report_json_escaped(file, prepareScope);
    fprintf(file, ",\n  \"runtime_reused\": %s,\n  \"compiler_reused\": %s,\n  \"jit_state_reused\": %s,\n",
            runtimeReused ? "true" : "false", compilerReused ? "true" : "false", jitStateReused ? "true" : "false");
    /* 校准批次不进入 runs；这里只保留选定 repetitions 的总耗时作为依据。
     * 未校准时显式 null 保留“未测量”的含义，不能用 0 伪造校准成本。 */
    if (metadata->calibrationEnabled) {
        fprintf(file,
                "  \"calibration\": {\"enabled\": true, \"min_sample_ms\": %.3f, "
                "\"aggregate_wall_ms\": %.3f, \"repetitions\": %" PRIu32 "},\n",
                metadata->minimumSampleMs,
                metadata->calibrationAggregateWallMs,
                metadata->repetitions);
    } else {
        fputs("  \"calibration\": {\"enabled\": false, \"min_sample_ms\": null, "
              "\"aggregate_wall_ms\": null, \"repetitions\": 1},\n",
              file);
    }
    fprintf(file,
            "  \"stability\": \"%s\",\n  \"comparable\": %s,\n  \"gate_eligible\": %s,\n",
            metadata->stability,
            metadata->comparable ? "true" : "false",
            metadata->gateEligible ? "true" : "false");
    if (persistentMode) {
        fprintf(file, "  \"persistent_session\": {\"pid\": %" PRIu64 ", \"same_pid\": true, \"checksum_contract\": ", persistentSession->processId);
        zr_perf_report_json_escaped(file, checksumContract); fputs(", \"expected_checksum\": ", file);
        zr_perf_report_json_escaped(file, expectedChecksum);
        fprintf(file, ", \"peak_working_set_bytes\": %" PRIu64 ", \"exit_code\": %d},\n", persistentSession->peakWorkingSetBytes, persistentSession->exitCode);
    } else fputs("  \"persistent_session\": null,\n", file);
    fputs("  \"command\": [", file);
    for (index = 0; command[index] != NULL; index++) { if (index > 0) fputs(", ", file); zr_perf_report_json_escaped(file, command[index]); }
    fputs("],\n  \"runs\": [\n", file);
    /* runs 的 schema_version=3 同时保留总耗时与每次重复的归一化耗时。
     * 持久模式的样本和 summary 内存字段都写 null，suite 只读会话峰值；
     * process 模式则发布各逻辑样本的进程峰值，不按重复次数平均峰值。
     * Bootstrap seed 用十进制字符串保留完整 uint64_t 参数，区间来自 summary。 */
    for (index = 0; index < metadata->sampleCount; index++) {
        if (persistentMode) fprintf(file, "    {\"schema_version\": 3, \"index\": %d, \"repetitions\": %" PRIu32 ", \"aggregate_wall_ms\": %.3f, \"wall_ms\": %.3f, \"pid\": %" PRIu64 ", \"peak_working_set_bytes\": null}%s\n", index + 1, metadata->repetitions, samples[index].aggregateWallMs, samples[index].wallMs, samples[index].processId, (index + 1) == metadata->sampleCount ? "" : ",");
        else fprintf(file, "    {\"schema_version\": 3, \"index\": %d, \"repetitions\": %" PRIu32 ", \"aggregate_wall_ms\": %.3f, \"wall_ms\": %.3f, \"peak_working_set_bytes\": %" PRIu64 "}%s\n", index + 1, metadata->repetitions, samples[index].aggregateWallMs, samples[index].wallMs, samples[index].peakWorkingSetBytes, (index + 1) == metadata->sampleCount ? "" : ",");
    }
    fputs("  ],\n  \"summary\": {\n", file);
    fprintf(file, "    \"mean_wall_ms\": %.3f,\n    \"median_wall_ms\": %.3f,\n    \"min_wall_ms\": %.3f,\n    \"max_wall_ms\": %.3f,\n    \"stddev_wall_ms\": %.3f,\n    \"mad_wall_ms\": %.3f,\n    \"coefficient_of_variation\": %.9f,\n", summary->meanWallMs, summary->medianWallMs, summary->minWallMs, summary->maxWallMs, summary->stddevWallMs, summary->madWallMs, summary->coefficientOfVariation);
    fprintf(file,
            "    \"bootstrap\": {\"seed\": \"%" PRIu64 "\", \"statistic\": \"median\", "
            "\"resamples\": %zu, \"low\": %.3f, \"high\": %.3f},\n",
            metadata->bootstrapSeed,
            metadata->bootstrapResampleCount,
            summary->bootstrapMedian95.low,
            summary->bootstrapMedian95.high);
    if (persistentMode) fputs("    \"mean_peak_working_set_bytes\": null,\n    \"median_peak_working_set_bytes\": null,\n    \"min_peak_working_set_bytes\": null,\n    \"max_peak_working_set_bytes\": null\n", file);
    else fprintf(file, "    \"mean_peak_working_set_bytes\": %.0f,\n    \"median_peak_working_set_bytes\": %.0f,\n    \"min_peak_working_set_bytes\": %" PRIu64 ",\n    \"max_peak_working_set_bytes\": %" PRIu64 "\n", summary->meanPeakWorkingSetBytes, summary->medianPeakWorkingSetBytes, summary->minPeakWorkingSetBytes, summary->maxPeakWorkingSetBytes);
    fputs("  }\n}\n", file);
    /* stdio 缓冲可能延后暴露写入失败，flush 和 close 也必须影响返回值。
     * fopen 已截断目标，后续失败不回滚，调用方不能把残留文件当成成功报告。 */
    if (ferror(file) || fflush(file) != 0) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

/* 固定数组必须在 capacity 内终止，避免验证和序列化越界读取。
 * requestedBackend/entryToken 必填，actualBackend 仅 UNAVAILABLE 可为空；
 * hash/toolchain/failureReason 可为空。只核对可打印 ASCII，不鉴定身份内容。 */
static int zr_perf_report_aot_text_valid(const TZrChar *text,
                                         size_t capacity,
                                         int required) {
    size_t index;
    size_t length = 0u;

    if (text == NULL || capacity == 0u) {
        return 0;
    }
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    if (length == capacity || (required && length == 0u)) {
        return 0;
    }
    for (index = 0u; index < length; ++index) {
        const unsigned char value = (unsigned char)text[index];
        if (value < 0x20u || value > 0x7eu) {
            return 0;
        }
    }
    return 1;
}

/* 阶段 -1 是唯一“不可用”哨兵，序列化为 null；零是合法测量。
 * 拒绝其他负值和 NaN/Infinity，避免生成非 JSON 数字。 */
static int zr_perf_report_aot_phase_valid(double value) {
    return (value == -1.0 || (isfinite(value) && value >= 0.0)) ? 1 : 0;
}

/* 返回静态状态文本供 JSON 写入；未知枚举也映射 INVALID，永不返回 NULL。
 * 有效性仍由 ValidateAotPhase 判定，状态名本身不能充当验证结果。 */
const TZrChar *ZrPerfReport_AotStatusName(EZrPerfAotReportStatus status) {
    switch (status) {
        case ZR_PERF_AOT_REPORT_RAN:
            return "RAN";
        case ZR_PERF_AOT_REPORT_FALLBACK:
            return "FALLBACK";
        case ZR_PERF_AOT_REPORT_UNAVAILABLE:
            return "UNAVAILABLE";
        case ZR_PERF_AOT_REPORT_FAILED:
            return "FAILED";
        case ZR_PERF_AOT_REPORT_INVALID:
        case ZR_PERF_AOT_REPORT_STATUS_COUNT:
        default:
            return "INVALID";
    }
}

/* AOT 报告在写文件前核对身份、可选阶段和覆盖率快照的相互约束。
 * 此处不调用 backend，也不推导计数；缺失信息必须按报告哨兵由生产方表达。 */
int ZrPerfReport_ValidateAotPhase(const SZrPerfAotPhaseReport *report) {
    TZrUInt64 semanticSum;

    if (report == NULL || report->status <= ZR_PERF_AOT_REPORT_INVALID ||
        report->status >= ZR_PERF_AOT_REPORT_STATUS_COUNT ||
        !zr_perf_report_aot_text_valid(report->requestedBackend,
                                       sizeof(report->requestedBackend), 1) ||
        !zr_perf_report_aot_text_valid(report->entryToken,
                                       sizeof(report->entryToken), 1) ||
        !zr_perf_report_aot_text_valid(report->actualBackend,
                                       sizeof(report->actualBackend),
                                       report->status != ZR_PERF_AOT_REPORT_UNAVAILABLE) ||
        !zr_perf_report_aot_text_valid(report->artifactHash,
                                       sizeof(report->artifactHash), 0) ||
        !zr_perf_report_aot_text_valid(report->toolchain,
                                       sizeof(report->toolchain), 0) ||
        !zr_perf_report_aot_text_valid(report->failureReason,
                                       sizeof(report->failureReason), 0) ||
        !zr_perf_report_aot_phase_valid(report->compileMs) ||
        !zr_perf_report_aot_phase_valid(report->linkMs) ||
        !zr_perf_report_aot_phase_valid(report->loadMs) ||
        !zr_perf_report_aot_phase_valid(report->startupMs) ||
        !zr_perf_report_aot_phase_valid(report->runMs)) {
        return 0;
    }
    /* RAN 必须成功退出且 backend 相同；FALLBACK 也必须成功退出。
     * FALLBACK 若仍声明同一 backend，需以可用覆盖率和 interpreterSites
     * 证明内部解释器回退。UNAVAILABLE 不能声明成功退出；
     * FAILED 在这里没有额外退出码规则。 */
    if (report->status == ZR_PERF_AOT_REPORT_RAN &&
        (report->processExitCode != 0 ||
         strcmp(report->requestedBackend, report->actualBackend) != 0)) {
        return 0;
    }
    if (report->status == ZR_PERF_AOT_REPORT_FALLBACK &&
        (report->processExitCode != 0 || report->actualBackend[0] == '\0' ||
         (strcmp(report->requestedBackend, report->actualBackend) == 0 &&
          (report->coverageAvailable == ZR_FALSE || report->interpreterSites == 0u)))) {
        return 0;
    }
    if (report->status == ZR_PERF_AOT_REPORT_UNAVAILABLE &&
        report->processExitCode == 0) {
        /* 不可用产物不能通过 processExitCode=0 冒充一次成功调用。 */
        return 0;
    }
    /* 无覆盖率时用 -1 与零语义计数封闭状态，写入方据此生成 null。
     * 可用时 native/helper/interpreter 必须无溢出地分割 executedSemanticSites；
     * semanticSites 是声明站点总数，约束 executedSemanticSites 的上界；
     * nativeCoverage 的分母仍是 executedSemanticSites，fallback/deopt 事件数
     * 不参加这次求和。 */
    if (report->coverageAvailable == ZR_FALSE) {
        if (report->nativeCoverage != -1.0 || report->semanticSites != 0u ||
            report->executedSemanticSites != 0u || report->nativeSites != 0u ||
            report->nativeHelperSites != 0u || report->interpreterSites != 0u) {
            return 0;
        }
    } else {
        if (report->semanticSites == 0u || report->executedSemanticSites == 0u ||
            report->executedSemanticSites > report->semanticSites ||
            !isfinite(report->nativeCoverage) || report->nativeCoverage < 0.0 ||
            report->nativeCoverage > 1.0 ||
            report->nativeSites > report->executedSemanticSites ||
            report->nativeHelperSites > report->executedSemanticSites ||
            report->interpreterSites > report->executedSemanticSites ||
            report->nativeSites > UINT64_MAX - report->nativeHelperSites) {
            return 0;
        }
        semanticSum = report->nativeSites + report->nativeHelperSites;
        if (semanticSum > UINT64_MAX - report->interpreterSites ||
            semanticSum + report->interpreterSites != report->executedSemanticSites) {
            return 0;
        }
        /* 与 coverage producer 保持同一 double 表达式：纯 native 除以实际执行数。
         * helper 不进入分子，不能改用 semanticSites，也不接受格式化舍入值；
         * JSON 的九位小数用于显示，不能反过来作为 Validate 的输入精度。 */
        if (report->nativeCoverage !=
            (double)report->nativeSites / (double)report->executedSemanticSites) {
            return 0;
        }
    }
    return 1;
}

/* 仅接收已通过阶段验证的值，保留 -1/0 的可用性区别。
 * name 来自写入方固定键名，不接受外部文本；I/O 状态由写入方统一收尾。 */
static void zr_perf_report_aot_json_phase(FILE *file,
                                          const char *name,
                                          double value) {
    fprintf(file, "\"%s\": ", name);
    if (value == -1.0) {
        fputs("null", file);
    } else {
        fprintf(file, "%.3f", value);
    }
}

/* 阶段快照单独发布，不与 runner 的逻辑样本和 gate 结果混为同一报告。
 * 无效快照必须在 fopen 之前失败，保留已有目标文件。 */
int ZrPerfReport_WriteAotJson(const char *jsonPath,
                              const SZrPerfAotPhaseReport *report) {
    FILE *file;

    /* 验证在截断目标前完成，错误比率 fixture 正依赖这一顺序保存原字节。 */
    if (jsonPath == NULL || !ZrPerfReport_ValidateAotPhase(report)) {
        return 0;
    }
    file = fopen(jsonPath, "wb");
    if (file == NULL) {
        return 0;
    }
    fprintf(file, "{\n  \"schema_version\": 1,\n  \"status\": ");
    zr_perf_report_json_escaped(file, ZrPerfReport_AotStatusName(report->status));
    fprintf(file, ",\n  \"process_exit_code\": %d,\n  \"requested_backend\": ",
            report->processExitCode);
    zr_perf_report_json_escaped(file, report->requestedBackend);
    /* 空可选身份写 null；阶段 -1 和未声明可用的内存字段也写 null。
     * 已声明可用的零字节和零毫秒仍是有效数字，不据数值为零推断缺失。 */
    fputs(",\n  \"actual_backend\": ", file);
    if (report->actualBackend[0] == '\0') fputs("null", file);
    else zr_perf_report_json_escaped(file, report->actualBackend);
    fputs(",\n  \"entry_token\": ", file);
    zr_perf_report_json_escaped(file, report->entryToken);
    fputs(",\n  \"failure_reason\": ", file);
    if (report->failureReason[0] == '\0') fputs("null", file);
    else zr_perf_report_json_escaped(file, report->failureReason);
    fputs(",\n  \"artifact\": {\n    \"hash\": ", file);
    if (report->artifactHash[0] == '\0') fputs("null", file);
    else zr_perf_report_json_escaped(file, report->artifactHash);
    fputs(",\n    \"toolchain\": ", file);
    if (report->toolchain[0] == '\0') fputs("null", file);
    else zr_perf_report_json_escaped(file, report->toolchain);
    fputs("\n  },\n  \"phase_ms\": {", file);
    zr_perf_report_aot_json_phase(file, "compile", report->compileMs);
    fputs(", ", file);
    zr_perf_report_aot_json_phase(file, "link", report->linkMs);
    fputs(", ", file);
    zr_perf_report_aot_json_phase(file, "load", report->loadMs);
    fputs(", ", file);
    zr_perf_report_aot_json_phase(file, "startup", report->startupMs);
    fputs(", ", file);
    zr_perf_report_aot_json_phase(file, "run", report->runMs);
    fputs("},\n  \"memory\": {\n    \"rss_bytes\": ", file);
    if (report->hasRssBytes == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->rssBytes);
    fputs(",\n    \"code_size_bytes\": ", file);
    if (report->hasCodeSizeBytes == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->codeSizeBytes);
    /* 不可用覆盖率的语义计数和比率写 null，保持与合法零覆盖率的区别。
     * fallback_count/deopt_count 独立保留，不受可用标志遮蔽或加入原生比例。 */
    fputs("\n  },\n  \"coverage\": {\n    \"available\": ", file);
    fputs(report->coverageAvailable != ZR_FALSE ? "true" : "false", file);
    fputs(",\n    \"semantic_sites\": ", file);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->semanticSites);
    fputs(",\n    \"executed_semantic_sites\": ", file);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->executedSemanticSites);
    fputs(",\n    \"native_sites\": ", file);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->nativeSites);
    fputs(",\n    \"native_helper_sites\": ", file);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->nativeHelperSites);
    fputs(",\n    \"interpreter_sites\": ", file);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%" PRIu64, report->interpreterSites);
    fprintf(file, ",\n    \"fallback_count\": %" PRIu64
                 ",\n    \"deopt_count\": %" PRIu64
                 ",\n    \"native_coverage\": ",
            report->fallbackCount, report->deoptCount);
    if (report->coverageAvailable == ZR_FALSE) fputs("null", file);
    else fprintf(file, "%.9f", report->nativeCoverage);
    fputs("\n  }\n}\n", file);
    /* 与普通报告相同，写入、flush 和 close 任一失败都返回 0；
     * 已打开目标后的失败可留下部分文件，返回值才是本次发布成功的依据。 */
    if (ferror(file) || fflush(file) != 0) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}
