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
    /* Physical descriptor indices, not logical ordinals or value IDs.
     * valueId identifies the source's sole logical occupant. */
    TZrUInt32 sourceSlot;
    TZrUInt32 targetSlot;
    TZrUInt32 byteSize;
    TZrUInt32 flags; /* Must be zero in the representation-plan contract. */
} SZrExecIrCallTransferValue;

typedef struct SZrExecIrCallTransferRequest {
    const SZrExecIrPackedFrameLayout *sourceLayout;
    const SZrExecIrPackedFrameLayout *targetLayout;
    const SZrExecIrCallTransferValue *values;
    TZrUInt32 valueCount;
    TZrBool returnMayAlias;
    TZrBool requiresWriteback;
} SZrExecIrCallTransferRequest;

/* Checked representation metadata only. Neither spans nor kinds authorize
 * memory copying, ownership consumption, borrow escape or GC-root changes. */
typedef struct SZrExecIrCallTransferSpan {
    TZrUInt32 sourceByteOffset;
    TZrUInt32 targetByteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 sourceByteAlign;
    TZrUInt32 targetByteAlign;
    TZrExecIrTypeToken typeToken;
} SZrExecIrCallTransferSpan;

typedef struct SZrExecIrCallTransferPlan {
    EZrExecIrTransferKind *kinds;
    TZrUInt32 *sourceSlots;
    TZrUInt32 *targetSlots;
    SZrExecIrCallTransferSpan *spans;
    TZrUInt64 sourceLayoutHash;
    TZrUInt64 targetLayoutHash;
    TZrUInt32 valueCount;
    TZrBool forwardReturn; /* False: no return descriptors/commit proof. */
    TZrBool noAliasConflict;
    TZrBool compatibleLayout; /* Selected argument representations only. */
} SZrExecIrCallTransferPlan;

ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanInit(SZrExecIrCallTransferPlan *plan);
ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanFree(SZrExecIrCallTransferPlan *plan);
ZR_PARSER_API TZrBool ZrParser_ExecIr_PrepareCallTransfer(
        const SZrExecIrCallTransferRequest *request,
        SZrExecIrCallTransferPlan *plan,
        SZrExecIrDiagnostic *diagnostic);

#endif
