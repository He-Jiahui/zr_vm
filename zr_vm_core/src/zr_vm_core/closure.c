//
// Created by HeJiahui on 2025/7/15.
//
#include "zr_vm_core/closure.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/state.h"
#define MAX_DELTA ((256UL << ((sizeof(state->stackBase.valuePointer->toBeClosedValueOffset) - 1) * 8)) - 1)
#define ZR_CLOSURE_CLOSED_COUNT_NONE ((TZrSize)0)

static TZrUInt32 closure_merge_scope_depth(TZrUInt32 currentScopeDepth, TZrUInt32 incomingScopeDepth) {
    if (currentScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return incomingScopeDepth;
    }
    if (incomingScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return currentScopeDepth;
    }
    return currentScopeDepth < incomingScopeDepth ? currentScopeDepth : incomingScopeDepth;
}

static ZR_FORCE_INLINE SZrRawObject *closure_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

static ZR_FORCE_INLINE SZrFunction *closure_refresh_forwarded_function(SZrFunction *function) {
    return function != ZR_NULL ? (SZrFunction *)closure_refresh_forwarded_raw_object(
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(function))
                               : ZR_NULL;
}

static ZR_FORCE_INLINE SZrClosure *closure_refresh_forwarded_closure(SZrClosure *closure) {
    return closure != ZR_NULL ? (SZrClosure *)closure_refresh_forwarded_raw_object(
                                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure))
                              : ZR_NULL;
}

static ZR_FORCE_INLINE SZrClosureValue *closure_refresh_forwarded_closure_value(SZrClosureValue *closureValue) {
    return closureValue != ZR_NULL ? (SZrClosureValue *)closure_refresh_forwarded_raw_object(
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue))
                                   : ZR_NULL;
}

static ZR_FORCE_INLINE SZrClosureValue **closure_refresh_parent_closure_values_from_base(SZrState *state,
                                                                                          TZrStackValuePointer base) {
    SZrTypeValue *ownerValue;
    SZrClosure *ownerClosure;

    if (state == ZR_NULL || base == ZR_NULL || base <= state->stackBase.valuePointer) {
        return ZR_NULL;
    }

    ownerValue = ZrCore_Stack_GetValue(base - 1);
    if (ownerValue == ZR_NULL ||
        ownerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        ownerValue->isNative ||
        ownerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    ownerClosure = closure_refresh_forwarded_closure(ZR_CAST_VM_CLOSURE(state, ownerValue->value.object));
    return ownerClosure != ZR_NULL ? ownerClosure->closureValuesExtend : ZR_NULL;
}

static void closure_value_apply_anchored_escape_to_closed_value(SZrState *state, SZrClosureValue *closureValue) {
    TZrUInt32 propagatedEscapeFlags;

    if (state == ZR_NULL || closureValue == ZR_NULL || !ZrCore_ClosureValue_IsClosed(closureValue) ||
        closureValue->anchoredEscapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    propagatedEscapeFlags = closureValue->captureEscapeFlags | closureValue->anchoredEscapeFlags;
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             &closureValue->link.closedValue,
                                             propagatedEscapeFlags,
                                             closureValue->captureScopeDepth,
                                             (EZrGarbageCollectPromotionReason)closureValue->anchoredPromotionReason);
}

SZrClosureNative *ZrCore_ClosureNative_New(struct SZrState *state, TZrSize closureValueCount) {
    TZrSize extraCaptureCount = closureValueCount > 1 ? closureValueCount - 1 : 0;
    TZrSize extraOwnerBytes = closureValueCount * sizeof(SZrRawObject *);
    SZrRawObject *object =
            ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE,
                                 sizeof(SZrClosureNative) + extraCaptureCount * sizeof(SZrTypeValue *) + extraOwnerBytes,
                                 ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->aotShimFunction = ZR_NULL;
    closure->closureValueCount = closureValueCount;
    if (closureValueCount > 0) {
        ZrCore_Memory_RawSet(closure->closureValuesExtend, 0, sizeof(SZrClosureValue *) * closureValueCount);
        ZrCore_Memory_RawSet(ZrCore_ClosureNative_GetCaptureOwners(closure), 0, extraOwnerBytes);
    }
    return closure;
}

SZrClosure *ZrCore_Closure_New(struct SZrState *state, TZrSize closureValueCount) {
    // SZrClosure 已经包含了 closureValuesExtend[1]，所以只需要分配 (closureValueCount - 1) 个额外的指针
    TZrSize extraSize = closureValueCount > 1 ? (closureValueCount - 1) * sizeof(SZrClosureValue *) : 0;
    SZrRawObject *object = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE,
                                          sizeof(SZrClosure) + extraSize, ZR_FALSE);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    closure->function = ZR_NULL;
    if (closureValueCount > 0) {
        ZrCore_Memory_RawSet(closure->closureValuesExtend, 0, sizeof(SZrClosureValue *) * closureValueCount);
    }
    return closure;
}

