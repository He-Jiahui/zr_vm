//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/stack.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"


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
    if (state->stackEnd.valuePointer - state->stackTop.valuePointer > (TZrMemoryOffset) space) {
        result = ZR_TRUE;
    } else {
        result = ZrStateStackRealloc(state, space, ZR_FALSE);
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

