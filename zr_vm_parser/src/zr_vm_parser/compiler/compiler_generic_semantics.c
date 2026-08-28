//
// Created by Auto on 2026/04/02.
//

#include "compiler_internal.h"
#include "compile_expression_internal.h"
#include "zr_vm_parser/place.h"
#include "zr_vm_parser/variance.h"

typedef enum EZrVariancePosition {
    ZR_VARIANCE_POSITION_INVARIANT = 0,
    ZR_VARIANCE_POSITION_OUT = 1,
    ZR_VARIANCE_POSITION_IN = -1,
} EZrVariancePosition;

typedef struct SZrVarianceCollector {
    TZrSize targetIndex;
    TZrSize currentIndex;
    SZrVarianceViolation *outViolation;
    TZrBool found;
} SZrVarianceCollector;

static const TZrChar *compiler_string_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

TZrBool compiler_parameter_is_readonly(const SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           (parameter->isConst || parameter->passingMode == ZR_PARAMETER_PASSING_MODE_IN);
}

void compiler_register_readonly_parameter_name(SZrCompilerState *cs,
                                               const SZrParameter *parameter,
                                               SZrString *parameterName) {
    if (cs == ZR_NULL || parameter == ZR_NULL || parameterName == ZR_NULL) {
        return;
    }

    if (compiler_parameter_is_readonly(parameter)) {
        ZrCore_Array_Push(cs->state, &cs->constParameters, &parameterName);
    }
}

TZrBool compiler_expression_is_assignable_storage_location(const SZrAstNode *node) {
    return ZrParser_PlaceExpression_Classify(node) !=
           ZR_PARSER_PLACE_EXPRESSION_INVALID;
}

