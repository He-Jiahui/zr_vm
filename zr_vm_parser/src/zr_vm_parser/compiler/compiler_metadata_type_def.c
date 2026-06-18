#include "compiler_metadata_type_def.h"
#include "compiler_metadata_type_def_layout.h"
#include "compiler_metadata_signature.h"
#include "type_inference_internal.h"

typedef struct SZrMetadataTypeDefUniqueEntry {
    SZrString *baseName;
    TZrUInt32 genericArity;
} SZrMetadataTypeDefUniqueEntry;

typedef struct SZrMetadataTypeDefScanContext {
    SZrCompilerState *compiler;
    TZrUInt32 count;
    TZrSize signatureHeapLength;
    TZrByte *emitHeap;
    TZrSize emitHeapLength;
    TZrSize *emitHeapOffset;
    SZrMetadataTokenRecord *records;
    TZrUInt32 recordCount;
    TZrUInt32 *recordIndex;
    TZrUInt32 *signatureRidCursor;
    TZrUInt32 emittedCount;
    SZrMetadataTypeDefUniqueEntry *uniqueEntries;
    TZrUInt32 uniqueEntryCount;
    TZrUInt32 uniqueEntryCapacity;
    const SZrMetadataStringHeapEntry *stringHeapEntries;
    TZrUInt32 stringHeapEntryCount;
    TZrMetadataTypeDefStringCollector stringCollector;
    void *stringCollectorUserData;
} SZrMetadataTypeDefScanContext;

static void metadata_type_def_init_unknown_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static TZrBool metadata_type_def_type_ref_from_ast(SZrCompilerState *cs,
                                                   SZrType *typeNode,
                                                   SZrFunctionTypedTypeRef *outType) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;
    SZrString *typeName;
    TZrNativeString typeNameText;
    TZrSize typeNameLength;
    EZrValueType primitiveType;

    if (cs == ZR_NULL || cs->state == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_type_def_init_unknown_type_ref(outType);
    if (typeNode == ZR_NULL) {
        return ZR_TRUE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeNode, &ownershipQualifier, &ownershipInnerType)) {
        if (!metadata_type_def_type_ref_from_ast(cs, (SZrType *)ownershipInnerType, outType)) {
            return ZR_FALSE;
        }
        outType->ownershipQualifier = (TZrUInt32)ownershipQualifier;
        return ZR_TRUE;
    }

    if (typeNode->dimensions > 0) {
        SZrType elementType = *typeNode;
        SZrFunctionTypedTypeRef elementRef;

        elementType.dimensions--;
        if (!metadata_type_def_type_ref_from_ast(cs, &elementType, &elementRef)) {
            return ZR_FALSE;
        }

        outType->baseType = ZR_VALUE_TYPE_ARRAY;
        outType->isArray = ZR_TRUE;
        outType->elementBaseType = elementRef.baseType;
        outType->elementTypeName = elementRef.typeName;
        outType->ownershipQualifier = (TZrUInt32)typeNode->ownershipQualifier;
        return ZR_TRUE;
    }

    typeName = extract_type_name_string(cs, typeNode);
    if (typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    typeNameLength = typeNameText != ZR_NULL ? strlen(typeNameText) : 0u;
    if (inferred_type_try_map_primitive_name(typeNameText, typeNameLength, &primitiveType)) {
        outType->baseType = primitiveType;
    } else {
        outType->baseType = ZR_VALUE_TYPE_OBJECT;
        outType->typeName = typeName;
    }
    outType->ownershipQualifier = (TZrUInt32)typeNode->ownershipQualifier;
    return ZR_TRUE;
}

