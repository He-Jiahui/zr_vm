/* 用假 AOT 条目验证后端身份、回退状态、覆盖计数和报告序列化。 */
#include "aot_coverage.h"
#include "aot_runner.h"
#include "perf_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 断言失败打印原因并终止 fixture 进程；调用者不能依赖失败之后的文件清理继续运行。 */
static void expect_true(TZrBool condition, const char *message) {
    if (condition == ZR_FALSE) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

/* fixture state 由测试栈持有，registry 借用其地址；同步回调返回后由测试继续管理，无堆资源转移。 */
typedef struct SZrFixtureEntryState {
    TZrUInt64 checksum;
    TZrBool fail;
    SZrAotCoverageCounts coverage;
} SZrFixtureEntryState;

/** @brief 通过注册的借用 state 注入 checksum、coverage 或同步失败，测试 runner 协议；该回调不证明真实生成 AOT 条目已执行。 */
static TZrBool fixture_entry_invoke(void *context,
                                    TZrUInt64 *checksum,
                                    SZrAotCoverageCounts *coverage) {
    SZrFixtureEntryState *state = (SZrFixtureEntryState *)context;

    if (state == ZR_NULL || checksum == ZR_NULL || coverage == ZR_NULL || state->fail != ZR_FALSE) {
        return ZR_FALSE;
    }
    *checksum = state->checksum;
    *coverage = state->coverage;
    return ZR_TRUE;
}

/** @brief 区分 native/helper/interpreter 与 deopt，验证动态原生比率及较大静态分母比率各自保留。 */
static void test_coverage_keeps_semantic_denominator_and_classes(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageReport report;

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 80u;
    counts.nativeHelperSites = 10u;
    counts.interpreterSites = 10u;
    counts.deoptCount = 3u;
    counts.semanticSites = 100u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;

    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "representative coverage counts were rejected");
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "representative coverage report failed");
    expect_true(report.available != ZR_FALSE && report.semanticSites == 100u,
                "coverage denominator was not retained");
    expect_true(report.nativeCoverage > 0.799 && report.nativeCoverage < 0.801,
                "native coverage ratio was not 80 percent");
    expect_true(report.nativeExecutionCoverage > 0.899 &&
                    report.nativeExecutionCoverage < 0.901,
                "native helper execution was not included in the native execution ratio");
    expect_true(report.interpreterShare > 0.099 && report.interpreterShare < 0.101,
                "interpreter fallback share was not reported");
    expect_true(report.mixedExecution != ZR_FALSE,
                "mixed native/interpreter execution was hidden");

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 1u;
    counts.semanticSites = 5u;
    counts.executedSemanticSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "partial semantic-site coverage report failed");
    expect_true(report.semanticSites == 5u && report.executedSemanticSites == 1u &&
                    report.nativeCoverage > 0.999 && report.nativeStaticCoverage > 0.199 &&
                    report.nativeStaticCoverage < 0.201,
                "static and dynamic semantic denominators were conflated");
}

/** @brief 验证零观察仍结构合法，但 Compute 返回 unavailable 和负比率哨兵。 */
static void test_zero_coverage_is_unavailable_not_perfect(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageReport report;

    memset(&counts, 0, sizeof(counts));
    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "zero counts should be structurally valid");
    expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                "zero counts should produce an unavailable report");
    expect_true(report.available == ZR_FALSE && report.nativeCoverage < 0.0,
                "missing coverage data was reported as 100 percent");
}

/** @brief 拒绝静态分母小于已执行类和，同时允许未执行的静态站点。 */
static void test_coverage_rejects_inconsistent_denominator(void) {
    SZrAotCoverageCounts counts;

    memset(&counts, 0, sizeof(counts));
    counts.nativeSites = 2u;
    counts.semanticSites = 1u;
    expect_true(!ZrTests_AotCoverage_Validate(&counts),
                "coverage accepted a denominator smaller than class counts");

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 1u;
    counts.semanticSites = 5u;
    counts.executedSemanticSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    expect_true(ZrTests_AotCoverage_Validate(&counts),
                "unexecuted semantic sites were rejected");
}

