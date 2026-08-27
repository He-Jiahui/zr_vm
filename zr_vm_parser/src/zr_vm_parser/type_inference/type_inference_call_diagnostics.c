#include "type_inference_call_diagnostics.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic_facts.h"

typedef enum EZrCallOwnershipDiagnosticKind {
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE = 0,
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_MISMATCH,
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE,
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE,
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE,
    ZR_CALL_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN,
} EZrCallOwnershipDiagnosticKind;

static TZrBool call_diagnostic_ownership_surface_equal(
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    SZrInferredType parameterSurface;
    SZrInferredType argumentSurface;

    if (parameterType == ZR_NULL || argumentType == ZR_NULL) {
        return ZR_FALSE;
    }
    parameterSurface = *parameterType;
    argumentSurface = *argumentType;
    parameterSurface.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    argumentSurface.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    parameterSurface.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    argumentSurface.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    return ZrParser_InferredType_Equal(&parameterSurface, &argumentSurface);
}

static EZrCallOwnershipDiagnosticKind call_diagnostic_classify_ownership(
        EZrParameterPassingMode passingMode,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    if (!call_diagnostic_ownership_surface_equal(parameterType, argumentType) ||
        parameterType->ownershipQualifier == argumentType->ownershipQualifier) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE;
    }
    if (passingMode == ZR_PARAMETER_PASSING_MODE_IN &&
        parameterType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
        (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
         argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED)) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE;
    }
    if (parameterType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED &&
        (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
         argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED)) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE;
    }
    if (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK &&
        (parameterType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
         parameterType->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
         passingMode == ZR_PARAMETER_PASSING_MODE_REF)) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE;
    }
    if (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE;
    }
    if (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE;
    }
    if (parameterType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
        (argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
         argumentType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED)) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN;
    }
    if (parameterType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        argumentType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        return ZR_CALL_OWNERSHIP_DIAGNOSTIC_MISMATCH;
    }
    return ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE;
}

const TZrChar *type_inference_call_diagnostic_ownership_message(
        EZrParameterPassingMode passingMode,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    switch (call_diagnostic_classify_ownership(
            passingMode, parameterType, argumentType)) {
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE:
            return "Weak value must be woken before it can be borrowed";
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE:
            return "Borrowed value cannot escape its owner";
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE:
            return "Loaned value cannot escape its owner";
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN:
            return "Owned value cannot flow into a plain GC value implicitly";
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_MISMATCH:
            return "Ownership qualifier mismatch";
        case ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE:
        default:
            return ZR_NULL;
    }
}

static SZrAstNodeArray *call_diagnostic_declaration_parameter_list(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.params;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.params;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.params;
        default:
            return ZR_NULL;
    }
}

static SZrAstNodeArray *call_diagnostic_parameter_list(
        const SZrFunctionTypeInfo *funcType) {
    return call_diagnostic_declaration_parameter_list(
            funcType != ZR_NULL ? funcType->declarationNode : ZR_NULL);
}

SZrFileRange type_inference_call_diagnostic_argument_location(
        const SZrAstNode *argumentNode) {
    if (argumentNode != ZR_NULL &&
        argumentNode->type == ZR_AST_PRIMARY_EXPRESSION &&
        argumentNode->data.primaryExpression.property != ZR_NULL &&
        (argumentNode->data.primaryExpression.members == ZR_NULL ||
         argumentNode->data.primaryExpression.members->count == 0U)) {
        return argumentNode->data.primaryExpression.property->location;
    }
    return argumentNode != ZR_NULL ? argumentNode->location
                                   : (SZrFileRange){0};
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

static EZrParameterPassingMode call_diagnostic_passing_mode_at(
        const SZrResolvedCallSignature *resolvedSignature,
        TZrSize parameterIndex) {
    const EZrParameterPassingMode *mode;

    if (resolvedSignature == ZR_NULL ||
        parameterIndex >= resolvedSignature->parameterPassingModes.length) {
        return ZR_PARAMETER_PASSING_MODE_VALUE;
    }
    mode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterPassingModes,
            parameterIndex);
    return mode != ZR_NULL ? *mode : ZR_PARAMETER_PASSING_MODE_VALUE;
}

