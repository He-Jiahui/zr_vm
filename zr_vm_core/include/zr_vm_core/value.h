//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_VALUE_H
#define ZR_VM_CORE_VALUE_H

#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrMeta;

typedef TInt64 (*FZrNativeFunction)(struct SZrState *state);


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

ZR_CORE_API void ZrValueBarrier(struct SZrState *state, SZrRawObject *object, SZrTypeValue *value);

ZR_CORE_API void ZrValueResetAsNull(SZrTypeValue *value);

ZR_CORE_API void ZrValueInitAsRawObject(struct SZrState *state, SZrTypeValue *value, SZrRawObject *object);

ZR_CORE_API void ZrValueInitAsUInt(struct SZrState *state, SZrTypeValue *value, TUInt64 intValue);

ZR_CORE_API void ZrValueInitAsInt(struct SZrState *state, SZrTypeValue *value, TInt64 intValue);

ZR_CORE_API void ZrValueInitAsFloat(struct SZrState *state, SZrTypeValue *value, TFloat64 floatValue);

ZR_CORE_API void ZrValueInitAsNativePointer(struct SZrState *state, SZrTypeValue *value, TZrPtr pointerValue);

ZR_CORE_API TBool ZrValueEqual(struct SZrState *state, SZrTypeValue *value1, SZrTypeValue *value2);


ZR_CORE_API SZrTypeValue *ZrValueGetStackOffsetValue(struct SZrState *state, TZrMemoryOffset offset);

#define ZR_VALUE_FAST_SET(VALUE, REGION, DATA, TYPE)                                                                   \
    {                                                                                                                  \
        (VALUE)->type = (TYPE);                                                                                        \
        (VALUE)->value.nativeObject.REGION = (DATA);                                                                   \
        (VALUE)->isGarbageCollectable = ZR_FALSE;                                                                      \
        (VALUE)->isNative = ZR_TRUE;                                                                                   \
    }

ZR_CORE_API void ZrValueCopy(struct SZrState *state, SZrTypeValue *destination, SZrTypeValue *source);

ZR_FORCE_INLINE EZrValueType ZrValueGetType(const SZrTypeValue *value) { return value->type; }

ZR_FORCE_INLINE SZrRawObject *ZrValueGetRawObject(const SZrTypeValue *value) { return value->value.object; }

ZR_FORCE_INLINE TBool ZrValueIsGarbageCollectable(const SZrTypeValue *value) { return value->isGarbageCollectable; }


ZR_FORCE_INLINE TBool ZrValueIsNative(const SZrTypeValue *value) { return value->isNative; }

ZR_FORCE_INLINE TBool ZrValueCanValueToString(struct SZrState *state, SZrTypeValue *value) {
    ZR_UNUSED_PARAMETER(state);
    EZrValueType type = value->type;
    return ZR_VALUE_IS_TYPE_NORMAL(type);
}
ZR_CORE_API TZrString *ZrValueConvertToString(struct SZrState *state, SZrTypeValue *value);

ZR_CORE_API struct SZrMeta *ZrValueGetMeta(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType);

#endif // ZR_VM_CORE_VALUE_H