/** @brief 验证递增/合并溢出及超静态分母失败不部分发布，并验证未知采样率不冒充精确数据。 */
static void test_coverage_record_and_merge_are_overflow_safe(void) {
    SZrAotCoverageCounts counts;
    SZrAotCoverageCounts other;
    TZrUInt64 original;

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = UINT64_MAX;
    original = counts.nativeSites;
    expect_true(!ZrTests_AotCoverage_Record(&counts,
                                            ZR_AOT_COVERAGE_CLASS_NATIVE,
                                            1u),
                "coverage counter overflow was accepted");
    expect_true(counts.nativeSites == original,
                "failed coverage increment partially mutated the counter");

    ZrTests_AotCoverage_Init(&counts);
    ZrTests_AotCoverage_Init(&other);
    counts.nativeSites = UINT64_MAX;
    other.nativeSites = 1u;
    expect_true(!ZrTests_AotCoverage_Merge(&counts, &other),
                "coverage merge overflow was accepted");
    expect_true(counts.nativeSites == UINT64_MAX && counts.semanticSites == 0u,
                "failed coverage merge partially mutated the destination");

    ZrTests_AotCoverage_Init(&counts);
    ZrTests_AotCoverage_Init(&other);
    counts.nativeSites = 1u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    other.nativeSites = 1u;
    other.sampleRatePermille = 0u;
    expect_true(ZrTests_AotCoverage_Merge(&counts, &other),
                "mixed-rate coverage merge failed");
    expect_true(counts.sampleRatePermille == 0u,
                "unknown sample rate was incorrectly reported as exact");

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 1u;
    counts.semanticSites = 1u;
    counts.executedSemanticSites = 1u;
    expect_true(!ZrTests_AotCoverage_Record(&counts,
                                            ZR_AOT_COVERAGE_CLASS_NATIVE,
                                            1u),
                "record exceeded the declared static denominator");
    expect_true(counts.nativeSites == 1u && counts.executedSemanticSites == 1u,
                "failed denominator record partially mutated the counters");
}

/** @brief 表驱动区分空计数与非空未知证据；不同采样率降为未知，后续精确来源不能恢复已丢失证据。 */
static void test_coverage_merge_preserves_sampling_evidence(void) {
/* 采样证据矩阵同时覆盖空目的、空来源、未知和同/异采样率；断言观察最终聚合而非只检查返回值。 */
    static const struct {
        TZrUInt64 destinationSites;
        TZrUInt32 destinationRate;
        TZrUInt64 sourceSites;
        TZrUInt32 sourceRate;
        TZrUInt32 expectedRate;
    } cases[] = {
            {1u, 0u, 1u, 1000u, 0u},
            {1u, 1000u, 1u, 0u, 0u},
            {0u, 0u, 1u, 1000u, 1000u},
            {0u, 500u, 1u, 1000u, 1000u},
            {1u, 1000u, 0u, 0u, 1000u},
            {1u, 1000u, 0u, 500u, 1000u},
            {1u, 0u, 0u, 1000u, 0u},
            {1u, 500u, 1u, 500u, 500u}
    };
    SZrAotCoverageCounts counts;
    SZrAotCoverageCounts source;
    SZrAotCoverageReport report;

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ZrTests_AotCoverage_Init(&counts);
        ZrTests_AotCoverage_Init(&source);
        counts.nativeSites = cases[i].destinationSites;
        counts.sampleRatePermille = cases[i].destinationRate;
        source.nativeSites = cases[i].sourceSites;
        source.sampleRatePermille = cases[i].sourceRate;
        expect_true(ZrTests_AotCoverage_Merge(&counts, &source),
                    "valid sampling evidence merge failed");
        expect_true(counts.sampleRatePermille == cases[i].expectedRate,
                    "sampling merge confused empty data with unknown evidence");
        expect_true(ZrTests_AotCoverage_Compute(&counts, &report),
                    "merged sampling report failed");
        expect_true(report.available == (TZrBool)(cases[i].expectedRate != 0u) &&
                        report.exact == (TZrBool)(cases[i].expectedRate == 1000u) &&
                        report.executedSemanticSites ==
                                cases[i].destinationSites + cases[i].sourceSites,
                    "merged sampling report invented availability or exactness");
    }

    ZrTests_AotCoverage_Init(&counts);
    ZrTests_AotCoverage_Init(&source);
    counts.nativeSites = 1u;
    counts.sampleRatePermille = 1000u;
    source.nativeSites = 1u;
    source.sampleRatePermille = 500u;
    expect_true(ZrTests_AotCoverage_Merge(&counts, &source),
                "different-rate merge failed");
    expect_true(counts.sampleRatePermille == 0u,
                "different-rate merge retained a sampling claim");
    source.sampleRatePermille = 1000u;
    expect_true(ZrTests_AotCoverage_Merge(&counts, &source) &&
                    counts.sampleRatePermille == 0u &&
                    ZrTests_AotCoverage_Compute(&counts, &report) &&
                    report.available == ZR_FALSE && report.exact == ZR_FALSE,
                "later exact sample restored unknown aggregate evidence");

    /* A nonempty declared denominator is evidence even without observed hits. */
    ZrTests_AotCoverage_Init(&counts);
    counts.semanticSites = 5u;
    expect_true(ZrTests_AotCoverage_Merge(&counts, &source) &&
                    counts.sampleRatePermille == 0u,
                "unobserved declared sites were treated as an empty aggregate");
}

