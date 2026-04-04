//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_CLOSURE_H
#define ZR_VM_CORE_CLOSURE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"

struct SZrState;
struct SZrClosureValue;
struct SZrCallInfo;

union TZrClosureLink {
    struct {
        struct SZrClosureValue *next;
        struct SZrClosureValue **previous;
    };
    // if value is not on stack, value is closed
    SZrTypeValue closedValue;
};

struct ZR_STRUCT_ALIGN SZrClosureValue {
    SZrRawObject super;
    TZrStackPointer value;
    union TZrClosureLink link;
};

typedef struct SZrClosureValue SZrClosureValue;


struct ZR_STRUCT_ALIGN SZrClosureNative {
    SZrRawObject super;
    // SZrRawObject *gcList;
    FZrNativeFunction nativeFunction;
    struct SZrFunction *aotShimFunction;
    TZrSize closureValueCount;
    SZrTypeValue *closureValuesExtend[1];
};

typedef struct SZrClosureNative SZrClosureNative;

struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super;
    // SZrRawObject *gcList;
    // todo: closure info
    SZrFunction *function;
    TZrSize closureValueCount;
    SZrClosureValue *closureValuesExtend[1];
};

typedef struct SZrClosure SZrClosure;

union TZrClosure {
    SZrClosureNative nativeClosure;
    SZrClosure zrClosure;
};

typedef union TZrClosure TZrClosure;

ZR_CORE_API SZrClosureNative *ZrCore_ClosureNative_New(struct SZrState *state, TZrSize closureValueCount);

ZR_CORE_API SZrClosure *ZrCore_Closure_New(struct SZrState *state, TZrSize closureValueCount);

ZR_CORE_API void ZrCore_Closure_InitValue(struct SZrState *state, SZrClosure *closure);

ZR_CORE_API SZrClosureValue *ZrCore_Closure_FindOrCreateValue(struct SZrState *state, TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Closure_ToBeClosedValueClosureNew(struct SZrState *state, TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Closure_UnlinkValue(SZrClosureValue *closureValue);

ZR_CORE_API void ZrCore_Closure_CloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer);

ZR_CORE_API TZrStackValuePointer ZrCore_Closure_CloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                       EZrThreadStatus errorStatus, TZrBool isYield);

ZR_CORE_API TZrSize ZrCore_Closure_CloseRegisteredValues(struct SZrState *state,
                                                   TZrSize count,
                                                   EZrThreadStatus errorStatus,
                                                   TZrBool isYield);

ZR_CORE_API void ZrCore_Closure_PushToStack(struct SZrState *state, struct SZrFunction *function,
                                      SZrClosureValue **closureValueList, TZrStackValuePointer base,
                                      TZrStackValuePointer closurePointer);

ZR_CORE_API struct SZrFunction *ZrCore_Closure_GetMetadataFunctionFromValue(struct SZrState *state,
                                                                            const SZrTypeValue *value);

ZR_CORE_API struct SZrFunction *ZrCore_Closure_GetMetadataFunctionFromCallInfo(struct SZrState *state,
                                                                               struct SZrCallInfo *callInfo);

ZR_FORCE_INLINE TZrBool ZrCore_ClosureValue_IsClosed(SZrClosureValue *closureValue) {
    return closureValue->value.valuePointer == ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
}

ZR_FORCE_INLINE SZrTypeValue *ZrCore_ClosureValue_GetValue(SZrClosureValue *closureValue) {
    if (ZrCore_ClosureValue_IsClosed(closureValue)) {
        return &closureValue->link.closedValue;
    }
    return ZrCore_Stack_GetValue(closureValue->value.valuePointer);
}


#endif // ZR_VM_CORE_CLOSURE_H
