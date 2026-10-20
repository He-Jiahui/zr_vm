#include "eval/debug_evaluation_effect_internal.h"

static TZrBool zr_debug_formal_value_is_integer(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(value->type == ZR_VALUE_TYPE_INT8 ||
                     value->type == ZR_VALUE_TYPE_INT16 ||
                     value->type == ZR_VALUE_TYPE_INT32 ||
                     value->type == ZR_VALUE_TYPE_INT64 ||
                     value->type == ZR_VALUE_TYPE_UINT8 ||
                     value->type == ZR_VALUE_TYPE_UINT16 ||
                     value->type == ZR_VALUE_TYPE_UINT32 ||
                     value->type == ZR_VALUE_TYPE_UINT64);
}
static TZrBool zr_debug_formal_value_is_boolean(const SZrTypeValue *value) {
    return (TZrBool)(value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL);
}

static TZrBool zr_debug_formal_value_is_number(const SZrTypeValue *value) {
    return (TZrBool)(zr_debug_formal_value_is_integer(value) ||
                     (value != ZR_NULL &&
                      (value->type == ZR_VALUE_TYPE_FLOAT || value->type == ZR_VALUE_TYPE_DOUBLE)));
}

static TZrFloat64 zr_debug_formal_value_as_number(const SZrTypeValue *value) {
    if (zr_debug_formal_value_is_integer(value)) {
        return (TZrFloat64)value->value.nativeObject.nativeInt64;
    }
    return value->value.nativeObject.nativeDouble;
}

static TZrBool zr_debug_formal_integer_add_overflows(TZrInt64 left, TZrInt64 right) {
    return (TZrBool)((right > 0 && left > ZR_TYPE_RANGE_INT64_MAX - right) ||
                     (right < 0 && left < ZR_TYPE_RANGE_INT64_MIN - right));
}

static TZrBool zr_debug_formal_integer_subtract_overflows(TZrInt64 left, TZrInt64 right) {
    return (TZrBool)((right > 0 && left < ZR_TYPE_RANGE_INT64_MIN + right) ||
                     (right < 0 && left > ZR_TYPE_RANGE_INT64_MAX + right));
}

static TZrBool zr_debug_formal_integer_multiply_overflows(TZrInt64 left, TZrInt64 right) {
    if (left == 0 || right == 0) {
        return ZR_FALSE;
    }
    if (left == -1) {
        return (TZrBool)(right == ZR_TYPE_RANGE_INT64_MIN);
    }
    if (right == -1) {
        return (TZrBool)(left == ZR_TYPE_RANGE_INT64_MIN);
    }
    if (left > 0) {
        return right > 0
                       ? (TZrBool)(left > ZR_TYPE_RANGE_INT64_MAX / right)
                       : (TZrBool)(right < ZR_TYPE_RANGE_INT64_MIN / left);
    }
    return right > 0
                   ? (TZrBool)(left < ZR_TYPE_RANGE_INT64_MIN / right)
                   : (TZrBool)(left < ZR_TYPE_RANGE_INT64_MAX / right);
}

