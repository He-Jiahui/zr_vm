#include "compile_expression_internal.h"

TZrBool compiler_call_has_spread_argument(const SZrFunctionCall *call) {
    if (call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < call->args->count; index++) {
        const SZrAstNode *argument = call->args->nodes[index];
        if (argument != ZR_NULL && argument->type == ZR_AST_SPREAD_ARGUMENT) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool compiler_validate_trailing_spread_call(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        SZrFileRange location) {
    TZrSize spreadCount = 0u;

    if (cs == ZR_NULL || call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_FALSE;
    }
    if (call->argNames != ZR_NULL) {
        for (TZrSize index = 0u; index < call->argNames->length; index++) {
            SZrString *const *name = (SZrString *const *)ZrCore_Array_Get(
                    call->argNames, index);
            if (name != ZR_NULL && *name != ZR_NULL) {
                ZrParser_Compiler_Error(
                        cs,
                        "Spread arguments cannot be combined with named arguments",
                        location);
                return ZR_FALSE;
            }
        }
    }
    if (call->argumentMarkers != ZR_NULL) {
        for (TZrSize index = 0u;
             index < call->argumentMarkers->length;
             index++) {
            const SZrCallArgumentSyntax *syntax =
                    (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
                            call->argumentMarkers, index);
            if (syntax != ZR_NULL &&
                syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
                ZrParser_Compiler_Error(
                        cs,
                        "Spread arguments cannot be combined with ref or out arguments",
                        location);
                return ZR_FALSE;
            }
        }
    }

    for (TZrSize index = 0u; index < call->args->count; index++) {
        const SZrAstNode *argument = call->args->nodes[index];
        if (argument == ZR_NULL || argument->type != ZR_AST_SPREAD_ARGUMENT) {
            continue;
        }
        spreadCount++;
        if (index + 1u != call->args->count) {
            ZrParser_Compiler_Error(
                    cs,
                    "The spread argument must be the final call argument",
                    argument->location);
            return ZR_FALSE;
        }
    }

    if (spreadCount != 1u) {
        ZrParser_Compiler_Error(
                cs,
                "A call may contain exactly one spread argument",
                location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool compiler_validate_spread_call_signature(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        SZrFileRange location) {
    SZrAstNode *spreadNode;
    SZrAstNode *spreadExpression;
    SZrInferredType spreadType;
    const SZrInferredType *elementType;
    TZrSize prefixCount;
    TZrSize parameterCount;
    TZrSize knownSpreadCount = 0u;
    TZrBool hasKnownSpreadCount = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || call == ZR_NULL || call->args == ZR_NULL ||
        call->args->count == 0u || parameterTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixCount = call->args->count - 1u;
    parameterCount = parameterTypes->length;
    spreadNode = call->args->nodes[prefixCount];
    spreadExpression =
            spreadNode != ZR_NULL && spreadNode->type == ZR_AST_SPREAD_ARGUMENT
                    ? spreadNode->data.spreadArgument.expression
                    : ZR_NULL;
    if (spreadExpression == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "Spread argument requires an expression", location);
        return ZR_FALSE;
    }
    if (prefixCount > parameterCount) {
        ZrParser_Compiler_Error(
                cs, "Spread call has too many fixed arguments", location);
        return ZR_FALSE;
    }

    if (!compiler_validate_call_argument_passing_contract(
                cs, parameterPassingModes, ZR_NULL, ZR_NULL, call)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(
            cs->state, &spreadType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, spreadExpression, &spreadType)) {
        ZrParser_InferredType_Free(cs->state, &spreadType);
        return ZR_FALSE;
    }
    if (spreadType.baseType != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_Compiler_Error(
                cs, "Spread argument must have an array type", location);
        goto cleanup;
    }

    if (spreadExpression->type == ZR_AST_ARRAY_LITERAL) {
        knownSpreadCount =
                spreadExpression->data.arrayLiteral.elements != ZR_NULL
                        ? spreadExpression->data.arrayLiteral.elements->count
                        : 0u;
        hasKnownSpreadCount = ZR_TRUE;
    } else if (spreadType.hasArraySizeConstraint) {
        knownSpreadCount = spreadType.arrayFixedSize;
        hasKnownSpreadCount = ZR_TRUE;
    }
    if (hasKnownSpreadCount &&
        prefixCount + knownSpreadCount != parameterCount) {
        ZrParser_Compiler_Error(
                cs,
                "Spread argument count does not match the callable signature",
                location);
        goto cleanup;
    }

    elementType = spreadType.elementTypes.length == 1u
                          ? (const SZrInferredType *)ZrCore_Array_Get(
                                    &spreadType.elementTypes, 0u)
                          : ZR_NULL;
    for (TZrSize index = prefixCount; index < parameterCount; index++) {
        const SZrInferredType *expectedType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)parameterTypes, index);
        EZrParameterPassingMode mode = ZR_PARAMETER_PASSING_MODE_VALUE;

        if (parameterPassingModes != ZR_NULL &&
            index < parameterPassingModes->length) {
            const EZrParameterPassingMode *modePtr =
                    (const EZrParameterPassingMode *)ZrCore_Array_Get(
                            (SZrArray *)parameterPassingModes, index);
            if (modePtr != ZR_NULL) {
                mode = *modePtr;
            }
        }
        if (mode != ZR_PARAMETER_PASSING_MODE_VALUE) {
            ZrParser_Compiler_Error(
                    cs,
                    "Spread arguments only support value parameters",
                    location);
            goto cleanup;
        }
        if (elementType == ZR_NULL || expectedType == ZR_NULL ||
            !ZrParser_InferredType_IsCompatible(elementType, expectedType) ||
            ZrParser_InferredType_GetConversionOpcode(
                    elementType, expectedType) !=
                    ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Spread array elements must match parameter types without conversion",
                    location);
            goto cleanup;
        }
    }

    result = ZR_TRUE;

cleanup:
    ZrParser_InferredType_Free(cs->state, &spreadType);
    return result;
}
