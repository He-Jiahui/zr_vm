#ifndef ZR_VM_CORE_EXECUTION_CALL_TRANSFER_H
#define ZR_VM_CORE_EXECUTION_CALL_TRANSFER_H

#include "zr_vm_core/conf.h"

typedef enum EZrExecutionTransferStatus {
    ZR_EXECUTION_TRANSFER_OK = 0,
    ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT,
    ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT,
    ZR_EXECUTION_TRANSFER_INCOMPATIBLE_LAYOUT,
    ZR_EXECUTION_TRANSFER_PENDING_CLEANUP,
    ZR_EXECUTION_TRANSFER_ESCAPING_ALIAS,
    ZR_EXECUTION_TRANSFER_DEBUG_FRAME_REQUIRED
} EZrExecutionTransferStatus;

typedef struct SZrExecutionReturnTransfer {
    TZrBool noAliasConflict;
    TZrBool compatibleLayout;
    TZrBool commitOrderPreserved;
    TZrBool requiresWriteback;
} SZrExecutionReturnTransfer;

typedef struct SZrExecutionTailEligibility {
    TZrBool noPendingCleanup;
    TZrBool noEscapingFrameAlias;
    TZrBool compatibleContinuation;
    TZrBool debugPolicyAllows;
} SZrExecutionTailEligibility;

ZR_CORE_API TZrBool ZrCore_Execution_CanForwardReturn(
        const SZrExecutionReturnTransfer *transfer);
ZR_CORE_API TZrBool ZrCore_Execution_CanReuseTailFrame(
        const SZrExecutionTailEligibility *eligibility);
ZR_CORE_API const TZrChar *ZrCore_Execution_TransferStatusName(
        EZrExecutionTransferStatus status);

#endif