/** @brief 经 fixture 注册/调用检查请求及实际 backend、token、checksum 和可用 coverage，不把名称当成真实生成 AOT 证据。 */
static void test_runner_reports_actual_backend_and_checksum(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = UINT64_C(0x12345678);
    state.coverage.nativeSites = 4u;
    state.coverage.semanticSites = 4u;
    state.coverage.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, UINT64_C(7), "fixture-c",
                        fixture_entry_invoke, &state),
                "compiled C entry could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.entryToken = UINT64_C(7);
    request.expectedChecksum = state.checksum;
    request.checksumRequired = ZR_TRUE;
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "registered C entry did not run");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_RAN &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.entryToken == UINT64_C(7) &&
                    result.checksum == state.checksum,
                "runner lost requested/actual backend or entry token");
    expect_true(result.coverage.available != ZR_FALSE,
                "runner discarded entry coverage");
    expect_true(ZrTests_AotRunner_ValidateResult(&result),
                "valid AOT runner result failed structural validation");
}

/** @brief 编译身份下的采样解释器站点标记 mixed/FALLBACK；helper-only 与未知采样率两种对照不能被误标。 */
static void test_runner_marks_compiled_entry_with_fallback_as_mixed(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = UINT64_C(0xfeed);
    state.coverage.interpreterSites = 2u;
    state.coverage.semanticSites = 2u;
    state.coverage.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, UINT64_C(8), "fixture-c-fallback",
                        fixture_entry_invoke, &state),
                "fallback C entry could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.entryToken = UINT64_C(8);
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "fallback C entry did not run");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FALLBACK &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.coverage.interpreterSites == 2u &&
                    result.coverage.mixedExecution != ZR_FALSE &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK &&
                    ZrTests_AotRunner_ValidateResult(&result),
                "sampled compiled AOT fallback work was not reported explicitly");

    state.coverage.nativeHelperSites = 2u;
    state.coverage.interpreterSites = 0u;
    state.coverage.semanticSites = 2u;
    request.entryToken = UINT64_C(8);
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "native-helper-only C entry did not run");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_RAN &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.coverage.nativeHelperSites == 2u &&
                    result.coverage.mixedExecution == ZR_FALSE,
                "native helper execution was incorrectly reported as fallback");

    state.coverage.nativeHelperSites = 0u;
    state.coverage.interpreterSites = 2u;
    state.coverage.sampleRatePermille = 0u;
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "unsampled C entry did not run");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_RAN &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.coverage.available == ZR_FALSE &&
                    result.coverage.mixedExecution == ZR_FALSE,
                "unsampled interpreter counts invented a fallback status");
}

/** @brief 检查三行 registry 矩阵及未知 backend 的明确 UNAVAILABLE/UNSUPPORTED_BACKEND 诊断。 */
static void test_runner_matrix_and_unsupported_backend_diagnostics(void) {
    SZrAotRunner runner;
    SZrAotRunnerMatrix matrix;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 1u;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, 12u, "c-entry",
                        fixture_entry_invoke, &state),
                "matrix C entry could not be registered");
    expect_true(ZrTests_AotRunner_DescribeMatrix(&runner, 12u, &matrix),
                "backend matrix could not be described");
    expect_true(matrix.rowCount == 3u && matrix.rows[0u].available != ZR_FALSE &&
                    matrix.rows[1u].available == ZR_FALSE,
                "backend matrix did not expose C/LLVM availability");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_UNKNOWN;
    request.entryToken = 12u;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "unknown backend returned fake success");
    expect_true(result.failure == ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND &&
                    result.status == ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                "unknown backend diagnostic was not specific");
}

