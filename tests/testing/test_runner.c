#include "unity.h"

#include "testing/test_runner.h"

#include <stdio.h>
#include <string.h>

/* 由 runner 回调计数；筛选和 skip 用例不应进入执行器，多 worker 共享此状态。 */
typedef struct SZrRunnerProbe {
    TZrSize calls;
} SZrRunnerProbe;

/* 用可控结果替代真实隔离进程，单独检验 runner 的调度、状态归并与退出码。 */
static TZrBool execute_probe(
        const SZrCliTestCaseReference *reference,
        TZrUInt64 timeoutMilliseconds,
        SZrCliTestCaseResult *result,
        TZrPtr userData) {
    SZrRunnerProbe *probe = (SZrRunnerProbe *)userData;

    (void)timeoutMilliseconds;
    /* TODO: 并行 worker 内的 Unity 断言失败会跳到主线程的 AbortFrame；改为回调错误结果后在主线程断言。 */
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(probe);
    /* BUG: 多模块且 jobs>1 时 worker 并发共享 probe，非原子递增产生数据竞争且计数可能丢失。 */
    probe->calls++;
    result->durationMilliseconds = 2U;
    snprintf(result->output, sizeof(result->output), "output:%s", reference->id);
    if (strcmp(reference->entry->qualifiedName, "fails") == 0) {
        result->status = ZR_CLI_TEST_STATUS_FAILED;
        snprintf(result->message, sizeof(result->message), "assertion failed");
    } else if (strcmp(reference->entry->qualifiedName, "slow") == 0) {
        result->durationMilliseconds = 11U;
        result->status = ZR_CLI_TEST_STATUS_PASSED;
    } else if (strcmp(reference->entry->qualifiedName, "crashes") == 0) {
        snprintf(result->message, sizeof(result->message), "isolate fault");
        return ZR_FALSE;
    } else {
        result->status = ZR_CLI_TEST_STATUS_PASSED;
    }
    return ZR_TRUE;
}

/* list 模式只枚举稳定 ID 和顺序，不执行 case；默认随机种子须可复用。 */
static void test_runner_discovers_stable_ids_order_and_seed(void) {
    SZrParserTestConstant arguments[] = {
            {.kind = ZR_PARSER_TEST_CONSTANT_INT, .value.intValue = -2},
            {.kind = ZR_PARSER_TEST_CONSTANT_STRING, .value.stringValue = "edge"},
    };
    SZrParserTestCaseDescriptor cases[] = {
            {.ordinal = 4U, .arguments = arguments, .argumentCount = 2U},
    };
    SZrParserTestEntry entries[] = {
            {.moduleId = "zeta", .qualifiedName = "plain"},
            {.moduleId = "alpha", .qualifiedName = "parameterized", .cases = cases, .caseCount = 1U},
    };
    SZrParserTestManifest manifest = {
            .schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION,
            .entries = entries,
            .entryCount = 2U,
    };
    SZrCliTestRunnerOptions options = {.jobs = 2U, .listOnly = ZR_TRUE};
    SZrCliTestRunResult first;
    SZrCliTestRunResult second;
    SZrRunnerProbe probe = {0U};

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &first));
    TEST_ASSERT_EQUAL_UINT32(2U, first.caseCount);
    TEST_ASSERT_EQUAL_STRING("alpha::parameterized#4(-2,\"edge\")", first.cases[0].reference.id);
    TEST_ASSERT_EQUAL_STRING("zeta::plain#0()", first.cases[1].reference.id);
    TEST_ASSERT_EQUAL_UINT32(0U, first.jobsUsed);
    TEST_ASSERT_EQUAL_UINT32(0U, probe.calls);
    TEST_ASSERT_NOT_EQUAL(0U, first.seed);

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &second));
    TEST_ASSERT_EQUAL_UINT64(first.seed, second.seed);
    ZrCli_TestRunner_Free(&second);
    ZrCli_TestRunner_Free(&first);
}

/* glob 筛选发生在调度之前，未匹配用例不可触发执行回调。 */
static void test_runner_filters_before_execution(void) {
    SZrParserTestEntry entries[] = {
            {.moduleId = "suite", .qualifiedName = "kept"},
            {.moduleId = "suite", .qualifiedName = "ignored"},
    };
    SZrParserTestManifest manifest = {
            .schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION,
            .entries = entries,
            .entryCount = 2U,
    };
    SZrCliTestRunnerOptions options = {.filterPattern = "*::ke?t#*", .jobs = 1U};
    SZrCliTestRunResult result;
    SZrRunnerProbe probe = {0U};

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &result));
    TEST_ASSERT_EQUAL_UINT32(1U, result.caseCount);
    TEST_ASSERT_EQUAL_UINT32(1U, probe.calls);
    TEST_ASSERT_EQUAL_UINT32(1U, result.passedCount);
    TEST_ASSERT_EQUAL(0, ZrCli_TestRunner_ExitCode(&result));
    ZrCli_TestRunner_Free(&result);
}

