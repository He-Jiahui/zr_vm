//
// Created by Auto on 2026/04/02.
//

#include "type_inference_internal.h"

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
            return "%in";
        case ZR_PARAMETER_PASSING_MODE_OUT:
            return "%out";
        case ZR_PARAMETER_PASSING_MODE_REF:
            return "%ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return "value";
    }
}

static TZrBool expression_is_assignable_storage(SZrAstNode *node) {
    SZrPrimaryExpression *primaryExpr;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }

    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primaryExpr = &node->data.primaryExpression;
    if (primaryExpr->property == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!expression_is_assignable_storage(primaryExpr->property) &&
        primaryExpr->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    if (primaryExpr->members == ZR_NULL || primaryExpr->members->count == 0) {
        return primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL;
    }

    for (TZrSize index = 0; index < primaryExpr->members->count; index++) {
        SZrAstNode *memberNode = primaryExpr->members->nodes[index];
        if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool validate_call_argument_passing_modes(SZrCompilerState *cs,
                                             const SZrArray *parameterPassingModes,
                                             const SZrArray *parameterTypes,
                                             SZrFunctionCall *call,
                                             const SZrArray *argTypes) {
    if (cs == ZR_NULL || parameterTypes == ZR_NULL || call == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < parameterTypes->length && index < argTypes->length; index++) {
        EZrParameterPassingMode passingMode = get_parameter_passing_mode_at(parameterPassingModes, index);
        SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argTypes, index);
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrAstNode *argNode;
        TZrChar errorBuffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];

        if (passingMode != ZR_PARAMETER_PASSING_MODE_OUT && passingMode != ZR_PARAMETER_PASSING_MODE_REF) {
            continue;
        }

        if (call->args == ZR_NULL || index >= call->args->count) {
            return ZR_FALSE;
        }

        argNode = call->args->nodes[index];
        if (argNode == ZR_NULL || argType == ZR_NULL || paramType == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!expression_is_assignable_storage(argNode)) {
            snprintf(errorBuffer,
                     sizeof(errorBuffer),
                     "%s argument must be an assignable storage location",
                     parameter_passing_mode_label(passingMode));
            ZrParser_Compiler_Error(cs, errorBuffer, argNode->location);
            return ZR_FALSE;
        }

        if (!ZrParser_InferredType_Equal(argType, paramType)) {
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