void ZrCore_Closure_InitValue(struct SZrState *state, SZrClosure *closure) {
    for (TZrSize i = 0; i < closure->closureValueCount; i++) {
        SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
        // if value is on stack
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
        ZrCore_Value_ResetAsNull(&closureValue->value.valuePointer->value);
        closureValue->captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
        closureValue->captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
        closureValue->anchoredEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
        closureValue->anchoredPromotionReason = (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
        closure->closureValuesExtend[i] = closureValue;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure), ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
    }
}

static SZrClosureValue *closure_value_new(struct SZrState *state, TZrStackValuePointer stackPointer,
                                          SZrClosureValue **previous) {
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
    SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
    SZrClosureValue *next = *previous;
    closureValue->value.valuePointer = stackPointer;
    closureValue->link.next = next;
    closureValue->link.previous = previous;
    closureValue->captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
    closureValue->captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    closureValue->anchoredEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    closureValue->anchoredPromotionReason = (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
    if (next) {
        next->link.previous = &closureValue->link.next;
    }
    *previous = closureValue;
    if (!ZrCore_State_IsInClosureValueThreadList(state)) {
        state->threadWithStackClosures = state->global->threadWithStackClosures;
        state->global->threadWithStackClosures = state;
    }
    return closureValue;
}

SZrClosureValue *ZrCore_Closure_FindOrCreateValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue **closureValues = &state->stackClosureValueList;
    SZrClosureValue *closureValue = ZR_NULL;
    ZR_ASSERT(ZrCore_State_IsInClosureValueThreadList(state) || state->stackClosureValueList == ZR_NULL);
    while (ZR_TRUE) {
        closureValue = *closureValues;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
        if (closureValue->value.valuePointer < stackPointer) {
            break;
        }
        // Open upvalues are anchored by the thread closure list and may survive GC cycles.
        ZR_ASSERT(!ZrCore_Gc_RawObjectIsDead(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue)));
        if (closureValue->value.valuePointer == stackPointer) {
            return closureValue;
        }
        closureValues = &closureValue->link.next;
    }
    return closure_value_new(state, stackPointer, closureValues);
}

void ZrCore_ClosureValue_SetCaptureMetadata(SZrClosureValue *closureValue,
                                            TZrUInt32 scopeDepth,
                                            TZrUInt32 escapeFlags) {
    if (closureValue == ZR_NULL) {
        return;
    }

    closureValue->captureScopeDepth = closure_merge_scope_depth(closureValue->captureScopeDepth, scopeDepth);
    closureValue->captureEscapeFlags |= escapeFlags;
}

