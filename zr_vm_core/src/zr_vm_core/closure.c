//
// Created by HeJiahui on 2025/7/15.
//
#include "zr_vm_core/closure.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"

SZrClosureNative *ZrClosureNativeNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object = ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION, sizeof(SZrClosureNative), ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    return closure;
}

SZrClosure *ZrClosureNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object = ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION,
                                          sizeof(SZrClosure) + sizeof(SZrClosureValue) * closureValueCount, ZR_FALSE);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    closure->function = ZR_NULL;
    ZrMemoryRawSet(closure->closureValuesExtend, (TByte) ZR_NULL, sizeof(SZrClosureValue) * closureValueCount);
    return closure;
}

void ZrClosureInitValue(struct SZrState *state, SZrClosure *closure) {
    for (TZrSize i = 0; i < closure->closureValueCount; i++) {
        SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
        // if value is on stack
        closureValue->value.valuePointer = ZR_CAST_STACK_OBJECT(&closureValue->link.independentValue);
        ZrValueResetAsNull(&closureValue->value.valuePointer->value);
        closure->closureValuesExtend[i] = *closureValue;
        ZrGarbageCollectorBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                  ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
    }
}

static SZrClosureValue *ZrClosureValueNew(struct SZrState *state, TZrStackPointer stackPointer,
                                          SZrClosureValue **previous) {
    SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
    SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
    SZrClosureValue *next = *previous;
    closureValue->value.valuePointer = stackPointer.valuePointer;
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

SZrClosureValue *ZrClosureFindOrCreateValue(struct SZrState *state, TZrStackPointer stackPointer) {
    SZrClosureValue **closureValues = &state->stackClosureValueList;
    SZrClosureValue *closureValue = ZR_NULL;
    ZR_ASSERT(ZrStateIsInClosureValueThreadList(state) || state->stackClosureValueList == ZR_NULL);
    while (ZR_TRUE) {
        closureValue = *closureValues;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrClosureValueIsIndependent(state, closureValue));
        if (closureValue->value.valuePointer < stackPointer.valuePointer) {
            break;
        }
        ZR_ASSERT(ZrGlobalRawObjectIsDead(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue)));
        if (closureValue->value.valuePointer == stackPointer.valuePointer) {
            return closureValue;
        }
        closureValues = &closureValue->link.next;
    }
    return ZrClosureValueNew(state, stackPointer, closureValues);
}

static TBool ZrClosureValueCheckCloseMeta(struct SZrState *state, TZrStackPointer stackPointer) {
    SZrTypeValue *stackValue = ZrStackGetValue(stackPointer.valuePointer);
    // todo: if it is a basic type
    if (ZR_VALUE_IS_TYPE_OBJECT(stackValue->type)) {
        SZrObject *object = ZR_CAST_OBJECT(state, stackValue->value.object);
        SZrMeta *meta = ZrObjectGetMetaRecursively(state, object, ZR_META_CLOSE);
        if (meta == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        SZrObjectPrototype *basicPrototype = state->global->basicTypeObjectPrototype[stackValue->type];
        SZrMeta *meta = ZrPrototypeGetMetaRecursively(state, basicPrototype, ZR_META_CLOSE);
        if (meta == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}


void ZrClosureToBeClosedValueClosureNew(struct SZrState *state, TZrStackPointer stackPointer) {
    ZR_ASSERT(stackPointer.valuePointer > state->toBeClosedValueList.valuePointer);
    SZrTypeValue *stackValue = ZrStackGetValue(stackPointer.valuePointer);
    if (ZR_VALUE_IS_TYPE_NULL(stackValue)) {
        return;
    }
    TBool hasCloseMeta = ZrClosureValueCheckCloseMeta(state, stackPointer);
    if (!hasCloseMeta) {
        // todo : log error
        ZrLogError(state, "");
    }

#define MAX_DELTA ((256UL << ((sizeof(state->stackBase.valuePointer->toBeClosedValueOffset) - 1) * 8)) - 1)
    // extends to be closed value list
    while (stackPointer.valuePointer - state->toBeClosedValueList.valuePointer > MAX_DELTA) {
        state->toBeClosedValueList.valuePointer += MAX_DELTA;
        state->toBeClosedValueList.valuePointer->toBeClosedValueOffset = 0;
    }
    stackPointer.valuePointer->toBeClosedValueOffset =
            ZR_CAST(TUInt32, stackPointer.valuePointer - state->toBeClosedValueList.valuePointer);
    state->toBeClosedValueList.valuePointer = stackPointer.valuePointer;
#undef MAX_DELTA
}
