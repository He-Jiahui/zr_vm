//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_VALUE_H
#define ZR_VM_CORE_VALUE_H

#include "zr_vm_core/conf.h"
struct SZrState;

typedef TUInt32 (*FZrNativeFunction)(struct SZrState *state);


union TZrPureValue {
    SZrRawObject *object;
    TZrNativeObject nativeObject;
    FZrNativeFunction nativeFunction;
};

typedef union TZrPureValue TZrPureValue;

struct ZR_STRUCT_ALIGN SZrTypeValue {
    EZrValueType type;
    TZrPureValue value;
};

typedef struct SZrTypeValue SZrTypeValue;

ZR_CORE_API void ZrValueInitAsNull(SZrTypeValue *value);


#endif //ZR_VM_CORE_VALUE_H
