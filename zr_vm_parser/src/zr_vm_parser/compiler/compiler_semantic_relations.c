#include "compiler_internal.h"
#include "compile_expression_internal.h"
#include "compiler_interface_contracts.h"

#include "zr_vm_parser/semantic_relations.h"

static TZrBool compiler_semantic_relations_prototypes_have_same_identity(
        const SZrTypePrototypeInfo *left,
        const SZrTypePrototypeInfo *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return left->name != ZR_NULL && right->name != ZR_NULL &&
           ZrCore_String_Equal(left->name, right->name);
}

static const SZrSemanticSymbolRecord *
compiler_semantic_relations_find_source_type_symbol(
        const SZrSemanticContext *context,
        const SZrTypePrototypeInfo *prototype) {
    const SZrSemanticSymbolRecord *matched = ZR_NULL;
    TZrSize index;

    if (context == ZR_NULL || prototype == ZR_NULL ||
        prototype->name == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol == ZR_NULL ||
            symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->astNode == ZR_NULL ||
            symbol->id == ZR_SEMANTIC_ID_INVALID ||
            symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            symbol->name == ZR_NULL ||
            !ZrCore_String_Equal(symbol->name, prototype->name)) {
            continue;
        }
        if (matched != ZR_NULL && matched->id != symbol->id) {
            return ZR_NULL;
        }
        matched = symbol;
    }
    return matched;
}

static void compiler_semantic_relations_publish_type_contract(
        SZrCompilerState *cs,
        EZrSemanticRelationKind kind,
        const SZrTypePrototypeInfo *sourcePrototype,
        const SZrTypePrototypeInfo *targetPrototype) {
    const SZrSemanticSymbolRecord *sourceSymbol;
    const SZrSemanticSymbolRecord *targetSymbol;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourcePrototype == ZR_NULL || targetPrototype == ZR_NULL) {
        return;
    }
    if (sourcePrototype->declarationNode != ZR_NULL &&
        targetPrototype->declarationNode != ZR_NULL) {
        (void)ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
                cs->semanticContext,
                kind,
                sourcePrototype->declarationNode,
                targetPrototype->declarationNode);
        return;
    }
    sourceSymbol = compiler_semantic_relations_find_source_type_symbol(
            cs->semanticContext, sourcePrototype);
    targetSymbol = compiler_semantic_relations_find_source_type_symbol(
            cs->semanticContext, targetPrototype);
    if (sourceSymbol != ZR_NULL && targetSymbol != ZR_NULL) {
        (void)ZrParser_SemanticRelations_PublishSymbolRelation(
                cs->semanticContext,
                kind,
                sourceSymbol->id,
                targetSymbol->id);
    }
}

static const SZrTypeMemberInfo *compiler_semantic_relations_find_implementation(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *sourcePrototype,
        const SZrTypeMemberInfo *requiredMember,
        TZrUInt32 depth) {
    TZrSize index;

    if (cs == ZR_NULL || sourcePrototype == ZR_NULL ||
        requiredMember == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }
    for (index = 0U; index < sourcePrototype->members.length; index++) {
        const SZrTypeMemberInfo *candidate =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&sourcePrototype->members, index);
        if (candidate != ZR_NULL &&
            compiler_interface_contracts_member_signatures_match(
                    requiredMember, candidate) &&
            compiler_receiver_effect_can_implement(
                    requiredMember->receiverEffect,
                    candidate->receiverEffect) &&
            (candidate->modifierFlags &
             ZR_DECLARATION_MODIFIER_ABSTRACT) == 0U &&
            (!requiredMember->isConst || candidate->isConst)) {
            return candidate;
        }
    }
    if (sourcePrototype->extendsTypeName != ZR_NULL) {
        const SZrTypePrototypeInfo *basePrototype =
                find_compiler_type_prototype(
                        cs, sourcePrototype->extendsTypeName);
        if (!compiler_semantic_relations_prototypes_have_same_identity(
                    basePrototype, sourcePrototype)) {
            return compiler_semantic_relations_find_implementation(
                    cs, basePrototype, requiredMember, depth + 1U);
        }
    }
    return ZR_NULL;
}