static TZrBool zr_debug_formal_read_frame_binding(ZrDebugAgent *agent,
                                                   TZrUInt32 frameId,
                                                   const SZrSemanticReferenceFact *reference,
                                                   SZrTypeValue *outValue) {
    SZrDebugEvaluationContext context;
    SZrDebugFrameBinding binding;
    TZrUInt32 index;

    if (agent == ZR_NULL || agent->state == ZR_NULL || reference == ZR_NULL || outValue == ZR_NULL ||
        !reference->isResolved || reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        reference->originKind != ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION ||
        reference->placeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(agent->state,
                                          frameId == 0u ? 0u : frameId - 1u,
                                          &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    /* 以 symbol/type/place 三重身份定位实际栈槽，避免同名局部或已过期绑定被误读。 */
    for (index = 0u; index < context.activeBindingCount; ++index) {
        const SZrTypeValue *value;

        memset(&binding, 0, sizeof(binding));
        if (ZrCore_Debug_EvaluationContext_GetBinding(agent->state,
                                                      &context,
                                                      index,
                                                      &binding) !=
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return ZR_FALSE;
        }
        if (binding.symbolId != reference->symbolId || binding.typeId != reference->typeId ||
            binding.placeId != reference->placeId) {
            continue;
        }

        value = zr_debug_frame_value_slot(agent->state,
                                          context.activation.function,
                                          context.activation.callInfo,
                                          binding.stackSlot);
        if (value == ZR_NULL) {
            return ZR_FALSE;
        }
        *outValue = *value;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_formal_read_runtime_root(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticReferenceFact *reference,
        SZrTypeValue *outValue) {
    SZrDebugEvaluationContext context;
    SZrDebugRuntimeRootBinding runtimeRoot;

    if (agent == ZR_NULL || agent->state == ZR_NULL || reference == ZR_NULL ||
        outValue == ZR_NULL || !reference->isResolved ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        reference->originKind != ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT ||
        reference->runtimeRootKind != ZR_SEMANTIC_RUNTIME_ROOT_ZR ||
        reference->originToken == 0u || reference->placeId != ZR_SEMANTIC_ID_INVALID ||
        reference->declarationRange.source != ZR_NULL || reference->hasDefinitionRange ||
        reference->definitionRange.source != ZR_NULL || reference->definitionRanges.length != 0u) {
        return ZR_FALSE;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(
                agent->state,
                frameId == 0u ? 0u : frameId - 1u,
                &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    runtimeRoot.kind = ZR_DEBUG_RUNTIME_ROOT_ZR;
    runtimeRoot.token = reference->originToken;
    ZrCore_Value_ResetAsNull(outValue);
    return (TZrBool)(ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                             agent->state, &context, &runtimeRoot, outValue) ==
                     ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK);
}

static TZrBool zr_debug_formal_read_closure_capture(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticReferenceFact *reference,
        SZrTypeValue *outValue) {
    SZrDebugEvaluationContext context;
    SZrDebugClosureCaptureBinding capture;

    if (agent == ZR_NULL || agent->state == ZR_NULL || reference == ZR_NULL || outValue == ZR_NULL ||
        !reference->isResolved || reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        reference->originKind != ZR_SEMANTIC_REFERENCE_ORIGIN_CLOSURE_CAPTURE ||
        reference->runtimeRootKind != ZR_SEMANTIC_RUNTIME_ROOT_NONE ||
        reference->originToken == 0u || reference->placeId != ZR_SEMANTIC_ID_INVALID ||
        reference->declarationRange.source == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(
                agent->state,
                frameId == 0u ? 0u : frameId - 1u,
                &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        context.activation.function == ZR_NULL ||
        reference->declarationRange.source != context.activation.function->sourceCodeList) {
        return ZR_FALSE;
    }

    memset(&capture, 0, sizeof(capture));
    /* 捕获元数据须同时匹配词法来源与运行时 token，才可取出暂停帧中的闭包值。 */
    if (ZrCore_Debug_EvaluationContext_GetClosureCapture(
                agent->state,
                &context,
                reference->originIndex,
                &capture) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        capture.captureIndex != reference->originIndex ||
        capture.symbolId != reference->symbolId || capture.typeId != reference->typeId ||
        capture.token != reference->originToken ||
        capture.declarationStartLine != (TZrUInt32)reference->declarationRange.start.line ||
        capture.declarationStartColumn != (TZrUInt32)reference->declarationRange.start.column ||
        capture.declarationEndLine != (TZrUInt32)reference->declarationRange.end.line ||
        capture.declarationEndColumn != (TZrUInt32)reference->declarationRange.end.column) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(outValue);
    return (TZrBool)(ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
                             agent->state,
                             &context,
                             &capture,
                             outValue) == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK);
}

static TZrBool zr_debug_formal_read_reference_value(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticReferenceFact *reference,
        SZrTypeValue *outValue) {
    if (reference == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (reference->originKind) {
        case ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION:
            return zr_debug_formal_read_frame_binding(agent, frameId, reference, outValue);
        case ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT:
            return zr_debug_formal_read_runtime_root(agent, frameId, reference, outValue);
        case ZR_SEMANTIC_REFERENCE_ORIGIN_CLOSURE_CAPTURE:
            return zr_debug_formal_read_closure_capture(agent, frameId, reference, outValue);
        default:
            return ZR_FALSE;
    }
}

TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact) {
    const SZrSemanticReferenceFact *reference;
    SZrTypeValue frameValue;
    TZrSize index;

    if (semanticContext == ZR_NULL || expression == ZR_NULL || expressionFact == ZR_NULL ||
        expression->type != ZR_AST_PRIMARY_EXPRESSION ||
        expression->data.primaryExpression.property == ZR_NULL ||
        expression->data.primaryExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.primaryExpression.members == ZR_NULL ||
        expression->data.primaryExpression.members->count == 0u ||
        expressionFact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_ERROR) {
        return ZR_FALSE;
    }

    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            semanticContext,
            expression->data.primaryExpression.property,
            ZR_SEMANTIC_REFERENCE_READ);
    if (reference == ZR_NULL || !reference->isResolved ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    /* 规范事实缺失时只承认已解析数组根和整数字面量索引这一可直接核验的窄路径。 */
    ZrCore_Value_ResetAsNull(&frameValue);
    if (!zr_debug_formal_read_reference_value(agent, frameId, reference, &frameValue) ||
        frameValue.type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    for (index = 0u; index < expression->data.primaryExpression.members->count; ++index) {
        const SZrAstNode *member = expression->data.primaryExpression.members->nodes[index];

        if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION ||
            !member->data.memberExpression.computed ||
            member->data.memberExpression.property == ZR_NULL ||
            member->data.memberExpression.property->type != ZR_AST_INTEGER_LITERAL) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_formal_evaluate_array_literal(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *node,
        SZrTypeValue *outValue,
        TZrBool *outSupported,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    SZrObject *array;
    SZrTypeValue arrayValue;
    SZrTypeValue elementValue;
    SZrTypeValue indexValue;
    const SZrAstNodeArray *elements;
    TZrSize index;

    if (agent == ZR_NULL || agent->state == ZR_NULL || semanticContext == ZR_NULL ||
        node == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL || outValue == ZR_NULL ||
        outSupported == ZR_NULL) {
        if (outSupported != ZR_NULL) {
            *outSupported = ZR_FALSE;
        }
        return ZR_TRUE;
    }

    /* 分配由上层能力位许可；数组由 VM 构造，结果句柄在快照层注册。 */
    array = ZrCore_Object_NewCustomized(
            agent->state, sizeof(*array), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "formal array allocation failed");
        return ZR_FALSE;
    }
    ZrCore_Object_Init(agent->state, array);
    ZrCore_Value_InitAsRawObject(agent->state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(array));

    elements = node->data.arrayLiteral.elements;
    for (index = 0u; elements != ZR_NULL && index < elements->count; ++index) {
        const SZrAstNode *element = elements->nodes[index];

        if (element == ZR_NULL) {
            *outSupported = ZR_FALSE;
            return ZR_TRUE;
        }
        ZrCore_Value_ResetAsNull(&elementValue);
        if (!zr_debug_formal_evaluate_node(agent,
                                           frameId,
                                           semanticContext,
                                           element,
                                           &elementValue,
                                           outSupported,
                                           errorBuffer,
                                           errorBufferSize)) {
            return ZR_FALSE;
        }
        if (!*outSupported) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsInt(agent->state, &indexValue, (TZrInt64)index);
        if (!ZrCore_Object_TrySetValue(agent->state, array, &indexValue, &elementValue)) {
            *outSupported = ZR_FALSE;
            return ZR_TRUE;
        }
    }

    *outValue = arrayValue;
    return ZR_TRUE;
}

TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent,
                                             TZrUInt32 frameId,
                                             const SZrSemanticContext *semanticContext,
                                             const SZrAstNode *node,
                                             SZrTypeValue *outValue,
                                             TZrBool *outSupported,
                                             TZrChar *errorBuffer,
                                             TZrSize errorBufferSize) {
    SZrTypeValue left;
    SZrTypeValue right;
    const TZrChar *op;
    TZrInt64 shiftAmount;
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;

    if (outSupported != ZR_NULL) {
        *outSupported = ZR_TRUE;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || semanticContext == ZR_NULL ||
        node == ZR_NULL || outValue == ZR_NULL || outSupported == ZR_NULL) {
        if (outSupported != ZR_NULL) {
            *outSupported = ZR_FALSE;
        }
        return ZR_TRUE;
    }

    /* 仅处理已支持的 AST 子集；未支持节点以 outSupported 回传给上层作拒绝或兼容决策。 */
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrCore_Value_InitAsBool(agent->state, outValue, node->data.booleanLiteral.value);
            return ZR_TRUE;
        case ZR_AST_NULL_LITERAL:
            ZrCore_Value_ResetAsNull(outValue);
            return ZR_TRUE;
        case ZR_AST_FLOAT_LITERAL:
            ZrCore_Value_InitAsFloat(agent->state, outValue, node->data.floatLiteral.value);
            return ZR_TRUE;
        case ZR_AST_INTEGER_LITERAL:
            ZrCore_Value_InitAsInt(agent->state, outValue, node->data.integerLiteral.value);
            return ZR_TRUE;
        case ZR_AST_ARRAY_LITERAL:
            return zr_debug_formal_evaluate_array_literal(agent,
                                                          frameId,
                                                          semanticContext,
                                                          node,
                                                          outValue,
                                                          outSupported,
                                                          errorBuffer,
                                                          errorBufferSize);
        case ZR_AST_IDENTIFIER_LITERAL: {
            const SZrSemanticReferenceFact *reference =
                    ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                            semanticContext,
                            node,
                            ZR_SEMANTIC_REFERENCE_READ);

            if (reference == ZR_NULL || !reference->isResolved ||
                reference->symbolId == ZR_SEMANTIC_ID_INVALID) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "canonical paused-frame binding is unavailable for the resolved reference");
                return ZR_FALSE;
            }
            if (!zr_debug_formal_read_reference_value(agent,
                                                       frameId,
                                                       reference,
                                                       outValue)) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }
            return ZR_TRUE;
        }
        case ZR_AST_PRIMARY_EXPRESSION:
            ZrCore_Value_ResetAsNull(&left);
            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.primaryExpression.property,
                                               &left,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }
            if (node->data.primaryExpression.members == ZR_NULL) {
                *outValue = left;
                return ZR_TRUE;
            }
            for (TZrSize index = 0u; index < node->data.primaryExpression.members->count; ++index) {
                const SZrAstNode *member = node->data.primaryExpression.members->nodes[index];

                if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION ||
                    !member->data.memberExpression.computed ||
                    left.type != ZR_VALUE_TYPE_ARRAY) {
                    *outSupported = ZR_FALSE;
                    return ZR_TRUE;
                }
                ZrCore_Value_ResetAsNull(&right);
                if (!zr_debug_formal_evaluate_node(agent,
                                                   frameId,
                                                   semanticContext,
                                                   member->data.memberExpression.property,
                                                   &right,
                                                   outSupported,
                                                   errorBuffer,
                                                   errorBufferSize)) {
                    return ZR_FALSE;
                }
                if (!*outSupported) {
                    return ZR_TRUE;
                }
                if (!zr_debug_formal_value_is_integer(&right) ||
                    !ZrCore_Object_GetByIndex(agent->state, &left, &right, outValue)) {
                    *outSupported = ZR_FALSE;
                    return ZR_TRUE;
                }
                left = *outValue;
            }
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION:
            ZrCore_Value_ResetAsNull(&left);
            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.unaryExpression.argument,
                                               &left,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }
            op = node->data.unaryExpression.op.op;
            if (op == ZR_NULL) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }
            if (strcmp(op, "!") == 0) {
                if (!zr_debug_formal_value_is_boolean(&left)) {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal logical negation requires a boolean operand");
                    return ZR_FALSE;
                }
                ZrCore_Value_InitAsBool(agent->state,
                                        outValue,
                                        left.value.nativeObject.nativeBool ? ZR_FALSE : ZR_TRUE);
                return ZR_TRUE;
            }
            if (strcmp(op, "~") == 0) {
                if (!zr_debug_formal_value_is_integer(&left)) {
                    *outSupported = ZR_FALSE;
                    return ZR_TRUE;
                }
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       ~left.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
                if (!zr_debug_formal_value_is_number(&left)) {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal numeric negation requires a numeric operand");
                    return ZR_FALSE;
                }
                if (!zr_debug_formal_value_is_integer(&left)) {
                    ZrCore_Value_InitAsFloat(agent->state,
                                             outValue,
                                             strcmp(op, "-") == 0
                                                     ? -left.value.nativeObject.nativeDouble
                                                     : left.value.nativeObject.nativeDouble);
                    return ZR_TRUE;
                }
                if (strcmp(op, "-") == 0 &&
                    left.value.nativeObject.nativeInt64 == ZR_TYPE_RANGE_INT64_MIN) {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal integer negation overflows int64");
                    return ZR_FALSE;
                }
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       strcmp(op, "-") == 0
                                               ? -left.value.nativeObject.nativeInt64
                                               : left.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            *outSupported = ZR_FALSE;
            return ZR_TRUE;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            ZrCore_Value_ResetAsNull(&left);
            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.conditionalExpression.test,
                                               &left,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }
            if (!zr_debug_formal_value_is_boolean(&left)) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "formal conditional evaluation requires a boolean test");
                return ZR_FALSE;
            }
            return zr_debug_formal_evaluate_node(
                    agent,
                    frameId,
                    semanticContext,
                    left.value.nativeObject.nativeBool
                            ? node->data.conditionalExpression.consequent
                            : node->data.conditionalExpression.alternate,
                    outValue,
                    outSupported,
                    errorBuffer,
                    errorBufferSize);
        case ZR_AST_LOGICAL_EXPRESSION:
            ZrCore_Value_ResetAsNull(&left);
            ZrCore_Value_ResetAsNull(&right);
            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.logicalExpression.left,
                                               &left,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }
            if (!zr_debug_formal_value_is_boolean(&left)) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "formal logical evaluation requires boolean operands");
                return ZR_FALSE;
            }

            op = node->data.logicalExpression.op;
            if (op == ZR_NULL || (strcmp(op, "&&") != 0 && strcmp(op, "||") != 0)) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }
            /* 短路分支不读取右侧暂停帧值；能力分类已在执行前覆盖整棵 AST。 */
            if ((strcmp(op, "&&") == 0 && !left.value.nativeObject.nativeBool) ||
                (strcmp(op, "||") == 0 && left.value.nativeObject.nativeBool)) {
                ZrCore_Value_InitAsBool(agent->state, outValue, left.value.nativeObject.nativeBool);
                return ZR_TRUE;
            }

            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.logicalExpression.right,
                                               &right,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }
            if (!zr_debug_formal_value_is_boolean(&right)) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "formal logical evaluation requires boolean operands");
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsBool(agent->state, outValue, right.value.nativeObject.nativeBool);
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            ZrCore_Value_ResetAsNull(&left);
            ZrCore_Value_ResetAsNull(&right);
            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.binaryExpression.left,
                                               &left,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }

            op = node->data.binaryExpression.op.op;
            if (op == ZR_NULL) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }

            if (!zr_debug_formal_evaluate_node(agent,
                                               frameId,
                                               semanticContext,
                                               node->data.binaryExpression.right,
                                               &right,
                                               outSupported,
                                               errorBuffer,
                                               errorBufferSize)) {
                return ZR_FALSE;
            }
            if (!*outSupported) {
                return ZR_TRUE;
            }

            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                TZrBool equals;

                if (zr_debug_formal_value_is_number(&left) && zr_debug_formal_value_is_number(&right)) {
                    equals = (TZrBool)(zr_debug_formal_value_as_number(&left) ==
                                       zr_debug_formal_value_as_number(&right));
                } else if (zr_debug_formal_value_is_boolean(&left) &&
                           zr_debug_formal_value_is_boolean(&right)) {
                    equals = (TZrBool)(left.value.nativeObject.nativeBool ==
                                       right.value.nativeObject.nativeBool);
                } else if (left.type == ZR_VALUE_TYPE_NULL && right.type == ZR_VALUE_TYPE_NULL) {
                    equals = ZR_TRUE;
                } else {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal equality evaluation requires matching numeric, boolean, or null operands");
                    return ZR_FALSE;
                }
                ZrCore_Value_InitAsBool(agent->state,
                                        outValue,
                                        strcmp(op, "==") == 0 ? equals : !equals);
                return ZR_TRUE;
            }
            if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
                strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
                if (!zr_debug_formal_value_is_number(&left) ||
                    !zr_debug_formal_value_is_number(&right)) {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal comparison evaluation requires numeric operands");
                    return ZR_FALSE;
                }
                leftNumber = zr_debug_formal_value_as_number(&left);
                rightNumber = zr_debug_formal_value_as_number(&right);
                if (strcmp(op, "<") == 0) {
                    ZrCore_Value_InitAsBool(agent->state,
                                            outValue,
                                            leftNumber < rightNumber);
                } else if (strcmp(op, "<=") == 0) {
                    ZrCore_Value_InitAsBool(agent->state,
                                            outValue,
                                            leftNumber <= rightNumber);
                } else if (strcmp(op, ">") == 0) {
                    ZrCore_Value_InitAsBool(agent->state,
                                            outValue,
                                            leftNumber > rightNumber);
                } else {
                    ZrCore_Value_InitAsBool(agent->state,
                                            outValue,
                                            leftNumber >= rightNumber);
                }
                return ZR_TRUE;
            }
            if (strcmp(op, "+") != 0 && strcmp(op, "-") != 0 && strcmp(op, "*") != 0 &&
                strcmp(op, "/") != 0 && strcmp(op, "%") != 0 &&
                strcmp(op, "<<") != 0 && strcmp(op, ">>") != 0 &&
                strcmp(op, "&") != 0 && strcmp(op, "|") != 0 && strcmp(op, "^") != 0) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }
            if (strcmp(op, "/") == 0) {
                if (!zr_debug_formal_value_is_number(&left) ||
                    !zr_debug_formal_value_is_number(&right) ||
                    zr_debug_formal_value_as_number(&right) == 0.0) {
                    *outSupported = ZR_FALSE;
                    return ZR_TRUE;
                }
                ZrCore_Value_InitAsFloat(agent->state,
                                         outValue,
                                         zr_debug_formal_value_as_number(&left) /
                                                 zr_debug_formal_value_as_number(&right));
                return ZR_TRUE;
            }
            if (strcmp(op, "%") == 0) {
                if (!zr_debug_formal_value_is_integer(&left) ||
                    !zr_debug_formal_value_is_integer(&right) ||
                    right.value.nativeObject.nativeInt64 == 0 ||
                    (left.value.nativeObject.nativeInt64 == ZR_TYPE_RANGE_INT64_MIN &&
                     right.value.nativeObject.nativeInt64 == -1)) {
                    *outSupported = ZR_FALSE;
                    return ZR_TRUE;
                }
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 %
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0) {
                if (!zr_debug_formal_value_is_number(&left) ||
                    !zr_debug_formal_value_is_number(&right)) {
                    zr_debug_copy_text(errorBuffer,
                                       errorBufferSize,
                                       "formal arithmetic evaluation requires numeric operands");
                    return ZR_FALSE;
                }
                if (!zr_debug_formal_value_is_integer(&left) ||
                    !zr_debug_formal_value_is_integer(&right)) {
                    leftNumber = zr_debug_formal_value_as_number(&left);
                    rightNumber = zr_debug_formal_value_as_number(&right);
                    if (strcmp(op, "+") == 0) {
                        ZrCore_Value_InitAsFloat(agent->state, outValue, leftNumber + rightNumber);
                    } else if (strcmp(op, "-") == 0) {
                        ZrCore_Value_InitAsFloat(agent->state, outValue, leftNumber - rightNumber);
                    } else {
                        ZrCore_Value_InitAsFloat(agent->state, outValue, leftNumber * rightNumber);
                    }
                    return ZR_TRUE;
                }
            }
            if (!zr_debug_formal_value_is_integer(&left) ||
                !zr_debug_formal_value_is_integer(&right)) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "formal integer binary evaluation requires integer operands");
                return ZR_FALSE;
            }

            if ((strcmp(op, "+") == 0 &&
                 zr_debug_formal_integer_add_overflows(left.value.nativeObject.nativeInt64,
                                                        right.value.nativeObject.nativeInt64)) ||
                (strcmp(op, "-") == 0 &&
                 zr_debug_formal_integer_subtract_overflows(left.value.nativeObject.nativeInt64,
                                                             right.value.nativeObject.nativeInt64)) ||
                (strcmp(op, "*") == 0 &&
                 zr_debug_formal_integer_multiply_overflows(left.value.nativeObject.nativeInt64,
                                                             right.value.nativeObject.nativeInt64))) {
                *outSupported = ZR_FALSE;
                return ZR_TRUE;
            }
            if (strcmp(op, "+") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 +
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "-") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 -
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "*") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 *
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }

            if (strcmp(op, "&") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 &
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "|") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 |
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (strcmp(op, "^") == 0) {
                ZrCore_Value_InitAsInt(agent->state,
                                       outValue,
                                       left.value.nativeObject.nativeInt64 ^
                                               right.value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }

            shiftAmount = right.value.nativeObject.nativeInt64;
            if (left.value.nativeObject.nativeInt64 < 0 || shiftAmount < 0 || shiftAmount >= 63) {
                zr_debug_copy_text(errorBuffer,
                                   errorBufferSize,
                                   "shift requires a non-negative value and shift count below 63");
                return ZR_FALSE;
            }
            /* TODO: 当前只校验左值非负及位数；2 << 62 等结果超出 int64 可表示范围。
             * 需沿 ZrDebug_Evaluate 的规范求值路径补充溢出用例，确认类型推断是否先行拒绝。
             */
            ZrCore_Value_InitAsInt(agent->state,
                                   outValue,
                                   strcmp(op, "<<") == 0
                                           ? left.value.nativeObject.nativeInt64 << (TZrUInt32)shiftAmount
                                           : left.value.nativeObject.nativeInt64 >> (TZrUInt32)shiftAmount);
            return ZR_TRUE;
        default:
            *outSupported = ZR_FALSE;
            return ZR_TRUE;
    }
}

