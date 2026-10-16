/* Exercise actual registered instance dispatch; never release the lost C local. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"

#define ARG_COUNT (ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY + 1u)
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)
typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int failures, allocatorError, armed, mode, callbackCount;
static unsigned valueAttempts, flagAttempts, rejected, releasedSlots;
static TZrUInt32 strongAtRejection;
static SZrState *activeState;
static SZrOwnershipControl *receiverControl;
static SZrFunctionStackAnchor receiverSlot, stableSlot;
static int hasReceiverSlot, hasStableSlot;

static void remember_receiver(SZrState *state) {
    ZrCore_Function_StackAnchorInit(state,
            state->callInfoList->functionBase.valuePointer + 1, &receiverSlot);
    hasReceiverSlot = 1;
}

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    int deny = 0;
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
    if (armed && old == NULL && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT) {
        if (newSize == ARG_COUNT * sizeof(SZrTypeValue)) {
            ++valueAttempts;
            deny = mode == 1 && valueAttempts == 2;
        }
        if (newSize == ARG_COUNT * sizeof(TZrBool)) {
            ++flagAttempts;
            deny = mode == 2 && flagAttempts == 1;
        }
        if (deny) {
            ++rejected;
            remember_receiver(activeState);
            strongAtRejection = receiverControl->strongRefCount;
            fprintf(stderr, "reject bytes=%zu values=%u flags=%u strong=%u\n",
                    (size_t)newSize, valueAttempts, flagAttempts, (unsigned)strongAtRejection);
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
    return next;
}

static TZrBool probe(ZrLibCallContext *context, SZrTypeValue *result) {
    ++callbackCount;
    CHECK(context->argumentCount == ARG_COUNT);
    CHECK(context->selfValue != NULL && context->selfValue->ownershipControl == receiverControl);
    remember_receiver(context->state);
    /* Generic lane puts its separately owned stack self just before arguments. */
    ZrCore_Function_StackAnchorInit(context->state, context->argumentBase - 1, &stableSlot);
    hasStableSlot = 1;
    for (TZrSize i = 0; i < ARG_COUNT; ++i) {
        SZrTypeValue *argument = ZrLib_CallContext_Argument(context, i);
        CHECK(argument != NULL && ZR_VALUE_IS_TYPE_SIGNED_INT(argument->type));
        if (argument != NULL) CHECK(argument->value.nativeObject.nativeInt64 == (TZrInt64)i);
    }
    ZrLib_Value_SetInt(context->state, result, 73);
    return ZR_TRUE;
}
static const ZrLibMethodDescriptor methods[] = {
    {.name = "probe", .minArgumentCount = ARG_COUNT, .maxArgumentCount = ARG_COUNT,
     .callback = probe, .returnTypeName = "int", .dispatchFlags = 0},
};
static const ZrLibTypeDescriptor types[] = {
    {.name = "DispatchSelfOom", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
     .methods = methods, .methodCount = 1},
};
static const ZrLibModuleDescriptor descriptor = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "dispatch_self_oom", .types = types, .typeCount = 1,
    .moduleVersion = "1.0.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

static void release_observed_slot(SZrState *state, SZrFunctionStackAnchor *anchor) {
    SZrTypeValue *slot = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, anchor));
    /* This is an actual VM stack owner, not a fabricated copy of the lost local.
     * If Core already consumed it, do nothing; never compensate by count. */
    if (slot->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED &&
        slot->ownershipControl == receiverControl) {
        ++releasedSlots;
        ZrCore_Ownership_ReleaseValue(state, slot);
    }
}

