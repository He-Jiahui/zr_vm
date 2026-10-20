#include "debug_internal.h"

#include <stdint.h>

#include "zr_vm_core/debug.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

/* 只把可由暂停值精确表达的整数/布尔事实带入推断；超出 int64 的无符号值不伪造范围。 */
static void zr_debug_semantic_apply_exact_value_range(const SZrTypeValue *value,
                                                      SZrInferredType *inferredType) {
    TZrUInt64 unsignedValue;

    if (value == ZR_NULL || inferredType == ZR_NULL) {
        return;
    }

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        inferredType->knownBoolValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        inferredType->hasKnownBoolValue = ZR_TRUE;
        return;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        inferredType->minValue = value->value.nativeObject.nativeInt64;
        inferredType->maxValue = value->value.nativeObject.nativeInt64;
        inferredType->hasRangeConstraint = ZR_TRUE;
        return;
    }

    if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return;
    }

    unsignedValue = value->value.nativeObject.nativeUInt64;
    if (unsignedValue <= (TZrUInt64)INT64_MAX) {
        inferredType->minValue = (TZrInt64)unsignedValue;
        inferredType->maxValue = (TZrInt64)unsignedValue;
        inferredType->hasRangeConstraint = ZR_TRUE;
    }
}

/* 将 VM 导出的 zr 根绑定成带 token 的语义根，避免调试表达式把它当作普通局部变量。 */
static TZrBool zr_debug_semantic_register_runtime_root(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    SZrDebugRuntimeRootBinding runtimeRoot;
    SZrTypeValue value;
    SZrInferredType inferredType;
    SZrString *name;
    EZrDebugEvaluationContextStatus status;
    TZrBool registered;

    if (agent == ZR_NULL || agent->state == ZR_NULL || compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    name = ZrCore_String_CreateFromNative(compilerState->state, "zr");
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name) != ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(
            agent->state, frameId == 0u ? 0u : frameId - 1u, &context);
    /* 缺少暂停元数据时保留其它可推断绑定；真正的解析/读取失败才阻止规范绑定。 */
    if (status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT ||
        status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE) {
        return ZR_TRUE;
    }
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    memset(&runtimeRoot, 0, sizeof(runtimeRoot));
    status = ZrCore_Debug_EvaluationContext_GetRuntimeRoot(
            agent->state, &context, ZR_DEBUG_RUNTIME_ROOT_ZR, &runtimeRoot);
    if (status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE) {
        return ZR_TRUE;
    }
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                agent->state, &context, &runtimeRoot, &value) !=
        ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(compilerState->state, &inferredType, value.type);
    zr_debug_semantic_apply_exact_value_range(&value, &inferredType);
    registered = ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            compilerState->state,
            compilerState->typeEnv,
            name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_ZR,
            runtimeRoot.token);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return registered;
}

/* 诊断摘要的宽松回退：只为 AST 中实际出现且能在暂停帧解析的名字补值类型。 */
static void zr_debug_semantic_register_summary_value_binding(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState,
        const SZrAstNode *identifier) {
    SZrString *name;
    SZrTypeValue value;
    SZrInferredType inferredType;
    SZrFileRange emptyRange;

    if (agent == ZR_NULL || compilerState == ZR_NULL || compilerState->typeEnv == ZR_NULL ||
        identifier == ZR_NULL || identifier->type != ZR_AST_IDENTIFIER_LITERAL) {
        return;
    }
    name = identifier->data.identifier.name;
    if (name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name) != ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (!zr_debug_resolve_identifier_value(agent,
                                           frameId,
                                           zr_debug_string_native(name),
                                           &value,
                                           ZR_NULL,
                                           0u)) {
        return;
    }

    memset(&emptyRange, 0, sizeof(emptyRange));
    ZrParser_InferredType_Init(compilerState->state, &inferredType, value.type);
    zr_debug_semantic_apply_exact_value_range(&value, &inferredType);
    (void)ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                      compilerState->typeEnv,
                                                      name,
                                                      &inferredType,
                                                      ZR_NULL,
                                                      emptyRange);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
}

/* 只走会影响表达式推断的子节点，不把属性名误注册成独立变量。 */
static void zr_debug_semantic_register_summary_node(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *node);

static void zr_debug_semantic_register_summary_list(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return;
    }
    for (index = 0u; index < nodes->count; ++index) {
        zr_debug_semantic_register_summary_node(
                agent, frameId, compilerState, nodes->nodes[index]);
    }
}

