#include "testing/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct SZrCliTestModuleGroup {
    TZrSize start;
    TZrSize end;
} SZrCliTestModuleGroup;

typedef struct SZrCliTestWorker {
    SZrCliTestCaseResult *results;
    const SZrCliTestModuleGroup *groups;
    TZrSize groupCount;
    TZrSize workerIndex;
    TZrSize workerCount;
    TZrUInt64 timeoutMilliseconds;
    FZrCliTestCaseExecutor executor;
    TZrPtr userData;
} SZrCliTestWorker;

static const TZrChar *test_runner_module_id(const SZrParserTestEntry *entry) {
    return entry != ZR_NULL && entry->moduleId != ZR_NULL ? entry->moduleId : "main";
}

static void test_runner_append(TZrChar *buffer, TZrSize capacity, const TZrChar *text) {
    TZrSize used;

    if (buffer == ZR_NULL || capacity == 0U || text == ZR_NULL) {
        return;
    }
    used = strlen(buffer);
    if (used >= capacity - 1U) {
        return;
    }
    snprintf(buffer + used, capacity - used, "%s", text);
}

static void test_runner_append_constant(
        TZrChar *buffer,
        TZrSize capacity,
        const SZrParserTestConstant *constant) {
    TZrChar scratch[96];

    if (constant == ZR_NULL) {
        test_runner_append(buffer, capacity, "?");
        return;
    }
    switch (constant->kind) {
        case ZR_PARSER_TEST_CONSTANT_NULL:
            test_runner_append(buffer, capacity, "null");
            break;
        case ZR_PARSER_TEST_CONSTANT_BOOL:
            test_runner_append(buffer, capacity, constant->value.boolValue ? "true" : "false");
            break;
        case ZR_PARSER_TEST_CONSTANT_INT:
            snprintf(scratch, sizeof(scratch), "%lld", (long long)constant->value.intValue);
            test_runner_append(buffer, capacity, scratch);
            break;
        case ZR_PARSER_TEST_CONSTANT_UINT:
            snprintf(scratch, sizeof(scratch), "%llu", (unsigned long long)constant->value.uintValue);
            test_runner_append(buffer, capacity, scratch);
            break;
        case ZR_PARSER_TEST_CONSTANT_FLOAT:
            snprintf(scratch, sizeof(scratch), "%.17g", constant->value.floatValue);
            test_runner_append(buffer, capacity, scratch);
            break;
        case ZR_PARSER_TEST_CONSTANT_STRING:
            test_runner_append(buffer, capacity, "\"");
            test_runner_append(
                    buffer,
                    capacity,
                    constant->value.stringValue != ZR_NULL ? constant->value.stringValue : "");
            test_runner_append(buffer, capacity, "\"");
            break;
        default:
            test_runner_append(buffer, capacity, "?");
            break;
    }
}

static void test_runner_format_id(
        const SZrParserTestEntry *entry,
        const SZrParserTestCaseDescriptor *testCase,
        TZrUInt32 caseOrdinal,
        TZrChar *buffer,
        TZrSize capacity) {
    if (buffer == ZR_NULL || capacity == 0U) {
        return;
    }
    buffer[0] = '\0';
    snprintf(
            buffer,
            capacity,
            "%s::%s#%u(",
            test_runner_module_id(entry),
            entry != ZR_NULL && entry->qualifiedName != ZR_NULL ? entry->qualifiedName : "<unnamed>",
            (unsigned)caseOrdinal);
    if (testCase != ZR_NULL) {
        for (TZrUInt32 index = 0U; index < testCase->argumentCount; index++) {
            if (index > 0U) {
                test_runner_append(buffer, capacity, ",");
            }
            test_runner_append_constant(buffer, capacity, &testCase->arguments[index]);
        }
    }
    test_runner_append(buffer, capacity, ")");
}

static TZrUInt64 test_runner_hash_text(TZrUInt64 hash, const TZrChar *text) {
    const TZrUInt64 prime = UINT64_C(1099511628211);

    if (text == ZR_NULL) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= (TZrUInt8)*text++;
        hash *= prime;
    }
    return hash;
}

