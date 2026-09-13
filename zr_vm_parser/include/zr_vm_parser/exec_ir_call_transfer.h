#ifndef ZR_VM_PARSER_EXEC_IR_CALL_TRANSFER_H
#define ZR_VM_PARSER_EXEC_IR_CALL_TRANSFER_H

#include "zr_vm_parser/exec_ir_frame_layout.h"

typedef enum EZrExecIrTransferKind {
    ZR_EXEC_IR_TRANSFER_SCALAR_COPY = 0,
    ZR_EXEC_IR_TRANSFER_SPAN_COPY,
    ZR_EXEC_IR_TRANSFER_MOVE,
    ZR_EXEC_IR_TRANSFER_BORROW,
    ZR_EXEC_IR_TRANSFER_BOXED_BRIDGE,
    ZR_EXEC_IR_TRANSFER_KIND_COUNT
} EZrExecIrTransferKind;

typedef struct SZrExecIrCallTransferValue {
    TZrExecIrValueId valueId;
    EZrExecIrOwnership ownership;
    EZrExecIrPackedSlotClass slotClass;
    TZrUInt32 sourceSlot;
    TZrUInt32 targetSlot;
    TZrUInt32 byteSize;
    TZrUInt32 flags;
} SZrExecIrCallTransferValue;

typedef struct SZrExecIrCallTransferRequest {
    const SZrExecIrPackedFrameLayout *sourceLayout;
    const SZrExecIrPackedFrameLayout *targetLayout;
    const SZrExecIrCallTransferValue *values;
    TZrUInt32 valueCount;
    TZrBool returnMayAlias;
    TZrBool requiresWriteback;
} SZrExecIrCallTransferRequest;

typedef struct SZrExecIrCallTransferPlan {
    EZrExecIrTransferKind *kinds;
    TZrUInt32 *sourceSlots;
    TZrUInt32 *targetSlots;
    TZrUInt32 valueCount;
    TZrBool forwardReturn;
    TZrBool noAliasConflict;
    TZrBool compatibleLayout;
} SZrExecIrCallTransferPlan;

ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanInit(SZrExecIrCallTransferPlan *plan);
ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanFree(SZrExecIrCallTransferPlan *plan);
ZR_PARSER_API TZrBool ZrParser_ExecIr_PrepareCallTransfer(
        const SZrExecIrCallTransferRequest *request,
        SZrExecIrCallTransferPlan *plan,
        SZrExecIrDiagnostic *diagnostic);

#endif
