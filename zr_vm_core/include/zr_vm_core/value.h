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
    TBool isGarbageCollectable;
    TBool isNative;
};

typedef struct SZrTypeValue SZrTypeValue;

ZR_CORE_API void ZrValueResetAsNull(SZrTypeValue *value);

ZR_CORE_API void ZrValueInitAsRawObject(struct SZrState *state, SZrTypeValue *value, SZrRawObject *object);


ZR_CORE_API SZrTypeValue *ZrValueGetStackOffsetValue(struct SZrState *state, TZrMemoryOffset offset);

ZR_FORCE_INLINE EZrValueType ZrValueGetType(const SZrTypeValue *value) {
    return value->type;
}

ZR_FORCE_INLINE TBool ZrValueIsNative(const SZrTypeValue *value) {
    return value->isNative;
}

#endif //ZR_VM_CORE_VALUE_H