static void compiler_semantic_relations_publish_interface_members(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *sourcePrototype,
        const SZrTypePrototypeInfo *interfacePrototype,
        TZrUInt32 depth) {
    SZrTypePrototypeInfo interfaceSnapshot;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourcePrototype == ZR_NULL || interfacePrototype == ZR_NULL ||
        interfacePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return;
    }
    interfaceSnapshot = *interfacePrototype;
    for (index = 0U; index < interfaceSnapshot.members.length; index++) {
        const SZrTypeMemberInfo *requiredMember =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &interfaceSnapshot.members, index);
        const SZrTypeMemberInfo *implementation =
                compiler_semantic_relations_find_implementation(
                        cs, sourcePrototype, requiredMember, 0U);

        if (requiredMember != ZR_NULL && implementation != ZR_NULL &&
            requiredMember->declarationNode != ZR_NULL &&
            implementation->declarationNode != ZR_NULL) {
            (void)ZrParser_SemanticRelations_PublishSymbolDeclarationRelation(
                    cs->semanticContext,
                    ZR_SEMANTIC_RELATION_IMPLEMENTATION,
                    implementation->declarationNode,
                    requiredMember->declarationNode);
        }
    }
    for (index = 0U; index < interfaceSnapshot.inherits.length; index++) {
        SZrString *const *parentName =
                (SZrString *const *)ZrCore_Array_Get(
                        &interfaceSnapshot.inherits, index);
        const SZrTypePrototypeInfo *parentPrototype =
                parentName != ZR_NULL && *parentName != ZR_NULL
                        ? find_compiler_type_prototype(cs, *parentName)
                        : ZR_NULL;
        if (parentPrototype != interfacePrototype) {
            compiler_semantic_relations_publish_interface_members(
                    cs,
                    sourcePrototype,
                    parentPrototype,
                    depth + 1U);
        }
    }
}

static void compiler_semantic_relations_publish_overrides(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *sourcePrototype) {
    SZrTypePrototypeInfo sourceSnapshot;
    const SZrTypePrototypeInfo *basePrototype;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourcePrototype == ZR_NULL ||
        sourcePrototype->extendsTypeName == ZR_NULL) {
        return;
    }
    sourceSnapshot = *sourcePrototype;
    basePrototype = find_compiler_type_prototype(
            cs, sourceSnapshot.extendsTypeName);
    if (basePrototype == ZR_NULL ||
        compiler_semantic_relations_prototypes_have_same_identity(
                basePrototype, &sourceSnapshot)) {
        return;
    }
    for (index = 0U; index < sourceSnapshot.members.length; index++) {
        const SZrTypeMemberInfo *sourceMember =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &sourceSnapshot.members, index);
        const SZrTypeMemberInfo *baseMember;

        if (sourceMember == ZR_NULL ||
            (sourceMember->modifierFlags &
             ZR_DECLARATION_MODIFIER_OVERRIDE) == 0U) {
            continue;
        }
        baseMember = compiler_semantic_relations_find_implementation(
                cs, basePrototype, sourceMember, 0U);
        if (baseMember != ZR_NULL &&
            sourceMember->declarationNode != ZR_NULL &&
            baseMember->declarationNode != ZR_NULL) {
            (void)ZrParser_SemanticRelations_PublishSymbolDeclarationRelation(
                    cs->semanticContext,
                    ZR_SEMANTIC_RELATION_OVERRIDE,
                    sourceMember->declarationNode,
                    baseMember->declarationNode);
        }
    }
}

TZrBool compiler_publish_type_hierarchy_relation(
        SZrCompilerState *cs,
        const SZrAstNode *sourceDeclaration,
        const SZrTypePrototypeInfo *targetPrototype,
        TZrBool isImplementation) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourceDeclaration == ZR_NULL || targetPrototype == ZR_NULL ||
        targetPrototype->declarationNode == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
            cs->semanticContext,
            isImplementation
                    ? ZR_SEMANTIC_RELATION_IMPLEMENTATION
                    : ZR_SEMANTIC_RELATION_BASE_TYPE,
            sourceDeclaration,
            targetPrototype->declarationNode);
}

TZrBool compiler_publish_member_override_relation(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *sourceMember,
        const SZrTypeMemberInfo *targetMember) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourceMember == ZR_NULL || targetMember == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrParser_SemanticRelations_PublishSymbolRelation(
            cs->semanticContext,
            ZR_SEMANTIC_RELATION_OVERRIDE,
            sourceMember->symbolId,
            targetMember->symbolId);
}

