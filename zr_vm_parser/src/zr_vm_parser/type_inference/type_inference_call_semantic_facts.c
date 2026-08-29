#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_contract.h"
#include "zr_vm_parser/type_inference.h"

#include "type_inference_internal.h"

static SZrAstNode *type_inference_call_target(const SZrPrimaryExpression *primary,
                                              SZrAstNode *callNode) {
    SZrAstNode *candidate = ZR_NULL;

    if (primary == ZR_NULL || callNode == ZR_NULL || primary->members == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < primary->members->count; ++index) {
        SZrAstNode *member = primary->members->nodes[index];
        if (member == callNode) {
            break;
        }
        if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION) {
            candidate = member->data.memberExpression.property;
        }
    }
    return candidate != ZR_NULL ? candidate : primary->property;
}

static TZrBool type_inference_call_label_append(TZrChar *buffer,
                                                TZrSize bufferSize,
                                                TZrSize *offset,
                                                const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(text);
    if (*offset + length + 1u > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer + *offset, text, length);
    *offset += length;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

const SZrAstNodeArray *type_inference_call_parameters(
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
        case ZR_AST_CLASS_META_FUNCTION:
            return declaration->data.classMetaFunction.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            return declaration->data.structMetaFunction.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.params;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            return declaration->data.interfaceMetaSignature.params;
        case ZR_AST_LAMBDA_EXPRESSION:
            return declaration->data.lambdaExpression.params;
        default:
            return ZR_NULL;
    }
}

static const TZrChar *type_inference_call_passing_prefix(
        const SZrCanonicalParameterContract *contract) {
    if (contract == ZR_NULL) {
        return ZR_NULL;
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref "
                           : "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref readonly "
                           : "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
        default: return "";
    }
}

static TZrBool type_inference_call_generic_clause_append(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset,
        const SZrArray *genericParameters) {
    TZrSize index;

    if (genericParameters == ZR_NULL || genericParameters->length == 0u) {
        return ZR_TRUE;
    }
    if (!genericParameters->isValid ||
        !type_inference_call_label_append(buffer, bufferSize, offset, "<")) {
        return ZR_FALSE;
    }
    for (index = 0u; index < genericParameters->length; ++index) {
        const SZrTypeGenericParameterInfo *parameter =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)genericParameters, index);
        const TZrChar *name;

        if (parameter == ZR_NULL || parameter->name == ZR_NULL) {
            return ZR_FALSE;
        }
        name = ZrCore_String_GetNativeString(parameter->name);
        if (name == ZR_NULL || name[0] == '\0' ||
            (index > 0u && !type_inference_call_label_append(
                                   buffer, bufferSize, offset, ", "))) {
            return ZR_FALSE;
        }
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT) {
            if (!type_inference_call_label_append(
                        buffer, bufferSize, offset, "const ") ||
                !type_inference_call_label_append(buffer, bufferSize, offset, name) ||
                !type_inference_call_label_append(
                        buffer, bufferSize, offset, ": int")) {
                return ZR_FALSE;
            }
            continue;
        }
        if (parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE) {
            return ZR_FALSE;
        }
        if (parameter->variance == ZR_GENERIC_VARIANCE_IN &&
            !type_inference_call_label_append(buffer, bufferSize, offset, "in ")) {
            return ZR_FALSE;
        }
        if (parameter->variance == ZR_GENERIC_VARIANCE_OUT &&
            !type_inference_call_label_append(buffer, bufferSize, offset, "out ")) {
            return ZR_FALSE;
        }
        if (!type_inference_call_label_append(buffer, bufferSize, offset, name)) {
            return ZR_FALSE;
        }
    }
    return type_inference_call_label_append(buffer, bufferSize, offset, ">");
}

