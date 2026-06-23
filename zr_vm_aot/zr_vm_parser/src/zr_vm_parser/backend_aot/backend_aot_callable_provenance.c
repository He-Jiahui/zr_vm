#include "backend_aot_callable_provenance.h"

#include "backend_aot_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"

static const SZrFunction *backend_aot_find_owner_child_function_by_name(const SZrFunction *ownerFunction,
                                                                        const SZrString *name) {
    if (ownerFunction == ZR_NULL || name == ZR_NULL || ownerFunction->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 childIndex = 0; childIndex < ownerFunction->childFunctionLength; childIndex++) {
        const SZrFunction *childFunction = &ownerFunction->childFunctionList[childIndex];
        if (childFunction->functionName == name ||
            (childFunction->functionName != ZR_NULL &&
             ZrCore_String_Equal(childFunction->functionName, (SZrString *)name))) {
            return childFunction;
        }
    }

    return ZR_NULL;
}

static const SZrFunction *backend_aot_find_owner_child_function_by_stack_slot(const SZrFunction *ownerFunction,
                                                                              TZrUInt32 stackSlot) {
    if (ownerFunction == ZR_NULL || ownerFunction->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    if (ownerFunction->topLevelCallableBindings != ZR_NULL) {
        for (TZrUInt32 bindingIndex = 0; bindingIndex < ownerFunction->topLevelCallableBindingLength; bindingIndex++) {
            const SZrFunctionTopLevelCallableBinding *binding =
                    &ownerFunction->topLevelCallableBindings[bindingIndex];
            if (binding->stackSlot == stackSlot &&
                binding->callableChildIndex != ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE &&
                binding->callableChildIndex < ownerFunction->childFunctionLength) {
                return &ownerFunction->childFunctionList[binding->callableChildIndex];
            }
        }
    }

    if (ownerFunction->exportedVariables != ZR_NULL) {
        for (TZrUInt32 bindingIndex = 0; bindingIndex < ownerFunction->exportedVariableLength; bindingIndex++) {
            const SZrFunctionExportedVariable *binding = &ownerFunction->exportedVariables[bindingIndex];
            if (binding->stackSlot == stackSlot &&
                binding->callableChildIndex != ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE &&
                binding->callableChildIndex < ownerFunction->childFunctionLength) {
                return &ownerFunction->childFunctionList[binding->callableChildIndex];
            }
        }
    }

    return ZR_NULL;
}

static TZrUInt32 backend_aot_resolve_callable_closure_function_index(const SZrAotFunctionTable *table,
                                                                     SZrState *state,
                                                                     const SZrFunction *function,
                                                                     TZrUInt32 closureIndex,
                                                                     TZrUInt32 recursionDepth) {
    const SZrFunctionClosureVariable *closure;
    const SZrFunction *ownerFunction;
    const SZrFunction *childFunction;
    TZrUInt32 functionIndex;

    if (table == ZR_NULL || state == ZR_NULL || function == ZR_NULL ||
        function->closureValueList == ZR_NULL || closureIndex >= function->closureValueLength ||
        recursionDepth > function->instructionsLength + function->closureValueLength + 1u ||
        function->ownerFunction == ZR_NULL) {
        return ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    closure = &function->closureValueList[closureIndex];
    ownerFunction = function->ownerFunction;

    childFunction = backend_aot_find_owner_child_function_by_name(ownerFunction, closure->name);
    functionIndex = backend_aot_find_function_table_index(table, childFunction);
    if (functionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
        return functionIndex;
    }

    if (closure->inStack) {
        childFunction = backend_aot_find_owner_child_function_by_stack_slot(ownerFunction, closure->index);
        functionIndex = backend_aot_find_function_table_index(table, childFunction);
        if (functionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
            return functionIndex;
        }

        return backend_aot_resolve_callable_slot_function_index_before_instruction(table,
                                                                                  state,
                                                                                  ownerFunction,
                                                                                  ownerFunction->instructionsLength,
                                                                                  closure->index,
                                                                                  recursionDepth + 1u);
    }

    if (ownerFunction->closureValueList != ZR_NULL && closure->index < ownerFunction->closureValueLength) {
        return backend_aot_resolve_callable_closure_function_index(table,
                                                                  state,
                                                                  ownerFunction,
                                                                  closure->index,
                                                                  recursionDepth + 1u);
    }

    return ZR_AOT_INVALID_FUNCTION_INDEX;
}

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
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE): {
                TZrInt32 closureIndex = instruction->instruction.operand.operand2[0];
                if (closureIndex < 0) {
                    return ZR_AOT_INVALID_FUNCTION_INDEX;
                }
                return backend_aot_resolve_callable_closure_function_index(table,
                                                                          state,
                                                                          function,
                                                                          (TZrUInt32)closureIndex,
                                                                          recursionDepth + 1u);
            }
            case ZR_INSTRUCTION_ENUM(GETUPVAL): {
                TZrInt32 closureIndex = instruction->instruction.operand.operand1[0];
                if (closureIndex < 0) {
                    return ZR_AOT_INVALID_FUNCTION_INDEX;
                }
                return backend_aot_resolve_callable_closure_function_index(table,
                                                                          state,
                                                                          function,
                                                                          (TZrUInt32)closureIndex,
                                                                          recursionDepth + 1u);
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
