#ifndef ZR_VM_CORE_EXECUTION_INLINE_FRAME_COPY_FAST_H
#define ZR_VM_CORE_EXECUTION_INLINE_FRAME_COPY_FAST_H

#include "zr_vm_core/function.h"

typedef TZrBool (*TZrExecutionInlineFrameCopyProbe)(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot);

static ZR_FORCE_INLINE TZrBool execution_frame_value_slot_copy_requires_inline_probe(
        const SZrFunction *function,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    return (TZrBool)(
            !ZrCore_Function_IsDirectFrameValueSlot(function, destinationSlot) ||
            !ZrCore_Function_IsDirectFrameValueSlot(function, sourceSlot));
}

static ZR_FORCE_INLINE TZrBool execution_inline_frame_try_copy_stack_slot_dispatch(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrExecutionInlineFrameCopyProbe probe) {
    if (ZrCore_Function_HasDirectValueFrameSlotSummary(function) &&
        destinationSlot < function->frameSlotLayoutLength &&
        sourceSlot < function->frameSlotLayoutLength) {
        return ZR_FALSE;
    }
    if (!execution_frame_value_slot_copy_requires_inline_probe(
                function, destinationSlot, sourceSlot)) {
        return ZR_FALSE;
    }
    return probe(
            state, function, frameBase, destinationSlot, sourceSlot);
}

#endif
