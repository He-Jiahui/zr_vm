#ifndef ZR_VM_TESTS_BENCHMARKS_AOT_RUNNER_H
#define ZR_VM_TESTS_BENCHMARKS_AOT_RUNNER_H

#include "aot_coverage.h"

#define ZR_AOT_RUNNER_MAX_ENTRIES 64u
#define ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY 64u

typedef enum EZrAotBackend {
    ZR_AOT_BACKEND_UNKNOWN = 0,
    ZR_AOT_BACKEND_C,
    ZR_AOT_BACKEND_LLVM,
    ZR_AOT_BACKEND_INTERPRETER,
    ZR_AOT_BACKEND_COUNT
} EZrAotBackend;

/* Names used by the performance manifest are intentionally available too. */
#define ZR_AOT_BACKEND_AOT_C ZR_AOT_BACKEND_C
#define ZR_AOT_BACKEND_AOT_LLVM ZR_AOT_BACKEND_LLVM

typedef enum EZrAotRunnerStatus {
    ZR_AOT_RUNNER_STATUS_INVALID = 0,
    ZR_AOT_RUNNER_STATUS_RAN,
    ZR_AOT_RUNNER_STATUS_FALLBACK,
    ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
    ZR_AOT_RUNNER_STATUS_FAILED,
    ZR_AOT_RUNNER_STATUS_COUNT
} EZrAotRunnerStatus;

typedef enum EZrAotRunnerFailure {
    ZR_AOT_RUNNER_FAILURE_NONE = 0,
    ZR_AOT_RUNNER_FAILURE_INVALID_ARGUMENT,
    ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND,
    ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE,
    ZR_AOT_RUNNER_FAILURE_REGISTRY_FULL,
    ZR_AOT_RUNNER_FAILURE_DUPLICATE_ENTRY,
    ZR_AOT_RUNNER_FAILURE_INVOCATION,
    ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH,
    ZR_AOT_RUNNER_FAILURE_MALFORMED_COVERAGE,
    ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK,
    ZR_AOT_RUNNER_FAILURE_COUNT
} EZrAotRunnerFailure;

typedef struct SZrAotRunnerMatrixRow {
    EZrAotBackend backend;
    TZrBool available;
    TZrUInt32 registeredEntries;
} SZrAotRunnerMatrixRow;

typedef struct SZrAotRunnerMatrix {
    TZrUInt64 entryToken;
    TZrUInt32 rowCount;
    SZrAotRunnerMatrixRow rows[ZR_AOT_BACKEND_COUNT - 1u];
} SZrAotRunnerMatrix;

typedef TZrBool (*FZrAotRunnerEntry)(TZrPtr context,
                                     TZrUInt64 *checksum,
                                     SZrAotCoverageCounts *coverage);

typedef struct SZrAotRunnerEntry {
    EZrAotBackend backend;
    TZrUInt64 entryToken;
    TZrChar name[ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY];
    FZrAotRunnerEntry invoke;
    TZrPtr context;
} SZrAotRunnerEntry;

typedef struct SZrAotRunner {
    TZrBool initialized;
    TZrUInt32 entryCount;
    SZrAotRunnerEntry entries[ZR_AOT_RUNNER_MAX_ENTRIES];
} SZrAotRunner;

typedef struct SZrAotRunnerRequest {
    EZrAotBackend requestedBackend;
    TZrUInt64 entryToken;
    TZrUInt64 expectedChecksum;
    TZrBool checksumRequired;
    TZrBool allowInterpreterFallback;
} SZrAotRunnerRequest;

typedef struct SZrAotRunnerResult {
    EZrAotRunnerStatus status;
    EZrAotRunnerFailure failure;
    EZrAotBackend requestedBackend;
    EZrAotBackend actualBackend;
    TZrUInt64 entryToken;
    TZrUInt64 checksum;
    int processExitCode;
    TZrBool entryInvoked;
    TZrChar entryName[ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY];
    SZrAotCoverageReport coverage;
} SZrAotRunnerResult;

typedef TZrBool (*FZrAotRunnerRegisterCompiledEntries)(SZrAotRunner *runner);

void ZrTests_AotRunner_Init(SZrAotRunner *runner);
TZrBool ZrTests_AotRunner_Register(SZrAotRunner *runner,
                                   EZrAotBackend backend,
                                   TZrUInt64 entryToken,
                                   const TZrChar *name,
                                   FZrAotRunnerEntry invoke,
                                   TZrPtr context);
TZrBool ZrTests_AotRunner_Run(const SZrAotRunner *runner,
                              const SZrAotRunnerRequest *request,
                              SZrAotRunnerResult *result);
TZrBool ZrTests_AotRunner_ValidateResult(const SZrAotRunnerResult *result);
TZrBool ZrTests_AotRunner_DescribeMatrix(const SZrAotRunner *runner,
                                         TZrUInt64 entryToken,
                                         SZrAotRunnerMatrix *matrix);

/* Synonym retained for callers that use the plan's “build matrix” wording. */
TZrBool ZrTests_AotRunner_BuildMatrix(const SZrAotRunner *runner,
                                      TZrUInt64 entryToken,
                                      SZrAotRunnerMatrix *matrix);

const TZrChar *ZrTests_AotRunner_BackendName(EZrAotBackend backend);
const TZrChar *ZrTests_AotRunner_StatusName(EZrAotRunnerStatus status);
const TZrChar *ZrTests_AotRunner_FailureName(EZrAotRunnerFailure failure);

/*
 * The command-line entry point is deliberately callback-driven.  Generated
 * AOT artifacts provide the registration callback; no interpreter or zr_binary
 * executable is substituted when that callback is absent.
 */
int ZrTests_AotRunner_Main(int argc,
                           char **argv,
                           FZrAotRunnerRegisterCompiledEntries registerEntries);

#endif /* ZR_VM_TESTS_BENCHMARKS_AOT_RUNNER_H */
