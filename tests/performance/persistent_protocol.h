#ifndef ZR_VM_TESTS_PERFORMANCE_PERSISTENT_PROTOCOL_H
#define ZR_VM_TESTS_PERFORMANCE_PERSISTENT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define ZR_PERF_MAX_REPETITIONS UINT32_C(1048576)

typedef struct SZrPerfPersistentOptions {
    const char *workingDirectory;
    char *const *command;
    const char *checksumContract;
    const char *expectedChecksum;
    uint32_t readyTimeoutMs;
    uint32_t requestTimeoutMs;
    uint32_t stopTimeoutMs;
} SZrPerfPersistentOptions;

typedef struct SZrPerfPersistentSample {
    double wallMs;
    uint64_t processId;
} SZrPerfPersistentSample;

typedef struct SZrPerfPersistentSessionInfo {
    uint64_t processId;
    uint64_t peakWorkingSetBytes;
    int exitCode;
} SZrPerfPersistentSessionInfo;

typedef struct SZrPerfPersistentSession {
    void *implementation;
} SZrPerfPersistentSession;

int ZrPerfPersistentSession_Start(SZrPerfPersistentSession *session,
                                  const SZrPerfPersistentOptions *options,
                                  char *errorBuffer,
                                  size_t errorBufferSize);

int ZrPerfPersistentSession_Request(SZrPerfPersistentSession *session,
                                    int isWarmup,
                                    int index,
                                    uint32_t repetitions,
                                    SZrPerfPersistentSample *sample,
                                    char *errorBuffer,
                                    size_t errorBufferSize);

int ZrPerfPersistentSession_Finish(SZrPerfPersistentSession *session,
                                   SZrPerfPersistentSessionInfo *sessionInfo,
                                   char *errorBuffer,
                                   size_t errorBufferSize);

void ZrPerfPersistentSession_Abort(SZrPerfPersistentSession *session);

int ZrPerfPersistent_Run(const SZrPerfPersistentOptions *options,
                         int warmupCount,
                         int iterationCount,
                         SZrPerfPersistentSample *samples,
                         SZrPerfPersistentSessionInfo *sessionInfo,
                         char *errorBuffer,
                         size_t errorBufferSize);

#endif
