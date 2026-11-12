/* Real Core regression: a secondary state without a local recovery point
 * forwards its Error to the main state's recovery point. The worker is reset
 * on that path, so the Error must be published on the main state first.
 * Both states run on one native thread; cross-thread longjmp is not covered. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#define WORKER_MESSAGE "worker forwarded payload"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;

static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError;
static int callbackReturned;
static int workerLaunched;
static int workerErrorBuilt;
static SZrState *worker;
static SZrRawObject *workerError;

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
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
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
        fprintf(stderr, "line %d: %s (live=%zu allocatorError=%d)\n", \
                __LINE__, #condition, liveCount, allocatorError); \
        return 1; \
    } \
} while (0)

/* Runs under the main state's TryRun. The worker never installs a recovery
 * point, so its Throw must leave through the main state's one. */
static void worker_throws_without_recovery_point(SZrState *mainState, TZrPtr arguments) {
    SZrString *text;
    SZrTypeValue payload;
    (void)arguments;
    worker = ZrCore_State_New(mainState->global);
    if (worker == NULL || !ZrCore_State_MutatorLaunch(worker)) return;
    workerLaunched = 1;
    text = ZrCore_String_CreateFromNative(worker, (TZrNativeString)WORKER_MESSAGE);
    if (text == NULL) return;
    ZrCore_Value_InitAsRawObject(worker, &payload, &text->super);
    payload.type = ZR_VALUE_TYPE_STRING;
    if (!ZrCore_Exception_NormalizeThrownValue(worker, &payload, worker->callInfoList,
                                               ZR_THREAD_STATUS_RUNTIME_ERROR) ||
        !worker->hasCurrentException || worker->exceptionRecoverPoint != NULL) {
        return;
    }
    workerError = worker->currentException.value.object;
    workerErrorBuilt = 1;
    ZrCore_Exception_Throw(worker, ZR_THREAD_STATUS_RUNTIME_ERROR);
    callbackReturned = 1;
}

static int current_message_is_worker_payload(SZrState *state) {
    SZrString *key = ZrCore_String_CreateFromNative(state, (TZrNativeString)"message");
    SZrTypeValue keyValue;
    const SZrTypeValue *message;
    if (key == NULL || state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == NULL) {
        return 0;
    }
    ZrCore_Value_InitAsRawObject(state, &keyValue, &key->super);
    keyValue.type = ZR_VALUE_TYPE_STRING;
    message = ZrCore_Object_GetValue(
            state, ZR_CAST_OBJECT(state, state->currentException.value.object), &keyValue);
    return message != NULL && message->type == ZR_VALUE_TYPE_STRING &&
           message->value.object != NULL &&
           strcmp(ZrCore_String_GetNativeString((SZrString *)message->value.object),
                  WORKER_MESSAGE) == 0;
}

int main(void) {
    SZrGlobalState *global;
    SZrState *state;
    EZrThreadStatus status;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    CHECK(global->errorPrototype != NULL);
    state = global->mainThreadState;
    ZrCore_Exception_ClearCurrent(state);

    status = ZrCore_Exception_TryRun(state, worker_throws_without_recovery_point, NULL);
    CHECK(workerLaunched && workerErrorBuilt && !callbackReturned);
    CHECK(status == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(state->exceptionRecoverPoint == NULL);

    /* The main state holds the worker's own Error, not an empty slot or a
     * replacement built by status normalization. */
    CHECK(state->hasCurrentException);
    CHECK(state->currentExceptionStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    CHECK(state->currentException.value.object == workerError);
    CHECK(current_message_is_worker_payload(state));

    /* The worker itself was reset for reuse. */
    CHECK(!worker->hasCurrentException);
    CHECK(worker->currentExceptionStatus == ZR_THREAD_STATUS_FINE);
    CHECK(worker->currentException.type == ZR_VALUE_TYPE_NULL);
    CHECK(worker->exceptionRecoverPoint == NULL);

    /* Only the main state's slot keeps the Error reachable now. */
    ZrCore_State_MutatorExit(worker);
    ZrCore_State_Free(global, worker);
    worker = NULL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    CHECK(state->hasCurrentException);
    CHECK(current_message_is_worker_payload(state));

    ZrCore_Exception_ClearCurrent(state);
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}
