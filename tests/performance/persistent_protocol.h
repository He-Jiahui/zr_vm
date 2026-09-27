#ifndef ZR_VM_TESTS_PERFORMANCE_PERSISTENT_PROTOCOL_H
#define ZR_VM_TESTS_PERFORMANCE_PERSISTENT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* 校准倍增的硬上限，同时约束 wire protocol 的 repetitions。 */
#define ZR_PERF_MAX_REPETITIONS UINT32_C(1048576)

/** @brief 借用命令、工作目录及协议文本；会话存活期间调用方须保持其有效。 */
typedef struct SZrPerfPersistentOptions {
    const char *workingDirectory;
    char *const *command;
    const char *checksumContract;
    const char *expectedChecksum;
    uint32_t readyTimeoutMs;
    uint32_t requestTimeoutMs;
    uint32_t stopTimeoutMs;
} SZrPerfPersistentOptions;

/** @brief 单次 WARMUP/RUN 往返的耗时和同一服务器 PID。 */
typedef struct SZrPerfPersistentSample {
    double wallMs;
    uint64_t processId;
} SZrPerfPersistentSample;

/** @brief STOP 后进程退出状态与整个会话的内存峰值。 */
typedef struct SZrPerfPersistentSessionInfo {
    uint64_t processId;
    uint64_t peakWorkingSetBytes;
    int exitCode;
} SZrPerfPersistentSessionInfo;

/** @brief 不透明的进程及管道所有权句柄，使用前须零初始化。 */
typedef struct SZrPerfPersistentSession {
    void *implementation;
} SZrPerfPersistentSession;

/** @brief 启动持久服务器并校验 READY 契约；成功后由 Finish 或 Abort 释放。 */
int ZrPerfPersistentSession_Start(SZrPerfPersistentSession *session,
                                  const SZrPerfPersistentOptions *options,
                                  char *errorBuffer,
                                  size_t errorBufferSize);

/** @brief 发送一个编号请求并校验 DONE 索引及 checksum。
 * @note 失败不自动销毁会话，调用方须调用 Abort。 */
int ZrPerfPersistentSession_Request(SZrPerfPersistentSession *session,
                                    int isWarmup,
                                    int index,
                                    uint32_t repetitions,
                                    SZrPerfPersistentSample *sample,
                                    char *errorBuffer,
                                    size_t errorBufferSize);

/** @brief 发送 STOP、等待退出并释放会话；失败也清理拥有的进程和管道。 */
int ZrPerfPersistentSession_Finish(SZrPerfPersistentSession *session,
                                   SZrPerfPersistentSessionInfo *sessionInfo,
                                   char *errorBuffer,
                                   size_t errorBufferSize);

/** @brief 强制结束会话及后代进程，允许对空会话重复调用。 */
void ZrPerfPersistentSession_Abort(SZrPerfPersistentSession *session);

/** @brief 简单一次性入口，按先 warmup 后测量的顺序运行固定重复次数。 */
int ZrPerfPersistent_Run(const SZrPerfPersistentOptions *options,
                         int warmupCount,
                         int iterationCount,
                         SZrPerfPersistentSample *samples,
                         SZrPerfPersistentSessionInfo *sessionInfo,
                         char *errorBuffer,
                         size_t errorBufferSize);

#endif