SZrString *type_inference_callable_signature_display(
        SZrCompilerState *cs,
        const TZrChar *namePrefix,
        SZrString *name,
        const SZrAstNodeArray *parameters,
        const SZrArray *parameterNames,
        const SZrArray *genericParameters,
        TZrTypeId callTypeId) {
    const SZrCanonicalTypeNode *functionType;
    TZrChar buffer[1024];
    TZrChar typeBuffer[256];
    const TZrChar *receiverPrefix = "";
    TZrSize offset = 0u;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || namePrefix == ZR_NULL ||
        name == ZR_NULL || callTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    functionType = ZrParser_CanonicalType_Find(cs->semanticContext, callTypeId);
    if (functionType == ZR_NULL || functionType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_NULL;
    }
    if (functionType->data.function.receiverEffect ==
        ZR_CANONICAL_RECEIVER_READONLY) {
        receiverPrefix = "const fn ";
    } else if (functionType->data.function.receiverEffect ==
               ZR_CANONICAL_RECEIVER_MUTABLE) {
        receiverPrefix = "fn ";
    }
    buffer[0] = '\0';
    if (!type_inference_call_label_append(
                buffer, sizeof(buffer), &offset, receiverPrefix) ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset, namePrefix) ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset,
                                          ZrCore_String_GetNativeString(name)) ||
        !type_inference_call_generic_clause_append(
                buffer, sizeof(buffer), &offset, genericParameters) ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset, "(")) {
        return ZR_NULL;
    }
    for (index = 0u; index < functionType->data.function.parameterContracts.length; ++index) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->data.function.parameterContracts, index);
        const SZrCanonicalTypeNode *contractType;
        const TZrChar *parameterName = ZR_NULL;
        TZrTypeId displayTypeId;

        if (contract == ZR_NULL) {
            return ZR_NULL;
        }
        displayTypeId = contract->typeId;
        if (contract->passingForm != ZR_CANONICAL_PASSING_VALUE) {
            contractType = ZrParser_CanonicalType_Find(
                    cs->semanticContext, contract->typeId);
            if (contractType == ZR_NULL || contractType->kind != ZR_CANONICAL_TYPE_REF) {
                return ZR_NULL;
            }
            displayTypeId = contractType->data.refType.pointeeTypeId;
        }
        if (!ZrParser_CanonicalType_Format(
                    cs->semanticContext, displayTypeId, typeBuffer, sizeof(typeBuffer))) {
            return ZR_NULL;
        }
        if (parameters != ZR_NULL && index < parameters->count &&
            parameters->nodes[index] != ZR_NULL &&
            parameters->nodes[index]->type == ZR_AST_PARAMETER &&
            parameters->nodes[index]->data.parameter.name != ZR_NULL &&
            parameters->nodes[index]->data.parameter.name->name != ZR_NULL) {
            parameterName = ZrCore_String_GetNativeString(
                    parameters->nodes[index]->data.parameter.name->name);
        } else if (parameterNames != ZR_NULL) {
            SZrString **metadataName;

            if (!parameterNames->isValid || index >= parameterNames->length) {
                return ZR_NULL;
            }
            metadataName = (SZrString **)ZrCore_Array_Get(
                    (SZrArray *)parameterNames, index);
            if (metadataName == ZR_NULL || *metadataName == ZR_NULL) {
                return ZR_NULL;
            }
            parameterName = ZrCore_String_GetNativeString(*metadataName);
            if (parameterName == ZR_NULL || parameterName[0] == '\0') {
                return ZR_NULL;
            }
        }
        if (index > 0u && !type_inference_call_label_append(
                                  buffer, sizeof(buffer), &offset, ", ")) {
            return ZR_NULL;
        }
        if (parameterName != ZR_NULL && parameterName[0] != '\0') {
            if (!type_inference_call_label_append(buffer, sizeof(buffer), &offset, parameterName) ||
                !type_inference_call_label_append(buffer, sizeof(buffer), &offset, ": ")) {
                return ZR_NULL;
            }
        }
        if (!type_inference_call_label_append(
                    buffer,
                    sizeof(buffer),
                    &offset,
                    type_inference_call_passing_prefix(contract)) ||
            !type_inference_call_label_append(
                    buffer, sizeof(buffer), &offset, typeBuffer)) {
            return ZR_NULL;
        }
    }
    if (!ZrParser_CanonicalType_Format(cs->semanticContext,
                                       functionType->data.function.returnTypeId,
                                       typeBuffer,
                                       sizeof(typeBuffer)) ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset, "): ") ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset, typeBuffer)) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(cs->state, buffer, offset);
}

