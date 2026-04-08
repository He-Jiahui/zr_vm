//
// Extracted object-call, stack-anchor, and pinning helpers.
//

#include "object_call_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include <string.h>

static TZrStackValuePointer object_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                             const SZrCallInfo *callInfo) {
    TZrStackValuePointer base = stackTop;

    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer > base) {
        base = callInfo->functionTop.valuePointer;
    }

    return base;
}

static TZrBool object_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller) {
    SZrRawObject *object;

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return ZR_TRUE;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, object)) {
        return ZR_TRUE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObject(state, object)) {
        return ZR_FALSE;
    }

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_TRUE;
    }
    return ZR_TRUE;
}

static void object_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller) {
    SZrRawObject *object;

    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

static TZrBool object_value_is_struct_instance(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT;
}

static TZrBool object_sync_struct_receiver_value(SZrState *state,
                                                 SZrTypeValue *receiverTarget,
                                                 const SZrTypeValue *stackReceiver) {
    SZrObject *targetObject;
    SZrObject *sourceObject;

    if (state == ZR_NULL || receiverTarget == ZR_NULL || stackReceiver == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_value_is_struct_instance(state, receiverTarget) ||
        !object_value_is_struct_instance(state, stackReceiver)) {
        ZrCore_Value_Copy(state, receiverTarget, stackReceiver);
        return state->threadStatus == ZR_THREAD_STATUS_FINE;
    }

    targetObject = ZR_CAST_OBJECT(state, receiverTarget->value.object);
    sourceObject = ZR_CAST_OBJECT(state, stackReceiver->value.object);
    if (targetObject == ZR_NULL || sourceObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetObject == sourceObject) {
        return ZR_TRUE;
    }

    targetObject->prototype = sourceObject->prototype;
    targetObject->internalType = sourceObject->internalType;
    if (object_node_map_is_ready(sourceObject)) {
        for (TZrSize bucketIndex = 0; bucketIndex < sourceObject->nodeMap.capacity; bucketIndex++) {
            for (SZrHashKeyValuePair *pair = sourceObject->nodeMap.buckets[bucketIndex]; pair != ZR_NULL; pair = pair->next) {
                ZrCore_Object_SetValue(state, targetObject, &pair->key, &pair->value);
                if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                    return ZR_FALSE;
                }
            }
        }
    }

    receiverTarget->type = stackReceiver->type;
    receiverTarget->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(targetObject);
    receiverTarget->isGarbageCollectable = stackReceiver->isGarbageCollectable;
    receiverTarget->isNative = stackReceiver->isNative;
    return ZR_TRUE;
}

static TZrBool object_make_callable_value(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    return ZR_TRUE;
}

