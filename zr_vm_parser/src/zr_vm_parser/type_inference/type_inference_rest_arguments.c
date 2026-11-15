#include "type_inference/type_inference_internal.h"

const SZrParameter *type_inference_function_rest_parameter(const SZrFunctionTypeInfo *function) {
    return function != ZR_NULL && function->declarationNode != ZR_NULL &&
           function->declarationNode->type == ZR_AST_FUNCTION_DECLARATION
            ? function->declarationNode->data.functionDeclaration.args : ZR_NULL;
}

static const SZrInferredType *rest_element_type(const SZrInferredType *array) {
    return array->baseType == ZR_VALUE_TYPE_ARRAY && array->elementTypes.length == 1u
            ? (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&array->elementTypes, 0u)
            : ZR_NULL;
}

static TZrBool rest_element_matches(const SZrInferredType *actual, const SZrInferredType *expected) {
    return actual != ZR_NULL && expected != ZR_NULL &&
           ZrParser_InferredType_IsCompatible(actual, expected) &&
           ZrParser_InferredType_GetConversionOpcode(actual, expected) == ZR_INSTRUCTION_ENUM(ENUM_MAX);
}

/* Collect the actual tail separately from the fixed signature. Generic binding
 * must run before compatibility checking, without evaluating any expression. */
TZrBool type_inference_rest_argument_types(SZrCompilerState *cs,
                                          const SZrFunctionCall *call,
                                          TZrSize fixedCount,
                                          TZrSize positionalCount,
                                          const SZrInferredType *spreadType,
                                          SZrArray *argumentTypes) {
    if (argumentTypes == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(argumentTypes);
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (positionalCount <= fixedCount && spreadType == ZR_NULL) {
        return ZR_TRUE;
    }
    if (call == ZR_NULL || call->args == ZR_NULL || positionalCount > call->args->count) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(cs->state, argumentTypes, sizeof(SZrInferredType),
                      positionalCount > fixedCount ? positionalCount - fixedCount : 1u);
    for (TZrSize index = fixedCount; index < positionalCount; ++index) {
        SZrInferredType actual;
        const SZrCallArgumentSyntax *syntax = call->argumentMarkers != ZR_NULL
                ? (const SZrCallArgumentSyntax *)ZrCore_Array_Get(call->argumentMarkers, index) : ZR_NULL;
        if (syntax != ZR_NULL && syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
            goto error;
        }
        ZrParser_InferredType_Init(cs->state, &actual, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, call->args->nodes[index], &actual)) {
            ZrParser_InferredType_Free(cs->state, &actual);
            goto error;
        }
        ZrCore_Array_Push(cs->state, argumentTypes, &actual);
    }
    if (spreadType != ZR_NULL) {
        const SZrAstNode *spreadNode = positionalCount < call->args->count
                ? call->args->nodes[positionalCount] : ZR_NULL;
        const SZrAstNode *expression;
        const SZrInferredType *element;
        SZrInferredType copiedType;
        TZrSize remainingFixed = fixedCount > positionalCount ? fixedCount - positionalCount : 0u;
        if (spreadNode == ZR_NULL || spreadNode->type != ZR_AST_SPREAD_ARGUMENT ||
            spreadNode->data.spreadArgument.expression == ZR_NULL) {
            goto error;
        }
        expression = spreadNode->data.spreadArgument.expression;
        if (expression->type == ZR_AST_ARRAY_LITERAL) {
            const SZrAstNodeArray *items = expression->data.arrayLiteral.elements;
            if ((items == ZR_NULL ? 0u : items->count) <= remainingFixed) {
                return ZR_TRUE;
            }
        } else if (spreadType->hasArraySizeConstraint && spreadType->arrayFixedSize > 0u &&
                   spreadType->arrayFixedSize <= remainingFixed) {
            /* A sized array consumed entirely by fixed parameters has no rest
             * element. Zero is also used for open extents, so it is not proof. */
            return ZR_TRUE;
        }
        element = rest_element_type(spreadType);
        if (element == ZR_NULL) {
            goto error;
        }
        ZrParser_InferredType_Copy(cs->state, &copiedType, element);
        ZrCore_Array_Push(cs->state, argumentTypes, &copiedType);
    }
    return ZR_TRUE;

error:
    free_inferred_type_array(cs->state, argumentTypes);
    ZrCore_Array_Construct(argumentTypes);
    return ZR_FALSE;
}

TZrBool type_inference_rest_argument_types_match(const SZrArray *argumentTypes,
                                                const SZrInferredType *elementType) {
    if (argumentTypes == ZR_NULL || elementType == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < argumentTypes->length; ++index) {
        const SZrInferredType *actual = ZrCore_Array_Get((SZrArray *)argumentTypes, index);
        if (!rest_element_matches(actual, elementType)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* Match the tail without changing the fixed signature seen by overload resolution.
 * The call retains the original evaluation order; the VM packs the evaluated values. */
TZrBool type_inference_rest_arguments_match(SZrCompilerState *cs,
                                           const SZrFunctionCall *call,
                                           const SZrParameter *rest,
                                           TZrSize fixedCount,
                                           TZrSize positionalCount,
                                           const SZrInferredType *spreadType) {
    SZrArray argumentTypes;
    SZrInferredType restType;
    TZrBool result;

    if (cs == ZR_NULL || rest == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!type_inference_rest_argument_types(cs, call, fixedCount, positionalCount,
                                            spreadType, &argumentTypes)) {
        return ZR_FALSE;
    }
    if (argumentTypes.length == 0u) {
        free_inferred_type_array(cs->state, &argumentTypes);
        return ZR_TRUE;
    }
    ZrParser_InferredType_Init(cs->state, &restType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs, rest->typeInfo, &restType)) {
        ZrParser_InferredType_Free(cs->state, &restType);
        free_inferred_type_array(cs->state, &argumentTypes);
        return ZR_FALSE;
    }
    result = type_inference_rest_argument_types_match(&argumentTypes, rest_element_type(&restType));
    ZrParser_InferredType_Free(cs->state, &restType);
    free_inferred_type_array(cs->state, &argumentTypes);
    return result;
}