TZrBool type_inference_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        EZrParameterPassingMode passingMode,
        SZrAstNode *node,
        SZrFileRange location,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType) {
    EZrCallOwnershipDiagnosticKind kind;
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticOwnershipFact fact;
    TZrChar expectedBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrChar actualBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *expectedText;
    const TZrChar *actualText;
    TZrBool built = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }
    kind = call_diagnostic_classify_ownership(
            passingMode, expectedType, actualType);
    if (kind == ZR_CALL_OWNERSHIP_DIAGNOSTIC_NONE) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (kind == ZR_CALL_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE) {
        built = ZrParser_DiagnosticBuilder_BuildWeakWake(
                cs->state, &diagnostic, location);
    } else if (kind == ZR_CALL_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE) {
        built = ZrParser_DiagnosticBuilder_BuildBorrowEscape(
                cs->state, &diagnostic, location);
    } else if (kind == ZR_CALL_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE) {
        built = ZrParser_DiagnosticBuilder_BuildLoanEscape(
                cs->state, &diagnostic, location);
    } else if (kind == ZR_CALL_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN) {
        built = ZrParser_DiagnosticBuilder_BuildOwnerToPlainEscape(
                cs->state, &diagnostic, location);
    } else {
        expectedText = ZrParser_TypeNameString_Get(
                cs->state, expectedType, expectedBuffer, sizeof(expectedBuffer));
        actualText = ZrParser_TypeNameString_Get(
                cs->state, actualType, actualBuffer, sizeof(actualBuffer));
        built = ZrParser_DiagnosticBuilder_BuildOwnershipMismatch(
                cs->state,
                &diagnostic,
                location,
                expectedText != ZR_NULL ? expectedText : "unknown",
                actualText != ZR_NULL ? actualText : "unknown");
    }
    if (!built) {
        return ZR_FALSE;
    }

    if (cs->semanticContext != ZR_NULL) {
        memset(&fact, 0, sizeof(fact));
        fact.node = node;
        fact.range = location;
        fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
        fact.qualifier = actualType->ownershipQualifier;
        fact.symbolId = ZR_SEMANTIC_ID_INVALID;
        fact.lifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
        fact.ownerLifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
        fact.isViolation = ZR_TRUE;
        fact.diagnosticMessage = diagnostic.message;
        (void)ZrParser_SemanticFacts_AppendOwnership(
                cs->semanticContext, &fact);
    }
    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_TRUE;
}

TZrBool type_inference_call_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *funcType,
        const SZrResolvedCallSignature *resolvedSignature,
        const SZrFunctionCall *call,
        TZrSize parameterIndex,
        const SZrInferredType *argumentType) {
    SZrAstNodeArray *parameters = call_diagnostic_parameter_list(funcType);
    const SZrInferredType *parameterType;
    SZrAstNode *argumentNode;
    EZrParameterPassingMode passingMode;

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
            call, parameters, parameterIndex);
    passingMode = call_diagnostic_passing_mode_at(
            resolvedSignature, parameterIndex);
    if (parameterType == ZR_NULL || argumentNode == ZR_NULL) {
        return ZR_FALSE;
    }
    return type_inference_diagnostic_report_ownership_mismatch(
            cs,
            passingMode,
            argumentNode,
            type_inference_call_diagnostic_argument_location(argumentNode),
            parameterType,
            argumentType);
}