void ZrCore_ClosureValue_AnchorEscape(SZrState *state,
                                      SZrClosureValue *closureValue,
                                      TZrUInt32 escapeFlags,
                                      EZrGarbageCollectPromotionReason promotionReason) {
    if (state == ZR_NULL || closureValue == ZR_NULL || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    closureValue->anchoredEscapeFlags |= escapeFlags;
    if (closureValue->anchoredPromotionReason == (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE ||
        closureValue->anchoredPromotionReason == (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL) {
        closureValue->anchoredPromotionReason = (TZrUInt32)promotionReason;
    }

    ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                 ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue),
                                                 escapeFlags,
                                                 closureValue->captureScopeDepth,
                                                 promotionReason);
    closure_value_apply_anchored_escape_to_closed_value(state, closureValue);
}

void ZrCore_Closure_PropagateEscapeFromObject(SZrState *state,
                                              SZrRawObject *closureObject,
                                              TZrUInt32 escapeFlags,
                                              EZrGarbageCollectPromotionReason promotionReason) {
    if (state == ZR_NULL || closureObject == ZR_NULL || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE ||
        closureObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return;
    }

    if (!closureObject->isNative) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureObject);
        if (closure == ZR_NULL) {
            return;
        }

        for (TZrUInt32 captureIndex = 0; captureIndex < closure->closureValueCount; captureIndex++) {
            SZrClosureValue *closureValue = closure->closureValuesExtend[captureIndex];

            if (closureValue == ZR_NULL) {
                continue;
            }

            if (closure->function != ZR_NULL &&
                closure->function->closureValueList != ZR_NULL &&
                captureIndex < closure->function->closureValueLength) {
                const SZrFunctionClosureVariable *closureVariable = &closure->function->closureValueList[captureIndex];
                ZrCore_ClosureValue_SetCaptureMetadata(closureValue,
                                                      closureVariable->scopeDepth,
                                                      closureVariable->escapeFlags);
            }

            ZrCore_ClosureValue_AnchorEscape(state, closureValue, escapeFlags, promotionReason);
        }
        return;
    }

    {
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, closureObject);
        SZrFunction *metadataFunction = closure != ZR_NULL ? closure->aotShimFunction : ZR_NULL;
        TZrUInt32 captureCount = closure != ZR_NULL ? (TZrUInt32)closure->closureValueCount : 0u;

        for (TZrUInt32 captureIndex = 0; captureIndex < captureCount; captureIndex++) {
            TZrUInt32 captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
            TZrUInt32 captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
            SZrRawObject *captureOwner;
            SZrTypeValue *captureValue;

            if (metadataFunction != ZR_NULL &&
                metadataFunction->closureValueList != ZR_NULL &&
                captureIndex < metadataFunction->closureValueLength) {
                captureScopeDepth = metadataFunction->closureValueList[captureIndex].scopeDepth;
                captureEscapeFlags = metadataFunction->closureValueList[captureIndex].escapeFlags;
            }

            captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, captureIndex);
            captureValue = ZrCore_ClosureNative_GetCaptureValue(closure, captureIndex);
            if (captureOwner != ZR_NULL && captureOwner->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
                SZrClosureValue *closureValue = (SZrClosureValue *)captureOwner;
                ZrCore_ClosureValue_SetCaptureMetadata(closureValue, captureScopeDepth, captureEscapeFlags);
                ZrCore_ClosureValue_AnchorEscape(state, closureValue, escapeFlags, promotionReason);
            } else if (captureValue != ZR_NULL) {
                ZrCore_GarbageCollector_MarkValueEscaped(state,
                                                         captureValue,
                                                         escapeFlags | captureEscapeFlags,
                                                         captureScopeDepth,
                                                         promotionReason);
            }
        }
    }
}

static TZrBool closure_value_check_close_meta(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrTypeValue *stackValue = ZrCore_Stack_GetValue(stackPointer);
    // todo: if it is a basic type
    SZrMeta *meta = ZrCore_Value_GetMeta(state, stackValue, ZR_META_CLOSE);
    return meta != ZR_NULL;
}

static void closure_value_call_close_meta(SZrState *state, SZrTypeValue *value, SZrTypeValue *error, TZrBool isYield) {
    TZrStackPointer top = state->stackTop;
    SZrCallInfo *callInfo = state->callInfoList;
    const SZrMeta *meta = ZrCore_Value_GetMeta(state, value, ZR_META_CLOSE);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        return;
    }
    top.valuePointer = ZrCore_Function_ReserveScratchSlots(state, 3, top.valuePointer);
    ZrCore_Stack_SetRawObjectValue(state, top.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrCore_Stack_CopyValue(state, top.valuePointer + 1, value);
    ZrCore_Stack_CopyValue(state, top.valuePointer + 2, error);
    state->stackTop.valuePointer = top.valuePointer + 3;
    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }
    if (isYield) {
        ZrCore_Function_Call(state, top.valuePointer, 0);
    } else {
        ZrCore_Function_CallWithoutYield(state, top.valuePointer, 0);
    }
}