static EZrCallArgumentMarker compiler_call_argument_marker_at(
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

static SZrString *compiler_call_parameter_name_at(
        const SZrAstNodeArray *parameterList,
        const SZrArray *parameterNames,
        TZrSize index) {
    if (parameterList != ZR_NULL && index < parameterList->count) {
        SZrAstNode *parameter = parameterList->nodes[index];
        if (parameter != ZR_NULL && parameter->type == ZR_AST_PARAMETER &&
            parameter->data.parameter.name != ZR_NULL) {
            return parameter->data.parameter.name->name;
        }
    }
    if (parameterNames != ZR_NULL && index < parameterNames->length) {
        SZrString **name = (SZrString **)ZrCore_Array_Get(
                (SZrArray *)parameterNames, index);
        return name != ZR_NULL ? *name : ZR_NULL;
    }
    return ZR_NULL;
}

static TZrSize compiler_call_argument_index_for_parameter(
        const SZrFunctionCall *call,
        const SZrAstNodeArray *parameterList,
        const SZrArray *parameterNames,
        TZrSize parameterIndex) {
    SZrString *parameterName;
    if (call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }
    if (!call->hasNamedArgs || call->argNames == ZR_NULL) {
        return parameterIndex < call->args->count
                       ? parameterIndex
                       : ZR_PARSER_INDEX_NONE;
    }
    parameterName = compiler_call_parameter_name_at(
            parameterList, parameterNames, parameterIndex);
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

TZrBool compiler_validate_call_argument_passing_contract(
        SZrCompilerState *cs,
        const SZrArray *parameterPassingModes,
        const SZrAstNodeArray *parameterList,
        const SZrArray *parameterNames,
        const SZrFunctionCall *call) {
    if (cs == ZR_NULL || call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_TRUE;
    }
    TZrSize parameterCount = parameterPassingModes != ZR_NULL
            ? parameterPassingModes->length
            : call->args->count;
    for (TZrSize index = 0u; index < parameterCount; index++) {
        EZrParameterPassingMode mode = ZR_PARAMETER_PASSING_MODE_VALUE;
        TZrSize argumentIndex = compiler_call_argument_index_for_parameter(
                call, parameterList, parameterNames, index);
        EZrCallArgumentMarker marker;
        SZrAstNode *argument;
        if (argumentIndex == ZR_PARSER_INDEX_NONE) {
            continue;
        }
        marker = compiler_call_argument_marker_at(call, argumentIndex);
        argument = call->args->nodes[argumentIndex];
        if (parameterPassingModes != ZR_NULL &&
            index < parameterPassingModes->length) {
            EZrParameterPassingMode *modePtr =
                    (EZrParameterPassingMode *)ZrCore_Array_Get(
                            (SZrArray *)parameterPassingModes, index);
            if (modePtr != ZR_NULL) {
                mode = *modePtr;
            }
        }
        if ((mode == ZR_PARAMETER_PASSING_MODE_OUT ||
             mode == ZR_PARAMETER_PASSING_MODE_REF) &&
            !compiler_expression_is_assignable_storage_location(argument)) {
            ZrParser_Compiler_Error(
                    cs,
                    mode == ZR_PARAMETER_PASSING_MODE_OUT
                            ? "out argument must be an assignable storage location (writable Place)"
                            : "ref argument must be an assignable storage location (writable Place)",
                    argument != ZR_NULL ? argument->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
        if (mode == ZR_PARAMETER_PASSING_MODE_OUT &&
            marker != ZR_CALL_ARGUMENT_MARKER_OUT) {
            ZrParser_Compiler_Error(
                    cs,
                    "out parameter requires the 'out' argument marker",
                    argument != ZR_NULL ? argument->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
        if (mode == ZR_PARAMETER_PASSING_MODE_REF &&
            marker != ZR_CALL_ARGUMENT_MARKER_REF) {
            ZrParser_Compiler_Error(
                    cs,
                    "ref parameter requires the 'ref' argument marker",
                    argument != ZR_NULL ? argument->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
        if (mode != ZR_PARAMETER_PASSING_MODE_OUT &&
            mode != ZR_PARAMETER_PASSING_MODE_REF &&
            marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
            ZrParser_Compiler_Error(
                    cs,
                    "Value and in parameters do not accept an argument marker",
                    argument != ZR_NULL ? argument->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static EZrGenericVariance interface_generic_variance_for_name(
        SZrAstNodeArray *params,
        SZrString *name,
        const SZrAstNode **outDeclaration) {
    if (outDeclaration != ZR_NULL) {
        *outDeclaration = ZR_NULL;
    }
    if (params == ZR_NULL || name == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode == ZR_NULL ||
            paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL ||
            paramNode->data.parameter.name->name == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(paramNode->data.parameter.name->name, name)) {
            if (outDeclaration != ZR_NULL) {
                *outDeclaration = paramNode;
            }
            return paramNode->data.parameter.variance;
        }
    }

    return ZR_GENERIC_VARIANCE_NONE;
}

static EZrGenericVariance prototype_generic_variance_at(SZrCompilerState *cs, SZrString *typeName, TZrSize index) {
    SZrTypePrototypeInfo *prototype;
    SZrTypeGenericParameterInfo *parameterInfo;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    prototype = find_compiler_type_prototype(cs, typeName);
    if (prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        index >= prototype->genericParameters.length) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    parameterInfo = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&prototype->genericParameters, index);
    return parameterInfo != ZR_NULL ? parameterInfo->variance : ZR_GENERIC_VARIANCE_NONE;
}

static EZrVariancePosition combine_variance_position(EZrVariancePosition outerPosition,
                                                     EZrGenericVariance declaredVariance) {
    if (outerPosition == ZR_VARIANCE_POSITION_INVARIANT || declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
        return ZR_VARIANCE_POSITION_INVARIANT;
    }

    if (declaredVariance == ZR_GENERIC_VARIANCE_OUT) {
        return outerPosition;
    }

    return outerPosition == ZR_VARIANCE_POSITION_OUT ? ZR_VARIANCE_POSITION_IN : ZR_VARIANCE_POSITION_OUT;
}

static SZrFileRange variance_type_location(
        const SZrType *typeNode,
        SZrFileRange fallback) {
    return typeNode != ZR_NULL && typeNode->name != ZR_NULL
                   ? typeNode->name->location
                   : fallback;
}

static TZrBool collect_invalid_variance_usage(
        SZrVarianceCollector *collector,
        const SZrAstNode *parameterDeclaration,
        SZrString *parameterName,
        EZrGenericVariance declaredVariance,
        EZrVarianceContextKind contextKind,
        TZrBool nestedUsage,
        const SZrType *typeNode,
        SZrFileRange fallbackLocation) {
    SZrVarianceViolation *violation;

    if (collector == ZR_NULL || collector->outViolation == ZR_NULL ||
        parameterDeclaration == ZR_NULL || parameterName == ZR_NULL) {
        return ZR_TRUE;
    }
    if (collector->currentIndex++ != collector->targetIndex) {
        return ZR_TRUE;
    }

    violation = collector->outViolation;
    memset(violation, 0, sizeof(*violation));
    violation->node = typeNode != ZR_NULL ? typeNode->name : ZR_NULL;
    violation->parameterName = parameterName;
    violation->declaredVariance = declaredVariance;
    violation->contextKind = contextKind;
    violation->nestedUsage = nestedUsage;
    violation->location = variance_type_location(typeNode, fallbackLocation);
    violation->declarationRange =
            parameterDeclaration->data.parameter.nameLocation;
    collector->found = ZR_TRUE;
    return ZR_FALSE;
}

static TZrBool validate_interface_type_variance(SZrCompilerState *cs,
                                                SZrAstNodeArray *interfaceGenericParams,
                                                SZrType *typeNode,
                                                EZrVariancePosition position,
                                                EZrVarianceContextKind contextKind,
                                                TZrBool nestedUsage,
                                                SZrFileRange location,
                                                SZrVarianceCollector *collector) {
    SZrString *typeName = ZR_NULL;
    EZrGenericVariance declaredVariance;
    const SZrAstNode *parameterDeclaration = ZR_NULL;

    if (cs == ZR_NULL || interfaceGenericParams == ZR_NULL ||
        typeNode == ZR_NULL || collector == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeNode->subType != ZR_NULL &&
        !validate_interface_type_variance(cs,
                                          interfaceGenericParams,
                                          typeNode->subType,
                                          ZR_VARIANCE_POSITION_INVARIANT,
                                          contextKind,
                                          nestedUsage,
                                          location,
                                          collector)) {
        return ZR_FALSE;
    }

    if (typeNode->name == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        typeName = typeNode->name->data.identifier.name;
        declaredVariance = interface_generic_variance_for_name(
                interfaceGenericParams, typeName, &parameterDeclaration);
        if (declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
            return ZR_TRUE;
        }

        if (position == ZR_VARIANCE_POSITION_INVARIANT ||
            (declaredVariance == ZR_GENERIC_VARIANCE_OUT && position == ZR_VARIANCE_POSITION_IN) ||
            (declaredVariance == ZR_GENERIC_VARIANCE_IN && position == ZR_VARIANCE_POSITION_OUT)) {
            return collect_invalid_variance_usage(
                    collector,
                    parameterDeclaration,
                    typeName,
                    declaredVariance,
                    contextKind,
                    nestedUsage,
                    typeNode,
                    location);
        }

        return ZR_TRUE;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;
        SZrString *outerTypeName = genericType->name != ZR_NULL ? genericType->name->name : ZR_NULL;

        for (TZrSize index = 0; genericType->params != ZR_NULL && index < genericType->params->count; index++) {
            SZrAstNode *argNode = genericType->params->nodes[index];
            EZrGenericVariance declaredOuterVariance = prototype_generic_variance_at(cs, outerTypeName, index);
            EZrVariancePosition childPosition = combine_variance_position(position, declaredOuterVariance);

            if (argNode != ZR_NULL &&
                argNode->type == ZR_AST_TYPE &&
                !validate_interface_type_variance(cs,
                                                 interfaceGenericParams,
                                                 &argNode->data.type,
                                                 childPosition,
                                                 contextKind,
                                                 ZR_TRUE,
                                                 location,
                                                 collector)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrParser_Variance_InterfaceViolationAt(
        SZrCompilerState *cs,
        const SZrAstNode *interfaceNode,
        TZrSize violationIndex,
        SZrVarianceViolation *outViolation) {
    const SZrInterfaceDeclaration *interfaceDecl;
    SZrVarianceCollector collector;

    if (outViolation != ZR_NULL) {
        memset(outViolation, 0, sizeof(*outViolation));
    }
    if (cs == ZR_NULL || interfaceNode == ZR_NULL ||
        interfaceNode->type != ZR_AST_INTERFACE_DECLARATION ||
        outViolation == ZR_NULL) {
        return ZR_FALSE;
    }

    interfaceDecl = &interfaceNode->data.interfaceDeclaration;
    if (interfaceDecl->generic == ZR_NULL ||
        interfaceDecl->generic->params == ZR_NULL ||
        interfaceDecl->members == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&collector, 0, sizeof(collector));
    collector.targetIndex = violationIndex;
    collector.outViolation = outViolation;

    for (TZrSize memberIndex = 0;
         memberIndex < interfaceDecl->members->count;
         memberIndex++) {
        SZrAstNode *member = interfaceDecl->members->nodes[memberIndex];

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_INTERFACE_FIELD_DECLARATION:
                if (!validate_interface_type_variance(
                            cs,
                            interfaceDecl->generic->params,
                            member->data.interfaceFieldDeclaration.typeInfo,
                            ZR_VARIANCE_POSITION_INVARIANT,
                            ZR_VARIANCE_CONTEXT_FIELD,
                            ZR_FALSE,
                            member->location,
                            &collector)) {
                    return collector.found;
                }
                break;

            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
                if (member->data.interfaceMethodSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMethodSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode =
                                member->data.interfaceMethodSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL &&
                            paramNode->type == ZR_AST_PARAMETER &&
                            !validate_interface_type_variance(
                                    cs,
                                    interfaceDecl->generic->params,
                                    paramNode->data.parameter.typeInfo,
                                    ZR_VARIANCE_POSITION_IN,
                                    ZR_VARIANCE_CONTEXT_PARAMETER,
                                    ZR_FALSE,
                                    paramNode->location,
                                    &collector)) {
                            return collector.found;
                        }
                    }
                }
                if (!validate_interface_type_variance(
                            cs,
                            interfaceDecl->generic->params,
                            member->data.interfaceMethodSignature.returnType,
                            ZR_VARIANCE_POSITION_OUT,
                            ZR_VARIANCE_CONTEXT_RETURN,
                            ZR_FALSE,
                            member->location,
                            &collector)) {
                    return collector.found;
                }
                break;

            case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: {
                SZrInterfacePropertySignature *property =
                        &member->data.interfacePropertySignature;
                EZrVariancePosition propertyPosition =
                        ZR_VARIANCE_POSITION_INVARIANT;
                EZrVarianceContextKind contextKind =
                        ZR_VARIANCE_CONTEXT_PROPERTY;

                if (property->hasGet && !property->hasSet) {
                    propertyPosition = ZR_VARIANCE_POSITION_OUT;
                    contextKind = ZR_VARIANCE_CONTEXT_GETTER;
                } else if (property->hasSet && !property->hasGet) {
                    propertyPosition = ZR_VARIANCE_POSITION_IN;
                    contextKind = ZR_VARIANCE_CONTEXT_SETTER;
                }

                if (!validate_interface_type_variance(
                            cs,
                            interfaceDecl->generic->params,
                            property->typeInfo,
                            propertyPosition,
                            contextKind,
                            ZR_FALSE,
                            member->location,
                            &collector)) {
                    return collector.found;
                }
                break;
            }

            case ZR_AST_PROPERTY_DECLARATION: {
                SZrPropertyDeclaration *property =
                        &member->data.propertyDeclaration;
                TZrBool hasGet = ZR_FALSE;
                TZrBool hasWrite = ZR_FALSE;
                EZrVariancePosition propertyPosition =
                        ZR_VARIANCE_POSITION_INVARIANT;
                EZrVarianceContextKind contextKind =
                        ZR_VARIANCE_CONTEXT_PROPERTY;

                if (property->accessors != ZR_NULL) {
                    for (TZrSize accessorIndex = 0U;
                         accessorIndex < property->accessors->count;
                         accessorIndex++) {
                        SZrAstNode *accessorNode =
                                property->accessors->nodes[accessorIndex];
                        if (accessorNode == ZR_NULL ||
                            accessorNode->type != ZR_AST_PROPERTY_ACCESSOR) {
                            continue;
                        }
                        if (accessorNode->data.propertyAccessor.kind ==
                            ZR_PROPERTY_ACCESSOR_GET) {
                            hasGet = ZR_TRUE;
                        } else {
                            hasWrite = ZR_TRUE;
                        }
                    }
                }
                if (hasGet && !hasWrite) {
                    propertyPosition = ZR_VARIANCE_POSITION_OUT;
                    contextKind = ZR_VARIANCE_CONTEXT_GETTER;
                } else if (hasWrite && !hasGet) {
                    propertyPosition = ZR_VARIANCE_POSITION_IN;
                    contextKind = ZR_VARIANCE_CONTEXT_SETTER;
                }
                if (!validate_interface_type_variance(
                            cs,
                            interfaceDecl->generic->params,
                            property->typeInfo,
                            propertyPosition,
                            contextKind,
                            ZR_FALSE,
                            member->location,
                            &collector)) {
                    return collector.found;
                }
                break;
            }

            case ZR_AST_INTERFACE_META_SIGNATURE:
                if (member->data.interfaceMetaSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMetaSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode =
                                member->data.interfaceMetaSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL &&
                            paramNode->type == ZR_AST_PARAMETER &&
                            !validate_interface_type_variance(
                                    cs,
                                    interfaceDecl->generic->params,
                                    paramNode->data.parameter.typeInfo,
                                    ZR_VARIANCE_POSITION_IN,
                                    ZR_VARIANCE_CONTEXT_PARAMETER,
                                    ZR_FALSE,
                                    paramNode->location,
                                    &collector)) {
                            return collector.found;
                        }
                    }
                }
                if (!validate_interface_type_variance(
                            cs,
                            interfaceDecl->generic->params,
                            member->data.interfaceMetaSignature.returnType,
                            ZR_VARIANCE_POSITION_OUT,
                            ZR_VARIANCE_CONTEXT_RETURN,
                            ZR_FALSE,
                            member->location,
                            &collector)) {
                    return collector.found;
                }
                break;

            default:
                break;
        }
    }

    return ZR_FALSE;
}

static const TZrChar *variance_context_text(EZrVarianceContextKind contextKind) {
    switch (contextKind) {
        case ZR_VARIANCE_CONTEXT_FIELD:
            return "field";
        case ZR_VARIANCE_CONTEXT_PARAMETER:
            return "contravariant parameter";
        case ZR_VARIANCE_CONTEXT_RETURN:
            return "covariant return";
        case ZR_VARIANCE_CONTEXT_GETTER:
            return "getter";
        case ZR_VARIANCE_CONTEXT_SETTER:
            return "setter";
        case ZR_VARIANCE_CONTEXT_PROPERTY:
        default:
            return "property";
    }
}

TZrBool ZrParser_Variance_BuildDiagnostic(
        SZrState *state,
        const SZrVarianceViolation *violation,
        SZrStructuredDiagnostic *outDiagnostic) {
    const TZrChar *nameText;
    const TZrChar *varianceText;
    const TZrChar *cause;
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (state == ZR_NULL || violation == ZR_NULL ||
        violation->parameterName == ZR_NULL || outDiagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = compiler_string_native(violation->parameterName);
    varianceText = violation->declaredVariance == ZR_GENERIC_VARIANCE_OUT
            ? "covariant"
            : "contravariant";
    cause = violation->declaredVariance == ZR_GENERIC_VARIANCE_OUT
            ? "A covariant generic parameter cannot appear in an input or invariant position."
            : "A contravariant generic parameter cannot appear in an output or invariant position.";
    snprintf(message,
             sizeof(message),
             "%s generic parameter '%s' cannot be used in %s%s position",
             varianceText,
             nameText != ZR_NULL ? nameText : "<unknown>",
             violation->nestedUsage ? "nested " : "",
             variance_context_text(violation->contextKind));

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                outDiagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                violation->location,
                "invalid_variance",
                message,
                cause,
                "Change the generic parameter variance or move this type use to a compatible position.")) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                outDiagnostic,
                violation->declarationRange,
                "Variance parameter is declared here") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                outDiagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Variance_PublishInterfaceDiagnostics(
        SZrCompilerState *compilerState,
        const SZrAstNode *interfaceNode) {
    TZrSize violationIndex = 0U;
    SZrVarianceViolation violation;

    if (compilerState == ZR_NULL || compilerState->state == ZR_NULL ||
        compilerState->semanticContext == ZR_NULL || interfaceNode == ZR_NULL) {
        return ZR_FALSE;
    }

    while (ZrParser_Variance_InterfaceViolationAt(
            compilerState,
            interfaceNode,
            violationIndex,
            &violation)) {
        SZrStructuredDiagnostic diagnostic;
        SZrSemanticDiagnosticFact fact;

        ZrParser_StructuredDiagnostic_Init(&diagnostic);
        if (!ZrParser_Variance_BuildDiagnostic(
                    compilerState->state, &violation, &diagnostic)) {
            return ZR_FALSE;
        }
        memset(&fact, 0, sizeof(fact));
        fact.node = violation.node;
        fact.diagnostic = diagnostic;
        if (!ZrParser_SemanticFacts_AppendDiagnostic(
                    compilerState->semanticContext, &fact)) {
            ZrParser_StructuredDiagnostic_Free(
                    compilerState->state, &diagnostic);
            return ZR_FALSE;
        }
        ZrParser_StructuredDiagnostic_Free(compilerState->state, &diagnostic);
        violationIndex++;
    }

    return ZR_TRUE;
}

TZrBool compiler_validate_interface_variance_rules(
        SZrCompilerState *cs,
        SZrAstNode *interfaceNode) {
    SZrVarianceViolation violation;
    SZrStructuredDiagnostic diagnostic;

    if (!ZrParser_Variance_InterfaceViolationAt(
                cs, interfaceNode, 0U, &violation)) {
        return ZR_TRUE;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (ZrParser_Variance_BuildDiagnostic(
                cs->state, &violation, &diagnostic)) {
        ZrParser_Compiler_StructuredError(cs, &diagnostic);
    } else {
        ZrParser_Compiler_Error(
                cs, "Invalid generic variance", violation.location);
    }
    return ZR_FALSE;
}