/** @brief 以仅有解释器注册的 token 验证显式允许回退仍保留实际解释器与 FALLBACK 身份。 */
static void test_runner_makes_interpreter_fallback_explicit(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 99u;
    state.coverage.interpreterSites = 2u;
    state.coverage.semanticSites = 2u;
    state.coverage.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_INTERPRETER, UINT64_C(9), "fixture-interp",
                        fixture_entry_invoke, &state),
                "interpreter fixture could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_LLVM;
    request.entryToken = UINT64_C(9);
    request.allowInterpreterFallback = ZR_TRUE;
    expect_true(ZrTests_AotRunner_Run(&runner, &request, &result),
                "explicit interpreter fallback was rejected");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FALLBACK &&
                    result.actualBackend == ZR_AOT_BACKEND_INTERPRETER &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK &&
                    result.coverage.interpreterSites == 2u,
                "interpreter fallback was not visible in runner result");
}

/** @brief 区分未注册 LLVM 入口与 C 回调 checksum 不匹配；后一失败须保留已实际调用的 C 身份。 */
static void test_runner_rejects_missing_backend_and_checksum_mismatch(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.checksum = 10u;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, UINT64_C(4), "fixture-c",
                        fixture_entry_invoke, &state),
                "checksum fixture could not be registered");

    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_LLVM;
    request.entryToken = UINT64_C(4);
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "missing AOT backend returned fake success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_UNAVAILABLE &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE &&
                    result.actualBackend == ZR_AOT_BACKEND_UNKNOWN,
                "unsupported AOT backend diagnostic was lost");

    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.expectedChecksum = 11u;
    request.checksumRequired = ZR_TRUE;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "checksum mismatch returned success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FAILED &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH &&
                    result.actualBackend == ZR_AOT_BACKEND_C,
                "checksum mismatch did not preserve actual backend");
}

/** @brief 注入 C 回调失败，确认 FAILED/INVOCATION 且无静默解释器替代。 */
static void test_runner_reports_invocation_failure_without_fallback(void) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    SZrFixtureEntryState state;

    memset(&state, 0, sizeof(state));
    state.fail = ZR_TRUE;
    ZrTests_AotRunner_Init(&runner);
    expect_true(ZrTests_AotRunner_Register(
                        &runner, ZR_AOT_BACKEND_C, 17u, "failing-c",
                        fixture_entry_invoke, &state),
                "failing entry could not be registered");
    memset(&request, 0, sizeof(request));
    request.requestedBackend = ZR_AOT_BACKEND_C;
    request.entryToken = 17u;
    expect_true(!ZrTests_AotRunner_Run(&runner, &request, &result),
                "failed AOT invocation returned success");
    expect_true(result.status == ZR_AOT_RUNNER_STATUS_FAILED &&
                    result.actualBackend == ZR_AOT_BACKEND_C &&
                    result.failure == ZR_AOT_RUNNER_FAILURE_INVOCATION &&
                    ZrTests_AotRunner_ValidateResult(&result),
                "AOT invocation failure was not visible or valid");
}

/** @brief 验证阶段 -1 不可用哨兵、动态覆盖与 mixed 身份可序列化；只调用写入 API，不重新解析 JSON 内容。 */
static void test_aot_phase_report_keeps_costs_and_unavailable_values_explicit(void) {
    SZrPerfAotPhaseReport report;
    SZrPerfAotPhaseReport invalid;

    memset(&report, 0, sizeof(report));
    report.status = ZR_PERF_AOT_REPORT_RAN;
    report.processExitCode = 0;
    snprintf(report.requestedBackend, sizeof(report.requestedBackend), "aot_c");
    snprintf(report.actualBackend, sizeof(report.actualBackend), "aot_c");
    snprintf(report.entryToken, sizeof(report.entryToken), "fixture-7");
    snprintf(report.artifactHash, sizeof(report.artifactHash), "sha256:fixture");
    snprintf(report.toolchain, sizeof(report.toolchain), "gcc-11");
    report.compileMs = 2.0;
    report.linkMs = -1.0; /* unavailable, not an invented zero */
    report.loadMs = 1.0;
    report.startupMs = 3.0;
    report.runMs = 4.0;
    report.hasCodeSizeBytes = ZR_TRUE;
    report.codeSizeBytes = 128u;
    report.coverageAvailable = ZR_TRUE;
    report.semanticSites = 10u;
    report.executedSemanticSites = 10u;
    report.nativeSites = 8u;
    report.nativeHelperSites = 1u;
    report.interpreterSites = 1u;
    report.nativeCoverage = 0.8;
    expect_true(ZrPerfReport_ValidateAotPhase(&report),
                "valid AOT phase report was rejected");

    report.status = ZR_PERF_AOT_REPORT_FALLBACK;
    expect_true(ZrPerfReport_ValidateAotPhase(&report),
                "mixed compiled fallback report was rejected");
    report.status = ZR_PERF_AOT_REPORT_RAN;
    expect_true(ZrPerfReport_WriteAotJson("ssa_aot_runner_coverage_report.json", &report),
                "AOT phase report was not serialized");
    remove("ssa_aot_runner_coverage_report.json");

    invalid = report;
    invalid.actualBackend[0] = 'x';
    expect_true(!ZrPerfReport_ValidateAotPhase(&invalid),
                "requested/actual mismatch was hidden in AOT report");
}

