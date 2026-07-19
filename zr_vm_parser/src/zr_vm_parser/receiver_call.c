#include "zr_vm_parser/receiver_call.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_system.h"

#include <string.h>

static const SZrCanonicalTypeNode *receiver_call_unwrap_view(
        const SZrSemanticContext *context,
        TZrTypeId *typeId,
        TZrBool *readonlyView) {
    const SZrCanonicalTypeNode *node;

    node = ZrParser_CanonicalType_Find(context, *typeId);
    while (node != ZR_NULL &&
           (node->kind == ZR_CANONICAL_TYPE_READONLY_VIEW ||
            node->kind == ZR_CANONICAL_TYPE_NULLABLE)) {
        if (node->kind == ZR_CANONICAL_TYPE_READONLY_VIEW) {
            *readonlyView = ZR_TRUE;
        }
        *typeId = node->data.target.targetTypeId;
        node = ZrParser_CanonicalType_Find(context, *typeId);
    }
    return node;
}

TZrBool ZrParser_ReceiverCall_Analyze(
        const SZrSemanticContext *context,
        const SZrReceiverCallRequest *request,
        SZrReceiverCallDecision *decision) {
    const SZrCanonicalTypeNode *node;
    TZrTypeId typeId;
    TZrBool readonlyCapability = ZR_FALSE;
    TZrBool writableReference = ZR_FALSE;
    TZrBool weakOwner = ZR_FALSE;

    if (context == ZR_NULL || request == ZR_NULL || decision == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(decision, 0, sizeof(*decision));
    decision->loanAccess = request->receiverEffect == ZR_CANONICAL_RECEIVER_MUTABLE
                                   ? ZR_SEMANTIC_LOAN_MUTABLE
                                   : ZR_SEMANTIC_LOAN_SHARED;
    if (request->receiverTypeId == ZR_SEMANTIC_ID_INVALID ||
        request->receiverEffect < ZR_CANONICAL_RECEIVER_NONE ||
        request->receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
        request->dispatchKind < ZR_RECEIVER_DISPATCH_CLASS ||
        request->dispatchKind >= ZR_RECEIVER_DISPATCH_ENUM_MAX) {
        decision->diagnostic = ZR_RECEIVER_CALL_DIAGNOSTIC_INVALID_CONTRACT;
        return ZR_TRUE;
    }
    if (request->receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        decision->allowed = ZR_TRUE;
        return ZR_TRUE;
    }

    typeId = request->receiverTypeId;
    node = receiver_call_unwrap_view(context, &typeId, &readonlyCapability);
    if (node == ZR_NULL) {
        decision->diagnostic = ZR_RECEIVER_CALL_DIAGNOSTIC_INVALID_CONTRACT;
        return ZR_TRUE;
    }
    if (node->kind == ZR_CANONICAL_TYPE_REF) {
        writableReference =
                node->data.refType.access == ZR_CANONICAL_REF_WRITABLE;
        readonlyCapability = (TZrBool)(readonlyCapability || !writableReference);
    } else if (node->kind == ZR_CANONICAL_TYPE_OWNER) {
        decision->requiresOwnerAutoDeref = ZR_TRUE;
        if (node->data.owner.ownerKind == ZR_CANONICAL_OWNER_SHARED) {
            readonlyCapability = ZR_TRUE;
        } else if (node->data.owner.ownerKind == ZR_CANONICAL_OWNER_WEAK) {
            weakOwner = ZR_TRUE;
            readonlyCapability = ZR_TRUE;
        } else if (node->data.owner.ownerKind != ZR_CANONICAL_OWNER_UNIQUE) {
            decision->diagnostic =
                    ZR_RECEIVER_CALL_DIAGNOSTIC_OWNER_DEREF_UNAVAILABLE;
            return ZR_TRUE;
        }
    }

    if (weakOwner) {
        decision->diagnostic =
                ZR_RECEIVER_CALL_DIAGNOSTIC_OWNER_DEREF_UNAVAILABLE;
        return ZR_TRUE;
    }
    if (request->receiverEffect == ZR_CANONICAL_RECEIVER_READONLY) {
        decision->allowed = ZR_TRUE;
        return ZR_TRUE;
    }
    if (readonlyCapability) {
        decision->diagnostic =
                ZR_RECEIVER_CALL_DIAGNOSTIC_READONLY_CAPABILITY;
        return ZR_TRUE;
    }
    if (request->dispatchKind == ZR_RECEIVER_DISPATCH_STRUCT &&
        !request->receiverIsAddressable && !writableReference &&
        !decision->requiresOwnerAutoDeref) {
        decision->diagnostic =
                ZR_RECEIVER_CALL_DIAGNOSTIC_REQUIRES_ADDRESSABLE_PLACE;
        return ZR_TRUE;
    }

    decision->allowed = ZR_TRUE;
    decision->usesTwoPhaseBorrow =
            request->compilerGeneratedReceiverBorrow;
    return ZR_TRUE;
}

TZrBool ZrParser_ReceiverCall_AnalyzeInferred(
        SZrSemanticContext *context,
        const SZrInferredType *receiverType,
        EZrCanonicalReceiverEffect receiverEffect,
        EZrReceiverDispatchKind dispatchKind,
        TZrBool receiverIsAddressable,
        TZrBool compilerGeneratedReceiverBorrow,
        SZrReceiverCallDecision *decision) {
    SZrReceiverCallRequest request;

    if (context == ZR_NULL || receiverType == ZR_NULL || decision == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&request, 0, sizeof(request));
    request.receiverTypeId =
            ZrParser_CanonicalType_FromInferred(context, receiverType);
    request.receiverEffect = receiverEffect;
    request.dispatchKind = dispatchKind;
    request.receiverIsAddressable = receiverIsAddressable;
    request.compilerGeneratedReceiverBorrow = compilerGeneratedReceiverBorrow;
    return ZrParser_ReceiverCall_Analyze(context, &request, decision);
}

const TZrChar *ZrParser_ReceiverCall_DiagnosticMessage(
        EZrReceiverCallDiagnostic diagnostic) {
    switch (diagnostic) {
        case ZR_RECEIVER_CALL_DIAGNOSTIC_READONLY_CAPABILITY:
            return "Readonly receiver capability cannot call a writable member";
        case ZR_RECEIVER_CALL_DIAGNOSTIC_REQUIRES_ADDRESSABLE_PLACE:
            return "Writable struct receiver requires an addressable Place";
        case ZR_RECEIVER_CALL_DIAGNOSTIC_OWNER_DEREF_UNAVAILABLE:
            return "Receiver owner does not provide the required auto-deref capability";
        case ZR_RECEIVER_CALL_DIAGNOSTIC_INVALID_CONTRACT:
            return "Receiver call has an invalid canonical contract";
        case ZR_RECEIVER_CALL_DIAGNOSTIC_NONE:
        default:
            return "Receiver call is not permitted";
    }
}
