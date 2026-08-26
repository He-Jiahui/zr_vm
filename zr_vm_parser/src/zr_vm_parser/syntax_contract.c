#include "zr_vm_parser/syntax_contract.h"

#include "zr_vm_parser/semantic.h"

#include <string.h>

static EZrCanonicalReceiverEffect syntax_callable_receiver_effect_from_contract(
        EZrMethodReceiverModifier modifier,
        EZrOwnershipQualifier qualifier) {
    if (modifier == ZR_METHOD_RECEIVER_CONST ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        return ZR_CANONICAL_RECEIVER_READONLY;
    }
    return ZR_CANONICAL_RECEIVER_MUTABLE;
}

EZrCanonicalReceiverEffect
ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_CANONICAL_RECEIVER_NONE;
    }

    switch (declaration->type) {
        case ZR_AST_STRUCT_METHOD:
            if (declaration->data.structMethod.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return syntax_callable_receiver_effect_from_contract(
                    declaration->data.structMethod.receiverModifier,
                    declaration->data.structMethod.receiverQualifier);
        case ZR_AST_CLASS_METHOD:
            if (declaration->data.classMethod.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return syntax_callable_receiver_effect_from_contract(
                    declaration->data.classMethod.receiverModifier,
                    declaration->data.classMethod.receiverQualifier);
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return syntax_callable_receiver_effect_from_contract(
                    declaration->data.interfaceMethodSignature.receiverModifier,
                    ZR_OWNERSHIP_QUALIFIER_NONE);
        case ZR_AST_CLASS_PROPERTY:
            if (declaration->data.classProperty.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return declaration->data.classProperty.modifier != ZR_NULL &&
                           declaration->data.classProperty.modifier->type ==
                                   ZR_AST_PROPERTY_GET
                           ? ZR_CANONICAL_RECEIVER_READONLY
                           : ZR_CANONICAL_RECEIVER_MUTABLE;
        case ZR_AST_STRUCT_META_FUNCTION:
            return declaration->data.structMetaFunction.isStatic
                           ? ZR_CANONICAL_RECEIVER_NONE
                           : ZR_CANONICAL_RECEIVER_MUTABLE;
        case ZR_AST_CLASS_META_FUNCTION:
            if (declaration->data.classMetaFunction.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return syntax_callable_receiver_effect_from_contract(
                    declaration->data.classMetaFunction.receiverModifier,
                    ZR_OWNERSHIP_QUALIFIER_NONE);
        case ZR_AST_INTERFACE_META_SIGNATURE:
            return ZR_CANONICAL_RECEIVER_MUTABLE;
        default:
            return ZR_CANONICAL_RECEIVER_NONE;
    }
}

TZrUInt32 ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_CANONICAL_CALLABLE_EFFECT_NONE;
    }

    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.isAsync
                           ? ZR_CANONICAL_CALLABLE_EFFECT_ASYNC
                           : ZR_CANONICAL_CALLABLE_EFFECT_NONE;
        case ZR_AST_LAMBDA_EXPRESSION:
            return declaration->data.lambdaExpression.isAsync
                           ? ZR_CANONICAL_CALLABLE_EFFECT_ASYNC
                           : ZR_CANONICAL_CALLABLE_EFFECT_NONE;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.isAsync
                           ? ZR_CANONICAL_CALLABLE_EFFECT_ASYNC
                           : ZR_CANONICAL_CALLABLE_EFFECT_NONE;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.isAsync
                           ? ZR_CANONICAL_CALLABLE_EFFECT_ASYNC
                           : ZR_CANONICAL_CALLABLE_EFFECT_NONE;
        default:
            return ZR_CANONICAL_CALLABLE_EFFECT_NONE;
    }
}

