#include "compiler_internal.h"

typedef struct SZrUnionPayloadFieldLayout {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 ownershipQualifier;
} SZrUnionPayloadFieldLayout;

typedef struct SZrUnionVariantLayout {
    TZrUInt32 payloadSize;
    TZrUInt32 payloadAlign;
    SZrArray fieldLayouts;
} SZrUnionVariantLayout;

typedef struct SZrUnionLayoutInfo {
    TZrUInt32 tagSize;
    TZrUInt32 tagAlign;
    TZrUInt32 payloadOffset;
    TZrUInt32 payloadSize;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
    SZrArray variantLayouts;
} SZrUnionLayoutInfo;

static void compiler_union_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->memberType = ZR_AST_UNION_VARIANT;
    memberInfo->accessModifier = ZR_ACCESS_PRIVATE;
    memberInfo->isStatic = ZR_TRUE;
    memberInfo->isConst = ZR_FALSE;
    memberInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    memberInfo->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    memberInfo->metaType = ZR_META_ENUM_MAX;
    memberInfo->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo->virtualSlotIndex = (TZrUInt32)-1;
    memberInfo->interfaceContractSlot = (TZrUInt32)-1;
    memberInfo->propertyIdentity = (TZrUInt32)-1;
    memberInfo->accessorRole = 0;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->parameterNames);
    ZrCore_Array_Construct(&memberInfo->parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo->parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo->decorators);
    memberInfo->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&memberInfo->decoratorMetadataValue);
}

static void compiler_union_append_payload_type(SZrCompilerState *cs,
                                               SZrArray *parameterTypes,
                                               SZrType *typeInfo) {
    SZrInferredType payloadType;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    if (typeInfo != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &payloadType)) {
        ZrCore_Array_Push(cs->state, parameterTypes, &payloadType);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &payloadType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Push(cs->state, parameterTypes, &payloadType);
}

static void compiler_union_collect_payload_metadata(SZrCompilerState *cs,
                                                    SZrTypeMemberInfo *memberInfo,
                                                    SZrAstNodeArray *fields) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || fields == ZR_NULL || fields->count == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state, &memberInfo->parameterTypes, sizeof(SZrInferredType), fields->count);
    ZrCore_Array_Init(cs->state, &memberInfo->parameterNames, sizeof(SZrString *), fields->count);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      fields->count);

    for (TZrSize index = 0; index < fields->count; index++) {
        SZrAstNode *fieldNode = fields->nodes[index];
        SZrParameter *field;
        SZrString *fieldName = ZR_NULL;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

        if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        field = &fieldNode->data.parameter;
        compiler_union_append_payload_type(cs, &memberInfo->parameterTypes, field->typeInfo);
        if (field->name != ZR_NULL) {
            fieldName = field->name->name;
        }
        ZrCore_Array_Push(cs->state, &memberInfo->parameterNames, &fieldName);
        passingMode = field->passingMode;
        ZrCore_Array_Push(cs->state, &memberInfo->parameterPassingModes, &passingMode);
    }

    memberInfo->parameterCount = (TZrUInt32)memberInfo->parameterTypes.length;
}

static TZrUInt32 compiler_union_select_tag_size(TZrSize variantCount) {
    if (variantCount <= 0xffu) {
        return 1u;
    }
    if (variantCount <= 0xffffu) {
        return 2u;
    }
    return 4u;
}

static EZrOwnershipQualifier compiler_union_payload_ownership_qualifier(SZrType *typeInfo) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (typeInfo == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeInfo, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipInnerType);
        return ownershipQualifier;
    }

    return typeInfo->ownershipQualifier;
}