TZrBool compiler_publish_member_implementation_relation(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *sourceMember,
        const SZrTypeMemberInfo *targetMember) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourceMember == ZR_NULL || targetMember == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrParser_SemanticRelations_PublishSymbolRelation(
            cs->semanticContext,
            ZR_SEMANTIC_RELATION_IMPLEMENTATION,
            sourceMember->symbolId,
            targetMember->symbolId);
}

TZrBool compiler_publish_type_constructor_relation(
        SZrCompilerState *cs,
        const SZrAstNode *sourceTypeDeclaration,
        const SZrTypeMemberInfo *constructorMember) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourceTypeDeclaration == ZR_NULL || constructorMember == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrParser_SemanticRelations_PublishConstructorRelation(
            cs->semanticContext,
            sourceTypeDeclaration,
            constructorMember->symbolId);
}

TZrBool compiler_publish_source_constructor_relations(SZrCompilerState *cs) {
    TZrSize typeIndex;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }
    for (typeIndex = 0U; typeIndex < cs->typePrototypes.length; typeIndex++) {
        const SZrTypePrototypeInfo *typeInfo =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs->typePrototypes, typeIndex);
        TZrSize memberIndex;

        if (typeInfo == ZR_NULL || typeInfo->declarationNode == ZR_NULL) {
            continue;
        }
        for (memberIndex = 0U; memberIndex < typeInfo->members.length; memberIndex++) {
            const SZrTypeMemberInfo *member =
                    (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                            (SZrArray *)&typeInfo->members, memberIndex);

            if (member == ZR_NULL || !member->isMetaMethod ||
                member->metaType != ZR_META_CONSTRUCTOR ||
                member->symbolId == ZR_SEMANTIC_ID_INVALID) {
                continue;
            }
            if (!compiler_publish_type_constructor_relation(
                        cs, typeInfo->declarationNode, member)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticRelations_PublishCompilerContracts(
        SZrCompilerState *compilerState) {
    TZrSize sourceCount;
    TZrSize sourceIndex;

    if (compilerState == ZR_NULL ||
        compilerState->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    sourceCount = compilerState->typePrototypes.length;
    for (sourceIndex = 0U; sourceIndex < sourceCount; sourceIndex++) {
        const SZrTypePrototypeInfo *sourcePrototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &compilerState->typePrototypes, sourceIndex);
        SZrTypePrototypeInfo sourceSnapshot;
        TZrSize inheritIndex;

        if (sourcePrototype == ZR_NULL ||
            (sourcePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
             sourcePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT)) {
            continue;
        }
        sourceSnapshot = *sourcePrototype;
        compiler_semantic_relations_publish_overrides(
                compilerState, &sourceSnapshot);
        for (inheritIndex = 0U;
             inheritIndex < sourceSnapshot.inherits.length;
             inheritIndex++) {
            SZrString *const *targetName =
                    (SZrString *const *)ZrCore_Array_Get(
                            &sourceSnapshot.inherits,
                            inheritIndex);
            const SZrTypePrototypeInfo *targetPrototype =
                    targetName != ZR_NULL && *targetName != ZR_NULL
                            ? find_compiler_type_prototype(
                                    compilerState, *targetName)
                            : ZR_NULL;
            SZrTypePrototypeInfo targetSnapshot;

            if (targetPrototype == ZR_NULL) {
                continue;
            }
            targetSnapshot = *targetPrototype;
            compiler_semantic_relations_publish_type_contract(
                    compilerState,
                    ZR_SEMANTIC_RELATION_BASE_TYPE,
                    &sourceSnapshot,
                    &targetSnapshot);
            if (targetSnapshot.type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
                compiler_semantic_relations_publish_type_contract(
                        compilerState,
                        ZR_SEMANTIC_RELATION_IMPLEMENTATION,
                        &sourceSnapshot,
                        &targetSnapshot);
                compiler_semantic_relations_publish_interface_members(
                        compilerState,
                        &sourceSnapshot,
                        &targetSnapshot,
                        0U);
            }
        }
    }
    return ZR_TRUE;
}