static void zr_debug_semantic_register_summary_node(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            zr_debug_semantic_register_summary_value_binding(
                    agent, frameId, compilerState, node);
            break;
        case ZR_AST_BINARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.binaryExpression.left);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.binaryExpression.right);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.logicalExpression.left);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.logicalExpression.right);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.unaryExpression.argument);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.typeCastExpression.expression);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.typeQueryExpression.operand);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.test);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.consequent);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.alternate);
            break;
        case ZR_AST_IF_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.condition);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.thenExpr);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.elseExpr);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.primaryExpression.property);
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.primaryExpression.members);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.memberExpression.property);
            break;
        case ZR_AST_FUNCTION_CALL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.functionCall.args);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.constructExpression.target);
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.constructExpression.args);
            break;
        case ZR_AST_ARRAY_LITERAL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.arrayLiteral.elements);
            break;
        case ZR_AST_OBJECT_LITERAL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.objectLiteral.properties);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.keyValuePair.key);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.keyValuePair.value);
            break;
        default:
            break;
    }
}

/* 将编译函数留下的类型引用转换为本次推断环境拥有的类型对象。 */
static void zr_debug_semantic_type_ref_to_inferred(SZrCompilerState *compilerState,
                                                   const SZrFunctionTypedTypeRef *typeRef,
                                                   SZrInferredType *result) {
    if (compilerState == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (typeRef == ZR_NULL) {
        ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isArray) {
        SZrInferredType elementType;

        ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = typeRef->ownershipQualifier;
        result->isNullable = typeRef->isNullable;
        ZrCore_Array_Init(compilerState->state, &result->elementTypes, sizeof(SZrInferredType), 1);
        ZrParser_InferredType_InitFull(compilerState->state,
                                       &elementType,
                                       typeRef->elementBaseType,
                                       ZR_FALSE,
                                       typeRef->elementTypeName);
        ZrCore_Array_Push(compilerState->state, &result->elementTypes, &elementType);
        return;
    }

    if (typeRef->typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(compilerState->state,
                                       result,
                                       typeRef->baseType,
                                       typeRef->isNullable,
                                       typeRef->typeName);
    } else {
        ZrParser_InferredType_Init(compilerState->state, result, typeRef->baseType);
        result->isNullable = typeRef->isNullable;
    }
    result->ownershipQualifier = typeRef->ownershipQualifier;
}

static const SZrFunctionTypedLocalBinding *zr_debug_semantic_find_typed_local_binding(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; ++index) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];

        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

/* 以活动槽名核对编译期绑定，再把 symbol/type/place ID 注入暂停求值。 */
static TZrBool zr_debug_semantic_register_canonical_frame_binding(
        SZrCompilerState *compilerState,
        const SZrFunction *function,
        const SZrDebugFrameBinding *frameBinding,
        SZrString *activeName,
        const SZrTypeValue *activeValue) {
    const SZrFunctionTypedLocalBinding *typedBinding;
    SZrInferredType inferredType;
    SZrFileRange declarationRange;
    TZrBool registered;

    if (compilerState == ZR_NULL ||
        compilerState->state == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        function == ZR_NULL ||
        frameBinding == ZR_NULL ||
        activeName == ZR_NULL) {
        return ZR_FALSE;
    }

    typedBinding = zr_debug_semantic_find_typed_local_binding(function, frameBinding->stackSlot);
    if (typedBinding == ZR_NULL ||
        typedBinding->name == ZR_NULL ||
        !ZrCore_String_Equal(typedBinding->name, activeName) ||
        frameBinding->symbolId == ZR_SEMANTIC_ID_INVALID ||
        frameBinding->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&declarationRange, 0, sizeof(declarationRange));
    declarationRange.source = function->sourceCodeList;
    declarationRange.start.line = (TZrInt32)frameBinding->declarationStartLine;
    declarationRange.start.column = (TZrInt32)frameBinding->declarationStartColumn;
    declarationRange.end.line = (TZrInt32)frameBinding->declarationEndLine;
    declarationRange.end.column = (TZrInt32)frameBinding->declarationEndColumn;
    zr_debug_semantic_type_ref_to_inferred(compilerState, &typedBinding->type, &inferredType);
    zr_debug_semantic_apply_exact_value_range(activeValue, &inferredType);
    registered = ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(compilerState->state,
                                                                     compilerState->typeEnv,
                                                                     typedBinding->name,
                                                                     &inferredType,
                                                                     frameBinding->symbolId,
                                                                     frameBinding->typeId,
                                                                     frameBinding->placeId,
                                                                     declarationRange);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return registered;
}

