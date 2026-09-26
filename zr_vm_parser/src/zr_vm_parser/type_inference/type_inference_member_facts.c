#include "type_inference_internal.h"
#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_core/string.h"

static TZrBool member_owner_is_same_type(SZrCompilerState *cs,
                                        SZrString *left, SZrString *right) {
    SZrTypePrototypeInfo *leftType;
    SZrAstNode *leftDeclaration;
    SZrTypePrototypeInfo *rightType;
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrCore_String_Equal(left, right)) {
        return ZR_TRUE;
    }
    leftType = find_compiler_type_prototype_inference(cs, left);
    leftDeclaration = leftType != ZR_NULL ? leftType->declarationNode : ZR_NULL;
    rightType = find_compiler_type_prototype_inference(cs, right);
    return leftDeclaration != ZR_NULL && rightType != ZR_NULL &&
           leftDeclaration == rightType->declarationNode;
}

static TZrBool member_owner_derives_from(SZrCompilerState *cs,
                                         SZrString *typeName,
                                         SZrString *ownerName,
                                         TZrUInt32 depth) {
    SZrTypePrototypeInfo *type;
    SZrArray inherits;
    if (typeName == ZR_NULL || ownerName == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }
    if (member_owner_is_same_type(cs, typeName, ownerName)) {
        return ZR_TRUE;
    }
    type = find_compiler_type_prototype_inference(cs, typeName);
    if (type == ZR_NULL) {
        return ZR_FALSE;
    }
    inherits = type->inherits;
    for (TZrSize index = 0; index < inherits.length; index++) {
        SZrString **base = (SZrString **)ZrCore_Array_Get(&inherits, index);
        if (base != ZR_NULL && member_owner_derives_from(cs, *base, ownerName, depth + 1)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool type_inference_validate_field_access(
        SZrCompilerState *cs,
        SZrAstNode *primaryNode,
        SZrAstNode *memberNode,
        const SZrTypeMemberInfo *memberInfo,
        SZrString *receiverTypeName,
        TZrBool receiverIsPrototype) {
    TZrBool accessible;
    SZrAstNode *root;
    TZrBool isSuperReceiver;
    if (memberInfo == ZR_NULL || memberInfo->declarationNode == ZR_NULL ||
        (memberInfo->memberType != ZR_AST_CLASS_FIELD &&
         memberInfo->memberType != ZR_AST_STRUCT_FIELD) ||
        (memberInfo->declarationNode->type != ZR_AST_CLASS_FIELD &&
         memberInfo->declarationNode->type != ZR_AST_STRUCT_FIELD)) {
        return ZR_TRUE;
    }
    if (receiverIsPrototype && !memberInfo->isStatic) {
        ZrParser_Compiler_Error(cs, "Instance field requires an instance receiver", memberNode->location);
        return ZR_FALSE;
    }
    accessible = memberInfo->accessModifier == ZR_ACCESS_PUBLIC ||
                 member_owner_is_same_type(cs, cs->currentTypeName, memberInfo->ownerTypeName);
    root = primaryNode != ZR_NULL && primaryNode->type == ZR_AST_PRIMARY_EXPRESSION
                   ? primaryNode->data.primaryExpression.property : ZR_NULL;
    isSuperReceiver = root != ZR_NULL && root->type == ZR_AST_IDENTIFIER_LITERAL &&
                      primaryNode->data.primaryExpression.members != ZR_NULL &&
                      primaryNode->data.primaryExpression.members->count > 0 &&
                      primaryNode->data.primaryExpression.members->nodes[0] == memberNode &&
                      zr_string_equals_cstr(root->data.identifier.name, "super");
    if (!accessible && memberInfo->accessModifier == ZR_ACCESS_PROTECTED &&
        member_owner_derives_from(cs, cs->currentTypeName, memberInfo->ownerTypeName, 0)) {
        accessible = memberInfo->isStatic || isSuperReceiver ||
                     member_owner_derives_from(cs, receiverTypeName, cs->currentTypeName, 0);
    }
    if (!accessible) {
        ZrParser_Compiler_Error(cs, "Field is not accessible in the current scope", memberNode->location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* Member references are keyed by the already-bound AST node and operation.
 * Repeated inference must not add an unresolved duplicate of an exact fact. */
static void publish_member_reference(SZrSemanticContext *context,
                                     const SZrSemanticReferenceFact *fact) {
    SZrSemanticReferenceFact *existing =
            (SZrSemanticReferenceFact *)ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                    context, fact->node, fact->kind);
    if (existing != ZR_NULL) {
        if (!existing->isResolved && fact->isResolved) {
            existing->declarationRange = fact->declarationRange;
            existing->definitionRange = fact->definitionRange;
            existing->hasDefinitionRange = fact->hasDefinitionRange;
            existing->symbolId = fact->symbolId;
            existing->typeId = fact->typeId;
            existing->ownershipQualifier = fact->ownershipQualifier;
            existing->isResolved = ZR_TRUE;
        }
        return;
    }
    ZrParser_SemanticFacts_AppendReference(context, fact);
}

static void record_member_reference(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *member = ZR_NULL;
    SZrAstNode *property;
    TZrBool computedAccess;
    const SZrSemanticReferenceFact *read;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.members == ZR_NULL) {
        return;
    }
    for (TZrSize index = node->data.primaryExpression.members->count; index > 0; index--) {
        SZrAstNode *candidate = node->data.primaryExpression.members->nodes[index - 1];
        if (candidate != ZR_NULL && candidate->type == ZR_AST_MEMBER_EXPRESSION) {
            member = candidate;
            break;
        }
    }
    if (member == ZR_NULL) {
        return;
    }
    property = member->data.memberExpression.property;
    computedAccess = kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
                     member->data.memberExpression.computed;
    if (computedAccess && property != ZR_NULL) {
        SZrInferredType propertyType;
        ZrParser_InferredType_Init(cs->state, &propertyType, ZR_VALUE_TYPE_OBJECT);
        (void)ZrParser_ExpressionType_Infer(cs, property, &propertyType);
        ZrParser_InferredType_Free(cs->state, &propertyType);
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = computedAccess || property == ZR_NULL ? member : property;
    fact.range = fact.node->location;
    fact.declarationRange = fact.range;
    fact.kind = kind;
    fact.name = property != ZR_NULL && property->type == ZR_AST_IDENTIFIER_LITERAL
                        ? property->data.identifier.name
                        : ZR_NULL;
    read = kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE &&
                   !member->data.memberExpression.computed
                   ? ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                             cs->semanticContext, fact.node, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS)
                   : ZR_NULL;
    if (read != ZR_NULL && read->isResolved) {
        fact = *read;
        fact.kind = kind;
    }
    publish_member_reference(cs->semanticContext, &fact);
}

void type_inference_record_member_access_reference_fact(SZrCompilerState *cs, SZrAstNode *node) {
    record_member_reference(cs, node, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
}

void type_inference_record_member_write_reference_fact(SZrCompilerState *cs, SZrAstNode *node) {
    record_member_reference(cs, node, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
}

void type_inference_record_resolved_member_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        SZrTypeMemberInfo *memberInfo,
        const SZrInferredType *valueType,
        EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *declaration;
    SZrSemanticSymbolRecord *symbol = ZR_NULL;
    TZrTypeId typeId;
    TZrSymbolId symbolId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION || memberInfo == ZR_NULL ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        (kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
         kind != ZR_SEMANTIC_REFERENCE_MEMBER_WRITE)) {
        return;
    }
    declaration = memberInfo->declarationNode;
    memset(&fact, 0, sizeof(fact));
    if (memberInfo->memberType == ZR_AST_PROPERTY_DECLARATION) {
        symbolId = memberInfo->propertySymbolId;
        typeId = memberInfo->propertyValueTypeId;
        if (declaration != ZR_NULL && declaration->type == ZR_AST_PROPERTY_DECLARATION) {
            fact.definitionRange = declaration->data.propertyDeclaration.nameLocation;
            fact.hasDefinitionRange = ZR_TRUE;
        }
    } else if ((memberInfo->memberType == ZR_AST_CLASS_FIELD ||
                memberInfo->memberType == ZR_AST_STRUCT_FIELD) &&
               declaration != ZR_NULL && valueType != ZR_NULL &&
               (declaration->type == ZR_AST_CLASS_FIELD ||
                declaration->type == ZR_AST_STRUCT_FIELD)) {
        symbol = (SZrSemanticSymbolRecord *)ZrParser_Semantic_FindSymbolById(
                cs->semanticContext, memberInfo->symbolId);
        if (symbol != ZR_NULL &&
            (symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FIELD || symbol->astNode != declaration)) {
            return;
        }
        if (symbol == ZR_NULL) {
            for (TZrSize index = 0; index < cs->semanticContext->symbols.length; index++) {
                SZrSemanticSymbolRecord *candidate = (SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        &cs->semanticContext->symbols, index);
                if (candidate != ZR_NULL && candidate->kind == ZR_SEMANTIC_SYMBOL_KIND_FIELD &&
                    candidate->astNode == declaration) {
                    symbol = candidate;
                    break;
                }
            }
        }
        typeId = symbol != ZR_NULL ? symbol->typeId : ZR_SEMANTIC_ID_INVALID;
        if (symbol == ZR_NULL &&
            (valueType->baseType != ZR_VALUE_TYPE_OBJECT || valueType->typeName != ZR_NULL ||
             memberInfo->fieldTypeName != ZR_NULL)) {
            typeId = ZrParser_Semantic_RegisterInferredType(
                    cs->semanticContext, valueType,
                    valueType->baseType == ZR_VALUE_TYPE_OBJECT ||
                            valueType->baseType == ZR_VALUE_TYPE_ARRAY ||
                            valueType->baseType == ZR_VALUE_TYPE_STRING
                            ? ZR_SEMANTIC_TYPE_KIND_REFERENCE : ZR_SEMANTIC_TYPE_KIND_VALUE,
                    valueType->typeName, declaration);
        }
        fact.definitionRange = declaration->type == ZR_AST_CLASS_FIELD
                                       ? declaration->data.classField.nameLocation
                                       : declaration->location;
        fact.hasDefinitionRange = ZR_TRUE;
        symbolId = symbol != ZR_NULL ? symbol->id : ZrParser_Semantic_RegisterSymbol(
                cs->semanticContext, memberInfo->name, ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                typeId, ZR_SEMANTIC_ID_INVALID, declaration, fact.definitionRange);
        memberInfo->symbolId = symbolId;
    } else {
        return;
    }
    if (symbolId == ZR_SEMANTIC_ID_INVALID ||
        (memberInfo->memberType == ZR_AST_PROPERTY_DECLARATION &&
         typeId == ZR_SEMANTIC_ID_INVALID)) {
        return;
    }
    fact.node = memberNode->data.memberExpression.property;
    fact.range = fact.node->location;
    if (declaration != ZR_NULL) {
        fact.declarationRange = declaration->location;
    }
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.typeId = typeId;
    fact.name = memberInfo->name;
    fact.ownershipQualifier = valueType != ZR_NULL ? valueType->ownershipQualifier
                                                  : memberInfo->ownershipQualifier;
    fact.isResolved = ZR_TRUE;
    publish_member_reference(cs->semanticContext, &fact);
    if (symbol != ZR_NULL || declaration == ZR_NULL ||
        memberInfo->memberType == ZR_AST_PROPERTY_DECLARATION) {
        return;
    }
    fact.node = declaration;
    fact.range = fact.definitionRange;
    fact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    publish_member_reference(cs->semanticContext, &fact);
}
