#include "compiler_internal.h"
#include "compiler_property.h"

typedef struct SZrCompilerPropertyAccessors {
    SZrAstNode *getter;
    SZrAstNode *setter;
    SZrAstNode *initializer;
} SZrCompilerPropertyAccessors;

static void compiler_property_init_member(
        SZrTypeMemberInfo *member,
        SZrString *ownerTypeName,
        SZrAstNode *declarationNode,
        TZrUInt32 declarationOrder) {
    memset(member, 0, sizeof(*member));
    member->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    member->accessModifier = ZR_ACCESS_PRIVATE;
    member->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    member->metaType = ZR_META_ENUM_MAX;
    member->declarationOrder = declarationOrder;
    member->declarationNode = declarationNode;
    member->ownerTypeName = ownerTypeName;
    member->baseDefinitionOwnerTypeName = ownerTypeName;
    member->virtualSlotIndex = (TZrUInt32)-1;
    member->interfaceContractSlot = (TZrUInt32)-1;
    member->propertyIdentity = (TZrUInt32)-1;
    member->accessorRole = ZR_PROPERTY_ACCESSOR_ROLE_NONE;
    member->propertySymbolId = ZR_SEMANTIC_ID_INVALID;
    member->propertyValueTypeId = ZR_SEMANTIC_ID_INVALID;
    member->getterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->setterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->initAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    ZrCore_Array_Construct(&member->parameterTypes);
    ZrCore_Array_Construct(&member->parameterNames);
    ZrCore_Array_Construct(&member->parameterHasDefaultValues);
    ZrCore_Array_Construct(&member->parameterDefaultValues);
    ZrCore_Array_Construct(&member->genericParameters);
    ZrCore_Array_Construct(&member->parameterPassingModes);
    ZrCore_Array_Construct(&member->decorators);
    ZrCore_Value_ResetAsNull(&member->decoratorMetadataValue);
}

static TZrUInt32 compiler_property_access_rank(EZrAccessModifier access) {
    switch (access) {
        case ZR_ACCESS_PRIVATE:
            return 0U;
        case ZR_ACCESS_PROTECTED:
            return 1U;
        case ZR_ACCESS_PUBLIC:
            return 2U;
        default:
            return 0U;
    }
}

static TZrBool compiler_property_is_contract_only(
        const SZrPropertyDeclaration *property,
        EZrCompilerPropertyContainerKind containerKind) {
    return containerKind == ZR_COMPILER_PROPERTY_CONTAINER_INTERFACE ||
           (property != ZR_NULL &&
            (property->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) != 0U);
}

