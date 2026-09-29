/**
 * @file reflection_field_value.c
 * @brief 以 FieldDef 的布局元数据访问调用方提供的内联字段存储。
 * @note Token API 接收显式 runtime/token；Object API 从 FieldInfo 身份字段恢复二者。结构视图是借用指针，写入按值槽或布局 copy 规则更新。
 * @note 聚合写入失败不承诺整块回滚；各入口分别说明其参数约束与失败输出。
 */

#include "zr_vm_core/reflection.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include "reflection_field_value_nested.h"
#include "reflection_field_value_primitive.h"

#include <string.h>

/** @brief 在 owner layout 中按解析出的字节偏移及可选类型布局索引找回真实字段描述。
 * @return 返回 runtime 布局表中的借用字段指针；NONE 索引仅放宽索引匹配，不放宽偏移匹配。
 */
static const SZrTypeLayoutField *reflection_field_value_find_field_layout_by_offset(
        const SZrTypeLayout *ownerTypeLayout,
        TZrUInt32 byteOffset,
        TZrUInt32 fieldTypeLayoutId) {
    TZrUInt32 index;

    if (ownerTypeLayout == ZR_NULL || ownerTypeLayout->fields == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < ownerTypeLayout->fieldCount; index++) {
        const SZrTypeLayoutField *field = ownerTypeLayout->fields + index;

        if (field->byteOffset == byteOffset &&
            (fieldTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
             field->typeLayoutIndex == fieldTypeLayoutId)) {
            return field;
        }
    }

    return ZR_NULL;
}

/** @brief 将 FieldDef token 绑定到所属聚合的字段描述，并确认整个字段落在调用方给出的 storage 范围内。
 * @pre runtime 和 token 对应的布局元数据在调用期间保持有效；结果指针仅借用该元数据。
 * @return 拒绝非 Field token、缺少布局或超出 storage 的字段；此处只做范围验证。
 */
static TZrBool reflection_field_value_resolve_field_layout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        TZrUInt32 inlineStorageByteSize,
        SZrReflectionResolvedToken *outResolved,
        const SZrTypeLayoutField **outFieldLayout) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (outFieldLayout != ZR_NULL) {
        *outFieldLayout = ZR_NULL;
    }

    if (runtime == ZR_NULL ||
        outFieldLayout == ZR_NULL ||
        !ZrCore_Reflection_ResolveToken(runtime, fieldToken, &resolved) ||
        resolved.kind != ZR_REFLECTION_RESOLVED_TOKEN_FIELD ||
        resolved.ownerTypeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: 范围验证没有检查 VALUE_SLOT 地址的天然对齐；TypeLayout_Validate 也接受 byteOffset=1。
     * 公开读写随后把 storage + byteOffset 转为 SZrTypeValue* 并交给 Value_Copy，错位字段会触发 C 未定义行为。 */
    fieldLayout = reflection_field_value_find_field_layout_by_offset(resolved.ownerTypeLayout,
                                                                     resolved.byteOffset,
                                                                     resolved.fieldTypeLayoutId);
    if (fieldLayout == ZR_NULL ||
        fieldLayout->byteOffset > inlineStorageByteSize ||
        fieldLayout->byteSize > inlineStorageByteSize - fieldLayout->byteOffset) {
        return ZR_FALSE;
    }

    if (outResolved != ZR_NULL) {
        *outResolved = resolved;
    }
    *outFieldLayout = fieldLayout;
    return ZR_TRUE;
}

/** @brief 读取 FieldDef 的 FIELD_SIG 根节点下的字段类型节点，供 POD 与内联聚合分派共用。
 * @return 缺少或损坏签名时失败，不推断缺省字段类型。
 */