static TZrBool test_runner_match_pattern(const TZrChar *pattern, const TZrChar *text) {
    if (pattern == ZR_NULL || pattern[0] == '\0') {
        return ZR_TRUE;
    }
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    while (*pattern != '\0') {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0') {
                return ZR_TRUE;
            }
            while (*text != '\0') {
                if (test_runner_match_pattern(pattern, text)) {
                    return ZR_TRUE;
                }
                text++;
            }
            return test_runner_match_pattern(pattern, text);
        }
        if (*pattern != '?' && *pattern != *text) {
            return ZR_FALSE;
        }
        if (*text == '\0') {
            return ZR_FALSE;
        }
        pattern++;
        text++;
    }
    return *text == '\0' ? ZR_TRUE : ZR_FALSE;
}

static int test_runner_compare_results(const void *left, const void *right) {
    const SZrCliTestCaseResult *a = (const SZrCliTestCaseResult *)left;
    const SZrCliTestCaseResult *b = (const SZrCliTestCaseResult *)right;
    int compare = strcmp(test_runner_module_id(a->reference.entry),
                         test_runner_module_id(b->reference.entry));

    if (compare != 0) {
        return compare;
    }
    compare = strcmp(a->reference.entry->qualifiedName, b->reference.entry->qualifiedName);
    if (compare != 0) {
        return compare;
    }
    if (a->reference.caseOrdinal < b->reference.caseOrdinal) return -1;
    if (a->reference.caseOrdinal > b->reference.caseOrdinal) return 1;
    return 0;
}

static void test_runner_execute_case(SZrCliTestWorker *worker, TZrSize caseIndex) {
    SZrCliTestCaseResult *result = &worker->results[caseIndex];

    if (result->reference.entry->skipReason != ZR_NULL) {
        result->status = ZR_CLI_TEST_STATUS_SKIPPED;
        snprintf(result->message, sizeof(result->message), "%s", result->reference.entry->skipReason);
        return;
    }
    result->executed = ZR_TRUE;
    result->status = ZR_CLI_TEST_STATUS_CRASHED;
    if (worker->executor == ZR_NULL ||
        !worker->executor(
                &result->reference,
                worker->timeoutMilliseconds,
                result,
                worker->userData)) {
        if (result->message[0] == '\0') {
            snprintf(result->message, sizeof(result->message), "test isolate crashed");
        }
        result->status = ZR_CLI_TEST_STATUS_CRASHED;
        return;
    }
    if (worker->timeoutMilliseconds > 0U &&
        result->durationMilliseconds > worker->timeoutMilliseconds &&
        result->status != ZR_CLI_TEST_STATUS_CRASHED) {
        result->status = ZR_CLI_TEST_STATUS_TIMED_OUT;
        if (result->message[0] == '\0') {
            snprintf(result->message, sizeof(result->message), "test exceeded host timeout");
        }
    }
}

