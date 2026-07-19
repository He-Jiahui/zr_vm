#include "compiler_internal.h"
#include "zr_vm_parser/receiver_call.h"

static EZrCanonicalReceiverEffect compiler_receiver_effect_from_contract(
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

EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.receiverQualifier;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.receiverQualifier;
        default:
            return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
}

EZrCanonicalReceiverEffect get_member_receiver_effect(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_CANONICAL_RECEIVER_NONE;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            if (node->data.structMethod.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return compiler_receiver_effect_from_contract(
                    node->data.structMethod.receiverModifier,
                    node->data.structMethod.receiverQualifier);
        case ZR_AST_CLASS_METHOD:
            if (node->data.classMethod.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return compiler_receiver_effect_from_contract(
                    node->data.classMethod.receiverModifier,
                    node->data.classMethod.receiverQualifier);
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return compiler_receiver_effect_from_contract(
                    node->data.interfaceMethodSignature.receiverModifier,
                    ZR_OWNERSHIP_QUALIFIER_NONE);
        case ZR_AST_CLASS_PROPERTY:
            if (node->data.classProperty.isStatic) {
                return ZR_CANONICAL_RECEIVER_NONE;
            }
            return node->data.classProperty.modifier != ZR_NULL &&
                           node->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET
                           ? ZR_CANONICAL_RECEIVER_READONLY
                           : ZR_CANONICAL_RECEIVER_MUTABLE;
        case ZR_AST_STRUCT_META_FUNCTION:
            return node->data.structMetaFunction.isStatic
                           ? ZR_CANONICAL_RECEIVER_NONE
                           : ZR_CANONICAL_RECEIVER_MUTABLE;
        case ZR_AST_CLASS_META_FUNCTION:
            return node->data.classMetaFunction.isStatic
                           ? ZR_CANONICAL_RECEIVER_NONE
                           : ZR_CANONICAL_RECEIVER_MUTABLE;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            return ZR_CANONICAL_RECEIVER_MUTABLE;
        default:
            return ZR_CANONICAL_RECEIVER_NONE;
    }
}

EZrOwnershipQualifier get_implicit_this_ownership_qualifier(
        EZrOwnershipQualifier receiverQualifier) {
    if (receiverQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        return ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }
    return receiverQualifier;
}

TZrBool compiler_receiver_effect_can_implement(
        EZrCanonicalReceiverEffect requiredEffect,
        EZrCanonicalReceiverEffect implementationEffect) {
    if (requiredEffect == ZR_CANONICAL_RECEIVER_READONLY) {
        return implementationEffect == ZR_CANONICAL_RECEIVER_READONLY;
    }
    if (requiredEffect == ZR_CANONICAL_RECEIVER_MUTABLE) {
        return implementationEffect == ZR_CANONICAL_RECEIVER_MUTABLE ||
               implementationEffect == ZR_CANONICAL_RECEIVER_READONLY;
    }
    return requiredEffect == implementationEffect;
}

static EZrReceiverDispatchKind compiler_receiver_dispatch_kind(
        SZrCompilerState *cs,
        const SZrInferredType *receiverType,
        const SZrTypeMemberInfo *memberInfo) {
    ZR_UNUSED_PARAMETER(cs);
    if (memberInfo != ZR_NULL &&
        (memberInfo->metadataToken != 0U ||
         memberInfo->signatureToken != 0U)) {
        return ZR_RECEIVER_DISPATCH_NATIVE;
    }
    if (memberInfo != ZR_NULL &&
        (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE) != 0U) {
        return ZR_RECEIVER_DISPATCH_OVERRIDE;
    }
    if ((receiverType != ZR_NULL && receiverType->elementTypes.length > 0U) ||
        (memberInfo != ZR_NULL && memberInfo->genericParameters.length > 0U)) {
        return ZR_RECEIVER_DISPATCH_GENERIC;
    }
    if (memberInfo != ZR_NULL && memberInfo->declarationNode != ZR_NULL &&
        (memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_METHOD_SIGNATURE ||
         memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_META_SIGNATURE ||
         memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_PROPERTY_SIGNATURE)) {
        return ZR_RECEIVER_DISPATCH_INTERFACE;
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->memberType == ZR_AST_STRUCT_METHOD) {
        return ZR_RECEIVER_DISPATCH_STRUCT;
    }
    return ZR_RECEIVER_DISPATCH_CLASS;
}

TZrBool compiler_validate_receiver_call(
        SZrCompilerState *cs,
        SZrAstNode *receiverNode,
        SZrString *receiverTypeName,
        EZrOwnershipQualifier receiverQualifier,
        const SZrTypeMemberInfo *memberInfo,
        TZrBool receiverIsAddressable,
        SZrFileRange location) {
    SZrInferredType receiverType;
    SZrReceiverCallDecision decision;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || memberInfo->isStatic ||
        memberInfo->receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        return ZR_TRUE;
    }
    ZrParser_InferredType_InitFull(
            cs->state,
            &receiverType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            receiverTypeName);
    if (receiverNode != ZR_NULL) {
        SZrInferredType inferredType;
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_ExpressionType_Infer(cs, receiverNode, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &receiverType);
            ZrParser_InferredType_Init(cs->state, &receiverType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(cs->state, &receiverType, &inferredType);
        }
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }
    if (receiverType.typeName == ZR_NULL) {
        receiverType.typeName = receiverTypeName;
    }
    if (receiverType.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        receiverType.ownershipQualifier = receiverQualifier;
    }
    memset(&decision, 0, sizeof(decision));
    if (!ZrParser_ReceiverCall_AnalyzeInferred(
                cs->semanticContext,
                &receiverType,
                memberInfo->receiverEffect,
                compiler_receiver_dispatch_kind(cs, &receiverType, memberInfo),
                receiverIsAddressable,
                ZR_TRUE,
                &decision) ||
        !decision.allowed) {
        ZrParser_InferredType_Free(cs->state, &receiverType);
        ZrParser_Compiler_Error(
                cs,
                ZrParser_ReceiverCall_DiagnosticMessage(decision.diagnostic),
                location);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &receiverType);
    return ZR_TRUE;
}
