/**
 * @file
 * @brief 声明反射 FieldInfo 对原始 POD 字段的值编解码边界。
 */
#ifndef ZR_VM_CORE_REFLECTION_FIELD_VALUE_PRIMITIVE_H
#define ZR_VM_CORE_REFLECTION_FIELD_VALUE_PRIMITIVE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrTypeLayoutField;
struct SZrTypeValue;

/**
 * @brief 从通过布局与元数据核验的原始字段地址读取 primitive 值。
 * @pre 调用方保证 fieldAddress 覆盖 fieldLayout->byteSize；字段不能是 GC、ownership 或 value 槽。
 * @note 普通与嵌套字段读取都走此入口；窄整数提升为 VM 64 位整数，float32 提升为 double。
 * @return 参数、布局或 primitive 类型不匹配时返回 false。
 */
TZrBool ZrCore_ReflectionFieldValue_LoadPrimitive(
        struct SZrState *state,
        const struct SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        const TZrByte *fieldAddress,
        struct SZrTypeValue *result);

/**
 * @brief 将 VM primitive 值写回经布局核验的原始字段地址。
 * @pre 调用方保证 fieldAddress 可写且覆盖 fieldLayout->byteSize；不能用于 GC、ownership 或 value 槽。
 * @note 窄整数和 float32 只接受可无损表示的输入，拒绝时不写入字段。
 * @return 类型、宽度或可表示性不符时返回 false。
 */
TZrBool ZrCore_ReflectionFieldValue_StorePrimitive(
        const struct SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        TZrByte *fieldAddress,
        const struct SZrTypeValue *value);

#endif
