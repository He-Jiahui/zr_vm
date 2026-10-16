/* Real Core regression: a permanently failing allocator must not recurse while
 * materializing MemoryError. The allocator itself never throws or recovers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;

static Allocation allocations[32768];
static size_t liveCount;
static size_t rejectedRequests;
static int allocatorError;
static int failAllAllocations;
static int callbackReturned;

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        for (; index < liveCount && allocations[index].pointer != old; ++index) {}
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) {
        allocatorError = 1;
        return NULL;
    }
    /* Deallocation remains available throughout the OOM interval. */
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
        }
        return NULL;
    }
    if (failAllAllocations) {
        ++rejectedRequests;
        if (rejectedRequests == 1) {
            fprintf(stderr, "persistent OOM armed: every nonzero request fails\n");
            fflush(stderr);
        }
        return NULL;
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
        fprintf(stderr, "line %d: %s (live=%zu rejected=%zu allocatorError=%d)\n", \
                __LINE__, #condition, liveCount, rejectedRequests, allocatorError); \
        return 1; \
    } \
} while (0)

static void allocate_under_oom(SZrState *state, TZrPtr arguments) {
    void *unexpected;
    (void)arguments;
    unexpected = ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_OBJECT, 256);
    if (unexpected != NULL) {
        ZrCore_Memory_RawFreeWithType(state->global, unexpected, 256,
                                    ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    callbackReturned = 1;
}

static void throw_runtime_error(SZrState *state, TZrPtr arguments) {
    (void)arguments;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    callbackReturned = 1;
}

static void throw_memory_error(SZrState *state, TZrPtr arguments) {
    (void)arguments;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    callbackReturned = 1;
}

static void return_normally(SZrState *state, TZrPtr arguments) {
    (void)state;
    (void)arguments;
    callbackReturned = 1;
}

static int persistent_oom(FZrTryFunction callback) {
    SZrGlobalState *global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    SZrState *state;
    EZrThreadStatus status;
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    CHECK(global->errorPrototype != NULL && global->memoryErrorMessage != NULL);
    state = global->mainThreadState;
    ZrCore_Exception_ClearCurrent(state);
    CHECK(!state->hasCurrentException);

    failAllAllocations = 1;
    status = ZrCore_Exception_TryRun(state, callback, NULL);
    /* Keep OOM armed until the protected call has actually returned. */
    CHECK(failAllAllocations && rejectedRequests > 0);
    CHECK(status == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(!callbackReturned && state->exceptionRecoverPoint == NULL);
    CHECK(!allocatorError);
    failAllAllocations = 0;

    /* OOM recovery must not leave exception handling permanently disabled. */
    ZrCore_Exception_ClearCurrent(state);
    status = ZrCore_Exception_TryRun(state, throw_runtime_error, NULL);
    CHECK(status == ZR_THREAD_STATUS_RUNTIME_ERROR && !callbackReturned);
    CHECK(state->hasCurrentException);
    CHECK(state->currentExceptionStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    CHECK(state->currentException.value.object != NULL);
    CHECK(state->exceptionRecoverPoint == NULL);
    ZrCore_Exception_ClearCurrent(state);
    CHECK(ZrCore_Exception_TryRun(state, return_normally, NULL) == ZR_THREAD_STATUS_FINE);
    CHECK(callbackReturned);
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

/* Ordinary allocation must still materialize MemoryError, while a previously
 * saved Error remains usable even if all later allocation requests fail. */
static int memory_error_semantics(int preserveExisting) {
    SZrGlobalState *global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    SZrState *state;
    SZrRawObject *savedError = NULL;
    EZrThreadStatus status;
    CHECK(global != NULL);
    state = global->mainThreadState;
    ZrCore_Exception_ClearCurrent(state);
    if (preserveExisting) {
        CHECK(ZrCore_Exception_TryRun(state, throw_runtime_error, NULL) ==
              ZR_THREAD_STATUS_RUNTIME_ERROR);
        CHECK(state->hasCurrentException && state->currentException.value.object != NULL);
        savedError = state->currentException.value.object;
        failAllAllocations = 1;
    }
    status = ZrCore_Exception_TryRun(state, throw_memory_error, NULL);
    CHECK(status == ZR_THREAD_STATUS_MEMORY_ERROR && !callbackReturned);
    CHECK(state->hasCurrentException);
    CHECK(state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    CHECK(state->currentException.value.object != NULL);
    CHECK(state->exceptionRecoverPoint == NULL);
    if (preserveExisting) {
        CHECK(state->currentException.value.object == savedError);
        CHECK(rejectedRequests == 0);
    } else {
        CHECK(state->currentExceptionStatus == ZR_THREAD_STATUS_MEMORY_ERROR);
    }
    failAllAllocations = 0;
    ZrCore_Exception_ClearCurrent(state);
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

static EZrThreadStatus innerStatus;
static int outerPointRestored;

static void nested_oom_callback(SZrState *state, TZrPtr arguments) {
    SZrExceptionLongJump *outerPoint = state->exceptionRecoverPoint;
    (void)arguments;
    failAllAllocations = 1;
    innerStatus = ZrCore_Exception_TryRun(state, allocate_under_oom, NULL);
    outerPointRestored = state->exceptionRecoverPoint == outerPoint;
    failAllAllocations = 0;
    ZrCore_Exception_ClearCurrent(state);
    /* The outer recovery point must still perform ordinary normalization. */
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    callbackReturned = 1;
}

static int nested_oom(void) {
    SZrGlobalState *global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    SZrState *state;
    CHECK(global != NULL);
    state = global->mainThreadState;
    ZrCore_Exception_ClearCurrent(state);
    CHECK(ZrCore_Exception_TryRun(state, nested_oom_callback, NULL) ==
          ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(innerStatus == ZR_THREAD_STATUS_MEMORY_ERROR && outerPointRestored);
    CHECK(rejectedRequests > 0 && !callbackReturned);
    CHECK(state->hasCurrentException);
    CHECK(state->currentExceptionStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    CHECK(state->currentException.value.object != NULL);
    CHECK(state->exceptionRecoverPoint == NULL);
    ZrCore_Exception_ClearCurrent(state);
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
    if (strcmp(argv[1], "persistent_allocation_oom") == 0)
        return persistent_oom(allocate_under_oom);
    if (strcmp(argv[1], "runtime_normalization_oom") == 0)
        return persistent_oom(throw_runtime_error);
    if (strcmp(argv[1], "normal_memory_error") == 0)
        return memory_error_semantics(0);
    if (strcmp(argv[1], "existing_error_oom") == 0)
        return memory_error_semantics(1);
    if (strcmp(argv[1], "nested_oom") == 0)
        return nested_oom();
    return 2;
}
