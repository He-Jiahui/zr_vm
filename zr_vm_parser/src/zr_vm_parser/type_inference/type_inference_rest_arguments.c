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

/* Match the tail without changing the fixed signature seen by overload resolution.
 * The call retains the original evaluation order; the VM packs the evaluated values. */
TZrBool type_inference_rest_arguments_match(SZrCompilerState *cs,
                                           const SZrFunctionCall *call,
                                           const SZrParameter *rest,
                                           TZrSize fixedCount,
                                           TZrSize positionalCount,
                                           const SZrInferredType *spreadType) {
    SZrInferredType restType;
    const SZrInferredType *expected;
    TZrBool result = ZR_FALSE;

    if (positionalCount <= fixedCount && spreadType == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrParser_AstTypeToInferredType_Convert(cs, rest->typeInfo, &restType)) {
        return ZR_FALSE;
    }
    expected = rest_element_type(&restType);
    if (expected == ZR_NULL) {
        goto cleanup;
    }
    for (TZrSize index = fixedCount; index < positionalCount; ++index) {
        SZrInferredType actual;
        TZrBool matches;
        const SZrCallArgumentSyntax *syntax = call->argumentMarkers != ZR_NULL
                ? (const SZrCallArgumentSyntax *)ZrCore_Array_Get(call->argumentMarkers, index) : ZR_NULL;
        if (syntax != ZR_NULL && syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
            goto cleanup;
        }
        ZrParser_InferredType_Init(cs->state, &actual, ZR_VALUE_TYPE_OBJECT);
        matches = ZrParser_ExpressionType_Infer(cs, call->args->nodes[index], &actual) &&
                  rest_element_matches(&actual, expected);
        ZrParser_InferredType_Free(cs->state, &actual);
        if (!matches) {
            goto cleanup;
        }
    }
    if (spreadType != ZR_NULL) {
        const SZrAstNode *expression = call->args->nodes[positionalCount]->data.spreadArgument.expression;
        TZrSize remainingFixed = fixedCount > positionalCount ? fixedCount - positionalCount : 0u;
        if (expression->type == ZR_AST_ARRAY_LITERAL) {
            const SZrAstNodeArray *items = expression->data.arrayLiteral.elements;
            if ((items == ZR_NULL ? 0u : items->count) <= remainingFixed) {
                result = ZR_TRUE;
                goto cleanup;
            }
        }
        if (!rest_element_matches(rest_element_type(spreadType), expected)) {
            goto cleanup;
        }
    }
    result = ZR_TRUE;

cleanup:
    ZrParser_InferredType_Free(cs->state, &restType);
    return result;
}
