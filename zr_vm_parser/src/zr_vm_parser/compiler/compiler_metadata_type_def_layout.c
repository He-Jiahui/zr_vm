#include "compiler_metadata_type_def_layout.h"
#include "compiler_metadata_signature.h"

#include "zr_vm_core/type_layout.h"

/* Stable metadata ABI v2 value-slot width; independent of host pointer width and struct padding. */
#define ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE ((TZrUInt32)40u)
/* Scalar fallback alignment is a schema rule, not a query of host struct layout. */
#define ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN ((TZrUInt32)8u)

/* Shared registry lookup supplies alignment only for a previously known concrete type. */
SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName);

/** @brief 选择能编码全部变体的最小规范 tag 宽度，参与跨平台布局身份。
 * @note tag ordinal 宽度随有效 variant 数选取，身份由 schema 规定而不随平台变化。 */
static TZrUInt32 metadata_type_def_select_union_tag_size(TZrUInt32 variantCount) {
    if (variantCount <= 0xffu) {
        return 1u;
    }
    if (variantCount <= 0xffffu) {
        return 2u;
    }
    return 4u;
}

/** @brief 统一识别字段所有权限定符，供 GC/drop 布局标记使用。
 * @note 统一 Ownership<T> 和字段 qualifier 的读法；调用方据此选择值槽及 drop 计数。 */
static TZrUInt32 metadata_type_def_payload_field_ownership_qualifier(const SZrType *typeInfo) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (typeInfo == ZR_NULL) {
        return (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeInfo, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipInnerType);
        return (TZrUInt32)ownershipQualifier;
    }

    return (TZrUInt32)typeInfo->ownershipQualifier;
}

/** @brief 判断类型名是否为当前 union 泛型参数，避免将未知实例尺寸视作标量尺寸。
 * @note 只识别当前 union 声明的形式参数，不把同名外部类型视作开放参数。 */
