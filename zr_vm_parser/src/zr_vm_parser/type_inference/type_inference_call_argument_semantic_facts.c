#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

static SZrString *call_argument_parameter_name(
        const SZrAstNodeArray *parameters,
        const SZrArray *parameterNames,
        TZrSize parameterIndex) {
    if (parameters != ZR_NULL && parameterIndex < parameters->count) {
        const SZrAstNode *parameter = parameters->nodes[parameterIndex];

        if (parameter != ZR_NULL && parameter->type == ZR_AST_PARAMETER &&
            parameter->data.parameter.name != ZR_NULL) {
            return parameter->data.parameter.name->name;
        }
    }
    if (parameterNames != ZR_NULL && parameterNames->isValid &&
        parameterIndex < parameterNames->length) {
        SZrString **name = (SZrString **)ZrCore_Array_Get(
                (SZrArray *)parameterNames, parameterIndex);
        return name != ZR_NULL ? *name : ZR_NULL;
    }
    return ZR_NULL;
}

static SZrString *call_argument_name(
        const SZrFunctionCall *call,
        TZrSize argumentIndex) {
    SZrString **name;

    if (call == ZR_NULL || !call->hasNamedArgs || call->argNames == ZR_NULL ||
        !call->argNames->isValid || argumentIndex >= call->argNames->length) {
        return ZR_NULL;
    }
    name = (SZrString **)ZrCore_Array_Get(call->argNames, argumentIndex);
    return name != ZR_NULL ? *name : ZR_NULL;
}

static TZrBool call_argument_parameter_index(
        const SZrFunctionCall *call,
        const SZrAstNodeArray *parameters,
        const SZrArray *parameterNames,
        TZrSize parameterCount,
        TZrSize argumentIndex,
        TZrSize *outParameterIndex,
        TZrBool *outIsNamed) {
    SZrString *argumentName = call_argument_name(call, argumentIndex);

    if (outParameterIndex == ZR_NULL || outIsNamed == ZR_NULL) {
        return ZR_FALSE;
    }
    *outIsNamed = argumentName != ZR_NULL;
    if (argumentName == ZR_NULL) {
        if (argumentIndex >= parameterCount) {
            return ZR_FALSE;
        }
        *outParameterIndex = argumentIndex;
        return ZR_TRUE;
    }
    for (TZrSize parameterIndex = 0U;
         parameterIndex < parameterCount;
         parameterIndex++) {
        SZrString *parameterName = call_argument_parameter_name(
                parameters, parameterNames, parameterIndex);
        if (parameterName != ZR_NULL &&
            ZrCore_String_Equal(parameterName, argumentName)) {
            *outParameterIndex = parameterIndex;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static EZrParameterPassingMode call_argument_passing_mode(
        const SZrResolvedCallSignature *resolvedSignature,
        TZrSize parameterIndex) {
    const EZrParameterPassingMode *mode;

    if (resolvedSignature == ZR_NULL ||
        !resolvedSignature->parameterPassingModes.isValid ||
        parameterIndex >= resolvedSignature->parameterPassingModes.length) {
        return ZR_PARAMETER_PASSING_MODE_VALUE;
    }
    mode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterPassingModes,
            parameterIndex);
    return mode != ZR_NULL ? *mode : ZR_PARAMETER_PASSING_MODE_VALUE;
}

static void call_argument_types(
        SZrCompilerState *cs,
        const SZrAstNode *argument,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrSize parameterIndex,
        TZrBool validateCompatibility,
        SZrSemanticCallArgumentFact *mapping) {
    const SZrSemanticExpressionFact *argumentFact;
    const SZrInferredType *argumentType;
    const SZrInferredType *parameterType;
    SZrInferredType inferredArgument;
    TZrBool ownsInferredArgument = ZR_FALSE;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || argument == ZR_NULL ||
        resolvedSignature == ZR_NULL || mapping == ZR_NULL ||
        parameterIndex >= resolvedSignature->parameterTypes.length) {
        return;
    }
    argumentFact = ZrParser_SemanticFacts_FindExpressionByNode(
            cs->semanticContext, argument);
    parameterType = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterTypes, parameterIndex);
    if (parameterType == ZR_NULL) {
        return;
    }
    if (argumentFact != ZR_NULL &&
        argumentFact->exactness == ZR_SEMANTIC_FACT_EXACT &&
        argumentFact->typeId != ZR_SEMANTIC_ID_INVALID) {
        argumentType = &argumentFact->inferredType;
        mapping->argumentTypeId = argumentFact->typeId;
    } else {
        ZrParser_InferredType_Init(
                cs->state, &inferredArgument, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(
                    cs, (SZrAstNode *)argument, &inferredArgument)) {
            ZrParser_InferredType_Free(cs->state, &inferredArgument);
            return;
        }
        argumentType = &inferredArgument;
        ownsInferredArgument = ZR_TRUE;
        mapping->argumentTypeId = ZrParser_CanonicalType_FromInferred(
                cs->semanticContext, argumentType);
    }
    mapping->parameterTypeId = ZrParser_CanonicalType_FromInferred(
            cs->semanticContext, parameterType);
    if (mapping->argumentTypeId == ZR_SEMANTIC_ID_INVALID ||
        mapping->parameterTypeId == ZR_SEMANTIC_ID_INVALID ||
        (validateCompatibility &&
         !ZrParser_TypeCompatibility_Check(
                 cs, argumentType, parameterType, argument->location))) {
        mapping->argumentTypeId = ZR_SEMANTIC_ID_INVALID;
        mapping->parameterTypeId = ZR_SEMANTIC_ID_INVALID;
        if (ownsInferredArgument) {
            ZrParser_InferredType_Free(cs->state, &inferredArgument);
        }
        return;
    }
    mapping->conversion = mapping->argumentTypeId == mapping->parameterTypeId
                                  ? ZR_SEMANTIC_CALL_CONVERSION_EXACT
                                  : ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT;
    if (ownsInferredArgument) {
        ZrParser_InferredType_Free(cs->state, &inferredArgument);
    }
}