/* 内部精确 case ID 中的星号是名称字符，不得再次按用户 glob 扩展。 */
static void test_runner_internal_exact_case_id_does_not_expand_globs(void) {
    SZrParserTestEntry entries[] = {
            {.moduleId = "suite", .qualifiedName = "literal*name"},
            {.moduleId = "suite", .qualifiedName = "literalXname"},
    };
    SZrParserTestManifest manifest = {
            .schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION,
            .entries = entries,
            .entryCount = 2U,
    };
    SZrCliTestRunnerOptions options = {
            .exactCaseId = "suite::literal*name#0()",
            .jobs = 1U,
    };
    SZrCliTestRunResult result;
    SZrRunnerProbe probe = {0U};

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &result));
    TEST_ASSERT_EQUAL_UINT32(1U, result.caseCount);
    TEST_ASSERT_EQUAL_STRING(options.exactCaseId, result.cases[0].reference.id);
    TEST_ASSERT_EQUAL_UINT32(1U, probe.calls);
    ZrCli_TestRunner_Free(&result);
}

/* 同一 manifest 同时覆盖跳过、断言失败、超时和隔离故障的计数及退出码。 */
static void test_runner_reports_skip_failure_timeout_and_crash(void) {
    SZrParserTestEntry entries[] = {
            {.moduleId = "alpha", .qualifiedName = "passes"},
            {.moduleId = "alpha", .qualifiedName = "skipped", .skipReason = "not on this target"},
            {.moduleId = "beta", .qualifiedName = "fails"},
            {.moduleId = "beta", .qualifiedName = "slow"},
            {.moduleId = "gamma", .qualifiedName = "crashes"},
    };
    SZrParserTestManifest manifest = {
            .schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION,
            .entries = entries,
            .entryCount = 5U,
    };
    SZrCliTestRunnerOptions options = {.jobs = 8U, .timeoutMilliseconds = 5U};
    SZrCliTestRunResult result;
    SZrRunnerProbe probe = {0U};

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &result));
    TEST_ASSERT_EQUAL_UINT32(5U, result.caseCount);
    TEST_ASSERT_EQUAL_UINT32(4U, probe.calls);
    TEST_ASSERT_EQUAL_UINT32(3U, result.jobsUsed);
    TEST_ASSERT_EQUAL_UINT32(1U, result.passedCount);
    TEST_ASSERT_EQUAL_UINT32(1U, result.failedCount);
    TEST_ASSERT_EQUAL_UINT32(1U, result.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(1U, result.timedOutCount);
    TEST_ASSERT_EQUAL_UINT32(1U, result.crashedCount);
    TEST_ASSERT_EQUAL(3, ZrCli_TestRunner_ExitCode(&result));
    TEST_ASSERT_EQUAL_STRING("not on this target", result.cases[1].message);
    TEST_ASSERT_NOT_NULL(strstr(result.cases[0].output, "alpha::passes#0()"));
    ZrCli_TestRunner_Free(&result);
}

/* 仅断言失败时的退出码应区别于含隔离故障的聚合状态。 */
static void test_exit_code_distinguishes_assertion_failure(void) {
    SZrParserTestEntry entry = {.moduleId = "suite", .qualifiedName = "fails"};
    SZrParserTestManifest manifest = {
            .schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION,
            .entries = &entry,
            .entryCount = 1U,
    };
    SZrCliTestRunnerOptions options = {.jobs = 1U};
    SZrCliTestRunResult result;
    SZrRunnerProbe probe = {0U};

    TEST_ASSERT_TRUE(ZrCli_TestRunner_Run(
            &manifest, 1U, &options, execute_probe, &probe, &result));
    TEST_ASSERT_EQUAL(1, ZrCli_TestRunner_ExitCode(&result));
    TEST_ASSERT_EQUAL_STRING("Failed", ZrCli_TestRunner_StatusName(result.cases[0].status));
    ZrCli_TestRunner_Free(&result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_runner_discovers_stable_ids_order_and_seed);
    RUN_TEST(test_runner_filters_before_execution);
    RUN_TEST(test_runner_internal_exact_case_id_does_not_expand_globs);
    RUN_TEST(test_runner_reports_skip_failure_timeout_and_crash);
    RUN_TEST(test_exit_code_distinguishes_assertion_failure);
    return UNITY_END();
}