/**
 * @fn TZrBool zr_debug_formal_value_is_integer(const SZrTypeValue *value)
 * ## Summary
 * 识别正式执行器使用的八种整数值标签，使位运算、索引及整数溢出预检共享同一入口。
 *
 * ## Parameters
 * @param[in] value 借用值记录；可为 NULL。
 *
 * ## Returns
 * @return INT8/16/32/64 或 UINT8/16/32/64 返回 TRUE，其他类型和 NULL 返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 只读取类型标签，无分配和状态修改。TRUE 不验证 nativeInt64 当前表示、值域或后续有符号运算安全；调用者仍负责相应范围门禁。
 *
 * ## References
 * - zr_debug_formal_value_is_number — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:22（实际调用或注册）；组合数值标签。
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:461（实际调用或注册）；选择整数算术、位运算与索引。
 */

/**
 * @fn TZrBool zr_debug_formal_value_is_boolean(const SZrTypeValue *value)
 * ## Summary
 * 为正式逻辑、条件与布尔比较提供严格 BOOL 标签判断，区别于旧兼容求值器的宽泛真值转换。
 *
 * ## Parameters
 * @param[in] value 借用值记录；可为 NULL。
 *
 * ## Returns
 * @return 仅 ZR_VALUE_TYPE_BOOL 返回 TRUE；其他标签和 NULL 返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 纯标签判断，不读取未确认为 BOOL 的布尔载荷，不做数值或对象到布尔的隐式转换，也不持有任何值。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:490（实际调用或注册）；逻辑、条件、取反及相等判断的布尔前提。
 */