static TZrBool reflection_field_value_read_field_type_node(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        SZrMetadataRuntimeSignatureTypeNodeView *outNodeView) {
    SZrMetadataRuntimeSignatureView signatureView;

    if (outNodeView != ZR_NULL) {
        memset(outNodeView, 0, sizeof(*outNodeView));
    }

    if (runtime == ZR_NULL ||
        outNodeView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadSignatureView(runtime, fieldToken, &signatureView) ||
        signatureView.rootNode != ZR_METADATA_SIGNATURE_NODE_FIELD_SIG ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                      signatureView.fieldTypeBlobOffset,
                                                      outNodeView)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief TYPE_DEF 与 TYPE_REF 是本适配层可解析为内联聚合布局的签名节点。
 * @return 节点为 TYPE_DEF/TYPE_REF 时返回 true，其它节点返回 false；此处不继续解析布局。
 */
static TZrBool reflection_field_value_signature_node_is_inline_layout(
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    return nodeView != ZR_NULL &&
           (nodeView->node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF ||
            nodeView->node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
}

/** @brief 判定一个字段能否只以借用视图暴露其 struct/union 字节区，而不把 VM 值槽误当聚合。
 * @pre resolved layout、FieldDef 布局字段与签名节点须来自同一 runtime 解析结果。
 * @return 签名、字段尺寸一致且无 VM/GC/ownership 标记时，返回 struct/union 借用视图；其它情况返回 false。
 */
static TZrBool reflection_field_value_can_read_inline_borrowed_view(
        const SZrReflectionResolvedToken *resolved,
        const SZrTypeLayoutField *fieldLayout,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    TZrUInt32 unsupportedFieldFlags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;

    if (resolved == ZR_NULL ||
        fieldLayout == ZR_NULL ||
        resolved->fieldTypeLayout == ZR_NULL ||
        !reflection_field_value_signature_node_is_inline_layout(nodeView) ||
        (fieldLayout->flags & unsupportedFieldFlags) != 0u ||
        fieldLayout->byteSize == 0u ||
        fieldLayout->byteSize != resolved->fieldTypeLayout->byteSize) {
        return ZR_FALSE;
    }

    return (TZrBool)(resolved->fieldTypeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT ||
                     resolved->fieldTypeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION);
}

/** @brief 限定聚合写入源为非空 NativePointer，并要求目标布局支持原始或字段级 copy。
 * @return 仅非空 NativePointer 且布局支持原始或字段级 copy 时成功；不取得源所有权或验证缓冲区长度。
 */
static TZrBool reflection_field_value_can_write_inline_borrowed_source(
        const SZrReflectionResolvedToken *resolved,
        const SZrTypeLayoutField *fieldLayout,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView,
        const SZrTypeValue *value) {
    if (value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        value->value.nativeObject.nativePointer == ZR_NULL ||
        !reflection_field_value_can_read_inline_borrowed_view(resolved, fieldLayout, nodeView)) {
        return ZR_FALSE;
    }

    if (ZrCore_TypeLayout_CanRawCopy(resolved->fieldTypeLayout)) {
        return ZR_TRUE;
    }

    return (TZrBool)(resolved->fieldTypeLayout->copyKind == (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY);
}

/** @brief 在 GC native-call pin 保护下取 FieldInfo 的内部身份属性，并浅拷贝属性值到调用方临时值。
 * @return pin 期间成功读取并浅拷贝属性时返回 true，缺失或读取失败返回 false；当前仅用于两个身份属性。
 */
static TZrBool reflection_field_value_read_field_info_object_field(
        SZrState *state,
        SZrObject *fieldInfo,
        const TZrChar *fieldName,
        SZrTypeValue *outValue) {
    SZrGcNativeCallPin fieldInfoPin = {0};
    SZrGcNativeCallPin fieldNamePin = {0};
    SZrString *fieldString;
    SZrTypeValue key;
    const SZrTypeValue *storedValue;
    TZrBool result = ZR_FALSE;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || fieldInfo == ZR_NULL || fieldName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldInfo), &fieldInfoPin)) {
        return ZR_FALSE;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;

    if (!ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &fieldNamePin)) {
        ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
        return ZR_FALSE;
    }

    storedValue = ZrCore_Object_GetValue(state, fieldInfo, &key);
    if (storedValue != ZR_NULL) {
        *outValue = *storedValue;
        result = ZR_TRUE;
    }

    ZrCore_Gc_NativeCallUnpin(state->global, &fieldNamePin);
    ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
    return result;
}

/**
 * @brief 从 FieldInfo 内部属性恢复 runtime 指针与 FieldDef token，供 Object 版读写包装器立即转入 Token API。
 * @pre FieldInfo 对象地址以及其中 runtime 所指对象须在本次调用期间有效；恢复出的 runtime 只借用，不会被保留。
 * @return 缺少身份字段、字段类型不符或 token 超出 32 位范围时失败并清空输出。
 * TODO: BuildFieldInfoTokenObject 将 runtime 以裸 native pointer 存入对象；确认模块卸载与 FieldInfo 可达期的约束。
 * 下一步核查 module teardown 与 FieldInfo 消费入口的并存生命周期。
 */
