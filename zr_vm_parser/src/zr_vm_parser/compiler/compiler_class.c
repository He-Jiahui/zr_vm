//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compile_expression_internal.h"
#include "compile_time_executor_internal.h"
#include "cfg_internal.h"
#include "compiler_attribute_binding.h"
#include "compiler_decorator_contract.h"

static TZrBool compiler_class_ffi_integer_type_name_supported(SZrString *typeName) {
    static const TZrChar *const kSupportedIntegerTypeNames[] = {
            "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
    };

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kSupportedIntegerTypeNames); index++) {
        if (extern_compiler_string_equals(typeName, kSupportedIntegerTypeNames[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_class_view_type_is_source_extern_struct(SZrCompilerState *cs, SZrString *typeName) {
    SZrScript *script;

    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL || cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL || script->statements->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < script->statements->count; index++) {
        SZrAstNode *statement = script->statements->nodes[index];
        SZrAstNode *declaration;

        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK) {
            continue;
        }

        declaration = extern_compiler_find_named_declaration(&statement->data.externBlock, typeName);
        if (declaration != ZR_NULL && declaration->type == ZR_AST_STRUCT_DECLARATION) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrTypeMemberInfo *compiler_class_find_declared_member(SZrTypePrototypeInfo *info, SZrString *memberName) {
    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < info->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

static TZrUInt32 compiler_member_virtual_slot_none(void) {
    return (TZrUInt32)-1;
}

static TZrUInt32 compiler_member_property_identity_none(void) {
    return (TZrUInt32)-1;
}

static TZrUInt32 compiler_member_interface_contract_slot_none(void) {
    return (TZrUInt32)-1;
}

static TZrBool compiler_member_has_modifier(const SZrTypeMemberInfo *memberInfo, TZrUInt32 modifierFlag) {
    return memberInfo != ZR_NULL && (memberInfo->modifierFlags & modifierFlag) != 0;
}

static TZrBool compiler_class_has_modifier(const SZrTypePrototypeInfo *info, TZrUInt32 modifierFlag) {
    return info != ZR_NULL && (info->modifierFlags & modifierFlag) != 0;
}

static TZrBool compiler_strings_equal_or_both_null(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }

    return ZrCore_String_Equal(left, right);
}

static TZrBool compiler_member_is_field_kind(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    return memberInfo->memberType == ZR_AST_STRUCT_FIELD ||
           memberInfo->memberType == ZR_AST_CLASS_FIELD;
}

static TZrBool compiler_member_is_constructor_meta_method(const SZrTypeMemberInfo *memberInfo) {
    return memberInfo != ZR_NULL &&
           memberInfo->isMetaMethod &&
           memberInfo->metaType == ZR_META_CONSTRUCTOR;
}

static TZrBool compiler_member_supports_virtual_chain(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->isStatic) {
        return ZR_FALSE;
    }

    if (compiler_member_is_constructor_meta_method(memberInfo)) {
        return ZR_FALSE;
    }

    return memberInfo->memberType == ZR_AST_CLASS_METHOD ||
           memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION ||
           memberInfo->memberType == ZR_AST_PROPERTY_DECLARATION;
}

static TZrBool compiler_parameter_types_match(const SZrArray *leftTypes, const SZrArray *rightTypes) {
    if (leftTypes == ZR_NULL || rightTypes == ZR_NULL) {
        return leftTypes == rightTypes;
    }

    if (leftTypes->length != rightTypes->length) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < leftTypes->length; index++) {
        const SZrInferredType *leftType = (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)leftTypes, index);
        const SZrInferredType *rightType = (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)rightTypes, index);
        if (leftType == ZR_NULL || rightType == ZR_NULL) {
            if (leftType != rightType) {
                return ZR_FALSE;
            }
            continue;
        }

        if (leftType->baseType != rightType->baseType ||
            leftType->ownershipQualifier != rightType->ownershipQualifier) {
            return ZR_FALSE;
        }

        if (leftType->typeName == ZR_NULL || rightType->typeName == ZR_NULL) {
            if (leftType->typeName != rightType->typeName) {
                return ZR_FALSE;
            }
        } else if (!ZrCore_String_Equal(leftType->typeName, rightType->typeName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_member_signatures_match(const SZrTypeMemberInfo *baseMember,
                                                const SZrTypeMemberInfo *currentMember) {
    if (baseMember == ZR_NULL || currentMember == ZR_NULL ||
        baseMember->name == ZR_NULL || currentMember->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_String_Equal(baseMember->name, currentMember->name) ||
        baseMember->isStatic != currentMember->isStatic ||
        baseMember->parameterCount != currentMember->parameterCount ||
        baseMember->metaType != currentMember->metaType ||
        baseMember->accessorRole != currentMember->accessorRole) {
        return ZR_FALSE;
    }

    if (compiler_member_is_field_kind(baseMember) != compiler_member_is_field_kind(currentMember)) {
        return ZR_FALSE;
    }

    if (baseMember->isMetaMethod != currentMember->isMetaMethod) {
        return ZR_FALSE;
    }

    if (baseMember->memberType == ZR_AST_PROPERTY_DECLARATION ||
        currentMember->memberType == ZR_AST_PROPERTY_DECLARATION) {
        if (baseMember->memberType != ZR_AST_PROPERTY_DECLARATION ||
            currentMember->memberType != ZR_AST_PROPERTY_DECLARATION ||
            baseMember->propertyValueTypeId !=
                    currentMember->propertyValueTypeId ||
            baseMember->structuredReturnType.referenceAccess !=
                    currentMember->structuredReturnType.referenceAccess ||
            (baseMember->getterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (currentMember->getterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) ||
            (baseMember->setterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (currentMember->setterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) ||
            (baseMember->initAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                    (currentMember->initAccessorSymbolId != ZR_SEMANTIC_ID_INVALID)) {
            return ZR_FALSE;
        }
    }

    if (!compiler_strings_equal_or_both_null(baseMember->fieldTypeName, currentMember->fieldTypeName) ||
        !compiler_strings_equal_or_both_null(baseMember->returnTypeName, currentMember->returnTypeName)) {
        return ZR_FALSE;
    }

    return compiler_parameter_types_match(&baseMember->parameterTypes, &currentMember->parameterTypes);
}

static const SZrTypeMemberInfo *compiler_class_find_matching_member_in_class_chain_recursive(
        SZrCompilerState *cs,
        SZrString *typeName,
        const SZrTypeMemberInfo *memberInfo,
        TZrUInt32 depth) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || typeName == ZR_NULL || memberInfo == ZR_NULL || memberInfo->name == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    prototype = find_compiler_type_prototype(cs, typeName);
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *candidate =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, index);
        if (compiler_member_signatures_match(candidate, memberInfo)) {
            return candidate;
        }
    }

    if (prototype->extendsTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    return compiler_class_find_matching_member_in_class_chain_recursive(cs,
                                                                        prototype->extendsTypeName,
                                                                        memberInfo,
                                                                        depth + 1);
}

static const SZrTypeMemberInfo *compiler_class_find_base_override_target(SZrCompilerState *cs,
                                                                         const SZrTypePrototypeInfo *info,
                                                                         const SZrTypeMemberInfo *memberInfo) {
    if (cs == ZR_NULL || info == ZR_NULL || memberInfo == ZR_NULL || info->extendsTypeName == ZR_NULL ||
        memberInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    return compiler_class_find_matching_member_in_class_chain_recursive(cs, info->extendsTypeName, memberInfo, 0);
}

static const SZrTypeMemberInfo *compiler_class_find_declared_member_by_signature(
        const SZrTypePrototypeInfo *info,
        const SZrTypeMemberInfo *requiredMember) {
    if (info == ZR_NULL || requiredMember == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < info->members.length; index++) {
        const SZrTypeMemberInfo *declaredMember =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&info->members, index);
        if (compiler_member_signatures_match(requiredMember, declaredMember)) {
            return declaredMember;
        }
    }

    return ZR_NULL;
}

static TZrBool compiler_class_member_body_is_present(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (memberInfo->declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return memberInfo->declarationNode->data.classMethod.body != ZR_NULL;
        case ZR_AST_CLASS_META_FUNCTION:
            return memberInfo->declarationNode->data.classMetaFunction.body != ZR_NULL;
        case ZR_AST_CLASS_PROPERTY:
            if (memberInfo->declarationNode->data.classProperty.modifier == ZR_NULL) {
                return ZR_FALSE;
            }
            if (memberInfo->declarationNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET) {
                return memberInfo->declarationNode->data.classProperty.modifier->data.propertyGet.body != ZR_NULL;
            }
            if (memberInfo->declarationNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET) {
                return memberInfo->declarationNode->data.classProperty.modifier->data.propertySet.body != ZR_NULL;
            }
            return ZR_FALSE;
        case ZR_AST_PROPERTY_DECLARATION: {
            const SZrPropertyDeclaration *property =
                    &memberInfo->declarationNode->data.propertyDeclaration;
            if (property->accessors == ZR_NULL) {
                return ZR_FALSE;
            }
            if (memberInfo->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
                for (TZrSize index = 0U; index < property->accessors->count; index++) {
                    const SZrAstNode *accessorNode = property->accessors->nodes[index];
                    if (accessorNode != ZR_NULL &&
                        accessorNode->type == ZR_AST_PROPERTY_ACCESSOR &&
                        accessorNode->data.propertyAccessor.body != ZR_NULL) {
                        return ZR_TRUE;
                    }
                }
                return ZR_FALSE;
            }
            for (TZrSize index = 0U; index < property->accessors->count; index++) {
                const SZrAstNode *accessorNode = property->accessors->nodes[index];
                const SZrPropertyAccessor *accessor;
                EZrPropertyAccessorRole role;

                if (accessorNode == ZR_NULL ||
                    accessorNode->type != ZR_AST_PROPERTY_ACCESSOR) {
                    continue;
                }
                accessor = &accessorNode->data.propertyAccessor;
                role = accessor->kind == ZR_PROPERTY_ACCESSOR_GET
                               ? ZR_PROPERTY_ACCESSOR_ROLE_GET
                               : (accessor->kind == ZR_PROPERTY_ACCESSOR_SET
                                          ? ZR_PROPERTY_ACCESSOR_ROLE_SET
                                          : ZR_PROPERTY_ACCESSOR_ROLE_INIT);
                if (role == memberInfo->accessorRole) {
                    return accessor->body != ZR_NULL &&
                           accessor->bodyKind != ZR_PROPERTY_ACCESSOR_BODY_BODYLESS;
                }
            }
            return ZR_FALSE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_class_validate_member_override_semantics(SZrCompilerState *cs,
                                                                 SZrTypePrototypeInfo *info,
                                                                 SZrTypeMemberInfo *memberInfo,
                                                                 SZrFileRange location) {
    const SZrTypeMemberInfo *baseMember;
    TZrBool hasOverride;
    TZrBool hasShadow;
    TZrBool hasAbstract;
    TZrBool hasVirtual;
    TZrBool hasFinal;
    TZrBool memberHasBody;

    if (cs == ZR_NULL || info == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    hasOverride = (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE) != 0;
    hasShadow = (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_SHADOW) != 0;
    hasAbstract = (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) != 0;
    hasVirtual = (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_VIRTUAL) != 0;
    hasFinal = (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL) != 0;
    memberHasBody = compiler_class_member_body_is_present(memberInfo);
    baseMember = compiler_class_find_base_override_target(cs, info, memberInfo);
    memberInfo->baseDefinitionName = memberInfo->name;

    if (hasAbstract && !hasVirtual) {
        memberInfo->modifierFlags |= ZR_DECLARATION_MODIFIER_VIRTUAL;
        hasVirtual = ZR_TRUE;
    }

    if (hasOverride && hasShadow) {
        ZrParser_Compiler_Error(cs, "Member cannot combine override and shadow", location);
        return ZR_FALSE;
    }

    if (hasAbstract) {
        if (!compiler_class_has_modifier(info, ZR_DECLARATION_MODIFIER_ABSTRACT)) {
            ZrParser_Compiler_Error(cs, "Abstract members can only appear in abstract classes", location);
            return ZR_FALSE;
        }
        if (memberInfo->isStatic || hasFinal || hasShadow) {
            ZrParser_Compiler_Error(cs,
                                    "Abstract members cannot be static, final, or shadow",
                                    location);
            return ZR_FALSE;
        }
        if (memberHasBody) {
            ZrParser_Compiler_Error(cs, "Abstract members cannot declare a body", location);
            return ZR_FALSE;
        }
    } else if (!memberHasBody && compiler_member_supports_virtual_chain(memberInfo)) {
        ZrParser_Compiler_Error(cs, "Non-abstract class members must declare a body", location);
        return ZR_FALSE;
    }

    if (compiler_member_is_constructor_meta_method(memberInfo) &&
        memberInfo->modifierFlags != ZR_DECLARATION_MODIFIER_NONE) {
        ZrParser_Compiler_Error(cs, "@constructor does not support abstract/virtual/override/final/shadow modifiers", location);
        return ZR_FALSE;
    }

    if (!compiler_member_supports_virtual_chain(memberInfo) &&
        (hasAbstract || hasVirtual || hasOverride || hasShadow || hasFinal)) {
        ZrParser_Compiler_Error(cs,
                                "Only instance methods, properties/accessors, and non-constructor meta methods support abstract/virtual/override/final/shadow",
                                location);
        return ZR_FALSE;
    }

    if (hasOverride) {
        if (baseMember == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "override must target an inherited base member", location);
            return ZR_FALSE;
        }
        if (!compiler_member_signatures_match(baseMember, memberInfo)) {
            ZrParser_Compiler_Error(cs, "override target signature does not match the inherited base member", location);
            return ZR_FALSE;
        }
        if (!compiler_receiver_effect_can_implement(
                    baseMember->receiverEffect, memberInfo->receiverEffect)) {
            ZrParser_Compiler_Error(
                    cs,
                    "override cannot strengthen a readonly receiver contract to writable",
                    location);
            return ZR_FALSE;
        }
        if (!compiler_member_supports_virtual_chain(baseMember) ||
            (!compiler_member_has_modifier(baseMember, ZR_DECLARATION_MODIFIER_VIRTUAL) &&
             !compiler_member_has_modifier(baseMember, ZR_DECLARATION_MODIFIER_ABSTRACT) &&
             !compiler_member_has_modifier(baseMember, ZR_DECLARATION_MODIFIER_OVERRIDE))) {
            ZrParser_Compiler_Error(cs, "override target is not virtual or abstract", location);
            return ZR_FALSE;
        }
        if (compiler_member_has_modifier(baseMember, ZR_DECLARATION_MODIFIER_FINAL)) {
            ZrParser_Compiler_Error(cs, "Cannot override a final member", location);
            return ZR_FALSE;
        }

        memberInfo->baseDefinitionOwnerTypeName = baseMember->baseDefinitionOwnerTypeName != ZR_NULL
                                                          ? baseMember->baseDefinitionOwnerTypeName
                                                          : baseMember->ownerTypeName;
        memberInfo->baseDefinitionName = baseMember->baseDefinitionName != ZR_NULL
                                                 ? baseMember->baseDefinitionName
                                                 : baseMember->name;
        memberInfo->virtualSlotIndex = baseMember->virtualSlotIndex;
        memberInfo->interfaceContractSlot = baseMember->interfaceContractSlot;
        if (baseMember->propertyIdentity != compiler_member_property_identity_none()) {
            TZrUInt32 previousPropertyIdentity = memberInfo->propertyIdentity;
            if (memberInfo->propertyIdentity != compiler_member_property_identity_none() &&
                memberInfo->propertyIdentity != baseMember->propertyIdentity &&
                info->nextPropertyIdentity > 0) {
                info->nextPropertyIdentity--;
            }
            memberInfo->propertyIdentity = baseMember->propertyIdentity;
            if (memberInfo->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE &&
                memberInfo->propertySymbolId != ZR_SEMANTIC_ID_INVALID &&
                previousPropertyIdentity != baseMember->propertyIdentity) {
                for (TZrSize relatedIndex = 0U;
                     relatedIndex < info->members.length;
                     relatedIndex++) {
                    SZrTypeMemberInfo *relatedMember =
                            (SZrTypeMemberInfo *)ZrCore_Array_Get(
                                    &info->members, relatedIndex);
                    if (relatedMember != ZR_NULL &&
                        relatedMember->propertySymbolId ==
                                memberInfo->propertySymbolId) {
                        relatedMember->propertyIdentity =
                                baseMember->propertyIdentity;
                    }
                }
            }
        }
        if (memberInfo->memberType != ZR_AST_PROPERTY_DECLARATION &&
            memberInfo->virtualSlotIndex == compiler_member_virtual_slot_none()) {
            memberInfo->virtualSlotIndex = info->nextVirtualSlotIndex++;
        }
        return ZR_TRUE;
    }

    if (hasShadow) {
        if (baseMember == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "shadow must target an inherited base member", location);
            return ZR_FALSE;
        }

        memberInfo->baseDefinitionOwnerTypeName = memberInfo->ownerTypeName;
        memberInfo->baseDefinitionName = memberInfo->name;
        memberInfo->virtualSlotIndex = (hasVirtual || hasAbstract) ? info->nextVirtualSlotIndex++
                                                                   : compiler_member_virtual_slot_none();
        return ZR_TRUE;
    }

    if (baseMember != ZR_NULL &&
        compiler_member_is_constructor_meta_method(memberInfo) &&
        compiler_member_is_constructor_meta_method(baseMember)) {
        memberInfo->baseDefinitionOwnerTypeName = memberInfo->ownerTypeName;
        memberInfo->baseDefinitionName = memberInfo->name;
        memberInfo->virtualSlotIndex = compiler_member_virtual_slot_none();
        return ZR_TRUE;
    }

    if (baseMember != ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Inherited members with the same name require explicit override or shadow",
                                location);
        return ZR_FALSE;
    }

    if ((hasVirtual || hasAbstract) &&
        memberInfo->memberType != ZR_AST_PROPERTY_DECLARATION) {
        memberInfo->virtualSlotIndex = info->nextVirtualSlotIndex++;
    }

    if (!hasVirtual && !hasAbstract && !hasFinal) {
        memberInfo->virtualSlotIndex = compiler_member_virtual_slot_none();
    }

    if (hasFinal && !(hasVirtual || hasAbstract)) {
        memberInfo->virtualSlotIndex = compiler_member_virtual_slot_none();
    }

    return ZR_TRUE;
}

static const SZrTypeMemberInfo *compiler_class_find_effective_requirement_implementation(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *info,
        const SZrTypeMemberInfo *requiredMember,
        TZrBool *outDeclaredInCurrentClass) {
    const SZrTypeMemberInfo *declaredMember;

    if (outDeclaredInCurrentClass != ZR_NULL) {
        *outDeclaredInCurrentClass = ZR_FALSE;
    }

    if (cs == ZR_NULL || info == ZR_NULL || requiredMember == ZR_NULL) {
        return ZR_NULL;
    }

    declaredMember = compiler_class_find_declared_member_by_signature(info, requiredMember);
    if (declaredMember != ZR_NULL) {
        if (outDeclaredInCurrentClass != ZR_NULL) {
            *outDeclaredInCurrentClass = ZR_TRUE;
        }
        return declaredMember;
    }

    if (info->extendsTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    return compiler_class_find_matching_member_in_class_chain_recursive(cs, info->extendsTypeName, requiredMember, 0);
}

static TZrBool compiler_class_requirement_owner_is_interface(SZrCompilerState *cs,
                                                             const SZrTypeMemberInfo *requiredMember) {
    SZrTypePrototypeInfo *ownerPrototype;

    if (cs == ZR_NULL || requiredMember == ZR_NULL || requiredMember->ownerTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ownerPrototype = find_compiler_type_prototype(cs, requiredMember->ownerTypeName);
    return ownerPrototype != ZR_NULL && ownerPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
}

static void compiler_class_bind_interface_contract_slot(const SZrTypeMemberInfo *requiredMember,
                                                        const SZrTypeMemberInfo *implementation,
                                                        TZrBool declaredInCurrentClass) {
    if (!declaredInCurrentClass ||
        requiredMember == ZR_NULL ||
        implementation == ZR_NULL ||
        compiler_member_has_modifier(implementation, ZR_DECLARATION_MODIFIER_SHADOW)) {
        return;
    }

    if (requiredMember->contractRole != ZR_MEMBER_CONTRACT_ROLE_NONE &&
        implementation->contractRole == ZR_MEMBER_CONTRACT_ROLE_NONE) {
        ((SZrTypeMemberInfo *)implementation)->contractRole = requiredMember->contractRole;
    }
    if (requiredMember->interfaceContractSlot != compiler_member_interface_contract_slot_none()) {
        ((SZrTypeMemberInfo *)implementation)->interfaceContractSlot = requiredMember->interfaceContractSlot;
    }
}

static TZrBool compiler_class_member_satisfies_requirement(SZrCompilerState *cs,
                                                           const SZrTypeMemberInfo *requiredMember,
                                                           const SZrTypeMemberInfo *implementation,
                                                           TZrBool declaredInCurrentClass) {
    TZrBool requirementFromInterface;

    if (cs == ZR_NULL || requiredMember == ZR_NULL || implementation == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_member_signatures_match(requiredMember, implementation) ||
        !compiler_receiver_effect_can_implement(
                requiredMember->receiverEffect,
                implementation->receiverEffect) ||
        compiler_member_has_modifier(implementation, ZR_DECLARATION_MODIFIER_ABSTRACT)) {
        return ZR_FALSE;
    }

    if (declaredInCurrentClass && compiler_member_has_modifier(implementation, ZR_DECLARATION_MODIFIER_SHADOW)) {
        return ZR_FALSE;
    }

    requirementFromInterface = compiler_class_requirement_owner_is_interface(cs, requiredMember);
    if (!requirementFromInterface &&
        declaredInCurrentClass &&
        !compiler_member_has_modifier(implementation, ZR_DECLARATION_MODIFIER_OVERRIDE)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool compiler_class_validate_required_members_recursive(SZrCompilerState *cs,
                                                                  SZrTypePrototypeInfo *info,
                                                                  SZrString *typeName,
                                                                  TZrUInt32 depth) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || info == ZR_NULL || typeName == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_TRUE;
    }

    prototype = find_compiler_type_prototype(cs, typeName);
    if (prototype == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize memberIndex = 0; memberIndex < prototype->members.length; memberIndex++) {
        const SZrTypeMemberInfo *requiredMember =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, memberIndex);
        const SZrTypeMemberInfo *implementation = ZR_NULL;
        TZrBool declaredInCurrentClass = ZR_FALSE;

        if (requiredMember == ZR_NULL ||
            (requiredMember->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) == 0) {
            continue;
        }

        implementation = compiler_class_find_effective_requirement_implementation(cs,
                                                                                  info,
                                                                                  requiredMember,
                                                                                  &declaredInCurrentClass);
        if (compiler_class_requirement_owner_is_interface(cs, requiredMember)) {
            compiler_class_bind_interface_contract_slot(requiredMember, implementation, declaredInCurrentClass);
        }

        if (!compiler_class_has_modifier(info, ZR_DECLARATION_MODIFIER_ABSTRACT) &&
            !compiler_class_member_satisfies_requirement(cs,
                                                         requiredMember,
                                                         implementation,
                                                         declaredInCurrentClass)) {
            ZrParser_Compiler_Error(cs,
                                    "Concrete class does not implement all abstract/interface members",
                                    requiredMember->declarationNode != ZR_NULL
                                            ? requiredMember->declarationNode->location
                                            : ZrParser_FileRange_Create(
                                                      ZrParser_FilePosition_Create(0, 0, 0),
                                                      ZrParser_FilePosition_Create(0, 0, 0),
                                                      ZR_NULL));
            return ZR_FALSE;
        }
    }

    for (TZrSize inheritIndex = 0; inheritIndex < prototype->inherits.length; inheritIndex++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&prototype->inherits, inheritIndex);
        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }

        if (!compiler_class_validate_required_members_recursive(cs, info, *inheritNamePtr, depth + 1)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_class_extract_builtin_ffi_string_decorator(SZrCompilerState *cs,
                                                                   SZrAstNodeArray *decorators,
                                                                   const TZrChar *leafName,
                                                                   SZrString **outValue,
                                                                   TZrBool *outPresent,
                                                                   SZrFileRange location) {
    SZrFunctionCall *call = ZR_NULL;
    SZrAstNode *decoratorNode;

    if (outValue != ZR_NULL) {
        *outValue = ZR_NULL;
    }
    if (outPresent != ZR_NULL) {
        *outPresent = ZR_FALSE;
    }

    if (cs == ZR_NULL || leafName == ZR_NULL) {
        return ZR_FALSE;
    }

    decoratorNode = extern_compiler_decorators_find_call(decorators, leafName, &call);
    if (decoratorNode == ZR_NULL) {
        for (TZrSize index = 0; decorators != ZR_NULL && index < decorators->count; index++) {
            TZrBool hasCall = ZR_FALSE;
            const TZrChar *builtinLeafName =
                    ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
                            decorators->nodes[index], &hasCall);
            if (builtinLeafName != ZR_NULL && strcmp(builtinLeafName, leafName) == 0 && !hasCall) {
                ZrParser_Compiler_Error(cs, "zr.ffi class wrapper decorators require a single string argument", location);
                return ZR_FALSE;
            }
        }
        return ZR_TRUE;
    }

    if (outPresent != ZR_NULL) {
        *outPresent = ZR_TRUE;
    }
    if (!extern_compiler_extract_string_argument(call, outValue) || outValue == ZR_NULL || *outValue == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "zr.ffi class wrapper decorators require a single string argument", location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static SZrObject *compiler_class_ensure_type_metadata_object(SZrCompilerState *cs, SZrTypePrototypeInfo *info) {
    SZrObject *metadataObject;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_NULL;
    }

    if (info->hasDecoratorMetadata && info->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        info->decoratorMetadataValue.value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(cs->state, info->decoratorMetadataValue.value.object);
    }

    metadataObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &info->decoratorMetadataValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    info->decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    info->hasDecoratorMetadata = ZR_TRUE;
    return metadataObject;
}

static TZrBool compiler_class_set_type_metadata_string_field(SZrCompilerState *cs,
                                                             SZrTypePrototypeInfo *info,
                                                             const TZrChar *fieldName,
                                                             SZrString *fieldValue) {
    SZrTypeValue value;
    SZrObject *metadataObject;

    if (cs == ZR_NULL || info == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    metadataObject = compiler_class_ensure_type_metadata_object(cs, info);
    if (metadataObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldValue));
    value.type = ZR_VALUE_TYPE_STRING;
    return extern_compiler_set_object_field(cs, metadataObject, fieldName, &value);
}

static TZrBool compiler_class_append_builtin_decorator_name(SZrCompilerState *cs,
                                                            SZrTypePrototypeInfo *info,
                                                            const TZrChar *qualifiedName) {
    SZrTypeDecoratorInfo decoratorInfo;
    SZrString *decoratorName;

    if (cs == ZR_NULL || info == ZR_NULL || qualifiedName == ZR_NULL) {
        return ZR_FALSE;
    }

    decoratorName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)qualifiedName);
    if (decoratorName == ZR_NULL) {
        return ZR_FALSE;
    }

    decoratorInfo.name = decoratorName;
    ZrCore_Array_Push(cs->state, &info->decorators, &decoratorInfo);
    return ZR_TRUE;
}

static TZrBool compiler_class_apply_builtin_ffi_wrapper_decorators(SZrCompilerState *cs,
                                                                   SZrAstNodeArray *decorators,
                                                                   SZrTypePrototypeInfo *info,
                                                                   SZrFileRange location) {
    SZrString *loweringValue = ZR_NULL;
    SZrString *viewTypeValue = ZR_NULL;
    SZrString *underlyingValue = ZR_NULL;
    SZrString *ownerModeValue = ZR_NULL;
    SZrString *releaseHookValue = ZR_NULL;
    TZrBool hasLowering = ZR_FALSE;
    TZrBool hasViewType = ZR_FALSE;
    TZrBool hasUnderlying = ZR_FALSE;
    TZrBool hasOwnerMode = ZR_FALSE;
    TZrBool hasReleaseHook = ZR_FALSE;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    if (!compiler_class_extract_builtin_ffi_string_decorator(
                cs, decorators, "lowering", &loweringValue, &hasLowering, location) ||
        !compiler_class_extract_builtin_ffi_string_decorator(
                cs, decorators, "viewType", &viewTypeValue, &hasViewType, location) ||
        !compiler_class_extract_builtin_ffi_string_decorator(
                cs, decorators, "underlying", &underlyingValue, &hasUnderlying, location) ||
        !compiler_class_extract_builtin_ffi_string_decorator(
                cs, decorators, "ownerMode", &ownerModeValue, &hasOwnerMode, location) ||
        !compiler_class_extract_builtin_ffi_string_decorator(
                cs, decorators, "releaseHook", &releaseHookValue, &hasReleaseHook, location)) {
        return ZR_FALSE;
    }

    if (!hasLowering && !hasViewType && !hasUnderlying && !hasOwnerMode && !hasReleaseHook) {
        return ZR_TRUE;
    }

    if (hasLowering &&
        !extern_compiler_string_equals(loweringValue, "value") &&
        !extern_compiler_string_equals(loweringValue, "pointer") &&
        !extern_compiler_string_equals(loweringValue, "handle_id")) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.lowering on class wrappers requires one of: value, pointer, handle_id",
                                location);
        return ZR_FALSE;
    }

    if (hasOwnerMode &&
        !extern_compiler_string_equals(ownerModeValue, "borrowed") &&
        !extern_compiler_string_equals(ownerModeValue, "owned")) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.ownerMode on class wrappers requires one of: borrowed, owned",
                                location);
        return ZR_FALSE;
    }

    if (hasUnderlying &&
        (!hasLowering || !extern_compiler_string_equals(loweringValue, "handle_id"))) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.underlying on class wrappers currently requires zr.ffi.lowering(\"handle_id\")",
                                location);
        return ZR_FALSE;
    }

    if (hasLowering && extern_compiler_string_equals(loweringValue, "handle_id") && !hasUnderlying) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.lowering(\"handle_id\") on class wrappers requires zr.ffi.underlying(...)",
                                location);
        return ZR_FALSE;
    }

    if (hasLowering && extern_compiler_string_equals(loweringValue, "handle_id") && hasUnderlying &&
        !compiler_class_ffi_integer_type_name_supported(underlyingValue)) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.underlying on class wrappers requires a supported integer type name: i8, u8, i16, u16, i32, u32, i64, u64",
                                location);
        return ZR_FALSE;
    }

    if (hasViewType && !compiler_class_view_type_is_source_extern_struct(cs, viewTypeValue)) {
        ZrParser_Compiler_Error(cs,
                                "zr.ffi.viewType on class wrappers requires a source extern struct name",
                                location);
        return ZR_FALSE;
    }

    if (hasLowering &&
        (!compiler_class_set_type_metadata_string_field(cs, info, "ffiLoweringKind", loweringValue) ||
         !compiler_class_append_builtin_decorator_name(cs, info, "zr.ffi.lowering"))) {
        return ZR_FALSE;
    }
    if (hasViewType &&
        (!compiler_class_set_type_metadata_string_field(cs, info, "ffiViewTypeName", viewTypeValue) ||
         !compiler_class_append_builtin_decorator_name(cs, info, "zr.ffi.viewType"))) {
        return ZR_FALSE;
    }
    if (hasUnderlying &&
        (!compiler_class_set_type_metadata_string_field(cs, info, "ffiUnderlyingTypeName", underlyingValue) ||
         !compiler_class_append_builtin_decorator_name(cs, info, "zr.ffi.underlying"))) {
        return ZR_FALSE;
    }
    if (hasOwnerMode &&
        (!compiler_class_set_type_metadata_string_field(cs, info, "ffiOwnerMode", ownerModeValue) ||
         !compiler_class_append_builtin_decorator_name(cs, info, "zr.ffi.ownerMode"))) {
        return ZR_FALSE;
    }
    if (hasReleaseHook &&
        (!compiler_class_set_type_metadata_string_field(cs, info, "ffiReleaseHook", releaseHookValue) ||
         !compiler_class_append_builtin_decorator_name(cs, info, "zr.ffi.releaseHook"))) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool compiler_class_validate_interface_const_fields(SZrCompilerState *cs,
                                                              const SZrTypePrototypeInfo *classInfo,
                                                              const SZrArray *inherits,
                                                              SZrFileRange errorLocation) {
    if (cs == ZR_NULL || classInfo == ZR_NULL || inherits == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize inheritIndex = 0; inheritIndex < inherits->length; inheritIndex++) {
        SZrString **inheritTypeNamePtr =
                (SZrString **)ZrCore_Array_Get((SZrArray *)inherits, inheritIndex);
        SZrString *inheritTypeName =
                inheritTypeNamePtr != ZR_NULL ? *inheritTypeNamePtr : ZR_NULL;
        SZrTypePrototypeInfo *inheritInfo;

        if (inheritTypeName == ZR_NULL) {
            continue;
        }

        inheritInfo = find_compiler_type_prototype(cs, inheritTypeName);
        if (inheritInfo == ZR_NULL || inheritInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
            continue;
        }

        for (TZrSize memberIndex = 0; memberIndex < inheritInfo->members.length; memberIndex++) {
            SZrTypeMemberInfo *interfaceMember =
                    (SZrTypeMemberInfo *)ZrCore_Array_Get(&inheritInfo->members, memberIndex);
            SZrTypeMemberInfo *classMember;
            TZrNativeString fieldNameText;
            TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

            if (interfaceMember == ZR_NULL || interfaceMember->name == ZR_NULL ||
                interfaceMember->memberType != ZR_AST_CLASS_FIELD || !interfaceMember->isConst) {
                continue;
            }

            classMember = compiler_class_find_declared_member((SZrTypePrototypeInfo *)classInfo, interfaceMember->name);
            if (classMember != ZR_NULL && classMember->isConst) {
                continue;
            }

            fieldNameText = ZrCore_String_GetNativeStringShort(interfaceMember->name);
            if (fieldNameText != ZR_NULL) {
                snprintf(errorMsg,
                         sizeof(errorMsg),
                         "Interface const field '%s' must remain const in implementing class",
                         fieldNameText);
            } else {
                snprintf(errorMsg,
                         sizeof(errorMsg),
                         "Interface const field must remain const in implementing class");
            }
            ZrParser_Compiler_Error(cs, errorMsg, errorLocation);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void compiler_class_append_parameter_type(SZrCompilerState *cs,
                                                 SZrArray *parameterTypes,
                                                 SZrType *typeInfo) {
    SZrInferredType paramType;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    if (typeInfo != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &paramType)) {
        ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
}

static void compiler_class_collect_parameter_types(SZrCompilerState *cs,
                                                   SZrArray *parameterTypes,
                                                   SZrAstNodeArray *params,
                                                   SZrAstNode *functionNode) {
    SZrAstNode *previousFunctionNode;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    previousFunctionNode = cs->currentFunctionNode;
    if (functionNode != ZR_NULL) {
        cs->currentFunctionNode = functionNode;
    }

    if (params == ZR_NULL || params->count == 0) {
        cs->currentFunctionNode = previousFunctionNode;
        return;
    }

    ZrCore_Array_Init(cs->state, parameterTypes, sizeof(SZrInferredType), params->count);
    for (TZrSize paramIndex = 0; paramIndex < params->count; paramIndex++) {
        SZrAstNode *paramNode = params->nodes[paramIndex];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        compiler_class_append_parameter_type(cs, parameterTypes, paramNode->data.parameter.typeInfo);
    }

    cs->currentFunctionNode = previousFunctionNode;
}

void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CLASS_DECLARATION) {
        ZrParser_Statement_Compile(cs, node);
        return;
    }
    
    SZrClassDeclaration *classDecl = &node->data.classDeclaration;
    
    // 获取类型名称
    if (classDecl->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = classDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class name is null", node->location);
        return;
    }
    
    // 设置当前类型名称（用于成员字段 const 检查）
    SZrString *oldTypeName = cs->currentTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    SZrAstNode *oldTypeNode = cs->currentTypeNode;
    cs->currentTypeName = typeName;
    cs->currentTypeNode = node;
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    info.accessModifier = classDecl->accessModifier;
    info.modifierFlags = classDecl->modifierFlags;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    info.hasDecoratorMetadata = ZR_FALSE;
    info.nextVirtualSlotIndex = 0;
    info.nextPropertyIdentity = 0;
    info.layoutByteSize = 0;
    info.layoutByteAlign = 0;
    ZrCore_Value_ResetAsNull(&info.decoratorMetadataValue);

    if (cs->typeEnv != ZR_NULL &&
        !ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                cs->state, cs->typeEnv, typeName, node)) {
        ZrParser_Compiler_Error(cs, "Failed to register class symbol before expansion", node->location);
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    // 初始化继承数组
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state,
                      &info.genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      classDecl->generic != ZR_NULL && classDecl->generic->params != ZR_NULL
                              ? classDecl->generic->params->count
                              : 1);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    compiler_collect_generic_parameter_info(cs, &info.genericParameters, classDecl->generic);
    if (info.genericParameters.length > 0u) {
        info.modifierFlags |= ZR_TYPE_MODIFIER_FLAG_OPEN_GENERIC;
    }
    SZrString *primarySuperTypeName = ZR_NULL;
    // 处理继承关系
    if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
            SZrAstNode *inheritType = classDecl->inherits->nodes[i];
            SZrString *inheritTypeName =
                    inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE
                            ? extract_type_name_string(cs, &inheritType->data.type)
                            : ZR_NULL;
            if (inheritTypeName != ZR_NULL) {
                SZrTypePrototypeInfo *inheritPrototype = find_compiler_type_prototype(cs, inheritTypeName);
                if (inheritPrototype != ZR_NULL && inheritPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
                    ZrCore_Array_Push(cs->state, &info.implements, &inheritTypeName);
                }
                if (primarySuperTypeName == ZR_NULL &&
                    (inheritPrototype == ZR_NULL || inheritPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE)) {
                    primarySuperTypeName = inheritTypeName;
                }
                ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
            }
        }
    }
    if (primarySuperTypeName == ZR_NULL) {
        primarySuperTypeName = ZrCore_String_Create(cs->state,
                                                    "zr.builtin.Object",
                                                    strlen("zr.builtin.Object"));
        if (primarySuperTypeName != ZR_NULL) {
            ZrCore_Array_Push(cs->state, &info.inherits, &primarySuperTypeName);
        }
    }
    info.extendsTypeName = primarySuperTypeName;
    if (info.extendsTypeName != ZR_NULL) {
        SZrTypePrototypeInfo *superPrototype = find_compiler_type_prototype(cs, info.extendsTypeName);
        if (superPrototype != ZR_NULL) {
            info.nextVirtualSlotIndex = superPrototype->nextVirtualSlotIndex;
            info.nextPropertyIdentity = superPrototype->nextPropertyIdentity;
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (classDecl->members != ZR_NULL && classDecl->members->count > 0) {
        for (TZrUInt32 memberPhase = 0U; memberPhase < 3U; memberPhase++) {
            for (TZrSize i = 0; i < classDecl->members->count; i++) {
                SZrAstNode *member = classDecl->members->nodes[i];
                TZrUInt32 actualPhase;
                if (member == ZR_NULL) {
                    continue;
                }
                actualPhase = member->type == ZR_AST_CLASS_FIELD
                                      ? 0U
                                      : (member->type == ZR_AST_CLASS_META_FUNCTION ? 2U : 1U);
                if (actualPhase != memberPhase) {
                    continue;
                }

            if (!compiler_test_validate_non_module_roles(cs, member)) {
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                cs->currentTypeNode = oldTypeNode;
                return;
            }

            if (member->type == ZR_AST_PROPERTY_DECLARATION) {
                if (!compiler_property_bind(
                            cs,
                            &info,
                            member,
                            typeName,
                            primarySuperTypeName,
                            ZR_COMPILER_PROPERTY_CONTAINER_CLASS,
                            (TZrUInt32)i,
                            ZR_TRUE)) {
                    cs->currentTypeName = oldTypeName;
                    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                    cs->currentTypeNode = oldTypeNode;
                    return;
                }
                continue;
            }
            if (member->type == ZR_AST_CLASS_PROPERTY) {
                ZrParser_Compiler_Error(
                        cs,
                        "legacy property accessor syntax is not a semantic source",
                        member->location);
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                cs->currentTypeNode = oldTypeNode;
                return;
            }
            
            SZrTypeMemberInfo memberInfo;
            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;

            // 初始化所有字段
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.modifierFlags = ZR_DECLARATION_MODIFIER_NONE;
            memberInfo.isConst = ZR_FALSE;
            memberInfo.reservedRemovedUsingManaged = ZR_FALSE;
            memberInfo.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.gcBridgeKind = ZR_GC_BRIDGE_NONE;
            memberInfo.receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
            memberInfo.callsClose = ZR_FALSE;
            memberInfo.callsDestructor = ZR_FALSE;
            memberInfo.declarationOrder = (TZrUInt32)i;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            memberInfo.fieldType = ZR_NULL;
            memberInfo.fieldTypeName = ZR_NULL;
            memberInfo.fieldOffset = 0;
            memberInfo.fieldSize = 0;
            memberInfo.compiledFunction = ZR_NULL;
            memberInfo.functionConstantIndex = 0;
            memberInfo.parameterCount = 0;
            ZrCore_Array_Construct(&memberInfo.parameterTypes);
            ZrCore_Array_Construct(&memberInfo.genericParameters);
            ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
            ZrCore_Array_Construct(&memberInfo.decorators);
            memberInfo.hasDecoratorMetadata = ZR_FALSE;
            ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
            memberInfo.declarationNode = member;
            memberInfo.metaType = ZR_META_ENUM_MAX;
            memberInfo.isMetaMethod = ZR_FALSE;
            memberInfo.returnTypeName = ZR_NULL;
            memberInfo.ownerTypeName = typeName;
            memberInfo.baseDefinitionOwnerTypeName = typeName;
            memberInfo.baseDefinitionName = ZR_NULL;
            memberInfo.virtualSlotIndex = compiler_member_virtual_slot_none();
            memberInfo.interfaceContractSlot = compiler_member_interface_contract_slot_none();
            memberInfo.propertyIdentity = compiler_member_property_identity_none();
            memberInfo.accessorRole = 0;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_CLASS_FIELD: {
                    SZrClassField *field = &member->data.classField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic; // class字段也有isStatic
                    memberInfo.isConst = field->isConst;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }
                    // 处理字段类型信息
                    if (field->typeInfo != ZR_NULL) {
                        EZrOwnershipQualifier intrinsicOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                        const SZrType *intrinsicInnerType = ZR_NULL;
                        EZrGcBridgeKind intrinsicGcBridgeKind = ZR_GC_BRIDGE_NONE;
                        const SZrType *intrinsicGcBridgeInnerType = ZR_NULL;
                        memberInfo.fieldType = field->typeInfo;
                        memberInfo.fieldTypeName = extract_type_name_string(cs, field->typeInfo);
                        memberInfo.ownershipQualifier = field->typeInfo->ownershipQualifier;
                        if (ZrParser_AstType_TryUnwrapOwnershipGeneric(field->typeInfo,
                                                                       &intrinsicOwnershipQualifier,
                                                                       &intrinsicInnerType)) {
                            ZR_UNUSED_PARAMETER(intrinsicInnerType);
                            memberInfo.ownershipQualifier = intrinsicOwnershipQualifier;
                        }
                        if (ZrParser_AstType_TryUnwrapGcBridgeGeneric(
                                    field->typeInfo,
                                    &intrinsicGcBridgeKind,
                                    &intrinsicGcBridgeInnerType)) {
                            memberInfo.gcBridgeKind = intrinsicGcBridgeKind;
                            memberInfo.fieldTypeName = extract_type_name_string(
                                    cs, (SZrType *)intrinsicGcBridgeInnerType);
                        }
                        memberInfo.fieldSize = calculate_type_size(cs, field->typeInfo);
                    } else if (field->init != ZR_NULL) {
                        // 没有类型注解，从初始值推断类型
                        SZrInferredType inferredType;
                        if (ZrParser_ExpressionType_Infer(cs, field->init, &inferredType)) {
                            memberInfo.fieldTypeName = get_type_name_from_inferred_type(cs, &inferredType);
                            memberInfo.ownershipQualifier = inferredType.ownershipQualifier;
                            memberInfo.gcBridgeKind = inferredType.gcBridgeKind;
                            // 根据推断类型计算字段大小
                            switch (inferredType.baseType) {
                                case ZR_VALUE_TYPE_INT8: memberInfo.fieldSize = sizeof(TZrInt8); break;
                                case ZR_VALUE_TYPE_INT16: memberInfo.fieldSize = sizeof(TZrInt16); break;
                                case ZR_VALUE_TYPE_INT32: memberInfo.fieldSize = sizeof(TZrInt32); break;
                                case ZR_VALUE_TYPE_INT64: memberInfo.fieldSize = sizeof(TZrInt64); break;
                                case ZR_VALUE_TYPE_UINT8: memberInfo.fieldSize = sizeof(TZrUInt8); break;
                                case ZR_VALUE_TYPE_UINT16: memberInfo.fieldSize = sizeof(TZrUInt16); break;
                                case ZR_VALUE_TYPE_UINT32: memberInfo.fieldSize = sizeof(TZrUInt32); break;
                                case ZR_VALUE_TYPE_UINT64: memberInfo.fieldSize = sizeof(TZrUInt64); break;
                                case ZR_VALUE_TYPE_FLOAT: memberInfo.fieldSize = sizeof(TZrFloat32); break;
                                case ZR_VALUE_TYPE_DOUBLE: memberInfo.fieldSize = sizeof(TZrDouble); break;
                                case ZR_VALUE_TYPE_BOOL: memberInfo.fieldSize = sizeof(TZrBool); break;
                                case ZR_VALUE_TYPE_STRING:
                                case ZR_VALUE_TYPE_OBJECT:
                                default:
                                    memberInfo.fieldSize = sizeof(TZrPtr); // 指针大小
                                    break;
                            }
                            ZrParser_InferredType_Free(cs->state, &inferredType);
                        } else {
                            // 类型推断失败，默认为object类型
                            memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                            memberInfo.fieldSize = sizeof(TZrPtr);
                        }
                    } else {
                        // 没有类型注解和初始值，默认为object类型（8字节指针）
                        memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                        memberInfo.fieldSize = sizeof(TZrPtr);
                    }

                    if (memberInfo.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
                        memberInfo.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED) {
                        memberInfo.callsClose = ZR_TRUE;
                        memberInfo.callsDestructor = ZR_TRUE;
                    }
                    break;
                }
                case ZR_AST_CLASS_METHOD: {
                    SZrClassMethod *method = &member->data.classMethod;
                    SZrString *inferredReturnTypeName = ZR_NULL;
                    SZrAstNode *previousFunctionNode = cs->currentFunctionNode;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    memberInfo.modifierFlags = method->modifierFlags;
                    memberInfo.receiverQualifier = method->receiverQualifier;
                    memberInfo.receiverEffect = get_member_receiver_effect(member);
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    cs->currentFunctionNode = member;
                    // 处理返回类型信息
                    compiler_type_member_capture_structured_return_type(
                            cs,
                            &memberInfo,
                            method->returnType);
                    ZrCore_Array_Init(cs->state,
                                      &memberInfo.genericParameters,
                                      sizeof(SZrTypeGenericParameterInfo),
                                      method->generic != ZR_NULL && method->generic->params != ZR_NULL
                                              ? method->generic->params->count
                                              : 1);
                    compiler_collect_generic_parameter_info(cs, &memberInfo.genericParameters, method->generic);
                    compiler_class_collect_parameter_types(cs, &memberInfo.parameterTypes, method->params, member);
                    compiler_collect_parameter_passing_modes(cs->state, &memberInfo.parameterPassingModes, method->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    cs->currentFunctionNode = previousFunctionNode;
                    {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMethod =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !method->isStatic, &compiledParameterCount,
                                                              method->returnType == ZR_NULL ? &inferredReturnTypeName
                                                                                            : ZR_NULL);
                        if (compiledMethod == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            cs->currentTypeNode = oldTypeNode;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMethod));
                        memberInfo.compiledFunction = compiledMethod;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        ZR_UNUSED_PARAMETER(compiledParameterCount);
                    }
                    if (memberInfo.returnTypeName == ZR_NULL && inferredReturnTypeName != ZR_NULL) {
                        memberInfo.returnTypeName = inferredReturnTypeName;
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_CLASS_META_FUNCTION: {
                    SZrClassMetaFunction *metaFunc = &member->data.classMetaFunction;
                    SZrString *inferredReturnTypeName = ZR_NULL;
                    SZrAstNode *previousFunctionNode = cs->currentFunctionNode;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    memberInfo.receiverEffect = get_member_receiver_effect(member);
                    memberInfo.modifierFlags = metaFunc->modifierFlags;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        memberInfo.metaType = compiler_resolve_meta_type_name(metaFunc->meta->name);
                        memberInfo.isMetaMethod = memberInfo.metaType != ZR_META_ENUM_MAX;
                    }
                    if ((classDecl->modifierFlags & ZR_DECLARATION_MODIFIER_RESOURCE) != 0U &&
                        memberInfo.metaType == ZR_META_DESTRUCTOR &&
                        cfg_node_may_enter_catch(metaFunc->body)) {
                        ZrParser_Compiler_Error(
                                cs,
                                "Resource Drop body must be non-throwing",
                                metaFunc->body != ZR_NULL ? metaFunc->body->location : member->location);
                        cs->currentTypeName = oldTypeName;
                        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                        cs->currentTypeNode = oldTypeNode;
                        return;
                    }
                    cs->currentFunctionNode = member;
                    // 处理返回类型信息
                    compiler_type_member_capture_structured_return_type(
                            cs,
                            &memberInfo,
                            metaFunc->returnType);
                    compiler_class_collect_parameter_types(cs, &memberInfo.parameterTypes, metaFunc->params, member);
                    compiler_collect_parameter_passing_modes(cs->state,
                                                             &memberInfo.parameterPassingModes,
                                                             metaFunc->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    cs->currentFunctionNode = previousFunctionNode;
                    if (memberInfo.isMetaMethod) {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMeta =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !metaFunc->isStatic, &compiledParameterCount,
                                                              (metaFunc->returnType == ZR_NULL &&
                                                               memberInfo.metaType != ZR_META_CONSTRUCTOR)
                                                                      ? &inferredReturnTypeName
                                                                      : ZR_NULL);
                        if (compiledMeta == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            cs->currentTypeNode = oldTypeNode;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMeta));
                        memberInfo.compiledFunction = compiledMeta;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        ZR_UNUSED_PARAMETER(compiledParameterCount);
                    }
                    if (memberInfo.returnTypeName == ZR_NULL && inferredReturnTypeName != ZR_NULL) {
                        memberInfo.returnTypeName = inferredReturnTypeName;
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }

            if ((member->type == ZR_AST_CLASS_FIELD || member->type == ZR_AST_CLASS_METHOD ||
                 member->type == ZR_AST_CLASS_PROPERTY) &&
                !ZrParser_CompileTime_ApplyMemberDecorators(
                        cs,
                        member,
                        member->type == ZR_AST_CLASS_FIELD
                                ? member->data.classField.decorators
                                : (member->type == ZR_AST_CLASS_METHOD
                                           ? member->data.classMethod.decorators
                                           : member->data.classProperty.decorators),
                        &memberInfo)) {
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                cs->currentTypeNode = oldTypeNode;
                return;
            }
            if ((member->type == ZR_AST_CLASS_FIELD || member->type == ZR_AST_CLASS_METHOD ||
                 member->type == ZR_AST_CLASS_PROPERTY) &&
                !ZrParser_Metadata_ApplyMemberAttributes(
                        cs,
                        member->type == ZR_AST_CLASS_FIELD
                                ? member->data.classField.decorators
                                : (member->type == ZR_AST_CLASS_METHOD
                                           ? member->data.classMethod.decorators
                                           : member->data.classProperty.decorators),
                        member->type == ZR_AST_CLASS_FIELD
                                ? ZR_PARSER_ATTRIBUTE_TARGET_FIELD
                                : (member->type == ZR_AST_CLASS_METHOD
                                           ? ZR_PARSER_ATTRIBUTE_TARGET_METHOD
                                           : ZR_PARSER_ATTRIBUTE_TARGET_PROPERTY),
                        &memberInfo,
                        member->location)) {
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                cs->currentTypeNode = oldTypeNode;
                return;
            }

            if (memberInfo.name != ZR_NULL) {
                if (((member->type == ZR_AST_CLASS_FIELD &&
                     !compiler_type_member_register_field_symbol(cs, &memberInfo)) ||
                     (member->type == ZR_AST_CLASS_METHOD &&
                     !compiler_type_member_register_function_symbol(cs, &memberInfo)))) {
                    ZrParser_Compiler_Error(
                            cs,
                            "Failed to register canonical class member symbol",
                            member->location);
                    cs->currentTypeName = oldTypeName;
                    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                    cs->currentTypeNode = oldTypeNode;
                    return;
                }
                ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
            }
        }
        }
    }
    compiler_type_members_restore_declaration_order(&info.members);

    if (!ZrParser_Compiler_ApplyCompileTimeTypeDecorators(cs, node, classDecl->decorators, &info)) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }
    if (!ZrParser_Metadata_ApplyTypeAttributes(
                cs, classDecl->decorators, &info, node->location)) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    if (compiler_class_has_modifier(&info, ZR_DECLARATION_MODIFIER_ABSTRACT)) {
        info.allowValueConstruction = ZR_FALSE;
        info.allowBoxedConstruction = ZR_FALSE;
    }

    if (compiler_class_has_modifier(&info, ZR_DECLARATION_MODIFIER_ABSTRACT) &&
        compiler_class_has_modifier(&info, ZR_DECLARATION_MODIFIER_FINAL)) {
        ZrParser_Compiler_Error(cs, "abstract final class is not allowed", node->location);
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    if (info.extendsTypeName != ZR_NULL) {
        SZrTypePrototypeInfo *superPrototype = find_compiler_type_prototype(cs, info.extendsTypeName);
        if (superPrototype != ZR_NULL &&
            compiler_class_has_modifier(superPrototype, ZR_DECLARATION_MODIFIER_FINAL)) {
            ZrParser_Compiler_Error(cs, "Cannot inherit from a final class", node->location);
            cs->currentTypeName = oldTypeName;
            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
            cs->currentTypeNode = oldTypeNode;
            return;
        }
    }

    if (classDecl->members != ZR_NULL && classDecl->members->count > 0) {
        for (TZrSize memberIndex = 0; memberIndex < info.members.length; memberIndex++) {
            SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info.members, memberIndex);
            if (memberInfo == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
                continue;
            }
            if (!compiler_class_validate_member_override_semantics(cs,
                                                                   &info,
                                                                   memberInfo,
                                                                   memberInfo->declarationNode->location)) {
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                cs->currentTypeNode = oldTypeNode;
                return;
            }
        }
    }

    for (TZrSize inheritIndex = 0; inheritIndex < info.inherits.length; inheritIndex++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&info.inherits, inheritIndex);
        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }

        if (!compiler_class_validate_required_members_recursive(cs, &info, *inheritNamePtr, 0)) {
            cs->currentTypeName = oldTypeName;
            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
            cs->currentTypeNode = oldTypeNode;
            return;
        }
    }

    if (!compiler_class_validate_interface_const_fields(
                cs, &info, &info.inherits, node->location)) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    if (!compiler_class_apply_builtin_ffi_wrapper_decorators(cs, classDecl->decorators, &info, node->location)) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }

    compiler_class_lint_process_local_shared_cycles(cs, &info);

    // 将 prototype 信息添加到数组
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);

    emit_class_static_field_initializers(cs, node);
    if (cs->hasError) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        cs->currentTypeNode = oldTypeNode;
        return;
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
    cs->currentTypeNode = oldTypeNode;
}

