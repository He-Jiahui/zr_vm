#include "compiler_metadata_type_def_layout.h"
#include "compiler_metadata_signature.h"

#include "zr_vm_core/hash.h"

#define ZR_METADATA_TYPE_DEF_LAYOUT_VERSION 1u
#define ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE 8u
#define ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN 8u

static const TZrByte CZrMetadataTypeDefLayoutHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        'l',
        'a',
        'y',
        'o',
        'u',
        't',
        '.',
        'v',
        '1',
        '\0',
};

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
                                                          const SZrType *typeInfo,
                                                          TZrUInt32 ownershipQualifier,
                                                          TZrUInt32 *outSize,
                                                          TZrUInt32 *outAlign) {
    TZrUInt32 fieldSize = ZR_METADATA_TYPE_DEF_LAYOUT_REFERENCE_SIZE;
    TZrUInt32 fieldAlign = ZR_METADATA_TYPE_DEF_LAYOUT_MAX_SCALAR_ALIGN;

    if (ownershipQualifier != (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE) {
        fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
        fieldAlign = metadata_type_def_canonical_align_for_size(fieldSize);
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
    TZrSize bufferSize;
    TZrByte *buffer;
    TZrSize offset = 0;
    TZrUInt64 hash;

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
    bufferSize = sizeof(TZrUInt32) * 7u;
    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize variantIndex = 0; variantIndex < unionDeclaration->data.unionDeclaration.variants->count;
             variantIndex++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[variantIndex];
            SZrUnionVariant *variant;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }

            variant = &variantNode->data.unionVariant;
            bufferSize += sizeof(TZrUInt32) * 5u;
            if (variant->fields != ZR_NULL) {
                if (variant->fields->count > (TZrSize)ZR_METADATA_TOKEN_RID_MASK) {
                    return ZR_FALSE;
                }
                bufferSize += variant->fields->count * sizeof(TZrUInt32) * 4u;
            }
        }
    }

    buffer = (TZrByte *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                       bufferSize,
                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (buffer == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_token_write_u32(buffer, &offset, ZR_METADATA_TYPE_DEF_LAYOUT_VERSION);
    metadata_token_write_u32(buffer, &offset, variantCount);
    metadata_token_write_u32(buffer, &offset, tagSize);

    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize variantIndex = 0; variantIndex < unionDeclaration->data.unionDeclaration.variants->count;
             variantIndex++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[variantIndex];
            SZrUnionVariant *variant;
            TZrUInt32 currentOffset = 0;
            TZrUInt32 variantAlign = 1;
            TZrUInt32 fieldCount = 0;
            TZrUInt32 variantPayloadSize;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }

            variant = &variantNode->data.unionVariant;
            fieldCount = variant->fields != ZR_NULL ? (TZrUInt32)variant->fields->count : 0u;
            metadata_token_write_u32(buffer, &offset, (TZrUInt32)variantIndex);
            metadata_token_write_u32(buffer, &offset, (TZrUInt32)variant->kind);
            metadata_token_write_u32(buffer, &offset, fieldCount);

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
                                                                      field->typeInfo,
                                                                      ownershipQualifier,
                                                                      &fieldSize,
                                                                      &fieldAlign);
                    }

                    currentOffset = align_offset(currentOffset, fieldAlign);
                    metadata_token_write_u32(buffer, &offset, (TZrUInt32)fieldIndex);
                    metadata_token_write_u32(buffer, &offset, currentOffset);
                    metadata_token_write_u32(buffer, &offset, fieldSize);
                    metadata_token_write_u32(buffer, &offset, fieldAlign);
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

            metadata_token_write_u32(buffer, &offset, variantPayloadSize);
            metadata_token_write_u32(buffer, &offset, variantAlign);
        }
    }

    payloadOffset = maxPayloadSize > 0 ? align_offset(tagSize, maxPayloadAlign) : tagSize;
    layoutByteAlign = tagSize > maxPayloadAlign ? tagSize : maxPayloadAlign;
    layoutByteSize = align_offset(payloadOffset + maxPayloadSize, layoutByteAlign);
    metadata_token_write_u32(buffer, &offset, payloadOffset);
    metadata_token_write_u32(buffer, &offset, maxPayloadSize);
    metadata_token_write_u32(buffer, &offset, layoutByteSize);
    metadata_token_write_u32(buffer, &offset, layoutByteAlign);

    if (offset != bufferSize) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      buffer,
                                      bufferSize,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    hash = ZrCore_Hash_CreateStable64WithPrefix(CZrMetadataTypeDefLayoutHashV1Prefix,
                                                sizeof(CZrMetadataTypeDefLayoutHashV1Prefix),
                                                buffer,
                                                bufferSize);
    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  buffer,
                                  bufferSize,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (hash == 0) {
        return ZR_FALSE;
    }

    *outLayoutVersion = ZR_METADATA_TYPE_DEF_LAYOUT_VERSION;
    *outLayoutHash = hash;
    return ZR_TRUE;
}
