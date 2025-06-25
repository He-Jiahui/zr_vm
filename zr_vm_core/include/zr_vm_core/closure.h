//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_CLOSURE_H
#define ZR_VM_CORE_CLOSURE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
struct SZrClosureValue;

union TZrClosureLink {
    struct {
        struct SZrClosureValue *next;
        struct SZrClosureValue **previous;
    };

    SZrTypeValue releasedValue;
};

struct ZR_STRUCT_ALIGN SZrClosureValue {
    SZrRawObject super;
    TZrStackPointer value;
    union TZrClosureLink link;
};

typedef struct SZrClosureValue SZrClosureValue;


struct ZR_STRUCT_ALIGN SZrClosureNative {
    SZrRawObject super;
    SZrRawObject *gcList;
    FZrNativeFunction nativeFunction;
    TZrSize closureValueCount;
    SZrTypeValue closureValuesExtend[1];
};

typedef struct SZrClosureNative SZrClosureNative;

struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super;
    SZrRawObject *gcList;
    // todo: closure info
    TZrSize closureValueCount;
    SZrTypeValue closureValuesExtend[1];
};

typedef struct SZrClosure SZrClosure;

union TZrClosure {
    SZrClosureNative nativeClosure;
    SZrClosure zrClosure;
};

typedef union TZrClosure TZrClosure;
#endif //ZR_VM_CORE_CLOSURE_H
