#include "zr_vm_parser/exec_ir_call_transfer.h"

#include <stdlib.h>
#include <string.h>

static void transfer_diag(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
                          TZrUInt32 valueId) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->actualVersion = valueId;
    }
}

void ZrParser_ExecIr_CallTransferPlanInit(SZrExecIrCallTransferPlan *plan) {
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
    }
}

void ZrParser_ExecIr_CallTransferPlanFree(SZrExecIrCallTransferPlan *plan) {
    if (plan == ZR_NULL) return;
    free(plan->kinds);
    free(plan->sourceSlots);
    free(plan->targetSlots);
    memset(plan, 0, sizeof(*plan));
}

static EZrExecIrTransferKind transfer_kind(const SZrExecIrCallTransferValue *value) {
    if (value->ownership == ZR_EXEC_IR_OWNERSHIP_BORROWED) return ZR_EXEC_IR_TRANSFER_BORROW;
    if (value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE) return ZR_EXEC_IR_TRANSFER_MOVE;
    if (value->slotClass == ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN) return ZR_EXEC_IR_TRANSFER_SPAN_COPY;
    if (value->slotClass == ZR_EXEC_IR_PACKED_SLOT_SCALAR) return ZR_EXEC_IR_TRANSFER_SCALAR_COPY;
    return ZR_EXEC_IR_TRANSFER_BOXED_BRIDGE;
}

TZrBool ZrParser_ExecIr_PrepareCallTransfer(const SZrExecIrCallTransferRequest *request,
                                             SZrExecIrCallTransferPlan *plan,
                                             SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrCallTransferPlan candidate;
    TZrUInt32 i;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (request == ZR_NULL || plan == ZR_NULL ||
        (request->valueCount != 0u && request->values == ZR_NULL) ||
        request->sourceLayout == ZR_NULL || request->targetLayout == ZR_NULL) {
        transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_CallTransferPlanInit(&candidate);
    candidate.valueCount = request->valueCount;
    candidate.compatibleLayout = (TZrBool)(request->sourceLayout->frame.parameterPrefixCount ==
                                           request->targetLayout->frame.parameterPrefixCount);
    candidate.noAliasConflict = (TZrBool)!request->returnMayAlias;
    candidate.forwardReturn = (TZrBool)(candidate.compatibleLayout && candidate.noAliasConflict &&
                                        !request->requiresWriteback);
    if (request->valueCount != 0u) {
        candidate.kinds = (EZrExecIrTransferKind *)malloc(sizeof(*candidate.kinds) * request->valueCount);
        candidate.sourceSlots = (TZrUInt32 *)malloc(sizeof(*candidate.sourceSlots) * request->valueCount);
        candidate.targetSlots = (TZrUInt32 *)malloc(sizeof(*candidate.targetSlots) * request->valueCount);
        if (candidate.kinds == ZR_NULL || candidate.sourceSlots == ZR_NULL || candidate.targetSlots == ZR_NULL) {
            transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
            ZrParser_ExecIr_CallTransferPlanFree(&candidate);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < request->valueCount; ++i) {
        const SZrExecIrCallTransferValue *value = &request->values[i];
        if (value->valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            value->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
            value->slotClass >= ZR_EXEC_IR_PACKED_SLOT_CLASS_COUNT ||
            value->sourceSlot >= request->sourceLayout->frame.storageSlotCount ||
            value->targetSlot >= request->targetLayout->frame.storageSlotCount) {
            transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i);
            ZrParser_ExecIr_CallTransferPlanFree(&candidate);
            return ZR_FALSE;
        }
        candidate.kinds[i] = transfer_kind(value);
        candidate.sourceSlots[i] = value->sourceSlot;
        candidate.targetSlots[i] = value->targetSlot;
    }
    ZrParser_ExecIr_CallTransferPlanFree(plan);
    *plan = candidate;
    return ZR_TRUE;
}
