#include "zr_vm_parser/exec_ir_call_transfer.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    free(plan->spans);
    memset(plan, 0, sizeof(*plan));
}

static TZrBool transfer_class_valid(EZrExecIrPackedSlotClass slotClass) {
    switch (slotClass) {
        case ZR_EXEC_IR_PACKED_SLOT_BOXED:
        case ZR_EXEC_IR_PACKED_SLOT_SCALAR:
        case ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN:
        case ZR_EXEC_IR_PACKED_SLOT_REF: return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool transfer_ownership_valid(EZrExecIrOwnership ownership) {
    switch (ownership) {
        case ZR_EXEC_IR_OWNERSHIP_UNKNOWN:
        case ZR_EXEC_IR_OWNERSHIP_BORROWED:
        case ZR_EXEC_IR_OWNERSHIP_UNIQUE:
        case ZR_EXEC_IR_OWNERSHIP_SHARED:
        case ZR_EXEC_IR_OWNERSHIP_GC: return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool transfer_alignment_valid(TZrUInt32 align) {
    return (TZrBool)(align != 0u && (align & (align - 1u)) == 0u);
}

static TZrBool transfer_layout_valid(const SZrExecIrPackedFrameLayout *layout) {
    const SZrExecIrFrameLayout *frame = &layout->frame;
    TZrUInt32 i, j;
    if (frame->slotCount != frame->storageSlotCount ||
        frame->slotCount > frame->slotCapacity ||
        frame->storageSlotCount > frame->logicalSlotCount ||
        frame->parameterPrefixCount > frame->logicalSlotCount ||
        !transfer_alignment_valid(frame->frameByteAlign) ||
        frame->frameByteSize % frame->frameByteAlign != 0u ||
        frame->returnBufferOffset > frame->frameByteSize ||
        (frame->slotCount != 0u && frame->slots == ZR_NULL) ||
        (frame->logicalSlotCount != 0u && (layout->logicalValueIds == ZR_NULL ||
            layout->logicalToPhysical == ZR_NULL || layout->slotClasses == ZR_NULL))) return ZR_FALSE;
    for (i = 0u; i < frame->slotCount; ++i) {
        const SZrExecIrFrameSlot *slot = &frame->slots[i];
        TZrBool occupied = ZR_FALSE;
        if (slot->slotId == ZR_EXEC_IR_VALUE_ID_INVALID || slot->byteSize == 0u ||
            !transfer_alignment_valid(slot->byteAlign) || slot->byteAlign > frame->frameByteAlign ||
            slot->byteOffset % slot->byteAlign != 0u || slot->byteOffset > frame->frameByteSize ||
            slot->byteSize > frame->frameByteSize - slot->byteOffset ||
            (slot->kind != ZR_EXEC_IR_FRAME_SLOT_VALUE && slot->kind != ZR_EXEC_IR_FRAME_SLOT_REFERENCE))
            return ZR_FALSE;
        for (j = 0u; j < i; ++j) {
            const SZrExecIrFrameSlot *prior = &frame->slots[j];
            /* Ends cannot overflow after the frame containment checks. */
            if (slot->byteOffset < prior->byteOffset + prior->byteSize &&
                prior->byteOffset < slot->byteOffset + slot->byteSize) return ZR_FALSE;
        }
        for (j = 0u; j < frame->logicalSlotCount; ++j)
            if (layout->logicalToPhysical[j] == i) occupied = ZR_TRUE;
        if (!occupied) return ZR_FALSE;
    }
    for (i = 0u; i < frame->logicalSlotCount; ++i) {
        TZrUInt32 physical = layout->logicalToPhysical[i];
        if (layout->logicalValueIds[i] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            physical >= frame->storageSlotCount || !transfer_class_valid(layout->slotClasses[i]) ||
            ((layout->slotClasses[i] == ZR_EXEC_IR_PACKED_SLOT_REF) !=
             (frame->slots[physical].kind == ZR_EXEC_IR_FRAME_SLOT_REFERENCE))) return ZR_FALSE;
        for (j = 0u; j < i; ++j) {
            if (layout->logicalValueIds[j] == layout->logicalValueIds[i] ||
                (layout->logicalToPhysical[j] == physical &&
                 layout->slotClasses[j] != layout->slotClasses[i])) return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool transfer_single_occupant(const SZrExecIrPackedFrameLayout *layout,
                                       TZrUInt32 physical, TZrUInt32 *logical) {
    TZrUInt32 i, count = 0u;
    for (i = 0u; i < layout->frame.logicalSlotCount; ++i) {
        if (layout->logicalToPhysical[i] == physical) {
            *logical = i;
            if (++count > 1u) return ZR_FALSE;
        }
    }
    return (TZrBool)(count == 1u &&
        layout->frame.slots[physical].slotId == layout->logicalValueIds[*logical]);
}

static TZrBool transfer_count_fits(TZrUInt32 count) {
#if SIZE_MAX <= UINT32_MAX
    return (TZrBool)(count <= SIZE_MAX / sizeof(SZrExecIrCallTransferSpan) &&
                    count <= SIZE_MAX / sizeof(EZrExecIrTransferKind) &&
                    count <= SIZE_MAX / sizeof(TZrUInt32));
#else
    (void)count;
    return ZR_TRUE;
#endif
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
    if (!transfer_layout_valid(request->sourceLayout) || !transfer_layout_valid(request->targetLayout)) {
        transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        return ZR_FALSE;
    }
    if (!transfer_count_fits(request->valueCount)) {
        transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_CallTransferPlanInit(&candidate);
    candidate.valueCount = request->valueCount;
    candidate.compatibleLayout = ZR_TRUE;
    candidate.sourceLayoutHash = request->sourceLayout->frame.layoutHash;
    candidate.targetLayoutHash = request->targetLayout->frame.layoutHash;
    candidate.noAliasConflict = (TZrBool)!request->returnMayAlias;
    /* Argument representation validation cannot prove return layout/commit. */
    candidate.forwardReturn = ZR_FALSE;
    if (request->valueCount != 0u) {
        candidate.kinds = (EZrExecIrTransferKind *)malloc(sizeof(*candidate.kinds) * request->valueCount);
        candidate.sourceSlots = (TZrUInt32 *)malloc(sizeof(*candidate.sourceSlots) * request->valueCount);
        candidate.targetSlots = (TZrUInt32 *)malloc(sizeof(*candidate.targetSlots) * request->valueCount);
        candidate.spans = (SZrExecIrCallTransferSpan *)malloc(sizeof(*candidate.spans) * request->valueCount);
        if (candidate.kinds == ZR_NULL || candidate.sourceSlots == ZR_NULL ||
            candidate.targetSlots == ZR_NULL || candidate.spans == ZR_NULL) {
            transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
            ZrParser_ExecIr_CallTransferPlanFree(&candidate);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < request->valueCount; ++i) {
        const SZrExecIrCallTransferValue *value = &request->values[i];
        TZrUInt32 sourceLogical = 0u, targetLogical = 0u, prior;
        const SZrExecIrFrameSlot *sourceSlot, *targetSlot;
        if (value->valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            !transfer_ownership_valid(value->ownership) || !transfer_class_valid(value->slotClass) ||
            value->flags != 0u || value->byteSize == 0u ||
            value->sourceSlot >= request->sourceLayout->frame.storageSlotCount ||
            value->targetSlot >= request->targetLayout->frame.storageSlotCount ||
            !transfer_single_occupant(request->sourceLayout, value->sourceSlot, &sourceLogical) ||
            !transfer_single_occupant(request->targetLayout, value->targetSlot, &targetLogical) ||
            request->sourceLayout->logicalValueIds[sourceLogical] != value->valueId ||
            request->sourceLayout->slotClasses[sourceLogical] != value->slotClass ||
            request->targetLayout->slotClasses[targetLogical] != value->slotClass) {
            transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i);
            ZrParser_ExecIr_CallTransferPlanFree(&candidate);
            return ZR_FALSE;
        }
        sourceSlot = &request->sourceLayout->frame.slots[value->sourceSlot];
        targetSlot = &request->targetLayout->frame.slots[value->targetSlot];
        if (sourceSlot->typeToken == 0u || targetSlot->typeToken == 0u ||
            sourceSlot->typeToken != targetSlot->typeToken ||
            sourceSlot->byteSize != value->byteSize || targetSlot->byteSize != value->byteSize ||
            sourceSlot->byteAlign != targetSlot->byteAlign) goto invalid_row;
        for (prior = 0u; prior < i; ++prior) {
            if (request->values[prior].targetSlot == value->targetSlot ||
                (request->values[prior].sourceSlot == value->sourceSlot &&
                 (request->values[prior].ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
                  value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE))) goto invalid_row;
        }
        candidate.kinds[i] = transfer_kind(value);
        candidate.sourceSlots[i] = value->sourceSlot;
        candidate.targetSlots[i] = value->targetSlot;
        candidate.spans[i].sourceByteOffset = sourceSlot->byteOffset;
        candidate.spans[i].targetByteOffset = targetSlot->byteOffset;
        candidate.spans[i].byteSize = value->byteSize;
        candidate.spans[i].sourceByteAlign = sourceSlot->byteAlign;
        candidate.spans[i].targetByteAlign = targetSlot->byteAlign;
        candidate.spans[i].typeToken = sourceSlot->typeToken;
        continue;
invalid_row:
        transfer_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i);
        ZrParser_ExecIr_CallTransferPlanFree(&candidate);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_CallTransferPlanFree(plan);
    *plan = candidate;
    return ZR_TRUE;
}
