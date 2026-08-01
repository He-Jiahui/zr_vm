#ifndef ZR_VM_CLI_TEST_RUNNER_H
#define ZR_VM_CLI_TEST_RUNNER_H

#include "zr_vm_parser/test_contract.h"

#define ZR_CLI_TEST_CASE_ID_CAPACITY 512U
#define ZR_CLI_TEST_RESULT_MESSAGE_CAPACITY 512U
#define ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY 2048U

typedef enum EZrCliTestStatus {
    ZR_CLI_TEST_STATUS_PASSED = 0,
    ZR_CLI_TEST_STATUS_FAILED = 1,
    ZR_CLI_TEST_STATUS_SKIPPED = 2,
    ZR_CLI_TEST_STATUS_TIMED_OUT = 3,
    ZR_CLI_TEST_STATUS_CRASHED = 4
} EZrCliTestStatus;

typedef struct SZrCliTestRunnerOptions {
    const TZrChar *filterPattern;
    const TZrChar *exactCaseId;
    TZrUInt32 jobs;
    TZrUInt64 timeoutMilliseconds;
    TZrBool listOnly;
} SZrCliTestRunnerOptions;

typedef struct SZrCliTestCaseReference {
    const SZrParserTestEntry *entry;
    const SZrParserTestCaseDescriptor *testCase;
    TZrUInt32 caseOrdinal;
    TZrChar id[ZR_CLI_TEST_CASE_ID_CAPACITY];
} SZrCliTestCaseReference;

typedef struct SZrCliTestCaseResult {
    SZrCliTestCaseReference reference;
    EZrCliTestStatus status;
    TZrUInt64 durationMilliseconds;
    TZrBool executed;
    TZrChar message[ZR_CLI_TEST_RESULT_MESSAGE_CAPACITY];
    TZrChar output[ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY];
} SZrCliTestCaseResult;

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

typedef TZrBool (*FZrCliTestCaseExecutor)(
        const SZrCliTestCaseReference *reference,
        TZrUInt64 timeoutMilliseconds,
        SZrCliTestCaseResult *result,
        TZrPtr userData);

TZrBool ZrCli_TestRunner_Run(
        const SZrParserTestManifest *manifests,
        TZrSize manifestCount,
        const SZrCliTestRunnerOptions *options,
        FZrCliTestCaseExecutor executor,
        TZrPtr userData,
        SZrCliTestRunResult *outResult);
void ZrCli_TestRunner_Free(SZrCliTestRunResult *result);
int ZrCli_TestRunner_ExitCode(const SZrCliTestRunResult *result);
const TZrChar *ZrCli_TestRunner_StatusName(EZrCliTestStatus status);

#endif
