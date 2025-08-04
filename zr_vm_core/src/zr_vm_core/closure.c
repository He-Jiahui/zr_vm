//
// Created by HeJiahui on 2025/7/15.
//
#include "zr_vm_core/closure.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/state.h"
#define MAX_DELTA ((256UL << ((sizeof(state->stackBase.valuePointer->toBeClosedValueOffset) - 1) * 8)) - 1)
SZrClosureNative *ZrClosureNativeNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object =
            ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION,
                           sizeof(SZrClosureNative) + sizeof(SZrClosureValue *) * closureValueCount, ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    ZrMemoryRawSet(closure->closureValuesExtend, (TByte) ZR_NULL, sizeof(SZrClosureValue *) * closureValueCount);
    return closure;
}

SZrClosure *ZrClosureNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object = ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION,
                                          sizeof(SZrClosure) + sizeof(SZrClosureValue *) * closureValueCount, ZR_FALSE);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    closure->function = ZR_NULL;
    ZrMemoryRawSet(closure->closureValuesExtend, (TByte) ZR_NULL, sizeof(SZrClosureValue *) * closureValueCount);
    return closure;
}

void ZrClosureInitValue(struct SZrState *state, SZrClosure *closure) {
    for (TZrSize i = 0; i < closure->closureValueCount; i++) {
        SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
        // if value is on stack
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
        ZrValueResetAsNull(&closureValue->value.valuePointer->value);
        closure->closureValuesExtend[i] = closureValue;
        ZrRawObjectBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure), ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
    }
}

static SZrClosureValue *ZrClosureValueNew(struct SZrState *state, TZrStackValuePointer stackPointer,
                                          SZrClosureValue **previous) {
    SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
    SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
    SZrClosureValue *next = *previous;
    closureValue->value.valuePointer = stackPointer;
    closureValue->link.next = next;
    closureValue->link.previous = previous;
    if (next) {
        next->link.previous = &closureValue->link.next;
    }
    *previous = closureValue;
    if (!ZrStateIsInClosureValueThreadList(state)) {
        state->threadWithStackClosures = state->global->threadWithStackClosures;
        state->global->threadWithStackClosures = state;
    }
    return closureValue;
}

SZrClosureValue *ZrClosureFindOrCreateValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue **closureValues = &state->stackClosureValueList;
    SZrClosureValue *closureValue = ZR_NULL;
    ZR_ASSERT(ZrStateIsInClosureValueThreadList(state) || state->stackClosureValueList == ZR_NULL);
    while (ZR_TRUE) {
        closureValue = *closureValues;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrClosureValueIsClosed(closureValue));
        if (closureValue->value.valuePointer < stackPointer) {
            break;
        }
        ZR_ASSERT(ZrGcRawObjectIsDead(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue)));
        if (closureValue->value.valuePointer == stackPointer) {
            return closureValue;
        }
        closureValues = &closureValue->link.next;
    }
    return ZrClosureValueNew(state, stackPointer, closureValues);
}

static TBool ZrClosureValueCheckCloseMeta(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrTypeValue *stackValue = ZrStackGetValue(stackPointer);
    // todo: if it is a basic type
    SZrMeta *meta = ZrValueGetMeta(state, stackValue, ZR_META_CLOSE);
    return meta != ZR_NULL;
}