TZrBool ZrParser_SyntaxParameter_Normalize(
        SZrSemanticContext *context,
        const SZrParameter *parameter,
        TZrTypeId valueTypeId,
        SZrCanonicalParameterContract *outContract) {
    EZrCanonicalRefAccess access = ZR_CANONICAL_REF_WRITABLE;

    if (context == ZR_NULL || parameter == ZR_NULL || outContract == ZR_NULL ||
        valueTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(outContract, 0, sizeof(*outContract));
    outContract->typeId = valueTypeId;
    outContract->passingForm = ZR_CANONICAL_PASSING_VALUE;
    outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    outContract->entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    outContract->exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    outContract->acceptsTemporary = ZR_TRUE;
    outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;

    switch (parameter->sourcePassingForm) {
        case ZR_PARAMETER_SOURCE_VALUE:
            return ZR_TRUE;
        case ZR_PARAMETER_SOURCE_IN:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_IN;
            break;
        case ZR_PARAMETER_SOURCE_REF:
            outContract->passingForm = ZR_CANONICAL_PASSING_REF;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_REF_READONLY:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_REF_READONLY;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_SCOPED_REF:
            outContract->passingForm = ZR_CANONICAL_PASSING_REF;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_REF_READONLY;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_OUT:
            outContract->passingForm = ZR_CANONICAL_PASSING_OUT;
            outContract->entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
            outContract->exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;
            break;
        default:
            return ZR_FALSE;
    }

    outContract->typeId = ZrParser_CanonicalType_InternRef(context, valueTypeId, access);
    return outContract->typeId != ZR_SEMANTIC_ID_INVALID;
}

TZrTypeId ZrParser_SyntaxCallable_Intern(
        SZrSemanticContext *context,
        const SZrAstNodeArray *parameters,
        const TZrTypeId *parameterTypeIds,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags) {
    SZrArray contracts;
    TZrTypeId result;

    if (context == ZR_NULL || returnTypeId == ZR_SEMANTIC_ID_INVALID ||
        (parameters != ZR_NULL && parameters->count > 0u && parameterTypeIds == ZR_NULL)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    ZrCore_Array_Init(context->state, &contracts, sizeof(SZrCanonicalParameterContract),
                      parameters != ZR_NULL && parameters->count > 0u ? parameters->count : 1u);
    if (parameters != ZR_NULL) {
        for (TZrSize index = 0u; index < parameters->count; index++) {
            SZrCanonicalParameterContract contract;
            SZrAstNode *node = parameters->nodes[index];
            if (node == ZR_NULL || node->type != ZR_AST_PARAMETER ||
                !ZrParser_SyntaxParameter_Normalize(
                        context, &node->data.parameter, parameterTypeIds[index], &contract)) {
                ZrCore_Array_Free(context->state, &contracts);
                return ZR_SEMANTIC_ID_INVALID;
            }
            ZrCore_Array_Push(context->state, &contracts, &contract);
        }
    }

    result = ZrParser_CanonicalType_InternFunction(
            context,
            (const SZrCanonicalParameterContract *)contracts.head,
            contracts.length,
            returnTypeId,
            receiverEffect,
            effectFlags);
    ZrCore_Array_Free(context->state, &contracts);
    return result;
}

static const SZrAstNodeArray *syntax_callable_parameters(
        const SZrAstNode *declaration,
        TZrBool *outRecognized) {
    if (outRecognized != ZR_NULL) {
        *outRecognized = ZR_TRUE;
    }
    if (declaration == ZR_NULL) {
        if (outRecognized != ZR_NULL) {
            *outRecognized = ZR_FALSE;
        }
        return ZR_NULL;
    }

    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.params;
        case ZR_AST_LAMBDA_EXPRESSION:
            return declaration->data.lambdaExpression.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.params;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return declaration->data.externDelegateDeclaration.params;
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
            if (outRecognized != ZR_NULL) {
                *outRecognized = ZR_FALSE;
            }
            return ZR_NULL;
    }
}

TZrTypeId ZrParser_SyntaxCallable_RefineFromDeclaration(
        SZrSemanticContext *context,
        const SZrAstNode *declaration,
        TZrTypeId callableTypeId) {
    const SZrCanonicalTypeNode *callableType;
    const SZrAstNodeArray *parameters;
    SZrArray valueTypeIds;
    EZrCanonicalReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;
    TZrTypeId result;
    TZrBool recognized = ZR_FALSE;
    TZrSize index;

    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    callableType = ZrParser_CanonicalType_Find(context, callableTypeId);
    if (callableType == ZR_NULL || callableType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    parameters = syntax_callable_parameters(declaration, &recognized);
    if (!recognized) {
        return callableTypeId;
    }
    receiverEffect = ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(
            declaration);
    effectFlags = ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(
            declaration);
    if (parameters == ZR_NULL) {
        if (callableType->data.function.parameterContracts.length != 0U) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        if (receiverEffect == callableType->data.function.receiverEffect &&
            effectFlags == callableType->data.function.effectFlags) {
            return callableTypeId;
        }
        return ZrParser_CanonicalType_InternFunction(
                context,
                ZR_NULL,
                0U,
                callableType->data.function.returnTypeId,
                receiverEffect,
                effectFlags);
    }
    if (parameters->count != callableType->data.function.parameterContracts.length) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    ZrCore_Array_Init(
            context->state,
            &valueTypeIds,
            sizeof(TZrTypeId),
            parameters->count > 0U ? parameters->count : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0U; index < parameters->count; index++) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&callableType->data.function.parameterContracts,
                        index);
        const SZrCanonicalTypeNode *contractType;
        const SZrAstNode *parameterNode = parameters->nodes[index];
        TZrTypeId valueTypeId;

        if (contract == ZR_NULL || parameterNode == ZR_NULL ||
            parameterNode->type != ZR_AST_PARAMETER) {
            ZrCore_Array_Free(context->state, &valueTypeIds);
            return ZR_SEMANTIC_ID_INVALID;
        }
        valueTypeId = contract->typeId;
        if (parameterNode->data.parameter.sourcePassingForm != ZR_PARAMETER_SOURCE_VALUE) {
            contractType = ZrParser_CanonicalType_Find(context, contract->typeId);
            if (contractType == ZR_NULL || contractType->kind != ZR_CANONICAL_TYPE_REF) {
                ZrCore_Array_Free(context->state, &valueTypeIds);
                return ZR_SEMANTIC_ID_INVALID;
            }
            valueTypeId = contractType->data.refType.pointeeTypeId;
        }
        ZrCore_Array_Push(context->state, &valueTypeIds, &valueTypeId);
    }

    result = ZrParser_SyntaxCallable_Intern(
            context,
            parameters,
            (const TZrTypeId *)valueTypeIds.head,
            callableType->data.function.returnTypeId,
            receiverEffect,
            effectFlags);
    ZrCore_Array_Free(context->state, &valueTypeIds);
    return result;
}
