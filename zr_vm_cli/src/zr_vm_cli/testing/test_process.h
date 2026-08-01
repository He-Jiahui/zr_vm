#ifndef ZR_VM_CLI_TEST_PROCESS_H
#define ZR_VM_CLI_TEST_PROCESS_H

#include "testing/test_runner.h"

typedef struct SZrCliTestProcessRequest {
    const TZrChar *executablePath;
    const TZrChar *targetPath;
    const TZrChar *caseId;
    TZrUInt64 timeoutMilliseconds;
} SZrCliTestProcessRequest;

typedef struct SZrCliTestProcessResult {
    int exitCode;
    TZrUInt64 durationMilliseconds;
    TZrBool timedOut;
    TZrChar output[ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY];
} SZrCliTestProcessResult;

TZrBool ZrCli_TestProcess_Run(
        const SZrCliTestProcessRequest *request,
        SZrCliTestProcessResult *outResult);

#endif
