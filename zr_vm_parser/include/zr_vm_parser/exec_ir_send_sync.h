#ifndef ZR_VM_PARSER_EXEC_IR_SEND_SYNC_H
#define ZR_VM_PARSER_EXEC_IR_SEND_SYNC_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

typedef enum EZrExecIrSendSyncReason {
    ZR_EXEC_IR_SEND_SYNC_REASON_NONE = 0,
    ZR_EXEC_IR_SEND_SYNC_REASON_INVALID_INPUT,
    ZR_EXEC_IR_SEND_SYNC_REASON_UNKNOWN_OWNERSHIP,
    ZR_EXEC_IR_SEND_SYNC_REASON_BORROWED_ALIAS,
    ZR_EXEC_IR_SEND_SYNC_REASON_UNIQUE_MUTABLE,
    ZR_EXEC_IR_SEND_SYNC_REASON_THREAD_AFFINE
} EZrExecIrSendSyncReason;

typedef struct SZrExecIrSendSyncSummary {
    TZrBool send;
    TZrBool sync;
    EZrExecIrSendSyncReason reason;
    TZrExecIrValueId reasonValueId;
    TZrExecIrInstructionId reasonInstructionId;
    TZrExecIrSourceId reasonSourceId;
} SZrExecIrSendSyncSummary;

ZR_PARSER_API TZrBool ZrParser_ExecIr_InferSendSync(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_InferSend(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_InferSync(
        const SZrExecIrFunction *function,
        SZrExecIrSendSyncSummary *summary,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_SendSyncReasonName(
        EZrExecIrSendSyncReason reason);

#endif
