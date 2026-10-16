/* Exercise the real native result setter, including soft String_Create NULL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_binding.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError, failures, armed, rejected;
static TZrInt64 failFlag;
static TZrSize failSize;
static void *failOld;

static TZrPtr tracked_allocator(TZrPtr context, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)context;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) { allocatorError = 1; return NULL; }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        return NULL;
    }
    if (armed && old == failOld && flag == failFlag && newSize == failSize) {
        ++rejected;
        return NULL;
    }
    next = realloc(old, newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index] = (Allocation){next, newSize, flag};
    return next;
}

static void check(int condition, const char *message) {
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", message); }
}

typedef struct Call { const char *text; SZrTypeValue result; } Call;
static void invoke(SZrState *state, TZrPtr data) {
    Call *call = (Call *)data;
    ZrLib_Value_SetString(state, &call->result, call->text);
}

static void run_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    SZrHashSet *set = &global->stringTable->stringHashSet;
    char longText[ZR_VM_SHORT_STRING_MAX + 129U];
    Call call = {"publication-missing-string", {0}};
    EZrThreadStatus status;
    size_t index;
    memset(longText, 'q', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    if (strcmp(name, "normal_controls") == 0) {
        call.text = NULL;
    } else if (strcmp(name, "short_rehash") == 0) {
        for (index = 0; set->elementCount < set->resizeThreshold && index < 100000U; ++index) {
            char text[48];
            SZrString *fill;
            snprintf(text, sizeof(text), "publication-fill-%zu", index);
            fill = ZrCore_String_Create(state, text, strlen(text));
            if (fill == NULL) break;
        }
        check(set->elementCount == set->resizeThreshold, "actual intern table reaches its resize threshold");
        if (set->elementCount != set->resizeThreshold) return;
        failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET;
        failOld = set->buckets;
        failSize = set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR * sizeof(TZrPtr);
        armed = 1;
    } else {
        call.text = longText;
        failFlag = ZR_MEMORY_NATIVE_TYPE_STRING;
        failOld = NULL;
        failSize = sizeof(longText);
        armed = 1;
    }
    ZrLib_Value_SetInt(state, &call.result, 1337);
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    if (armed) {
        check(rejected > 0, "the intended string allocation was rejected");
        check(status == ZR_THREAD_STATUS_MEMORY_ERROR, "void setter reports creation failure as MemoryError");
        check(call.result.type == ZR_VALUE_TYPE_INT64 && call.result.value.nativeObject.nativeInt64 == 1337,
              "failed string creation preserves the previous result");
    } else {
        check(status == ZR_THREAD_STATUS_FINE && call.result.type == ZR_VALUE_TYPE_STRING,
              "NULL native input produces an empty string");
    }
    check(state->exceptionRecoverPoint == NULL, "the protected setter restores recovery chain");
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    check(status == ZR_THREAD_STATUS_FINE && call.result.type == ZR_VALUE_TYPE_STRING,
          "the same setter succeeds after removing the fault");
    if (status == ZR_THREAD_STATUS_FINE && call.result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *string = (SZrString *)ZrCore_Value_GetRawObject(&call.result);
        check(strcmp(ZrCore_String_GetNativeString(string), call.text != NULL ? call.text : "") == 0,
              "retry publishes the complete native text");
    }
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "short_rehash") != 0 &&
                      strcmp(argv[1], "long_payload") != 0 &&
                      strcmp(argv[1], "normal_controls") != 0)) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "Core initializes before injection");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global, argv[1]);
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "teardown releases all allocations with their original size and category");
    return failures == 0 ? 0 : 1;
}