/** @brief 仅为比率 fixture 复制必要 coverage 字段并将全部阶段设为 -1；不是完整 runner 结果到性能报告的通用转换器。 */
static SZrPerfAotPhaseReport phase_report_from_coverage(
        const SZrAotCoverageCounts *counts) {
    SZrAotCoverageReport coverage;
    SZrPerfAotPhaseReport report;

    expect_true(ZrTests_AotCoverage_Compute(counts, &coverage),
                "phase ratio coverage producer failed");
    memset(&report, 0, sizeof(report));
    report.status = ZR_PERF_AOT_REPORT_RAN;
    snprintf(report.requestedBackend, sizeof(report.requestedBackend), "aot_c");
    snprintf(report.actualBackend, sizeof(report.actualBackend), "aot_c");
    snprintf(report.entryToken, sizeof(report.entryToken), "ratio-fixture");
    report.compileMs = report.linkMs = report.loadMs = -1.0;
    report.startupMs = report.runMs = -1.0;
    report.coverageAvailable = coverage.available;
    report.semanticSites = coverage.semanticSites;
    report.executedSemanticSites = coverage.executedSemanticSites;
    report.nativeSites = coverage.nativeSites;
    report.nativeHelperSites = coverage.nativeHelperSites;
    report.interpreterSites = coverage.interpreterSites;
    report.nativeCoverage = coverage.nativeCoverage;
    return report;
}

/** @brief 验证报告 native 比率精确等于 producer 的动态原生/执行值，覆盖零、全量、1/3 和最大计数以及 unavailable 哨兵。 */
static void test_aot_phase_ratio_matches_dynamic_native_counters(void) {
    static const double mismatches[] = {1.0, 0.9, 0.08};
/* 端点与分数 fixture 检查 producer double 的精确合同，不能用格式化小数取代动态计数重算。 */
    static const struct {
        TZrUInt64 nativeSites;
        TZrUInt64 helperSites;
        TZrUInt64 executedSites;
        double expectedRatio;
    } validCases[] = {
            {0u, 10u, 10u, 0.0},
            {10u, 0u, 10u, 1.0},
            {1u, 2u, 3u, 1.0 / 3.0},
            {UINT64_MAX, 0u, UINT64_MAX, 1.0}
    };
    SZrAotCoverageCounts counts;
    SZrPerfAotPhaseReport report;
    SZrPerfAotPhaseReport invalid;

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 8u;
    counts.nativeHelperSites = 1u;
    counts.interpreterSites = 1u;
    counts.semanticSites = 100u;
    counts.executedSemanticSites = 10u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    report = phase_report_from_coverage(&counts);
    expect_true(report.nativeCoverage == 0.8 &&
                    ZrPerfReport_ValidateAotPhase(&report) != 0,
                "phase ratio did not preserve the producer's dynamic denominator");
    for (size_t i = 0u; i < sizeof(mismatches) / sizeof(mismatches[0]); ++i) {
        invalid = report;
        invalid.nativeCoverage = mismatches[i];
        expect_true(ZrPerfReport_ValidateAotPhase(&invalid) == 0,
                    "phase report accepted a ratio inconsistent with native counters");
    }

    for (size_t i = 0u; i < sizeof(validCases) / sizeof(validCases[0]); ++i) {
        ZrTests_AotCoverage_Init(&counts);
        counts.nativeSites = validCases[i].nativeSites;
        counts.nativeHelperSites = validCases[i].helperSites;
        counts.semanticSites = counts.executedSemanticSites =
                validCases[i].executedSites;
        counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
        report = phase_report_from_coverage(&counts);
        expect_true(report.nativeCoverage == validCases[i].expectedRatio &&
                        ZrPerfReport_ValidateAotPhase(&report) != 0,
                    "valid producer endpoint or fractional ratio was rejected");
        if (validCases[i].executedSites == 3u) {
            invalid = report;
            invalid.nativeCoverage = 0.333333333;
            expect_true(ZrPerfReport_ValidateAotPhase(&invalid) == 0,
                        "phase report accepted a rounded ratio as the producer double");
        }
    }

    ZrTests_AotCoverage_Init(&counts);
    report = phase_report_from_coverage(&counts);
    expect_true(report.coverageAvailable == ZR_FALSE &&
                    report.nativeCoverage == -1.0 &&
                    ZrPerfReport_ValidateAotPhase(&report) != 0,
                "unavailable phase ratio sentinel was rejected");
}