static TZrBool metadata_type_def_generic_parameter_name_matches(SZrGenericDeclaration *generic,
                                                                SZrString *typeName) {
    if (generic == ZR_NULL || generic->params == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < generic->params->count; index++) {
        SZrAstNode *paramNode = generic->params->nodes[index];
        if (paramNode != ZR_NULL &&
            paramNode->type == ZR_AST_PARAMETER &&
            paramNode->data.parameter.name != ZR_NULL &&
            paramNode->data.parameter.name->name != ZR_NULL &&
            ZrCore_String_Equal(paramNode->data.parameter.name->name, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** @brief 递归识别复合类型中的泛型依赖，决定 payload 是否使用统一值槽。
 * @note 递归检查 ownership/subType、泛型实参和 tuple 元素；不实例化或改写类型树。 */
static TZrBool metadata_type_def_type_references_generic_parameter(SZrGenericDeclaration *generic,
                                                                   const SZrType *typeInfo) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (generic == ZR_NULL || generic->params == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeInfo, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipQualifier);
        return metadata_type_def_type_references_generic_parameter(generic, ownershipInnerType);
    }

    if (typeInfo->subType != ZR_NULL &&
        metadata_type_def_type_references_generic_parameter(generic, typeInfo->subType)) {
        return ZR_TRUE;
    }

    if (typeInfo->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return metadata_type_def_generic_parameter_name_matches(generic,
                                                               typeInfo->name->data.identifier.name);
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeInfo->name->data.genericType;
        if (genericType->name != ZR_NULL &&
            metadata_type_def_generic_parameter_name_matches(generic, genericType->name->name)) {
            return ZR_TRUE;
        }
        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *argumentNode = genericType->params->nodes[index];
                if (argumentNode != ZR_NULL &&
                    argumentNode->type == ZR_AST_TYPE &&
                    metadata_type_def_type_references_generic_parameter(generic, &argumentNode->data.type)) {
                    return ZR_TRUE;
                }
            }
        }
    }

    if (typeInfo->name->type == ZR_AST_TUPLE_TYPE &&
        typeInfo->name->data.tupleType.elements != ZR_NULL) {
        SZrAstNodeArray *elements = typeInfo->name->data.tupleType.elements;
        for (TZrSize index = 0; index < elements->count; index++) {
            SZrAstNode *elementNode = elements->nodes[index];
            if (elementNode != ZR_NULL &&
                elementNode->type == ZR_AST_TYPE &&
                metadata_type_def_type_references_generic_parameter(generic, &elementNode->data.type)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

/** @brief 将拥有语义或依赖泛型的 payload 映射为统一引用槽，隔离实例化差异。
 * @note ownership 与开放泛型参数采用稳定 value-slot；普通已知值类型保留物理宽度。 */
static TZrBool metadata_type_def_payload_uses_value_slot(const SZrAstNode *unionDeclaration,
                                                         const SZrType *typeInfo,
                                                         TZrUInt32 ownershipQualifier) {
    SZrGenericDeclaration *generic = ZR_NULL;

    if (unionDeclaration != ZR_NULL && unionDeclaration->type == ZR_AST_UNION_DECLARATION) {
        generic = unionDeclaration->data.unionDeclaration.generic;
    }

    return (TZrBool)(ownershipQualifier != (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE ||
                     metadata_type_def_type_references_generic_parameter(generic, typeInfo));
}

/** @brief 原型缺少对齐信息时选择 metadata schema 规定的标量对齐回退值。
 * @note schema 固定 1/2/4/8 字节回退，避免摘要依赖宿主自然对齐。 */
static TZrUInt32 metadata_type_def_canonical_align_for_size(TZrUInt32 size) {
    if (size <= 1u) {
        return 1u;
    }
    if (size <= 2u) {
        return 2u;
    }
    if (size <= 4u) {
        return 4u;
    }
    return ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;
}

/** @brief 读取已知类型原型的字节对齐；未找到时由调用者使用规范回退值。
 * @note 只消费已注册原型的具体对齐；查不到时由统一 schema 回退规则处理。 */
static TZrBool metadata_type_def_try_get_prototype_align(SZrCompilerState *cs,
                                                         const SZrType *typeInfo,
                                                         TZrUInt32 *outAlign) {
    SZrString *typeName;
    SZrTypePrototypeInfo *prototype;

    if (outAlign != ZR_NULL) {
        *outAlign = 0;
    }
    if (cs == ZR_NULL ||
        typeInfo == ZR_NULL ||
        typeInfo->name == ZR_NULL ||
        typeInfo->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        outAlign == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = typeInfo->name->data.identifier.name;
    prototype = typeName != ZR_NULL ? find_compiler_type_prototype(cs, typeName) : ZR_NULL;
    if (prototype == ZR_NULL || prototype->layoutByteAlign == 0) {
        return ZR_FALSE;
    }

    *outAlign = prototype->layoutByteAlign;
    return ZR_TRUE;
}

/** @brief 将语义字段投影为 ABI union payload 大小与对齐，供变体布局累计。
 * @note 未知大小采用固定槽回退；返回尺寸供外层按 variant 累积最大 payload 布局。 */
static void metadata_type_def_select_payload_field_layout(SZrCompilerState *cs,
                                                          const SZrAstNode *unionDeclaration,
                                                          const SZrType *typeInfo,
                                                          TZrUInt32 ownershipQualifier,
                                                          TZrUInt32 *outSize,
                                                          TZrUInt32 *outAlign) {
    TZrUInt32 fieldSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
    TZrUInt32 fieldAlign = ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;

    if (metadata_type_def_payload_uses_value_slot(unionDeclaration, typeInfo, ownershipQualifier)) {
        fieldSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
        fieldAlign = ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;
    } else if (typeInfo != ZR_NULL) {
        TZrUInt32 prototypeAlign = 0;

        fieldSize = calculate_type_size(cs, (SZrType *)typeInfo);
        if (fieldSize == 0) {
            fieldSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
            fieldAlign = ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;
        } else if (metadata_type_def_try_get_prototype_align(cs, typeInfo, &prototypeAlign)) {
            fieldAlign = prototypeAlign;
        } else {
            fieldAlign = metadata_type_def_canonical_align_for_size(fieldSize);
        }
    }

    if (outSize != ZR_NULL) {
        *outSize = fieldSize;
    }
    if (outAlign != ZR_NULL) {
        *outAlign = fieldAlign;
    }
}

/** @brief 统计有效 AST 变体并确保数量可由 metadata token RID 表示。
 * @note 忽略恢复 AST 中非 variant 节点；有效计数同时驱动 tag 选择和 activeTag 校验。 */
static TZrBool metadata_type_def_union_variant_count(const SZrAstNode *unionDeclaration,
                                                     TZrUInt32 *outCount) {
    TZrUInt32 count = 0;

    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION ||
        outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize index = 0; index < unionDeclaration->data.unionDeclaration.variants->count; index++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[index];

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }
            if (count >= ZR_METADATA_TOKEN_RID_MASK) {
                return ZR_FALSE;
            }
            count++;
        }
    }

    *outCount = count;
    return ZR_TRUE;
}

/** @brief 在生成身份前验证布局满足 core union、tag 及 GC/ownership 字段约定。
 * @note 在身份散列前校验临时布局满足 core 消费契约，避免发布不完整字段集合。 */
static TZrBool metadata_type_def_validate_canonical_union_layout(
        const SZrTypeLayout *layout,
        TZrUInt32 variantCount) {
    TZrUInt32 gcFieldCount = 0u;
    TZrUInt32 ownershipFieldCount = 0u;

    if (layout == ZR_NULL ||
        layout->layoutVersion != ZR_TYPE_LAYOUT_SCHEMA_VERSION ||
        layout->layoutHash == 0u ||
        layout->layoutHash != ZrCore_TypeLayout_ComputeHash(layout) ||
        layout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
        layout->byteSize == 0u ||
        layout->byteAlign == 0u ||
        (layout->byteAlign & (layout->byteAlign - 1u)) != 0u ||
        layout->tagOffset != 0u ||
        layout->tagSize != metadata_type_def_select_union_tag_size(variantCount) ||
        (layout->fieldCount > 0u && layout->fields == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        const TZrUInt32 allowedFlags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                       ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                       ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;

        if (field->byteSize != ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE ||
            field->byteOffset > layout->byteSize ||
            field->byteSize > layout->byteSize - field->byteOffset ||
            field->typeLayoutIndex != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
            field->activeTag >= variantCount ||
            (field->flags & ~allowedFlags) != 0u ||
            (field->flags & (ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                             ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE)) !=
                    (ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                     ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE)) {
            return ZR_FALSE;
        }
        gcFieldCount++;
        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u) {
            ownershipFieldCount++;
        }
    }

    return (TZrBool)(layout->gcFieldCount == gcFieldCount &&
                     layout->ownershipFieldCount == ownershipFieldCount &&
                     layout->refFieldCount == 0u);
}

/**
 * @brief 为 union TypeDef 计算跨模块比较的物理布局身份，不修改运行时字段表。
 * @pre 同一 union AST 和已注册类型原型应对应本轮 metadata 计划。
 * @note 唯一生产 caller 是 TypeDef metadata 发射器；失败让外层放弃 metadata。
 *       TypeLayout schema 摘要含 tag、payload、GC 值槽与 ownership 信息。
 */
TZrBool compiler_metadata_type_def_compute_union_layout_identity(SZrCompilerState *cs,
                                                                 const SZrAstNode *unionDeclaration,
                                                                 TZrUInt32 *outLayoutVersion,
                                                                 TZrUInt64 *outLayoutHash) {
    TZrUInt32 variantCount;
    TZrUInt32 tagSize;
    TZrUInt32 maxPayloadSize = 0;
    TZrUInt32 maxPayloadAlign = 1;
    TZrUInt32 payloadOffset;
    TZrUInt32 layoutByteAlign;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutFieldCount = 0u;
    TZrUInt32 layoutFieldCapacity = 0u;
    SZrTypeLayoutField *layoutFields = ZR_NULL;
    SZrTypeLayout layout;
    EZrTypeLayoutDropKind dropKind = ZR_TYPE_LAYOUT_DROP_KIND_NONE;

    if (outLayoutVersion != ZR_NULL) {
        *outLayoutVersion = 0;
    }
    if (outLayoutHash != ZR_NULL) {
        *outLayoutHash = 0;
    }
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL ||
        unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION ||
        outLayoutVersion == ZR_NULL ||
        outLayoutHash == ZR_NULL ||
        !metadata_type_def_union_variant_count(unionDeclaration, &variantCount)) {
        return ZR_FALSE;
    }

    tagSize = metadata_type_def_select_union_tag_size(variantCount);
    /* 先按有效 AST variant 的全部字段数预留临时布局项，后续只发布 GC-visible 值槽。 */
    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize variantIndex = 0; variantIndex < unionDeclaration->data.unionDeclaration.variants->count;
             variantIndex++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[variantIndex];
            if (variantNode != ZR_NULL &&
                variantNode->type == ZR_AST_UNION_VARIANT &&
                variantNode->data.unionVariant.fields != ZR_NULL) {
                if (variantNode->data.unionVariant.fields->count > UINT32_MAX - layoutFieldCapacity) {
                    return ZR_FALSE;
                }
                layoutFieldCapacity += (TZrUInt32)variantNode->data.unionVariant.fields->count;
            }
        }
    }
    if (layoutFieldCapacity > 0u) {
        layoutFields = (SZrTypeLayoutField *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrTypeLayoutField) * layoutFieldCapacity,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (layoutFields == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(layoutFields, 0, sizeof(SZrTypeLayoutField) * layoutFieldCapacity);
    }

    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize variantIndex = 0; variantIndex < unionDeclaration->data.unionDeclaration.variants->count;
             variantIndex++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[variantIndex];
            SZrUnionVariant *variant;
            TZrUInt32 currentOffset = 0;
            TZrUInt32 variantAlign = 1;
            TZrUInt32 variantPayloadSize;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }

            variant = &variantNode->data.unionVariant;

            if (variant->fields != ZR_NULL) {
                for (TZrSize fieldIndex = 0; fieldIndex < variant->fields->count; fieldIndex++) {
                    SZrAstNode *fieldNode = variant->fields->nodes[fieldIndex];
                    SZrParameter *field = ZR_NULL;
                    TZrUInt32 fieldSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
                    TZrUInt32 fieldAlign = ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;
                    TZrUInt32 ownershipQualifier = (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE;

                    if (fieldNode != ZR_NULL && fieldNode->type == ZR_AST_PARAMETER) {
                        field = &fieldNode->data.parameter;
                    }
                    if (field != ZR_NULL && field->typeInfo != ZR_NULL) {
                        ownershipQualifier =
                                metadata_type_def_payload_field_ownership_qualifier(field->typeInfo);
                        metadata_type_def_select_payload_field_layout(cs,
                                                                      unionDeclaration,
                                                                      field->typeInfo,
                                                                      ownershipQualifier,
                                                                      &fieldSize,
                                                                      &fieldAlign);
                    }

                    /* TODO: align_offset 与以下 u32 加法不报告溢出；需确认输入布局存在可证明上界。 */
                    currentOffset = align_offset(currentOffset, fieldAlign);
                    /* TODO: 宽度只能证明至少容纳一个槽，不能证明首 40 字节就是 SZrTypeValue。
                     * calculate_type_size 也会返回普通自定义 struct 的内联宽度；核实其 union ABI 表示，
                     * 否则应使用嵌套布局或展开 GC 槽，不应仅凭 fieldSize >= 40 推断槽身份。 */
                    if (fieldSize >= ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE) {
                        SZrTypeLayoutField *layoutField = &layoutFields[layoutFieldCount++];
                        layoutField->byteOffset = currentOffset;
                        layoutField->byteSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
                        layoutField->typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
                        layoutField->flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                             ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
                        if (ownershipQualifier != (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE) {
                            layoutField->flags |= ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
                            dropKind = ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE;
                        }
                        layoutField->activeTag = (TZrUInt32)variantIndex;
                    }
                    currentOffset += fieldSize;
                    if (fieldAlign > variantAlign) {
                        variantAlign = fieldAlign;
                    }
                }
            }

            variantPayloadSize = currentOffset > 0 ? align_offset(currentOffset, variantAlign) : 0u;
            if (variantPayloadSize > maxPayloadSize) {
                maxPayloadSize = variantPayloadSize;
            }
            if (variantAlign > maxPayloadAlign) {
                maxPayloadAlign = variantAlign;
            }
        }
    }

    /* 所有 variant 共用 tag 后 payload 起点；总尺寸再按整个 union 的最大对齐封口。 */
    payloadOffset = maxPayloadSize > 0 ? align_offset(tagSize, maxPayloadAlign) : tagSize;
    layoutByteAlign = tagSize > maxPayloadAlign ? tagSize : maxPayloadAlign;
    layoutByteSize = align_offset(payloadOffset + maxPayloadSize, layoutByteAlign);
    for (TZrUInt32 index = 0u; index < layoutFieldCount; index++) {
        if (layoutFields[index].byteOffset > UINT32_MAX - payloadOffset) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                         layoutFields,
                                         sizeof(SZrTypeLayoutField) * layoutFieldCapacity,
                                         ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
        layoutFields[index].byteOffset += payloadOffset;
    }

    ZrCore_TypeLayout_InitUnion(&layout,
                                layoutByteSize,
                                layoutByteAlign,
                                0u,
                                tagSize,
                                layoutFieldCount > 0u ? ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE
                                                      : ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                dropKind,
                                layoutFields,
                                layoutFieldCount);
    if (!metadata_type_def_validate_canonical_union_layout(&layout, variantCount)) {
        if (layoutFields != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                         layoutFields,
                                         sizeof(SZrTypeLayoutField) * layoutFieldCapacity,
                                         ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    *outLayoutVersion = layout.layoutVersion;
    *outLayoutHash = layout.layoutHash;
    if (layoutFields != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                     layoutFields,
                                     sizeof(SZrTypeLayoutField) * layoutFieldCapacity,
                                     ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}
