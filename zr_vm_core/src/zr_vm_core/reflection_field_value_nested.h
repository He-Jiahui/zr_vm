#ifndef ZR_VM_CORE_REFLECTION_FIELD_VALUE_NESTED_H
#define ZR_VM_CORE_REFLECTION_FIELD_VALUE_NESTED_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrMetadataRuntime;
struct SZrState;
struct SZrTypeLayout;
struct SZrTypeLayoutField;
struct SZrTypeValue;

/**
 * @brief 为已解析的 FieldInfo 内联布局提供嵌套 VALUE_SLOT 与 primitive 叶访问。
 *
 * 外层反射适配器负责解析 token/object、检查根字段范围并初始化输出值；本头文件中的 helper
 * 只按布局索引访问当次传入的存储。runtime、layout、field、storage、path 和 value 均由调用方
 * 借用到本次调用结束，本模块不会保存这些指针。
 * TODO: 终端及中间路径均可能选中 union 字段却不比较 activeTag；需确认非活动成员的读取
 * 与写入契约，以及写入时是否应拒绝或更新 tag，再对照 type_layout.c 的相关路径。
 * TODO: 公开 FieldInfo 尚未说明 inlineStorage 基址对齐；需核对存储来源并决定声明调用前提或校验地址。
 */

/**
 * @brief 从已选中的内联布局字段读取一个 VM VALUE_SLOT。
 * @pre field 属于 layout 的字段表；inlineStorage 覆盖 layout->byteSize；outValue 已初始化为可写 VM 值。
 * @return 成功时将槽值复制到 outValue；参数、范围、标记或槽大小不合要求时返回 false 且不改 outValue。
 */
TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutField(
        struct SZrState *state,
        const struct SZrTypeLayout *layout,
        const struct SZrTypeLayoutField *field,
        const TZrByte *inlineStorage,
        struct SZrTypeValue *outValue);

/**
 * @brief 将 VM 值写入已选中的内联布局 VALUE_SLOT。
 * @pre field 属于 layout 的字段表；inlineStorage 是按该布局初始化的可写存储，value 是有效 VM 值。
 * @return 成功时只替换所选槽并沿用 VM 值复制的 GC/ownership 处理；校验失败时返回 false 且不改存储。
 */
TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutField(
        struct SZrState *state,
        const struct SZrTypeLayout *layout,
        const struct SZrTypeLayoutField *field,
        TZrByte *inlineStorage,
        const struct SZrTypeValue *value);

/**
 * @brief 沿非空字段索引路径读取末端 VALUE_SLOT。
 * @pre 非空 runtime、layout、storage、path 和 outValue 在调用期间有效；存储覆盖根布局，outValue 已初始化。
 * @return 路径和末端槽有效时复制 VM 值；空路径或中间字段不兼容时返回 false 且不改 outValue。
 */
TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

/**
 * @brief 沿非空字段索引路径读取末端原始 primitive 字段。
 * @pre 非空 runtime、layout、storage、path 和 outValue 在调用期间有效；存储覆盖根布局，outValue 已初始化。
 * @return 成功时由共享 primitive codec 规范化 VM 结果；路径、字段或 primitiveValueType 不匹配时返回 false。
 */
TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPrimitivePath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        struct SZrTypeValue *outValue);

/**
 * @brief 沿非空字段索引路径替换末端 VALUE_SLOT。
 * @pre 非空 runtime、layout、storage、path 和 value 在调用期间有效；存储覆盖根布局且末端槽已初始化。
 * @return 成功时只替换末端槽并应用 VM 值复制的 GC/ownership 语义；空路径或中间字段不兼容时返回 false 且不改存储。
 */
TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

/**
 * @brief 沿非空字段索引路径写入末端原始 primitive 字段。
 * @pre 非空 runtime、layout、storage、path 和 value 在调用期间有效；存储覆盖根布局。
 * @return 成功时只更新末端字段；路径、字段、类型、范围或精度不匹配时返回 false 且保留原字段字节。
 */
TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPrimitivePath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        const struct SZrTypeValue *value);

#endif
