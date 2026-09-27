#ifndef ZR_VM_CLI_TEST_RUNNER_H
#define ZR_VM_CLI_TEST_RUNNER_H

#include "zr_vm_parser/test_contract.h"

#define ZR_CLI_TEST_CASE_ID_CAPACITY 512U
#define ZR_CLI_TEST_RESULT_MESSAGE_CAPACITY 512U
#define ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY 2048U

/** 测试协议状态；runner 将 worker 退出码和超时映射到这些分类。 */
typedef enum EZrCliTestStatus {
    ZR_CLI_TEST_STATUS_PASSED = 0,
    ZR_CLI_TEST_STATUS_FAILED = 1,
    ZR_CLI_TEST_STATUS_SKIPPED = 2,
    ZR_CLI_TEST_STATUS_TIMED_OUT = 3,
    ZR_CLI_TEST_STATUS_CRASHED = 4
} EZrCliTestStatus;

/** 过滤在调度前完成；exactCaseId 供隔离 worker 精确重入，jobs 只限制模块并行数。 */
typedef struct SZrCliTestRunnerOptions {
    const TZrChar *filterPattern;
    const TZrChar *exactCaseId;
    TZrUInt32 jobs;
    TZrUInt64 timeoutMilliseconds;
    TZrBool listOnly;
} SZrCliTestRunnerOptions;

/** 指向 discovery manifest 的借用引用；其拥有者必须存活到所有 worker 结束。 */
typedef struct SZrCliTestCaseReference {
    const SZrParserTestEntry *entry;
    const SZrParserTestCaseDescriptor *testCase;
    TZrUInt32 caseOrdinal;
    TZrChar id[ZR_CLI_TEST_CASE_ID_CAPACITY];
} SZrCliTestCaseReference;

/** 一个稳定 ID 对应的状态和有限长度诊断；case 描述符本身不由结果持有。 */
typedef struct SZrCliTestCaseResult {
    SZrCliTestCaseReference reference;
    EZrCliTestStatus status;
    TZrUInt64 durationMilliseconds;
    TZrBool executed;
    TZrChar message[ZR_CLI_TEST_RESULT_MESSAGE_CAPACITY];
    TZrChar output[ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY];
} SZrCliTestCaseResult;

/** 排序后的 case 聚合结果；Run 成功后必须调用 Free。 */
typedef struct SZrCliTestRunResult {
    SZrCliTestCaseResult *cases;
    TZrSize caseCount;
    TZrSize passedCount;
    TZrSize failedCount;
    TZrSize skippedCount;
    TZrSize timedOutCount;
    TZrSize crashedCount;
    TZrUInt32 jobsUsed;
    TZrUInt64 seed;
} SZrCliTestRunResult;

/** executor 接收借用 case，可并发调用；false 表示隔离基础设施崩溃或不可用。 */
typedef TZrBool (*FZrCliTestCaseExecutor)(
        const SZrCliTestCaseReference *reference,
        TZrUInt64 timeoutMilliseconds,
        SZrCliTestCaseResult *result,
        TZrPtr userData);

/** @brief 从 discovery manifest 构造稳定 ID，按模块分组并执行或仅列出 case。
 *  @note 多 worker 共享 executor 的 userData；调用方须保证回调及其上下文线程安全。
 */
TZrBool ZrCli_TestRunner_Run(
        const SZrParserTestManifest *manifests,
        TZrSize manifestCount,
        const SZrCliTestRunnerOptions *options,
        FZrCliTestCaseExecutor executor,
        TZrPtr userData,
        SZrCliTestRunResult *outResult);
/** @brief 释放聚合结果中的 case 数组；可用于部分失败后的清理。 */
void ZrCli_TestRunner_Free(SZrCliTestRunResult *result);
/** @brief 将聚合状态转为 CLI 退出码；崩溃优先于断言失败或超时。 */
int ZrCli_TestRunner_ExitCode(const SZrCliTestRunResult *result);
/** @brief 返回供人阅读的稳定状态名，未知枚举按 Crashed 呈现。 */
const TZrChar *ZrCli_TestRunner_StatusName(EZrCliTestStatus status);

#endif
