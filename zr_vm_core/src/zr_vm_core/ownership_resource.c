#include "ownership_resource_internal.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

static void ownership_resource_reset_value(SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    value->type = ZR_VALUE_TYPE_NULL;
    value->value.nativeObject.nativeUInt64 = 0;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

static void ownership_resource_set_direct_value(SZrTypeValue *value,
                                                 SZrRawObject *object,
                                                 EZrOwnershipValueKind ownershipKind) {
    if (value == ZR_NULL || object == ZR_NULL) {
        return;
    }

    value->type = (EZrValueType)object->type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    value->ownershipKind = ownershipKind;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

static void ownership_resource_set_direct_unique(SZrTypeValue *value,
                                                  SZrRawObject *object) {
    ownership_resource_set_direct_value(
            value, object, ZR_OWNERSHIP_VALUE_KIND_UNIQUE);
}

TZrBool ZrCore_OwnershipResource_IsObject(const SZrRawObject *object) {
    const SZrObject *zrObject;

    if (object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    zrObject = (const SZrObject *)object;
    return zrObject->prototype != ZR_NULL &&
           (zrObject->prototype->modifierFlags & ZR_TYPE_MODIFIER_FLAG_RESOURCE) != 0;
}

TZrBool ZrCore_OwnershipResource_IsDirectUniqueValue(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE &&
           value->ownershipControl == ZR_NULL &&
           ZrCore_OwnershipResource_IsObject(value->value.object);
}

TZrBool ZrCore_OwnershipResource_IsDirectLoanedValue(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED &&
           value->ownershipControl == ZR_NULL &&
           ZrCore_OwnershipResource_IsObject(value->value.object);
}

TZrBool ZrCore_OwnershipResource_InitUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrRawObject *object) {
    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsObject(object)) {
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_RegisterOwnershipRoot(state, object)) {
        return ZR_FALSE;
    }

    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipResource_MoveUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    if (object->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_CONSTRUCTING) {
        object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    }
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipResource_LoanUnique(SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_value(
            destination, object, ZR_OWNERSHIP_VALUE_KIND_LOANED);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipResource_ReturnLoan(SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectLoanedValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

void ZrCore_OwnershipResource_CopyUnique(SZrTypeValue *destination,
                                         const SZrTypeValue *source) {
    if (destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return;
    }
    ownership_resource_set_direct_unique(destination, source->value.object);
}

typedef struct SZrResourceDropFailure {
    EZrThreadStatus status;
    SZrTypeValue exception;
    EZrThreadStatus exceptionStatus;
    TZrBool hasException;
    SZrRawObject **root;
} SZrResourceDropFailure;

static void ownership_resource_capture_failure(
        SZrState *state,
        SZrResourceDropFailure *failure,
        EZrThreadStatus status) {
    if (failure->status != ZR_THREAD_STATUS_FINE || status == ZR_THREAD_STATUS_FINE) {
        return;
    }
    failure->status = status;
    failure->exception = state->currentException;
    failure->exceptionStatus = state->currentExceptionStatus;
    failure->hasException = state->hasCurrentException;
    *failure->root = failure->hasException &&
                             ZrCore_Value_IsGarbageCollectable(&failure->exception)
                             ? ZrCore_Value_GetRawObject(&failure->exception)
                             : ZR_NULL;
}

static void ownership_resource_close_callback_values(SZrState *state, TZrPtr argument) {
    TZrMemoryOffset *boundary = (TZrMemoryOffset *)argument;
    ZrCore_Closure_CloseClosure(
            state, ZrCore_Stack_LoadOffsetToPointer(state, *boundary),
            ZR_THREAD_STATUS_INVALID, ZR_FALSE);
}

static void ownership_resource_clear_callback_pending(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    execution_clear_pending_control(state);
}

static EZrThreadStatus ownership_resource_run_callback(
        SZrState *state,
        FZrTryFunction callback,
        TZrPtr argument,
        SZrResourceDropFailure *failure) {
    SZrCallInfo *savedCallInfo = state->callInfoList;
    SZrFunctionStackAnchor stackTop;
    SZrFunctionStackAnchor functionBase;
    SZrFunctionStackAnchor functionTop;
    SZrFunctionStackAnchor returnDestination;
    SZrAotGcRootFrame *savedRootFrame = state->aotGcRootFrameStack;
    TZrUInt32 savedRootDepth = state->aotGcRootFrameDepth;
    TZrUInt32 savedHandlerCount = state->exceptionHandlerStackLength;
    TZrUInt32 savedYieldCount = state->nestedNativeCallYieldFlag;
    TZrMemoryOffset cleanupBoundary;
    TZrBool hasBase = savedCallInfo != ZR_NULL &&
                     savedCallInfo->functionBase.valuePointer != ZR_NULL;
    TZrBool hasTop = savedCallInfo != ZR_NULL &&
                    savedCallInfo->functionTop.valuePointer != ZR_NULL;
    TZrBool hasReturn = savedCallInfo != ZR_NULL &&
                       savedCallInfo->hasReturnDestination &&
                       savedCallInfo->returnDestination != ZR_NULL;
    EZrThreadStatus status;

    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &stackTop);
    cleanupBoundary = ZrCore_Stack_SavePointerAsOffset(
            state, hasTop ? savedCallInfo->functionTop.valuePointer
                          : state->stackTop.valuePointer);
    if (hasBase) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionBase.valuePointer, &functionBase);
    }
    if (hasTop) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionTop.valuePointer, &functionTop);
    }
    if (hasReturn) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->returnDestination, &returnDestination);
    }

    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    status = ZrCore_Exception_TryRun(state, callback, argument);
    if (status == ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus;
    }
    if (status != ZR_THREAD_STATUS_FINE) {
        ownership_resource_capture_failure(state, failure, status);
        state->aotGcRootFrameStack = savedRootFrame;
        state->aotGcRootFrameDepth = savedRootDepth;
        /* Closing pops each registration before invoking its callback. */
        do {
            ZrCore_Exception_ClearCurrent(state);
            state->threadStatus = ZR_THREAD_STATUS_FINE;
            (void)ZrCore_Exception_TryRun(
                    state, ownership_resource_close_callback_values, &cleanupBoundary);
            state->aotGcRootFrameStack = savedRootFrame;
            state->aotGcRootFrameDepth = savedRootDepth;
        } while (state->toBeClosedValueList.valuePointer >=
                 ZrCore_Stack_LoadOffsetToPointer(state, cleanupBoundary));
    }

    while (state->pendingControl.kind != ZR_VM_PENDING_CONTROL_NONE ||
           state->pendingControl.hasValue) {
        EZrThreadStatus clearStatus;
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        clearStatus = ZrCore_Exception_TryRun(
                state, ownership_resource_clear_callback_pending, ZR_NULL);
        if (clearStatus != ZR_THREAD_STATUS_FINE) {
            ownership_resource_capture_failure(state, failure, clearStatus);
            state->aotGcRootFrameStack = savedRootFrame;
            state->aotGcRootFrameDepth = savedRootDepth;
            if (status == ZR_THREAD_STATUS_FINE) {
                status = clearStatus;
            }
        }
    }

    state->callInfoList = savedCallInfo;
    state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &stackTop);
    state->exceptionHandlerStackLength = savedHandlerCount;
    state->nestedNativeCallYieldFlag = savedYieldCount;
    if (hasBase) {
        savedCallInfo->functionBase.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &functionBase);
    }
    if (hasTop) {
        savedCallInfo->functionTop.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &functionTop);
    }
    if (hasReturn) {
        savedCallInfo->returnDestination =
                ZrCore_Function_StackAnchorRestore(state, &returnDestination);
    }
    return status;
}