static SZrString *type_inference_call_signature_display(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *functionInfo,
        TZrTypeId callTypeId) {
    if (functionInfo == ZR_NULL) {
        return ZR_NULL;
    }
    if (functionInfo->signatureDisplay != ZR_NULL) {
        return functionInfo->signatureDisplay;
    }
    return type_inference_callable_signature_display(
            cs,
            "",
            functionInfo->name,
            type_inference_call_parameters(functionInfo->declarationNode),
            ZR_NULL,
            &functionInfo->genericParameters,
            callTypeId);
}

static TZrTypeId type_inference_resolved_call_type_id(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *functionInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    const SZrCanonicalTypeNode *declaredFunction;
    EZrCanonicalReceiverEffect receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    TZrUInt32 effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_NONE;
    TZrTypeId resolvedTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || functionInfo == ZR_NULL ||
        resolvedSignature == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    declaredFunction = ZrParser_CanonicalType_Find(cs->semanticContext, functionInfo->typeId);
    if (declaredFunction != ZR_NULL && declaredFunction->kind == ZR_CANONICAL_TYPE_FUNCTION) {
        receiverEffect = declaredFunction->data.function.receiverEffect;
        effectFlags = declaredFunction->data.function.effectFlags;
    }
    resolvedTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &resolvedSignature->parameterTypes,
            &resolvedSignature->parameterPassingModes,
            &resolvedSignature->returnType,
            receiverEffect,
            effectFlags);
    if (resolvedTypeId == ZR_SEMANTIC_ID_INVALID) {
        return resolvedTypeId;
    }
    if (declaredFunction == ZR_NULL ||
        declaredFunction->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZrParser_SyntaxCallable_RefineFromDeclaration(
                cs->semanticContext,
                functionInfo->declarationNode,
                resolvedTypeId);
    }
    return ZrParser_CanonicalType_RebindFunctionSignature(
            cs->semanticContext, functionInfo->typeId, resolvedTypeId);
}

void type_inference_record_primary_call_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrAstNode *callNode,
        const SZrFunctionTypeInfo *funcTypeInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *target;
    TZrTypeId callTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION || callNode == ZR_NULL ||
        callNode->type != ZR_AST_FUNCTION_CALL || funcTypeInfo == ZR_NULL ||
        funcTypeInfo->name == ZR_NULL || resolvedSignature == ZR_NULL) {
        return;
    }

    target = type_inference_call_target(&node->data.primaryExpression, callNode);
    callTypeId = type_inference_resolved_call_type_id(cs, funcTypeInfo, resolvedSignature);
    if (callTypeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = target != ZR_NULL ? target->location : callNode->location;
    fact.declarationRange = funcTypeInfo->hasDeclarationRange
                                    ? funcTypeInfo->declarationRange
                                    : (SZrFileRange){0};
    fact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    fact.symbolId = funcTypeInfo->symbolId;
    fact.typeId = callTypeId;
    fact.name = funcTypeInfo->name;
    fact.signatureDisplay = type_inference_call_signature_display(cs, funcTypeInfo, callTypeId);
    fact.isResolved = !funcTypeInfo->isExternalCallable;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

static TZrTypeId type_inference_resolved_member_call_type_id(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    TZrTypeId resolvedTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        resolvedSignature == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    resolvedTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &resolvedSignature->parameterTypes,
            &resolvedSignature->parameterPassingModes,
            &resolvedSignature->returnType,
            memberInfo->receiverEffect,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (resolvedTypeId == ZR_SEMANTIC_ID_INVALID) {
        return resolvedTypeId;
    }
    return ZrParser_SyntaxCallable_RefineFromDeclaration(
            cs->semanticContext, memberInfo->declarationNode, resolvedTypeId);
}

TZrSymbolId type_inference_member_symbol_id(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        TZrTypeId callTypeId) {
    TZrSize index;
    TZrTypeId declarationTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (memberInfo->symbolId != ZR_SEMANTIC_ID_INVALID) {
        return memberInfo->symbolId;
    }
    for (index = 0U; index < cs->semanticContext->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        &cs->semanticContext->symbols, index);
        if (symbol != ZR_NULL &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            memberInfo->declarationNode != ZR_NULL &&
            symbol->astNode == memberInfo->declarationNode) {
            memberInfo->symbolId = symbol->id;
            return symbol->id;
        }
    }
    declarationTypeId =
            memberInfo->declarationNode == ZR_NULL &&
                    memberInfo->genericParameters.length != 0U
                    ? ZR_SEMANTIC_ID_INVALID
                    : callTypeId;
    memberInfo->symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            memberInfo->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            declarationTypeId,
            ZR_SEMANTIC_ID_INVALID,
            memberInfo->declarationNode,
            memberInfo->declarationNode != ZR_NULL
                    ? memberInfo->declarationNode->location
                    : (SZrFileRange){0});
    return memberInfo->symbolId;
}

