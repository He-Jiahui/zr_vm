#include "compiler_internal.h"

#include <string.h>

TZrBool compiler_publish_lambda_callable_binding_identity(
        SZrCompilerState *cs,
        SZrTypeEnvironment *env,
        TZrSize bindingIndex,
        SZrAstNode *lambdaNode) {
    SZrFunctionTypeInfo **entry;
    SZrFunctionTypeInfo *functionInfo;
    SZrSemanticReferenceFact declarationFact;

    if (cs == ZR_NULL || env == ZR_NULL || env->semanticContext == ZR_NULL ||
        lambdaNode == ZR_NULL || lambdaNode->type != ZR_AST_LAMBDA_EXPRESSION ||
        bindingIndex >= env->functionReturnTypes.length) {
        return ZR_FALSE;
    }
    entry = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
            &env->functionReturnTypes, bindingIndex);
    if (entry == ZR_NULL || *entry == ZR_NULL ||
        (*entry)->declarationNode != lambdaNode ||
        (*entry)->symbolId == ZR_SEMANTIC_ID_INVALID ||
        (*entry)->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    functionInfo = *entry;
    functionInfo->declarationRange = lambdaNode->location;
    functionInfo->hasDeclarationRange = ZR_TRUE;

    memset(&declarationFact, 0, sizeof(declarationFact));
    declarationFact.node = lambdaNode;
    declarationFact.range = lambdaNode->location;
    declarationFact.declarationRange = lambdaNode->location;
    declarationFact.definitionRange = lambdaNode->location;
    declarationFact.hasDefinitionRange = ZR_TRUE;
    declarationFact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    declarationFact.symbolId = functionInfo->symbolId;
    declarationFact.typeId = functionInfo->typeId;
    declarationFact.name = functionInfo->name;
    declarationFact.isResolved = ZR_TRUE;
    return ZrParser_SemanticFacts_AppendReference(
            env->semanticContext, &declarationFact);
}

static void compiler_rebind_reference_fact_types(SZrSemanticContext *semanticContext,
                                                  TZrSymbolId symbolId,
                                                  TZrTypeId typeId) {
    TZrSize index;

    if (semanticContext == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    for (index = 0; index < semanticContext->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &semanticContext->referenceFacts,
                        index);
        if (fact != ZR_NULL && fact->symbolId == symbolId) {
            fact->typeId = typeId;
        }
    }
}

TZrBool compiler_refine_function_type_binding_return(
        SZrCompilerState *cs,
        SZrAstNode *declarationNode,
        const SZrInferredType *returnType) {
    SZrTypeEnvironment *env;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL ||
        declarationNode == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }

    for (env = cs->typeEnv; env != ZR_NULL; env = env->parent) {
        TZrSize index;

        for (index = 0; index < env->functionReturnTypes.length; index++) {
            SZrFunctionTypeInfo **entry = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
                    &env->functionReturnTypes,
                    index);
            SZrFunctionTypeInfo *functionInfo;
            SZrSemanticContext *semanticContext;
            TZrTypeId returnTypeId;
            TZrTypeId refinedTypeId;
            const SZrCanonicalTypeNode *templateType;
            SZrArray genericBindings;
            TZrSize genericIndex;

            if (entry == ZR_NULL || *entry == ZR_NULL ||
                (*entry)->declarationNode != declarationNode) {
                continue;
            }
            functionInfo = *entry;
            semanticContext = env->semanticContext;
            if (semanticContext == ZR_NULL ||
                functionInfo->typeId == ZR_SEMANTIC_ID_INVALID) {
                ZrParser_InferredType_Free(cs->state, &functionInfo->returnType);
                ZrParser_InferredType_Copy(cs->state, &functionInfo->returnType, returnType);
                return ZR_TRUE;
            }

            ZrCore_Array_Construct(&genericBindings);
            if (functionInfo->genericParameters.length > 0U) {
                ZrCore_Array_Init(cs->state,
                                  &genericBindings,
                                  sizeof(SZrCanonicalGenericBinding),
                                  functionInfo->genericParameters.length);
                for (genericIndex = 0;
                     genericIndex < functionInfo->genericParameters.length;
                     genericIndex++) {
                    const SZrTypeGenericParameterInfo *parameter =
                            (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                                    &functionInfo->genericParameters,
                                    genericIndex);
                    SZrCanonicalGenericBinding binding;

                    if (parameter == ZR_NULL || parameter->name == ZR_NULL) {
                        continue;
                    }
                    binding.name = parameter->name;
                    binding.kind = parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE
                                           ? ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                                           : ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
                    binding.ownerSymbolId = functionInfo->symbolId;
                    binding.ordinal = (TZrUInt32)genericIndex;
                    binding.typeId = parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE
                                             ? ZrParser_CanonicalType_InternGenericParameter(
                                                       semanticContext,
                                                       functionInfo->symbolId,
                                                       (TZrUInt32)genericIndex)
                                             : ZR_SEMANTIC_ID_INVALID;
                    if (parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE &&
                        binding.typeId == ZR_SEMANTIC_ID_INVALID) {
                        ZrCore_Array_Free(cs->state, &genericBindings);
                        return ZR_FALSE;
                    }
                    ZrCore_Array_Push(cs->state, &genericBindings, &binding);
                }
            }

            returnTypeId = ZrParser_CanonicalType_FromInferredWithGenericBindings(
                    semanticContext,
                    returnType,
                    (const SZrCanonicalGenericBinding *)genericBindings.head,
                    genericBindings.length);
            if (genericBindings.isValid) {
                ZrCore_Array_Free(cs->state, &genericBindings);
            }
            templateType = ZrParser_CanonicalType_Find(semanticContext,
                                                       functionInfo->typeId);
            if (returnTypeId == ZR_SEMANTIC_ID_INVALID || templateType == ZR_NULL ||
                templateType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
                return ZR_FALSE;
            }
            refinedTypeId = ZrParser_CanonicalType_InternFunction(
                    semanticContext,
                    (const SZrCanonicalParameterContract *)
                            templateType->data.function.parameterContracts.head,
                    templateType->data.function.parameterContracts.length,
                    returnTypeId,
                    templateType->data.function.receiverEffect,
                    templateType->data.function.effectFlags);
            if (refinedTypeId == ZR_SEMANTIC_ID_INVALID ||
                !ZrParser_Semantic_RebindSymbolType(semanticContext,
                                                    functionInfo->symbolId,
                                                    refinedTypeId)) {
                return ZR_FALSE;
            }

            ZrParser_InferredType_Free(cs->state, &functionInfo->returnType);
            ZrParser_InferredType_Copy(cs->state, &functionInfo->returnType, returnType);
            functionInfo->typeId = refinedTypeId;
            compiler_rebind_reference_fact_types(semanticContext,
                                                 functionInfo->symbolId,
                                                 refinedTypeId);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