static void test_runner_worker_run(SZrCliTestWorker *worker) {
    for (TZrSize groupIndex = worker->workerIndex;
         groupIndex < worker->groupCount;
         groupIndex += worker->workerCount) {
        for (TZrSize caseIndex = worker->groups[groupIndex].start;
             caseIndex < worker->groups[groupIndex].end;
             caseIndex++) {
            test_runner_execute_case(worker, caseIndex);
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI test_runner_worker_entry(LPVOID argument) {
    test_runner_worker_run((SZrCliTestWorker *)argument);
    return 0U;
}
#else
static void *test_runner_worker_entry(void *argument) {
    test_runner_worker_run((SZrCliTestWorker *)argument);
    return ZR_NULL;
}
#endif

static void test_runner_count_results(SZrCliTestRunResult *result) {
    for (TZrSize index = 0U; index < result->caseCount; index++) {
        if (!result->cases[index].executed &&
            result->cases[index].status != ZR_CLI_TEST_STATUS_SKIPPED) {
            continue;
        }
        switch (result->cases[index].status) {
            case ZR_CLI_TEST_STATUS_PASSED: result->passedCount++; break;
            case ZR_CLI_TEST_STATUS_FAILED: result->failedCount++; break;
            case ZR_CLI_TEST_STATUS_SKIPPED: result->skippedCount++; break;
            case ZR_CLI_TEST_STATUS_TIMED_OUT: result->timedOutCount++; break;
            case ZR_CLI_TEST_STATUS_CRASHED: result->crashedCount++; break;
            default: break;
        }
    }
}

TZrBool ZrCli_TestRunner_Run(
        const SZrParserTestManifest *manifests,
        TZrSize manifestCount,
        const SZrCliTestRunnerOptions *options,
        FZrCliTestCaseExecutor executor,
        TZrPtr userData,
        SZrCliTestRunResult *outResult) {
    SZrCliTestRunnerOptions effective = {ZR_NULL, ZR_NULL, 1U, 0U, ZR_FALSE};
    SZrCliTestModuleGroup *groups = ZR_NULL;
    SZrCliTestWorker *workers = ZR_NULL;
    TZrSize unfilteredCount = 0U;
    TZrSize resultIndex = 0U;
    TZrSize groupCount = 0U;
    TZrSize workerCount;
    TZrBool success = ZR_TRUE;
    TZrUInt64 seed = UINT64_C(1469598103934665603);

    if (outResult == ZR_NULL || (manifestCount > 0U && manifests == ZR_NULL)) {
        return ZR_FALSE;
    }
    memset(outResult, 0, sizeof(*outResult));
    if (options != ZR_NULL) {
        effective = *options;
    }
    if (effective.jobs == 0U) {
        effective.jobs = 1U;
    }
    for (TZrSize manifestIndex = 0U; manifestIndex < manifestCount; manifestIndex++) {
        for (TZrUInt32 entryIndex = 0U; entryIndex < manifests[manifestIndex].entryCount; entryIndex++) {
            const SZrParserTestEntry *entry = &manifests[manifestIndex].entries[entryIndex];
            TZrSize entryCaseCount = entry->caseCount > 0U ? (TZrSize)entry->caseCount : 1U;

            if (entryCaseCount > (TZrSize)-1 - unfilteredCount) {
                return ZR_FALSE;
            }
            unfilteredCount += entryCaseCount;
        }
    }
    if (unfilteredCount > 0U) {
        outResult->cases = (SZrCliTestCaseResult *)calloc(
                unfilteredCount, sizeof(SZrCliTestCaseResult));
        if (outResult->cases == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (TZrSize manifestIndex = 0U; manifestIndex < manifestCount; manifestIndex++) {
        for (TZrUInt32 entryIndex = 0U; entryIndex < manifests[manifestIndex].entryCount; entryIndex++) {
            const SZrParserTestEntry *entry = &manifests[manifestIndex].entries[entryIndex];
            TZrUInt32 count = entry->caseCount > 0U ? entry->caseCount : 1U;
            for (TZrUInt32 caseIndex = 0U; caseIndex < count; caseIndex++) {
                const SZrParserTestCaseDescriptor *testCase =
                        entry->caseCount > 0U ? &entry->cases[caseIndex] : ZR_NULL;
                TZrUInt32 ordinal = testCase != ZR_NULL ? testCase->ordinal : 0U;
                SZrCliTestCaseResult candidate;

                memset(&candidate, 0, sizeof(candidate));
                candidate.reference.entry = entry;
                candidate.reference.testCase = testCase;
                candidate.reference.caseOrdinal = ordinal;
                test_runner_format_id(
                        entry, testCase, ordinal, candidate.reference.id, sizeof(candidate.reference.id));
                if ((effective.exactCaseId != ZR_NULL &&
                     strcmp(effective.exactCaseId, candidate.reference.id) != 0) ||
                    (effective.exactCaseId == ZR_NULL &&
                     !test_runner_match_pattern(effective.filterPattern, candidate.reference.id))) {
                    continue;
                }
                outResult->cases[resultIndex++] = candidate;
            }
        }
    }
    outResult->caseCount = resultIndex;
    if (outResult->caseCount > 1U) {
        qsort(outResult->cases,
              outResult->caseCount,
              sizeof(SZrCliTestCaseResult),
              test_runner_compare_results);
    }
    for (TZrSize index = 0U; index < outResult->caseCount; index++) {
        seed = test_runner_hash_text(seed, outResult->cases[index].reference.id);
    }
    outResult->seed = seed;
    if (effective.listOnly || outResult->caseCount == 0U) {
        outResult->jobsUsed = 0U;
        return ZR_TRUE;
    }

    groups = (SZrCliTestModuleGroup *)calloc(
            outResult->caseCount, sizeof(SZrCliTestModuleGroup));
    if (groups == ZR_NULL) {
        ZrCli_TestRunner_Free(outResult);
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < outResult->caseCount;) {
        TZrSize end = index + 1U;
        while (end < outResult->caseCount &&
               strcmp(test_runner_module_id(outResult->cases[index].reference.entry),
                      test_runner_module_id(outResult->cases[end].reference.entry)) == 0) {
            end++;
        }
        groups[groupCount].start = index;
        groups[groupCount].end = end;
        groupCount++;
        index = end;
    }
    workerCount = effective.jobs < groupCount ? effective.jobs : groupCount;
    outResult->jobsUsed = (TZrUInt32)workerCount;
    workers = (SZrCliTestWorker *)calloc(workerCount, sizeof(SZrCliTestWorker));
    if (workers == ZR_NULL) {
        free(groups);
        ZrCli_TestRunner_Free(outResult);
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < workerCount; index++) {
        workers[index].results = outResult->cases;
        workers[index].groups = groups;
        workers[index].groupCount = groupCount;
        workers[index].workerIndex = index;
        workers[index].workerCount = workerCount;
        workers[index].timeoutMilliseconds = effective.timeoutMilliseconds;
        workers[index].executor = executor;
        workers[index].userData = userData;
    }
    if (workerCount == 1U) {
        test_runner_worker_run(&workers[0]);
    } else {
#if defined(_WIN32)
        HANDLE *handles = (HANDLE *)calloc(workerCount, sizeof(HANDLE));
        if (handles == ZR_NULL) {
            success = ZR_FALSE;
        } else {
            for (TZrSize index = 0U; index < workerCount; index++) {
                handles[index] = CreateThread(ZR_NULL, 0U, test_runner_worker_entry, &workers[index], 0U, ZR_NULL);
                if (handles[index] == ZR_NULL) {
                    success = ZR_FALSE;
                    workerCount = index;
                    break;
                }
            }
            if (workerCount > 0U) {
                WaitForMultipleObjects((DWORD)workerCount, handles, TRUE, INFINITE);
            }
            for (TZrSize index = 0U; index < workerCount; index++) CloseHandle(handles[index]);
            free(handles);
        }
#else
        pthread_t *threads = (pthread_t *)calloc(workerCount, sizeof(pthread_t));
        TZrSize created = 0U;
        if (threads == ZR_NULL) {
            success = ZR_FALSE;
        } else {
            for (; created < workerCount; created++) {
                if (pthread_create(&threads[created], ZR_NULL, test_runner_worker_entry, &workers[created]) != 0) {
                    success = ZR_FALSE;
                    break;
                }
            }
            for (TZrSize index = 0U; index < created; index++) pthread_join(threads[index], ZR_NULL);
            free(threads);
        }
#endif
    }
    free(workers);
    free(groups);
    if (!success) {
        ZrCli_TestRunner_Free(outResult);
        return ZR_FALSE;
    }
    test_runner_count_results(outResult);
    return ZR_TRUE;
}

void ZrCli_TestRunner_Free(SZrCliTestRunResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    free(result->cases);
    memset(result, 0, sizeof(*result));
}

int ZrCli_TestRunner_ExitCode(const SZrCliTestRunResult *result) {
    if (result == ZR_NULL) return 3;
    if (result->crashedCount > 0U) return 3;
    if (result->failedCount > 0U || result->timedOutCount > 0U) return 1;
    return 0;
}

const TZrChar *ZrCli_TestRunner_StatusName(EZrCliTestStatus status) {
    switch (status) {
        case ZR_CLI_TEST_STATUS_PASSED: return "Passed";
        case ZR_CLI_TEST_STATUS_FAILED: return "Failed";
        case ZR_CLI_TEST_STATUS_SKIPPED: return "Skipped";
        case ZR_CLI_TEST_STATUS_TIMED_OUT: return "TimedOut";
        case ZR_CLI_TEST_STATUS_CRASHED: return "Crashed";
        default: return "Crashed";
    }
}