/* 没有可用暂停帧时，仅注册入口函数中仍带规范身份的顶层局部绑定。 */
static void zr_debug_semantic_register_entry_typed_locals(ZrDebugAgent *agent,
                                                          SZrCompilerState *compilerState) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (agent == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        agent->entryFunction == ZR_NULL) {
        return;
    }

    entryFunction = agent->entryFunction;
    for (index = 0; index < entryFunction->typedLocalBindingLength; ++index) {
        const SZrFunctionTypedLocalBinding *binding = &entryFunction->typedLocalBindings[index];
        SZrInferredType inferredType;
        SZrFileRange declarationRange;
        TZrBool hasCanonicalIdentity;

        if (binding->name == ZR_NULL ||
            ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, binding->name) != ZR_NULL) {
            continue;
        }

        memset(&declarationRange, 0, sizeof(declarationRange));
        declarationRange.source = entryFunction->sourceCodeList;
        declarationRange.start.line = (TZrInt32)binding->declarationStartLine;
        declarationRange.start.column = (TZrInt32)binding->declarationStartColumn;
        declarationRange.end.line = (TZrInt32)binding->declarationEndLine;
        declarationRange.end.column = (TZrInt32)binding->declarationEndColumn;
        hasCanonicalIdentity = binding->symbolId != ZR_SEMANTIC_ID_INVALID &&
                               binding->typeId != ZR_SEMANTIC_ID_INVALID &&
                               declarationRange.source != ZR_NULL;
        zr_debug_semantic_type_ref_to_inferred(compilerState, &binding->type, &inferredType);
        if (hasCanonicalIdentity) {
            (void)ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
                    compilerState->state,
                    compilerState->typeEnv,
                    binding->name,
                    &inferredType,
                    binding->symbolId,
                    binding->typeId,
                    binding->placeId,
                    declarationRange);
        }
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }
}

static void zr_debug_semantic_free_inferred_type_array(SZrCompilerState *compilerState, SZrArray *types) {
    TZrSize index;

    if (compilerState == ZR_NULL || types == ZR_NULL || !types->isValid) {
        return;
    }

    for (index = 0; index < types->length; ++index) {
        SZrInferredType *type = (SZrInferredType *)ZrCore_Array_Get(types, index);
        if (type != ZR_NULL) {
            ZrParser_InferredType_Free(compilerState->state, type);
        }
    }
    if (types->head != ZR_NULL) {
        ZrCore_Array_Free(compilerState->state, types);
    }
    ZrCore_Array_Construct(types);
}

static TZrBool zr_debug_semantic_collect_callable_parameter_types(SZrCompilerState *compilerState,
                                                                  const SZrFunction *callable,
                                                                  SZrArray *paramTypes,
                                                                  SZrArray *parameterPassingModes) {
    TZrUInt32 index;

    if (compilerState == ZR_NULL || callable == ZR_NULL || paramTypes == ZR_NULL ||
        parameterPassingModes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(compilerState->state,
                      paramTypes,
                      sizeof(SZrInferredType),
                      callable->parameterMetadataCount > 0 ? callable->parameterMetadataCount : 1u);
    ZrCore_Array_Init(compilerState->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      callable->parameterMetadataCount > 0 ? callable->parameterMetadataCount : 1u);

    for (index = 0; index < callable->parameterMetadataCount; ++index) {
        SZrInferredType paramType;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

        zr_debug_semantic_type_ref_to_inferred(compilerState, &callable->parameterMetadata[index].type, &paramType);
        ZrCore_Array_Push(compilerState->state, paramTypes, &paramType);
        ZrCore_Array_Push(compilerState->state, parameterPassingModes, &passingMode);
    }

    return ZR_TRUE;
}

/* 以编译产物中的签名提供顶层函数推断，数组和临时类型在每轮注册后释放。 */
static void zr_debug_semantic_register_entry_callables(ZrDebugAgent *agent, SZrCompilerState *compilerState) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (agent == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        agent->entryFunction == ZR_NULL) {
        return;
    }

    entryFunction = agent->entryFunction;
    for (index = 0; index < entryFunction->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &entryFunction->topLevelCallableBindings[index];
        const SZrFunction *callable;
        SZrInferredType returnType;
        SZrArray paramTypes;
        SZrArray parameterPassingModes;
        TZrBool returnTypeInitialized = ZR_FALSE;
        TZrBool paramTypesInitialized = ZR_FALSE;
        TZrBool parameterPassingModesInitialized = ZR_FALSE;

        if (binding->name == ZR_NULL ||
            binding->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            binding->callableChildIndex >= entryFunction->childFunctionLength) {
            continue;
        }

        callable = &entryFunction->childFunctionList[binding->callableChildIndex];
        if (!callable->hasCallableReturnType) {
            continue;
        }

        ZrCore_Array_Construct(&paramTypes);
        ZrCore_Array_Construct(&parameterPassingModes);
        zr_debug_semantic_type_ref_to_inferred(compilerState, &callable->callableReturnType, &returnType);
        returnTypeInitialized = ZR_TRUE;

        if (!zr_debug_semantic_collect_callable_parameter_types(compilerState,
                                                               callable,
                                                               &paramTypes,
                                                               &parameterPassingModes)) {
            goto cleanup;
        }
        paramTypesInitialized = ZR_TRUE;
        parameterPassingModesInitialized = ZR_TRUE;

        (void)ZrParser_TypeEnvironment_RegisterFunctionEx(compilerState->state,
                                                          compilerState->typeEnv,
                                                          binding->name,
                                                          &returnType,
                                                          &paramTypes,
                                                          ZR_NULL,
                                                          &parameterPassingModes,
                                                          ZR_NULL);

cleanup:
        if (returnTypeInitialized) {
            ZrParser_InferredType_Free(compilerState->state, &returnType);
        }
        if (paramTypesInitialized) {
            zr_debug_semantic_free_inferred_type_array(compilerState, &paramTypes);
        }
        if (parameterPassingModesInitialized && parameterPassingModes.head != ZR_NULL) {
            ZrCore_Array_Free(compilerState->state, &parameterPassingModes);
        }
    }
}

