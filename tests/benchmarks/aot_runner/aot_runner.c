#include "aot_runner.h"

#include <string.h>

static TZrBool zr_aot_runner_backend_is_valid(EZrAotBackend backend) {
    return (TZrBool)(backend > ZR_AOT_BACKEND_UNKNOWN &&
                     backend < ZR_AOT_BACKEND_COUNT);
}

static TZrBool zr_aot_runner_requested_backend_is_aot(EZrAotBackend backend) {
    return (TZrBool)(backend == ZR_AOT_BACKEND_C || backend == ZR_AOT_BACKEND_LLVM);
}

static TZrBool zr_aot_runner_name_is_valid(const TZrChar *name, size_t *lengthOut) {
    size_t length = 0u;

    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    while (length < (size_t)ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY && name[length] != '\0') {
        const unsigned char value = (unsigned char)name[length];
        if (value < 0x21u || value > 0x7eu) {
            return ZR_FALSE;
        }
        ++length;
    }
    if (length == 0u || length >= (size_t)ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY) {
        return ZR_FALSE;
    }
    if (lengthOut != ZR_NULL) {
        *lengthOut = length;
    }
    return ZR_TRUE;
}

static const SZrAotRunnerEntry *zr_aot_runner_find(const SZrAotRunner *runner,
                                                   EZrAotBackend backend,
                                                   TZrUInt64 entryToken) {
    TZrUInt32 index;

    if (runner == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < runner->entryCount; ++index) {
        const SZrAotRunnerEntry *entry = &runner->entries[index];
        if (entry->backend == backend && entry->entryToken == entryToken) {
            return entry;
        }
    }
    return ZR_NULL;
}

static void zr_aot_runner_result_init(SZrAotRunnerResult *result,
                                      const SZrAotRunnerRequest *request) {
    if (result == ZR_NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->status = ZR_AOT_RUNNER_STATUS_INVALID;
    result->failure = ZR_AOT_RUNNER_FAILURE_NONE;
    result->actualBackend = ZR_AOT_BACKEND_UNKNOWN;
    result->processExitCode = -1;
    result->coverage.status = ZR_AOT_COVERAGE_STATUS_UNAVAILABLE;
    result->coverage.nativeCoverage = -1.0;
    result->coverage.nativeExecutionCoverage = -1.0;
    result->coverage.nativeHelperShare = -1.0;
    result->coverage.interpreterShare = -1.0;
    result->coverage.fallbackTimeShare = -1.0;
    if (request != ZR_NULL) {
        result->requestedBackend = request->requestedBackend;
        result->entryToken = request->entryToken;
    }
}

static void zr_aot_runner_result_fail(SZrAotRunnerResult *result,
                                      EZrAotRunnerStatus status,
                                      EZrAotRunnerFailure failure) {
    if (result != ZR_NULL) {
        result->status = status;
        result->failure = failure;
    }
}

void ZrTests_AotRunner_Init(SZrAotRunner *runner) {
    if (runner != ZR_NULL) {
        memset(runner, 0, sizeof(*runner));
        runner->initialized = ZR_TRUE;
    }
}

TZrBool ZrTests_AotRunner_Register(SZrAotRunner *runner,
                                   EZrAotBackend backend,
                                   TZrUInt64 entryToken,
                                   const TZrChar *name,
                                   FZrAotRunnerEntry invoke,
                                   TZrPtr context) {
    SZrAotRunnerEntry *entry;
    size_t nameLength;

    if (runner == ZR_NULL || runner->initialized == ZR_FALSE ||
        !zr_aot_runner_backend_is_valid(backend) || entryToken == 0u || invoke == ZR_NULL ||
        !zr_aot_runner_name_is_valid(name, &nameLength)) {
        return ZR_FALSE;
    }
    if (runner->entryCount >= ZR_AOT_RUNNER_MAX_ENTRIES ||
        zr_aot_runner_find(runner, backend, entryToken) != ZR_NULL) {
        return ZR_FALSE;
    }
    entry = &runner->entries[runner->entryCount++];
    memset(entry, 0, sizeof(*entry));
    entry->backend = backend;
    entry->entryToken = entryToken;
    memcpy(entry->name, name, nameLength);
    entry->name[nameLength] = '\0';
    entry->invoke = invoke;
    entry->context = context;
    return ZR_TRUE;
}

TZrBool ZrTests_AotRunner_Run(const SZrAotRunner *runner,
                              const SZrAotRunnerRequest *request,
                              SZrAotRunnerResult *result) {
    const SZrAotRunnerEntry *entry;
    SZrAotCoverageCounts counts;
    TZrUInt64 checksum = 0u;
    TZrBool fallback = ZR_FALSE;

    zr_aot_runner_result_init(result, request);
    if (result == ZR_NULL || runner == ZR_NULL || request == ZR_NULL ||
        runner->initialized == ZR_FALSE || request->entryToken == 0u) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    if (request->requestedBackend <= ZR_AOT_BACKEND_UNKNOWN ||
        request->requestedBackend >= ZR_AOT_BACKEND_COUNT) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND);
        return ZR_FALSE;
    }

    entry = zr_aot_runner_find(runner, request->requestedBackend, request->entryToken);
    if (entry == ZR_NULL && request->allowInterpreterFallback != ZR_FALSE &&
        zr_aot_runner_requested_backend_is_aot(request->requestedBackend)) {
        entry = zr_aot_runner_find(runner,
                                   ZR_AOT_BACKEND_INTERPRETER,
                                   request->entryToken);
        fallback = (TZrBool)(entry != ZR_NULL);
    }
    if (entry == ZR_NULL) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE);
        return ZR_FALSE;
    }

    result->actualBackend = entry->backend;
    result->entryInvoked = ZR_TRUE;
    memcpy(result->entryName, entry->name, sizeof(result->entryName));
    ZrTests_AotCoverage_Init(&counts);
    if (!entry->invoke(entry->context, &checksum, &counts)) {
        result->processExitCode = 1;
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_INVOCATION);
        return ZR_FALSE;
    }
    result->processExitCode = 0;
    result->checksum = checksum;
    if (!ZrTests_AotCoverage_Validate(&counts) ||
        !ZrTests_AotCoverage_Compute(&counts, &result->coverage)) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_MALFORMED_COVERAGE);
        return ZR_FALSE;
    }
    if (request->checksumRequired != ZR_FALSE && checksum != request->expectedChecksum) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH);
        return ZR_FALSE;
    }
    if (fallback != ZR_FALSE) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FALLBACK,
                                  ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK);
    } else {
        result->status = ZR_AOT_RUNNER_STATUS_RAN;
        result->failure = ZR_AOT_RUNNER_FAILURE_NONE;
    }
    return ZR_TRUE;
}

