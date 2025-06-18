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
    TZrStackObject value;
    union TZrClosureLink link;
};

typedef struct SZrClosureValue SZrClosureValue;

struct ZR_STRUCT_ALIGN SZrClosureInfo {
    TUInt32 closureValueListLength;
    SZrRawObject *closureValueList;
};

struct ZR_STRUCT_ALIGN SZrClosureNative {
    SZrRawObject super;
    struct SZrClosureInfo closureInfo;
    FZrNativeFunction nativeFunction;
    SZrTypeValue closureValuesExtend[1];
};

typedef struct SZrClosureNative SZrClosureNative;

struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super;
    struct SZrClosureInfo closureInfo;
    SZrTypeValue closureValuesExtend[1];
};

typedef struct SZrClosure SZrClosure;

union TZrClosure {
    SZrClosureNative native;
    SZrClosure zrClosure;
};

typedef union TZrClosure TZrClosure;
#endif //ZR_VM_CORE_CLOSURE_H