/* 当前 PC 的活动绑定顺序由 Core Debug 提供，失配时整帧视为元数据不可用。 */
static EZrDebugEvaluationContextStatus zr_debug_semantic_register_frame_variables(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    SZrDebugFrameBinding frameBinding;
    EZrDebugEvaluationContextStatus status;
    TZrUInt32 bindingIndex;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(agent->state,
                                               frameId == 0 ? 0u : frameId - 1u,
                                               &context);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }

    if (context.activation.function == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    for (bindingIndex = 0; bindingIndex < context.activeBindingCount; ++bindingIndex) {
        SZrString *activeName;
        const SZrTypeValue *activeValue;

        memset(&frameBinding, 0, sizeof(frameBinding));
        status = ZrCore_Debug_EvaluationContext_GetBinding(agent->state,
                                                           &context,
                                                           bindingIndex,
                                                           &frameBinding);
        if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return status;
        }

        activeName = ZrCore_Function_GetLocalVariableName(context.activation.function,
                                                           frameBinding.stackSlot,
                                                           context.instructionOffset);
        activeValue = zr_debug_frame_value_slot(agent->state,
                                                context.activation.function,
                                                context.activation.callInfo,
                                                frameBinding.stackSlot);
        if (!zr_debug_semantic_register_canonical_frame_binding(compilerState,
                                                                context.activation.function,
                                                                &frameBinding,
                                                                activeName,
                                                                activeValue)) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
    }

    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

/* 捕获变量沿用 Core Debug 的 token 与原声明位置，避免推断生成第二套符号身份。 */
static EZrDebugEvaluationContextStatus zr_debug_semantic_register_closure_captures(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    EZrDebugEvaluationContextStatus status;
    TZrUInt32 captureIndex;

    if (agent == ZR_NULL || agent->state == ZR_NULL || compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(
            agent->state, frameId == 0u ? 0u : frameId - 1u, &context);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }
    if (context.activation.function == ZR_NULL || context.activation.function->closureValueLength == 0u) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
    }
    if (context.activation.function->closureValueList == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    for (captureIndex = 0u;
         captureIndex < context.activation.function->closureValueLength;
         ++captureIndex) {
        SZrDebugClosureCaptureBinding capture;
        SZrString *name;
        const SZrTypeBinding *existing;
        SZrInferredType inferredType;
        SZrFileRange declarationRange;

        memset(&capture, 0, sizeof(capture));
        status = ZrCore_Debug_EvaluationContext_GetClosureCapture(
                agent->state, &context, captureIndex, &capture);
        if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return status;
        }
        name = context.activation.function->closureValueList[captureIndex].name;
        if (name == ZR_NULL) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }

        existing = ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name);
        if (existing != ZR_NULL) {
            if (existing->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION) {
                continue;
            }
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }

        memset(&declarationRange, 0, sizeof(declarationRange));
        declarationRange.source = context.activation.function->sourceCodeList;
        declarationRange.start.line = (TZrInt32)capture.declarationStartLine;
        declarationRange.start.column = (TZrInt32)capture.declarationStartColumn;
        declarationRange.end.line = (TZrInt32)capture.declarationEndLine;
        declarationRange.end.column = (TZrInt32)capture.declarationEndColumn;
        zr_debug_semantic_type_ref_to_inferred(compilerState, capture.type, &inferredType);
        if (!ZrParser_TypeEnvironment_RegisterClosureCapture(
                    compilerState->state,
                    compilerState->typeEnv,
                    name,
                    &inferredType,
                    capture.symbolId,
                    capture.typeId,
                    declarationRange,
                    capture.captureIndex,
                    capture.token)) {
            ZrParser_InferredType_Free(compilerState->state, &inferredType);
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }

    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

