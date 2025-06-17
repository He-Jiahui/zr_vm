//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_VALUE_H
#define ZR_VM_CORE_VALUE_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/type.h"
struct SZrState;

typedef TUInt32 (*FZrNativeFunction)(struct SZrState *state);


union TZrPureValue {
    SGcObject *object;
    TUInt64 uint64;
    TInt64 int64;
    TDouble float64;
    TFloat float32;
    TBool bool;
    TZrPtr nativePointer;
    FZrNativeFunction nativeFunction;
};

typedef union TZrPureValue TZrPureValue;

struct SZrTypeValue {
    EZrValueType type;
    TZrPureValue value;
};

typedef struct SZrTypeValue SZrTypeValue;


#endif //ZR_VM_CORE_VALUE_H