static void closure_value_pre_call_close_meta(SZrState *state, TZrStackPointer stackPointer, EZrThreadStatus errorStatus,
                                           TZrBool isYield) {
    SZrTypeValue *value = ZrCore_Stack_GetValue(stackPointer.valuePointer);
    SZrTypeValue *error = ZR_NULL;
    if (errorStatus == ZR_THREAD_STATUS_INVALID) {
        error = &state->global->nullValue;
    } else {
        error = ZrCore_Stack_GetValue(stackPointer.valuePointer + 1);
        ZrCore_Exception_MarkError(state, errorStatus, stackPointer.valuePointer + 1);
    }
    closure_value_call_close_meta(state, value, error, isYield);
}


void ZrCore_Closure_ToBeClosedValueClosureNew(struct SZrState *state, TZrStackValuePointer stackPointer) {
    ZR_ASSERT(stackPointer > state->toBeClosedValueList.valuePointer);
    SZrTypeValue *stackValue = ZrCore_Stack_GetValue(stackPointer);
    if (ZR_VALUE_IS_TYPE_NULL(stackValue)) {
        return;
    }
    TZrBool hasCloseMeta = closure_value_check_close_meta(state, stackPointer);
    if (!hasCloseMeta) {
        return;
    }


    // extends to be closed value list
    while ((TZrSize)(stackPointer - state->toBeClosedValueList.valuePointer) > (TZrSize)MAX_DELTA) {
        state->toBeClosedValueList.valuePointer += MAX_DELTA;
        state->toBeClosedValueList.valuePointer->toBeClosedValueOffset = 0;
    }
    stackPointer->toBeClosedValueOffset = ZR_CAST(TZrUInt32, stackPointer - state->toBeClosedValueList.valuePointer);
    state->toBeClosedValueList.valuePointer = stackPointer;
}

void ZrCore_Closure_UnlinkValue(SZrClosureValue *closureValue) {
    ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
    *closureValue->link.previous = closureValue->link.next;
    if (closureValue->link.next) {
        closureValue->link.next->link.previous = closureValue->link.previous;
    }
}

void ZrCore_Closure_CloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue *closureValue = ZR_NULL;
    while (ZR_TRUE) {
        closureValue = state->stackClosureValueList;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
        // Open upvalues are kept in descending stack-slot order. Once we move
        // below the closing threshold, the remaining entries belong to older frames.
        if (closureValue->value.valuePointer < stackPointer) {
            break;
        }
        SZrTypeValue *slot = &closureValue->link.closedValue;
        TZrStackValuePointer sourcePointer = closureValue->value.valuePointer;
        SZrTypeValue *sourceValue = ZR_CAST_FROM_STACK_VALUE(sourcePointer);
        ZR_ASSERT(closureValue->value.valuePointer < state->stackTop.valuePointer);
        ZrCore_Closure_UnlinkValue(closureValue);
        ZrCore_Value_ResetAsNull(slot);
        ZrCore_Value_Copy(state, slot, sourceValue);
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(slot);
        closure_value_apply_anchored_escape_to_closed_value(state, closureValue);
        SZrRawObject *rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
        if (ZrCore_RawObject_IsWaitToScan(rawObject) || ZrCore_RawObject_IsReferenced(rawObject)) {
            ZrCore_RawObject_MarkAsReferenced(rawObject);
            ZrCore_RawObject_Barrier(state, rawObject, slot->value.object);
        }
    }
}

static void closure_pop_to_be_closed_list(SZrState *state) {
    TZrStackValuePointer toBeClosed = state->toBeClosedValueList.valuePointer;
    ZR_ASSERT(toBeClosed->toBeClosedValueOffset > 0);
    toBeClosed -= toBeClosed->toBeClosedValueOffset;
    while (toBeClosed > state->stackBase.valuePointer && toBeClosed->toBeClosedValueOffset == 0) {
        toBeClosed -= MAX_DELTA;
    }
    state->toBeClosedValueList.valuePointer = toBeClosed;
}