/** @brief 为正式暂停求值建立规范帧绑定、捕获、运行时根与入口可调用项。 */
TZrBool zr_debug_semantic_register_bindings(ZrDebugAgent *agent,
                                            TZrUInt32 frameId,
                                            SZrCompilerState *compilerState) {
    EZrDebugEvaluationContextStatus frameStatus;

    frameStatus = zr_debug_semantic_register_frame_variables(agent, frameId, compilerState);
    if (frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
        frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT) {
        return ZR_FALSE;
    }

    if (frameStatus == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT) {
        zr_debug_semantic_register_entry_typed_locals(agent, compilerState);
    }
    if (frameStatus == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        frameStatus = zr_debug_semantic_register_closure_captures(agent, frameId, compilerState);
        if (frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return ZR_FALSE;
        }
    }
    if (!zr_debug_semantic_register_runtime_root(agent, frameId, compilerState)) {
        return ZR_FALSE;
    }
    if (agent->entryFunction != ZR_NULL &&
        !ZrParser_TypeInference_RegisterRuntimePrototypes(
                compilerState,
                agent->entryFunction)) {
        return ZR_FALSE;
    }
    zr_debug_semantic_register_entry_callables(agent, compilerState);
    return ZR_TRUE;
}

/** @brief 为只读语义摘要补充可解析的标识符；规范帧绑定缺席时仍允许部分摘要。 */
TZrBool zr_debug_semantic_register_summary_bindings(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *expression) {
    TZrBool canonicalBindingsAvailable =
            zr_debug_semantic_register_bindings(agent, frameId, compilerState);

    zr_debug_semantic_register_summary_node(agent, frameId, compilerState, expression);
    return (TZrBool)(canonicalBindingsAvailable || expression != ZR_NULL);
}

/**
 * @fn static void zr_debug_semantic_apply_exact_value_range(const SZrTypeValue *value, SZrInferredType *inferredType)
 *
 * ## Summary
 * 从借用的暂停值补充推断类型中的精确布尔值或 int64 范围。
 *
 * ## Parameters
 * @param[in] value 借用的暂停值；NULL 不补充精确范围，不取得值资源。
 * @param[in,out] inferredType 已初始化的推断类型，只补充当前支持的精确布尔或整数范围。
 *
 * ## Returns
 * 无返回值；仅在 inferredType 中补充支持的精确布尔或整数事实。
 *
 * ## Constraints and lifecycle
 * 只处理当前支持的 bool、signed int 和不大于 INT64_MAX 的 unsigned；大 unsigned 不窄化。NULL 值或不支持的种类不新增范围，不拥有或修改源值。
 *
 * ## References
 * - zr_debug_semantic_register_runtime_root（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:45）
 * - zr_debug_semantic_register_summary_value_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:113）
 * - zr_debug_semantic_register_canonical_frame_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:331）
 */

/**
 * @fn static TZrBool zr_debug_semantic_register_runtime_root( ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 在没有同名源码绑定时，把 Core Debug 提供的 zr 运行时根及令牌注册为推断来源。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * @return 真表示已注册、已有同名绑定或允许缺席的元数据被跳过；假表示参数、上下文、根解析或注册不满足，不能把真等同根一定存在。
 *
 * ## Constraints and lifecycle
 * 已有 zr 绑定优先；获取上下文遇 INVALID_ARGUMENT 或 METADATA_UNAVAILABLE、获取根遇 METADATA_UNAVAILABLE 时允许跳过，其他非 OK 状态及解析或注册失败返回假。Core 令牌及代际须在使用期间有效；临时类型普通释放，名称和运行时值只借用。
 *
 * ## References
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - ZrCore_Debug_EvaluationContext_GetRuntimeRoot（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:83）
 * - ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:93）
 */

/**
 * @fn static void zr_debug_semantic_register_summary_value_binding( ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState, const SZrAstNode *identifier)
 *
 * ## Summary
 * 为规范绑定不足的只读摘要，从可解析的暂停标识符值补充推断类型。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 * @param[in] identifier 借用的 AST 标识符节点；仅按其名称解析暂停值，节点及名称在本次摘要补全期间有效。
 *
 * ## Returns
 * 无返回值；通过类型环境的尽力修改交付，不提供每个条目的成功反馈。
 *
 * ## Constraints and lifecycle
 * 只处理 AST 标识符且已有环境绑定优先；不能据宽松类型补全得到 canonical symbol/type/place。RegisterVariableEx 的返回值未在此传播；临时类型在普通路径释放，环境先前变化不回滚。
 *
 * ## References
 * - zr_debug_semantic_register_summary_node（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:176）
 * - zr_debug_semantic_apply_exact_value_range（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:11）
 */

