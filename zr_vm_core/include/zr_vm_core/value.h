//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_VALUE_H
#define ZR_VM_CORE_VALUE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/raw_object.h"

struct SZrState;
struct SZrMeta;
struct SZrString;
typedef TZrInt64 (*FZrNativeFunction)(struct SZrState *state);
struct SZrOwnershipControl;
struct SZrOwnershipWeakRef;


union TZrPureValue {
    SZrRawObject *object;
    TZrNativeObject nativeObject;
    FZrNativeFunction nativeFunction;
};

typedef union TZrPureValue TZrPureValue;

typedef enum EZrOwnershipValueKind {
    ZR_OWNERSHIP_VALUE_KIND_NONE = 0,
    ZR_OWNERSHIP_VALUE_KIND_UNIQUE,
    ZR_OWNERSHIP_VALUE_KIND_SHARED,
    ZR_OWNERSHIP_VALUE_KIND_WEAK,
    ZR_OWNERSHIP_VALUE_KIND_BORROWED,
    ZR_OWNERSHIP_VALUE_KIND_LOANED,
    ZR_OWNERSHIP_VALUE_KIND_USING,
} EZrOwnershipValueKind;

struct ZR_STRUCT_ALIGN SZrTypeValue {
    EZrValueType type;
    TZrPureValue value;
    TZrBool isGarbageCollectable;
    TZrBool isNative;
    EZrOwnershipValueKind ownershipKind;
    struct SZrOwnershipControl *ownershipControl;
    struct SZrOwnershipWeakRef *ownershipWeakRef;
};

typedef struct SZrTypeValue SZrTypeValue;


ZR_CORE_API void ZrCore_Value_Barrier(struct SZrState *state, SZrRawObject *object, SZrTypeValue *value);

ZR_CORE_API void ZrCore_Value_ResetAsNull(SZrTypeValue *value);

ZR_CORE_API void ZrCore_Value_InitAsRawObject(struct SZrState *state, SZrTypeValue *value, SZrRawObject *object);

ZR_CORE_API void ZrCore_Value_InitAsUInt(struct SZrState *state, SZrTypeValue *value, TZrUInt64 intValue);

ZR_CORE_API void ZrCore_Value_InitAsInt(struct SZrState *state, SZrTypeValue *value, TZrInt64 intValue);

ZR_CORE_API void ZrCore_Value_InitAsBool(struct SZrState *state, SZrTypeValue *value, TZrBool boolValue);

ZR_CORE_API void ZrCore_Value_InitAsFloat(struct SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue);

ZR_CORE_API void ZrCore_Value_InitAsNativePointer(struct SZrState *state, SZrTypeValue *value, TZrPtr pointerValue);

ZR_CORE_API TZrBool ZrCore_Value_Equal(struct SZrState *state, SZrTypeValue *value1, SZrTypeValue *value2);


ZR_CORE_API SZrTypeValue *ZrCore_Value_GetStackOffsetValue(struct SZrState *state, TZrMemoryOffset offset);

#define ZR_VALUE_FAST_SET(VALUE, REGION, DATA, TYPE)                                                                   \
    {                                                                                                                  \
        (VALUE)->type = (TYPE);                                                                                        \
        (VALUE)->value.nativeObject.REGION = (DATA);                                                                   \
        (VALUE)->isGarbageCollectable = ZR_FALSE;                                                                      \
        (VALUE)->isNative = ZR_TRUE;                                                                                   \
        (VALUE)->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;                                                         \
        (VALUE)->ownershipControl = ZR_NULL;                                                                           \
        (VALUE)->ownershipWeakRef = ZR_NULL;                                                                           \
    }

ZR_CORE_API void ZrCore_Value_Copy(struct SZrState *state, SZrTypeValue *destination, const SZrTypeValue *source);

ZR_CORE_API TZrUInt64 ZrCore_Value_GetHash(struct SZrState *state, const SZrTypeValue *value);

// directly compare without calling meta function, for hash set to find the same value
ZR_CORE_API TZrBool ZrCore_Value_CompareDirectly(struct SZrState *state, const SZrTypeValue *value1,
                                         const SZrTypeValue *value2);

ZR_FORCE_INLINE EZrValueType ZrCore_Value_GetType(const SZrTypeValue *value) { return value->type; }

ZR_FORCE_INLINE SZrRawObject *ZrCore_Value_GetRawObject(const SZrTypeValue *value) { return value->value.object; }

ZR_FORCE_INLINE TZrBool ZrCore_Value_IsGarbageCollectable(const SZrTypeValue *value) { return value->isGarbageCollectable; }


ZR_FORCE_INLINE TZrBool ZrCore_Value_IsNative(const SZrTypeValue *value) { return value->isNative; }

ZR_FORCE_INLINE TZrBool ZrCore_Value_CanValueToString(struct SZrState *state, SZrTypeValue *value) {
    ZR_UNUSED_PARAMETER(state);
    EZrValueType type = value->type;
    return ZR_VALUE_IS_TYPE_NORMAL(type);
}
ZR_CORE_API struct SZrString *ZrCore_Value_ConvertToString(struct SZrState *state, SZrTypeValue *value);

ZR_CORE_API struct SZrMeta *ZrCore_Value_GetMeta(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType);

// 调用指定值的元方法并返回结果
ZR_CORE_API TZrBool ZrCore_Value_CallMetaMethod(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType,
                                         SZrTypeValue *result, TZrSize argumentCount, ...);

// 专门用于调用 TO_STRING 元方法的便捷函数
ZR_CORE_API struct SZrString *ZrCore_Value_CallMetaToString(struct SZrState *state, SZrTypeValue *value);

// 将值转换为调试字符串，包含详细信息（用于测试和调试）
// 对于对象：打印类型名称和第一层字段（最多10个）
// 对于数组：打印第一层元素（最多10个）
// 格式：对象: <object type=${className or object}>{a=1,b=2,... | count = 12}
//       数组: [1,2,3,... | count = 12]
ZR_CORE_API struct SZrString *ZrCore_Value_ToDebugString(struct SZrState *state, SZrTypeValue *value);

#endif // ZR_VM_CORE_VALUE_H