/** @brief 在固定相对路径写 sentinel，再提交不一致比率并逐字节检查原文件保留；正常路径关闭/删除文件，失败断言会立即退出。 */
static void test_invalid_phase_ratio_leaves_existing_output_unchanged(void) {
    static const char sentinel[] = "existing phase report\n";
    const char *path = "ssa_aot_phase_ratio_existing.json";
    SZrAotCoverageCounts counts;
    SZrPerfAotPhaseReport report;
    FILE *file;
    char retained[sizeof(sentinel)];
    size_t bytesRead;
    int extraByte;

    ZrTests_AotCoverage_Init(&counts);
    counts.nativeSites = 8u;
    counts.nativeHelperSites = 2u;
    counts.executedSemanticSites = 10u;
    counts.sampleRatePermille = ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE;
    report = phase_report_from_coverage(&counts);
    report.nativeCoverage = 1.0;
/* 该测试路径为相对 CTest 工作目录中的固定文件；此处创建 sentinel，拒绝写入必须发生在下游 fopen 之前。 */
    file = fopen(path, "wb");
    expect_true(file != ZR_NULL, "ratio sentinel file could not be opened");
    expect_true(fwrite(sentinel, 1u, sizeof(sentinel) - 1u, file) ==
                    sizeof(sentinel) - 1u && fclose(file) == 0,
                "ratio sentinel file could not be written");
    expect_true(ZrPerfReport_WriteAotJson(path, &report) == 0,
                "inconsistent ratio was written over an existing report");
    file = fopen(path, "rb");
    expect_true(file != ZR_NULL, "rejected ratio removed the existing report");
    bytesRead = fread(retained, 1u, sizeof(retained), file);
    extraByte = fgetc(file);
    expect_true(fclose(file) == 0, "retained ratio sentinel could not be closed");
    expect_true(bytesRead == sizeof(sentinel) - 1u && extraByte == EOF &&
                    memcmp(retained, sentinel, sizeof(sentinel) - 1u) == 0,
                "rejected ratio changed the existing report bytes");
    expect_true(remove(path) == 0, "ratio sentinel file could not be removed");
}

/** @brief 按固定顺序执行 coverage、registry、callback failure 与 phase report 回归，全部断言返回后才打印 PASS；不执行外部生成 AOT backend。 */
int main(void) {
    test_coverage_keeps_semantic_denominator_and_classes();
    test_zero_coverage_is_unavailable_not_perfect();
    test_coverage_rejects_inconsistent_denominator();
    test_coverage_record_and_merge_are_overflow_safe();
    test_coverage_merge_preserves_sampling_evidence();
    test_runner_reports_actual_backend_and_checksum();
    test_runner_marks_compiled_entry_with_fallback_as_mixed();
    test_runner_matrix_and_unsupported_backend_diagnostics();
    test_runner_makes_interpreter_fallback_explicit();
    test_runner_rejects_missing_backend_and_checksum_mismatch();
    test_runner_reports_invocation_failure_without_fallback();
    test_aot_phase_report_keeps_costs_and_unavailable_values_explicit();
    test_aot_phase_ratio_matches_dynamic_native_counters();
    test_invalid_phase_ratio_leaves_existing_output_unchanged();
    puts("ssa aot runner coverage PASS");
    return EXIT_SUCCESS;
}
