/* Real FFI aggregate conversion must close its TempValueRoot on setter OOM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "ffi_runtime/ffi_runtime_internal.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount, rejected;
static int allocatorError, failures, armed;

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
    if (armed && old == NULL && flag == ZR_MEMORY_NATIVE_TYPE_HASH_PAIR &&
        newSize == sizeof(SZrHashKeyValuePair)) {
        ++rejected;
        if (rejected <= 4) {
            fprintf(stderr, "persistent pair rejection: flag=%lld bytes=%zu ordinal=%zu\n",
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
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

typedef struct Conversion {
    ZrFfiTypeLayout aggregate;
    ZrFfiTypeLayout scalar;
    ZrFfiFieldLayout field;
    TZrInt32 input;
    SZrTypeValue output;
    TZrBool succeeded;
    int returned;
} Conversion;

static void convert(SZrState *state, TZrPtr data) {
    Conversion *conversion = (Conversion *)data;
    conversion->succeeded = zr_ffi_struct_to_object(state, &conversion->aggregate,
            (const unsigned char *)&conversion->input, &conversion->output);
    conversion->returned = 1;
}

static int scopes_equal(const SZrGcDomainScopeSnapshot *before, const SZrGcDomainScopeSnapshot *after) {
    return before->domain == after->domain && before->identity.id == after->identity.id &&
           before->identity.generation == after->identity.generation &&
           before->mutatorId == after->mutatorId && before->executionDepth == after->executionDepth &&
           before->nativeDepth == after->nativeDepth && before->mutationDepth == after->mutationDepth &&
           before->nativeMode == after->nativeMode &&
           before->nativeEnteredFromInactive == after->nativeEnteredFromInactive;
}

static void field_pair_oom(SZrGlobalState *global) {
    char fieldName[] = "ffiCleanupField";
    SZrState *state = global->mainThreadState;
    SZrString *key;
    SZrGcRootLease keyLease = {0};
    Conversion conversion;
    SZrGcDomainScopeSnapshot before, after;
    SZrAotGcRootFrame *aotTop;
    TZrUInt32 aotDepth, nestedCalls;
    TZrSize roots;
    TZrStackValuePointer stackTop;
    SZrCallInfo *callInfo;
    SZrCallInfo savedCallInfo;
    EZrThreadStatus status;
    const SZrTypeValue *stored;
    memset(&conversion, 0, sizeof(conversion));
    /* Use the target's real private ABI, including ZR_VM_HAS_LIBFFI layout. */
    conversion.scalar.kind = ZR_FFI_TYPE_I32;
    conversion.scalar.size = sizeof(TZrInt32);
    conversion.scalar.align = sizeof(TZrInt32);
    conversion.field.name = fieldName;
    conversion.field.type = &conversion.scalar;
    conversion.aggregate.kind = ZR_FFI_TYPE_STRUCT;
    conversion.aggregate.size = sizeof(TZrInt32);
    conversion.aggregate.align = sizeof(TZrInt32);
    conversion.aggregate.as.aggregate.fieldCount = 1;
    conversion.aggregate.as.aggregate.fields = &conversion.field;
    conversion.input = 24681357;
    ZrCore_Value_InitAsInt(state, &conversion.output, 707);
    /* Pre-intern and retain the key, so rejection reaches the aggregate field's
     * actual storage pair, not the short-string table's own pair allocation. */
    key = ZrCore_String_CreateFromNative(state, fieldName);
    check(key != NULL, "field name is pre-interned before injection");
    if (key == NULL) return;
    if (!ZrCore_Gc_RootLeaseAcquire(state, &key->super, &keyLease)) {
        check(0, "caller holds the pre-interned key");
        return;
    }
    roots = global->garbageCollector->ignoredObjectCount;
    stackTop = state->stackTop.valuePointer;
    callInfo = state->callInfoList;
    memset(&savedCallInfo, 0, sizeof(savedCallInfo));
    if (callInfo != NULL) savedCallInfo = *callInfo;
    aotTop = state->aotGcRootFrameStack;
    aotDepth = state->aotGcRootFrameDepth;
    nestedCalls = state->nestedNativeCalls;
    ZrCore_GcDomain_CaptureScopes(state, &before);
    armed = 1;
    status = ZrCore_Exception_TryRun(state, convert, &conversion);
    ZrCore_GcDomain_CaptureScopes(state, &after);
    check(armed && rejected > 0, "exact HASH_PAIR seam remains denied until conversion exits");
    check(status == ZR_THREAD_STATUS_FINE && conversion.returned && !conversion.succeeded,
          "aggregate bool API returns false normally after field write failure");
    check(state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR,
          "failed aggregate conversion preserves MEMORY_ERROR status");
    check(conversion.output.type == ZR_VALUE_TYPE_INT64 &&
                  conversion.output.value.nativeObject.nativeInt64 == 707,
          "failed aggregate is not published into the caller output");
    check(state->stackTop.valuePointer == stackTop, "TempValueRoot restores caller stackTop");
    check(state->callInfoList == callInfo, "conversion preserves caller frame chain");
    if (callInfo != NULL && state->callInfoList == callInfo) {
        check(callInfo->functionBase.valuePointer == savedCallInfo.functionBase.valuePointer &&
                      callInfo->functionTop.valuePointer == savedCallInfo.functionTop.valuePointer &&
                      callInfo->returnDestination == savedCallInfo.returnDestination &&
                      callInfo->hasReturnDestination == savedCallInfo.hasReturnDestination,
              "TempValueRoot restores caller frame anchors");
    }
    check(scopes_equal(&before, &after), "conversion preserves execution/native/mutation scopes");
    check(state->aotGcRootFrameStack == aotTop && state->aotGcRootFrameDepth == aotDepth &&
                  state->nestedNativeCalls == nestedCalls && state->exceptionRecoverPoint == NULL,
          "conversion preserves AOT/native/recovery boundaries");
    check(global->garbageCollector->ignoredObjectCount == roots &&
                  ZrCore_GarbageCollector_IsObjectIgnored(global, &key->super),
          "failed conversion leaves no extra roots and preserves caller key root");
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    conversion.returned = 0;
    conversion.succeeded = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, convert, &conversion);
    check(status == ZR_THREAD_STATUS_FINE && conversion.returned && conversion.succeeded,
          "same conversion succeeds after removing the fault");
    if (status == ZR_THREAD_STATUS_FINE && conversion.succeeded &&
        conversion.output.type == ZR_VALUE_TYPE_OBJECT && conversion.output.value.object != NULL) {
        stored = ZrLib_Object_GetFieldCString(state, (SZrObject *)conversion.output.value.object, fieldName);
        check(stored != NULL && stored->value.nativeObject.nativeInt64 == conversion.input,
              "retry publishes the requested native field value");
    } else check(0, "retry publishes a complete aggregate object");
    check(state->stackTop.valuePointer == stackTop && global->garbageCollector->ignoredObjectCount == roots,
          "retry also balances temporary stack roots and leases");
    ZrCore_Gc_RootLeaseRelease(global, &keyLease);
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || strcmp(argv[1], "ffi_field_pair_oom") != 0) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "actual Core initializes before injection");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) field_pair_oom(global);
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "all tracked allocations release with matching size/category");
    return failures == 0 ? 0 : 1;
}