/**
 * @fn TZrBool zr_debug_formal_value_is_number(const SZrTypeValue *value)
 * ## Summary
 * 统一正式算术与比较接受的数值标签集合，组合整数家族与两种浮点标签。
 *
 * ## Parameters
 * @param[in] value 借用值记录；可为 NULL。
 *
 * ## Returns
 * @return 整数家族、FLOAT 或 DOUBLE 返回 TRUE，其余及 NULL 为 FALSE。
 *
 * ## Constraints and lifecycle
 * 不执行转换，不检查有限性、精度或某个运算是否受支持。具体的零除、整数范围和运算符分支由正式执行器决定。
 *
 * ## References
 * - zr_debug_formal_value_is_integer — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:22（配套消费位置）；整数标签子集。
 * - zr_debug_formal_value_as_number — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:27（配套实现定义）；数值标签确认后的转换。
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:512（实际调用或注册）；数值操作消费者。
 */

/**
 * @fn TZrFloat64 zr_debug_formal_value_as_number(const SZrTypeValue *value)
 * ## Summary
 * 把已确认的数值记录读成 TZrFloat64，供正式比较、混合浮点算术与除法使用。
 *
 * ## Parameters
 * @param[in] value 非 NULL 且已经 value_is_number 确认的值记录；不在此处验证参数。
 *
 * ## Returns
 * @return 整数标签按 nativeInt64 转为 TZrFloat64；其余受约束的浮点标签读取 nativeDouble。
 *
 * ## Constraints and lifecycle
 * 纯读取，无新所有权。转换可能损失大整数精度，且整数路径实际采用有符号 nativeInt64 视图；不能据整数标签识别范围承诺全 UINT64 数学值无损转换。调用前必须完成类型检查。
 *
 * ## References
 * - zr_debug_formal_value_is_number — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:21（配套实现定义）；配套前提检查。
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:671（实际调用或注册）；数值相等、顺序、除法及混合算术。
 */

