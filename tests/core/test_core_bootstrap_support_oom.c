/* Bootstrap allocation failures against the real Core, with exact size/type
 * accounting. Each CTest case runs in its own process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;

static Allocation allocations[32768];
static size_t liveCount;
static size_t requestOrdinal;
static size_t matchingOrdinal;
static TZrSize failureSize;
static TZrInt64 failureFlag;
static int failureEnabled;
static int failureTriggered;
static int allocatorError;

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        for (; index < liveCount && allocations[index].pointer != old; ++index) {}
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) {
            fprintf(stderr, "allocator contract mismatch: oldSize=%zu flag=%lld\n",
                    (size_t)oldSize, (long long)flag);
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) {
        allocatorError = 1;
        return NULL;
    }
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
        }
        return NULL;
    }
    ++requestOrdinal;
    if (failureEnabled && old == NULL && flag == failureFlag && newSize == failureSize) {
        ++matchingOrdinal;
        if (matchingOrdinal == 1) {
            failureTriggered = 1;
            fprintf(stderr, "bootstrap OOM: category=%lld bytes=%zu matching_ordinal=%zu allocation_ordinal=%zu\n",
                    (long long)flag, (size_t)newSize, matchingOrdinal, requestOrdinal);
            fflush(stderr);
            return NULL;
        }
    }
    next = realloc(old, newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s (live=%zu triggered=%d allocatorError=%d)\n", \
                __LINE__, #condition, liveCount, failureTriggered, allocatorError); \
        return 1; \
    } \
} while (0)

static int constructor_failure(TZrInt64 flag, TZrSize size) {
    SZrGlobalState *global;
    failureFlag = flag;
    failureSize = size;
    failureEnabled = 1;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(failureTriggered);
    if (global != NULL) {
        /* Release unexpected success before reporting the failed contract. */
        ZrCore_GlobalState_Free(global);
        fprintf(stderr, "constructor unexpectedly succeeded after injected OOM\n");
        return 1;
    }
    CHECK(liveCount == 0 && !allocatorError);
    failureEnabled = 0;
    /* A failed constructor must not poison a later, successful bootstrap. */
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 2, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "gc_manager_oom") == 0)
        return constructor_failure(ZR_MEMORY_NATIVE_TYPE_MANAGER, sizeof(SZrGarbageCollector));
    if (strcmp(argv[1], "string_table_manager_oom") == 0)
        return constructor_failure(ZR_MEMORY_NATIVE_TYPE_MANAGER, sizeof(SZrStringTable));
    if (strcmp(argv[1], "string_table_bucket_oom") == 0)
        return constructor_failure(ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET,
                ((TZrSize)1 << ZR_STRING_TABLE_INITIAL_SIZE_LOG2) * sizeof(TZrPtr));
    return 2;
}
