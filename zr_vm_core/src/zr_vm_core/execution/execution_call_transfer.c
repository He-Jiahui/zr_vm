#include "zr_vm_core/execution_call_transfer.h"

TZrBool ZrCore_Execution_CanForwardReturn(const SZrExecutionReturnTransfer *transfer) {
    return (TZrBool)(transfer != ZR_NULL && transfer->noAliasConflict &&
                     transfer->compatibleLayout && transfer->commitOrderPreserved &&
                     !transfer->requiresWriteback);
}

TZrBool ZrCore_Execution_CanReuseTailFrame(const SZrExecutionTailEligibility *eligibility) {
    return (TZrBool)(eligibility != ZR_NULL && eligibility->noPendingCleanup &&
                     eligibility->noEscapingFrameAlias && eligibility->compatibleContinuation &&
                     eligibility->debugPolicyAllows);
}

const TZrChar *ZrCore_Execution_TransferStatusName(EZrExecutionTransferStatus status) {
    switch (status) {
        case ZR_EXECUTION_TRANSFER_OK: return "ok";
        case ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT: return "alias-conflict";
        case ZR_EXECUTION_TRANSFER_INCOMPATIBLE_LAYOUT: return "incompatible-layout";
        case ZR_EXECUTION_TRANSFER_PENDING_CLEANUP: return "pending-cleanup";
        case ZR_EXECUTION_TRANSFER_ESCAPING_ALIAS: return "escaping-alias";
        case ZR_EXECUTION_TRANSFER_DEBUG_FRAME_REQUIRED: return "debug-frame-required";
        default: return "unknown";
    }
}