/**
 * @fn static void zr_debug_semantic_register_summary_list(ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState, const SZrAstNodeArray *nodes)
 *
 * ## Summary
 * 对 AST 列表中的节点执行摘要标识符补全。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 * @param[in] nodes 借用 AST 子节点数组；本次只读遍历，不释放或重排数组。
 *
 * ## Returns
 * 无返回值；通过类型环境的尽力修改交付，不提供每个条目的成功反馈。
 *
 * ## Constraints and lifecycle
 * 空节点列表不处理；借用列表不改其结构，环境修改和失败界限由节点注册器承担。
 *
 * ## References
 * - zr_debug_semantic_register_summary_node（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:176）
 */

/**
 * @fn static void zr_debug_semantic_register_summary_node(ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState, const SZrAstNode *node)
 *
 * ## Summary
 * 只遍历会参与摘要推断的 AST 子表达式，尝试补齐可解析变量。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 * @param[in] node 借用当前 AST 节点；NULL 可表示没有子表达式，不转移所有权。
 *
 * ## Returns
 * 无返回值；通过类型环境的尽力修改交付，不提供每个条目的成功反馈。
 *
 * ## Constraints and lifecycle
 * 属性拼写不单独当变量；只覆盖显式列出的 AST 形态。FUNCTION_CALL 分支仅遍历参数列表；CONSTRUCT 分支另遍历 target 和参数，不在这里执行 callable。注册结果可部分存在，不承诺完整环境。
 *
 * ## References
 * - zr_debug_semantic_register_summary_list（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:161）
 * - zr_debug_semantic_register_summary_value_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:113）
 * - zr_debug_semantic_register_summary_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:728）
 */

/**
 * @fn static void zr_debug_semantic_type_ref_to_inferred(SZrCompilerState *compilerState, const SZrFunctionTypedTypeRef *typeRef, SZrInferredType *result)
 *
 * ## Summary
 * 把编译产物的类型引用初始化为本次推断所需的独立临时类型。
 *
 * ## Parameters
 * @param[in,out] compilerState 借用的编译状态；通过其 state 初始化临时推断类型，并按需分配元素数组，不向类型环境注册绑定，不转移状态所有权。
 * @param[in] typeRef 借用编译产物的类型引用；NULL 使用 object 后备类型，GC 名称不转移所有权。
 * @param[out] result 待初始化的临时推断对象；不能覆盖尚未释放的本地资源，用后按 InferredType_Free 释放。
 *
 * ## Returns
 * 无返回值；通过 result 交付初始化后的临时类型，无效 compilerState 或 result 不处理。
 *
 * ## Constraints and lifecycle
 * result 应是待初始化的临时对象，不能覆盖尚未释放的拥有资源。NULL typeRef 回退 object；数组分配本地元素类型数组，GC typeName 只借用。使用后由 InferredType_Free 释放本地类型资源；Array_Init/Push 可 Throw，不承诺异常全回滚。
 *
 * ## References
 * - zr_debug_semantic_register_canonical_frame_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:331）
 * - zr_debug_semantic_register_entry_callables（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:482）
 * - ZrParser_InferredType_Free（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:880）
 */

/**
 * @fn static const SZrFunctionTypedLocalBinding *zr_debug_semantic_find_typed_local_binding( const SZrFunction *function, TZrUInt32 stackSlot)
 *
 * ## Summary
 * 按实际栈槽查找编译产物中的局部类型记录，供活动帧身份核对。
 *
 * ## Parameters
 * @param[in] function 借用含类型和局部记录的编译函数；相关数组、名称和源码范围在查询期间有效。
 * @param[in] stackSlot 实际栈槽编号，用于匹配 typedLocalBindings，不是数组下标。
 *
 * ## Returns
 * @return 找到则返回 function 内的借用类型记录，否则返回 NULL；调用者不得释放记录。
 *
 * ## Constraints and lifecycle
 * 返回记录借用 function 的 typedLocalBindings 存储；它不是 ordinal 索引，也不延长编译产物或 GC 名称的有效期。
 *
 * ## References
 * - zr_debug_semantic_register_canonical_frame_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:331）
 */

