//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_CLOSURE_H
#define ZR_VM_CORE_CLOSURE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/convertion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"

struct SZrState;
struct SZrClosureValue;

union TZrClosureLink {
    struct {
        struct SZrClosureValue *next;
        struct SZrClosureValue **previous;
    };
    // if value is not on stack, value is independent
    SZrTypeValue independentValue;
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
    TZrSize closureValueCount;
    SZrTypeValue closureValuesExtend[1];
};

typedef struct SZrClosureNative SZrClosureNative;

struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super;
    // SZrRawObject *gcList;
    // todo: closure info
    SZrFunction *function;
    TZrSize closureValueCount;
    SZrClosureValue closureValuesExtend[1];
};

typedef struct SZrClosure SZrClosure;

union TZrClosure {
    SZrClosureNative nativeClosure;
    SZrClosure zrClosure;
};

typedef union TZrClosure TZrClosure;

ZR_CORE_API SZrClosureNative *ZrClosureNativeNew(struct SZrState *state, TZrSize closureValueCount);

ZR_CORE_API SZrClosure *ZrClosureNew(struct SZrState *state, TZrSize closureValueCount);

ZR_CORE_API void ZrClosureInitValue(struct SZrState *state, SZrClosure *closure);

ZR_FORCE_INLINE TBool ZrClosureValueIsIndependent(struct SZrState *state, SZrClosureValue *closureValue) {
    return closureValue->value.valuePointer == ZR_CAST_STACK_OBJECT(&closureValue->link.independentValue);
}

#endif // ZR_VM_CORE_CLOSURE_H