static TZrBool compiler_union_generic_parameter_name_matches(SZrGenericDeclaration *generic,
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

static TZrBool compiler_union_type_references_generic_parameter(SZrGenericDeclaration *generic,
                                                                const SZrType *typeInfo) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (generic == ZR_NULL || generic->params == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeInfo, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipQualifier);
        return compiler_union_type_references_generic_parameter(generic, ownershipInnerType);
    }

    if (typeInfo->subType != ZR_NULL &&
        compiler_union_type_references_generic_parameter(generic, typeInfo->subType)) {
        return ZR_TRUE;
    }

    if (typeInfo->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return compiler_union_generic_parameter_name_matches(generic,
                                                            typeInfo->name->data.identifier.name);
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeInfo->name->data.genericType;
        if (genericType->name != ZR_NULL &&
            compiler_union_generic_parameter_name_matches(generic, genericType->name->name)) {
            return ZR_TRUE;
        }
        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *argumentNode = genericType->params->nodes[index];
                if (argumentNode != ZR_NULL &&
                    argumentNode->type == ZR_AST_TYPE &&
                    compiler_union_type_references_generic_parameter(generic, &argumentNode->data.type)) {
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
                compiler_union_type_references_generic_parameter(generic, &elementNode->data.type)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_union_payload_uses_value_slot(SZrUnionDeclaration *unionDecl,
                                                      SZrType *typeInfo,
                                                      TZrUInt32 ownershipQualifier) {
    return (TZrBool)(ownershipQualifier != (TZrUInt32)ZR_OWNERSHIP_QUALIFIER_NONE ||
                     compiler_union_type_references_generic_parameter(
                             unionDecl != ZR_NULL ? unionDecl->generic : ZR_NULL,
                             typeInfo));
}

static void compiler_union_layout_info_init(SZrCompilerState *cs,
                                            SZrUnionDeclaration *unionDecl,
                                            SZrUnionLayoutInfo *layoutInfo) {
    TZrSize variantCount;
    TZrUInt32 maxPayloadSize = 0;
    TZrUInt32 maxPayloadAlign = 1;

    if (cs == ZR_NULL || unionDecl == ZR_NULL || layoutInfo == ZR_NULL) {
        return;
    }

    memset(layoutInfo, 0, sizeof(*layoutInfo));
    variantCount = unionDecl->variants != ZR_NULL ? unionDecl->variants->count : 0;
    layoutInfo->tagSize = compiler_union_select_tag_size(variantCount);
    layoutInfo->tagAlign = layoutInfo->tagSize;
    ZrCore_Array_Init(cs->state,
                      &layoutInfo->variantLayouts,
                      sizeof(SZrUnionVariantLayout),
                      variantCount > 0 ? variantCount : 1);

    if (unionDecl->variants != ZR_NULL) {
        for (TZrSize variantIndex = 0; variantIndex < unionDecl->variants->count; variantIndex++) {
            SZrAstNode *variantNode = unionDecl->variants->nodes[variantIndex];
            SZrUnionVariantLayout variantLayout;
            TZrUInt32 currentOffset = 0;
            TZrUInt32 variantAlign = 1;
            TZrSize fieldCount = 0;

            memset(&variantLayout, 0, sizeof(variantLayout));
            if (variantNode != ZR_NULL && variantNode->type == ZR_AST_UNION_VARIANT &&
                variantNode->data.unionVariant.fields != ZR_NULL) {
                fieldCount = variantNode->data.unionVariant.fields->count;
            }
            ZrCore_Array_Init(cs->state,
                              &variantLayout.fieldLayouts,
                              sizeof(SZrUnionPayloadFieldLayout),
                              fieldCount > 0 ? fieldCount : 1);

            if (fieldCount > 0) {
                SZrAstNodeArray *fields = variantNode->data.unionVariant.fields;
                for (TZrSize fieldIndex = 0; fieldIndex < fields->count; fieldIndex++) {
                    SZrAstNode *fieldNode = fields->nodes[fieldIndex];
                    SZrUnionPayloadFieldLayout fieldLayout;
                    TZrUInt32 fieldSize = ZR_ALIGN_SIZE;
                    TZrUInt32 fieldAlign = ZR_ALIGN_SIZE;

                    memset(&fieldLayout, 0, sizeof(fieldLayout));
                    if (fieldNode != ZR_NULL && fieldNode->type == ZR_AST_PARAMETER) {
                        SZrParameter *field = &fieldNode->data.parameter;
                        if (field->typeInfo != ZR_NULL) {
                            fieldLayout.ownershipQualifier =
                                    (TZrUInt32)compiler_union_payload_ownership_qualifier(field->typeInfo);
                            if (compiler_union_payload_uses_value_slot(unionDecl,
                                                                       field->typeInfo,
                                                                       fieldLayout.ownershipQualifier)) {
                                fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
                                fieldAlign = (TZrUInt32)ZR_ALIGN_SIZE;
                            } else {
                                fieldSize = calculate_type_size(cs, field->typeInfo);
                                fieldAlign = get_type_alignment(cs, field->typeInfo);
                            }
                        }
                    }
                    if (fieldSize == 0) {
                        fieldSize = ZR_ALIGN_SIZE;
                    }
                    if (fieldAlign == 0) {
                        fieldAlign = ZR_ALIGN_SIZE;
                    }

                    currentOffset = align_offset(currentOffset, fieldAlign);
                    fieldLayout.byteOffset = currentOffset;
                    fieldLayout.byteSize = fieldSize;
                    fieldLayout.byteAlign = fieldAlign;
                    currentOffset += fieldSize;
                    if (fieldAlign > variantAlign) {
                        variantAlign = fieldAlign;
                    }
                    ZrCore_Array_Push(cs->state, &variantLayout.fieldLayouts, &fieldLayout);
                }
            }

            variantLayout.payloadAlign = variantAlign;
            variantLayout.payloadSize = currentOffset > 0 ? align_offset(currentOffset, variantAlign) : 0;
            if (variantLayout.payloadSize > maxPayloadSize) {
                maxPayloadSize = variantLayout.payloadSize;
            }
            if (variantLayout.payloadAlign > maxPayloadAlign) {
                maxPayloadAlign = variantLayout.payloadAlign;
            }
            ZrCore_Array_Push(cs->state, &layoutInfo->variantLayouts, &variantLayout);
        }
    }

    layoutInfo->payloadSize = maxPayloadSize;
    layoutInfo->payloadOffset = maxPayloadSize > 0
                                        ? align_offset(layoutInfo->tagSize, maxPayloadAlign)
                                        : layoutInfo->tagSize;
    layoutInfo->layoutByteAlign = layoutInfo->tagAlign > maxPayloadAlign
                                          ? layoutInfo->tagAlign
                                          : maxPayloadAlign;
    layoutInfo->layoutByteSize = align_offset(layoutInfo->payloadOffset + maxPayloadSize,
                                              layoutInfo->layoutByteAlign);
}

static SZrUnionVariantLayout *compiler_union_layout_variant_at(SZrUnionLayoutInfo *layoutInfo, TZrSize index) {
    if (layoutInfo == ZR_NULL || !layoutInfo->variantLayouts.isValid ||
        index >= layoutInfo->variantLayouts.length) {
        return ZR_NULL;
    }

    return (SZrUnionVariantLayout *)ZrCore_Array_Get(&layoutInfo->variantLayouts, index);
}

static SZrUnionPayloadFieldLayout *compiler_union_layout_field_at(SZrUnionVariantLayout *variantLayout,
                                                                  TZrSize index) {
    if (variantLayout == ZR_NULL || !variantLayout->fieldLayouts.isValid ||
        index >= variantLayout->fieldLayouts.length) {
        return ZR_NULL;
    }

    return (SZrUnionPayloadFieldLayout *)ZrCore_Array_Get(&variantLayout->fieldLayouts, index);
}

static void compiler_union_layout_info_free(SZrCompilerState *cs, SZrUnionLayoutInfo *layoutInfo) {
    if (cs == ZR_NULL || layoutInfo == ZR_NULL || !layoutInfo->variantLayouts.isValid) {
        return;
    }

    for (TZrSize index = 0; index < layoutInfo->variantLayouts.length; index++) {
        SZrUnionVariantLayout *variantLayout =
                (SZrUnionVariantLayout *)ZrCore_Array_Get(&layoutInfo->variantLayouts, index);
        if (variantLayout != ZR_NULL && variantLayout->fieldLayouts.isValid) {
            ZrCore_Array_Free(cs->state, &variantLayout->fieldLayouts);
        }
    }
    ZrCore_Array_Free(cs->state, &layoutInfo->variantLayouts);
    memset(layoutInfo, 0, sizeof(*layoutInfo));
}

static TZrBool compiler_union_format_payload_storage_name(TZrSize index,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    int length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = snprintf(buffer, bufferSize, "__zr_unionPayload%u", (unsigned)index);
    if (length < 0) {
        return ZR_FALSE;
    }
    if ((TZrSize)length >= bufferSize) {
        buffer[bufferSize - 1] = '\0';
    }
    return ZR_TRUE;
}

static TZrBool compiler_union_set_payload_field_descriptor(SZrCompilerState *cs,
                                                           SZrObject *fieldObject,
                                                           SZrParameter *field,
                                                           TZrSize index,
                                                           const SZrUnionLayoutInfo *layoutInfo,
                                                           SZrUnionVariantLayout *variantLayout) {
    TZrChar storageName[ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH];
    SZrString *fieldName = ZR_NULL;
    SZrString *typeName = ZR_NULL;
    SZrUnionPayloadFieldLayout *fieldLayout;

    if (cs == ZR_NULL || fieldObject == ZR_NULL || field == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_union_format_payload_storage_name(index, storageName, sizeof(storageName))) {
        return ZR_FALSE;
    }

    if (field->name != ZR_NULL) {
        fieldName = field->name->name;
    }
    if (field->typeInfo != ZR_NULL) {
        typeName = extract_type_name_string(cs, field->typeInfo);
    }

    if (!extern_compiler_descriptor_set_int_field(cs, fieldObject, "index", (TZrInt64)index) ||
        !extern_compiler_descriptor_set_string_field(cs, fieldObject, "storageName", storageName) ||
        !extern_compiler_descriptor_set_int_field(cs,
                                                 fieldObject,
                                                 "passingMode",
                                                 (TZrInt64)field->passingMode)) {
        return ZR_FALSE;
    }

    fieldLayout = compiler_union_layout_field_at(variantLayout, index);
    if (layoutInfo != ZR_NULL && fieldLayout != ZR_NULL &&
        (!extern_compiler_descriptor_set_int_field(cs,
                                                  fieldObject,
                                                  "byteOffset",
                                                  (TZrInt64)(layoutInfo->payloadOffset + fieldLayout->byteOffset)) ||
         !extern_compiler_descriptor_set_int_field(cs,
                                                  fieldObject,
                                                  "byteSize",
                                                  (TZrInt64)fieldLayout->byteSize) ||
         !extern_compiler_descriptor_set_int_field(cs,
                                                  fieldObject,
                                                  "byteAlign",
                                                  (TZrInt64)fieldLayout->byteAlign) ||
         !extern_compiler_descriptor_set_int_field(cs,
                                                  fieldObject,
                                                  "ownershipQualifier",
                                                  (TZrInt64)fieldLayout->ownershipQualifier))) {
        return ZR_FALSE;
    }

    if (fieldName != ZR_NULL) {
        if (!extern_compiler_descriptor_set_string_object_field(cs, fieldObject, "name", fieldName)) {
            return ZR_FALSE;
        }
    } else if (!extern_compiler_descriptor_set_string_field(cs, fieldObject, "name", storageName)) {
        return ZR_FALSE;
    }

    if (typeName != ZR_NULL) {
        return extern_compiler_descriptor_set_string_object_field(cs, fieldObject, "type", typeName);
    }

    return extern_compiler_descriptor_set_string_field(cs, fieldObject, "type", "any");
}

static TZrBool compiler_union_build_variant_metadata_value(SZrCompilerState *cs,
                                                           SZrString *ownerTypeName,
                                                           SZrUnionVariant *variant,
                                                           TZrSize tag,
                                                           const SZrUnionLayoutInfo *layoutInfo,
                                                           SZrUnionVariantLayout *variantLayout,
                                                           SZrTypeValue *outValue) {
    SZrObject *metadataObject;
    SZrObject *payloadFieldsArray;
    SZrTypeValue payloadFieldsValue;
    TZrSize payloadFieldCount;
    ZrExternCompilerTempRoot metadataRoot = {0};
    ZrExternCompilerTempRoot payloadFieldsRoot = {0};
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || ownerTypeName == ZR_NULL || variant == ZR_NULL ||
        variant->name == ZR_NULL || variant->name->name == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    payloadFieldCount = variant->fields != ZR_NULL ? variant->fields->count : 0;
    ZrCore_Value_ResetAsNull(outValue);

    if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
        !extern_compiler_temp_root_begin(cs, &payloadFieldsRoot)) {
        goto cleanup;
    }

    metadataObject = extern_compiler_new_object_constant(cs);
    payloadFieldsArray = extern_compiler_new_array_constant(cs);
    if (metadataObject == ZR_NULL || payloadFieldsArray == ZR_NULL) {
        goto cleanup;
    }

    if (!extern_compiler_temp_root_set_object(&metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !extern_compiler_temp_root_set_object(&payloadFieldsRoot, payloadFieldsArray, ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }

    if (!extern_compiler_descriptor_set_string_field(cs, metadataObject, "kind", "unionVariant") ||
        !extern_compiler_descriptor_set_string_object_field(cs, metadataObject, "ownerType", ownerTypeName) ||
        !extern_compiler_descriptor_set_string_object_field(cs, metadataObject, "variantName", variant->name->name) ||
        !extern_compiler_descriptor_set_int_field(cs, metadataObject, "tag", (TZrInt64)tag) ||
        !extern_compiler_descriptor_set_int_field(cs, metadataObject, "variantKind", (TZrInt64)variant->kind) ||
        !extern_compiler_descriptor_set_int_field(cs,
                                                 metadataObject,
                                                 "payloadFieldCount",
                                                 (TZrInt64)payloadFieldCount)) {
        goto cleanup;
    }

    if (layoutInfo != ZR_NULL && variantLayout != ZR_NULL &&
        (!extern_compiler_descriptor_set_int_field(cs, metadataObject, "tagSize", layoutInfo->tagSize) ||
         !extern_compiler_descriptor_set_int_field(cs, metadataObject, "payloadOffset", layoutInfo->payloadOffset) ||
         !extern_compiler_descriptor_set_int_field(cs, metadataObject, "layoutByteSize", layoutInfo->layoutByteSize) ||
         !extern_compiler_descriptor_set_int_field(cs, metadataObject, "layoutByteAlign", layoutInfo->layoutByteAlign) ||
         !extern_compiler_descriptor_set_int_field(cs,
                                                  metadataObject,
                                                  "variantPayloadSize",
                                                  variantLayout->payloadSize) ||
         !extern_compiler_descriptor_set_int_field(cs,
                                                  metadataObject,
                                                  "variantPayloadAlign",
                                                  variantLayout->payloadAlign))) {
        goto cleanup;
    }

    if (variant->fields != ZR_NULL) {
        for (TZrSize index = 0; index < variant->fields->count; index++) {
            SZrAstNode *fieldNode = variant->fields->nodes[index];
            SZrObject *fieldObject;
            SZrTypeValue fieldObjectValue;
            ZrExternCompilerTempRoot fieldRoot = {0};
            TZrBool fieldSuccess = ZR_FALSE;

            if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (!extern_compiler_temp_root_begin(cs, &fieldRoot)) {
                goto cleanup;
            }

            fieldObject = extern_compiler_new_object_constant(cs);
            if (fieldObject != ZR_NULL &&
                extern_compiler_temp_root_set_object(&fieldRoot, fieldObject, ZR_VALUE_TYPE_OBJECT) &&
                compiler_union_set_payload_field_descriptor(cs,
                                                            fieldObject,
                                                            &fieldNode->data.parameter,
                                                            index,
                                                            layoutInfo,
                                                            variantLayout)) {
                ZrCore_Value_InitAsRawObject(cs->state,
                                             &fieldObjectValue,
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
                fieldObjectValue.type = ZR_VALUE_TYPE_OBJECT;
                fieldSuccess = extern_compiler_push_array_value(cs, payloadFieldsArray, &fieldObjectValue);
            }

            extern_compiler_temp_root_end(&fieldRoot);
            if (!fieldSuccess) {
                goto cleanup;
            }
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state,
                                 &payloadFieldsValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(payloadFieldsArray));
    payloadFieldsValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, metadataObject, "payloadFields", &payloadFieldsValue)) {
        goto cleanup;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    success = ZR_TRUE;

cleanup:
    if (payloadFieldsRoot.active) {
        extern_compiler_temp_root_end(&payloadFieldsRoot);
    }
    if (metadataRoot.active) {
        extern_compiler_temp_root_end(&metadataRoot);
    }
    return success;
}

void compile_union_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrUnionDeclaration *unionDecl;
    SZrString *typeName;
    SZrString *oldTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo;
    SZrAstNode *oldTypeNode;
    SZrTypePrototypeInfo info;
    SZrUnionLayoutInfo layoutInfo;
    TZrBool layoutInfoInitialized = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_UNION_DECLARATION) {
        ZrParser_Statement_Compile(cs, node);
        return;
    }

    unionDecl = &node->data.unionDeclaration;
    if (unionDecl->name == ZR_NULL || unionDecl->name->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Union declaration must have a valid name", node->location);
        return;
    }

    typeName = unionDecl->name->name;
    oldTypeName = cs->currentTypeName;
    oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    oldTypeNode = cs->currentTypeNode;
    cs->currentTypeName = typeName;
    cs->currentTypeNode = node;

    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_UNION;
    info.accessModifier = unionDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    compiler_union_layout_info_init(cs, unionDecl, &layoutInfo);
    layoutInfoInitialized = ZR_TRUE;
    info.layoutByteSize = layoutInfo.layoutByteSize;
    info.layoutByteAlign = layoutInfo.layoutByteAlign;
    ZrCore_Value_ResetAsNull(&info.decoratorMetadataValue);

    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state,
                      &info.genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      unionDecl->generic != ZR_NULL && unionDecl->generic->params != ZR_NULL
                              ? unionDecl->generic->params->count
                              : 1);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    compiler_collect_generic_parameter_info(cs, &info.genericParameters, unionDecl->generic);

    cs->currentTypePrototypeInfo = &info;
    if (unionDecl->variants != ZR_NULL) {
        for (TZrSize index = 0; index < unionDecl->variants->count; index++) {
            SZrAstNode *variantNode = unionDecl->variants->nodes[index];
            SZrUnionVariant *variant;
            SZrTypeMemberInfo memberInfo;
            SZrUnionVariantLayout *variantLayout;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT) {
                continue;
            }
            variant = &variantNode->data.unionVariant;
            if (variant->name == ZR_NULL || variant->name->name == ZR_NULL) {
                continue;
            }
            variantLayout = compiler_union_layout_variant_at(&layoutInfo, index);

            compiler_union_init_member_defaults(&memberInfo);
            memberInfo.declarationNode = variantNode;
            memberInfo.name = variant->name->name;
            memberInfo.accessModifier = unionDecl->accessModifier;
            memberInfo.declarationOrder = (TZrUInt32)index;
            memberInfo.fieldTypeName = typeName;
            memberInfo.returnTypeName = typeName;
            memberInfo.fieldOffset = (TZrUInt32)index;
            memberInfo.fieldSize = (TZrUInt32)variant->kind;
            memberInfo.ownerTypeName = typeName;
            memberInfo.baseDefinitionOwnerTypeName = typeName;
            memberInfo.baseDefinitionName = memberInfo.name;
            compiler_union_collect_payload_metadata(cs, &memberInfo, variant->fields);
            if (!compiler_union_build_variant_metadata_value(cs,
                                                             typeName,
                                                             variant,
                                                             index,
                                                             &layoutInfo,
                                                             variantLayout,
                                                             &memberInfo.decoratorMetadataValue)) {
                ZrParser_Compiler_Error(cs, "failed to build union variant metadata", variantNode->location);
                break;
            }
            memberInfo.hasDecoratorMetadata = ZR_TRUE;

            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    if (cs->hasError) {
        if (layoutInfoInitialized) {
            compiler_union_layout_info_free(cs, &layoutInfo);
        }
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }

    if (layoutInfoInitialized) {
        compiler_union_layout_info_free(cs, &layoutInfo);
    }

    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
    cs->currentTypeNode = oldTypeNode;
}