static void ZrClosureValueCallCloseMeta(SZrState *state, SZrTypeValue *value, SZrTypeValue *error, TBool isYield) {
    TZrStackPointer top = state->stackTop;
    const SZrMeta *meta = ZrValueGetMeta(state, value, ZR_META_CLOSE);
    ZrStackSetRawObjectValue(state, top.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrStackCopyValue(state, top.valuePointer + 1, value);
    ZrStackCopyValue(state, top.valuePointer + 2, error);
    state->stackTop.valuePointer = top.valuePointer + 3;
    if (isYield) {
        ZrFunctionCall(state, top.valuePointer, 0);
    } else {
        ZrFunctionCallWithoutYield(state, top.valuePointer, 0);
    }
}

static void ZrClosureValuePreCallCloseMeta(SZrState *state, TZrStackPointer stackPointer, EZrThreadStatus errorStatus,
                                           TBool isYield) {
    SZrTypeValue *value = ZrStackGetValue(stackPointer.valuePointer);
    SZrTypeValue *error = ZR_NULL;
    if (errorStatus == ZR_THREAD_STATUS_INVALID) {
        error = &state->global->nullValue;
    } else {
        error = ZrStackGetValue(stackPointer.valuePointer + 1);
        ZrExceptionMarkError(state, errorStatus, stackPointer.valuePointer + 1);
    }
    ZrClosureValueCallCloseMeta(state, value, error, isYield);
}


void ZrClosureToBeClosedValueClosureNew(struct SZrState *state, TZrStackValuePointer stackPointer) {
    ZR_ASSERT(stackPointer > state->toBeClosedValueList.valuePointer);
    SZrTypeValue *stackValue = ZrStackGetValue(stackPointer);
    if (ZR_VALUE_IS_TYPE_NULL(stackValue)) {
        return;
    }
    TBool hasCloseMeta = ZrClosureValueCheckCloseMeta(state, stackPointer);
    if (!hasCloseMeta) {
        // todo : log error
        ZrLogError(state, "");
    }


    // extends to be closed value list
    while (stackPointer - state->toBeClosedValueList.valuePointer > MAX_DELTA) {
        state->toBeClosedValueList.valuePointer += MAX_DELTA;
        state->toBeClosedValueList.valuePointer->toBeClosedValueOffset = 0;
    }
    stackPointer->toBeClosedValueOffset = ZR_CAST(TUInt32, stackPointer - state->toBeClosedValueList.valuePointer);
    state->toBeClosedValueList.valuePointer = stackPointer;
}

void ZrClosureUnlinkValue(SZrClosureValue *closureValue) {
    ZR_ASSERT(!ZrClosureValueIsClosed(closureValue));
    *closureValue->link.previous = closureValue->link.next;
    if (closureValue->link.next) {
        closureValue->link.next->link.previous = closureValue->link.previous;
    }
}

void ZrClosureCloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue *closureValue = ZR_NULL;
    while (ZR_TRUE) {
        closureValue = state->stackClosureValueList;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrClosureValueIsClosed(closureValue));
        if (closureValue->value.valuePointer >= stackPointer) {
            break;
        }
        SZrTypeValue *slot = &closureValue->link.closedValue;
        ZR_ASSERT(closureValue->value.valuePointer < state->stackTop.valuePointer);
        ZrClosureUnlinkValue(closureValue);
        ZrValueCopy(state, slot, ZR_CAST_FROM_STACK_VALUE(closureValue->value.valuePointer));
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(slot);
        SZrRawObject *rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
        if (ZrRawObjectIsWaitToScan(rawObject) || ZrRawObjectIsReferenced(rawObject)) {
            ZrRawObjectMarkAsReferenced(rawObject);
            ZrRawObjectBarrier(state, rawObject, slot->value.object);
        }
    }
}

static void ZrClosurePopToBeClosedList(SZrState *state) {
    TZrStackValuePointer toBeClosed = state->toBeClosedValueList.valuePointer;
    ZR_ASSERT(toBeClosed->toBeClosedValueOffset > 0);
    toBeClosed -= toBeClosed->toBeClosedValueOffset;
    while (toBeClosed > state->stackBase.valuePointer && toBeClosed->toBeClosedValueOffset == 0) {
        toBeClosed -= MAX_DELTA;
    }
    state->toBeClosedValueList.valuePointer = toBeClosed;
}

TZrStackValuePointer ZrClosureCloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                           EZrThreadStatus errorStatus, TBool isYield) {
    TZrMemoryOffset offset = ZrStackSavePointerAsOffset(state, stackPointer);
    ZrClosureCloseStackValue(state, stackPointer);
    while (state->toBeClosedValueList.valuePointer >= stackPointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrClosurePopToBeClosedList(state);
        ZrClosureValuePreCallCloseMeta(state, toBeClosed, errorStatus, isYield);
        TZrStackValuePointer pointer = ZrStackLoadOffsetToPointer(state, offset);
        stackPointer = pointer;
    }
    return stackPointer;
}

void ZrClosurePushToStack(struct SZrState *state, struct SZrFunction *function, SZrClosureValue **closureValueList,
                          TZrStackValuePointer base, TZrStackValuePointer closurePointer) {
    TZrSize closureSize = function->closureValueLength;
    SZrFunctionClosureVariable *closureVariables = function->closureValueList;
    SZrClosure *closure = ZrClosureNew(state, closureSize);
    closure->function = function;
    ZrStackSetRawObjectValue(state, closurePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    for (TZrSize i = 0; i < closureSize; i++) {
        SZrFunctionClosureVariable *closureValue = &closureVariables[i];
        if (closureValue->inStack) {
            closure->closureValuesExtend[i] = ZrClosureFindOrCreateValue(state, base + closureValue->index);
        } else {
            closure->closureValuesExtend[i] = closureValueList[closureValue->index];
        }
        ZrRawObjectBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                           ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
    }
}


#undef MAX_DELTA
