//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/stack.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"

ZR_FORCE_INLINE TZrMemoryOffset ZrStackSaveAsOffset(SZrState *state, TZrStackValuePointer pointer) {
    return (TBytePtr) pointer - (TBytePtr) state->stackBase.valuePointer;
}

ZR_FORCE_INLINE TZrStackValuePointer ZrStackLoadAsOffset(SZrState *state, TZrMemoryOffset offset) {
    return ZR_CAST_STACK_OBJECT((TBytePtr)state->stackBase.valuePointer + offset);
}

static void ZrStackMarkStackAsRelative(SZrState *state) {
    state->stackTop.reusableValueOffset = ZrStackSaveAsOffset(state, state->stackTop.valuePointer);
    state->waitToReleaseList.reusableValueOffset = ZrStackSaveAsOffset(
        state, state->waitToReleaseList.valuePointer);
    // closures
    for (SZrClosureValue *closureValue = state->aliveClosureValueList; closureValue != ZR_NULL;
         closureValue = closureValue->link.next) {
        closureValue->value.reusableValueOffset = ZrStackSaveAsOffset(
            state, closureValue->value.valuePointer);
    }
    // call infos
    for (SZrCallInfo *callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        callInfo->functionBase.reusableValueOffset = ZrStackSaveAsOffset(
            state, callInfo->functionBase.valuePointer);
        callInfo->functionTop.reusableValueOffset = ZrStackSaveAsOffset(state, callInfo->functionTop.valuePointer);
    }
}

static void ZrStackMarkStackAsAbsolute(SZrState *state) {
    state->stackTop.valuePointer = ZrStackLoadAsOffset(state, state->stackTop.reusableValueOffset);
    state->waitToReleaseList.valuePointer = ZrStackLoadAsOffset(state, state->waitToReleaseList.reusableValueOffset);
    for (SZrClosureValue *closureValue = state->aliveClosureValueList; closureValue != ZR_NULL;
         closureValue = closureValue->link.next) {
        closureValue->value.valuePointer = ZrStackLoadAsOffset(state, closureValue->value.reusableValueOffset);
    }
    for (SZrCallInfo *callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        callInfo->functionBase.valuePointer = ZrStackLoadAsOffset(state, callInfo->functionBase.reusableValueOffset);
        callInfo->functionTop.valuePointer = ZrStackLoadAsOffset(state, callInfo->functionTop.reusableValueOffset);
        if (!ZrCallInfoIsNative(callInfo)) {
            callInfo->context.context.trap = 1;
        }
    }
}

