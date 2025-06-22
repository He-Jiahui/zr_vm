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