static TZrBool reflection_field_value_read_field_info_identity(
        SZrState *state,
        SZrObject *fieldInfo,
        SZrMetadataRuntime **outRuntime,
        TZrMetadataToken *outFieldToken) {
    SZrTypeValue runtimeValue;
    SZrTypeValue tokenValue;

    if (outRuntime != ZR_NULL) {
        *outRuntime = ZR_NULL;
    }
    if (outFieldToken != ZR_NULL) {
        *outFieldToken = 0u;
    }

    if (state == ZR_NULL || fieldInfo == ZR_NULL || outRuntime == ZR_NULL || outFieldToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_object_field(state, fieldInfo, "metadataRuntime", &runtimeValue) ||
        runtimeValue.type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue.value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_object_field(state, fieldInfo, "metadataToken", &tokenValue) ||
        !ZR_VALUE_IS_TYPE_INT(tokenValue.type)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(tokenValue.type)) {
        if (tokenValue.value.nativeObject.nativeUInt64 > (TZrUInt64)UINT32_MAX) {
            return ZR_FALSE;
        }
        *outFieldToken = (TZrMetadataToken)tokenValue.value.nativeObject.nativeUInt64;
    } else {
        if (tokenValue.value.nativeObject.nativeInt64 < 0 ||
            tokenValue.value.nativeObject.nativeInt64 > (TZrInt64)UINT32_MAX) {
            return ZR_FALSE;
        }
        *outFieldToken = (TZrMetadataToken)tokenValue.value.nativeObject.nativeInt64;
    }

    *outRuntime = (SZrMetadataRuntime *)runtimeValue.value.nativeObject.nativePointer;
    return ZR_TRUE;
}