/**
 * @fn TZrBool zr_debug_formal_integer_add_overflows(TZrInt64 left, TZrInt64 right)
 * ## Summary
 * 在正式整数加法执行之前判定有符号 int64 结果是否越过实现范围，避免为预检先执行可能溢出的加法。
 *
 * ## Parameters
 * @param[in] left 加法左值，按 int64 解释。
 * @param[in] right 加法右值，按 int64 解释。
 *
 * ## Returns
 * @return 结果超出 INT64_MIN..INT64_MAX 时 TRUE，否则 FALSE。
 *
 * ## Constraints and lifecycle
 * 纯值边界检查，不改变参数或 VM。调用方仅在 FALSE 后执行实际加法；本函数不处理其他运算，也不把溢出直接转为诊断。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:786（实际调用或注册）；整数加法前调用并在溢出时回传未支持。
 */

/**
 * @fn TZrBool zr_debug_formal_integer_subtract_overflows(TZrInt64 left, TZrInt64 right)
 * ## Summary
 * 在正式整数减法之前判定有符号 int64 结果的上下界，供执行器选择未支持而非执行溢出的减法。
 *
 * ## Parameters
 * @param[in] left 被减数，按 int64 解释。
 * @param[in] right 减数，按 int64 解释。
 *
 * ## Returns
 * @return 结果超出 int64 可表示范围时 TRUE，否则 FALSE。
 *
 * ## Constraints and lifecycle
 * 不分配、不修改 VM，也不直接做 left-right；调用方根据该布尔值决定是否执行。零、正、负减数使用各自边界，不替代加法或乘法检查。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:789（实际调用或注册）；正式整数减法前的范围门禁。
 */

