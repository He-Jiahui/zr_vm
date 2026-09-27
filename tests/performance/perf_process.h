#ifndef ZR_VM_TESTS_PERFORMANCE_PERF_PROCESS_H
#define ZR_VM_TESTS_PERFORMANCE_PERF_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "perf_report.h"

/** @brief 逐次启动独立子进程，汇总壁钟时间与最大工作集供 process 模式采样。
 * @pre command 为以 NULL 结束的非空 argv；sample 可写，失败路径需要可写错误缓冲区。
 * @return 调度成功为 1；子进程非零退出仍返回 1，退出码保存在 sample 中。 */
int ZrPerfProcess_RunAggregate(const char *workingDirectory,
                               char *const *command,
                               uint32_t repetitions,
                               uint32_t processTimeoutMs,
                               SZrPerfRunSample *sample,
                               char *errorBuffer,
                               size_t errorBufferSize);

#endif
