#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
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

static const TZrChar *type_inference_call_passing_prefix(EZrCanonicalPassingForm passingForm) {
    switch (passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF: return "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY: return "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
        default: return "";
    }
}

static SZrString *type_inference_call_signature_display(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *functionInfo,
        TZrTypeId callTypeId) {
    const SZrCanonicalTypeNode *functionType;
    SZrAstNodeArray *parameters = ZR_NULL;
    TZrChar buffer[1024];
    TZrChar typeBuffer[256];
    TZrSize offset = 0u;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || functionInfo == ZR_NULL ||
        functionInfo->name == ZR_NULL || callTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    functionType = ZrParser_CanonicalType_Find(cs->semanticContext, callTypeId);
    if (functionType == ZR_NULL || functionType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_NULL;
    }
    if (functionInfo->declarationNode != ZR_NULL &&
        functionInfo->declarationNode->type == ZR_AST_FUNCTION_DECLARATION) {
        parameters = functionInfo->declarationNode->data.functionDeclaration.params;
    }
    buffer[0] = '\0';
    if (!type_inference_call_label_append(buffer, sizeof(buffer), &offset,
                                          ZrCore_String_GetNativeString(functionInfo->name)) ||
        !type_inference_call_label_append(buffer, sizeof(buffer), &offset, "(")) {
        return ZR_NULL;
    }
    for (index = 0u; index < functionType->data.function.parameterContracts.length; ++index) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->data.function.parameterContracts, index);
        const TZrChar *parameterName = ZR_NULL;
        if (contract == ZR_NULL ||
            !ZrParser_CanonicalType_Format(
                    cs->semanticContext, contract->typeId, typeBuffer, sizeof(typeBuffer))) {
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
        if ((index > 0u && !type_inference_call_label_append(
                                  buffer, sizeof(buffer), &offset, ", ")) ||
            !type_inference_call_label_append(
                    buffer, sizeof(buffer), &offset,
                    type_inference_call_passing_prefix(contract->passingForm))) {
            return ZR_NULL;
        }
        if (parameterName != ZR_NULL && parameterName[0] != '\0') {
            if (!type_inference_call_label_append(buffer, sizeof(buffer), &offset, parameterName) ||
                !type_inference_call_label_append(buffer, sizeof(buffer), &offset, ": ")) {
                return ZR_NULL;
            }
        }
        if (!type_inference_call_label_append(buffer, sizeof(buffer), &offset, typeBuffer)) {
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

static TZrTypeId type_inference_resolved_call_type_id(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *functionInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    const SZrCanonicalTypeNode *declaredFunction;
    EZrCanonicalReceiverEffect receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    TZrUInt32 effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_NONE;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || functionInfo == ZR_NULL ||
        resolvedSignature == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    declaredFunction = ZrParser_CanonicalType_Find(cs->semanticContext, functionInfo->typeId);
    if (declaredFunction != ZR_NULL && declaredFunction->kind == ZR_CANONICAL_TYPE_FUNCTION) {
        receiverEffect = declaredFunction->data.function.receiverEffect;
        effectFlags = declaredFunction->data.function.effectFlags;
    }
    return ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &resolvedSignature->parameterTypes,
            &resolvedSignature->parameterPassingModes,
            &resolvedSignature->returnType,
            receiverEffect,
            effectFlags);
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