/**
 * @fn static TZrBool zr_debug_semantic_register_canonical_frame_binding( SZrCompilerState *compilerState, const SZrFunction *function, const SZrDebugFrameBinding *frameBinding, SZrString *activeName, const SZrTypeValue *activeValue)
 *
 * ## Summary
 * 把活动栈槽与同名编译类型相接，用 Core 提供的 symbol/type/place 和声明范围注册规范绑定。
 *
 * ## Parameters
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 * @param[in] function 借用含类型和局部记录的编译函数；相关数组、名称和源码范围在查询期间有效。
 * @param[in] frameBinding 借用 Core 当前上下文的活动绑定描述，携带栈槽、规范身份、place 及声明位置。
 * @param[in] activeName 借用活动栈槽的 GC 名称，须与找到的编译记录匹配。
 * @param[in] activeValue 可选借用活动值，只用于精确范围补充；不修改或持有该值。
 *
 * ## Returns
 * @return 返回注册器的普通布尔结果；参数、栈槽、名称或必要 Core 身份不满足时为假。结果不是整帧事务保证。
 *
 * ## Constraints and lifecycle
 * 要求按栈槽找到同名类型记录且 Core symbol/type 非零；不在此比较类型记录自身的 ID，也不在此把 place=0 改造成有效位置。activeValue 可缺席，仅影响精确值事实。临时类型普通释放；失败不撤销已存在环境状态。
 *
 * ## References
 * - zr_debug_semantic_register_frame_variables（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:552）
 * - zr_debug_semantic_type_ref_to_inferred（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:269）
 * - ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:368）
 */

/**
 * @fn static void zr_debug_semantic_register_entry_typed_locals(ZrDebugAgent *agent, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 没有可用暂停帧时，从入口函数中带规范身份及源码范围的顶层局部记录补充类型。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * 无返回值；通过类型环境的尽力修改交付，不提供每个条目的成功反馈。
 *
 * ## Constraints and lifecycle
 * 同名环境绑定优先；没有 source 或有效 symbol/type 的条目不发布。注册返回值未传播，不能把本函数普通结束当全部绑定成功；临时类型逐条普通释放。
 *
 * ## References
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:368）
 */

/**
 * @fn static void zr_debug_semantic_free_inferred_type_array(SZrCompilerState *compilerState, SZrArray *types)
 *
 * ## Summary
 * 逐个释放可调用签名临时类型中的本地资源，再释放外层数组。
 *
 * ## Parameters
 * @param[in,out] compilerState 借用的编译状态；通过其 state 释放临时推断类型及数组存储，不向类型环境注册绑定，不转移状态所有权。
 * @param[in,out] types 本次构造的临时推断类型数组；逐元素及容器资源由本函数在普通路径释放。
 *
 * ## Returns
 * 无返回值；普通路径释放逐元素本地类型资源和外层容器，释放后不得再借用其存储。
 *
 * ## Constraints and lifecycle
 * 只接收本次构造的类型数组；元素的 GC 名称不人工释放。完成后不得再借用原元素，不能当作回收已被环境复制的签名。
 *
 * ## References
 * - zr_debug_semantic_register_entry_callables（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:482）
 * - ZrParser_InferredType_Free（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:880）
 */

/**
 * @fn static TZrBool zr_debug_semantic_collect_callable_parameter_types(SZrCompilerState *compilerState, const SZrFunction *callable, SZrArray *paramTypes, SZrArray *parameterPassingModes)
 *
 * ## Summary
 * 将编译函数的参数类型复制为推断用临时数组，并建立一一对应的传参模式数组。
 *
 * ## Parameters
 * @param[in,out] compilerState 借用的编译状态；通过其 state 构造临时参数类型与传参模式数组，不向类型环境注册绑定，不转移状态所有权。
 * @param[in] callable 借用入口的编译子函数，参数元数据数组与计数须对应有效存储。
 * @param[out] paramTypes 本次初始化的推断参数类型数组，普通成功后由调用方逐元素及容器释放。
 * @param[out] parameterPassingModes 本次初始化的模式数组，与 paramTypes 同序；当前逐项填 VALUE，调用方释放容器。
 *
 * ## Returns
 * @return 普通真表示两组参数数组构造完成；无效参数普通返回假。分配或扩容失败可非局部 Throw，不以 false 交付异常清理。
 *
 * ## Constraints and lifecycle
 * 两个输出由本次调用初始化，普通成功时长度对应参数数目，当前所有模式为 VALUE；不是对编译元数据中其他模式的恢复。调用方负责逐元素类型和两容器释放；分配或扩容可 Throw，普通 false 只表示参数无效。
 *
 * ## References
 * - zr_debug_semantic_register_entry_callables（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:482）
 * - zr_debug_semantic_type_ref_to_inferred（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:269）
 * - ZrCore_Array_Init（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:287）
 */