/**
 * @brief 按 FieldDef token 读取顶层字段，统一返回 VM 值或可借用的内联 struct/union 地址。
 * @pre runtime、inlineStorage 及其指向的 storage 在调用期间有效；若目标为 VALUE_SLOT，该槽已初始化且地址满足 SZrTypeValue 对齐。
 * @return token/layout 无法解析、字段超出给定长度或表示不受支持时失败并将 outValue 置 null；成功时分别复制 VM 值、解码 primitive POD，或返回借用 NativePointer。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;
    EZrValueType primitiveValueType;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    if ((fieldLayout->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
        if (fieldLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
        /* 源槽须已初始化；Value_Copy 保留 VM 值的 GC 与 ownership 复制语义。 */
        ZrCore_Value_Copy(state, outValue, (const SZrTypeValue *)fieldAddress);
        return ZR_TRUE;
    }

    if (!reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    if (fieldTypeNode.node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (fieldTypeNode.payload0 >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX) {
            return ZR_FALSE;
        }
        primitiveValueType = (EZrValueType)fieldTypeNode.payload0;
        return ZrCore_ReflectionFieldValue_LoadPrimitive(state,
                                                         fieldLayout,
                                                         primitiveValueType,
                                                         fieldAddress,
                                                         outValue);
    }

    if (!reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsNativePointer(state, outValue, (TZrPtr)fieldAddress);
    return ZR_TRUE;
}

/** @brief FieldInfo 对象形式的顶层读取适配器；先取其 runtime/token 身份，再复用 Token 版字段和存储校验。
 * @pre FieldInfo 对象、其中 runtime 所指对象及 backing storage 在读取期间仍有效；若目标为 VALUE_SLOT，该槽已初始化且地址满足 SZrTypeValue 对齐。
 * @return 身份或字段读取失败时返回 false 并将 outValue 置 null。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                     runtime,
                                                     fieldToken,
                                                     inlineStorage,
                                                     inlineStorageByteSize,
                                                     outValue);
}

/** @brief 读取内联聚合中由索引选中的一层子字段，路径遍历与 VM 值所有权处理委托给 nested helper。
 * @pre runtime、storage 和 backing bytes 在调用期间有效；命中的 VALUE_SLOT 已初始化且地址满足 SZrTypeValue 对齐。
 * @return 外层不是可借用 struct/union、布局无子字段、索引越界或子字段不支持时失败并将 outValue 置 null；成功时按 VM 值复制。
 * TODO: 单层读取未核对 union activeTag；下一步核查 tag 存储及非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    const SZrTypeLayoutField *nestedField;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode) ||
        resolved.fieldTypeLayout->fields == ZR_NULL ||
        nestedFieldIndex >= resolved.fieldTypeLayout->fieldCount) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    nestedField = resolved.fieldTypeLayout->fields + nestedFieldIndex;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutField(state,
                                                             resolved.fieldTypeLayout,
                                                             nestedField,
                                                             fieldAddress,
                                                             outValue);
}

/** @brief 单层嵌套读取的 FieldInfo 适配器；身份提取成功后转发给 token 版，不持有结果视图。
 * @pre FieldInfo 对象、其 runtime 指针目标与 backing storage 在调用期间有效；若命中的子字段为 VALUE_SLOT，该槽已初始化且地址满足 SZrTypeValue 对齐。
 * @return 身份或字段读取失败时返回 false 并将 outValue 置 null。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedValue(state,
                                                           runtime,
                                                           fieldToken,
                                                           inlineStorage,
                                                           inlineStorageByteSize,
                                                           nestedFieldIndex,
                                                           outValue);
}

/** @brief 沿索引路径读取内联聚合中的终端字段，保留普通 nested helper 对终端 VALUE_SLOT 的 VM copy 语义。
 * @pre runtime、storage 和路径数组在调用期间有效；命中的 VALUE_SLOT 已初始化且地址满足 SZrTypeValue 对齐。
 * @return 空路径、中间布局无法解析或终端不支持时失败并将 outValue 置 null；有效终端值通过 VM copy 返回。
 * TODO: 中间及终端 union 字段未核对 activeTag；核查多段与单段路径的非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutPath(state,
                                                            runtime,
                                                            resolved.fieldTypeLayout,
                                                            fieldAddress,
                                                            nestedFieldIndices,
                                                            nestedFieldIndexCount,
                                                            outValue);
}

/** @brief 嵌套路径读取的 FieldInfo 适配器；身份和路径约束最终由 token 版与 nested helper 共同执行。
 * @pre FieldInfo 对象、其 runtime 指针目标、路径数组和 backing storage 在调用期间有效；若终端为 VALUE_SLOT，该槽已初始化且地址满足 SZrTypeValue 对齐。
 * @return 身份或路径读取失败时返回 false 并将 outValue 置 null；成功结果是按 VM copy 规则取得的独立值。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(state,
                                                               runtime,
                                                               fieldToken,
                                                               inlineStorage,
                                                               inlineStorageByteSize,
                                                               nestedFieldIndices,
                                                               nestedFieldIndexCount,
                                                               outValue);
}

/** @brief 沿路径读取原始 primitive POD 叶子，并要求调用方给出的 ValueType 与终端布局一致。
 * @pre runtime、路径数组与 backing storage 在调用期间有效。
 * @return 空/无效路径、不可借用的外层布局、primitive 类型范围错误或终端表示不匹配时失败并将 outValue 置 null；成功时返回独立 VM 值。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查非活动成员的读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutPrimitivePath(
            state,
            runtime,
            resolved.fieldTypeLayout,
            fieldAddress,
            nestedFieldIndices,
            nestedFieldIndexCount,
            (EZrValueType)primitiveValueType,
            outValue);
}

/** @brief primitive 嵌套路径读取的 FieldInfo 适配器，恢复身份后转发到 token 版。
 * @pre FieldInfo 对象、其 runtime 指针目标、路径数组与 backing storage 在调用期间有效。
 * @return 身份或叶子解码失败时返回 false 并将 outValue 置 null。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
            state,
            runtime,
            fieldToken,
            inlineStorage,
            inlineStorageByteSize,
            nestedFieldIndices,
            nestedFieldIndexCount,
            primitiveValueType,
            outValue);
}

/** @brief 将 VM 值写到内联聚合路径的终端值槽；字段级替换的 copy/drop 交给 nested helper。
 * @pre runtime、可写 backing storage、路径数组和输入 value 在调用期间有效；目标 VALUE_SLOT 已初始化且满足对齐。
 * @return 空路径、不可解析字段或不兼容终端返回 false；多字段 copy 失败不保证先前复制的聚合前缀回滚。
 * TODO: 中间及终端 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的规则。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutPath(state,
                                                             runtime,
                                                             resolved.fieldTypeLayout,
                                                             fieldAddress,
                                                             nestedFieldIndices,
                                                             nestedFieldIndexCount,
                                                             value);
}

/** @brief 嵌套路径写入的 FieldInfo 适配器；从对象恢复身份后委托 token 版执行目标定位与值替换。
 * @pre FieldInfo 对象、其 runtime 指针目标、可写 backing storage、路径数组和输入 value 在调用期间有效；若终端为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 身份、路径或目标字段不兼容时返回 false；布局 copy 失败不承诺整块回滚。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(state,
                                                                runtime,
                                                                fieldToken,
                                                                inlineStorage,
                                                                inlineStorageByteSize,
                                                                nestedFieldIndices,
                                                                nestedFieldIndexCount,
                                                                value);
}

/** @brief 将 VM 值按显式 primitive 类型编码到嵌套路径的原始 POD 叶子。
 * @pre runtime、可写 backing storage、路径数组和输入 value 在调用期间有效。
 * @return 路径为空、primitive 类型无效、布局不支持或值类型/尺寸/范围不匹配时返回 false。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的规则。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutPrimitivePath(
            state,
            runtime,
            resolved.fieldTypeLayout,
            fieldAddress,
            nestedFieldIndices,
            nestedFieldIndexCount,
            (EZrValueType)primitiveValueType,
            value);
}

/** @brief primitive 嵌套路径写入的 FieldInfo 适配器，身份读取成功后转发到 token 版。
 * @pre FieldInfo 对象、其 runtime 指针目标、目标 storage、路径数组和输入 value 在调用期间有效。
 * @return 身份、路径或 primitive 编码失败时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
            state,
            runtime,
            fieldToken,
            inlineStorage,
            inlineStorageByteSize,
            nestedFieldIndices,
            nestedFieldIndexCount,
            primitiveValueType,
            value);
}

/** @brief 将 VM 值替换到内联聚合中索引选中的单层子字段，VM copy 负责旧值 ownership/drop。
 * @pre runtime、可写 backing storage 和输入 value 在调用期间有效；目标 VALUE_SLOT 已初始化且满足对齐。
 * @return 外层布局或子索引不匹配、目标不是值槽或尺寸不足时返回 false。
 * TODO: 单层 union 子字段写入未核对 activeTag；核查写入时拒绝或更新 tag 的契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    const SZrTypeLayoutField *nestedField;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode) ||
        resolved.fieldTypeLayout->fields == ZR_NULL ||
        nestedFieldIndex >= resolved.fieldTypeLayout->fieldCount) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    nestedField = resolved.fieldTypeLayout->fields + nestedFieldIndex;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutField(state,
                                                              resolved.fieldTypeLayout,
                                                              nestedField,
                                                              fieldAddress,
                                                              value);
}

/** @brief 单层嵌套写入的 FieldInfo 适配器；从对象恢复身份后复用 token 版更新子字段。
 * @pre FieldInfo 对象、其 runtime 指针目标、可写 backing storage 与输入 value 在调用期间有效；若目标子字段为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 身份、索引或目标子字段不兼容时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedValue(state,
                                                            runtime,
                                                            fieldToken,
                                                            inlineStorage,
                                                            inlineStorageByteSize,
                                                            nestedFieldIndex,
                                                            value);
}

/** @brief FieldInfo 对象形式的顶层写入适配器；成功取回身份后转发给 token 版。
 * @pre FieldInfo 对象、其 runtime 指针目标、可写 storage 与输入 value 在调用期间有效；若目标为 VALUE_SLOT，则已初始化且满足对齐。
 * @return 身份、字段范围或目标表示与输入不兼容时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                      runtime,
                                                      fieldToken,
                                                      inlineStorage,
                                                      inlineStorageByteSize,
                                                      value);
}

/**
 * @brief 按 FieldDef token 写入顶层字段，区分 VM 值槽、原始 primitive POD 与可复制的内联聚合。
 * @pre runtime、可写 inlineStorage 和聚合 NativePointer 源在调用期间有效；VALUE_SLOT 目标已初始化且满足 SZrTypeValue 对齐，聚合源实际覆盖目标布局。
 * @return token/layout 无法解析或字段超出给定长度时返回 false；VM 值槽遵循 Value_Copy 的替换/ownership 规则。
 * primitive 写入拒绝类型或范围不匹配；布局 copy 失败不承诺整块回滚。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;
    EZrValueType primitiveValueType;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    if ((fieldLayout->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
        if (fieldLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
        /* 目标槽须已初始化；Value_Copy 在替换前按 VM 规则释放或转移旧值。 */
        ZrCore_Value_Copy(state, (SZrTypeValue *)fieldAddress, value);
        return ZR_TRUE;
    }

    if (!reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    if (fieldTypeNode.node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (fieldTypeNode.payload0 >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX) {
            return ZR_FALSE;
        }
        primitiveValueType = (EZrValueType)fieldTypeNode.payload0;
        return ZrCore_ReflectionFieldValue_StorePrimitive(fieldLayout,
                                                          primitiveValueType,
                                                          fieldAddress,
                                                          value);
    }

    if (!reflection_field_value_can_write_inline_borrowed_source(&resolved, fieldLayout, &fieldTypeNode, value)) {
        return ZR_FALSE;
    }
    /* TODO: 聚合源是调用方借用的 NativePointer；布局级失败可能发生在字段级 copy 之后。
     * 下一步核查 FieldInfo 写入是否承诺原子失败，再决定是否需要复制前预检或回滚。 */
    return ZrCore_TypeLayout_CopyInline(state,
                                        resolved.fieldTypeLayout,
                                        fieldAddress,
                                        value->value.nativeObject.nativePointer);
}
