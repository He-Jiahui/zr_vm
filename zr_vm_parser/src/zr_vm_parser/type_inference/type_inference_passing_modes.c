//
// Created by Auto on 2026/04/02.
//

#include "type_inference_internal.h"
#include "zr_vm_parser/place.h"

static EZrParameterPassingMode get_parameter_passing_mode_at(const SZrArray *parameterPassingModes, TZrSize index) {
    EZrParameterPassingMode *mode;

    if (parameterPassingModes == ZR_NULL || index >= parameterPassingModes->length) {
        return ZR_PARAMETER_PASSING_MODE_VALUE;
    }

    mode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, index);
    return mode != ZR_NULL ? *mode : ZR_PARAMETER_PASSING_MODE_VALUE;
}

static const TZrChar *parameter_passing_mode_label(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN:
            return "in";
        case ZR_PARAMETER_PASSING_MODE_OUT:
            return "out";
        case ZR_PARAMETER_PASSING_MODE_REF:
            return "ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return "value";
    }
}

static EZrCallArgumentMarker call_argument_marker_at(
        const SZrFunctionCall *call,
        TZrSize index) {
    const SZrCallArgumentSyntax *syntax;

    if (call == ZR_NULL || call->argumentMarkers == ZR_NULL ||
        index >= call->argumentMarkers->length) {
        return ZR_CALL_ARGUMENT_MARKER_NONE;
    }
    syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
            call->argumentMarkers, index);
    return syntax != ZR_NULL ? syntax->marker : ZR_CALL_ARGUMENT_MARKER_NONE;
}

TZrBool type_inference_reference_argument_type_equal(
        const SZrInferredType *argumentType,
        const SZrInferredType *parameterType) {
    SZrInferredType argumentValueType;
    SZrInferredType parameterValueType;

    if (argumentType == ZR_NULL || parameterType == ZR_NULL) {
        return ZR_FALSE;
    }

    argumentValueType = *argumentType;
    parameterValueType = *parameterType;
    argumentValueType.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    parameterValueType.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    return ZrParser_InferredType_Equal(
            &argumentValueType, &parameterValueType);
}

static SZrAstNodeArray *function_parameter_list(
        const SZrFunctionTypeInfo *functionType) {
    SZrAstNode *declaration =
            functionType != ZR_NULL ? functionType->declarationNode : ZR_NULL;
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    if (declaration->type == ZR_AST_FUNCTION_DECLARATION) {
        return declaration->data.functionDeclaration.params;
    }
    if (declaration->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
        return declaration->data.externFunctionDeclaration.params;
    }
    return ZR_NULL;
}

static TZrSize call_argument_index_for_parameter(
        const SZrFunctionCall *call,
        const SZrAstNodeArray *parameters,
        TZrSize parameterIndex) {
    SZrString *parameterName;

    if (call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }
    if (!call->hasNamedArgs || call->argNames == ZR_NULL ||
        parameters == ZR_NULL || parameterIndex >= parameters->count) {
        return parameterIndex < call->args->count
                       ? parameterIndex
                       : ZR_PARSER_INDEX_NONE;
    }
    if (parameters->nodes[parameterIndex] == ZR_NULL ||
        parameters->nodes[parameterIndex]->type != ZR_AST_PARAMETER ||
        parameters->nodes[parameterIndex]->data.parameter.name == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }
    parameterName =
            parameters->nodes[parameterIndex]->data.parameter.name->name;
    for (TZrSize index = 0u; index < call->args->count; index++) {
        SZrString **argumentName = index < call->argNames->length
                ? (SZrString **)ZrCore_Array_Get(call->argNames, index)
                : ZR_NULL;
        if (argumentName == ZR_NULL || *argumentName == ZR_NULL) {
            if (index == parameterIndex) {
                return index;
            }
            continue;
        }
        if (parameterName != ZR_NULL &&
            ZrCore_String_Equal(parameterName, *argumentName)) {
            return index;
        }
    }
    return ZR_PARSER_INDEX_NONE;
}

