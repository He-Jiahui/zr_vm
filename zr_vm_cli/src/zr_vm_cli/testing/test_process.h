#ifndef ZR_VM_CLI_TEST_PROCESS_H
#define ZR_VM_CLI_TEST_PROCESS_H

#include "testing/test_runner.h"

/** 单个测试 case 的隔离执行请求；路径和 case ID 在 Run 返回前保持有效。 */
typedef struct SZrCliTestProcessRequest {
    const TZrChar *executablePath;
    const TZrChar *targetPath;
    const TZrChar *caseId;
    TZrUInt64 timeoutMilliseconds;
} SZrCliTestProcessRequest;

/** 子进程结果；output 为定长诊断摘录，非完整日志。 */
typedef struct SZrCliTestProcessResult {
    int exitCode;
    TZrUInt64 durationMilliseconds;
    TZrBool timedOut;
    TZrChar output[ZR_CLI_TEST_RESULT_OUTPUT_CAPACITY];
} SZrCliTestProcessResult;

/** @brief 从 runner 回调启动同一 CLI 的单 case worker，并采集超时、退出码及输出。
 *  @note false 表示启动或等待基础设施失败；非零退出码仍可作为一次成功采集返回 true。
 */
TZrBool ZrCli_TestProcess_Run(
        const SZrCliTestProcessRequest *request,
        SZrCliTestProcessResult *outResult);

#endif
