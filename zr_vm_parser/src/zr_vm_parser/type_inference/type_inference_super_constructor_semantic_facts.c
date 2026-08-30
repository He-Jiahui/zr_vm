#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_contract.h"
#include "zr_vm_parser/type_inference.h"

#include "type_inference_internal.h"

static SZrTypeMemberInfo *type_inference_super_constructor_member(
        SZrCompilerState *cs,
        SZrString *superTypeName) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || superTypeName == ZR_NULL) {
        return ZR_NULL;
    }
    prototype = find_compiler_type_prototype_inference(cs, superTypeName);
    if (prototype == ZR_NULL || !prototype->members.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *member = (SZrTypeMemberInfo *)ZrCore_Array_Get(
                &prototype->members, index);
        if (member != ZR_NULL && member->isMetaMethod &&
            member->metaType == ZR_META_CONSTRUCTOR) {
            return member;
        }
    }
    return ZR_NULL;
}

static TZrTypeId type_inference_super_constructor_call_type_id(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *constructor,
        const SZrResolvedCallSignature *resolvedSignature) {
    TZrTypeId callTypeId;
    TZrTypeId refinedTypeId;
    const SZrCanonicalTypeNode *refinedType;
    const SZrCanonicalParameterContract *contracts = ZR_NULL;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        constructor == ZR_NULL || resolvedSignature == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    callTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &resolvedSignature->parameterTypes,
            &resolvedSignature->parameterPassingModes,
            &resolvedSignature->returnType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (callTypeId == ZR_SEMANTIC_ID_INVALID ||
        constructor->declarationNode == ZR_NULL) {
        return callTypeId;
    }
    refinedTypeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
            cs->semanticContext, constructor->declarationNode, callTypeId);
    refinedType = ZrParser_CanonicalType_Find(
            cs->semanticContext, refinedTypeId);
    if (refinedType == ZR_NULL ||
        refinedType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (refinedType->data.function.parameterContracts.length > 0u) {
        contracts = (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                (SZrArray *)&refinedType->data.function.parameterContracts, 0u);
    }
    return ZrParser_CanonicalType_InternFunction(
            cs->semanticContext,
            contracts,
            refinedType->data.function.parameterContracts.length,
            refinedType->data.function.returnTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            refinedType->data.function.effectFlags);
}

void type_inference_record_super_constructor_call_facts(
        SZrCompilerState *cs,
        SZrAstNode *metaFunctionNode,
        SZrString *superTypeName,
        SZrAstNodeArray *superArgs) {
    SZrClassMetaFunction *metaFunction;
    SZrTypeMemberInfo *constructor;
    SZrFunctionCall call;
    SZrResolvedCallSignature resolvedSignature;
    SZrSemanticExpressionFact expressionFact;
    SZrSemanticReferenceFact referenceFact;
    SZrFileRange callRange;
    SZrFileRange targetRange;
    TZrTypeId callTypeId;
    TZrSymbolId symbolId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        metaFunctionNode == ZR_NULL ||
        metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION ||
        superTypeName == ZR_NULL) {
        return;
    }
    metaFunction = &metaFunctionNode->data.classMetaFunction;
    if (!metaFunction->hasSuperCall) {
        return;
    }
    callRange = metaFunction->superCallRange;
    if (callRange.end.offset < callRange.start.offset ||
        (callRange.end.offset == callRange.start.offset &&
         callRange.end.column <= callRange.start.column)) {
        return;
    }
    constructor = type_inference_super_constructor_member(cs, superTypeName);
    if (constructor == ZR_NULL || constructor->name == ZR_NULL) {
        return;
    }
    memset(&call, 0, sizeof(call));
    call.args = superArgs;
    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    if (resolve_generic_member_call_signature_detailed(
                cs,
                constructor,
                &call,
                &resolvedSignature,
                ZR_NULL,
                0u) != ZR_GENERIC_CALL_RESOLVE_OK) {
        return;
    }
    callTypeId = type_inference_super_constructor_call_type_id(
            cs, constructor, &resolvedSignature);
    if (callTypeId == ZR_SEMANTIC_ID_INVALID) {
        free_resolved_call_signature(cs->state, &resolvedSignature);
        return;
    }
    symbolId = type_inference_member_symbol_id(cs, constructor, callTypeId);
    type_inference_publish_member_declaration_fact(
            cs, constructor, symbolId, callTypeId);

    targetRange = callRange;
    targetRange.end = targetRange.start;
    targetRange.end.offset += sizeof("super") - 1u;
    targetRange.end.column += sizeof("super") - 1u;

    memset(&expressionFact, 0, sizeof(expressionFact));
    expressionFact.node = metaFunctionNode;
    expressionFact.range = callRange;
    expressionFact.callTargetRange = targetRange;
    expressionFact.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expressionFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    expressionFact.hasCallInfo = ZR_TRUE;
    expressionFact.callTargetName = constructor->name;
    expressionFact.argumentCount = superArgs != ZR_NULL ? superArgs->count : 0u;
    expressionFact.isMemberCall = ZR_FALSE;
    ZrParser_InferredType_Init(
            cs->state, &expressionFact.inferredType, ZR_VALUE_TYPE_NULL);
    ZrParser_SemanticFacts_AppendExpression(
            cs->semanticContext, &expressionFact);
    ZrParser_InferredType_Free(cs->state, &expressionFact.inferredType);

    memset(&referenceFact, 0, sizeof(referenceFact));
    referenceFact.node = metaFunctionNode;
    referenceFact.range = targetRange;
    referenceFact.declarationRange = constructor->declarationNode != ZR_NULL
                                             ? constructor->declarationNode->location
                                             : (SZrFileRange){0};
    referenceFact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    referenceFact.symbolId = symbolId;
    referenceFact.typeId = callTypeId;
    referenceFact.name = constructor->name;
    referenceFact.signatureDisplay = type_inference_callable_signature_display(
            cs,
            "@",
            constructor->name,
            type_inference_call_parameters(constructor->declarationNode),
            &constructor->parameterNames,
            &constructor->genericParameters,
            callTypeId);
    referenceFact.isResolved = symbolId != ZR_SEMANTIC_ID_INVALID;
    (void)type_inference_call_argument_facts_build(
            cs,
            &call,
            type_inference_call_parameters(constructor->declarationNode),
            &constructor->parameterNames,
            &resolvedSignature,
            ZR_TRUE,
            &referenceFact.argumentMappings);
    if (referenceFact.signatureDisplay != ZR_NULL && referenceFact.isResolved) {
        ZrParser_SemanticFacts_AppendReference(
                cs->semanticContext, &referenceFact);
    }
    if (referenceFact.argumentMappings.isValid) {
        ZrCore_Array_Free(cs->state, &referenceFact.argumentMappings);
    }
    free_resolved_call_signature(cs->state, &resolvedSignature);
}
