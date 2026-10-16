/* Real Core formatting/interning failures, one allocator seam per process.
 * No replacement implementation and no production-only test interface. */
#include <stdarg.h>
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
static int allocatorError, failures, armed;
static TZrInt64 failFlag;
static TZrSize failSize;

static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s (live=%zu rejected=%zu)\n",
            condition ? "PASS" : "FAIL", message, liveCount, rejected);
    if (!condition) ++failures;
}

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        return NULL;
    }
    if (armed && old == NULL && flag == failFlag && newSize == failSize) {
        ++rejected;
        if (rejected <= 4) {
            fprintf(stderr, "persistent rejection: flag=%lld bytes=%zu ordinal=%zu\n",
                    (long long)flag, (size_t)newSize, rejected);
            fflush(stderr);
        }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index] = (Allocation){next, newSize, flag};
    return next;
}

static size_t native_number_buffers(void) {
    size_t count = 0;
    for (size_t i = 0; i < liveCount; ++i)
        if (allocations[i].flag == ZR_MEMORY_NATIVE_TYPE_STRING &&
            allocations[i].size == ZR_NUMBER_TO_STRING_LENGTH_MAX) ++count;
    return count;
}

typedef enum Mode { FORMAT_PUSH, VFORMAT_CONCAT, FROM_NUMBER, SHORT_PAIR, NORMAL_FORMAT } Mode;
typedef struct Call {
    Mode mode;
    char *text;
    SZrString *result;
    TZrNativeString view;
    int returned;
} Call;

static TZrNativeString call_vformat(SZrState *state, TZrNativeString format, ...) {
    va_list args;
    TZrNativeString result;
    va_start(args, format);
    result = ZrCore_NativeString_VFormat(state, format, args);
    va_end(args);
    return result;
}

static void invoke(SZrState *state, TZrPtr data) {
    Call *call = (Call *)data;
    if (call->mode == FROM_NUMBER) {
        SZrTypeValue number;
        ZrCore_Value_InitAsInt(state, &number, INT64_C(3141592653589793));
        call->result = ZrCore_String_FromNumber(state, &number);
    } else if (call->mode == SHORT_PAIR) {
        call->result = ZrCore_String_CreateFromNative(state, call->text);
    } else if (call->mode == VFORMAT_CONCAT) {
        call->view = call_vformat(state, "%s", call->text);
    } else if (call->mode == NORMAL_FORMAT) {
        call->view = ZrCore_NativeString_Format(state, "prefix:%s:%d:%%", "ok", (TZrInt64)7);
    } else {
        call->view = ZrCore_NativeString_Format(state, "%s", call->text);
    }
    call->returned = 1;
}

static void prime_memory_error(SZrState *state, TZrPtr data) {
    (void)data;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
}

/* Compare content, not a head address/count that an actual OOM GC may change.
 * The String root list itself retains candidates while AddRawObject retries. */
static int short_list_contains(SZrGlobalState *global, const char *text) {
    size_t visited = 0;
    for (SZrString *string = global->stringTable->shortStringListHead;
         string != NULL; string = string->nextShortString) {
        if (++visited > 1000000U) { check(0, "short-string list has no cycle"); return 1; }
        if (ZrCore_String_GetByteLength(string) == strlen(text) &&
            strcmp(ZrCore_String_GetNativeString(string), text) == 0) return 1;
    }
    return 0;
}

static int intern_table_contains(SZrGlobalState *global, const char *text) {
    SZrHashSet *set = &global->stringTable->stringHashSet;
    for (TZrSize i = 0; i < set->capacity; ++i) {
        for (SZrHashKeyValuePair *pair = set->buckets[i]; pair != NULL; pair = pair->next) {
            SZrString *string = (SZrString *)ZrCore_Value_GetRawObject(&pair->key);
            if (string != NULL && ZrCore_String_GetByteLength(string) == strlen(text) &&
                strcmp(ZrCore_String_GetNativeString(string), text) == 0) return 1;
        }
    }
    return 0;
}

static TZrSize stack_depth(SZrState *state) {
    return (TZrSize)(state->stackTop.valuePointer - state->stackBase.valuePointer);
}