void type_inference_publish_member_declaration_fact(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        TZrSymbolId symbolId,
        TZrTypeId typeId) {
    SZrSemanticReferenceFact declarationFact;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->declarationNode == ZR_NULL || memberInfo->name == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    for (index = 0U; index < cs->semanticContext->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &cs->semanticContext->referenceFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->isResolved && fact->symbolId == symbolId) {
            return;
        }
    }

    memset(&declarationFact, 0, sizeof(declarationFact));
    declarationFact.node = memberInfo->declarationNode;
    declarationFact.range = memberInfo->declarationNode->location;
    declarationFact.declarationRange = memberInfo->declarationNode->location;
    declarationFact.definitionRange = memberInfo->declarationNode->location;
    declarationFact.hasDefinitionRange = ZR_TRUE;
    declarationFact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    declarationFact.symbolId = symbolId;
    declarationFact.typeId = typeId;
    declarationFact.name = memberInfo->name;
    declarationFact.isResolved = ZR_TRUE;
    ZrParser_SemanticFacts_AppendReference(
            cs->semanticContext, &declarationFact);
}

static TZrTypeId type_inference_unbound_member_reference_type_id(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo) {
    SZrInferredType returnType;
    TZrTypeId typeId = ZR_SEMANTIC_ID_INVALID;
    SZrTypePrototypeInfo *ownerPrototype;

    const SZrAstNodeArray *parameters;
    const SZrParameter *variadicParameter = ZR_NULL;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->declarationNode == ZR_NULL ||
        memberInfo->ownerTypeName == ZR_NULL ||
        memberInfo->genericParameters.length > 0U ||
        memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ||
        memberInfo->parameterTypes.length != memberInfo->parameterCount ||
        memberInfo->parameterPassingModes.length != memberInfo->parameterCount ||
        !memberInfo->hasStructuredReturnType) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    parameters = type_inference_call_parameters(memberInfo->declarationNode);
    switch (memberInfo->declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            variadicParameter = memberInfo->declarationNode->data.classMethod.args;
            break;
        case ZR_AST_STRUCT_METHOD:
            variadicParameter = memberInfo->declarationNode->data.structMethod.args;
            break;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            variadicParameter =
                    memberInfo->declarationNode->data.interfaceMethodSignature.args;
            break;
        default:
            return ZR_SEMANTIC_ID_INVALID;
    }
    if (variadicParameter != ZR_NULL ||
        (parameters == ZR_NULL
                 ? memberInfo->parameterCount != 0U
                 : parameters->count != memberInfo->parameterCount)) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (TZrSize index = 0U; index < memberInfo->parameterCount; index++) {
        const SZrAstNode *parameter = parameters->nodes[index];
        const SZrInferredType *recordedType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)&memberInfo->parameterTypes, index);
        SZrInferredType declarationType;
        TZrBool matches;

        if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
            parameter->data.parameter.typeInfo == ZR_NULL ||
            recordedType == ZR_NULL ||
            !ZrParser_AstTypeToInferredType_Convert(
                    cs, parameter->data.parameter.typeInfo, &declarationType)) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        matches = ZrParser_InferredType_Equal(&declarationType, recordedType);
        ZrParser_InferredType_Free(cs->state, &declarationType);
        if (!matches) {
            return ZR_SEMANTIC_ID_INVALID;
        }
    }
    ownerPrototype = find_compiler_type_prototype_inference(
            cs, memberInfo->ownerTypeName);
    if (ownerPrototype == ZR_NULL || ownerPrototype->genericParameters.length > 0U) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(
            cs->state, &returnType, &memberInfo->structuredReturnType);
    typeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &memberInfo->parameterTypes,
            &memberInfo->parameterPassingModes,
            &returnType,
            memberInfo->receiverEffect,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (typeId != ZR_SEMANTIC_ID_INVALID) {
        typeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
                cs->semanticContext, memberInfo->declarationNode, typeId);
    }
    ZrParser_InferredType_Free(cs->state, &returnType);
    return typeId;
}

