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
    TUInt32 toBeClosedValueOffset;
};

typedef struct SZrTypeValueOnStack SZrTypeValueOnStack;

typedef SZrTypeValueOnStack *TZrStackValuePointer;

union TZrStackPtr {
    TZrStackValuePointer valuePointer;
    TZrMemoryOffset reusableValueOffset;
};

typedef union TZrStackPtr TZrStackPointer;

#define ZR_STACK_CHECK_CALL_INFO_STACK_COUNT(STATE, COUNT)                                                             \
    ZR_CHECK(state,                                                                                                    \
             (TZrMemoryOffset) (COUNT) <                                                                               \
                     ((STATE)->stackTop.valuePointer - (STATE)->callInfoList->functionBase.valuePointer),              \
             "not enough elements in the stack")

ZR_CORE_API TZrPtr ZrStackInit(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

ZR_CORE_API TZrStackValuePointer ZrStackGetAddressFromOffset(struct SZrState *state, TZrMemoryOffset offset);

ZR_CORE_API TBool ZrStackGrow(struct SZrState *state, TZrSize space, TBool canThrowError);

ZR_CORE_API TBool ZrStackCheckFullAndGrow(struct SZrState *state, TZrSize space, TNativeString errorMessage);


ZR_FORCE_INLINE TZrSize ZrStackUsedSize(TZrStackPointer *stackBase, TZrStackPointer *stackTop) {
    ZR_ASSERT(stackTop->valuePointer >= stackBase->valuePointer);
    return (TZrSize) (stackTop->valuePointer - stackBase->valuePointer);
}

ZR_FORCE_INLINE SZrTypeValue *ZrStackGetValue(SZrTypeValueOnStack *valueOnStack) { return &valueOnStack->value; }

ZR_CORE_API void ZrStackSetRawObjectValue(struct SZrState *state, SZrTypeValueOnStack *destination,
                                          SZrRawObject *object);

ZR_CORE_API void ZrStackCopyValue(struct SZrState *state, SZrTypeValueOnStack *destination, SZrTypeValue *source);

ZR_CORE_API TZrMemoryOffset ZrStackSavePointerAsOffset(struct SZrState *state, TZrStackValuePointer stackPointer);

ZR_CORE_API TZrStackValuePointer ZrStackLoadOffsetToPointer(struct SZrState *state, TZrMemoryOffset offset);

#endif // ZR_VM_CORE_STACK_H