static TZrBool validate_call_argument_marker(
        SZrCompilerState *cs,
        EZrParameterPassingMode passingMode,
        EZrCallArgumentMarker marker,
        SZrFileRange location) {
    const TZrChar *requiredMarker = ZR_NULL;

    if (passingMode == ZR_PARAMETER_PASSING_MODE_OUT) {
        requiredMarker = "out";
        if (marker == ZR_CALL_ARGUMENT_MARKER_OUT) {
            return ZR_TRUE;
        }
    } else if (passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
        requiredMarker = "ref";
        if (marker == ZR_CALL_ARGUMENT_MARKER_REF) {
            return ZR_TRUE;
        }
    } else {
        if (marker == ZR_CALL_ARGUMENT_MARKER_NONE) {
            return ZR_TRUE;
        }
        ZrParser_Compiler_Error(
                cs, "Value and in parameters do not accept an argument marker", location);
        return ZR_FALSE;
    }

    {
        TZrChar errorBuffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];
        snprintf(errorBuffer,
                 sizeof(errorBuffer),
                 "%s parameter requires the '%s' argument marker",
                 parameter_passing_mode_label(passingMode),
                 requiredMarker);
        ZrParser_Compiler_Error(cs, errorBuffer, location);
    }
    return ZR_FALSE;
}

TZrBool validate_call_argument_passing_modes(SZrCompilerState *cs,
                                             const SZrArray *parameterPassingModes,
                                             const SZrArray *parameterTypes,
                                             SZrFunctionCall *call,
                                             const SZrArray *argTypes,
                                             const SZrFunctionTypeInfo *functionType) {
    SZrAstNodeArray *parameters = function_parameter_list(functionType);
    if (cs == ZR_NULL || parameterTypes == ZR_NULL || call == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < parameterTypes->length && index < argTypes->length; index++) {
        EZrParameterPassingMode passingMode = get_parameter_passing_mode_at(parameterPassingModes, index);
        SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argTypes, index);
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrAstNode *argNode;
        TZrSize argumentIndex;
        TZrChar errorBuffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];

        argumentIndex = call_argument_index_for_parameter(
                call, parameters, index);
        if (argumentIndex == ZR_PARSER_INDEX_NONE) {
            continue;
        }

        argNode = call->args->nodes[argumentIndex];
        if (argNode == ZR_NULL || argType == ZR_NULL || paramType == ZR_NULL) {
            return ZR_FALSE;
        }

        if ((passingMode == ZR_PARAMETER_PASSING_MODE_OUT ||
             passingMode == ZR_PARAMETER_PASSING_MODE_REF) &&
            ZrParser_PlaceExpression_Classify(argNode) ==
                    ZR_PARSER_PLACE_EXPRESSION_INVALID) {
            snprintf(errorBuffer,
                     sizeof(errorBuffer),
                     "%s argument must be an assignable storage location (writable Place)",
                     parameter_passing_mode_label(passingMode));
            ZrParser_Compiler_Error(cs, errorBuffer, argNode->location);
            return ZR_FALSE;
        }

        if (!validate_call_argument_marker(
                    cs,
                    passingMode,
                    call_argument_marker_at(call, argumentIndex),
                    argNode->location)) {
            return ZR_FALSE;
        }

        if ((passingMode == ZR_PARAMETER_PASSING_MODE_OUT ||
             passingMode == ZR_PARAMETER_PASSING_MODE_REF) &&
            argType->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) {
            snprintf(errorBuffer,
                     sizeof(errorBuffer),
                     "%s argument must be a writable Place",
                     parameter_passing_mode_label(passingMode));
            ZrParser_Compiler_Error(cs, errorBuffer, argNode->location);
            return ZR_FALSE;
        }

        if ((passingMode == ZR_PARAMETER_PASSING_MODE_OUT ||
             passingMode == ZR_PARAMETER_PASSING_MODE_REF) &&
            !type_inference_reference_argument_type_equal(argType, paramType)) {
            snprintf(errorBuffer,
                     sizeof(errorBuffer),
                     "%s argument type mismatch",
                     parameter_passing_mode_label(passingMode));
            ZrParser_TypeError_Report(cs, errorBuffer, paramType, argType, argNode->location);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}