TZrBool type_inference_call_argument_facts_build(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrAstNodeArray *parameters,
        const SZrArray *parameterNames,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrBool validateCompatibility,
        SZrArray *outMappings) {
    TZrSize argumentCount;
    TZrSize parameterCount;

    if (outMappings == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(outMappings);
    if (cs == ZR_NULL || cs->state == ZR_NULL || call == ZR_NULL ||
        resolvedSignature == ZR_NULL) {
        return ZR_FALSE;
    }
    argumentCount = call->args != ZR_NULL ? call->args->count : 0U;
    parameterCount = resolvedSignature->parameterTypes.length;
    if (argumentCount == 0U) {
        return ZR_TRUE;
    }
    for (TZrSize argumentIndex = 0U;
         argumentIndex < argumentCount;
         argumentIndex++) {
        const SZrAstNode *argument = call->args->nodes[argumentIndex];
        if (argument != ZR_NULL && argument->type == ZR_AST_SPREAD_ARGUMENT) {
            return ZR_TRUE;
        }
    }
    ZrCore_Array_Init(cs->state,
                      outMappings,
                      sizeof(SZrSemanticCallArgumentFact),
                      argumentCount);
    for (TZrSize argumentIndex = 0U;
         argumentIndex < argumentCount;
         argumentIndex++) {
        SZrAstNode *argument = call->args->nodes[argumentIndex];
        SZrSemanticCallArgumentFact mapping;

        memset(&mapping, 0, sizeof(mapping));
        mapping.argumentIndex = argumentIndex;
        mapping.argumentRange = argument != ZR_NULL
                                        ? argument->location
                                        : (SZrFileRange){0};
        mapping.argumentTypeId = ZR_SEMANTIC_ID_INVALID;
        mapping.parameterTypeId = ZR_SEMANTIC_ID_INVALID;
        if (argument == ZR_NULL ||
            !call_argument_parameter_index(call,
                                           parameters,
                                           parameterNames,
                                           parameterCount,
                                           argumentIndex,
                                           &mapping.parameterIndex,
                                           &mapping.isNamed)) {
            ZrCore_Array_Free(cs->state, outMappings);
            ZrCore_Array_Construct(outMappings);
            return ZR_FALSE;
        }
        mapping.passingMode = call_argument_passing_mode(
                resolvedSignature, mapping.parameterIndex);
        call_argument_types(cs,
                            argument,
                            resolvedSignature,
                            mapping.parameterIndex,
                            validateCompatibility,
                            &mapping);
        if (mapping.argumentTypeId == ZR_SEMANTIC_ID_INVALID ||
            mapping.parameterTypeId == ZR_SEMANTIC_ID_INVALID ||
            mapping.conversion == ZR_SEMANTIC_CALL_CONVERSION_UNKNOWN) {
            ZrCore_Array_Free(cs->state, outMappings);
            ZrCore_Array_Construct(outMappings);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, outMappings, &mapping);
    }
    return outMappings->length == argumentCount;
}
