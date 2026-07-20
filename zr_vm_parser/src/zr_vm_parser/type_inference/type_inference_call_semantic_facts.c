#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_contract.h"
#include "zr_vm_parser/type_inference.h"

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

static const SZrAstNodeArray *type_inference_call_parameters(
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

static SZrString *type_inference_callable_signature_display(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrAstNodeArray *parameters,
        TZrTypeId callTypeId) {
    const SZrCanonicalTypeNode *functionType;
    TZrChar buffer[1024];
    TZrChar typeBuffer[256];
    const TZrChar *receiverPrefix = "";
    TZrSize offset = 0u;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
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
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset,
                                          ZrCore_String_GetNativeString(name)) ||
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
    return type_inference_callable_signature_display(
            cs,
            functionInfo->name,
            type_inference_call_parameters(functionInfo->declarationNode),
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
                                    : fact.range;
    fact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    fact.symbolId = funcTypeInfo->symbolId;
    fact.typeId = callTypeId;
    fact.name = funcTypeInfo->name;
    fact.signatureDisplay = type_inference_call_signature_display(cs, funcTypeInfo, callTypeId);
    fact.isResolved = ZR_TRUE;
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

static TZrSymbolId type_inference_member_symbol_id(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        TZrTypeId callTypeId) {
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0U; index < cs->semanticContext->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        &cs->semanticContext->symbols, index);
        if (symbol != ZR_NULL &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            symbol->astNode == memberInfo->declarationNode) {
            return symbol->id;
        }
    }
    return ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            memberInfo->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callTypeId,
            ZR_SEMANTIC_ID_INVALID,
            memberInfo->declarationNode,
            memberInfo->declarationNode->location);
}

void type_inference_record_member_call_reference_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        const SZrTypeMemberInfo *memberInfo,
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
                                    : fact.range;
    fact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    fact.symbolId = symbolId;
    fact.typeId = callTypeId;
    fact.name = memberInfo->name;
    fact.signatureDisplay = type_inference_callable_signature_display(
            cs,
            memberInfo->name,
            type_inference_call_parameters(memberInfo->declarationNode),
            callTypeId);
    fact.isResolved = symbolId != ZR_SEMANTIC_ID_INVALID;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}
