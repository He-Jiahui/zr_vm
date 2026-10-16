/* Actual Value_ConvertToString and native meta callbacks, one fault per process. */
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
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount, rejected;
static int failures, allocatorError, armed;
static void *failOld;
static TZrSize failSize;
static TZrInt64 failFlag;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (i < liveCount && allocations[i].pointer != old) ++i;
        if (i == liveCount || allocations[i].size != oldSize || allocations[i].flag != flag) {
            allocatorError = 1; return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[i] = allocations[--liveCount]; }
        return NULL;
    }
    if (armed && old == failOld && newSize == failSize && flag == failFlag) {
        ++rejected;
        if (rejected <= 4) {
            fprintf(stderr, "persistent rejection: category=%lld old=%p bytes=%zu ordinal=%zu\n",
                    (long long)flag, old, (size_t)newSize, rejected);
            fflush(stderr);
        }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i] = (Allocation){next, newSize, flag};
    return next;
}

typedef struct Call { SZrTypeValue input; SZrString *result; int returned; } Call;
static void invoke(SZrState *state, TZrPtr data) {
    Call *call = (Call *)data;
    SZrString *converted = ZrCore_Value_ConvertToString(state, &call->input);
    if (converted != NULL) call->result = converted;
    call->returned = 1;
}
static void prime_error(SZrState *state, TZrPtr data) {
    (void)data;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
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
static void set_input(SZrState *state, Call *call, const char *name) {
    if (strcmp(name, "null") == 0) ZrCore_Value_ResetAsNull(&call->input);
    else if (strcmp(name, "number") == 0) ZrCore_Value_InitAsInt(state, &call->input, INT64_C(3141592653589793));
    else ZrCore_Value_InitAsBool(state, &call->input, (TZrBool)(strcmp(name, "bool_true") == 0));
}
static const char *expected_text(const char *name) {
    if (strcmp(name, "null") == 0) return "null";
    if (strcmp(name, "number") == 0) return "3141592653589793";
    return strcmp(name, "bool_true") == 0 ? "true" : "false";
}
static void run_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    SZrHashSet *set = &global->stringTable->stringHashSet;
    SZrGcRootLease sentinelLease = {0};
    SZrString *sentinel;
    TZrSize oldDepth, roots;
    EZrThreadStatus status;
    Call call = {0};
    int normal = strcmp(name, "normal_controls") == 0;
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    CHECK(ZrCore_Stack_Grow(state, 32, ZR_FALSE));
    /* Prime Error fields with a STRING payload, so null/bool conversions are
     * not accidentally interned before their own failure seam is armed. */
    oldDepth = (TZrSize)(state->stackTop.valuePointer - state->stackBase.valuePointer);
    ZrCore_Value_InitAsRawObject(state, &state->stackTop.valuePointer->value, &global->memoryErrorMessage->super);
    state->stackTop.valuePointer->value.type = ZR_VALUE_TYPE_STRING;
    state->stackTop.valuePointer++;
    CHECK(ZrCore_Exception_TryRun(state, prime_error, NULL) == ZR_THREAD_STATUS_MEMORY_ERROR);
    state->stackTop.valuePointer = state->stackBase.valuePointer + oldDepth;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    sentinel = ZrCore_String_CreateFromNative(state, "meta-oom-caller-result");
    CHECK(sentinel != NULL);
    if (sentinel == NULL) return;
    CHECK(ZrCore_Gc_RootLeaseAcquire(state, &sentinel->super, &sentinelLease));
    if (sentinelLease.object == NULL) return;
    roots = global->garbageCollector->ignoredObjectCount;
    if (!normal) {
        set_input(state, &call, name);
        call.result = sentinel;
        if (strcmp(name, "number") == 0) {
            failOld = NULL; failFlag = ZR_MEMORY_NATIVE_TYPE_STRING;
            failSize = ZR_NUMBER_TO_STRING_LENGTH_MAX;
        } else {
            CHECK(!intern_contains(global, expected_text(name)));
            if (intern_contains(global, expected_text(name))) goto cleanup;
            for (size_t i = 0; set->elementCount < set->resizeThreshold && i < 100000U; ++i) {
                char fill[48];
                snprintf(fill, sizeof(fill), "meta-oom-fill-%zu", i);
                if (ZrCore_String_CreateFromNative(state, fill) == NULL) break;
            }
            CHECK(set->elementCount == set->resizeThreshold);
            if (set->elementCount != set->resizeThreshold) goto cleanup;
            failOld = set->buckets; failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET;
            failSize = set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR * sizeof(TZrPtr);
        }
        armed = 1;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        CHECK(rejected > 0);
        CHECK(status == ZR_THREAD_STATUS_MEMORY_ERROR && !call.returned);
        CHECK(call.result == sentinel && strcmp(ZrCore_String_GetNativeString(call.result), "meta-oom-caller-result") == 0);
        CHECK(state->exceptionRecoverPoint == NULL);
        CHECK(global->garbageCollector->ignoredObjectCount == roots);
        armed = 0;
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        call.returned = 0;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        CHECK(status == ZR_THREAD_STATUS_FINE && call.returned && call.result != NULL);
        if (call.result != NULL) CHECK(strcmp(ZrCore_String_GetNativeString(call.result), expected_text(name)) == 0);
    } else {
        static const char *const names[] = {"null", "bool_true", "bool_false", "number"};
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            set_input(state, &call, names[i]); call.returned = 0; call.result = sentinel;
            CHECK(ZrCore_Exception_TryRun(state, invoke, &call) == ZR_THREAD_STATUS_FINE);
            CHECK(call.returned && call.result != NULL);
            if (call.result != NULL) CHECK(strcmp(ZrCore_String_GetNativeString(call.result), expected_text(names[i])) == 0);
        }
    }
cleanup:
    armed = 0;
    ZrCore_Gc_RootLeaseRelease(global, &sentinelLease);
}
int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "null") != 0 && strcmp(argv[1], "bool_true") != 0 &&
        strcmp(argv[1], "bool_false") != 0 && strcmp(argv[1], "number") != 0 &&
        strcmp(argv[1], "normal_controls") != 0)) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global, argv[1]);
        armed = 0; ZrCore_GlobalState_Free(global);
    }
    CHECK(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
