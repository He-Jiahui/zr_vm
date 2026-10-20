#include "compiler/compiler_internal.h"
#include "compiler/expression/compile_expression_internal.h"
#include "compiler/declaration/compiler_interface_contracts.h"
#include "compiler/compiler_type_prototype_lookup.h"
#include "type_inference/type_inference_generic_prototype.h"
#include "type_inference/type_inference_nominal_prototype.h"

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
    /* Native contracts have metadata identities, without source symbol ranges.
     * Their checked implementation is bound through the interface contract slot. */
    if (targetMember->declarationNode == ZR_NULL &&
        targetMember->symbolId == ZR_SEMANTIC_ID_INVALID &&
        targetMember->metadataToken != 0u) {
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
                member->declarationNode == ZR_NULL ||
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

static SZrType *compiler_semantic_relations_inherit_annotation(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *source, TZrSize ordinal) {
    SZrAstNodeArray *inherits = ZR_NULL;
    TZrSize filteredIndex = 0U;

    if (source->declarationNode != ZR_NULL) {
        if (source->declarationNode->type == ZR_AST_CLASS_DECLARATION) {
            inherits = source->declarationNode->data.classDeclaration.inherits;
        } else if (source->declarationNode->type == ZR_AST_STRUCT_DECLARATION) {
            inherits = source->declarationNode->data.structDeclaration.inherits;
        }
    }
    for (TZrSize index = 0U; inherits != ZR_NULL && index < inherits->count; index++) {
        SZrAstNode *node = inherits->nodes[index];

        if (node == ZR_NULL || node->type != ZR_AST_TYPE ||
            extract_type_name_string(cs, &node->data.type) == ZR_NULL) {
            continue;
        }
        if (filteredIndex++ == ordinal) {
            return &node->data.type;
        }
    }
    return ZR_NULL;
}

static TZrBool compiler_semantic_relations_canonical_declaration(
        SZrSemanticContext *context, TZrTypeId definitionTypeId, TZrSymbolId symbolId,
        const SZrAstNode **outDeclaration) {
    TZrSymbolId matchedSymbolId = ZR_SEMANTIC_ID_INVALID;

    *outDeclaration = ZR_NULL;
    if (symbolId != ZR_SEMANTIC_ID_INVALID) {
        const SZrSemanticSymbolRecord *symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);

        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->typeId != definitionTypeId || symbol->astNode == ZR_NULL) {
            return ZR_FALSE;
        }
        *outDeclaration = symbol->astNode;
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol = ZrCore_Array_Get(&context->symbols, index);

        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->typeId != definitionTypeId || symbol->id == ZR_SEMANTIC_ID_INVALID ||
            symbol->astNode == ZR_NULL) {
            continue;
        }
        if (matchedSymbolId != ZR_SEMANTIC_ID_INVALID && matchedSymbolId != symbol->id) {
            return ZR_FALSE;
        }
        matchedSymbolId = symbol->id;
        *outDeclaration = symbol->astNode;
    }
    return ZR_TRUE;
}

static SZrTypePrototypeInfo *compiler_semantic_relations_find_instance_target(
        SZrCompilerState *cs, TZrTypeId instanceTypeId) {
    SZrTypePrototypeInfo *sourceMatch = ZR_NULL;
    SZrTypePrototypeInfo *importMatch = ZR_NULL;

    for (TZrSize index = 0U; index <= cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = index < cs->typePrototypes.length
                ? ZrCore_Array_Get(&cs->typePrototypes, index) : cs->currentTypePrototypeInfo;

        if (prototype == ZR_NULL || prototype->genericInstanceTypeId != instanceTypeId) {
            continue;
        }
        if (type_inference_prototype_is_root_visible(prototype) &&
            type_inference_generic_prototype_is_root_visible(cs, prototype)) {
            return prototype;
        }
        if (!prototype->isImportedNative && sourceMatch == ZR_NULL) {
            sourceMatch = prototype;
        } else if (prototype->isImportedNative && importMatch == ZR_NULL) {
            importMatch = prototype;
        }
    }
    return sourceMatch != ZR_NULL ? sourceMatch : importMatch;
}