TZrBool ZrCore_Object_CallValue(SZrState *state,
                                const SZrTypeValue *callable,
                                const SZrTypeValue *receiver,
                                const SZrTypeValue *arguments,
                                TZrSize argumentCount,
                                SZrTypeValue *result) {
    SZrTypeValue stableCallable;
    SZrTypeValue stableReceiver;
    SZrTypeValue inlineArguments[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    SZrTypeValue *stableArguments = ZR_NULL;
    TZrBool inlineArgumentPinAdded[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    TZrBool *argumentPinAdded = ZR_NULL;
    TZrBool freeStableArguments = ZR_FALSE;
    TZrBool freeArgumentPinAdded = ZR_FALSE;
    TZrBool callablePinAdded = ZR_FALSE;
    TZrBool receiverPinAdded = ZR_FALSE;
    TZrSize stableArgumentsBytes = 0;
    TZrSize argumentPinAddedBytes = 0;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize totalArguments;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    TZrStackValuePointer resultStackSlot;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    SZrFunctionStackAnchor receiverAnchor;
    SZrFunctionStackAnchor resultAnchor;
    TZrBool syncStructReceiver = ZR_FALSE;
    TZrBool hasAnchoredReturnDestination = ZR_FALSE;
    TZrBool hasReceiverAnchor = ZR_FALSE;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableCallable = *callable;
    memset(inlineArgumentPinAdded, 0, sizeof(inlineArgumentPinAdded));
    if (receiver != ZR_NULL) {
        stableReceiver = *receiver;
    }
    if (argumentCount > 0) {
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }

        if (argumentCount <= ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY) {
            stableArguments = inlineArguments;
            argumentPinAdded = inlineArgumentPinAdded;
        } else {
            stableArgumentsBytes = argumentCount * sizeof(SZrTypeValue);
            stableArguments = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                              stableArgumentsBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (stableArguments == ZR_NULL) {
                return ZR_FALSE;
            }
            freeStableArguments = ZR_TRUE;

            argumentPinAddedBytes = argumentCount * sizeof(TZrBool);
            argumentPinAdded = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                          argumentPinAddedBytes,
                                                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (argumentPinAdded == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
                return ZR_FALSE;
            }
            freeArgumentPinAdded = ZR_TRUE;
        }

        memset(argumentPinAdded, 0, argumentCount * sizeof(TZrBool));
        for (index = 0; index < argumentCount; index++) {
            stableArguments[index] = arguments[index];
        }
    }

    if (!object_pin_value_object(state, &stableCallable, &callablePinAdded)) {
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    if (receiver != ZR_NULL && !object_pin_value_object(state, &stableReceiver, &receiverPinAdded)) {
        object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    for (index = 0; index < argumentCount; index++) {
        if (!object_pin_value_object(state, &stableArguments[index], &argumentPinAdded[index])) {
            while (index > 0) {
                index--;
                object_unpin_value_object(state->global, &stableArguments[index], argumentPinAdded[index]);
            }
            object_unpin_value_object(state->global, receiver != ZR_NULL ? &stableReceiver : ZR_NULL, receiverPinAdded);
            object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeStableArguments) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return ZR_FALSE;
        }
    }

    ZrCore_Value_ResetAsNull(result);
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    totalArguments = argumentCount + (receiver != ZR_NULL ? 1 : 0);
    scratchSlots = 1 + totalArguments;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    resultStackSlot = ZR_CAST(TZrStackValuePointer, result);
    syncStructReceiver = object_value_is_struct_instance(state, receiver);

    if (resultStackSlot >= state->stackBase.valuePointer && resultStackSlot < state->stackTail.valuePointer) {
        ZrCore_Function_StackAnchorInit(state, resultStackSlot, &resultAnchor);
        hasResultAnchor = ZR_TRUE;
    }

    if (syncStructReceiver) {
        TZrStackValuePointer receiverStackSlot = ZR_CAST(TZrStackValuePointer, receiver);
        if (receiverStackSlot >= state->stackBase.valuePointer && receiverStackSlot < state->stackTail.valuePointer) {
            ZrCore_Function_StackAnchorInit(state, receiverStackSlot, &receiverAnchor);
            hasReceiverAnchor = ZR_TRUE;
        }
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        hasAnchoredReturnDestination =
            (TZrBool)(savedCallInfo->hasReturnDestination && savedCallInfo->returnDestination != ZR_NULL);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }

    ZrCore_Function_ReserveScratchSlots(state, scratchSlots, base);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
        base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    }

    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }
    if (receiver != ZR_NULL) {
        ZrCore_Stack_CopyValue(state, base + 1, &stableReceiver);
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
        if (savedCallInfo != ZR_NULL) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
            if (hasAnchoredReturnDestination) {
                savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
            }
        }
    }
    for (index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state,
                               base + 1 + (receiver != ZR_NULL ? 1 : 0) + index,
                               &stableArguments[index]);
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
        if (savedCallInfo != ZR_NULL) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
            if (hasAnchoredReturnDestination) {
                savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
            }
        }
    }
    ZrCore_Stack_CopyValue(state, base, &stableCallable);

    for (index = argumentCount; index > 0; index--) {
        object_unpin_value_object(state->global,
                                  &stableArguments[index - 1],
                                  argumentPinAdded[index - 1]);
    }
    object_unpin_value_object(state->global,
                              receiver != ZR_NULL ? &stableReceiver : ZR_NULL,
                              receiverPinAdded);
    object_unpin_value_object(state->global, &stableCallable, callablePinAdded);

    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        if (syncStructReceiver) {
            SZrTypeValue *stackReceiver = ZrCore_Stack_GetValue(base + 1);
            SZrTypeValue *receiverTarget = ZR_NULL;

            if (hasReceiverAnchor) {
                receiverTarget = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &receiverAnchor));
            } else if (receiver != ZR_NULL) {
                receiverTarget = ZR_CAST(SZrTypeValue *, receiver);
            }

            if (receiverTarget != ZR_NULL &&
                stackReceiver != ZR_NULL &&
                !object_sync_struct_receiver_value(state, receiverTarget, stackReceiver) &&
                state->threadStatus == ZR_THREAD_STATUS_FINE) {
                state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
            }
        }

        {
            SZrTypeValue *stackResult = ZrCore_Stack_GetValue(base);
            ZrCore_Value_Copy(state, result, stackResult);
        }
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_TRUE;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    if (freeStableArguments) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      stableArguments,
                                      stableArgumentsBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    if (freeArgumentPinAdded) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      argumentPinAdded,
                                      argumentPinAddedBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    return ZR_FALSE;
}

TZrBool ZrCore_Object_CallFunctionWithReceiver(SZrState *state,
                                               SZrFunction *function,
                                               SZrTypeValue *receiver,
                                               const SZrTypeValue *arguments,
                                               TZrSize argumentCount,
                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;

    if (!object_make_callable_value(state, function, &callableValue)) {
        return ZR_FALSE;
    }

    return ZrCore_Object_CallValue(state, &callableValue, receiver, arguments, argumentCount, result);
}