static void ownership_resource_call_destructor(SZrState *state, TZrPtr argument) {
    SZrTypeValue borrowedSelf;
    ZrCore_Value_InitAsRawObject(state, &borrowedSelf, (SZrRawObject *)argument);
    borrowedSelf.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
    (void)ZrCore_Value_CallMetaMethod(
            state, &borrowedSelf, ZR_META_DESTRUCTOR, ZR_NULL, 0U);
}

static void ownership_resource_drop_fields(SZrState *state, TZrPtr argument) {
    ZrCore_Object_DropManagedFields(state, (SZrObject *)argument);
}

EZrThreadStatus ZrCore_OwnershipResource_DropProtected(
        SZrState *state, SZrRawObject *object) {
    static const SZrAotGcRootSlot stateRoots[] = {
        {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, (TZrUInt32)sizeof(SZrRawObject *), 0u, 0u,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, (TZrUInt32)(2u * sizeof(SZrRawObject *)), 0u, 0u,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
    };
    static const SZrAotGcRootMap stateRootMap = {3u, stateRoots};
    EZrResourceLifecycleState previousState;
    SZrMeta *destructor;
    SZrRawObject *rootedValues[3] = {ZR_NULL, ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame stateRootFrame;
    SZrResourceDropFailure failure;
    SZrTypeValue savedException;
    SZrVmPendingControl savedPending;
    EZrThreadStatus savedExceptionStatus;
    EZrThreadStatus savedThreadStatus;
    TZrBool hadException;
    TZrBool addedPin;

    if (state == ZR_NULL || !ZrCore_OwnershipResource_IsObject(object)) {
        return ZR_THREAD_STATUS_FINE;
    }
    previousState = (EZrResourceLifecycleState)object->resourceLifecycleState;
    if (previousState == ZR_RESOURCE_LIFECYCLE_DROPPING ||
        previousState == ZR_RESOURCE_LIFECYCLE_DROPPED) {
        return ZR_THREAD_STATUS_FINE;
    }

    savedException = state->currentException;
    savedExceptionStatus = state->currentExceptionStatus;
    savedThreadStatus = state->threadStatus;
    hadException = state->hasCurrentException;
    savedPending = state->pendingControl;
    if (hadException && ZrCore_Value_IsGarbageCollectable(&savedException)) {
        rootedValues[0] = ZrCore_Value_GetRawObject(&savedException);
    }
    if (savedPending.hasValue && ZrCore_Value_IsGarbageCollectable(&savedPending.value)) {
        rootedValues[2] = ZrCore_Value_GetRawObject(&savedPending.value);
    }
    if (!ZrCore_Gc_AotRootFramePush(
                state, &stateRootFrame,
                (TZrStackValuePointer)rootedValues, &stateRootMap)) {
        return ZR_THREAD_STATUS_MEMORY_ERROR;
    }
    /* Transfer caller pending storage while callbacks own the thread's active record. */
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
    failure.status = ZR_THREAD_STATUS_FINE;
    failure.root = &rootedValues[1];
    failure.hasException = ZR_FALSE;
    failure.exceptionStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_Value_ResetAsNull(&failure.exception);
    addedPin = (TZrBool)((object->garbageCollectMark.pinFlags &
                         ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u);
    /* The ownership root stays registered throughout callbacks and field cleanup. */
    ZrCore_GarbageCollector_PinObject(
            state, object, ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPING;
    destructor = ZrCore_Object_GetMetaRecursively(
            state->global, (SZrObject *)object, ZR_META_DESTRUCTOR);
    if (previousState == ZR_RESOURCE_LIFECYCLE_ALIVE &&
        destructor != ZR_NULL && destructor->function != ZR_NULL) {
        (void)ownership_resource_run_callback(
                state, ownership_resource_call_destructor, object, &failure);
    }
    /* A failed field release has already cleared its value, so retry makes progress. */
    while (ownership_resource_run_callback(
                   state, ownership_resource_drop_fields, object, &failure) !=
           ZR_THREAD_STATUS_FINE) {
    }
    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPED;
    ZrCore_GcDomain_UnregisterOwnershipRoot(state, object);
    if (addedPin) {
        object->garbageCollectMark.pinFlags &=
                (TZrUInt32)~((TZrUInt32)ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    }
    state->pendingControl = savedPending;
    if (rootedValues[2] != ZR_NULL) {
        state->pendingControl.value.value.object = rootedValues[2];
    }
    if (failure.status != ZR_THREAD_STATUS_FINE) {
        state->currentException = failure.exception;
        if (rootedValues[1] != ZR_NULL) {
            state->currentException.value.object = rootedValues[1];
        }
        state->currentExceptionStatus = failure.exceptionStatus;
        state->hasCurrentException = failure.hasException;
        state->threadStatus = failure.status;
    } else {
        state->currentException = savedException;
        if (rootedValues[0] != ZR_NULL) {
            state->currentException.value.object = rootedValues[0];
        }
        state->currentExceptionStatus = savedExceptionStatus;
        state->hasCurrentException = hadException;
        state->threadStatus = savedThreadStatus;
    }
    (void)ZrCore_Gc_AotRootFramePop(state, &stateRootFrame);
    return failure.status;
}

void ZrCore_OwnershipResource_Drop(SZrState *state, SZrRawObject *object) {
    EZrThreadStatus status = ZrCore_OwnershipResource_DropProtected(state, object);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
    }
}
