#include "type_inference_call_diagnostics.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

static SZrAstNodeArray *call_diagnostic_parameter_list(
        const SZrFunctionTypeInfo *funcType) {
    SZrAstNode *declaration = funcType != ZR_NULL
                                      ? funcType->declarationNode
                                      : ZR_NULL;

    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.params;
        default:
            return ZR_NULL;
    }
}

static SZrAstNode *call_diagnostic_argument_for_parameter(
        const SZrFunctionCall *call,
        const SZrAstNodeArray *parameters,
        TZrSize parameterIndex) {
    TZrSize argumentCount;
    TZrSize positionalCount = 0U;

    if (call == ZR_NULL || call->args == ZR_NULL || parameters == ZR_NULL ||
        parameterIndex >= parameters->count) {
        return ZR_NULL;
    }
    argumentCount = call->args->count;
    if (!call->hasNamedArgs || call->argNames == ZR_NULL) {
        return parameterIndex < argumentCount
                       ? call->args->nodes[parameterIndex]
                       : ZR_NULL;
    }

    while (positionalCount < argumentCount &&
           positionalCount < call->argNames->length) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get(
                call->argNames,
                positionalCount);
        if (namePtr == ZR_NULL || *namePtr != ZR_NULL) {
            break;
        }
        positionalCount++;
    }
    if (parameterIndex < positionalCount) {
        return call->args->nodes[parameterIndex];
    }

    if (parameters->nodes[parameterIndex] != ZR_NULL &&
        parameters->nodes[parameterIndex]->type == ZR_AST_PARAMETER &&
        parameters->nodes[parameterIndex]->data.parameter.name != ZR_NULL) {
        SZrString *parameterName =
                parameters->nodes[parameterIndex]->data.parameter.name->name;

        for (TZrSize argumentIndex = positionalCount;
             argumentIndex < argumentCount &&
             argumentIndex < call->argNames->length;
             argumentIndex++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(
                    call->argNames,
                    argumentIndex);
            if (namePtr != ZR_NULL && *namePtr != ZR_NULL &&
                parameterName != ZR_NULL &&
                ZrCore_String_Equal(*namePtr, parameterName)) {
                return call->args->nodes[argumentIndex];
            }
        }
    }
    return ZR_NULL;
}

TZrBool type_inference_call_diagnostic_report_argument_mismatch(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *funcType,
        const SZrResolvedCallSignature *resolvedSignature,
        const SZrFunctionCall *call,
        TZrSize parameterIndex,
        const SZrInferredType *argumentType) {
    SZrAstNodeArray *parameters = call_diagnostic_parameter_list(funcType);
    const SZrInferredType *parameterType;
    SZrAstNode *argumentNode;
    SZrAstNode *parameterNode;
    SZrFileRange expectedTypeLocation;
    const SZrFileRange *expectedTypeLocationPtr = ZR_NULL;

    if (cs == ZR_NULL || resolvedSignature == ZR_NULL ||
        argumentType == ZR_NULL || parameters == ZR_NULL ||
        parameterIndex >= parameters->count ||
        parameterIndex >= resolvedSignature->parameterTypes.length) {
        return ZR_FALSE;
    }
    parameterType = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterTypes,
            parameterIndex);
    argumentNode = call_diagnostic_argument_for_parameter(
            call,
            parameters,
            parameterIndex);
    parameterNode = parameters->nodes[parameterIndex];
    if (parameterType == ZR_NULL || argumentNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (parameterNode != ZR_NULL &&
        parameterNode->type == ZR_AST_PARAMETER &&
        parameterNode->data.parameter.typeInfo != ZR_NULL &&
        parameterNode->data.parameter.typeInfo->name != ZR_NULL) {
        expectedTypeLocation = parameterNode->data.parameter.typeInfo->name->location;
        expectedTypeLocationPtr = &expectedTypeLocation;
    }
    ZrParser_TypeError_ReportDetailed(
            cs,
            "Argument type mismatch",
            parameterType,
            argumentType,
            argumentNode->location,
            expectedTypeLocationPtr);
    return ZR_TRUE;
}