static TZrBool compiler_semantic_relations_resolve_type_fact(
        SZrCompilerState *cs, SZrType *annotation, SZrString *legacyTypeName,
        TZrTypeId typeId, TZrSymbolId symbolId, SZrTypePrototypeInfo **outPrototype) {
    const SZrCanonicalTypeNode *canonical = ZrParser_CanonicalType_Find(cs->semanticContext, typeId);
    const SZrAstNode *declaration;
    TZrTypeId definitionTypeId;
    TZrBool isInstance;

    *outPrototype = ZR_NULL;
    if (canonical == ZR_NULL) {
        return ZR_FALSE;
    }
    isInstance = canonical->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE;
    if (!isInstance && canonical->kind != ZR_CANONICAL_TYPE_NOMINAL) {
        return ZR_TRUE;
    }
    definitionTypeId = isInstance ? canonical->data.genericInstance.definitionTypeId : typeId;
    if (!compiler_semantic_relations_canonical_declaration(
                cs->semanticContext, definitionTypeId, symbolId, &declaration)) {
        return ZR_FALSE;
    }
    if (isInstance) {
        *outPrototype = compiler_semantic_relations_find_instance_target(cs, typeId);
        if (*outPrototype == ZR_NULL) {
            /* A resolved forward use can predate the definition's prototype cache. */
            if (!compiler_resolve_type_annotation_prototype(
                        cs, annotation, legacyTypeName, outPrototype)) {
                return ZR_FALSE;
            }
            if (*outPrototype != ZR_NULL && (*outPrototype)->genericInstanceTypeId != typeId) {
                *outPrototype = ZR_NULL;
                return ZR_FALSE;
            }
        }
        if (*outPrototype != ZR_NULL && (*outPrototype)->declarationNode != ZR_NULL &&
            (*outPrototype)->declarationNode != declaration) {
            *outPrototype = ZR_NULL;
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (declaration == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index <= cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = index < cs->typePrototypes.length
                ? ZrCore_Array_Get(&cs->typePrototypes, index) : cs->currentTypePrototypeInfo;

        if (prototype != ZR_NULL && prototype->declarationNode == declaration &&
            prototype->genericInstanceTypeId == ZR_SEMANTIC_ID_INVALID) {
            *outPrototype = prototype;
            break;
        }
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_relations_resolve_inherit_target(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *source, SZrType *annotation,
        SZrString *legacyTypeName, SZrTypePrototypeInfo **outPrototype) {
    SZrAstNode *oldTypeNode = cs->currentTypeNode;
    SZrString *oldTypeName = cs->currentTypeName;
    SZrTypePrototypeInfo *oldPrototype = cs->currentTypePrototypeInfo;
    TZrSize oldPrototypeIndex = ZR_MAX_SIZE;
    const SZrSemanticReferenceFact *fact;
    TZrBool resolved;

    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        if (oldPrototype == ZrCore_Array_Get(&cs->typePrototypes, index)) {
            oldPrototypeIndex = index;
            break;
        }
    }
    cs->currentTypeNode = (SZrAstNode *)source->declarationNode;
    cs->currentTypeName = source->name;
    cs->currentTypePrototypeInfo = (SZrTypePrototypeInfo *)source;
    fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
    if (fact != ZR_NULL && fact->isResolved) {
        TZrTypeId typeId = fact->typeId;
        TZrSymbolId symbolId = fact->symbolId;

        resolved = compiler_semantic_relations_resolve_type_fact(
                cs, annotation, legacyTypeName, typeId, symbolId, outPrototype);
    } else {
        resolved = compiler_resolve_type_annotation_prototype(
                cs, annotation, legacyTypeName, outPrototype);
    }
    cs->currentTypeNode = oldTypeNode;
    cs->currentTypeName = oldTypeName;
    /* Materialization may relocate the table containing the prior owner view. */
    cs->currentTypePrototypeInfo = oldPrototypeIndex != ZR_MAX_SIZE
            ? ZrCore_Array_Get(&cs->typePrototypes, oldPrototypeIndex) : oldPrototype;
    return resolved;
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
            SZrType *annotation = compiler_semantic_relations_inherit_annotation(
                    compilerState, &sourceSnapshot, inheritIndex);
            SZrTypePrototypeInfo *targetPrototype = ZR_NULL;
            SZrTypePrototypeInfo targetSnapshot;

            if (targetName == ZR_NULL || *targetName == ZR_NULL) {
                continue;
            }
            if (annotation != ZR_NULL) {
                if (!compiler_semantic_relations_resolve_inherit_target(
                            compilerState, &sourceSnapshot, annotation, *targetName, &targetPrototype)) {
                    return ZR_FALSE;
                }
            } else {
                targetPrototype = find_compiler_type_prototype(compilerState, *targetName);
            }
            if (targetPrototype == ZR_NULL ||
                (annotation != ZR_NULL && targetPrototype->declarationNode == ZR_NULL)) {
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
