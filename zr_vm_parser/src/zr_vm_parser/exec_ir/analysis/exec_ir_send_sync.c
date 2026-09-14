#include "zr_vm_parser/exec_ir_send_sync.h"

#include <string.h>

static void send_sync_fail(SZrExecIrSendSyncSummary *summary,
                           EZrExecIrSendSyncReason reason,
                           const SZrExecIrValue *value) {
    if (summary == ZR_NULL || summary->reason != ZR_EXEC_IR_SEND_SYNC_REASON_NONE) {
        return;
    }
    summary->reason = reason;
    if (value != ZR_NULL) {
        summary->reasonValueId = value->id;
        summary->reasonInstructionId = value->definition;
    }
}

static TZrExecIrSourceId send_sync_source_for_value(
        const SZrExecIrFunction *function,
        TZrExecIrInstructionId instructionId) {
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount || function->instructions == ZR_NULL) {
        return 0u;
    }
    return function->instructions[instructionId - 1u].sourceId;
}

TZrBool ZrParser_ExecIr_InferSendSync(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (summary != ZR_NULL) {
        memset(summary, 0, sizeof(*summary));
        summary->send = ZR_TRUE;
        summary->sync = ZR_TRUE;
    }
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (function == ZR_NULL || summary == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL)) {
        if (summary != ZR_NULL) {
            summary->send = ZR_FALSE;
            summary->sync = ZR_FALSE;
            summary->reason = ZR_EXEC_IR_SEND_SYNC_REASON_INVALID_INPUT;
        }
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
            diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
        }
        return ZR_FALSE;
    }
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        switch (value->ownership) {
            case ZR_EXEC_IR_OWNERSHIP_BORROWED:
                summary->send = ZR_FALSE;
                summary->sync = ZR_FALSE;
                send_sync_fail(summary, ZR_EXEC_IR_SEND_SYNC_REASON_BORROWED_ALIAS, value);
                break;
            case ZR_EXEC_IR_OWNERSHIP_UNIQUE:
                summary->sync = ZR_FALSE;
                send_sync_fail(summary, ZR_EXEC_IR_SEND_SYNC_REASON_UNIQUE_MUTABLE, value);
                break;
            case ZR_EXEC_IR_OWNERSHIP_GC:
            case ZR_EXEC_IR_OWNERSHIP_SHARED:
                break;
            case ZR_EXEC_IR_OWNERSHIP_UNKNOWN:
            default:
                summary->send = ZR_FALSE;
                summary->sync = ZR_FALSE;
                send_sync_fail(summary, ZR_EXEC_IR_SEND_SYNC_REASON_UNKNOWN_OWNERSHIP, value);
                break;
        }
        if (summary->reasonValueId == value->id && summary->reasonInstructionId == value->definition) {
            summary->reasonSourceId = send_sync_source_for_value(function, value->definition);
        }
    }
    if (summary->reason != ZR_EXEC_IR_SEND_SYNC_REASON_NONE && diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->functionToken = function->functionToken;
        diagnostic->instructionId = summary->reasonInstructionId;
        diagnostic->sourceId = summary->reasonSourceId;
    }
    return (TZrBool)(summary->send || summary->sync);
}

TZrBool ZrParser_ExecIr_InferSend(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    if (!ZrParser_ExecIr_InferSendSync(function, summary, diagnostic)) {
        return ZR_FALSE;
    }
    return summary != ZR_NULL ? summary->send : ZR_FALSE;
}

TZrBool ZrParser_ExecIr_InferSync(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    if (!ZrParser_ExecIr_InferSendSync(function, summary, diagnostic)) {
        return ZR_FALSE;
    }
    return summary != ZR_NULL ? summary->sync : ZR_FALSE;
}

const TZrChar *ZrParser_ExecIr_SendSyncReasonName(EZrExecIrSendSyncReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_SEND_SYNC_REASON_NONE: return "none";
        case ZR_EXEC_IR_SEND_SYNC_REASON_INVALID_INPUT: return "invalid-input";
        case ZR_EXEC_IR_SEND_SYNC_REASON_UNKNOWN_OWNERSHIP: return "unknown-ownership";
        case ZR_EXEC_IR_SEND_SYNC_REASON_BORROWED_ALIAS: return "borrowed-alias";
        case ZR_EXEC_IR_SEND_SYNC_REASON_UNIQUE_MUTABLE: return "unique-mutable";
        case ZR_EXEC_IR_SEND_SYNC_REASON_THREAD_AFFINE: return "thread-affine";
        default: return "unknown";
    }
}