static TZrBool compiler_property_validate(
        SZrCompilerState *cs,
        SZrAstNode *propertyNode,
        EZrCompilerPropertyContainerKind containerKind,
        SZrCompilerPropertyAccessors *outAccessors) {
    SZrPropertyDeclaration *property;
    TZrBool isContractOnly;

    if (cs == ZR_NULL || propertyNode == ZR_NULL || outAccessors == ZR_NULL ||
        propertyNode->type != ZR_AST_PROPERTY_DECLARATION) {
        return ZR_FALSE;
    }
    property = &propertyNode->data.propertyDeclaration;
    isContractOnly = compiler_property_is_contract_only(property, containerKind);
    memset(outAccessors, 0, sizeof(*outAccessors));
    if (property->name == ZR_NULL || property->name->name == ZR_NULL ||
        property->typeInfo == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "property declaration requires a name and type", propertyNode->location);
        return ZR_FALSE;
    }
    if (property->accessors == ZR_NULL || property->accessors->count == 0U) {
        ZrParser_Compiler_Error(
                cs, "property requires at least one accessor", propertyNode->location);
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < property->accessors->count; index++) {
        SZrAstNode *accessorNode = property->accessors->nodes[index];
        SZrPropertyAccessor *accessor;
        SZrAstNode **slot;

        if (accessorNode == ZR_NULL ||
            accessorNode->type != ZR_AST_PROPERTY_ACCESSOR) {
            ZrParser_Compiler_Error(
                    cs, "property contains an invalid accessor node", propertyNode->location);
            return ZR_FALSE;
        }
        accessor = &accessorNode->data.propertyAccessor;
        switch (accessor->kind) {
            case ZR_PROPERTY_ACCESSOR_GET:
                slot = &outAccessors->getter;
                break;
            case ZR_PROPERTY_ACCESSOR_SET:
                slot = &outAccessors->setter;
                break;
            case ZR_PROPERTY_ACCESSOR_INIT:
                slot = &outAccessors->initializer;
                break;
            default:
                ZrParser_Compiler_Error(
                        cs, "property contains an unknown accessor kind", accessorNode->location);
                return ZR_FALSE;
        }
        if (*slot != ZR_NULL) {
            const char *kindName = accessor->kind == ZR_PROPERTY_ACCESSOR_GET
                                           ? "get"
                                           : (accessor->kind == ZR_PROPERTY_ACCESSOR_SET ? "set" : "init");
            TZrChar message[96];
            snprintf(message, sizeof(message), "duplicate %s accessor", kindName);
            ZrParser_Compiler_Error(cs, message, accessor->keywordLocation);
            return ZR_FALSE;
        }
        *slot = accessorNode;

        if (accessor->hasAccessOverride &&
            compiler_property_access_rank(accessor->access) >
                    compiler_property_access_rank(property->access)) {
            ZrParser_Compiler_Error(
                    cs,
                    "accessor visibility cannot be wider than property visibility",
                    accessor->keywordLocation);
            return ZR_FALSE;
        }
        if (isContractOnly) {
            if (accessor->bodyKind != ZR_PROPERTY_ACCESSOR_BODY_BODYLESS ||
                accessor->body != ZR_NULL) {
                ZrParser_Compiler_Error(
                        cs,
                        containerKind == ZR_COMPILER_PROPERTY_CONTAINER_INTERFACE
                                ? "interface property accessor must be bodyless"
                                : "abstract property accessor must be bodyless",
                        accessor->keywordLocation);
                return ZR_FALSE;
            }
        } else if (accessor->bodyKind == ZR_PROPERTY_ACCESSOR_BODY_BODYLESS ||
                   accessor->body == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "concrete property accessor requires a body",
                    accessor->keywordLocation);
            return ZR_FALSE;
        }
    }

    if (outAccessors->setter != ZR_NULL && outAccessors->initializer != ZR_NULL) {
        ZrParser_Compiler_Error(
                cs,
                "set and init accessors are mutually exclusive",
                outAccessors->initializer->location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool compiler_property_append_parameter_type(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *member,
        SZrType *typeInfo) {
    SZrInferredType parameterType;
    EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    TZrBool hasDefault = ZR_FALSE;
    SZrTypeValue defaultValue;
    SZrString *parameterName;

    if (!ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &parameterType)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(cs->state, &member->parameterTypes, sizeof(SZrInferredType), 1U);
    ZrCore_Array_Init(cs->state, &member->parameterNames, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(cs->state, &member->parameterHasDefaultValues, sizeof(TZrBool), 1U);
    ZrCore_Array_Init(cs->state, &member->parameterDefaultValues, sizeof(SZrTypeValue), 1U);
    ZrCore_Array_Init(
            cs->state, &member->parameterPassingModes, sizeof(EZrParameterPassingMode), 1U);
    parameterName = ZrCore_String_CreateFromNative(cs->state, "value");
    ZrCore_Value_ResetAsNull(&defaultValue);
    ZrCore_Array_Push(cs->state, &member->parameterTypes, &parameterType);
    ZrCore_Array_Push(cs->state, &member->parameterNames, &parameterName);
    ZrCore_Array_Push(cs->state, &member->parameterHasDefaultValues, &hasDefault);
    ZrCore_Array_Push(cs->state, &member->parameterDefaultValues, &defaultValue);
    ZrCore_Array_Push(cs->state, &member->parameterPassingModes, &passingMode);
    member->parameterCount = 1U;
    member->minArgumentCount = 1U;
    return ZR_TRUE;
}

static TZrTypeId compiler_property_callable_type_id(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *member,
        TZrTypeId propertyValueTypeId) {
    SZrInferredType nullReturnType;
    const SZrInferredType *returnType;
    TZrTypeId typeId;

    if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) {
        returnType = &member->structuredReturnType;
    } else {
        ZrParser_InferredType_Init(cs->state, &nullReturnType, ZR_VALUE_TYPE_NULL);
        returnType = &nullReturnType;
    }
    typeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &member->parameterTypes,
            &member->parameterPassingModes,
            returnType,
            member->receiverEffect,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (member->accessorRole != ZR_PROPERTY_ACCESSOR_ROLE_GET) {
        ZrParser_InferredType_Free(cs->state, &nullReturnType);
    }
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return propertyValueTypeId;
    }
    return typeId;
}

static TZrBool compiler_property_emit_accessor(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *propertyNode,
        SZrAstNode *accessorNode,
        SZrString *ownerTypeName,
        SZrString *superTypeName,
        EZrCompilerPropertyContainerKind containerKind,
        TZrUInt32 declarationOrder,
        TZrUInt32 propertyIdentity,
        TZrSymbolId propertySymbolId,
        TZrTypeId propertyValueTypeId,
        TZrSymbolId *outAccessorSymbolId) {
    SZrPropertyDeclaration *property = &propertyNode->data.propertyDeclaration;
    SZrPropertyAccessor *accessor = &accessorNode->data.propertyAccessor;
    SZrTypeMemberInfo member;
    TZrTypeId callableTypeId;
    TZrUInt32 compiledParameterCount = 0U;
    TZrBool isContractOnly = compiler_property_is_contract_only(
            property, containerKind);

    compiler_property_init_member(&member, ownerTypeName, propertyNode, declarationOrder);
    member.memberType = containerKind == ZR_COMPILER_PROPERTY_CONTAINER_STRUCT
                                ? ZR_AST_STRUCT_METHOD
                                : ZR_AST_CLASS_METHOD;
    member.accessModifier = accessor->access;
    member.isStatic = property->isStatic;
    member.modifierFlags = property->modifierFlags;
    member.propertyIdentity = propertyIdentity;
    member.propertySymbolId = propertySymbolId;
    member.propertyValueTypeId = propertyValueTypeId;
    member.baseDefinitionName = property->name->name;
    member.accessorRole = accessor->kind == ZR_PROPERTY_ACCESSOR_GET
                                  ? ZR_PROPERTY_ACCESSOR_ROLE_GET
                                  : (accessor->kind == ZR_PROPERTY_ACCESSOR_SET
                                             ? ZR_PROPERTY_ACCESSOR_ROLE_SET
                                             : ZR_PROPERTY_ACCESSOR_ROLE_INIT);
    member.name = compiler_create_hidden_property_accessor_name(
            cs,
            property->name->name,
            accessor->kind != ZR_PROPERTY_ACCESSOR_GET);
    member.receiverEffect = property->isStatic
                                    ? ZR_CANONICAL_RECEIVER_NONE
                                    : (accessor->kind == ZR_PROPERTY_ACCESSOR_GET
                                               ? ZR_CANONICAL_RECEIVER_READONLY
                                               : ZR_CANONICAL_RECEIVER_MUTABLE);
    if (isContractOnly) {
        member.modifierFlags |=
                ZR_DECLARATION_MODIFIER_ABSTRACT | ZR_DECLARATION_MODIFIER_VIRTUAL;
        if (containerKind == ZR_COMPILER_PROPERTY_CONTAINER_INTERFACE) {
            member.virtualSlotIndex = prototype->nextVirtualSlotIndex++;
            member.interfaceContractSlot = member.virtualSlotIndex;
        }
    }

    if (accessor->kind == ZR_PROPERTY_ACCESSOR_GET) {
        if (!compiler_type_member_capture_structured_return_type(
                    cs, &member, property->typeInfo)) {
            ZrParser_Compiler_Error(
                    cs, "failed to bind property getter type", accessor->keywordLocation);
            return ZR_FALSE;
        }
    } else if (!compiler_property_append_parameter_type(
                       cs, &member, property->typeInfo)) {
        ZrParser_Compiler_Error(
                cs, "failed to bind property value parameter type", accessor->keywordLocation);
        return ZR_FALSE;
    }

    callableTypeId = compiler_property_callable_type_id(
            cs, &member, propertyValueTypeId);
    member.symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            member.name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            accessorNode,
            accessor->keywordLocation);
    if (member.symbolId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_Compiler_Error(
                cs, "failed to register property accessor symbol", accessor->keywordLocation);
        return ZR_FALSE;
    }
    if (!isContractOnly) {
        SZrFunction *compiledAccessor = compile_property_accessor_function(
                cs,
                propertyNode,
                accessorNode,
                superTypeName,
                !property->isStatic,
                member.receiverEffect,
                &compiledParameterCount);
        SZrTypeValue functionValue;

        if (compiledAccessor == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsRawObject(
                cs->state,
                &functionValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(compiledAccessor));
        member.compiledFunction = compiledAccessor;
        member.functionConstantIndex = add_constant(cs, &functionValue);
        ZR_UNUSED_PARAMETER(compiledParameterCount);
    }
    ZrCore_Array_Push(cs->state, &prototype->members, &member);
    *outAccessorSymbolId = member.symbolId;
    return ZR_TRUE;
}

TZrBool compiler_property_bind(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *propertyNode,
        SZrString *ownerTypeName,
        SZrString *superTypeName,
        EZrCompilerPropertyContainerKind containerKind,
        TZrUInt32 declarationOrder) {
    SZrPropertyDeclaration *property;
    SZrCompilerPropertyAccessors accessors;
    SZrTypeMemberInfo visible;
    TZrUInt32 propertyIdentity;
    TZrTypeId propertyValueTypeId;

    if (!compiler_property_validate(cs, propertyNode, containerKind, &accessors)) {
        return ZR_FALSE;
    }
    property = &propertyNode->data.propertyDeclaration;
    compiler_property_init_member(
            &visible, ownerTypeName, propertyNode, declarationOrder);
    visible.memberType = ZR_AST_PROPERTY_DECLARATION;
    visible.name = property->name->name;
    visible.accessModifier = property->access;
    visible.isStatic = property->isStatic;
    visible.modifierFlags = property->modifierFlags;
    visible.fieldType = property->typeInfo;
    visible.fieldTypeName = extract_type_name_string(cs, property->typeInfo);
    visible.baseDefinitionName = visible.name;
    if (!compiler_type_member_capture_structured_return_type(
                cs, &visible, property->typeInfo)) {
        ZrParser_Compiler_Error(
                cs, "failed to bind property value type", propertyNode->location);
        return ZR_FALSE;
    }
    propertyValueTypeId = ZrParser_CanonicalType_FromInferred(
            cs->semanticContext, &visible.structuredReturnType);
    if (propertyValueTypeId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_Compiler_Error(
                cs, "failed to canonicalize property value type", propertyNode->location);
        return ZR_FALSE;
    }
    propertyIdentity = prototype->nextPropertyIdentity++;
    visible.propertyIdentity = propertyIdentity;
    visible.propertyValueTypeId = propertyValueTypeId;
    visible.symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            visible.name,
            ZR_SEMANTIC_SYMBOL_KIND_PROPERTY,
            propertyValueTypeId,
            ZR_SEMANTIC_ID_INVALID,
            propertyNode,
            property->nameLocation);
    if (visible.symbolId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_Compiler_Error(
                cs, "failed to register property symbol", property->nameLocation);
        return ZR_FALSE;
    }
    visible.propertySymbolId = visible.symbolId;
    if (!ZrParser_CompileTime_ApplyMemberDecorators(
                cs,
                propertyNode,
                property->decorators,
                &visible)) {
        return ZR_FALSE;
    }

    if (accessors.getter != ZR_NULL &&
        !compiler_property_emit_accessor(
                cs,
                prototype,
                propertyNode,
                accessors.getter,
                ownerTypeName,
                superTypeName,
                containerKind,
                declarationOrder,
                propertyIdentity,
                visible.symbolId,
                propertyValueTypeId,
                &visible.getterAccessorSymbolId)) {
        return ZR_FALSE;
    }
    if (accessors.setter != ZR_NULL &&
        !compiler_property_emit_accessor(
                cs,
                prototype,
                propertyNode,
                accessors.setter,
                ownerTypeName,
                superTypeName,
                containerKind,
                declarationOrder,
                propertyIdentity,
                visible.symbolId,
                propertyValueTypeId,
                &visible.setterAccessorSymbolId)) {
        return ZR_FALSE;
    }
    if (accessors.initializer != ZR_NULL &&
        !compiler_property_emit_accessor(
                cs,
                prototype,
                propertyNode,
                accessors.initializer,
                ownerTypeName,
                superTypeName,
                containerKind,
                declarationOrder,
                propertyIdentity,
                visible.symbolId,
                propertyValueTypeId,
                &visible.initAccessorSymbolId)) {
        return ZR_FALSE;
    }

    /* The visible PropertySymbol precedes its accessors in semantic order. */
    ZrCore_Array_Push(cs->state, &prototype->members, &visible);
    if (prototype->members.length > 1U) {
        SZrTypeMemberInfo *members = (SZrTypeMemberInfo *)prototype->members.head;
        TZrSize visibleIndex = prototype->members.length - 1U;
        TZrSize accessorCount = (accessors.getter != ZR_NULL ? 1U : 0U) +
                                (accessors.setter != ZR_NULL ? 1U : 0U) +
                                (accessors.initializer != ZR_NULL ? 1U : 0U);
        TZrSize insertionIndex = visibleIndex - accessorCount;
        SZrTypeMemberInfo savedVisible = members[visibleIndex];
        memmove(&members[insertionIndex + 1U],
                &members[insertionIndex],
                accessorCount * sizeof(SZrTypeMemberInfo));
        members[insertionIndex] = savedVisible;
    }
    return ZR_TRUE;
}
