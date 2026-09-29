#include "compiler_internal.h"

#include <string.h>

/*
 * 在 callable value 注册成功后，以精确的 lambda AST 补齐声明事实，使语义查询能回到
 * lambda 声明及其完整范围，而不是仅指向绑定变量。
 * @pre bindingIndex 是注册前保存的数组长度，且该位置已登记同一 lambda 与有效 canonical ID。
 * @return 仅在条目校验通过且声明事实成功追加后返回 true。
 */
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

    /* 声明、定义与引用范围都以同一 lambda 节点为准，保持 definition 查询的身份和范围一致。 */
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

/* canonical symbol 类型更新成功后，同步该 symbol 的所有引用事实；位置和引用类别保持原样。 */
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

/*
 * 将省略返回类型的函数体推断结果写回声明对应的 callable binding；按 declaration AST
 * 在当前及祖先 type environment 中查找，以更新原有 canonical contract 和其语义事实。
 * @pre declarationNode 对应已登记函数，returnType 是该函数体的推断返回类型。
 * @return 找到并更新 binding 时为 true；输入无效、未找到或 canonical 重建/反绑失败时为 false。
 * @note 缺少 semantic context 或 canonical type identity 时仅更新本地返回类型缓存，不同步 symbol 和引用事实。
 */
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
                /* 没有可用于 canonical 重建的上下文或类型身份，只保留本地推断返回类型。 */
                ZrParser_InferredType_Free(cs->state, &functionInfo->returnType);
                ZrParser_InferredType_Copy(cs->state, &functionInfo->returnType, returnType);
                return ZR_TRUE;
            }

            /* 把函数泛型形参映射到其 owner symbol 与序号，供推断结果复用开放泛型身份。 */
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
            /* 以原函数契约为模板，只替换返回 TypeId，保留参数、receiver effect 与 effect flags。 */
            refinedTypeId = ZrParser_CanonicalType_InternFunction(
                    semanticContext,
                    (const SZrCanonicalParameterContract *)
                            templateType->data.function.parameterContracts.head,
                    templateType->data.function.parameterContracts.length,
                    returnTypeId,
                    templateType->data.function.receiverEffect,
                    templateType->data.function.effectFlags);
            /* 先让 symbol 接受新 canonical 类型，再改本地缓存并同步同 symbol 的引用事实。 */
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