/**
 * @fn static void zr_debug_semantic_register_entry_callables(ZrDebugAgent *agent, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 从入口顶层 callable 表提取返回和参数类型，供表达式推断使用而不调用运行时函数。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * 无返回值；通过类型环境的尽力修改交付，不提供每个条目的成功反馈。
 *
 * ## Constraints and lifecycle
 * 只接受有效 child 索引和 hasCallableReturnType。注册器深拷贝签名，临时返回类型、参数类型和模式数组在普通 cleanup 中释放；RegisterFunctionEx 的 false 未传播，不能承诺所有签名发布或失败事务回滚。
 *
 * ## References
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - zr_debug_semantic_collect_callable_parameter_types（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:449）
 * - ZrParser_TypeEnvironment_RegisterFunctionEx（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:529）
 */

/**
 * @fn static EZrDebugEvaluationContextStatus zr_debug_semantic_register_frame_variables( ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 在当前 Core Debug 上下文中逐一核对活动局部槽并注册其规范身份。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * @return 返回 Core 上下文或逐项注册状态；全部已检查条目成功（可为零条）时为 OK，身份冲突转换为 METADATA_UNAVAILABLE。失败前的环境变更可保留。
 *
 * ## Constraints and lifecycle
 * frameId=0 和1均映射 Core 层级0，其余减一；任一查询或身份配对失败即返回状态，先前成功注册的绑定仍可存在。活动函数、callInfo、PC 和元数据由暂停或等价稳定调用窗口保护。
 *
 * ## References
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - zr_debug_semantic_register_canonical_frame_binding（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:331）
 * - ZrCore_Debug_GetEvaluationContext（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:71）
 */

/**
 * @fn static EZrDebugEvaluationContextStatus zr_debug_semantic_register_closure_captures( ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 将 Core 验证过的捕获索引、身份、原声明范围和代际令牌复制为推断绑定。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * @return 返回 Core 上下文或逐项注册状态；全部已检查条目成功（可为零条）时为 OK，身份冲突转换为 METADATA_UNAVAILABLE。失败前的环境变更可保留。
 *
 * ## Constraints and lifecycle
 * 没有捕获时正常成功；有数量而缺列表或名称时失败。name_collision：已有 SOURCE_DECLARATION 绑定优先，其余同名来源冲突返回 metadata unavailable。临时类型普通释放；已注册捕获不全回滚，不拥有活动捕获值。
 *
 * ## References
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - ZrCore_Debug_EvaluationContext_GetClosureCapture（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:649）
 * - ZrParser_TypeEnvironment_RegisterClosureCapture（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:674）
 */

/**
 * @fn TZrBool zr_debug_semantic_register_bindings(ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState)
 *
 * ## Summary
 * 按帧、捕获、运行时根、原型及入口 callable 的顺序准备正式推断环境。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 *
 * ## Returns
 * @return 普通真表示主要准备步骤接受；假表示规范准备不足，先前变更可能保留。被忽略返回值的尽力注册不由该真值担保。
 *
 * ## Constraints and lifecycle
 * frame_fallback：仅 INVALID_ARGUMENT 退到入口类型；其他帧错误不降级。false 表示本次规范绑定准备不足，已有环境修改不回滚；调用方的 reference-free 值表达式例外及只读摘要回退由各自入口决定，不能推成全局禁止继续推断。TRUE 也不保证忽略返回值的所有 callable 注册成功。
 *
 * ## References
 * - zr_debug_semantic_register_summary_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:728）
 * - zr_debug_formal_prepare_expression（zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:292）
 */

/**
 * @fn TZrBool zr_debug_semantic_register_summary_bindings(ZrDebugAgent *agent, TZrUInt32 frameId, SZrCompilerState *compilerState, const SZrAstNode *expression)
 *
 * ## Summary
 * 先尝试规范绑定，再按 AST 从暂停值补充摘要所需类型，容许有限的部分摘要。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in,out] compilerState 当前已初始化并带有效 VM 线程和类型环境的编译状态；本次注册可增改环境，不转移其所有权。
 * @param[in] expression 借用 AST 表达式；NULL 允许规范绑定独立决定返回值。
 *
 * ## Returns
 * @return 真当规范绑定可用或 expression 非空；假表示两者均不满足。该真值不能证明所有变量已解析或获得正式求值许可。
 *
 * ## Constraints and lifecycle
 * 即使规范准备返回 false 也遍历 expression；返回 true 仅表示规范可用或 expression 非空，不证明每个名称已解析、canonical 身份完整或正式求值获准。资源借用与普通清理沿用下层，不提供整体事务。
 *
 * ## References
 * - zr_debug_append_expression_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:812）
 * - zr_debug_semantic_register_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:694）
 * - zr_debug_semantic_register_summary_node（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:176）
 */
