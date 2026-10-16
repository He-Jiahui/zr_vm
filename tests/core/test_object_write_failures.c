/* Existing API regression: a failed real object write must not report success.
 * Allocation failures run in separate processes against Core/Library archives. */
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
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "reflection/reflection_object_internal.h"
#include "gc/gc_domain_internal.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;
static Allocation allocations[32768];
static size_t liveCount, rejected;
static int allocatorError, failures, armed;
static TZrInt64 failFlag;
static TZrSize failSize;
static void *failOld;

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
    /* Repeated GC retries and exception materialization cannot consume a
     * one-shot failure and make the original operation spuriously succeed. */
    if (armed && flag == failFlag && newSize == failSize && old == failOld) {
        if (++rejected <= 4) {
            fprintf(stderr, "reject flag=%lld size=%zu request=%zu\n",
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

typedef enum Entry {
    ENTRY_CORE,
    ENTRY_MEMBER,
    ENTRY_INDEX,
    ENTRY_LIBRARY,
    ENTRY_REFLECTION
} Entry;
typedef struct Write {
    SZrObject *object;
    SZrString *name;
    SZrTypeValue receiver;
    SZrTypeValue key;
    SZrTypeValue value;
    Entry entry;
    TZrBool result;
    int returned;
} Write;

static const char fieldName[] = "object-write-failure-probe";

static void invoke_write(SZrState *state, TZrPtr data) {
    Write *write = (Write *)data;
    switch (write->entry) {
        case ENTRY_CORE:
            ZrCore_Object_SetValue(state, write->object, &write->key, &write->value);
            break;
        case ENTRY_MEMBER:
            write->result = ZrCore_Object_SetMember(state, &write->receiver, write->name, &write->value);
            break;
        case ENTRY_INDEX:
            write->result = ZrCore_Object_SetByIndex(state, &write->receiver, &write->key, &write->value);
            break;
        case ENTRY_LIBRARY:
            ZrLib_Object_SetFieldCString(state, write->object, fieldName, &write->value);
            break;
        case ENTRY_REFLECTION:
            write->result = ZrCore_Reflection_ObjectSetFieldValue(
                    state, write->object, fieldName, &write->value);
            break;
    }
    write->returned = 1;
}

static int fill_to_threshold(SZrState *state, SZrObject *object) {
    TZrInt64 index = 0;
    while (object->nodeMap.elementCount < object->nodeMap.resizeThreshold && index < 100000) {
        SZrTypeValue key, value;
        ZrCore_Value_InitAsInt(state, &key, index++);
        ZrCore_Value_InitAsInt(state, &value, 17);
        ZrCore_Object_SetValue(state, object, &key, &value);
        if (ZrCore_Object_GetValue(state, object, &key) == NULL) return 0;
    }
    return object->nodeMap.elementCount == object->nodeMap.resizeThreshold;
}

static int is_bool_entry(Entry entry) {
    return entry == ENTRY_MEMBER || entry == ENTRY_INDEX || entry == ENTRY_REFLECTION;
}

static void run_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    Write write = {0};
    EZrThreadStatus status;
    TZrBool callerRootAdded = ZR_FALSE;
    TZrSize baselineRoots, oldCount, oldCapacity;
    SZrHashKeyValuePair **oldBuckets;
    SZrGcDomainScopeSnapshot scopesBefore, scopesAfter;
    SZrAotGcRootFrame *aotBefore;
    TZrUInt32 aotDepthBefore, nativeCallsBefore;
    int initFailure = strcmp(name, "init_core") == 0;
    int pairFailure = strncmp(name, "pair_", 5) == 0;

    write.object = ZrCore_Object_New(state, NULL);
    check(write.object != NULL, "actual target object is constructed");
    if (write.object == NULL) return;
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                global, state, &write.object->super, &callerRootAdded)) {
        check(0, "caller establishes its explicit target root");
        return;
    }
    if (!initFailure) ZrCore_Object_Init(state, write.object);
    write.name = ZrCore_String_Create(state, (TZrNativeString)fieldName, sizeof(fieldName) - 1U);
    check(write.name != NULL, "field name is interned before injection");
    if (write.name == NULL) goto cleanup;
    ZrCore_Value_InitAsRawObject(state, &write.receiver, &write.object->super);
    ZrCore_Value_InitAsRawObject(state, &write.key, &write.name->super);
    write.key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &write.value, 123456);
    if (strstr(name, "member") != NULL) write.entry = ENTRY_MEMBER;
    else if (strstr(name, "index") != NULL) write.entry = ENTRY_INDEX;
    else if (strstr(name, "library") != NULL) write.entry = ENTRY_LIBRARY;
    else if (strstr(name, "reflection") != NULL) write.entry = ENTRY_REFLECTION;
    else write.entry = ENTRY_CORE;

    if (!initFailure && !pairFailure) {
        int ready = fill_to_threshold(state, write.object);
        check(ready, "actual object storage reaches its rehash threshold");
        if (!ready) goto cleanup;
    }
    oldCount = write.object->nodeMap.elementCount;
    oldCapacity = write.object->nodeMap.capacity;
    oldBuckets = write.object->nodeMap.buckets;
    baselineRoots = global->garbageCollector->ignoredObjectCount;
    check(!ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.name->super),
          "new key initially has no explicit ignored root");
    if (pairFailure) {
        failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_PAIR;
        failSize = sizeof(SZrHashKeyValuePair);
        failOld = NULL;
    } else {
        failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET;
        failSize = initFailure
                       ? ((TZrSize)1 << ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2) * sizeof(TZrPtr)
                       : oldCapacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR * sizeof(TZrPtr);
        failOld = oldBuckets;
    }
    armed = 1;
    ZrCore_GcDomain_CaptureScopes(state, &scopesBefore);
    aotBefore = state->aotGcRootFrameStack;
    aotDepthBefore = state->aotGcRootFrameDepth;
    nativeCallsBefore = state->nestedNativeCalls;
    status = ZrCore_Exception_TryRun(state, invoke_write, &write);
    ZrCore_GcDomain_CaptureScopes(state, &scopesAfter);
    check(scopesAfter.domain == scopesBefore.domain &&
                  scopesAfter.identity.id == scopesBefore.identity.id &&
                  scopesAfter.identity.generation == scopesBefore.identity.generation &&
                  scopesAfter.mutatorId == scopesBefore.mutatorId &&
                  scopesAfter.executionDepth == scopesBefore.executionDepth &&
                  scopesAfter.nativeDepth == scopesBefore.nativeDepth &&
                  scopesAfter.mutationDepth == scopesBefore.mutationDepth &&
                  scopesAfter.nativeMode == scopesBefore.nativeMode &&
                  scopesAfter.nativeEnteredFromInactive == scopesBefore.nativeEnteredFromInactive,
          "failed write preserves caller execution/native/mutation scope depths");
    check(state->aotGcRootFrameStack == aotBefore &&
                  state->aotGcRootFrameDepth == aotDepthBefore &&
                  state->nestedNativeCalls == nativeCallsBefore,
          "failed write preserves AOT roots and nested native-call baseline");
    check(armed && rejected > 0, "target allocation fails until the protected write returns");
    if (pairFailure) {
        check(status == ZR_THREAD_STATUS_MEMORY_ERROR ||
                      (is_bool_entry(write.entry) && status == ZR_THREAD_STATUS_FINE && !write.result),
              "persistent pair OOM is observable as failure");
    } else if (is_bool_entry(write.entry)) {
        check(status == ZR_THREAD_STATUS_FINE && write.returned && !write.result,
              "bool setter returns false for storage allocation failure");
    } else {
        check(status == ZR_THREAD_STATUS_MEMORY_ERROR && !write.returned,
              "void setter propagates storage failure through TryRun");
    }
    check(write.object->nodeMap.elementCount == oldCount,
          "failed write does not add an entry");
    if (!initFailure && !pairFailure) {
        SZrTypeValue oldKey;
        const SZrTypeValue *oldValue;
        ZrCore_Value_InitAsInt(state, &oldKey, 0);
        oldValue = ZrCore_Object_GetValue(state, write.object, &oldKey);
        check(oldValue != NULL && oldValue->value.nativeObject.nativeInt64 == 17,
              "existing entry remains readable after failed growth");
    }
    if (!pairFailure) {
        check(write.object->nodeMap.buckets == oldBuckets &&
                      write.object->nodeMap.capacity == oldCapacity,
              "failed bucket allocation preserves existing storage");
    }
    check(global->garbageCollector->ignoredObjectCount == baselineRoots,
          "failed write removes all roots it temporarily added");
    check(ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.object->super),
          "failed write preserves the caller's original explicit root");
    check(!ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.name->super),
          "failed write leaves no temporary key root");
    check(state->exceptionRecoverPoint == NULL, "TryRun restores the recovery chain");
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    write.returned = 0;
    write.result = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, invoke_write, &write);
    check(status == ZR_THREAD_STATUS_FINE && write.returned &&
                  (!is_bool_entry(write.entry) || write.result),
          "the same setter succeeds after removing the fault");
    if (status == ZR_THREAD_STATUS_FINE) {
        const SZrTypeValue *stored = ZrCore_Object_GetValue(state, write.object, &write.key);
        check(stored != NULL && stored->type == write.value.type &&
                      stored->value.nativeObject.nativeInt64 == 123456,
              "retry stores the requested value");
    }
    check(ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.object->super),
          "successful retry also preserves caller ownership of its root");
    check(global->garbageCollector->ignoredObjectCount == baselineRoots,
          "retry leaves no extra explicit roots");