/**
 * @fn TZrBool zr_debug_formal_integer_multiply_overflows(TZrInt64 left, TZrInt64 right)
 * ## Summary
 * 在正式整数乘法前按符号与商边界检查 int64 结果，明确处理零及最小值乘负一这一不能直接取反的组合。
 *
 * ## Parameters
 * @param[in] left 乘法左值，按 int64 解释。
 * @param[in] right 乘法右值，按 int64 解释。
 *
 * ## Returns
 * @return 乘积超出 int64 范围时 TRUE；零乘积等可表示结果为 FALSE。
 *
 * ## Constraints and lifecycle
 * 预检不先形成 left*right，先处理零与 INT64_MIN/-1，再用按符号选择的除法边界判定。只交付布尔结果，由执行器决定未支持状态，不产生资源或错误文本。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:792（实际调用或注册）；整数乘法执行前使用此预检。
 */

/**
 * @fn TZrBool zr_debug_formal_read_frame_binding(ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticReferenceFact *reference, SZrTypeValue *outValue)
 * ## Summary
 * 用语义事实的规范身份在当前暂停帧中定位局部变量，拒绝仅同名、类型已变或位置已失效的栈槽，避免调试读取错配。
 *
 * ## Parameters
 * @param[in] agent 借用有效代理与 VM，暂停帧的有效性由 Core EvaluationContext 再核对。
 * @param[in] frameId 调试帧号；0 与 1 都映射到 Core 深度 0，其余映射为 frameId-1。
 * @param[in] reference 借用已解析 SOURCE_DECLARATION 事实，symbol/type/place 必须有效。
 * @param[out] outValue 非 NULL 输出；仅匹配并取得有效栈槽时浅拷值记录，其他失败不保证改写。
 *
 * ## Returns
 * @return 当前 context 可读、遍历到三重身份一致的活动 binding 且栈槽可取时 TRUE；请求、Core 状态或匹配失败返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 读取不按拼写回退；symbolId、typeId、placeId 同时相等后才通过实际 activation/function/callInfo 和 stackSlot 取槽。中途 GetBinding 失败立即返回。输出不延长暂停帧或所引用 GC 对象寿命，不转移局部变量所有权；resume 或元数据变化后须重新准备并解析身份。
 *
 * ## References
 * - zr_debug_formal_read_reference_value — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:218（实际调用或注册）；SOURCE_DECLARATION 分派。
 * - ZrCore_Debug_GetEvaluationContext — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:81（配套消费位置）；取得当前帧视图。
 * - zr_debug_frame_value_slot — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:104（配套消费位置）；只在身份匹配后定位实际槽。
 */

