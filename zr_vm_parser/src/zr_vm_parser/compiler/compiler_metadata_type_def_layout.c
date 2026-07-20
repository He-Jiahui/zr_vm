#include "compiler_metadata_type_def_layout.h"
#include "compiler_metadata_signature.h"

#include "zr_vm_core/type_layout.h"

#define ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE ((TZrUInt32)sizeof(SZrTypeValue))
#define ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN ((TZrUInt32)ZR_ALIGN_SIZE)

SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName);

static TZrUInt32 metadata_type_def_select_union_tag_size(TZrUInt32 variantCount) {
    if (variantCount <= 0xffu) {
        return 1u;
    }
    if (variantCount <= 0xffffu) {
        return 2u;
    }
    return 4u;
}

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

                    currentOffset = align_offset(currentOffset, fieldAlign);
                    if (fieldSize >= sizeof(SZrTypeValue)) {
                        SZrTypeLayoutField *layoutField = &layoutFields[layoutFieldCount++];
                        layoutField->byteOffset = currentOffset;
                        layoutField->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
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
    if (!ZrCore_TypeLayout_Validate(&layout)) {
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
