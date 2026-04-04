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

SZrClosureNative *ZrCore_ClosureNative_New(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object =
            ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE,
                           sizeof(SZrClosureNative) + sizeof(SZrClosureValue *) * closureValueCount, ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->aotShimFunction = ZR_NULL;
    closure->closureValueCount = closureValueCount;
    if (closureValueCount > 0) {
        ZrCore_Memory_RawSet(closure->closureValuesExtend, 0, sizeof(SZrClosureValue *) * closureValueCount);
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
        ZR_ASSERT(closureValue->value.valuePointer < state->stackTop.valuePointer);
        ZrCore_Closure_UnlinkValue(closureValue);
        ZrCore_Value_ResetAsNull(slot);
        ZrCore_Value_Copy(state, slot, ZR_CAST_FROM_STACK_VALUE(closureValue->value.valuePointer));
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(slot);
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
    TZrSize closureSize = function->closureValueLength;
    SZrFunctionClosureVariable *closureVariables = function->closureValueList;
    SZrClosure *closure = ZrCore_Closure_New(state, closureSize);
    closure->function = function;
    ZrCore_Stack_SetRawObjectValue(state, closurePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    for (TZrSize i = 0; i < closureSize; i++) {
        SZrFunctionClosureVariable *closureValue = &closureVariables[i];
        if (closureValue->inStack) {
            closure->closureValuesExtend[i] = ZrCore_Closure_FindOrCreateValue(state, base + closureValue->index);
        } else {
            closure->closureValuesExtend[i] = closureValueList[closureValue->index];
        }
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                           ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
    }
}

SZrFunction *ZrCore_Closure_GetMetadataFunctionFromValue(struct SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION) {
        return value->isNative ? ZR_NULL : ZR_CAST_FUNCTION(state, value->value.object);
    }

    if (value->type != ZR_VALUE_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    if (value->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
        return nativeClosure != ZR_NULL ? nativeClosure->aotShimFunction : ZR_NULL;
    }

    {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
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