static SZrAstNode *metadata_type_def_find_union_declaration_in_array(SZrAstNodeArray *declarations,
                                                                     SZrString *baseName) {
    if (declarations == ZR_NULL || declarations->nodes == ZR_NULL || baseName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];

        if (declaration == ZR_NULL) {
            continue;
        }
        if (declaration->type == ZR_AST_UNION_DECLARATION &&
            declaration->data.unionDeclaration.name != ZR_NULL &&
            declaration->data.unionDeclaration.name->name != ZR_NULL &&
            ZrCore_String_Equal(declaration->data.unionDeclaration.name->name, baseName)) {
            return declaration;
        }
        if (declaration->type == ZR_AST_EXTERN_BLOCK) {
            SZrAstNode *match = metadata_type_def_find_union_declaration_in_array(
                    declaration->data.externBlock.declarations,
                    baseName);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

static SZrAstNode *metadata_type_def_find_union_declaration(SZrCompilerState *cs,
                                                            SZrString *baseName) {
    if (cs == ZR_NULL ||
        cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT ||
        baseName == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_type_def_find_union_declaration_in_array(cs->scriptAst->data.script.statements, baseName);
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

static TZrSize metadata_type_def_field_signature_size(SZrCompilerState *cs,
                                                      const SZrParameter *field) {
    SZrFunctionTypedTypeRef fieldType;

    if (field == ZR_NULL) {
        return sizeof(TZrUInt32) + sizeof(TZrUInt32) + (1u + sizeof(TZrUInt32));
    }

    if (!metadata_type_def_type_ref_from_ast(cs, field->typeInfo, &fieldType)) {
        metadata_type_def_init_unknown_type_ref(&fieldType);
    }

    return sizeof(TZrUInt32) +
           sizeof(TZrUInt32) +
           metadata_token_type_ref_signature_size(cs, &fieldType);
}

static TZrBool metadata_type_def_union_contract_size(SZrCompilerState *cs,
                                                     const SZrAstNode *unionDeclaration,
                                                     TZrSize *outSize) {
    TZrSize size = sizeof(TZrUInt32);
    TZrUInt32 variantCount;

    if (outSize != ZR_NULL) {
        *outSize = 0;
    }
    if (cs == ZR_NULL ||
        unionDeclaration == ZR_NULL ||
        outSize == ZR_NULL ||
        !metadata_type_def_union_variant_count(unionDeclaration, &variantCount)) {
        return ZR_FALSE;
    }

    if (unionDeclaration->data.unionDeclaration.variants != ZR_NULL) {
        for (TZrSize index = 0; index < unionDeclaration->data.unionDeclaration.variants->count; index++) {
            SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[index];
            SZrUnionVariant *variant;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }

            variant = &variantNode->data.unionVariant;
            if (variant->fields != ZR_NULL) {
                if (variant->fields->count > (TZrSize)ZR_METADATA_TOKEN_RID_MASK) {
                    return ZR_FALSE;
                }
            }

            size += sizeof(TZrUInt32) +
                    sizeof(TZrUInt32) +
                    sizeof(TZrUInt32) +
                    sizeof(TZrUInt32);
            if (variant->fields != ZR_NULL) {
                for (TZrSize fieldIndex = 0; fieldIndex < variant->fields->count; fieldIndex++) {
                    SZrAstNode *fieldNode = variant->fields->nodes[fieldIndex];
                    if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
                        continue;
                    }
                    size += metadata_type_def_field_signature_size(cs, &fieldNode->data.parameter);
                }
            }
        }
    }

    *outSize = size;
    return ZR_TRUE;
}

static TZrSize metadata_type_def_signature_size(SZrCompilerState *cs,
                                                const SZrAstNode *unionDeclaration) {
    TZrSize contractSize;

    if (!metadata_type_def_union_contract_size(cs, unionDeclaration, &contractSize)) {
        return 0;
    }

    return 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) + contractSize;
}

static TZrBool metadata_type_def_collect_type_ref_strings(SZrCompilerState *cs,
                                                          const SZrFunctionTypedTypeRef *typeRef,
                                                          TZrMetadataTypeDefStringCollector collector,
                                                          void *userData) {
    TZrNativeString typeNameText;

    if (cs == ZR_NULL || collector == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_type_def_collect_type_ref_strings(cs, &element, collector, userData);
    }

    typeNameText = typeRef->typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeRef->typeName) : ZR_NULL;
    if (typeNameText != ZR_NULL && strlen(typeNameText) > 0u) {
        SZrString *baseName = ZR_NULL;
        SZrArray argumentTypeNames;

        if (try_parse_generic_instance_type_name(cs->state, typeRef->typeName, &baseName, &argumentTypeNames)) {
            if (!collector(cs, baseName, userData)) {
                ZrCore_Array_Free(cs->state, &argumentTypeNames);
                return ZR_FALSE;
            }

            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_type_def_init_unknown_type_ref(&argumentTypeRef);
                argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
                if (!metadata_type_def_collect_type_ref_strings(cs, &argumentTypeRef, collector, userData)) {
                    ZrCore_Array_Free(cs->state, &argumentTypeNames);
                    return ZR_FALSE;
                }
            }

            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_TRUE;
        }

        return collector(cs, typeRef->typeName, userData);
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_def_collect_field_strings(SZrCompilerState *cs,
                                                       const SZrParameter *field,
                                                       TZrMetadataTypeDefStringCollector collector,
                                                       void *userData) {
    SZrFunctionTypedTypeRef fieldType;

    if (field == ZR_NULL || collector == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!collector(cs, field->name != ZR_NULL ? field->name->name : ZR_NULL, userData)) {
        return ZR_FALSE;
    }

    if (!metadata_type_def_type_ref_from_ast(cs, field->typeInfo, &fieldType)) {
        metadata_type_def_init_unknown_type_ref(&fieldType);
    }
    return metadata_type_def_collect_type_ref_strings(cs, &fieldType, collector, userData);
}

static TZrBool metadata_type_def_collect_union_contract_strings(SZrCompilerState *cs,
                                                                const SZrAstNode *unionDeclaration,
                                                                TZrMetadataTypeDefStringCollector collector,
                                                                void *userData) {
    if (unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION ||
        collector == ZR_NULL) {
        return ZR_TRUE;
    }

    if (unionDeclaration->data.unionDeclaration.variants == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < unionDeclaration->data.unionDeclaration.variants->count; index++) {
        SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[index];
        SZrUnionVariant *variant;

        if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
            continue;
        }

        variant = &variantNode->data.unionVariant;
        if (!collector(cs, variant->name != ZR_NULL ? variant->name->name : ZR_NULL, userData)) {
            return ZR_FALSE;
        }

        if (variant->fields != ZR_NULL) {
            for (TZrSize fieldIndex = 0; fieldIndex < variant->fields->count; fieldIndex++) {
                SZrAstNode *fieldNode = variant->fields->nodes[fieldIndex];

                if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
                    continue;
                }

                if (!metadata_type_def_collect_field_strings(cs,
                                                             &fieldNode->data.parameter,
                                                             collector,
                                                             userData)) {
                    return ZR_FALSE;
                }
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_def_seen_before(const SZrMetadataTypeDefScanContext *context,
                                             SZrString *baseName,
                                             TZrUInt32 genericArity) {
    if (context == ZR_NULL || context->uniqueEntries == ZR_NULL || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < context->uniqueEntryCount; index++) {
        const SZrMetadataTypeDefUniqueEntry *entry = &context->uniqueEntries[index];

        if (entry->baseName != ZR_NULL &&
            entry->genericArity == genericArity &&
            ZrCore_String_Equal(entry->baseName, baseName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_type_def_remember(SZrMetadataTypeDefScanContext *context,
                                          SZrString *baseName,
                                          TZrUInt32 genericArity) {
    if (context == ZR_NULL || context->uniqueEntries == ZR_NULL) {
        return ZR_TRUE;
    }
    if (context->uniqueEntryCount >= context->uniqueEntryCapacity || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    context->uniqueEntries[context->uniqueEntryCount].baseName = baseName;
    context->uniqueEntries[context->uniqueEntryCount].genericArity = genericArity;
    context->uniqueEntryCount++;
    return ZR_TRUE;
}

static TZrBool metadata_type_def_add_candidate_count(TZrUInt32 *ioCount,
                                                     TZrUInt32 addCount) {
    if (ioCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (addCount > ZR_METADATA_TOKEN_RID_MASK - *ioCount) {
        return ZR_FALSE;
    }

    *ioCount += addCount;
    return ZR_TRUE;
}

static void metadata_type_def_write_field_signature(TZrByte *buffer,
                                                    TZrSize *offset,
                                                    SZrCompilerState *cs,
                                                    const SZrParameter *field,
                                                    const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                    TZrUInt32 stringHeapEntryCount) {
    SZrFunctionTypedTypeRef fieldType;

    metadata_token_write_string_ref(buffer,
                                    offset,
                                    field != ZR_NULL && field->name != ZR_NULL ? field->name->name : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_u32(buffer, offset, field != ZR_NULL ? (TZrUInt32)field->passingMode : 0u);
    if (field == ZR_NULL || !metadata_type_def_type_ref_from_ast(cs, field->typeInfo, &fieldType)) {
        metadata_type_def_init_unknown_type_ref(&fieldType);
    }
    metadata_token_write_type_ref_signature(buffer,
                                            offset,
                                            cs,
                                            &fieldType,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
}

static void metadata_type_def_write_union_contract(TZrByte *buffer,
                                                   TZrSize *offset,
                                                   SZrCompilerState *cs,
                                                   const SZrAstNode *unionDeclaration,
                                                   const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                   TZrUInt32 stringHeapEntryCount) {
    TZrUInt32 variantCount = 0;

    (void)metadata_type_def_union_variant_count(unionDeclaration, &variantCount);
    metadata_token_write_u32(buffer, offset, variantCount);
    if (unionDeclaration == ZR_NULL || unionDeclaration->type != ZR_AST_UNION_DECLARATION ||
        unionDeclaration->data.unionDeclaration.variants == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < unionDeclaration->data.unionDeclaration.variants->count; index++) {
        SZrAstNode *variantNode = unionDeclaration->data.unionDeclaration.variants->nodes[index];
        SZrUnionVariant *variant;
        TZrUInt32 fieldCount = 0;

        if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
            continue;
        }

        variant = &variantNode->data.unionVariant;
        if (variant->fields != ZR_NULL) {
            fieldCount = (TZrUInt32)variant->fields->count;
        }

        metadata_token_write_string_ref(buffer,
                                        offset,
                                        variant->name != ZR_NULL ? variant->name->name : ZR_NULL,
                                        stringHeapEntries,
                                        stringHeapEntryCount);
        metadata_token_write_u32(buffer, offset, (TZrUInt32)variant->kind);
        metadata_token_write_u32(buffer, offset, variant->isDefaultUsingVariant ? 1u : 0u);
        metadata_token_write_u32(buffer, offset, fieldCount);

        if (variant->fields != ZR_NULL) {
            for (TZrSize fieldIndex = 0; fieldIndex < variant->fields->count; fieldIndex++) {
                SZrAstNode *fieldNode = variant->fields->nodes[fieldIndex];

                if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
                    continue;
                }

                metadata_type_def_write_field_signature(buffer,
                                                        offset,
                                                        cs,
                                                        &fieldNode->data.parameter,
                                                        stringHeapEntries,
                                                        stringHeapEntryCount);
            }
        }
    }
}

static void metadata_type_def_write_signature(TZrByte *buffer,
                                              TZrSize *offset,
                                              SZrCompilerState *cs,
                                              SZrString *baseName,
                                              TZrUInt32 genericArity,
                                              const SZrAstNode *unionDeclaration,
                                              const SZrMetadataStringHeapEntry *stringHeapEntries,
                                              TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_DEF);
    metadata_token_write_string_ref(buffer, offset, baseName, stringHeapEntries, stringHeapEntryCount);
    metadata_token_write_u32(buffer, offset, genericArity);
    metadata_type_def_write_union_contract(buffer,
                                           offset,
                                           cs,
                                           unionDeclaration,
                                           stringHeapEntries,
                                           stringHeapEntryCount);
}

static TZrBool metadata_type_def_write_record_pair(SZrMetadataTypeDefScanContext *context,
                                                   TZrUInt32 ownerIndex,
                                                   TZrUInt32 signatureStart,
                                                   TZrUInt32 signatureLength,
                                                   TZrUInt64 signatureHash,
                                                   TZrUInt32 layoutVersion,
                                                   TZrUInt64 layoutHash) {
    TZrUInt32 recordIndex;
    TZrMetadataToken typeDefToken;
    TZrMetadataToken signatureToken;

    if (context == ZR_NULL ||
        context->records == ZR_NULL ||
        context->recordIndex == ZR_NULL ||
        context->signatureRidCursor == ZR_NULL ||
        *context->recordIndex + 1u >= context->recordCount) {
        return ZR_FALSE;
    }

    typeDefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, context->emittedCount + 1u);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*context->signatureRidCursor)++);

    recordIndex = *context->recordIndex;
    context->records[recordIndex].token = typeDefToken;
    context->records[recordIndex].relatedToken = signatureToken;
    context->records[recordIndex].ownerToken = 0;
    context->records[recordIndex].ownerIndex = ownerIndex;
    context->records[recordIndex].signatureBlobOffset = signatureStart;
    context->records[recordIndex].signatureBlobLength = signatureLength;
    context->records[recordIndex].signatureHash = signatureHash;
    context->records[recordIndex].layoutVersion = layoutVersion;
    context->records[recordIndex].layoutHash = layoutHash;
    recordIndex++;

    context->records[recordIndex].token = signatureToken;
    context->records[recordIndex].relatedToken = typeDefToken;
    context->records[recordIndex].ownerToken = typeDefToken;
    context->records[recordIndex].ownerIndex = ownerIndex;
    context->records[recordIndex].signatureBlobOffset = signatureStart;
    context->records[recordIndex].signatureBlobLength = signatureLength;
    context->records[recordIndex].signatureHash = signatureHash;
    context->records[recordIndex].layoutVersion = layoutVersion;
    context->records[recordIndex].layoutHash = layoutHash;
    recordIndex++;

    *context->recordIndex = recordIndex;
    context->emittedCount++;
    return ZR_TRUE;
}

static TZrBool metadata_type_def_visit_named_type(SZrMetadataTypeDefScanContext *context,
                                                  const SZrFunctionTypedTypeRef *typeRef,
                                                  TZrUInt32 ownerIndex) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrAstNode *unionDeclaration;
    TZrUInt32 genericArity;
    TZrSize signatureLength;

    if (context == ZR_NULL || context->compiler == ZR_NULL || typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_token_try_resolve_union_signature_type(context->compiler,
                                                         typeRef->typeName,
                                                         &baseName,
                                                         &argumentTypeNames)) {
        return ZR_TRUE;
    }

    if (argumentTypeNames.length > (TZrSize)0xFFFFFFFFu) {
        ZrCore_Array_Free(context->compiler->state, &argumentTypeNames);
        return ZR_FALSE;
    }

    genericArity = (TZrUInt32)argumentTypeNames.length;
    ZrCore_Array_Free(context->compiler->state, &argumentTypeNames);
    if (baseName == ZR_NULL || metadata_type_def_seen_before(context, baseName, genericArity)) {
        return ZR_TRUE;
    }

    unionDeclaration = metadata_type_def_find_union_declaration(context->compiler, baseName);
    if (unionDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->stringCollector != ZR_NULL) {
        if (!context->stringCollector(context->compiler, baseName, context->stringCollectorUserData) ||
            !metadata_type_def_collect_union_contract_strings(context->compiler,
                                                              unionDeclaration,
                                                              context->stringCollector,
                                                              context->stringCollectorUserData) ||
            !metadata_type_def_remember(context, baseName, genericArity)) {
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    signatureLength = metadata_type_def_signature_size(context->compiler, unionDeclaration);
    if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    if (context->emitHeap == ZR_NULL) {
        if (context->signatureHeapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
            return ZR_FALSE;
        }
        context->signatureHeapLength += signatureLength;
        context->count++;
        return metadata_type_def_remember(context, baseName, genericArity);
    }

    {
        TZrSize signatureStart;
        TZrUInt64 signatureHash;
        TZrUInt32 layoutVersion;
        TZrUInt64 layoutHash;

        if (context->emitHeapOffset == ZR_NULL) {
            return ZR_FALSE;
        }

        signatureStart = *context->emitHeapOffset;
        if (signatureStart > context->emitHeapLength ||
            signatureLength > context->emitHeapLength - signatureStart ||
            signatureStart > (TZrSize)0xFFFFFFFFu) {
            return ZR_FALSE;
        }

        metadata_type_def_write_signature(context->emitHeap,
                                          context->emitHeapOffset,
                                          context->compiler,
                                          baseName,
                                          genericArity,
                                          unionDeclaration,
                                          context->stringHeapEntries,
                                          context->stringHeapEntryCount);
        if (*context->emitHeapOffset - signatureStart != signatureLength) {
            return ZR_FALSE;
        }

        signatureHash = metadata_signature_hash_v1(context->emitHeap + signatureStart, signatureLength);
        if (!compiler_metadata_type_def_compute_union_layout_identity(context->compiler,
                                                                      unionDeclaration,
                                                                      &layoutVersion,
                                                                      &layoutHash)) {
            return ZR_FALSE;
        }
        if (signatureHash == 0 ||
            (context->records != ZR_NULL &&
             !metadata_type_def_write_record_pair(context,
                                                  ownerIndex,
                                                  (TZrUInt32)signatureStart,
                                                  (TZrUInt32)signatureLength,
                                                  signatureHash,
                                                  layoutVersion,
                                                  layoutHash))) {
            return ZR_FALSE;
        }

        if (!metadata_type_def_remember(context, baseName, genericArity)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_def_visit_type(SZrMetadataTypeDefScanContext *context,
                                            const SZrFunctionTypedTypeRef *typeRef,
                                            TZrUInt32 ownerIndex) {
    if (context == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_type_def_visit_type(context, &element, ownerIndex);
    }

    if (typeRef->typeName != ZR_NULL && context->compiler != ZR_NULL && context->compiler->state != ZR_NULL) {
        SZrString *baseName = ZR_NULL;
        SZrArray argumentTypeNames;

        if (try_parse_generic_instance_type_name(context->compiler->state,
                                                 typeRef->typeName,
                                                 &baseName,
                                                 &argumentTypeNames)) {
            if (!metadata_type_def_visit_named_type(context, typeRef, ownerIndex)) {
                ZrCore_Array_Free(context->compiler->state, &argumentTypeNames);
                return ZR_FALSE;
            }
            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_type_def_init_unknown_type_ref(&argumentTypeRef);
                argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
                if (!metadata_type_def_visit_type(context, &argumentTypeRef, ownerIndex)) {
                    ZrCore_Array_Free(context->compiler->state, &argumentTypeNames);
                    return ZR_FALSE;
                }
            }
            ZrCore_Array_Free(context->compiler->state, &argumentTypeNames);
            return ZR_TRUE;
        }
    }

    return metadata_type_def_visit_named_type(context, typeRef, ownerIndex);
}

static TZrBool metadata_type_def_visit_export(SZrMetadataTypeDefScanContext *context,
                                              const SZrFunctionTypedExportSymbol *symbol,
                                              TZrUInt32 ownerIndex) {
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_type_def_visit_type(context, &symbol->valueType, ownerIndex)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < symbol->parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[index] : ZR_NULL;

        if (!metadata_type_def_visit_type(context, parameterType, ownerIndex)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool metadata_type_def_scan(SZrMetadataTypeDefScanContext *context,
                                      const SZrFunction *function) {
    if (context == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->typedExportedSymbolLength > 0 && function->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        if (!metadata_type_def_visit_export(context, &function->typedExportedSymbols[index], index)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_def_type_candidate_count(SZrCompilerState *cs,
                                                      const SZrFunctionTypedTypeRef *typeRef,
                                                      TZrUInt32 *ioCount) {
    if (ioCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_type_def_type_candidate_count(cs, &element, ioCount);
    }

    if (typeRef->typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_type_def_add_candidate_count(ioCount, 1u)) {
        return ZR_FALSE;
    }

    if (cs != ZR_NULL && cs->state != ZR_NULL) {
        SZrString *baseName = ZR_NULL;
        SZrArray argumentTypeNames;

        if (try_parse_generic_instance_type_name(cs->state, typeRef->typeName, &baseName, &argumentTypeNames)) {
            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_type_def_init_unknown_type_ref(&argumentTypeRef);
                argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
                if (!metadata_type_def_type_candidate_count(cs, &argumentTypeRef, ioCount)) {
                    ZrCore_Array_Free(cs->state, &argumentTypeNames);
                    return ZR_FALSE;
                }
            }
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_def_max_candidate_count(SZrCompilerState *cs,
                                                     const SZrFunction *function,
                                                     TZrUInt32 *outCount) {
    TZrUInt32 count = 0u;

    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (function == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->typedExportedSymbolLength > 0 && function->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        if (!metadata_type_def_type_candidate_count(cs, &symbol->valueType, &count)) {
            return ZR_FALSE;
        }
        for (TZrUInt32 parameterIndex = 0; parameterIndex < symbol->parameterCount; parameterIndex++) {
            if (!metadata_type_def_type_candidate_count(
                        cs,
                        symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[parameterIndex] : ZR_NULL,
                        &count)) {
                return ZR_FALSE;
            }
        }
    }

    *outCount = count;
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_def_collect_strings(SZrCompilerState *cs,
                                                   const SZrFunction *function,
                                                   TZrMetadataTypeDefStringCollector collector,
                                                   void *userData) {
    SZrMetadataTypeDefScanContext context;
    TZrUInt32 maxTypeDefCount;

    if (collector == ZR_NULL ||
        cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL ||
        function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!metadata_type_def_max_candidate_count(cs, function, &maxTypeDefCount)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    context.stringCollector = collector;
    context.stringCollectorUserData = userData;
    if (maxTypeDefCount > 0) {
        context.uniqueEntries = (SZrMetadataTypeDefUniqueEntry *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (context.uniqueEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(context.uniqueEntries,
                             0,
                             sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount);
        context.uniqueEntryCapacity = maxTypeDefCount;
    }

    if (!metadata_type_def_scan(&context, function)) {
        if (context.uniqueEntries != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          context.uniqueEntries,
                                          sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    if (context.uniqueEntries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      context.uniqueEntries,
                                      sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_def_plan(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        SZrMetadataTypeDefPlan *outPlan) {
    SZrMetadataTypeDefScanContext context;
    TZrUInt32 maxTypeDefCount;
    SZrGlobalState *global;

    if (outPlan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outPlan, 0, sizeof(*outPlan));
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!metadata_type_def_max_candidate_count(cs, function, &maxTypeDefCount)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    if (maxTypeDefCount > 0) {
        global = cs->state->global;
        context.uniqueEntries = (SZrMetadataTypeDefUniqueEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (context.uniqueEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(context.uniqueEntries,
                             0,
                             sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount);
        context.uniqueEntryCapacity = maxTypeDefCount;
    }

    if (!metadata_type_def_scan(&context, function)) {
        if (context.uniqueEntries != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          context.uniqueEntries,
                                          sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    outPlan->typeDefCount = context.count;
    outPlan->signatureHeapLength = context.signatureHeapLength;
    if (context.uniqueEntries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      context.uniqueEntries,
                                      sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_def_emit(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        SZrMetadataTokenRecord *records,
                                        TZrUInt32 recordCount,
                                        TZrUInt32 *ioRecordIndex,
                                        TZrByte *heap,
                                        TZrSize heapLength,
                                        TZrSize *ioHeapOffset,
                                        TZrUInt32 *ioSignatureRidCursor,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount) {
    SZrMetadataTypeDefScanContext context;
    TZrUInt32 maxTypeDefCount;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL ||
        function == ZR_NULL ||
        records == ZR_NULL ||
        ioRecordIndex == ZR_NULL ||
        heap == ZR_NULL ||
        ioHeapOffset == ZR_NULL ||
        ioSignatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!metadata_type_def_max_candidate_count(cs, function, &maxTypeDefCount)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    context.emitHeap = heap;
    context.emitHeapLength = heapLength;
    context.emitHeapOffset = ioHeapOffset;
    context.records = records;
    context.recordCount = recordCount;
    context.recordIndex = ioRecordIndex;
    context.signatureRidCursor = ioSignatureRidCursor;
    context.stringHeapEntries = stringHeapEntries;
    context.stringHeapEntryCount = stringHeapEntryCount;
    if (maxTypeDefCount > 0) {
        context.uniqueEntries = (SZrMetadataTypeDefUniqueEntry *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (context.uniqueEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(context.uniqueEntries,
                             0,
                             sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount);
        context.uniqueEntryCapacity = maxTypeDefCount;
    }

    if (!metadata_type_def_scan(&context, function)) {
        if (context.uniqueEntries != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          context.uniqueEntries,
                                          sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    if (context.uniqueEntries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      context.uniqueEntries,
                                      sizeof(SZrMetadataTypeDefUniqueEntry) * maxTypeDefCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}
