#include "backend_aot_callable_provenance.h"

#include "backend_aot_internal.h"
#include "zr_vm_core/function.h"

TZrUInt32 *backend_aot_allocate_callable_slot_function_indices(SZrState *state, const SZrFunction *function) {
    TZrUInt32 *slotFunctionIndices;
    TZrUInt32 slotCount;

    slotCount = ZrCore_Function_GetGeneratedFrameSlotCount(function);
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || slotCount == 0) {
        return ZR_NULL;
    }

    slotFunctionIndices = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*slotFunctionIndices) * slotCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (slotFunctionIndices == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < slotCount; slotIndex++) {
        slotFunctionIndices[slotIndex] = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    return slotFunctionIndices;
}

void backend_aot_release_callable_slot_function_indices(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrUInt32 *slotFunctionIndices) {
    TZrUInt32 slotCount = ZrCore_Function_GetGeneratedFrameSlotCount(function);

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || slotCount == 0 ||
        slotFunctionIndices == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  slotFunctionIndices,
                                  sizeof(*slotFunctionIndices) * slotCount,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

TZrUInt32 backend_aot_get_callable_slot_function_index(const TZrUInt32 *slotFunctionIndices,
                                                       const SZrFunction *function,
                                                       TZrUInt32 slotIndex) {
    if (slotFunctionIndices == ZR_NULL || function == ZR_NULL ||
        slotIndex >= ZrCore_Function_GetGeneratedFrameSlotCount(function)) {
        return ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    return slotFunctionIndices[slotIndex];
}

void backend_aot_set_callable_slot_function_index(TZrUInt32 *slotFunctionIndices,
                                                  const SZrFunction *function,
                                                  TZrUInt32 slotIndex,
                                                  TZrUInt32 functionIndex) {
    if (slotFunctionIndices == ZR_NULL || function == ZR_NULL ||
        slotIndex >= ZrCore_Function_GetGeneratedFrameSlotCount(function)) {
        return;
    }

    slotFunctionIndices[slotIndex] = functionIndex;
}

TZrUInt32 backend_aot_resolve_callable_slot_function_index_before_instruction(const SZrAotFunctionTable *table,
                                                                              SZrState *state,
                                                                              const SZrFunction *function,
                                                                              TZrUInt32 instructionLimit,
                                                                              TZrUInt32 slotIndex,
                                                                              TZrUInt32 recursionDepth) {
    TZrUInt32 scanIndex;

    if (table == ZR_NULL || state == ZR_NULL || function == ZR_NULL || recursionDepth > function->instructionsLength) {
        return ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    for (scanIndex = instructionLimit; scanIndex > 0; scanIndex--) {
        const TZrInstruction *instruction = &function->instructionsList[scanIndex - 1];
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

        if (destinationSlot != slotIndex) {
            continue;
        }

        switch (instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            {
                TZrUInt32 functionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
                if (backend_aot_resolve_callable_constant_function_index(table,
                                                                        state,
                                                                        function,
                                                                        instruction->instruction.operand.operand2[0],
                                                                        &functionIndex)) {
                    return functionIndex;
                }
                return ZR_AOT_INVALID_FUNCTION_INDEX;
            }
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE): {
                TZrUInt32 functionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
                if (backend_aot_resolve_callable_constant_function_index(
                            table,
                            state,
                            function,
                            (TZrInt32)instruction->instruction.operand.operand1[0],
                            &functionIndex)) {
                    return functionIndex;
                }
                return ZR_AOT_INVALID_FUNCTION_INDEX;
            }
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK): {
                TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
                if (sourceSlot == slotIndex) {
                    return ZR_AOT_INVALID_FUNCTION_INDEX;
                }
                return backend_aot_resolve_callable_slot_function_index_before_instruction(table,
                                                                                           state,
                                                                                           function,
                                                                                           scanIndex - 1,
                                                                                           sourceSlot,
                                                                                           recursionDepth + 1);
            }
            default:
                return ZR_AOT_INVALID_FUNCTION_INDEX;
        }
    }

    return ZR_AOT_INVALID_FUNCTION_INDEX;
}