TZrBool ZrTests_AotRunner_ValidateResult(const SZrAotRunnerResult *result) {
    TZrBool coverageShapeValid;
    size_t nameLength;

    if (result == ZR_NULL || !zr_aot_runner_backend_is_valid(result->requestedBackend) ||
        result->actualBackend < ZR_AOT_BACKEND_UNKNOWN ||
        result->actualBackend >= ZR_AOT_BACKEND_COUNT ||
        result->entryToken == 0u || result->status <= ZR_AOT_RUNNER_STATUS_INVALID ||
        result->status >= ZR_AOT_RUNNER_STATUS_COUNT ||
        result->failure < ZR_AOT_RUNNER_FAILURE_NONE ||
        result->failure >= ZR_AOT_RUNNER_FAILURE_COUNT ||
        result->coverage.status < ZR_AOT_COVERAGE_STATUS_INVALID ||
        result->coverage.status >= ZR_AOT_COVERAGE_STATUS_COUNT) {
        return ZR_FALSE;
    }
    coverageShapeValid = (TZrBool)(
            (result->coverage.available == ZR_FALSE &&
             result->coverage.status == ZR_AOT_COVERAGE_STATUS_UNAVAILABLE &&
             result->coverage.nativeCoverage < 0.0 &&
             result->coverage.nativeExecutionCoverage < 0.0) ||
            (result->coverage.available != ZR_FALSE &&
             result->coverage.status == ZR_AOT_COVERAGE_STATUS_AVAILABLE &&
             result->coverage.semanticSites != 0u &&
             result->coverage.sampleRatePermille != 0u &&
             result->coverage.sampleRatePermille <=
                     ZR_AOT_COVERAGE_MAX_SAMPLE_PERMILLE &&
             result->coverage.nativeCoverage >= 0.0 &&
             result->coverage.nativeCoverage <= 1.0 &&
             result->coverage.nativeExecutionCoverage >= 0.0 &&
             result->coverage.nativeExecutionCoverage <= 1.0 &&
             result->coverage.nativeHelperShare >= 0.0 &&
             result->coverage.nativeHelperShare <= 1.0 &&
             result->coverage.interpreterShare >= 0.0 &&
             result->coverage.interpreterShare <= 1.0));
    if (coverageShapeValid == ZR_FALSE) {
        return ZR_FALSE;
    }
    switch (result->status) {
        case ZR_AOT_RUNNER_STATUS_RAN:
            return (TZrBool)(result->failure == ZR_AOT_RUNNER_FAILURE_NONE &&
                             result->actualBackend == result->requestedBackend &&
                             result->entryInvoked != ZR_FALSE &&
                             result->processExitCode == 0 &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength));
        case ZR_AOT_RUNNER_STATUS_FALLBACK:
            return (TZrBool)(result->failure == ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK &&
                             result->actualBackend == ZR_AOT_BACKEND_INTERPRETER &&
                             result->entryInvoked != ZR_FALSE &&
                             result->processExitCode == 0 &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength));
        case ZR_AOT_RUNNER_STATUS_FAILED:
            return (TZrBool)(result->failure != ZR_AOT_RUNNER_FAILURE_NONE &&
                             result->entryInvoked != ZR_FALSE &&
                             result->actualBackend != ZR_AOT_BACKEND_UNKNOWN &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength));
        case ZR_AOT_RUNNER_STATUS_UNAVAILABLE:
            return (TZrBool)(result->actualBackend == ZR_AOT_BACKEND_UNKNOWN &&
                             result->entryInvoked == ZR_FALSE &&
                             result->failure != ZR_AOT_RUNNER_FAILURE_NONE);
        case ZR_AOT_RUNNER_STATUS_INVALID:
        case ZR_AOT_RUNNER_STATUS_COUNT:
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrTests_AotRunner_DescribeMatrix(const SZrAotRunner *runner,
                                         TZrUInt64 entryToken,
                                         SZrAotRunnerMatrix *matrix) {
    static const EZrAotBackend backends[] = {
            ZR_AOT_BACKEND_C,
            ZR_AOT_BACKEND_LLVM,
            ZR_AOT_BACKEND_INTERPRETER
    };
    TZrUInt32 index;
    TZrUInt32 entryIndex;

    if (runner == ZR_NULL || matrix == ZR_NULL || runner->initialized == ZR_FALSE ||
        entryToken == 0u) {
        return ZR_FALSE;
    }
    memset(matrix, 0, sizeof(*matrix));
    matrix->entryToken = entryToken;
    matrix->rowCount = (TZrUInt32)(sizeof(backends) / sizeof(backends[0]));
    for (index = 0u; index < matrix->rowCount; ++index) {
        matrix->rows[index].backend = backends[index];
        for (entryIndex = 0u; entryIndex < runner->entryCount; ++entryIndex) {
            if (runner->entries[entryIndex].backend == backends[index]) {
                matrix->rows[index].registeredEntries++;
                if (runner->entries[entryIndex].entryToken == entryToken) {
                    matrix->rows[index].available = ZR_TRUE;
                }
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrTests_AotRunner_BuildMatrix(const SZrAotRunner *runner,
                                      TZrUInt64 entryToken,
                                      SZrAotRunnerMatrix *matrix) {
    return ZrTests_AotRunner_DescribeMatrix(runner, entryToken, matrix);
}

const TZrChar *ZrTests_AotRunner_BackendName(EZrAotBackend backend) {
    switch (backend) {
        case ZR_AOT_BACKEND_C:
            return "aot_c";
        case ZR_AOT_BACKEND_LLVM:
            return "aot_llvm";
        case ZR_AOT_BACKEND_INTERPRETER:
            return "interpreter";
        case ZR_AOT_BACKEND_UNKNOWN:
        case ZR_AOT_BACKEND_COUNT:
        default:
            return "unknown";
    }
}

const TZrChar *ZrTests_AotRunner_StatusName(EZrAotRunnerStatus status) {
    switch (status) {
        case ZR_AOT_RUNNER_STATUS_RAN:
            return "RAN";
        case ZR_AOT_RUNNER_STATUS_FALLBACK:
            return "FALLBACK";
        case ZR_AOT_RUNNER_STATUS_UNAVAILABLE:
            return "UNAVAILABLE";
        case ZR_AOT_RUNNER_STATUS_FAILED:
            return "FAILED";
        case ZR_AOT_RUNNER_STATUS_INVALID:
        case ZR_AOT_RUNNER_STATUS_COUNT:
        default:
            return "INVALID";
    }
}

const TZrChar *ZrTests_AotRunner_FailureName(EZrAotRunnerFailure failure) {
    switch (failure) {
        case ZR_AOT_RUNNER_FAILURE_NONE:
            return "NONE";
        case ZR_AOT_RUNNER_FAILURE_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND:
            return "UNSUPPORTED_BACKEND";
        case ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE:
            return "ENTRY_UNAVAILABLE";
        case ZR_AOT_RUNNER_FAILURE_REGISTRY_FULL:
            return "REGISTRY_FULL";
        case ZR_AOT_RUNNER_FAILURE_DUPLICATE_ENTRY:
            return "DUPLICATE_ENTRY";
        case ZR_AOT_RUNNER_FAILURE_INVOCATION:
            return "INVOCATION_FAILED";
        case ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH:
            return "CHECKSUM_MISMATCH";
        case ZR_AOT_RUNNER_FAILURE_MALFORMED_COVERAGE:
            return "MALFORMED_COVERAGE";
        case ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK:
            return "INTERPRETER_FALLBACK";
        case ZR_AOT_RUNNER_FAILURE_COUNT:
        default:
            return "UNKNOWN";
    }
}
