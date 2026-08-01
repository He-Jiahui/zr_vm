#include "compiler_interface_contracts.h"

#include "compile_expression_internal.h"

static TZrBool interface_contract_strings_equal(
        SZrString *left,
        SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool interface_contract_member_is_field(
        const SZrTypeMemberInfo *member) {
    return member != ZR_NULL &&
           (member->memberType == ZR_AST_STRUCT_FIELD ||
            member->memberType == ZR_AST_CLASS_FIELD);
}

static TZrBool interface_contract_parameter_types_match(
        const SZrArray *requiredTypes,
        const SZrArray *implementationTypes) {
    if (requiredTypes == ZR_NULL || implementationTypes == ZR_NULL) {
        return requiredTypes == implementationTypes;
    }
    if (requiredTypes->length != implementationTypes->length) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < requiredTypes->length; index++) {
        const SZrInferredType *requiredType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)requiredTypes, index);
        const SZrInferredType *implementationType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)implementationTypes, index);

        if (requiredType == ZR_NULL || implementationType == ZR_NULL) {
            if (requiredType != implementationType) {
                return ZR_FALSE;
            }
            continue;
        }
        if (requiredType->baseType != implementationType->baseType ||
            requiredType->ownershipQualifier !=
                    implementationType->ownershipQualifier ||
            !interface_contract_strings_equal(
                    requiredType->typeName, implementationType->typeName)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool interface_contract_member_signatures_match(
        const SZrTypeMemberInfo *requiredMember,
        const SZrTypeMemberInfo *implementation) {
    if (requiredMember == ZR_NULL || implementation == ZR_NULL ||
        requiredMember->name == ZR_NULL || implementation->name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_String_Equal(requiredMember->name, implementation->name) ||
        requiredMember->isStatic != implementation->isStatic ||
        requiredMember->parameterCount != implementation->parameterCount ||
        requiredMember->metaType != implementation->metaType ||
        requiredMember->accessorRole != implementation->accessorRole ||
        requiredMember->isMetaMethod != implementation->isMetaMethod ||
        interface_contract_member_is_field(requiredMember) !=
                interface_contract_member_is_field(implementation)) {
        return ZR_FALSE;
    }

    if (requiredMember->memberType == ZR_AST_PROPERTY_DECLARATION ||
        implementation->memberType == ZR_AST_PROPERTY_DECLARATION) {
        if (requiredMember->memberType != ZR_AST_PROPERTY_DECLARATION ||
            implementation->memberType != ZR_AST_PROPERTY_DECLARATION ||
            requiredMember->propertyValueTypeId !=
                    implementation->propertyValueTypeId ||
            requiredMember->structuredReturnType.referenceAccess !=
                    implementation->structuredReturnType.referenceAccess ||
            (requiredMember->getterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (implementation->getterAccessorSymbolId !=
                     ZR_SEMANTIC_ID_INVALID) ||
            (requiredMember->setterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (implementation->setterAccessorSymbolId !=
                     ZR_SEMANTIC_ID_INVALID) ||
            (requiredMember->initAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (implementation->initAccessorSymbolId !=
                     ZR_SEMANTIC_ID_INVALID)) {
            return ZR_FALSE;
        }
    }

    return interface_contract_strings_equal(
                   requiredMember->fieldTypeName,
                   implementation->fieldTypeName) &&
           interface_contract_strings_equal(
                   requiredMember->returnTypeName,
                   implementation->returnTypeName) &&
           interface_contract_parameter_types_match(
                   &requiredMember->parameterTypes,
                   &implementation->parameterTypes);
}

static SZrTypeMemberInfo *interface_contract_find_implementation(
        SZrTypePrototypeInfo *valueTypeInfo,
        const SZrTypeMemberInfo *requiredMember) {
    if (valueTypeInfo == ZR_NULL || requiredMember == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < valueTypeInfo->members.length; index++) {
        SZrTypeMemberInfo *candidate =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &valueTypeInfo->members, index);
        if (interface_contract_member_signatures_match(
                    requiredMember, candidate)) {
            return candidate;
        }
    }
    return ZR_NULL;
}

static TZrBool interface_contract_validate_recursive(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *valueTypeInfo,
        SZrString *interfaceName,
        SZrFileRange errorLocation,
        TZrUInt32 depth) {
    SZrTypePrototypeInfo *interfaceInfo;

    if (cs == ZR_NULL || valueTypeInfo == ZR_NULL ||
        interfaceName == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_TRUE;
    }
    interfaceInfo = find_compiler_type_prototype(cs, interfaceName);
    if (interfaceInfo == ZR_NULL ||
        interfaceInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(
                cs,
                "Interface inheritance target must resolve to an interface",
                errorLocation);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < interfaceInfo->members.length; index++) {
        const SZrTypeMemberInfo *requiredMember =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &interfaceInfo->members, index);
        SZrTypeMemberInfo *implementation;

        if (requiredMember == ZR_NULL ||
            (requiredMember->modifierFlags &
             ZR_DECLARATION_MODIFIER_ABSTRACT) == 0U) {
            continue;
        }
        implementation = interface_contract_find_implementation(
                valueTypeInfo, requiredMember);
        if (implementation == ZR_NULL ||
            (implementation->modifierFlags &
             ZR_DECLARATION_MODIFIER_ABSTRACT) != 0U ||
            !compiler_receiver_effect_can_implement(
                    requiredMember->receiverEffect,
                    implementation->receiverEffect) ||
            (requiredMember->isConst && !implementation->isConst)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Struct does not implement all interface members",
                    requiredMember->declarationNode != ZR_NULL
                            ? requiredMember->declarationNode->location
                            : errorLocation);
            return ZR_FALSE;
        }
        if (requiredMember->contractRole != ZR_MEMBER_CONTRACT_ROLE_NONE &&
            implementation->contractRole == ZR_MEMBER_CONTRACT_ROLE_NONE) {
            implementation->contractRole = requiredMember->contractRole;
        }
        if (requiredMember->interfaceContractSlot != (TZrUInt32)-1) {
            implementation->interfaceContractSlot =
                    requiredMember->interfaceContractSlot;
        }
    }

    TZrSize parentCount = interfaceInfo->inherits.length;
    for (TZrSize index = 0; index < parentCount; index++) {
        SZrString *parentInterfaceName;
        SZrString **parentName =
                ZR_NULL;

        interfaceInfo = find_compiler_type_prototype(cs, interfaceName);
        if (interfaceInfo == ZR_NULL ||
            interfaceInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
            index >= interfaceInfo->inherits.length) {
            ZrParser_Compiler_Error(
                    cs,
                    "Interface inheritance metadata changed during validation",
                    errorLocation);
            return ZR_FALSE;
        }
        parentName = (SZrString **)ZrCore_Array_Get(
                &interfaceInfo->inherits, index);
        parentInterfaceName =
                parentName != ZR_NULL ? *parentName : ZR_NULL;
        if (parentInterfaceName == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "Interface inheritance target must resolve to an interface",
                    errorLocation);
            return ZR_FALSE;
        }
        if (!interface_contract_validate_recursive(
                    cs,
                    valueTypeInfo,
                    parentInterfaceName,
                    errorLocation,
                    depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool compiler_interface_contracts_validate_value_type(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *valueTypeInfo,
        SZrFileRange errorLocation) {
    if (cs == ZR_NULL || valueTypeInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < valueTypeInfo->inherits.length; index++) {
        SZrString **interfaceName =
                (SZrString **)ZrCore_Array_Get(
                        &valueTypeInfo->inherits, index);
        if (interfaceName != ZR_NULL && *interfaceName != ZR_NULL &&
            !interface_contract_validate_recursive(
                    cs,
                    valueTypeInfo,
                    *interfaceName,
                    errorLocation,
                    0U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
