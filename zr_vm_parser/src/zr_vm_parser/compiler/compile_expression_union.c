#include "compile_expression_internal.h"

static SZrString *union_constructor_lookup_name(SZrCompilerState *cs, SZrString *typeName) {
    const TZrChar *typeText;
    const TZrChar *genericStart;
    TZrSize baseLength;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeText = ZrCore_String_GetNativeString(typeName);
    if (typeText == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(typeText, '<');
    if (genericStart == ZR_NULL) {
        return typeName;
    }

    baseLength = (TZrSize)(genericStart - typeText);
    while (baseLength > 0 &&
           (typeText[baseLength - 1] == ' ' || typeText[baseLength - 1] == '\t' ||
            typeText[baseLength - 1] == '\r' || typeText[baseLength - 1] == '\n')) {
        baseLength--;
    }

    return baseLength > 0 ? ZrCore_String_Create(cs->state, (TZrNativeString)typeText, baseLength) : ZR_NULL;
}

static TZrBool union_constructor_target_type_name(SZrCompilerState *cs,
                                                  SZrAstNode *property,
                                                  SZrString **outTypeName) {
    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || property == ZR_NULL || outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (property->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outTypeName = property->data.identifier.name;
        return *outTypeName != ZR_NULL;
    }

    if (property->type == ZR_AST_TYPE) {
        *outTypeName = extract_type_name_string(cs, &property->data.type);
        return *outTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static SZrAstNode *find_union_declaration_for_constructor_type(SZrCompilerState *cs, SZrString *typeName) {
    SZrString *lookupName;
    SZrAstNode *declaration;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    lookupName = union_constructor_lookup_name(cs, typeName);
    if (lookupName == ZR_NULL) {
        return ZR_NULL;
    }

    declaration = find_type_declaration(cs, lookupName);
    return declaration != ZR_NULL && declaration->type == ZR_AST_UNION_DECLARATION ? declaration : ZR_NULL;
}

SZrAstNode *try_find_union_declaration_for_type_name(SZrCompilerState *cs, SZrString *typeName) {
    return find_union_declaration_for_constructor_type(cs, typeName);
}

static SZrString *union_variant_member_name(SZrAstNode *memberNode) {
    if (memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return memberNode->data.memberExpression.property->data.identifier.name;
}

static SZrAstNode *find_union_variant_node(SZrAstNode *unionDeclaration, SZrString *variantName) {
    SZrAstNodeArray *variants;

    if (unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION ||
        variantName == ZR_NULL) {
        return ZR_NULL;
    }

    variants = unionDeclaration->data.unionDeclaration.variants;
    if (variants == ZR_NULL || variants->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < variants->count; index++) {
        SZrAstNode *variant = variants->nodes[index];

        if (variant != ZR_NULL &&
            variant->type == ZR_AST_UNION_VARIANT &&
            variant->data.unionVariant.name != ZR_NULL &&
            variant->data.unionVariant.name->name != ZR_NULL &&
            ZrCore_String_Equal(variant->data.unionVariant.name->name, variantName)) {
            return variant;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *find_union_default_using_variant_node(SZrAstNode *unionDeclaration) {
    SZrAstNodeArray *variants;

    if (unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION) {
        return ZR_NULL;
    }

    variants = unionDeclaration->data.unionDeclaration.variants;
    if (variants == ZR_NULL || variants->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < variants->count; index++) {
        SZrAstNode *variant = variants->nodes[index];

        if (variant != ZR_NULL &&
            variant->type == ZR_AST_UNION_VARIANT &&
            variant->data.unionVariant.isDefaultUsingVariant) {
            return variant;
        }
    }

    return ZR_NULL;
}

static SZrString *union_variant_node_name(SZrAstNode *variant) {
    if (variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        variant->data.unionVariant.name == ZR_NULL) {
        return ZR_NULL;
    }

    return variant->data.unionVariant.name->name;
}

static const TZrChar *find_top_level_last_dot(const TZrChar *text) {
    const TZrChar *lastDot = ZR_NULL;
    TZrInt32 genericDepth = 0;

    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    for (const TZrChar *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            genericDepth++;
        } else if (*cursor == '>' && genericDepth > 0) {
            genericDepth--;
        } else if (*cursor == '.' && genericDepth == 0) {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static TZrBool split_union_variant_type_annotation(SZrCompilerState *cs,
                                                   SZrType *variantTypeInfo,
                                                   SZrString **outUnionTypeName,
                                                   SZrString **outVariantName) {
    SZrString *fullName;
    const TZrChar *fullText;
    const TZrChar *lastDot;

    if (outUnionTypeName != ZR_NULL) {
        *outUnionTypeName = ZR_NULL;
    }
    if (outVariantName != ZR_NULL) {
        *outVariantName = ZR_NULL;
    }
    if (cs == ZR_NULL || variantTypeInfo == ZR_NULL ||
        outUnionTypeName == ZR_NULL || outVariantName == ZR_NULL) {
        return ZR_FALSE;
    }

    fullName = extract_type_name_string(cs, variantTypeInfo);
    fullText = fullName != ZR_NULL ? ZrCore_String_GetNativeString(fullName) : ZR_NULL;
    lastDot = find_top_level_last_dot(fullText);
    if (fullText == ZR_NULL || lastDot == ZR_NULL || lastDot == fullText || lastDot[1] == '\0') {
        return ZR_FALSE;
    }

    *outUnionTypeName = ZrCore_String_Create(cs->state,
                                             (TZrNativeString)fullText,
                                             (TZrSize)(lastDot - fullText));
    *outVariantName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)(lastDot + 1));
    return *outUnionTypeName != ZR_NULL && *outVariantName != ZR_NULL;
}

static TZrSize union_variant_field_count(SZrAstNode *variant) {
    if (variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        variant->data.unionVariant.fields == ZR_NULL) {
        return 0;
    }

    return variant->data.unionVariant.fields->count;
}

static SZrString *union_struct_payload_property_name(SZrAstNode *propertyNode) {
    SZrKeyValuePair *pair;

    if (propertyNode == ZR_NULL ||
        propertyNode->type != ZR_AST_KEY_VALUE_PAIR) {
        return ZR_NULL;
    }

    pair = &propertyNode->data.keyValuePair;
    if (pair->keyIsComputed ||
        pair->key == ZR_NULL ||
        pair->key->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return pair->key->data.identifier.name;
}

static SZrParameter *union_variant_field_parameter(SZrAstNode *fieldNode) {
    if (fieldNode == ZR_NULL || fieldNode->type != ZR_AST_PARAMETER) {
        return ZR_NULL;
    }

    return &fieldNode->data.parameter;
}

static SZrParameter *union_variant_payload_field_parameter(SZrAstNode *variant, TZrSize payloadIndex) {
    SZrAstNodeArray *fields;

    if (variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        variant->data.unionVariant.fields == ZR_NULL ||
        payloadIndex >= variant->data.unionVariant.fields->count) {
        return ZR_NULL;
    }

    fields = variant->data.unionVariant.fields;
    return fields->nodes != ZR_NULL ? union_variant_field_parameter(fields->nodes[payloadIndex]) : ZR_NULL;
}

static EZrOwnershipQualifier union_variant_payload_declared_qualifier(SZrAstNode *variant,
                                                                      TZrSize payloadIndex) {
    SZrParameter *field = union_variant_payload_field_parameter(variant, payloadIndex);

    if (field == ZR_NULL || field->typeInfo == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    return field->typeInfo->ownershipQualifier;
}

EZrOwnershipQualifier union_variant_payload_default_binding_qualifier(SZrAstNode *variant,
                                                                      TZrSize payloadIndex,
                                                                      TZrBool moveBinding) {
    EZrOwnershipQualifier qualifier = union_variant_payload_declared_qualifier(variant, payloadIndex);

    if (moveBinding) {
        return qualifier;
    }

    if (qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        return ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }

    return qualifier;
}

TZrBool union_variant_payload_binding_defaults_to_borrow(SZrAstNode *variant,
                                                         TZrSize payloadIndex,
                                                         TZrBool moveBinding) {
    EZrOwnershipQualifier qualifier = union_variant_payload_declared_qualifier(variant, payloadIndex);

    if (moveBinding) {
        return ZR_FALSE;
    }

    return (TZrBool)(qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
                    qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                    qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED);
}

static SZrAstNode *find_union_struct_payload_property(SZrObjectLiteral *objectLiteral, SZrString *fieldName) {
    if (objectLiteral == ZR_NULL ||
        objectLiteral->properties == ZR_NULL ||
        objectLiteral->properties->nodes == ZR_NULL ||
        fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < objectLiteral->properties->count; index++) {
        SZrAstNode *propertyNode = objectLiteral->properties->nodes[index];
        SZrString *propertyName = union_struct_payload_property_name(propertyNode);

        if (propertyName != ZR_NULL && ZrCore_String_Equal(propertyName, fieldName)) {
            return propertyNode;
        }
    }

    return ZR_NULL;
}

static TZrBool union_variant_has_field_named(SZrAstNode *variant, SZrString *fieldName) {
    SZrAstNodeArray *fields;

    if (variant == ZR_NULL || variant->type != ZR_AST_UNION_VARIANT || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    fields = variant->data.unionVariant.fields;
    if (fields == ZR_NULL || fields->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < fields->count; index++) {
        SZrParameter *field = union_variant_field_parameter(fields->nodes[index]);

        if (field != ZR_NULL &&
            field->name != ZR_NULL &&
            field->name->name != ZR_NULL &&
            ZrCore_String_Equal(field->name->name, fieldName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const TZrChar *union_string_text(SZrString *value) {
    const TZrChar *text;

    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text != ZR_NULL) {
        return text;
    }

    return ZrCore_String_GetNativeStringShort(value);
}

static const TZrChar *union_variant_available_field_names(SZrCompilerState *cs,
                                                          SZrAstNode *variant,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    SZrAstNodeArray *fields;
    TZrSize offset = 0;

    ZR_UNUSED_PARAMETER(cs);

    if (buffer == ZR_NULL || bufferSize == 0) {
        return "";
    }
    buffer[0] = '\0';

    if (variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        variant->data.unionVariant.fields == ZR_NULL ||
        variant->data.unionVariant.fields->nodes == ZR_NULL) {
        return buffer;
    }

    fields = variant->data.unionVariant.fields;
    for (TZrSize index = 0; index < fields->count && offset + 1 < bufferSize; index++) {
        SZrParameter *field = union_variant_field_parameter(fields->nodes[index]);
        const TZrChar *fieldName;
        TZrSize fieldNameLength;
        TZrSize separatorLength;

        if (field == ZR_NULL || field->name == ZR_NULL || field->name->name == ZR_NULL) {
            continue;
        }

        fieldName = union_string_text(field->name->name);
        if (fieldName == ZR_NULL) {
            continue;
        }

        separatorLength = offset > 0 ? 2 : 0;
        fieldNameLength = strlen(fieldName);
        if (offset + separatorLength + fieldNameLength + 1 >= bufferSize) {
            break;
        }
        if (separatorLength > 0) {
            buffer[offset++] = ',';
            buffer[offset++] = ' ';
        }
        memcpy(buffer + offset, fieldName, fieldNameLength);
        offset += fieldNameLength;
        buffer[offset] = '\0';
    }

    return buffer;
}

static TZrBool union_struct_payload_has_duplicate_before(SZrObjectLiteral *objectLiteral,
                                                         TZrSize propertyIndex,
                                                         SZrString *propertyName) {
    if (objectLiteral == ZR_NULL ||
        objectLiteral->properties == ZR_NULL ||
        objectLiteral->properties->nodes == ZR_NULL ||
        propertyName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < propertyIndex; index++) {
        SZrString *previousName = union_struct_payload_property_name(objectLiteral->properties->nodes[index]);

        if (previousName != ZR_NULL && ZrCore_String_Equal(previousName, propertyName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool validate_union_struct_payload(SZrCompilerState *cs,
                                             SZrAstNode *variant,
                                             SZrObjectLiteral *objectLiteral,
                                             SZrFileRange location) {
    SZrAstNodeArray *fields;
    TZrSize expectedCount;
    TZrSize actualCount;

    if (cs == ZR_NULL || variant == ZR_NULL || variant->type != ZR_AST_UNION_VARIANT ||
        objectLiteral == ZR_NULL) {
        return ZR_FALSE;
    }

    if (variant->data.unionVariant.kind != ZR_UNION_VARIANT_STRUCT) {
        ZrParser_Compiler_Error(cs, "Object payload syntax is only valid for struct union variants", location);
        return ZR_FALSE;
    }

    fields = variant->data.unionVariant.fields;
    expectedCount = fields != ZR_NULL ? fields->count : 0;
    actualCount = objectLiteral->properties != ZR_NULL ? objectLiteral->properties->count : 0;
    if (expectedCount != actualCount) {
        ZrParser_Compiler_Error(cs, "Union struct variant field count mismatch", location);
        return ZR_FALSE;
    }

    if (objectLiteral->properties != ZR_NULL && objectLiteral->properties->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < objectLiteral->properties->count; index++) {
            SZrAstNode *propertyNode = objectLiteral->properties->nodes[index];
            SZrString *propertyName = union_struct_payload_property_name(propertyNode);

            if (propertyName == ZR_NULL) {
                ZrParser_Compiler_Error(cs,
                                        "Union struct variant payload fields must use identifier keys",
                                        propertyNode != ZR_NULL ? propertyNode->location : location);
                return ZR_FALSE;
            }
            if (union_struct_payload_has_duplicate_before(objectLiteral, index, propertyName)) {
                ZrParser_Compiler_Error(cs, "Duplicate union struct variant payload field",
                                        propertyNode->location);
                return ZR_FALSE;
            }
            if (!union_variant_has_field_named(variant, propertyName)) {
                ZrParser_Compiler_Error(cs, "Unknown union struct variant payload field",
                                        propertyNode->location);
                return ZR_FALSE;
            }
        }
    }

    if (fields == ZR_NULL || fields->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < fields->count; index++) {
        SZrParameter *field = union_variant_field_parameter(fields->nodes[index]);

        if (field == ZR_NULL ||
            field->name == ZR_NULL ||
            field->name->name == ZR_NULL ||
            find_union_struct_payload_property(objectLiteral, field->name->name) == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Missing union struct variant payload field", location);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool build_union_struct_pattern_bindings(SZrCompilerState *cs,
                                                   SZrAstNode *variant,
                                                   SZrObjectLiteral *objectLiteral,
                                                   SZrAstNodeArray **outBindings,
                                                   SZrFileRange location) {
    SZrAstNodeArray *fields;
    SZrAstNodeArray *bindings;
    TZrSize expectedCount;
    TZrSize actualCount;

    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (cs == ZR_NULL || variant == ZR_NULL || variant->type != ZR_AST_UNION_VARIANT ||
        objectLiteral == ZR_NULL || outBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    if (variant->data.unionVariant.kind != ZR_UNION_VARIANT_STRUCT) {
        ZrParser_Compiler_PatternShapeMismatch(
                cs,
                location,
                "Object destructuring is only valid for struct union patterns",
                "The selected union variant has positional payload fields, so the pattern must use tuple destructuring.",
                "Use tuple destructuring such as `var [value]: Union.Variant = resource`.");
        return ZR_FALSE;
    }

    fields = variant->data.unionVariant.fields;
    expectedCount = fields != ZR_NULL ? fields->count : 0;
    actualCount = objectLiteral->properties != ZR_NULL ? objectLiteral->properties->count : 0;

    if (objectLiteral->properties != ZR_NULL && objectLiteral->properties->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < objectLiteral->properties->count; index++) {
            SZrAstNode *propertyNode = objectLiteral->properties->nodes[index];
            SZrString *propertyName = union_struct_payload_property_name(propertyNode);
            SZrAstNode *bindingNode;

            if (propertyName == ZR_NULL) {
                ZrParser_Compiler_Error(cs,
                                        "Union struct pattern fields must use identifier keys",
                                        propertyNode != ZR_NULL ? propertyNode->location : location);
                return ZR_FALSE;
            }
            if (union_struct_payload_has_duplicate_before(objectLiteral, index, propertyName)) {
                ZrParser_Compiler_Error(cs, "Duplicate union struct pattern field",
                                        propertyNode->location);
                return ZR_FALSE;
            }
            if (!union_variant_has_field_named(variant, propertyName)) {
                TZrChar availableFields[256];
                ZrParser_Compiler_PatternUnknownField(
                        cs,
                        propertyNode->location,
                        union_string_text(propertyName),
                        union_variant_available_field_names(cs,
                                                            variant,
                                                            availableFields,
                                                            sizeof(availableFields)));
                return ZR_FALSE;
            }

            bindingNode = propertyNode->data.keyValuePair.value;
            if (bindingNode == ZR_NULL || bindingNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                ZrParser_Compiler_Error(cs,
                                        "Union struct pattern binding must be an identifier",
                                        bindingNode != ZR_NULL ? bindingNode->location : propertyNode->location);
                return ZR_FALSE;
            }
        }
    }

    if (expectedCount != actualCount) {
        TZrChar availableFields[256];
        ZrParser_Compiler_PatternArityMismatch(
                cs,
                location,
                expectedCount,
                actualCount,
                union_variant_available_field_names(cs,
                                                    variant,
                                                    availableFields,
                                                    sizeof(availableFields)));
        return ZR_FALSE;
    }

    bindings = ZrParser_AstNodeArray_New(cs->state, expectedCount);
    if (bindings == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate union struct pattern bindings", location);
        return ZR_FALSE;
    }

    if (fields != ZR_NULL && fields->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < fields->count; index++) {
            SZrParameter *field = union_variant_field_parameter(fields->nodes[index]);
            SZrAstNode *propertyNode;

            if (field == ZR_NULL ||
                field->name == ZR_NULL ||
                field->name->name == ZR_NULL) {
                ZrParser_Compiler_Error(cs, "Invalid union struct variant field", location);
                return ZR_FALSE;
            }

            propertyNode = find_union_struct_payload_property(objectLiteral, field->name->name);
            if (propertyNode == ZR_NULL || propertyNode->data.keyValuePair.value == ZR_NULL) {
                ZrParser_Compiler_Error(cs, "Missing union struct pattern field", location);
                return ZR_FALSE;
            }

            ZrParser_AstNodeArray_Add(cs->state, bindings, propertyNode->data.keyValuePair.value);
        }
    }

    *outBindings = bindings;
    return ZR_TRUE;
}

static TZrBool emit_union_carrier_string_member(SZrCompilerState *cs,
                                                TZrUInt32 destSlot,
                                                const TZrChar *memberNameText,
                                                SZrString *value,
                                                SZrFileRange location) {
    SZrString *memberName;
    TZrUInt32 valueSlot;
    TZrUInt32 memberId;

    if (cs == ZR_NULL || cs->hasError || memberNameText == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    memberName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)memberNameText);
    if (memberName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate union carrier member name", location);
        return ZR_FALSE;
    }

    valueSlot = emit_string_constant(cs, value);
    if (valueSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to emit union carrier string value", location);
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry(cs, memberName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        ZrParser_Compiler_Error(cs, "Failed to register union carrier member", location);
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER),
                                          (TZrUInt16)valueSlot,
                                          (TZrUInt16)destSlot,
                                          (TZrUInt16)memberId));
    collapse_stack_to_slot(cs, destSlot);
    return !cs->hasError;
}

static TZrBool emit_union_carrier_payload_member(SZrCompilerState *cs,
                                                 TZrUInt32 destSlot,
                                                 TZrSize payloadIndex,
                                                 SZrAstNode *argument,
                                                 SZrFileRange location) {
    TZrChar memberNameBuffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrInt32 written;
    SZrString *memberName;
    TZrUInt32 valueSlot;
    TZrUInt32 memberId;

    if (cs == ZR_NULL || cs->hasError || argument == ZR_NULL) {
        return ZR_FALSE;
    }

    written = snprintf(memberNameBuffer,
                       sizeof(memberNameBuffer),
                       "__zr_unionPayload%u",
                       (unsigned)payloadIndex);
    if (written <= 0 || (TZrSize)written >= sizeof(memberNameBuffer)) {
        ZrParser_Compiler_Error(cs, "Union payload member name is too long", location);
        return ZR_FALSE;
    }

    memberName = ZrCore_String_Create(cs->state, memberNameBuffer, (TZrSize)written);
    if (memberName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate union payload member name", location);
        return ZR_FALSE;
    }

    valueSlot = compile_expression_into_slot(cs, argument, destSlot + 1u);
    if (valueSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile union variant payload", location);
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry(cs, memberName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        ZrParser_Compiler_Error(cs, "Failed to register union payload member", location);
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER),
                                          (TZrUInt16)valueSlot,
                                          (TZrUInt16)destSlot,
                                          (TZrUInt16)memberId));
    if (valueSlot == destSlot + 1u) {
        emit_instruction(cs,
                         create_instruction_0(ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                                              (TZrUInt16)valueSlot));
    }
    collapse_stack_to_slot(cs, destSlot);
    return !cs->hasError;
}

static TZrBool emit_union_variant_carrier(SZrCompilerState *cs,
                                          SZrString *typeName,
                                          SZrString *variantName,
                                          SZrAstNode *variant,
                                          SZrFunctionCall *call,
                                          SZrObjectLiteral *objectLiteral,
                                          TZrUInt32 *outSlot,
                                          SZrFileRange location) {
    TZrUInt32 destSlot;

    if (outSlot != ZR_NULL) {
        *outSlot = ZR_PARSER_SLOT_NONE;
    }
    if (cs == ZR_NULL || cs->hasError || typeName == ZR_NULL || variantName == ZR_NULL) {
        return ZR_FALSE;
    }

    destSlot = allocate_stack_slot(cs);
    emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)destSlot));

    if (!emit_union_carrier_string_member(cs, destSlot, "__zr_unionType", typeName, location) ||
        !emit_union_carrier_string_member(cs, destSlot, "__zr_unionVariant", variantName, location)) {
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->args != ZR_NULL) {
        for (TZrSize index = 0; index < call->args->count; index++) {
            if (!emit_union_carrier_payload_member(cs,
                                                   destSlot,
                                                   index,
                                                   call->args->nodes[index],
                                                   call->args->nodes[index] != ZR_NULL
                                                           ? call->args->nodes[index]->location
                                                           : location)) {
                return ZR_FALSE;
            }
        }
    }

    if (objectLiteral != ZR_NULL && variant != ZR_NULL && variant->data.unionVariant.fields != ZR_NULL) {
        SZrAstNodeArray *fields = variant->data.unionVariant.fields;

        for (TZrSize index = 0; index < fields->count; index++) {
            SZrParameter *field = union_variant_field_parameter(fields->nodes[index]);
            SZrAstNode *propertyNode;

            if (field == ZR_NULL || field->name == ZR_NULL || field->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            propertyNode = find_union_struct_payload_property(objectLiteral, field->name->name);
            if (propertyNode == ZR_NULL || propertyNode->data.keyValuePair.value == ZR_NULL) {
                return ZR_FALSE;
            }

            if (!emit_union_carrier_payload_member(cs,
                                                   destSlot,
                                                   index,
                                                   propertyNode->data.keyValuePair.value,
                                                   propertyNode->data.keyValuePair.value->location)) {
                return ZR_FALSE;
            }
        }
    }

    collapse_stack_to_slot(cs, destSlot);
    cs->lastExpressionSlot = destSlot;
    if (outSlot != ZR_NULL) {
        *outSlot = destSlot;
    }
    return ZR_TRUE;
}

TZrBool try_resolve_union_variant_reference_expression(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrString **outVariantName) {
    SZrAstNodeArray *bindings = ZR_NULL;

    return try_resolve_union_variant_pattern_expression(cs, node, outVariantName, &bindings, ZR_NULL);
}

TZrBool try_resolve_union_variant_pattern_expression(SZrCompilerState *cs,
                                                     SZrAstNode *node,
                                                     SZrString **outVariantName,
                                                     SZrAstNodeArray **outBindings,
                                                     SZrAstNode **outVariant) {
    SZrPrimaryExpression *primary;
    SZrString *typeName = ZR_NULL;
    SZrAstNode *unionDeclaration;
    SZrString *variantName;
    SZrAstNode *variant;
    SZrFunctionCall *call = ZR_NULL;
    SZrAstNodeArray *structBindings = ZR_NULL;
    TZrSize expectedBindingCount;
    TZrSize actualBindingCount = 0;

    if (outVariantName != ZR_NULL) {
        *outVariantName = ZR_NULL;
    }
    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (outVariant != ZR_NULL) {
        *outVariant = ZR_NULL;
    }
    if (cs == ZR_NULL || node == ZR_NULL || outVariantName == ZR_NULL || outBindings == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION || cs->hasError) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->members == ZR_NULL ||
        primary->members->count == 0 ||
        primary->members->count > 2 ||
        !union_constructor_target_type_name(cs, primary->property, &typeName) ||
        typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    unionDeclaration = find_union_declaration_for_constructor_type(cs, typeName);
    if (unionDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    variantName = union_variant_member_name(primary->members->nodes[0]);
    if (variantName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Union variant reference must use a named variant", node->location);
        return ZR_TRUE;
    }

    variant = find_union_variant_node(unionDeclaration, variantName);
    if (variant == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Unknown union variant", primary->members->nodes[0]->location);
        return ZR_TRUE;
    }

    if (primary->members->count == 2) {
        SZrAstNode *payloadNode = primary->members->nodes[1];

        if (payloadNode == ZR_NULL) {
            return ZR_FALSE;
        }
        if (payloadNode->type == ZR_AST_FUNCTION_CALL) {
            call = &payloadNode->data.functionCall;
            actualBindingCount = call->args != ZR_NULL ? call->args->count : 0;
            if (variant->data.unionVariant.kind == ZR_UNION_VARIANT_STRUCT) {
                ZrParser_Compiler_PatternShapeMismatch(
                        cs,
                        payloadNode->location,
                        "Struct union switch patterns require object payload syntax",
                        "The selected union variant exposes named fields, so positional payload syntax cannot bind it.",
                        "Use object payload syntax such as `Variant { local: field }`.");
                return ZR_TRUE;
            }
            if (call->hasNamedArgs) {
                ZrParser_Compiler_Error(cs, "Union switch pattern bindings must be positional", payloadNode->location);
                return ZR_TRUE;
            }
            for (TZrSize index = 0; index < actualBindingCount; index++) {
                SZrAstNode *bindingNode = call->args->nodes[index];
                if (bindingNode == ZR_NULL || bindingNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                    ZrParser_Compiler_Error(cs,
                                            "Union switch pattern binding must be an identifier",
                                            bindingNode != ZR_NULL ? bindingNode->location : payloadNode->location);
                    return ZR_TRUE;
                }
            }
        } else if (payloadNode->type == ZR_AST_OBJECT_LITERAL) {
            if (!build_union_struct_pattern_bindings(cs,
                                                     variant,
                                                     &payloadNode->data.objectLiteral,
                                                     &structBindings,
                                                     payloadNode->location)) {
                return ZR_TRUE;
            }
            actualBindingCount = structBindings != ZR_NULL ? structBindings->count : 0;
        } else {
            return ZR_FALSE;
        }
    }

    expectedBindingCount = union_variant_field_count(variant);
    if ((call != ZR_NULL || structBindings != ZR_NULL) && expectedBindingCount != actualBindingCount) {
        TZrChar availableFields[256];
        ZrParser_Compiler_PatternArityMismatch(
                cs,
                node->location,
                expectedBindingCount,
                actualBindingCount,
                structBindings != ZR_NULL
                    ? union_variant_available_field_names(cs,
                                                          variant,
                                                          availableFields,
                                                          sizeof(availableFields))
                    : ZR_NULL);
        return ZR_TRUE;
    }

    *outVariantName = variantName;
    if (outVariant != ZR_NULL) {
        *outVariant = variant;
    }
    if (call != ZR_NULL) {
        *outBindings = call->args;
    } else if (structBindings != ZR_NULL) {
        *outBindings = structBindings;
    }
    return ZR_TRUE;
}

TZrBool try_resolve_union_variant_pattern_for_type(SZrCompilerState *cs,
                                                   SZrAstNode *node,
                                                   SZrString *typeName,
                                                   SZrString **outVariantName,
                                                   SZrAstNodeArray **outBindings,
                                                   SZrAstNode **outVariant) {
    SZrAstNode *unionDeclaration;
    SZrString *variantName = ZR_NULL;
    SZrAstNode *variant;
    SZrAstNode *payloadNode = ZR_NULL;
    SZrFunctionCall *call = ZR_NULL;
    SZrAstNodeArray *structBindings = ZR_NULL;
    TZrSize expectedBindingCount;
    TZrSize actualBindingCount = 0;

    if (outVariantName != ZR_NULL) {
        *outVariantName = ZR_NULL;
    }
    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (outVariant != ZR_NULL) {
        *outVariant = ZR_NULL;
    }
    if (cs == ZR_NULL || node == ZR_NULL || typeName == ZR_NULL ||
        outVariantName == ZR_NULL || outBindings == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (try_resolve_union_variant_pattern_expression(cs, node, outVariantName, outBindings, outVariant) ||
        cs->hasError) {
        return ZR_TRUE;
    }

    unionDeclaration = find_union_declaration_for_constructor_type(cs, typeName);
    if (unionDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_DESTRUCTURING_ARRAY ||
        node->type == ZR_AST_OBJECT_LITERAL) {
        variant = find_union_default_using_variant_node(unionDeclaration);
        variantName = union_variant_node_name(variant);
        if (variant == ZR_NULL || variantName == ZR_NULL) {
            return ZR_FALSE;
        }

        expectedBindingCount = union_variant_field_count(variant);
        if (node->type == ZR_AST_DESTRUCTURING_ARRAY) {
            if (variant->data.unionVariant.kind == ZR_UNION_VARIANT_STRUCT) {
                ZrParser_Compiler_PatternShapeMismatch(
                        cs,
                        node->location,
                        "Struct union using patterns require object destructuring",
                        "The default union variant exposes named fields, so tuple destructuring cannot bind it.",
                        "Use object destructuring such as `var {local: field} = resource`.");
                return ZR_TRUE;
            }
            actualBindingCount = node->data.destructuringArray.keys != ZR_NULL
                                         ? node->data.destructuringArray.keys->count
                                         : 0;
            if (expectedBindingCount != actualBindingCount) {
                ZrParser_Compiler_PatternArityMismatch(cs,
                                                       node->location,
                                                       expectedBindingCount,
                                                       actualBindingCount,
                                                       ZR_NULL);
                return ZR_TRUE;
            }
            *outBindings = node->data.destructuringArray.keys;
        } else {
            if (!build_union_struct_pattern_bindings(cs,
                                                     variant,
                                                     &node->data.objectLiteral,
                                                     &structBindings,
                                                     node->location)) {
                return ZR_TRUE;
            }
            actualBindingCount = structBindings != ZR_NULL ? structBindings->count : 0;
            if (expectedBindingCount != actualBindingCount) {
                TZrChar availableFields[256];
                ZrParser_Compiler_PatternArityMismatch(cs,
                                                       node->location,
                                                       expectedBindingCount,
                                                       actualBindingCount,
                                                       union_variant_available_field_names(
                                                               cs,
                                                               variant,
                                                               availableFields,
                                                               sizeof(availableFields)));
                return ZR_TRUE;
            }
            *outBindings = structBindings;
        }

        *outVariantName = variantName;
        if (outVariant != ZR_NULL) {
            *outVariant = variant;
        }
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        variantName = node->data.identifier.name;
    } else if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        SZrPrimaryExpression *primary = &node->data.primaryExpression;

        if (primary->property == ZR_NULL ||
            primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
            primary->members == ZR_NULL ||
            primary->members->count > 1) {
            return ZR_FALSE;
        }
        variantName = primary->property->data.identifier.name;
        if (primary->members->count == 1) {
            payloadNode = primary->members->nodes[0];
        }
    }

    if (variantName == ZR_NULL) {
        return ZR_FALSE;
    }

    variant = find_union_variant_node(unionDeclaration, variantName);
    if (variant == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Unknown union variant", node->location);
        return ZR_TRUE;
    }

    if (payloadNode != ZR_NULL) {
        if (payloadNode->type == ZR_AST_FUNCTION_CALL) {
            call = &payloadNode->data.functionCall;
            actualBindingCount = call->args != ZR_NULL ? call->args->count : 0;
            if (variant->data.unionVariant.kind == ZR_UNION_VARIANT_STRUCT) {
                ZrParser_Compiler_PatternShapeMismatch(
                        cs,
                        payloadNode->location,
                        "Struct union using patterns require object payload syntax",
                        "The selected union variant exposes named fields, so positional payload syntax cannot bind it.",
                        "Use object payload syntax such as `Variant { local: field }`.");
                return ZR_TRUE;
            }
            if (call->hasNamedArgs) {
                ZrParser_Compiler_Error(cs, "Union using pattern bindings must be positional", payloadNode->location);
                return ZR_TRUE;
            }
            for (TZrSize index = 0; index < actualBindingCount; index++) {
                SZrAstNode *bindingNode = call->args->nodes[index];
                if (bindingNode == ZR_NULL || bindingNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                    ZrParser_Compiler_Error(cs,
                                            "Union using pattern binding must be an identifier",
                                            bindingNode != ZR_NULL ? bindingNode->location : payloadNode->location);
                    return ZR_TRUE;
                }
            }
        } else if (payloadNode->type == ZR_AST_OBJECT_LITERAL) {
            if (!build_union_struct_pattern_bindings(cs,
                                                     variant,
                                                     &payloadNode->data.objectLiteral,
                                                     &structBindings,
                                                     payloadNode->location)) {
                return ZR_TRUE;
            }
            actualBindingCount = structBindings != ZR_NULL ? structBindings->count : 0;
        } else {
            return ZR_FALSE;
        }
    }

    expectedBindingCount = union_variant_field_count(variant);
    if ((call != ZR_NULL || structBindings != ZR_NULL) && expectedBindingCount != actualBindingCount) {
        TZrChar availableFields[256];
        ZrParser_Compiler_PatternArityMismatch(
                cs,
                node->location,
                expectedBindingCount,
                actualBindingCount,
                structBindings != ZR_NULL
                    ? union_variant_available_field_names(cs,
                                                          variant,
                                                          availableFields,
                                                          sizeof(availableFields))
                    : ZR_NULL);
        return ZR_TRUE;
    }

    *outVariantName = variantName;
    if (outVariant != ZR_NULL) {
        *outVariant = variant;
    }
    if (call != ZR_NULL) {
        *outBindings = call->args;
    } else if (structBindings != ZR_NULL) {
        *outBindings = structBindings;
    }
    return ZR_TRUE;
}

TZrBool try_resolve_union_variant_pattern_annotation(SZrCompilerState *cs,
                                                     SZrAstNode *pattern,
                                                     SZrType *variantTypeInfo,
                                                     SZrString **outUnionTypeName,
                                                     SZrString **outVariantName,
                                                     SZrAstNodeArray **outBindings,
                                                     SZrAstNode **outVariant) {
    SZrString *annotationUnionTypeName = ZR_NULL;
    SZrString *variantName = ZR_NULL;
    SZrAstNode *annotationUnion;
    SZrAstNode *variant;
    SZrAstNodeArray *structBindings = ZR_NULL;
    TZrSize expectedBindingCount;
    TZrSize actualBindingCount = 0;

    if (outUnionTypeName != ZR_NULL) {
        *outUnionTypeName = ZR_NULL;
    }
    if (outVariantName != ZR_NULL) {
        *outVariantName = ZR_NULL;
    }
    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (outVariant != ZR_NULL) {
        *outVariant = ZR_NULL;
    }
    if (cs == ZR_NULL || pattern == ZR_NULL || variantTypeInfo == ZR_NULL ||
        outUnionTypeName == ZR_NULL || outVariantName == ZR_NULL ||
        outBindings == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (!split_union_variant_type_annotation(cs,
                                             variantTypeInfo,
                                             &annotationUnionTypeName,
                                             &variantName) ||
        annotationUnionTypeName == ZR_NULL ||
        variantName == ZR_NULL) {
        annotationUnionTypeName = extract_type_name_string(cs, variantTypeInfo);
        annotationUnion = find_union_declaration_for_constructor_type(cs, annotationUnionTypeName);
        variant = find_union_default_using_variant_node(annotationUnion);
        variantName = union_variant_node_name(variant);
        if (annotationUnion == ZR_NULL || variant == ZR_NULL || variantName == ZR_NULL) {
            ZrParser_Compiler_Error(cs,
                                    "Using union pattern type annotation must be Union.Variant or a union with one default '@' variant",
                                    pattern->location);
            return ZR_TRUE;
        }
    } else {
        annotationUnion = find_union_declaration_for_constructor_type(cs, annotationUnionTypeName);
        if (annotationUnion == ZR_NULL) {
            ZrParser_Compiler_Error(cs,
                                    "Using union pattern type annotation must name a union",
                                    pattern->location);
            return ZR_TRUE;
        }
        variant = find_union_variant_node(annotationUnion, variantName);
        if (variant == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Unknown union variant", pattern->location);
            return ZR_TRUE;
        }
    }

    expectedBindingCount = union_variant_field_count(variant);
    if (pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        if (variant->data.unionVariant.kind == ZR_UNION_VARIANT_STRUCT) {
            ZrParser_Compiler_PatternShapeMismatch(
                    cs,
                    pattern->location,
                    "Struct union using patterns require object destructuring",
                    "The selected union variant exposes named fields, so tuple destructuring cannot bind it.",
                    "Use object destructuring such as `var {local: field}: Union.Variant = resource`.");
            return ZR_TRUE;
        }
        actualBindingCount = pattern->data.destructuringArray.keys != ZR_NULL
                                     ? pattern->data.destructuringArray.keys->count
                                     : 0;
        if (expectedBindingCount != actualBindingCount) {
            ZrParser_Compiler_PatternArityMismatch(cs,
                                                   pattern->location,
                                                   expectedBindingCount,
                                                   actualBindingCount,
                                                   ZR_NULL);
            return ZR_TRUE;
        }
        *outBindings = pattern->data.destructuringArray.keys;
    } else if (pattern->type == ZR_AST_OBJECT_LITERAL) {
        if (!build_union_struct_pattern_bindings(cs,
                                                 variant,
                                                 &pattern->data.objectLiteral,
                                                 &structBindings,
                                                 pattern->location)) {
            return ZR_TRUE;
        }
        actualBindingCount = structBindings != ZR_NULL ? structBindings->count : 0;
        if (expectedBindingCount != actualBindingCount) {
            TZrChar availableFields[256];
            ZrParser_Compiler_PatternArityMismatch(cs,
                                                   pattern->location,
                                                   expectedBindingCount,
                                                   actualBindingCount,
                                                   union_variant_available_field_names(
                                                           cs,
                                                           variant,
                                                           availableFields,
                                                           sizeof(availableFields)));
            return ZR_TRUE;
        }
        *outBindings = structBindings;
    } else {
        ZrParser_Compiler_Error(cs,
                                "Using union variant annotation requires tuple or object destructuring",
                                pattern->location);
        return ZR_TRUE;
    }

    *outUnionTypeName = annotationUnionTypeName;
    *outVariantName = variantName;
    if (outVariant != ZR_NULL) {
        *outVariant = variant;
    }
    return ZR_TRUE;
}

TZrBool try_resolve_union_variant_pattern_with_type_annotation(SZrCompilerState *cs,
                                                               SZrAstNode *pattern,
                                                               SZrType *variantTypeInfo,
                                                               SZrString *resourceTypeName,
                                                               SZrString **outVariantName,
                                                               SZrAstNodeArray **outBindings,
                                                               SZrAstNode **outVariant) {
    SZrString *annotationUnionTypeName = ZR_NULL;
    SZrString *variantName = ZR_NULL;
    SZrAstNode *resourceUnion;
    SZrAstNode *annotationUnion;

    if (outVariantName != ZR_NULL) {
        *outVariantName = ZR_NULL;
    }
    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (outVariant != ZR_NULL) {
        *outVariant = ZR_NULL;
    }
    if (cs == ZR_NULL || pattern == ZR_NULL || variantTypeInfo == ZR_NULL ||
        resourceTypeName == ZR_NULL || outVariantName == ZR_NULL ||
        outBindings == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (!try_resolve_union_variant_pattern_annotation(cs,
                                                      pattern,
                                                      variantTypeInfo,
                                                      &annotationUnionTypeName,
                                                      &variantName,
                                                      outBindings,
                                                      outVariant)) {
        return ZR_FALSE;
    }
    if (cs->hasError || annotationUnionTypeName == ZR_NULL || variantName == ZR_NULL) {
        return ZR_TRUE;
    }

    resourceUnion = find_union_declaration_for_constructor_type(cs, resourceTypeName);
    annotationUnion = find_union_declaration_for_constructor_type(cs, annotationUnionTypeName);
    if (resourceUnion == ZR_NULL || annotationUnion == ZR_NULL || resourceUnion != annotationUnion) {
        ZrParser_Compiler_PatternVariantMismatch(cs,
                                                 pattern->location,
                                                 union_string_text(annotationUnionTypeName),
                                                 union_string_text(variantName),
                                                 union_string_text(resourceTypeName));
        return ZR_TRUE;
    }

    *outVariantName = variantName;
    return ZR_TRUE;
}

void register_union_variant_payload_binding_type(SZrCompilerState *cs,
                                                 SZrAstNode *variant,
                                                 TZrSize payloadIndex,
                                                 SZrString *bindingName,
                                                 TZrBool moveBinding) {
    SZrParameter *field;
    SZrInferredType bindingType;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        bindingName == ZR_NULL) {
        return;
    }

    field = union_variant_payload_field_parameter(variant, payloadIndex);
    if (field == ZR_NULL || field->typeInfo == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_AstTypeToInferredType_Convert(cs, field->typeInfo, &bindingType)) {
        bindingType.ownershipQualifier =
                union_variant_payload_default_binding_qualifier(variant, payloadIndex, moveBinding);
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, bindingName, &bindingType);
    }
    ZrParser_InferredType_Free(cs->state, &bindingType);
}

TZrBool try_compile_union_variant_constructor_expression(SZrCompilerState *cs,
                                                         SZrAstNode *node,
                                                         TZrUInt32 *outSlot) {
    SZrPrimaryExpression *primary;
    SZrString *typeName = ZR_NULL;
    SZrAstNode *unionDeclaration;
    SZrString *variantName;
    SZrAstNode *variant;
    SZrFunctionCall *call = ZR_NULL;
    SZrObjectLiteral *objectLiteral = ZR_NULL;
    TZrSize expectedArgumentCount;
    TZrSize actualArgumentCount = 0;

    if (outSlot != ZR_NULL) {
        *outSlot = ZR_PARSER_SLOT_NONE;
    }
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || cs->hasError) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->members == ZR_NULL ||
        primary->members->count == 0 ||
        !union_constructor_target_type_name(cs, primary->property, &typeName) ||
        typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    unionDeclaration = find_union_declaration_for_constructor_type(cs, typeName);
    if (unionDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    variantName = union_variant_member_name(primary->members->nodes[0]);
    if (variantName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Union constructor must use a named variant", node->location);
        return ZR_TRUE;
    }

    variant = find_union_variant_node(unionDeclaration, variantName);
    if (variant == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Unknown union variant", primary->members->nodes[0]->location);
        return ZR_TRUE;
    }

    if (primary->members->count > 2) {
        ZrParser_Compiler_Error(cs,
                                "Union variant constructor cannot be followed by additional member access",
                                node->location);
        return ZR_TRUE;
    }

    if (primary->members->count == 2) {
        SZrAstNode *payloadNode = primary->members->nodes[1];
        if (payloadNode == ZR_NULL) {
            ZrParser_Compiler_Error(cs,
                                    "Union variant payload is missing",
                                    node->location);
            return ZR_TRUE;
        }
        if (payloadNode->type == ZR_AST_FUNCTION_CALL) {
            call = &payloadNode->data.functionCall;
            actualArgumentCount = call->args != ZR_NULL ? call->args->count : 0;
        } else if (payloadNode->type == ZR_AST_OBJECT_LITERAL) {
            objectLiteral = &payloadNode->data.objectLiteral;
            actualArgumentCount = objectLiteral->properties != ZR_NULL ? objectLiteral->properties->count : 0;
        } else {
            ZrParser_Compiler_Error(cs,
                                    "Union variant payload must be constructed with a call or object literal",
                                    payloadNode->location);
            return ZR_TRUE;
        }
    }

    expectedArgumentCount = union_variant_field_count(variant);
    if (objectLiteral != ZR_NULL) {
        if (!validate_union_struct_payload(cs, variant, objectLiteral, primary->members->nodes[1]->location)) {
            return ZR_TRUE;
        }
    } else if (variant->data.unionVariant.kind == ZR_UNION_VARIANT_STRUCT) {
        ZrParser_Compiler_Error(cs,
                                "Struct union variant requires object payload syntax",
                                node->location);
        return ZR_TRUE;
    }
    if (expectedArgumentCount == 0 && call != ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Unit union variant does not accept constructor arguments", node->location);
        return ZR_TRUE;
    }
    if (expectedArgumentCount > 0 && call == ZR_NULL && objectLiteral == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Union variant requires constructor arguments", node->location);
        return ZR_TRUE;
    }
    if (expectedArgumentCount != actualArgumentCount) {
        ZrParser_Compiler_Error(cs, "Union variant constructor argument count mismatch", node->location);
        return ZR_TRUE;
    }

    if (!emit_union_variant_carrier(cs, typeName, variantName, variant, call, objectLiteral, outSlot, node->location) &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile union variant constructor", node->location);
    }
    return ZR_TRUE;
}
