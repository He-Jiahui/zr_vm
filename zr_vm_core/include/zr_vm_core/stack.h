//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_STACK_H
#define ZR_VM_CORE_STACK_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

#define ZR_STACK_NATIVE_CALL_MIN 20

struct ZR_STRUCT_ALIGN SZrTypeValueOnStack {
    SZrTypeValue value;
    TUInt32 toBeReleasedValueOffset;
};

typedef struct SZrTypeValueOnStack SZrTypeValueOnStack;

typedef SZrTypeValueOnStack *TZrStackValuePointer;

union TZrStackPtr {
    TZrStackValuePointer valuePointer;
    TZrMemoryOffset reusableValueOffset;
};

typedef union TZrStackPtr TZrStackPointer;

ZR_CORE_API TZrPtr ZrStackInit(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

ZR_FORCE_INLINE TZrSize ZrStackSize(TZrStackPointer *stackBase, TZrStackPointer *stackTop) {
    ZR_ASSERT(stackTop->valuePointer >= stackBase->valuePointer);
    return (TZrSize) (stackTop->valuePointer - stackBase->valuePointer);
}

ZR_FORCE_INLINE void ZrStackSetValue(struct SZrState *state, SZrTypeValueOnStack *destination, SZrTypeValue *source) {
    ZR_TODO_PARAMETER(state);
    SZrTypeValue *destinationValue = &destination->value;
    destinationValue->value.object = source->value.object;
    destinationValue->type = source->type;
    // todo: 检查 setsvalue
}


#endif //ZR_VM_CORE_STACK_H
