#include "compiler_internal.h"

#include "zr_vm_parser/semantic_relations.h"

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
