#ifndef ZR_VM_PARSER_RECEIVER_CALL_H
#define ZR_VM_PARSER_RECEIVER_CALL_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_ir.h"

struct SZrSemanticContext;
struct SZrInferredType;

typedef enum EZrReceiverDispatchKind {
    ZR_RECEIVER_DISPATCH_CLASS = 0,
    ZR_RECEIVER_DISPATCH_STRUCT,
    ZR_RECEIVER_DISPATCH_INTERFACE,
    ZR_RECEIVER_DISPATCH_OVERRIDE,
    ZR_RECEIVER_DISPATCH_GENERIC,
    ZR_RECEIVER_DISPATCH_DYNAMIC,
    ZR_RECEIVER_DISPATCH_NATIVE,
    ZR_RECEIVER_DISPATCH_ENUM_MAX
} EZrReceiverDispatchKind;

typedef enum EZrReceiverCallDiagnostic {
    ZR_RECEIVER_CALL_DIAGNOSTIC_NONE = 0,
    ZR_RECEIVER_CALL_DIAGNOSTIC_READONLY_CAPABILITY,
    ZR_RECEIVER_CALL_DIAGNOSTIC_REQUIRES_ADDRESSABLE_PLACE,
    ZR_RECEIVER_CALL_DIAGNOSTIC_OWNER_DEREF_UNAVAILABLE,
    ZR_RECEIVER_CALL_DIAGNOSTIC_INVALID_CONTRACT
} EZrReceiverCallDiagnostic;

typedef struct SZrReceiverCallRequest {
    TZrTypeId receiverTypeId;
    EZrCanonicalReceiverEffect receiverEffect;
    EZrReceiverDispatchKind dispatchKind;
    TZrBool receiverIsAddressable;
    TZrBool compilerGeneratedReceiverBorrow;
} SZrReceiverCallRequest;

typedef struct SZrReceiverCallDecision {
    TZrBool allowed;
    EZrSemanticLoanAccess loanAccess;
    TZrBool requiresOwnerAutoDeref;
    TZrBool usesTwoPhaseBorrow;
    EZrReceiverCallDiagnostic diagnostic;
} SZrReceiverCallDecision;

ZR_PARSER_API TZrBool ZrParser_ReceiverCall_Analyze(
        const struct SZrSemanticContext *context,
        const SZrReceiverCallRequest *request,
        SZrReceiverCallDecision *decision);
ZR_PARSER_API TZrBool ZrParser_ReceiverCall_AnalyzeInferred(
        struct SZrSemanticContext *context,
        const struct SZrInferredType *receiverType,
        EZrCanonicalReceiverEffect receiverEffect,
        EZrReceiverDispatchKind dispatchKind,
        TZrBool receiverIsAddressable,
        TZrBool compilerGeneratedReceiverBorrow,
        SZrReceiverCallDecision *decision);
ZR_PARSER_API const TZrChar *ZrParser_ReceiverCall_DiagnosticMessage(
        EZrReceiverCallDiagnostic diagnostic);

#endif /* ZR_VM_PARSER_RECEIVER_CALL_H */
