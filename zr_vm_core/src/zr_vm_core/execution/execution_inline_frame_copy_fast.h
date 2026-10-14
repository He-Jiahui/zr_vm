#ifndef ZR_VM_CORE_EXECUTION_INLINE_FRAME_COPY_FAST_H
#define ZR_VM_CORE_EXECUTION_INLINE_FRAME_COPY_FAST_H

#include "zr_vm_core/function.h"

typedef TZrBool (*TZrExecutionInlineFrameCopyProbe)(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot);

/* DIRECT_VALUE 是 finalizer 写入的普通值证明。打包镜像偏移只用于取址，
 * 不能拿来决定是否跳过内联探测：有间隙的直接值槽仍不是 inline payload。 */
static ZR_FORCE_INLINE TZrBool execution_frame_slot_has_direct_value_proof(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    const SZrFunctionFrameSlotLayout *slotLayout;

    if (function == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL ||
        stackSlot >= function->frameSlotLayoutLength) {
        return ZR_FALSE;
    }
    slotLayout = &function->frameSlotLayouts[stackSlot];
    return (TZrBool)(slotLayout->stackSlot == stackSlot &&
                     (slotLayout->reserved0 &
                      ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
}

static ZR_FORCE_INLINE TZrBool execution_frame_value_slot_copy_requires_inline_probe(
        const SZrFunction *function,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    return (TZrBool)(
            !execution_frame_slot_has_direct_value_proof(function, destinationSlot) ||
            !execution_frame_slot_has_direct_value_proof(function, sourceSlot));
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