TZrStackValuePointer ZrCore_Closure_CloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                           EZrThreadStatus errorStatus, TZrBool isYield) {
    TZrMemoryOffset offset = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
    ZrCore_Closure_CloseStackValue(state, stackPointer);
    while (state->toBeClosedValueList.valuePointer >= stackPointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        closure_pop_to_be_closed_list(state);
        closure_value_pre_call_close_meta(state, toBeClosed, errorStatus, isYield);
        TZrStackValuePointer pointer = ZrCore_Stack_LoadOffsetToPointer(state, offset);
        stackPointer = pointer;
    }
    return stackPointer;
}

TZrSize ZrCore_Closure_CloseRegisteredValues(struct SZrState *state,
                                       TZrSize count,
                                       EZrThreadStatus errorStatus,
                                       TZrBool isYield) {
    TZrSize closedCount = ZR_CLOSURE_CLOSED_COUNT_NONE;

    if (state == ZR_NULL || count == 0) {
        return ZR_CLOSURE_CLOSED_COUNT_NONE;
    }

    while (closedCount < count &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        closure_pop_to_be_closed_list(state);
        closure_value_pre_call_close_meta(state, toBeClosed, errorStatus, isYield);
        closedCount++;
    }

    return closedCount;
}

void ZrCore_Closure_PushToStack(struct SZrState *state, struct SZrFunction *function, SZrClosureValue **closureValueList,
                           TZrStackValuePointer base, TZrStackValuePointer closurePointer) {
    function = closure_refresh_forwarded_function(function);
    if (function == ZR_NULL) {
        return;
    }

    TZrSize closureSize = function->closureValueLength;
    SZrClosure *closure = ZrCore_Closure_New(state, closureSize);
    closure = closure_refresh_forwarded_closure(closure);
    function = closure_refresh_forwarded_function(function);
    if (function == ZR_NULL || closure == ZR_NULL) {
        return;
    }

    SZrFunctionClosureVariable *closureVariables = function->closureValueList;
    closure->function = function;
    ZrCore_Stack_SetRawObjectValue(state, closurePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    if (closureValueList != ZR_NULL) {
        closureValueList = closure_refresh_parent_closure_values_from_base(state, base);
    }
    for (TZrSize i = 0; i < closureSize; i++) {
        SZrFunctionClosureVariable *closureValue = &closureVariables[i];
        SZrClosureValue *capturedValue;

        if (closureValue->inStack) {
            capturedValue = ZrCore_Closure_FindOrCreateValue(state, base + closureValue->index);
            closure = closure_refresh_forwarded_closure(closure);
            if (closureValueList != ZR_NULL) {
                closureValueList = closure_refresh_parent_closure_values_from_base(state, base);
            }
        } else {
            capturedValue = closureValueList != ZR_NULL ? closureValueList[closureValue->index] : ZR_NULL;
            capturedValue = closure_refresh_forwarded_closure_value(capturedValue);
        }
        if (closure == ZR_NULL) {
            return;
        }
        closure->closureValuesExtend[i] = capturedValue;
        if (closure->closureValuesExtend[i] != ZR_NULL) {
            ZrCore_ClosureValue_SetCaptureMetadata(closure->closureValuesExtend[i],
                                                  closureValue->scopeDepth,
                                                  closureValue->escapeFlags);
        }
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                           ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
    }
}

SZrFunction *ZrCore_Closure_GetMetadataFunctionFromValue(struct SZrState *state, const SZrTypeValue *value) {
    SZrRawObject *rawObject;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            if (value->value.object == ZR_NULL) {
                return ZR_NULL;
            }
            break;
        default:
            return ZR_NULL;
    }

    rawObject = closure_refresh_forwarded_raw_object(value->value.object);
    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION) {
        return value->isNative ? ZR_NULL : closure_refresh_forwarded_function(ZR_CAST_FUNCTION(state, rawObject));
    }

    if (value->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawObject);
        return nativeClosure != ZR_NULL ? closure_refresh_forwarded_function(nativeClosure->aotShimFunction) : ZR_NULL;
    }

    {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, rawObject);
        return closure != ZR_NULL ? closure_refresh_forwarded_function(closure->function) : ZR_NULL;
    }
}

SZrFunction *ZrCore_Closure_GetMetadataFunctionFromCallInfo(struct SZrState *state, struct SZrCallInfo *callInfo) {
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Closure_GetMetadataFunctionFromValue(state,
                                                       ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer));
}


#undef MAX_DELTA