static void run_case(SZrGlobalState *global) {
    SZrState *state = global->mainThreadState;
    SZrObjectPrototype *prototype;
    SZrObject *instance;
    SZrString *keyString;
    const SZrTypeValue *callable;
    SZrTypeValue key = {0}, plain = {0}, owner = {0}, weak = {0}, awakened = {0}, result = {0};
    SZrTypeValue args[ARG_COUNT];
    SZrGcRootLease lease = {0};
    SZrRawObject *raw;
    TZrUInt32 baseline;
    TZrBool called;
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    CHECK(ZrLibrary_NativeRegistry_Attach(global));
    CHECK(ZrLibrary_NativeRegistry_RegisterModule(global, &descriptor));
    CHECK(ZrCore_Module_ImportByPath(state,
            ZrCore_String_CreateFromNative(state, (TZrNativeString)descriptor.moduleName)) != NULL);
    prototype = ZrLib_Type_FindPrototype(state, "DispatchSelfOom");
    CHECK(prototype != NULL);
    if (prototype == NULL) return;
    instance = ZrLib_Type_NewInstanceWithPrototype(state, prototype);
    keyString = ZrCore_String_CreateFromNative(state, "probe");
    CHECK(instance != NULL && keyString != NULL);
    if (instance == NULL || keyString == NULL) return;
    raw = ZR_CAST_RAW_OBJECT_AS_SUPER(instance);
    CHECK(ZrCore_Gc_RootLeaseAcquire(state, raw, &lease));
    if (lease.object == NULL) return;
    ZrLib_Value_SetStringObject(state, &key, keyString);
    callable = ZrCore_Object_GetValue(state, instance, &key);
    CHECK(callable != NULL);
    if (callable == NULL) goto cleanup;
    ZrLib_Value_SetObject(state, &plain, instance, ZR_VALUE_TYPE_OBJECT);
    CHECK(ZrCore_Ownership_SharePlainValue(state, &owner, &plain));
    CHECK(ZrCore_Ownership_DegradeValue(state, &weak, &owner));
    receiverControl = owner.ownershipControl;
    CHECK(receiverControl != NULL);
    if (receiverControl == NULL) goto cleanup;
    baseline = ZrCore_Ownership_GetStrongRefCount(raw);
    CHECK(baseline == 1);
    /* No additional owners or GC relocation after arming the exact raw seam. */
    global->garbageCollector->stopGcFlag = ZR_TRUE;
    CHECK(ZrCore_Stack_Grow(state, 512, ZR_FALSE));
    for (TZrSize i = 0; i < ARG_COUNT; ++i) ZrLib_Value_SetInt(state, &args[i], (TZrInt64)i);
    activeState = state;
    armed = 1;
    called = ZrLib_CallValue(state, callable, &owner, args, ARG_COUNT, &result);
    armed = 0;
    CHECK(valueAttempts == 2);
    CHECK(flagAttempts == (mode == 1 ? 0u : 1u));
    CHECK(rejected == (mode == 0 ? 0u : 1u));
    CHECK(callbackCount == (mode == 0 ? 1 : 0));
    CHECK(hasStableSlot == (mode == 0));
    if (mode == 0) {
        CHECK(called && ZR_VALUE_IS_TYPE_SIGNED_INT(result.type));
        CHECK(result.value.nativeObject.nativeInt64 == 73);
    } else {
        /* Existing raw OOM contract can return true/null: test ownership only. */
        CHECK(strongAtRejection == baseline + 2);
    }
    CHECK(hasReceiverSlot);
    if (hasReceiverSlot) release_observed_slot(state, &receiverSlot);
    if (hasStableSlot) release_observed_slot(state, &stableSlot);
    fprintf(stderr, "released real scratch slots=%u remaining strong=%u baseline=%u\n",
            releasedSlots, (unsigned)ZrCore_Ownership_GetStrongRefCount(raw), (unsigned)baseline);
    CHECK(ZrCore_Ownership_GetStrongRefCount(raw) == baseline);
    ZrCore_Ownership_ReleaseValue(state, &owner);
    CHECK(ZrCore_Ownership_GetStrongRefCount(raw) == 0);
    CHECK(ZrCore_Ownership_WakeValue(state, &awakened, &weak));
    CHECK(awakened.type == ZR_VALUE_TYPE_NULL);
    /* On RED, Wake creates a legitimate extra owner; release only that owner.
     * The original leaked C reference is never reconstructed or decremented. */
    ZrCore_Ownership_ReleaseValue(state, &awakened);
cleanup:
    ZrCore_Ownership_ReleaseValue(state, &owner);
    ZrCore_Ownership_ReleaseValue(state, &weak);
    ZrCore_Ownership_ReleaseValue(state, &result);
    ZrCore_Gc_RootLeaseRelease(global, &lease);
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
    if (strcmp(argv[1], "stable_arguments_oom") == 0) mode = 1;
    else if (strcmp(argv[1], "pin_flags_oom") == 0) mode = 2;
    else if (strcmp(argv[1], "oversized_control") != 0) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global);
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    CHECK(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
