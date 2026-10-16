/* Real Error construction under allocator-driven full collection. */
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
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount, objectAttempts, objectsCreated, rejected, prematureFrees;
static void *constructionObjects[2];
static SZrGlobalState *activeGlobal;
static int failures, allocatorError, armed, faultStarted, mode;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    int watchObject = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (i < liveCount && allocations[i].pointer != old) ++i;
        if (i == liveCount || allocations[i].size != oldSize || allocations[i].flag != flag) {
            allocatorError = 1; return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) {
            if (armed && faultStarted &&
                (old == constructionObjects[0] || old == constructionObjects[1])) {
                ++prematureFrees;
                fprintf(stderr, "active construction object released during collection: %p\n", old);
            }
            free(old); allocations[i] = allocations[--liveCount];
        }
        return NULL;
    }
    if (armed && old == NULL && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT) {
        if (newSize == sizeof(SZrObject)) {
            ++objectAttempts;
            watchObject = objectsCreated < 2;
            if (mode == 1 && objectAttempts >= 2) faultStarted = 1;
        }
        if (mode == 2 && objectsCreated == 2 && newSize == sizeof(SZrString) + ZR_VM_SHORT_STRING_MAX) {
            faultStarted = 1;
        }
        if (faultStarted &&
            ((mode == 1 && newSize == sizeof(SZrObject)) ||
             (mode == 2 && newSize == sizeof(SZrString) + ZR_VM_SHORT_STRING_MAX))) {
            /* Setup pauses automatic work. Permit the production OOM full-GC
             * fallback to finish; a paused sweep need not increment telemetry. */
            activeGlobal->garbageCollector->stopGcFlag = ZR_FALSE;
            ++rejected;
            if (rejected <= 4) fprintf(stderr, "reject object bytes=%zu attempt=%zu\n", (size_t)newSize, rejected);
            return NULL;
        }
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i] = (Allocation){next, newSize, flag};
    if (watchObject) constructionObjects[objectsCreated++] = next;
    return next;
}

static int intern_contains(SZrGlobalState *global, const char *text) {
    SZrHashSet *set = &global->stringTable->stringHashSet;
    for (TZrSize i = 0; i < set->capacity; ++i) {
        for (SZrHashKeyValuePair *pair = set->buckets[i]; pair != NULL; pair = pair->next) {
            SZrString *string = (SZrString *)ZrCore_Value_GetRawObject(&pair->key);
            if (string != NULL && strcmp(ZrCore_String_GetNativeString(string), text) == 0) return 1;
        }
    }
    return 0;
}

static void throw_memory_error(SZrState *state, TZrPtr data) {
    (void)data;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
}

static const SZrTypeValue *read_field(SZrState *state, SZrObject *object, const char *name) {
    SZrString *text = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    SZrTypeValue key;
    CHECK(text != NULL);
    if (text == NULL) return NULL;
    ZrCore_Value_InitAsRawObject(state, &key, &text->super);
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void check_completed_error(SZrState *state) {
    const SZrTypeValue *message, *frames;
    SZrObject *error;
    CHECK(state->hasCurrentException && state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    if (!state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT) return;
    error = ZR_CAST_OBJECT(state, state->currentException.value.object);
    CHECK(error != NULL && error->prototype != NULL);
    if (error == NULL) return;
    message = read_field(state, error, "message");
    CHECK(message != NULL && message->type == ZR_VALUE_TYPE_STRING);
    frames = read_field(state, error, "stacks");
    CHECK(frames != NULL && frames->type == ZR_VALUE_TYPE_ARRAY && frames->value.object != NULL);
}

static void run_case(SZrGlobalState *global) {
    SZrState *state = global->mainThreadState;
    SZrAotGcRootFrame *rootTop;
    TZrUInt32 rootDepth;
    TZrSize stackDepth, leases;
    SZrGarbageCollectorStatsSnapshot before, after;
    EZrThreadStatus status;
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    CHECK(ZrCore_Stack_Grow(state, 64, ZR_FALSE));
    CHECK(ZrCore_String_CreateFromNative(state, "MemoryError") != NULL);
    CHECK(!intern_contains(global, "stacks"));
    if (intern_contains(global, "stacks")) return;
    /* Give normalization an already rooted string payload without prebuilding
     * an Error or any of its field names. No metadata frame is fabricated. */
    stackDepth = (TZrSize)(state->stackTop.valuePointer - state->stackBase.valuePointer);
    ZrCore_Value_InitAsRawObject(state, &state->stackTop.valuePointer->value, &global->memoryErrorMessage->super);
    state->stackTop.valuePointer->value.type = ZR_VALUE_TYPE_STRING;
    state->stackTop.valuePointer++;
    rootTop = state->aotGcRootFrameStack; rootDepth = state->aotGcRootFrameDepth;
    leases = global->garbageCollector->ignoredObjectCount;
    ZrCore_GarbageCollector_GetStatsSnapshot(global, &before);
    activeGlobal = global;
    armed = 1;
    status = ZrCore_Exception_TryRun(state, throw_memory_error, NULL);
    armed = 0;
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    ZrCore_GarbageCollector_GetStatsSnapshot(global, &after);
    CHECK(status == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(state->exceptionRecoverPoint == NULL);
    CHECK(state->aotGcRootFrameStack == rootTop && state->aotGcRootFrameDepth == rootDepth);
    CHECK(global->garbageCollector->ignoredObjectCount == leases);
    if (mode != 0) {
        CHECK(rejected >= 2);
        CHECK(objectsCreated == (mode == 1 ? 1u : 2u));
        CHECK(after.fullCollectionCount > before.fullCollectionCount);
        CHECK(prematureFrees == 0);
        fprintf(stderr, "objects=%zu rejects=%zu premature=%zu full collections=%llu\n",
                objectsCreated, rejected, prematureFrees,
                (unsigned long long)(after.fullCollectionCount - before.fullCollectionCount));
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        CHECK(ZrCore_Exception_TryRun(state, throw_memory_error, NULL) == ZR_THREAD_STATUS_MEMORY_ERROR);
    } else {
        CHECK(objectsCreated == 2 && rejected == 0 && prematureFrees == 0);
    }
    check_completed_error(state);
    CHECK(state->exceptionRecoverPoint == NULL);
    CHECK(state->aotGcRootFrameStack == rootTop && state->aotGcRootFrameDepth == rootDepth);
    CHECK(global->garbageCollector->ignoredObjectCount == leases);
    state->stackTop.valuePointer = state->stackBase.valuePointer + stackDepth;
    ZrCore_Exception_ClearCurrent(state);
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "error_during_frames_gc") == 0) mode = 1;
    else if (strcmp(argv[1], "frames_during_key_gc") == 0) mode = 2;
    else if (strcmp(argv[1], "normal_control") != 0) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global);
        armed = 0; ZrCore_GlobalState_Free(global);
    }
    CHECK(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
