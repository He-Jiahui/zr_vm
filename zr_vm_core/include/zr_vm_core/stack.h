//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_STACK_H
#define ZR_VM_CORE_STACK_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/value.h"

struct ZR_STRUCT_ALIGN SZrTypeValueOnStack {
    SZrTypeValue value;
    TZrUInt32 toBeClosedValueOffset;
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
             (TZrMemoryOffset) (COUNT) <=                                                                              \
                     ((STATE)->stackTop.valuePointer - (STATE)->callInfoList->functionBase.valuePointer),              \
             "not enough elements in the stack")

ZR_CORE_API TZrPtr ZrCore_Stack_Construct(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

ZR_CORE_API void ZrCore_Stack_Deconstruct(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

ZR_CORE_API TZrStackValuePointer ZrCore_Stack_GetAddressFromOffset(struct SZrState *state, TZrMemoryOffset offset);

ZR_CORE_API TZrBool ZrCore_Stack_GrowTo(struct SZrState *state, TZrSize requiredSize, TZrBool canThrowError);

ZR_CORE_API TZrBool ZrCore_Stack_Grow(struct SZrState *state, TZrSize space, TZrBool canThrowError);

ZR_CORE_API TZrBool ZrCore_Stack_CheckFullAndGrow(struct SZrState *state, TZrSize space, TZrNativeString errorMessage);


ZR_FORCE_INLINE TZrSize ZrCore_Stack_UsedSize(TZrStackPointer *stackBase, TZrStackPointer *stackTop) {
    ZR_ASSERT(stackTop->valuePointer >= stackBase->valuePointer);
    return (TZrSize) (stackTop->valuePointer - stackBase->valuePointer);
}

static ZR_FORCE_INLINE SZrTypeValue *ZrCore_Stack_GetValueNoProfile(SZrTypeValueOnStack *valueOnStack) {
    return &valueOnStack->value;
}

static ZR_FORCE_INLINE SZrTypeValue *ZrCore_Stack_GetValue(SZrTypeValueOnStack *valueOnStack) {
    ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_STACK_GET_VALUE);
    return ZrCore_Stack_GetValueNoProfile(valueOnStack);
}

ZR_CORE_API void ZrCore_Stack_SetRawObjectValue(struct SZrState *state, SZrTypeValueOnStack *destination,
                                          SZrRawObject *object);

ZR_CORE_API void ZrCore_Stack_CopyValue(struct SZrState *state,
                                        SZrTypeValueOnStack *destination,
                                        const SZrTypeValue *source);

ZR_CORE_API TZrMemoryOffset ZrCore_Stack_SavePointerAsOffset(struct SZrState *state, TZrStackValuePointer stackPointer);

ZR_CORE_API TZrStackValuePointer ZrCore_Stack_LoadOffsetToPointer(struct SZrState *state, TZrMemoryOffset offset);

#endif // ZR_VM_CORE_STACK_H