/**
 * @fn TZrBool zr_debug_formal_read_runtime_root( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticReferenceFact *reference, SZrTypeValue *outValue)
 * ## Summary
 * 读取语义事实明确标识的 zr 运行时根对象，以 Core 的帧 generation token 复核其来源，避免把无源码身份的根误当局部变量。
 *
 * ## Parameters
 * @param[in] agent 借用有效代理及 VM。
 * @param[in] frameId 当前调试帧号，按 0/1 对应 Core 深度 0 的规则转换。
 * @param[in] reference 借用已解析 RUNTIME_ROOT 事实；只接受 ZR kind、非零 token、有效 symbol/type、无 place 与源码/definition 范围。
 * @param[out] outValue 非 NULL 输出；本地与 Core 请求门禁后先置 NULL，再由 Core 解析成功交付浅值快照。
 *
 * ## Returns
 * @return Core context 和 runtime root 解析均为 OK 时 TRUE；身份形状、generation 或根可用性不符时 FALSE。
 *
 * ## Constraints and lifecycle
 * root 的 token 必须等于当前 frameGeneration，且 Core 再确认 zr root 当前可用。输出不建立永久 root、不复制整个对象，不允许调用方释放其原始 VM 存储；失败路径中输出可能已被置 NULL。
 *
 * ## References
 * - zr_debug_formal_read_reference_value — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:220（实际调用或注册）；RUNTIME_ROOT 分派。
 * - ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:149（配套消费位置）；generation 和可用性复核。
 */

/**
 * @fn TZrBool zr_debug_formal_read_closure_capture( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticReferenceFact *reference, SZrTypeValue *outValue)
 * ## Summary
 * 将编译期 closure-capture 事实与当前暂停函数的捕获元数据及运行时 generation 对齐后读取捕获值，防止按过期索引或名字读闭包。
 *
 * ## Parameters
 * @param[in] agent 借用有效代理和 VM。
 * @param[in] frameId 当前调试帧号，转换为 Core context 深度。
 * @param[in] reference 借用已解析 CLOSURE_CAPTURE 事实，要求有效 symbol/type、非零 token、无 place/runtimeRoot、有效 declaration source。
 * @param[out] outValue 非 NULL 输出；全部本地身份检查后置 NULL，再由 Core 交付浅值快照。
 *
 * ## Returns
 * @return 当前函数来源、capture 元数据以及 Core 再验证全部匹配时 TRUE，否则 FALSE。
 *
 * ## Constraints and lifecycle
 * 先要求 declarationRange.source 与当前 function.sourceCodeList 同一对象，再核 originIndex、symbol/type、token、行列范围。Core 随后复核 frameGeneration 和当前实际 capture binding，并读取有效 closure slot。没有名称或相邻槽回退；结果仍依赖暂停帧/VM，不获得独立对象所有权。
 *
 * ## References
 * - zr_debug_formal_read_reference_value — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:222（实际调用或注册）；CLOSURE_CAPTURE 分派。
 * - ZrCore_Debug_EvaluationContext_GetClosureCapture — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:184（配套消费位置）；取得当前捕获身份。
 * - ZrCore_Debug_EvaluationContext_ResolveClosureCapture — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:200（配套消费位置）；复核后读取捕获值。
 */

/**
 * @fn TZrBool zr_debug_formal_read_reference_value( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticReferenceFact *reference, SZrTypeValue *outValue)
 * ## Summary
 * 按规范引用事实的 origin 选择局部栈槽、zr runtime root 或闭包捕获的实际读值路径，集中禁止未知来源的拼写式猜测。
 *
 * ## Parameters
 * @param[in] agent 借用代理；交由选中的来源读取函数核对其 VM。
 * @param[in] frameId 传给来源专用读取函数的调试帧号。
 * @param[in] reference 借用语义引用事实；NULL 或未知 origin 不可读取。
 * @param[out] outValue 交给来源读取函数的输出槽；是否改写及失败状态遵循该函数契约。
 *
 * ## Returns
 * @return 原样返回三种已知来源读取结果；NULL 引用或未支持 origin 返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 本层不分配或持有事实，不额外提升其有效性。有效事实必须来自仍存活的 semanticContext；不按名称、无来源 root 或其他 origin 进行降级读取。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:410（实际调用或注册）；标识符执行消费此分派。
 * - zr_debug_formal_has_paused_array_index_facts — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:260（实际调用或注册）；窄数组事实识别读取 root 实值。
 * - zr_debug_formal_read_frame_binding — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:218（配套消费位置）；SOURCE_DECLARATION。
 * - zr_debug_formal_read_runtime_root — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:220（配套消费位置）；RUNTIME_ROOT。
 * - zr_debug_formal_read_closure_capture — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:222（配套消费位置）；CLOSURE_CAPTURE。
 */

/**
 * @fn TZrBool zr_debug_formal_has_paused_array_index_facts( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *semanticContext, const SZrAstNode *expression, const SZrSemanticExpressionFact *expressionFact)
 * ## Summary
 * 当一般规范事实门未满足时，识别可用当前暂停数组根实值支撑的整数字面量索引链，提供正式准备阶段的窄兼容事实判断。
 *
 * ## Parameters
 * @param[in] agent 借用代理和 VM，供 root 引用的来源读取函数验证。
 * @param[in] frameId 读取 root 所用的同一调试帧号。
 * @param[in] semanticContext 仍有效的语义上下文，供查找 root READ 事实。
 * @param[in] expression 借用 primary AST，必须是 identifier root 且存在成员链。
 * @param[in] expressionFact 借用与本次 AST 配套的表达式事实；本函数要求 exactness 为 EXACT 且 kind 不是 ERROR，不在此处核对事实的节点身份。
 *
 * ## Returns
 * @return root READ 已解析且 symbol 有效、当前值为 ARRAY，所有成员均为 computed MEMBER_EXPRESSION 且 property 为整数 AST 字面量时 TRUE；任一条件不满足则 FALSE。
 *
 * ## Constraints and lifecycle
 * 只读事实与 root 值，不创建 handle 或修改 AST。不会在这里读取整个链的元素、验证索引非负或越界，也不保证各级元素仍是 ARRAY；这些失败由实际执行器处理。TRUE 不能等价为整个表达式已完成一般类型/规范证明。
 *
 * ## References
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:282（实际调用或注册）；一般 hasCanonicalFacts 为 FALSE 后的窄补充。
 * - zr_debug_formal_read_reference_value — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:260（配套消费位置）；按当前来源读 root 实值。
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:347（配套实现定义）；随后执行每级索引。
 */

