#ifndef ZR_VM_CORE_EXECUTION_FRAME_VALUE_SLOT_FAST_H
#define ZR_VM_CORE_EXECUTION_FRAME_VALUE_SLOT_FAST_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/profile.h"

static ZR_FORCE_INLINE SZrTypeValue *execution_frame_value_slot_dispatch_try_direct_inline(
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 stackSlot,
        SZrProfileRuntime *profileRuntime,
        TZrBool recordHelpers) {
    if (ZR_LIKELY(function != ZR_NULL && frameBase != ZR_NULL &&
                  function->frameSlotLayouts != ZR_NULL &&
                  stackSlot < function->frameSlotLayoutLength)) {
        const SZrFunctionFrameSlotLayout *slotLayout =
                &function->frameSlotLayouts[stackSlot];

        if (ZR_LIKELY(slotLayout->stackSlot == stackSlot &&
                      (slotLayout->reserved0 &
                       ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u)) {
            if (ZR_UNLIKELY(recordHelpers)) {
                ZR_ASSERT(profileRuntime != ZR_NULL);
                profileRuntime->helperCounts[
                        ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]++;
            }
            return (SZrTypeValue *)((TZrByte *)frameBase +
                                    slotLayout->byteOffset);
        }
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrTypeValue *execution_frame_value_slot_dispatch_try_packed_direct_inline(
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 stackSlot,
        SZrProfileRuntime *profileRuntime,
        TZrBool recordHelpers) {
    if (ZR_LIKELY(function != ZR_NULL && frameBase != ZR_NULL &&
                  ZrCore_Function_HasDirectValueFrameSlotSummary(function) &&
                  stackSlot < function->frameSlotLayoutLength)) {
        if (ZR_UNLIKELY(recordHelpers)) {
            ZR_ASSERT(profileRuntime != ZR_NULL);
            profileRuntime->helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]++;
        }
        return (SZrTypeValue *)((TZrByte *)frameBase +
                                (function->stackSize + stackSlot) *
                                        sizeof(SZrTypeValueOnStack));
    }

    return ZR_NULL;
}

#endif