void type_inference_record_unbound_member_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        SZrTypeMemberInfo *memberInfo) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *target;
    TZrTypeId typeId;
    TZrSymbolId symbolId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
        return;
    }
    typeId = type_inference_unbound_member_reference_type_id(cs, memberInfo);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    target = memberNode->data.memberExpression.property;
    symbolId = type_inference_member_symbol_id(cs, memberInfo, typeId);
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = target != ZR_NULL ? target : memberNode;
    fact.range = target != ZR_NULL ? target->location : memberNode->location;
    fact.declarationRange = memberInfo->declarationNode->location;
    fact.kind = ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS;
    fact.symbolId = symbolId;
    fact.typeId = typeId;
    fact.name = memberInfo->name;
    fact.isResolved = ZR_TRUE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_external_callable_member_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        const SZrTypeMemberInfo *memberInfo,
        const SZrInferredType *returnType) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *target;
    TZrTypeId typeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || cs->typeEnv == ZR_NULL ||
        memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberInfo == ZR_NULL || returnType == ZR_NULL || memberInfo->name == ZR_NULL ||
        memberInfo->declarationNode != ZR_NULL || !memberInfo->isStatic ||
        memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ||
        memberInfo->parameterTypes.length != memberInfo->parameterCount ||
        (memberInfo->parameterPassingModes.length != 0U &&
         memberInfo->parameterPassingModes.length != memberInfo->parameterCount) ||
        memberInfo->genericParameters.length != 0U ||
        (memberInfo->parameterNames.isValid &&
         memberInfo->parameterNames.length != memberInfo->parameterCount)) {
        return;
    }

    typeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &memberInfo->parameterTypes,
            &memberInfo->parameterPassingModes,
            returnType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }

    target = memberNode->data.memberExpression.property;
    memset(&fact, 0, sizeof(fact));
    fact.node = target != ZR_NULL ? target : memberNode;
    fact.range = fact.node->location;
    fact.kind = ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS;
    fact.symbolId = ZR_SEMANTIC_ID_INVALID;
    fact.typeId = typeId;
    fact.name = memberInfo->name;
    fact.signatureDisplay = type_inference_callable_signature_display(
            cs,
            "",
            memberInfo->name,
            ZR_NULL,
            &memberInfo->parameterNames,
            &memberInfo->genericParameters,
            typeId);
    if (fact.signatureDisplay == ZR_NULL ||
        !ZrParser_TypeEnvironment_RegisterExternalCallable(
                cs->state,
                cs->typeEnv,
                memberInfo->name,
                returnType,
                &memberInfo->parameterTypes,
                &memberInfo->genericParameters,
                &memberInfo->parameterPassingModes,
                typeId,
                fact.signatureDisplay)) {
        return;
    }
    fact.isResolved = ZR_FALSE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_member_call_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        SZrTypeMemberInfo *memberInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *target;
    TZrTypeId callTypeId;
    TZrSymbolId symbolId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL || resolvedSignature == ZR_NULL) {
        return;
    }
    target = memberNode->data.memberExpression.property;
    callTypeId = type_inference_resolved_member_call_type_id(
            cs, memberInfo, resolvedSignature);
    if (callTypeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    symbolId = type_inference_member_symbol_id(cs, memberInfo, callTypeId);

    memset(&fact, 0, sizeof(fact));
    fact.node = target != ZR_NULL ? target : memberNode;
    fact.range = fact.node->location;
    fact.declarationRange = memberInfo->declarationNode != ZR_NULL
                                     ? memberInfo->declarationNode->location
                                     : (SZrFileRange){0};
    fact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    fact.symbolId = symbolId;
    fact.typeId = callTypeId;
    fact.contractRole = memberInfo->contractRole;
    fact.name = memberInfo->name;
    fact.signatureDisplay = type_inference_callable_signature_display(
            cs,
            "",
            memberInfo->name,
            type_inference_call_parameters(memberInfo->declarationNode),
            &memberInfo->parameterNames,
            &memberInfo->genericParameters,
            callTypeId);
    if (memberInfo->parameterNames.isValid && fact.signatureDisplay == ZR_NULL) {
        return;
    }
    fact.isResolved = symbolId != ZR_SEMANTIC_ID_INVALID ||
                      memberInfo->contractRole != ZR_MEMBER_CONTRACT_ROLE_NONE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

static SZrAstNode *type_inference_construct_call_target(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (node->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        return node->data.constructExpression.target;
    }
    if (node->type == ZR_AST_STRUCT_INIT_EXPRESSION &&
        node->data.structInitExpression.typeInfo != ZR_NULL) {
        return node->data.structInitExpression.typeInfo->name;
    }
    return ZR_NULL;
}

static SZrTypeMemberInfo *type_inference_construct_call_member(
        SZrCompilerState *cs,
        SZrAstNode *target,
        SZrString *typeName,
        SZrTypeMemberInfo *temporaryMember) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    prototype = find_compiler_type_prototype_inference(cs, typeName);
    if (prototype == ZR_NULL) {
        ensure_generic_instance_type_prototype(cs, typeName);
        prototype = find_compiler_type_prototype_inference(cs, typeName);
    }
    if (prototype != ZR_NULL) {
        for (TZrSize index = 0u; index < prototype->members.length; index++) {
            SZrTypeMemberInfo *member = (SZrTypeMemberInfo *)ZrCore_Array_Get(
                    &prototype->members, index);
            if (member != ZR_NULL && member->isMetaMethod &&
                member->metaType == ZR_META_CONSTRUCTOR) {
                return member;
            }
        }
    }
    return type_inference_source_constructor_member_build(
                   cs, target, typeName, temporaryMember)
                   ? temporaryMember
                   : ZR_NULL;
}