static TBool ZrStackRealloc(SZrState *state, TUInt64 newSize, TBool throwError) {
    SZrGlobalState *global = state->global;
    TZrSize previousStackSize = ZrStateStackGetSize(state);
    TBool previousStopGcFlag = state->global->garbageCollector.stopGcFlag;
    ZR_ASSERT(newSize <= ZR_VM_MAX_STACK || newSize == ZR_VM_ERROR_STACK);
    ZrStackMarkStackAsRelative(state);
    state->global->garbageCollector.stopGcFlag = ZR_TRUE;
    TZrStackValuePointer newStackPointer = ZR_CAST_STACK_OBJECT(
        ZrMemoryAllocate(global, state->stackBase.valuePointer, previousStackSize + ZR_THREAD_STACK_SIZE_EXTRA,newSize +
            ZR_THREAD_STACK_SIZE_EXTRA));
    state->global->garbageCollector.stopGcFlag = previousStopGcFlag;
    if (ZR_UNLIKELY(newStackPointer == ZR_NULL)) {
        // todo:
        ZrStackMarkStackAsAbsolute(state);
        if (throwError) {
            ZrExceptionThrow(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
        return ZR_FALSE;
    }
    state->stackBase.valuePointer = newStackPointer;
    ZrStackMarkStackAsAbsolute(state);
    state->stackTail.valuePointer = newStackPointer + newSize;
    for (TZrSize i = previousStackSize + ZR_THREAD_STACK_SIZE_EXTRA; i < newSize + ZR_THREAD_STACK_SIZE_EXTRA; i++) {
        ZrValueResetAsNull(ZrStackGetValue(newStackPointer + i));
    }
    return ZR_TRUE;
}

TZrPtr ZrStackInit(SZrState *state, TZrStackPointer *stack, TZrSize stackLength) {
    ZR_ASSERT(stackLength > 0);
    SZrGlobalState *global = state->global;
    TZrSize stackByteSize = sizeof(SZrTypeValueOnStack) * stackLength;
    stack->valuePointer = ZR_CAST_STACK_OBJECT(ZrMemoryRawMalloc(global, stackByteSize));
    return ZR_CAST_PTR(stack->valuePointer + stackLength);
}

TZrStackValuePointer ZrStackGetAddressFromOffset(struct SZrState *state, TZrMemoryOffset offset) {
    SZrCallInfo *callInfoTop = state->callInfoList;
    if (offset > 0) {
        TZrStackValuePointer address = callInfoTop->functionBase.valuePointer + offset;
        ZR_CHECK(state, address < state->stackTop.valuePointer, "stack index overflow from function base to stack top");
        return address;
    }
    // cannot access global module or closure
    ZR_CHECK(state, offset <= ZR_VM_STACK_GLOBAL_MODULE_REGISTRY,
             "cannot access global module registry or closure offset");
    // negative index from top to base
    ZR_CHECK(
        state, offset != 0 && -offset <= state->stackTop.valuePointer - (callInfoTop->functionBase.valuePointer + 1),
        "stack index overflow from stack top to function base");
    return state->stackTop.valuePointer + offset;
}

TBool ZrStackCheckFullAndGrow(SZrState *state, TZrSize space, TNativeString errorMessage) {
    TBool result = ZR_FALSE;
    ZR_THREAD_LOCK(state);
    SZrCallInfo *callInfoTop = state->callInfoList;
    ZR_CHECK(state, space > 0, "stack space to grow must be positive");
    if (state->stackTail.valuePointer - state->stackTop.valuePointer > (TZrMemoryOffset) space) {
        result = ZR_TRUE;
    } else {
        result = ZrStackRealloc(state, space, ZR_FALSE);
    }
    if (result && callInfoTop->functionTop.valuePointer < state->stackTop.valuePointer + space) {
        callInfoTop->functionTop.valuePointer = state->stackTop.valuePointer + space;
    }
    ZR_THREAD_UNLOCK(state);
    if (ZR_UNLIKELY(!result)) {
        if (errorMessage) {
            ZrLogError(state, "stack overflow: %s", errorMessage);
        } else {
            ZrLogError(state, "stack overflow");
        }
    }
    return result;
}

void ZrStackSetRawObjectValue(struct SZrState *state, SZrTypeValueOnStack *destination, SZrRawObject *object) {
    ZR_UNUSED_PARAMETER(state);
    SZrTypeValue *destinationValue = ZrStackGetValue(destination);
    ZrValueInitAsRawObject(state, destinationValue, object);
    destinationValue->isNative = ZR_FALSE;
    destinationValue->isGarbageCollectable = ZR_TRUE;
    ZrGlobalValueStaticAssertIsAlive(state, destinationValue);
}


void ZrStackCopyValue(SZrState *state, SZrTypeValueOnStack *destination, SZrTypeValue *source) {
    ZR_UNUSED_PARAMETER(state);
    SZrTypeValue *destinationValue = ZrStackGetValue(destination);
    destinationValue->value.object = source->value.object;
    destinationValue->type = source->type;
    destinationValue->isGarbageCollectable = source->isGarbageCollectable;
    destinationValue->isNative = source->isNative;
    ZrGlobalValueStaticAssertIsAlive(state, destinationValue);
}