static void run_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    char longText[ZR_STRING_FORMAT_BUFFER_SIZE + 17U];
    char shortText[] = "format-oom-short-candidate-20261016";
    const char *expected;
    Call call = {0};
    SZrString *kept;
    SZrGcRootLease keepLease = {0};
    TZrSize oldDepth, roots;
    size_t oldNumberBuffers;
    EZrThreadStatus status;
    int formatCase;

    memset(longText, 'x', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    call.text = longText;
    if (strcmp(name, "format_push_null") == 0) call.mode = FORMAT_PUSH;
    else if (strcmp(name, "vformat_concat_null") == 0) call.mode = VFORMAT_CONCAT;
    else if (strcmp(name, "number_pair_throw") == 0) call.mode = FROM_NUMBER;
    else if (strcmp(name, "short_pair_throw") == 0) { call.mode = SHORT_PAIR; call.text = shortText; }
    else call.mode = NORMAL_FORMAT;
    formatCase = call.mode == FORMAT_PUSH || call.mode == VFORMAT_CONCAT || call.mode == NORMAL_FORMAT;
    /* stopGcFlag suppresses automatic steps, not GcMalloc's explicit full-GC
     * retry. The kept baseline has a real lease across either kind of GC. */
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    check(ZrCore_Stack_Grow(state, 16, ZR_FALSE), "format has spare stack slots before injection");
    status = ZrCore_Exception_TryRun(state, prime_memory_error, NULL);
    check(status == ZR_THREAD_STATUS_MEMORY_ERROR, "normalization is preheated without a fault");
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    kept = ZrCore_String_CreateFromNative(state, "format-oom-kept-baseline");
    check(kept != NULL, "baseline string is created before injection");
    if (kept == NULL) return;
    if (!ZrCore_Gc_RootLeaseAcquire(state, &kept->super, &keepLease)) {
        check(0, "baseline string receives a caller-owned lease");
        return;
    }
    roots = global->garbageCollector->ignoredObjectCount;
    oldDepth = stack_depth(state);
    oldNumberBuffers = native_number_buffers();
    if (call.mode == FORMAT_PUSH) {
        failFlag = ZR_MEMORY_NATIVE_TYPE_STRING;
        failSize = sizeof(longText);
    } else if (call.mode == VFORMAT_CONCAT) {
        failFlag = ZR_MEMORY_NATIVE_TYPE_ARRAY;
        failSize = 2 * sizeof(SZrString *);
    } else {
        failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_PAIR;
        failSize = sizeof(SZrHashKeyValuePair);
    }
    if (call.mode == SHORT_PAIR)
        check(!intern_table_contains(global, shortText) && !short_list_contains(global, shortText),
              "candidate text is absent from both real intern structures before injection");
    armed = call.mode != NORMAL_FORMAT;
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    if (armed) {
        check(rejected > 0, "the intended allocation is persistently rejected through protected return");
        if (formatCase) {
            check(status == ZR_THREAD_STATUS_MEMORY_ERROR && !call.returned,
                  "format failure raises protected MemoryError instead of publishing NULL");
            check(stack_depth(state) == oldDepth,
                  "failed format rolls back all partially pushed result slots");
        } else {
            check(status == ZR_THREAD_STATUS_MEMORY_ERROR && !call.returned && call.result == NULL,
                  "real HASH_PAIR GcMalloc failure propagates MemoryError");
        }
        if (call.mode == FROM_NUMBER)
            check(native_number_buffers() == oldNumberBuffers,
                  "FromNumber frees its native formatting buffer before propagating a create exception");
        if (call.mode == SHORT_PAIR)
            check(!short_list_contains(global, shortText) && !intern_table_contains(global, shortText),
                  "throwing intern insertion leaves no failed candidate in the short-string root chain");
    } else {
        check(status == ZR_THREAD_STATUS_FINE && call.returned && call.view != NULL &&
                      strcmp(call.view, "prefix:ok:7:%") == 0 && stack_depth(state) == oldDepth + 1,
              "normal formatting preserves placeholders and one rooted result slot");
    }
    check(state->exceptionRecoverPoint == NULL, "TryRun restores its recovery chain");
    check(ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &kept->super) &&
                  strcmp(ZrCore_String_GetNativeString(kept), "format-oom-kept-baseline") == 0,
          "failure preserves the caller's rooted baseline string");
    check(global->garbageCollector->ignoredObjectCount == roots,
          "formatting/intern failure leaves no extra explicit roots");
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    /* The rollback assertion above is evaluated first. Repaired formatting
     * needs no fixture repair; skip retry if RED returned with a corrupt window. */
    if (formatCase && stack_depth(state) != oldDepth && call.mode != NORMAL_FORMAT) goto cleanup;
    if (call.mode == NORMAL_FORMAT) state->stackTop.valuePointer = state->stackBase.valuePointer + oldDepth;
    call.returned = 0;
    call.view = NULL;
    call.result = NULL;
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    check(status == ZR_THREAD_STATUS_FINE && call.returned,
          "same operation retries normally after removing the fault");
    expected = call.mode == FROM_NUMBER ? "3141592653589793" :
               call.mode == NORMAL_FORMAT ? "prefix:ok:7:%" : call.text;
    if (formatCase) {
        check(call.view != NULL && strcmp(call.view, expected) == 0 && stack_depth(state) == oldDepth + 1,
              "format retry publishes the complete text in one stack slot");
        if (call.view != NULL && stack_depth(state) == oldDepth + 1) {
            const SZrTypeValue *top = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
            check(top->type == ZR_VALUE_TYPE_STRING && ZrCore_Value_GetRawObject(top) != NULL &&
                          ZrCore_String_GetNativeString((SZrString *)ZrCore_Value_GetRawObject(top)) == call.view,
                  "native result view belongs to the actual stack-rooted string");
        }
    } else {
        check(call.result != NULL && strcmp(ZrCore_String_GetNativeString(call.result), expected) == 0,
              "create/number retry contains the complete requested text");
    }
    check(native_number_buffers() == oldNumberBuffers, "retry retains no raw number-formatting buffer");
cleanup:
    armed = 0;
    ZrCore_Gc_RootLeaseRelease(global, &keepLease);
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "format_push_null") != 0 &&
        strcmp(argv[1], "vformat_concat_null") != 0 && strcmp(argv[1], "number_pair_throw") != 0 &&
        strcmp(argv[1], "short_pair_throw") != 0 && strcmp(argv[1], "normal_format") != 0)) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "actual Core initializes before injection");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global, argv[1]);
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "teardown releases every block with its original size and category");
    return failures == 0 ? 0 : 1;
}