// 序列化的prototype信息结构（紧凑二进制格式）
typedef struct SZrSerializedPrototypeInfo {
    // 基本信息
    TZrUInt32 nameStringIndex;              // 类型名称字符串在常量池中的索引
    TZrUInt32 type;                         // EZrObjectPrototypeType
    TZrUInt32 accessModifier;               // EZrAccessModifier
    
    // 继承关系
    TZrUInt32 inheritsCount;                // 继承类型数量
    TZrUInt32 *inheritStringIndices;        // 继承类型名称字符串索引数组（动态分配）
    
    // 成员信息
    TZrUInt32 membersCount;                 // 成员数量
    // 成员数据紧随其后（动态数组）
} SZrSerializedPrototypeInfo;

// 序列化的成员信息结构（紧凑二进制格式）
typedef struct SZrSerializedMemberInfo {
    TZrUInt32 memberType;                   // EZrAstNodeType
    TZrUInt32 nameStringIndex;              // 成员名称字符串在常量池中的索引（如果为0表示无名）
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 isStatic;                     // TZrBool (0或1)
    
    // 字段特定信息（仅当memberType为STRUCT_FIELD或CLASS_FIELD时有效）
    TZrUInt32 fieldTypeNameStringIndex;     // 字段类型名称字符串索引（如果为0表示无类型名）
    TZrUInt32 fieldOffset;                  // 字段偏移量
    TZrUInt32 fieldSize;                    // 字段大小
    
    // 方法特定信息（仅当memberType为METHOD或META_FUNCTION时有效）
    TZrUInt32 isMetaMethod;                 // TZrBool (0或1)
    TZrUInt32 metaType;                     // EZrMetaType
    TZrUInt32 functionConstantIndex;        // 函数在常量池中的索引
    TZrUInt32 parameterCount;               // 参数数量
} SZrSerializedMemberInfo;

// 将prototype信息序列化为二进制数据（不存储到常量池）
// 编译时使用C原生结构，避免创建VM对象，提高编译速度
// 运行时（module.c）会从 function->prototypeData 读取并创建VM对象
// 返回：ZR_TRUE 表示成功，ZR_FALSE 表示失败
// 注意：outData 指向的内存需要调用者释放
