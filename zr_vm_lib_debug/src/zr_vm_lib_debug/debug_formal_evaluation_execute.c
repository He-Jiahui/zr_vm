#include "debug_evaluation_effect_internal.h"

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
        if (!ZrCore_Object_SetByIndex(agent->state, &arrayValue, &indexValue, &elementValue)) {
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