static TZrBool call_diagnostic_report_argument_mismatch(
        SZrCompilerState *cs,
        const SZrAstNodeArray *parameters,
        SZrAstNode *argumentNode,
        TZrSize parameterIndex,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    SZrAstNode *parameterNode;
    SZrFileRange expectedTypeLocation;
    const SZrFileRange *expectedTypeLocationPtr = ZR_NULL;

    if (cs == ZR_NULL || parameters == ZR_NULL ||
        argumentNode == ZR_NULL || parameterType == ZR_NULL ||
        argumentType == ZR_NULL || parameterIndex >= parameters->count) {
        return ZR_FALSE;
    }
    parameterNode = parameters->nodes[parameterIndex];

    if (parameterNode != ZR_NULL &&
        parameterNode->type == ZR_AST_PARAMETER &&
        parameterNode->data.parameter.typeInfo != ZR_NULL &&
        parameterNode->data.parameter.typeInfo->name != ZR_NULL) {
        expectedTypeLocation =
                parameterNode->data.parameter.typeInfo->name->location;
        expectedTypeLocationPtr = &expectedTypeLocation;
    }
    ZrParser_TypeError_ReportDetailed(
            cs,
            "Argument type mismatch",
            parameterType,
            argumentType,
            type_inference_call_diagnostic_argument_location(argumentNode),
            expectedTypeLocationPtr);
    return ZR_TRUE;
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

    if (cs == ZR_NULL || resolvedSignature == ZR_NULL ||
        argumentType == ZR_NULL || parameters == ZR_NULL ||
        parameterIndex >= parameters->count ||
        parameterIndex >= resolvedSignature->parameterTypes.length) {
        return ZR_FALSE;
    }
    argumentNode = call_diagnostic_argument_for_parameter(
            call,
            parameters,
            parameterIndex);
    parameterType = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterTypes,
            parameterIndex);
    if (argumentNode == ZR_NULL || parameterType == ZR_NULL) {
        return ZR_FALSE;
    }
    return call_diagnostic_report_argument_mismatch(
            cs,
            parameters,
            argumentNode,
            parameterIndex,
            parameterType,
            argumentType);
}

TZrBool type_inference_member_call_diagnostic_report_argument_mismatch(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrFunctionCall *call,
        SZrAstNode *argumentNode,
        TZrSize parameterIndex,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    SZrAstNodeArray *parameters;

    if (memberInfo == ZR_NULL || parameterType == ZR_NULL) {
        return ZR_FALSE;
    }
    parameters = call_diagnostic_declaration_parameter_list(
            memberInfo->declarationNode);
    if (argumentNode == ZR_NULL) {
        argumentNode = call_diagnostic_argument_for_parameter(
                call, parameters, parameterIndex);
    }
    return call_diagnostic_report_argument_mismatch(
            cs,
            parameters,
            argumentNode,
            parameterIndex,
            parameterType,
            argumentType);
}

TZrBool type_inference_member_call_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrFunctionCall *call,
        SZrAstNode *argumentNode,
        TZrSize parameterIndex,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType) {
    SZrAstNodeArray *parameters;
    EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || parameterType == ZR_NULL ||
        argumentType == ZR_NULL) {
        return ZR_FALSE;
    }
    parameters = call_diagnostic_declaration_parameter_list(
            memberInfo->declarationNode);
    if (argumentNode == ZR_NULL) {
        argumentNode = call_diagnostic_argument_for_parameter(
                call, parameters, parameterIndex);
    }
    if (argumentNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (parameterIndex < memberInfo->parameterPassingModes.length) {
        const EZrParameterPassingMode *mode =
                (const EZrParameterPassingMode *)ZrCore_Array_Get(
                        (SZrArray *)&memberInfo->parameterPassingModes,
                        parameterIndex);
        if (mode != ZR_NULL) {
            passingMode = *mode;
        }
    }
    return type_inference_diagnostic_report_ownership_mismatch(
            cs,
            passingMode,
            argumentNode,
            type_inference_call_diagnostic_argument_location(argumentNode),
            parameterType,
            argumentType);
}
