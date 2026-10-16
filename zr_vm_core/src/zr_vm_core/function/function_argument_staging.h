#ifndef ZR_VM_CORE_FUNCTION_ARGUMENT_STAGING_H
#define ZR_VM_CORE_FUNCTION_ARGUMENT_STAGING_H
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/execution_call_transfer.h"
#include <stdint.h>

static TZrUInt32 function_frame_parameter_index_for_stack_slot(const SZrFunction *function,
                                                              TZrUInt32 stackSlot) {
    TZrUInt32 parameterIndex = 0u;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];

        if (slotLayout->isParameter && slotLayout->stackSlot < stackSlot) {
            parameterIndex++;
        }
    }

    return parameterIndex;
}

static ZR_FORCE_INLINE TZrBool function_has_direct_value_parameter_summary(
        const SZrFunction *function) {
    TZrUInt32 parameterCount;
    TZrUInt32 scanLength;
    const SZrFunctionFrameSlotLayout *lastParameterLayout;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL ||
        function->directValueParameterCountPlusOne == 0u) {
        return ZR_FALSE;
    }
    parameterCount = function->directValueParameterCountPlusOne - 1u;
    scanLength = function->directValueParameterScanLength;
    if (scanLength > function->frameSlotLayoutLength ||
        parameterCount > scanLength) {
        return ZR_FALSE;
    }
    if (parameterCount == 0u) {
        return (TZrBool)(scanLength == 0u);
    }
    if (scanLength == 0u) {
        return ZR_FALSE;
    }

    lastParameterLayout = &function->frameSlotLayouts[scanLength - 1u];
    return (TZrBool)(lastParameterLayout->isParameter &&
                     ZrCore_Function_IsDirectFrameValueSlotLayout(
                             function, lastParameterLayout));
}

static ZR_FORCE_INLINE TZrBool function_make_direct_frame_value_slot_place(
        struct SZrState *state,
        TZrStackValuePointer frameBase,
        const SZrFunctionFrameSlotLayout *slotLayout,
        SZrStackFramePlace *outPlace) {
    TZrMemoryOffset stackByteSize;
    TZrMemoryOffset frameBaseOffset;
    TZrMemoryOffset absoluteOffset;

    if (state == ZR_NULL || frameBase == ZR_NULL ||
        state->stackBase.valuePointer == ZR_NULL ||
        state->stackTail.valuePointer == ZR_NULL ||
        state->stackTail.valuePointer < state->stackBase.valuePointer) {
        return ZR_FALSE;
    }

    stackByteSize =
            (TZrMemoryOffset)(state->stackTail.valuePointer -
                              state->stackBase.valuePointer) *
            (TZrMemoryOffset)sizeof(SZrTypeValueOnStack);
    frameBaseOffset = (TZrByte *)frameBase -
                      (TZrByte *)state->stackBase.valuePointer;
    if (frameBaseOffset < 0 || frameBaseOffset > stackByteSize ||
        (TZrMemoryOffset)slotLayout->byteOffset >
                stackByteSize - frameBaseOffset ||
        (TZrMemoryOffset)slotLayout->byteSize >
                stackByteSize - frameBaseOffset -
                        (TZrMemoryOffset)slotLayout->byteOffset) {
        return ZR_FALSE;
    }

    absoluteOffset = frameBaseOffset +
                     (TZrMemoryOffset)slotLayout->byteOffset;
    outPlace->address = (TZrByte *)frameBase + slotLayout->byteOffset;
    outPlace->byteOffset = absoluteOffset;
    outPlace->byteSize = slotLayout->byteSize;
    outPlace->byteAlign = slotLayout->byteAlign;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool function_make_value_parameter_place(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const SZrFunctionFrameSlotLayout *slotLayout,
        SZrStackFramePlace *outPlace,
        TZrBool *outDirect) {
    TZrBool direct = ZrCore_Function_IsDirectFrameValueSlotLayout(
            function, slotLayout);

    if (outDirect != ZR_NULL) {
        *outDirect = direct;
    }
    if (direct) {
        return function_make_direct_frame_value_slot_place(
                state, frameBase, slotLayout, outPlace);
    }
    return ZrCore_Function_MakeFrameSlotPlace(
            state, function, frameBase, slotLayout->stackSlot, outPlace);
}


typedef struct SZrFunctionArgumentStage {
    SZrTypeValue value;
    SZrTypeValue *destination;
    SZrTypeValue *mirror;
    TZrBool direct;
    TZrUInt32 parameterIndex;
    TZrUInt32 sourceStackSlot;
} SZrFunctionArgumentStage;

ZR_CORE_API TZrBool ZrCore_Function_ValueArgumentWorkspaceSize(const SZrFunction *callee,
        TZrSize *outSize);
ZR_CORE_API EZrExecutionTransferStatus ZrCore_Function_StageValueFrameParametersWithWorkspace(
        struct SZrState *state, const SZrFunction *callee, TZrStackValuePointer destinationBase,
        const SZrFunction *source, TZrStackValuePointer sourceBase,
        TZrUInt32 startSlot, TZrSize count, void *workspace, TZrSize capacity,
        SZrExecutionArgumentStagingDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionTransferStatus ZrCore_Function_StageValueFrameParameters(
        struct SZrState *state, const SZrFunction *callee, TZrStackValuePointer destinationBase,
        const SZrFunction *source, TZrStackValuePointer sourceBase,
        TZrUInt32 startSlot, TZrSize count,
        SZrExecutionArgumentStagingDiagnostic *diagnostic);
#endif