static TZrBool type_inference_construct_call_syntax(
        SZrAstNode *node,
        SZrFunctionCall *outCall) {
    if (node == ZR_NULL || outCall == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outCall, 0, sizeof(*outCall));
    if (node->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        const SZrConstructExpression *construct = &node->data.constructExpression;

        if (!construct->isNew &&
            construct->builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
            return ZR_FALSE;
        }
        outCall->args = construct->args;
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_STRUCT_INIT_EXPRESSION) {
        const SZrStructInitExpression *construct = &node->data.structInitExpression;

        outCall->args = construct->args;
        outCall->argNames = construct->argNames;
        outCall->hasNamedArgs = construct->hasNamedArgs;
        outCall->argumentMarkers = construct->argumentMarkers;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

void type_inference_record_construct_call_facts(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrInferredType *constructedType) {
    SZrSemanticExpressionFact expressionFact;
    SZrSemanticReferenceFact referenceFact;
    SZrResolvedCallSignature resolvedSignature;
    SZrFunctionCall call;
    SZrAstNode *target;
    SZrTypeMemberInfo *constructor;
    SZrTypeMemberInfo temporaryConstructor;
    TZrTypeId callTypeId;
    TZrSymbolId symbolId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL ||
        constructedType == ZR_NULL || constructedType->typeName == ZR_NULL ||
        !type_inference_construct_call_syntax(node, &call)) {
        return;
    }
    memset(&temporaryConstructor, 0, sizeof(temporaryConstructor));
    target = type_inference_construct_call_target(node);
    constructor = type_inference_construct_call_member(
            cs, target, constructedType->typeName, &temporaryConstructor);
    if (target == ZR_NULL || constructor == ZR_NULL || constructor->name == ZR_NULL) {
        type_inference_source_constructor_member_free(cs, &temporaryConstructor);
        return;
    }

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    if (resolve_generic_member_call_signature_detailed(
                cs,
                constructor,
                &call,
                &resolvedSignature,
                ZR_NULL,
                0u) != ZR_GENERIC_CALL_RESOLVE_OK) {
        type_inference_source_constructor_member_free(cs, &temporaryConstructor);
        return;
    }
    callTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &resolvedSignature.parameterTypes,
            &resolvedSignature.parameterPassingModes,
            &resolvedSignature.returnType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    if (callTypeId != ZR_SEMANTIC_ID_INVALID &&
        constructor->declarationNode != ZR_NULL) {
        TZrTypeId refinedTypeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
                cs->semanticContext, constructor->declarationNode, callTypeId);
        const SZrCanonicalTypeNode *refinedType =
                ZrParser_CanonicalType_Find(cs->semanticContext, refinedTypeId);

        if (refinedType != ZR_NULL &&
            refinedType->kind == ZR_CANONICAL_TYPE_FUNCTION) {
            const SZrCanonicalParameterContract *contracts =
                    refinedType->data.function.parameterContracts.length > 0u
                            ? (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                                      (SZrArray *)&refinedType->data.function.parameterContracts,
                                      0u)
                            : ZR_NULL;
            callTypeId = ZrParser_CanonicalType_InternFunction(
                    cs->semanticContext,
                    contracts,
                    refinedType->data.function.parameterContracts.length,
                    refinedType->data.function.returnTypeId,
                    ZR_CANONICAL_RECEIVER_NONE,
                    refinedType->data.function.effectFlags);
        } else {
            callTypeId = ZR_SEMANTIC_ID_INVALID;
        }
    }
    if (callTypeId == ZR_SEMANTIC_ID_INVALID) {
        free_resolved_call_signature(cs->state, &resolvedSignature);
        type_inference_source_constructor_member_free(cs, &temporaryConstructor);
        return;
    }
    symbolId = type_inference_member_symbol_id(cs, constructor, callTypeId);
    type_inference_publish_member_declaration_fact(
            cs, constructor, symbolId, callTypeId);

    memset(&expressionFact, 0, sizeof(expressionFact));
    expressionFact.node = node;
    expressionFact.range = node->location;
    expressionFact.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expressionFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    expressionFact.hasCallInfo = ZR_TRUE;
    expressionFact.callTargetName = constructor->name;
    expressionFact.callTargetRange = target->location;
    expressionFact.argumentCount = call.args != ZR_NULL ? call.args->count : 0u;
    expressionFact.hasNamedArguments = call.hasNamedArgs;
    expressionFact.isMemberCall = ZR_FALSE;
    ZrParser_InferredType_Copy(
            cs->state, &expressionFact.inferredType, constructedType);
    ZrParser_SemanticFacts_AppendExpression(cs->semanticContext, &expressionFact);
    ZrParser_InferredType_Free(cs->state, &expressionFact.inferredType);

    memset(&referenceFact, 0, sizeof(referenceFact));
    referenceFact.node = target;
    referenceFact.range = target->location;
    referenceFact.declarationRange = constructor->declarationNode != ZR_NULL
                                             ? constructor->declarationNode->location
                                             : (SZrFileRange){0};
    referenceFact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    referenceFact.symbolId = symbolId;
    referenceFact.typeId = callTypeId;
    referenceFact.contractRole = constructor->contractRole;
    referenceFact.name = constructor->name;
    referenceFact.signatureDisplay = type_inference_callable_signature_display(
            cs,
            "@",
            constructor->name,
            type_inference_call_parameters(constructor->declarationNode),
            &constructor->parameterNames,
            &constructor->genericParameters,
            callTypeId);
    referenceFact.isResolved = symbolId != ZR_SEMANTIC_ID_INVALID ||
                               constructor->contractRole != ZR_MEMBER_CONTRACT_ROLE_NONE;
    if (!constructor->parameterNames.isValid ||
        referenceFact.signatureDisplay != ZR_NULL) {
        ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &referenceFact);
    }
    free_resolved_call_signature(cs->state, &resolvedSignature);
    type_inference_source_constructor_member_free(cs, &temporaryConstructor);
}