cleanup:
    armed = 0;
    if (callerRootAdded) ZrCore_GarbageCollector_UnignoreObject(global, &write.object->super);
}

typedef struct StructWrite {
    SZrObject *source;
    SZrObject *target;
    SZrObject *clone;
    SZrTypeValue key;
    SZrTypeValue value;
    int directClone;
    int returned;
} StructWrite;

static void invoke_struct_write(SZrState *state, TZrPtr data) {
    StructWrite *write = (StructWrite *)data;
    if (write->directClone) write->clone = ZrCore_Object_CloneStruct(state, write->source);
    else ZrCore_Object_SetValue(state, write->target, &write->key, &write->value);
    write->returned = 1;
}

static void run_struct_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    StructWrite write = {0};
    SZrString *prototypeName = ZrCore_String_CreateFromNative(state, "WriteOomStruct");
    SZrStructPrototype *prototype;
    SZrTypeValue fieldKey, fieldValue;
    SZrGcRootLease sourceRoot = {0}, targetRoot = {0};
    SZrGcDomainScopeSnapshot before, after;
    SZrAotGcRootFrame *aotBefore;
    TZrUInt32 aotDepthBefore, nativeCallsBefore;
    EZrThreadStatus status;
    TZrSize roots, count;
    const SZrTypeValue *stored;
    check(prototypeName != NULL, "struct prototype name is created before injection");
    if (prototypeName == NULL) return;
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    check(prototype != NULL, "actual struct prototype is created");
    if (prototype == NULL) return;
    write.source = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    check(write.source != NULL, "actual plain struct source is created");
    if (write.source == NULL) return;
    write.source->prototype = &prototype->super;
    if (!ZrCore_Gc_RootLeaseAcquire(state, &write.source->super, &sourceRoot)) {
        check(0, "caller roots the source struct");
        return;
    }
    ZrCore_Object_Init(state, write.source);
    ZrCore_Value_InitAsInt(state, &fieldKey, 7);
    ZrCore_Value_InitAsInt(state, &fieldValue, 42);
    ZrCore_Object_SetValue(state, write.source, &fieldKey, &fieldValue);
    write.target = ZrCore_Object_New(state, NULL);
    check(write.target != NULL, "copy destination is created");
    if (write.target == NULL) goto cleanup;
    if (!ZrCore_Gc_RootLeaseAcquire(state, &write.target->super, &targetRoot)) {
        check(0, "caller roots the destination");
        goto cleanup;
    }
    ZrCore_Object_Init(state, write.target);
    ZrCore_Value_InitAsInt(state, &write.key, 99);
    ZrCore_Value_InitAsRawObject(state, &write.value, &write.source->super);
    write.value.type = ZR_VALUE_TYPE_OBJECT;
    write.directClone = strcmp(name, "struct_clone_pair") == 0;
    roots = global->garbageCollector->ignoredObjectCount;
    count = write.target->nodeMap.elementCount;
    failFlag = write.directClone ? ZR_MEMORY_NATIVE_TYPE_HASH_PAIR : ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET;
    failSize = write.directClone ? sizeof(SZrHashKeyValuePair)
            : ((TZrSize)1 << ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2) * sizeof(TZrPtr);
    failOld = NULL;
    armed = 1;
    ZrCore_GcDomain_CaptureScopes(state, &before);
    aotBefore = state->aotGcRootFrameStack;
    aotDepthBefore = state->aotGcRootFrameDepth;
    nativeCallsBefore = state->nestedNativeCalls;
    status = ZrCore_Exception_TryRun(state, invoke_struct_write, &write);
    ZrCore_GcDomain_CaptureScopes(state, &after);
    check(rejected > 0, "persistent fault reaches the struct copy allocation");
    if (write.directClone) {
        check(status == ZR_THREAD_STATUS_FINE && write.returned && write.clone == NULL,
              "CloneStruct returns NULL after cleaning up a failed field copy");
    } else {
        check(status == ZR_THREAD_STATUS_MEMORY_ERROR && !write.returned,
              "void setter exposes struct copy failure");
    }
    check(write.target->nodeMap.elementCount == count &&
                  ZrCore_Object_GetValue(state, write.target, &write.key) == NULL,
          "failed struct copy never publishes a destination field");
    check(global->garbageCollector->ignoredObjectCount == roots,
          "failed struct copy releases its clone and transaction roots");
    check(ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.source->super) &&
                  ZrCore_GarbageCollector_IsObjectIgnoredFast(global, &write.target->super),
          "failed copy preserves both caller roots");
    check(after.domain == before.domain && after.identity.id == before.identity.id &&
                  after.identity.generation == before.identity.generation && after.mutatorId == before.mutatorId &&
                  after.executionDepth == before.executionDepth && after.nativeDepth == before.nativeDepth &&
                  after.mutationDepth == before.mutationDepth && after.nativeMode == before.nativeMode &&
                  after.nativeEnteredFromInactive == before.nativeEnteredFromInactive,
          "failed copy restores caller execution/native/mutation depths");
    check(state->aotGcRootFrameStack == aotBefore && state->aotGcRootFrameDepth == aotDepthBefore &&
                  state->nestedNativeCalls == nativeCallsBefore,
          "failed copy restores caller AOT roots and nested-native baseline");
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    write.returned = 0;
    status = ZrCore_Exception_TryRun(state, invoke_struct_write, &write);
    check(status == ZR_THREAD_STATUS_FINE && write.returned,
          "the same struct copy completes after fault removal");
    if (!write.directClone) {
        stored = ZrCore_Object_GetValue(state, write.target, &write.key);
        write.clone = stored == NULL ? NULL : (SZrObject *)ZrCore_Value_GetRawObject(stored);
    }
    check(write.clone != NULL && write.clone != write.source &&
                  write.clone->prototype == write.source->prototype,
          "retry retains ordinary struct independent-copy semantics");
    if (write.clone != NULL) {
        stored = ZrCore_Object_GetValue(state, write.clone, &fieldKey);
        check(stored != NULL && stored->value.nativeObject.nativeInt64 == 42,
              "successful clone retains source field contents");
    }
    if (!write.directClone) {
        check(ZrCore_Object_TrySetValue(state, write.target, &write.key, &write.value),
              "existing field overwrite still completes");
        stored = ZrCore_Object_GetValue(state, write.target, &write.key);
        check(stored != NULL && ZrCore_Value_GetRawObject(stored) == &write.source->super,
              "existing plain field keeps its prior shallow overwrite semantics");
    }
    check(global->garbageCollector->ignoredObjectCount == roots,
          "successful struct copy leaves no temporary roots");
cleanup:
    armed = 0;
    ZrCore_Gc_RootLeaseRelease(global, &targetRoot);
    ZrCore_Gc_RootLeaseRelease(global, &sourceRoot);
}

int main(int argc, char **argv) {
    static const char *cases[] = {"init_core", "rehash_member", "rehash_index",
            "rehash_library", "rehash_reflection", "pair_core", "pair_library", "pair_reflection",
            "struct_clone_pair", "struct_write_bucket"};
    size_t index;
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (strcmp(argv[1], cases[index]) == 0) break;
    }
    if (index == sizeof(cases) / sizeof(cases[0])) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "Core initializes before injection");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            if (strncmp(argv[1], "struct_", 7) == 0) run_struct_case(global, argv[1]);
            else run_case(global, argv[1]);
        }
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError,
          "teardown releases all tracked blocks with matching size and category");
    return failures == 0 ? 0 : 1;
}
