#include "aot_typed_call_binding.h"

#include "zr_vm_core/call_binding.h"

TZrBool aot_prepare_call_binding(SZrState *state, ZrAotGeneratedFrame *frame,
                                SZrTypeValue *callable) {
    if (state == ZR_NULL || frame == ZR_NULL || callable == ZR_NULL) return ZR_FALSE;
    return ZrCore_CallBinding_TryPrepareKnownCall(state, frame->function,
            frame->currentInstructionIndex, callable, &state->lastCallBindingError);
}

TZrBool aot_prepare_meta_binding(SZrState *state, ZrAotGeneratedFrame *frame,
        const SZrTypeValue *receiver, SZrTypeValue *callable, TZrBool *hasBinding) {
    SZrFunction *function = frame->function;
    TZrUInt32 mapped;
    *hasBinding = ZR_FALSE;
    if (function == ZR_NULL || function->callBindingInstructionMap == ZR_NULL ||
        frame->currentInstructionIndex >= function->callBindingInstructionMapLength) return ZR_TRUE;
    mapped = function->callBindingInstructionMap[frame->currentInstructionIndex];
    if (mapped == 0u) return ZR_TRUE;
    *hasBinding = ZR_TRUE;
    if (mapped > function->callSiteCacheLength ||
        function->callSiteCaches[mapped - 1u].binding.contract.operation != ZR_CALL_BINDING_OPERATION_META) {
        state->lastCallBindingError.status = ZR_CALL_BINDING_INVALID_RELOCATION;
        state->lastCallBindingError.instructionIndex = frame->currentInstructionIndex;
        return ZR_FALSE;
    }
    return ZrCore_CallBinding_PrepareMember(state, function, mapped - 1u,
            receiver, callable, &state->lastCallBindingError);
}