/**
 * @fn TZrBool zr_debug_formal_evaluate_array_literal( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *semanticContext, const SZrAstNode *node, SZrTypeValue *outValue, TZrBool *outSupported, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 在上层已授予分配能力后构造正式数组字面量，按源码顺序求值元素并写入连续整数索引，交付后由快照层决定是否建立变量句柄。
 *
 * ## Parameters
 * @param[in] agent 借用有效代理及 VM；无效请求回传未支持。
 * @param[in] frameId 元素递归求值使用的同一调试帧号。
 * @param[in] semanticContext 非 NULL 且仍有效的借用语义上下文，供元素引用查找；NULL 请求回传未支持。
 * @param[in] node 借用 ARRAY_LITERAL AST，元素列表在递归期间有效；其他节点类型回传未支持。
 * @param[out] outValue 仅整数组构造成功后写入值记录；失败或未支持不发布部分数组。
 * @param[in,out] outSupported 必需支持状态槽，由 evaluate_node 在入口设 TRUE；本函数保留成功路径的当前值，遇无效请求、空元素、未支持元素或普通写入失败置 FALSE。
 * @param[out] errorBuffer 与容量配套的可选错误文本输出。
 * @param[in] errorBufferSize 错误输出的可写字节容量。
 *
 * ## Returns
 * @return VM 对象申请普通失败或递归硬执行错误返回 FALSE；不支持元素/写入则 TRUE 且 outSupported 为 FALSE；成功时 TRUE 并交付数组值。
 *
 * ## Constraints and lifecycle
 * 本函数不重复检查 ALLOCATION 效果许可；只能从已完成整树能力门禁且支持位已初始化为 TRUE 的正式路径调用。空数组成功时也不重新设置支持位。对象由 VM/GC 管理，不要求调用者 raw-free；普通未处理时不把部分构造发布到 outValue。Object_TrySetValue 自身保护 root lease 与普通失败清理，但不能由此承诺整个递归或非局部退出事务回滚。
 *
 * ## References
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:323（配套消费位置）；递归求值数组元素。
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:307（配套实现定义）；执行前整树能力门禁。
 * - ZrCore_Object_TrySetValue — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:337（配套消费位置）；逐索引写入并处理其自身失败状态。
 */

/**
 * @fn TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *semanticContext, const SZrAstNode *node, SZrTypeValue *outValue, TZrBool *outSupported, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 递归执行正式调试求值支持的 AST 子集，消费仍有效的规范引用事实，并用返回值与支持位分开表达硬执行错误和未覆盖的节点或边界。
 *
 * ## Parameters
 * @param[in] agent 借用代理及 VM；调用者负责当前帧稳定性与效果授权。
 * @param[in] frameId 本次递归使用的调试帧号，所有引用读取须对应同一准备状态。
 * @param[in] semanticContext 借用仍存活的编译语义上下文；使用引用事实的节点要求匹配的上下文。
 * @param[in] node 借用当前 AST 节点及所有子节点；NULL 时未处理。
 * @param[out] outValue 受支持并成功时交付值；失败或未支持时不得当作完整结果，递归期间可能已改写。
 * @param[out] outSupported 必需输出，入口设 TRUE；未覆盖节点或可回退边界置 FALSE。
 * @param[out] errorBuffer 与容量配套的错误输出；不是所有未支持分支都写错误文本。
 * @param[in] errorBufferSize 错误输出的可写字节容量。
 *
 * ## Returns
 * @return FALSE 表示类型、引用或运算等硬错误；TRUE 且支持位 FALSE 表示该节点未由正式执行器交付结果；TRUE 且支持位 TRUE 表示已处理。无效请求尽可能置支持位 FALSE 并返回 TRUE。
 *
 * ## Constraints and lifecycle
 * 不自行解析源码、建立 canonical facts 或授予 capability。标识符只读规范来源；primary 仅支持逐级 ARRAY 的 computed 索引。条件只执行所选分支，逻辑短路只跳右侧运行读取；整 AST 副作用分类必须已在上层完成。数值比较/混合运算经 double 视图，不承诺大整数无损；整数 +、-、* 先预检，部分除/模和未知运算边界回传未支持。位移限制左值非负、位数在 0..62，既有左移块尚未证明结果范围或类型推断前置拒绝，不宣称溢出已覆盖。递归结果是 VM 值记录，不在此处注册 handle，也没有独立全局异常回滚。
 *
 * ## References
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:441（实际调用或注册）；正式门禁后的主要消费者。
 * - zr_debug_formal_evaluate_array_literal — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:388（配套消费位置）；数组元素递归和构造。
 * - zr_debug_formal_read_reference_value — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:410（配套消费位置）；规范引用来源读取。
 * - zr_debug_formal_integer_add_overflows — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:786（配套消费位置）；整数加法范围预检。
 * - zr_debug_formal_integer_subtract_overflows — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:789（配套消费位置）；整数减法范围预检。
 * - zr_debug_formal_integer_multiply_overflows — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:792（配套消费位置）；整数乘法范围预检。
 */
