#include "backend_aot_c_function_body.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_frame_cleanup.h"
#include "backend_aot_c_frame_setup.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_scalar_stack_copy.h"
#include "backend_aot_c_scalar_semir.h"
#include "backend_aot_c_value_semir.h"
#include "backend_aot_internal.h"

static const SZrAotExecIrInstruction *backend_aot_find_exec_ir_instruction(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex) {
    TZrUInt32 instructionIndex;

    if (module == ZR_NULL ||
        functionIr == ZR_NULL ||
        module->instructions == ZR_NULL) {
        return ZR_NULL;
    }

    for (instructionIndex = 0; instructionIndex < functionIr->instructionCount; instructionIndex++) {
        TZrUInt32 moduleInstructionIndex = functionIr->firstInstructionOffset + instructionIndex;
        const SZrAotExecIrInstruction *instruction;

        if (moduleInstructionIndex >= module->instructionCount) {
            break;
        }

        instruction = &module->instructions[moduleInstructionIndex];
        if (instruction->execInstructionIndex == execInstructionIndex) {
            return instruction;
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_exec_ir_instruction_is_dynamic_call(
        const SZrAotExecIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_CALL ||
                     instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_TAIL_CALL);
}

void backend_aot_write_c_function_body(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotExecIrModule *module,
                                       const SZrAotFunctionEntry *entry) {
    TZrUInt32 instructionIndex;
    TZrBool publishExports;
    TZrUInt32 *callableSlotFunctionIndices;
    const SZrAotExecIrFunction *functionIr = ZR_NULL;

    if (file == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL) {
        return;
    }

    publishExports = entry->flatIndex == ZR_AOT_FUNCTION_TREE_ROOT_INDEX &&
                     entry->function->exportedVariableLength > 0;
    if (module != ZR_NULL) {
        functionIr = backend_aot_exec_ir_find_function(module, entry->flatIndex);
    }

    fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state) {\n", (unsigned)entry->flatIndex);
    fprintf(file, "    ZrAotGeneratedFrame frame = {0};\n");
    fprintf(file, "    TZrInt64 zr_aot_return_value = 0;\n");
    fprintf(file, "    TZrBool zr_aot_frame_started = ZR_FALSE;\n");
    fprintf(file, "    TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n");
    backend_aot_write_c_frame_setup(file,
                                    functionIr != ZR_NULL ? &functionIr->frameLayout : ZR_NULL,
                                    entry->flatIndex);
    fprintf(file, "    zr_aot_frame_started = ZR_TRUE;\n");
    if (functionIr != ZR_NULL) {
        backend_aot_write_c_scalar_locals(file, functionIr);
        backend_aot_write_c_value_semir_for_function(file, state, module, functionIr, &functionIr->frameLayout);
    }
    backend_aot_write_c_dispatch_loop(file, entry->flatIndex, entry->function->instructionsLength);
    callableSlotFunctionIndices = backend_aot_allocate_callable_slot_function_indices(state, entry->function);

    for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        TZrUInt32 operandA1 = instruction->instruction.operand.operand1[0];
        TZrUInt32 operandB1 = instruction->instruction.operand.operand1[1];
        TZrInt32 operandA2 = instruction->instruction.operand.operand2[0];

        fprintf(file, "zr_aot_fn_%u_ins_%u:\n", (unsigned)entry->flatIndex, (unsigned)instructionIndex);
        fprintf(file, "    /* opcode=%u extra=%u op1a=%u op1b=%u op2=%d */\n",
                (unsigned)instruction->instruction.operationCode,
                (unsigned)destinationSlot,
                (unsigned)operandA1,
                (unsigned)operandB1,
                (int)operandA2);
        backend_aot_write_c_begin_instruction(file,
                                              instructionIndex,
                                              backend_aot_c_step_flags_for_instruction(entry->function, instruction));

        if (backend_aot_try_write_c_scalar_semir_for_exec_instruction(file,
                                                                      module,
                                                                      functionIr,
                                                                      instruction,
                                                                      instructionIndex)) {
            backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                         entry->function,
                                                         destinationSlot,
                                                         ZR_AOT_INVALID_FUNCTION_INDEX);
            continue;
        }

        switch (instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            {
                TZrUInt32 callableFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

                if (backend_aot_resolve_callable_constant_function_index(functionTable,
                                                                        state,
                                                                        entry->function,
                                                                        operandA2,
                                                                        &callableFunctionIndex)) {
                    backend_aot_write_c_direct_callable_constant(file,
                                                                 destinationSlot,
                                                                 (TZrUInt32)operandA2,
                                                                 callableFunctionIndex);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 callableFunctionIndex);
                } else if (backend_aot_c_constant_requires_materialization(state, entry->function, operandA2)) {
                    backend_aot_write_c_unsupported_callable_constant_materialization(file,
                                                                                      destinationSlot,
                                                                                      (TZrUInt32)operandA2);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                } else if (backend_aot_c_constant_can_emit_immediate(entry->function, operandA2)) {
                    backend_aot_write_c_direct_primitive_constant(
                            file, functionIr, destinationSlot, backend_aot_c_get_constant_value(entry->function, operandA2));
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                } else {
                    backend_aot_write_c_direct_constant_copy(file, destinationSlot, (TZrUInt32)operandA2);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                backend_aot_write_c_direct_set_constant(file, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            {
                TZrUInt32 callableFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
                TZrUInt32 closureCaptureCount = instruction->instruction.operand.operand1[1];

                if (backend_aot_resolve_callable_constant_function_index(functionTable,
                                                                         state,
                                                                         entry->function,
                                                                         (TZrInt32)operandA1,
                                                                         &callableFunctionIndex)) {
                    if (closureCaptureCount == 0) {
                        backend_aot_write_c_direct_callable_constant(file,
                                                                     destinationSlot,
                                                                     operandA1,
                                                                     callableFunctionIndex);
                        backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                     entry->function,
                                                                     destinationSlot,
                                                                     callableFunctionIndex);
                    } else {
                        backend_aot_write_c_unsupported_create_closure_materialization(file,
                                                                                       destinationSlot,
                                                                                       operandA1,
                                                                                       closureCaptureCount);
                        backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                     entry->function,
                                                                     destinationSlot,
                                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
                    }
                } else {
                    backend_aot_write_c_unsupported_create_closure_materialization(file,
                                                                                   destinationSlot,
                                                                                   operandA1,
                                                                                   closureCaptureCount);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                backend_aot_write_c_get_closure_value(file, destinationSlot, (TZrUInt32)operandA2);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
                backend_aot_write_c_direct_get_global(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            {
                TZrUInt32 callableFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
                SZrFunction *childFunction = ZR_NULL;

                if (entry->function->childFunctionList != ZR_NULL && operandA1 < entry->function->childFunctionLength) {
                    childFunction = &entry->function->childFunctionList[operandA1];
                    callableFunctionIndex = backend_aot_find_function_table_index(functionTable, childFunction);
                }
                if (childFunction != ZR_NULL &&
                    callableFunctionIndex != ZR_AOT_INVALID_FUNCTION_INDEX &&
                    childFunction->closureValueLength == 0) {
                    backend_aot_write_c_direct_get_sub_function(file,
                                                                destinationSlot,
                                                                operandA1,
                                                                callableFunctionIndex);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 callableFunctionIndex);
                } else if (childFunction != ZR_NULL) {
                    backend_aot_write_c_unsupported_get_sub_function_materialization(file,
                                                                                    destinationSlot,
                                                                                    operandA1,
                                                                                    childFunction->closureValueLength);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                } else {
                    backend_aot_write_c_unsupported_get_sub_function_materialization(file, destinationSlot, operandA1, 0);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                backend_aot_write_c_direct_create_object(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                backend_aot_write_c_direct_create_array(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                backend_aot_write_c_set_closure_value(file, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                backend_aot_write_c_get_closure_value(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                backend_aot_write_c_set_closure_value(file, destinationSlot, operandA1);
                break;
            case ZR_INSTRUCTION_ENUM(NOP):
                break;
            case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
                backend_aot_write_c_direct_reset_stack_null(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):
                backend_aot_write_c_direct_reset_stack_null2(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             operandA1,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(file,
                                                                             state,
                                                                             module,
                                                                             functionIr,
                                                                             instructionIndex,
                                                                             ZR_AOT_INVALID_FUNCTION_INDEX,
                                                                             ZR_FALSE)) {
                    break;
                }
                if (backend_aot_try_write_c_scalar_stack_copy(file,
                                                              functionIr,
                                                              destinationSlot,
                                                              (TZrUInt32)operandA2)) {
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                    break;
                }
                backend_aot_write_c_direct_stack_copy(file, destinationSlot, (TZrUInt32)operandA2);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             backend_aot_get_callable_slot_function_index(
                                                                     callableSlotFunctionIndices,
                                                                     entry->function,
                                                                     (TZrUInt32)operandA2));
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
                backend_aot_write_c_direct_add_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_add_int_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_add_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_add_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
                backend_aot_write_c_direct_add_signed_load_const(file,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1],
                                                                 operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
                backend_aot_write_c_direct_add_signed_load_stack_const(file,
                                                                       entry->function,
                                                                       destinationSlot,
                                                                       instruction->instruction.operand.operand0[0],
                                                                       instruction->instruction.operand.operand0[1],
                                                                       operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                backend_aot_write_c_direct_add_signed_load_stack(file,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1]);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                backend_aot_write_c_direct_add_signed_load_stack_load_const(file,
                                                                            entry->function,
                                                                            destinationSlot,
                                                                            instruction->instruction.operand.operand0[0],
                                                                            instruction->instruction.operand.operand0[1],
                                                                            instruction->instruction.operand.operand0[2],
                                                                            operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_add_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_add_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
                backend_aot_write_c_direct_sub_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_sub_int_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_sub_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_sub_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
                backend_aot_write_c_direct_sub_signed_load_const(file,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1],
                                                                 operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
                backend_aot_write_c_direct_sub_signed_load_stack_const(file,
                                                                       entry->function,
                                                                       destinationSlot,
                                                                       instruction->instruction.operand.operand0[0],
                                                                       instruction->instruction.operand.operand0[1],
                                                                       operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_sub_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_sub_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
                backend_aot_write_c_direct_add(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                backend_aot_write_c_direct_add_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB):
                backend_aot_write_c_direct_sub(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                backend_aot_write_c_direct_sub_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL):
                backend_aot_write_c_direct_mul(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_mul_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_mul_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
                backend_aot_write_c_direct_mul_signed_load_const(file,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1],
                                                                 operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
                backend_aot_write_c_direct_mul_signed_load_stack_const(file,
                                                                       entry->function,
                                                                       destinationSlot,
                                                                       instruction->instruction.operand.operand0[0],
                                                                       instruction->instruction.operand.operand0[1],
                                                                       operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
                backend_aot_write_c_direct_mul_signed_load_stack(file,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1]);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
                backend_aot_write_c_direct_mul_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_mul_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                backend_aot_write_c_direct_mul_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV):
                backend_aot_write_c_direct_div(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                backend_aot_write_c_direct_div_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_div_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
                backend_aot_write_c_direct_div_signed_load_const(file,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1],
                                                                 operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
                backend_aot_write_c_direct_div_signed_load_stack_const(file,
                                                                       entry->function,
                                                                       destinationSlot,
                                                                       instruction->instruction.operand.operand0[0],
                                                                       instruction->instruction.operand.operand0[1],
                                                                       operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
                backend_aot_write_c_direct_div_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_div_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                backend_aot_write_c_direct_div_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD):
                backend_aot_write_c_direct_mod(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                backend_aot_write_c_direct_mod_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_mod_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST):
                backend_aot_write_c_direct_add_signed_mod_const(file,
                                                                entry->function,
                                                                destinationSlot,
                                                                instruction->instruction.operand.operand0[0],
                                                                instruction->instruction.operand.operand0[1],
                                                                operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
                backend_aot_write_c_direct_mod_signed_load_const(file,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 instruction->instruction.operand.operand0[0],
                                                                 instruction->instruction.operand.operand0[1],
                                                                 operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
                backend_aot_write_c_direct_mod_signed_load_stack_const(file,
                                                                       entry->function,
                                                                       destinationSlot,
                                                                       instruction->instruction.operand.operand0[0],
                                                                       instruction->instruction.operand.operand0[1],
                                                                       operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
                backend_aot_write_c_direct_mod_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
                backend_aot_write_c_direct_mod_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                backend_aot_write_c_direct_mod_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(NEG):
                backend_aot_write_c_direct_neg(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(NEG_SIGNED):
                backend_aot_write_c_direct_neg_signed(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(NEG_FLOAT):
                backend_aot_write_c_direct_neg_float(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(POW):
                backend_aot_write_c_direct_pow(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
                backend_aot_write_c_direct_pow_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
                backend_aot_write_c_direct_pow_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
                backend_aot_write_c_direct_pow_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
                backend_aot_write_c_direct_shift_left(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
                backend_aot_write_c_direct_shift_left_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
                backend_aot_write_c_direct_shift_right(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
                backend_aot_write_c_direct_shift_right_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
                backend_aot_write_c_direct_logical_not(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
                backend_aot_write_c_direct_logical_not_bool(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TYPEOF):
                backend_aot_write_c_direct_typeof(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
                backend_aot_write_c_direct_to_bool(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT):
                backend_aot_write_c_direct_to_uint(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                backend_aot_write_c_direct_to_float(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
                backend_aot_write_c_direct_to_float_signed(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
                backend_aot_write_c_direct_to_float_unsigned(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
                backend_aot_write_c_direct_to_int_float(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
                backend_aot_write_c_direct_to_int_unsigned(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
                backend_aot_write_c_direct_to_uint_float(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
                backend_aot_write_c_direct_to_uint_signed(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRING):
                backend_aot_write_c_direct_to_string(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRUCT):
                backend_aot_write_c_direct_to_struct(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_OBJECT):
                backend_aot_write_c_direct_to_object(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                backend_aot_write_c_direct_logical_equal(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
                backend_aot_write_c_direct_logical_equal_bool(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
                backend_aot_write_c_direct_logical_not_equal_bool(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
                backend_aot_write_c_direct_logical_equal_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
                backend_aot_write_c_direct_logical_not_equal_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
                backend_aot_write_c_direct_logical_equal_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
                backend_aot_write_c_direct_logical_not_equal_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
                backend_aot_write_c_direct_logical_equal_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
                backend_aot_write_c_direct_logical_not_equal_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
                backend_aot_write_c_direct_logical_equal_string(file,
                                                                functionIr != ZR_NULL
                                                                        ? &functionIr->frameLayout
                                                                        : ZR_NULL,
                                                                destinationSlot,
                                                                operandA1,
                                                                operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
                backend_aot_write_c_direct_logical_not_equal_string(file,
                                                                    functionIr != ZR_NULL
                                                                            ? &functionIr->frameLayout
                                                                            : ZR_NULL,
                                                                    destinationSlot,
                                                                    operandA1,
                                                                    operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
                backend_aot_write_c_direct_logical_and(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
                backend_aot_write_c_direct_logical_or(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                backend_aot_write_c_direct_logical_greater_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
                backend_aot_write_c_direct_logical_greater_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                backend_aot_write_c_direct_logical_greater_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                backend_aot_write_c_direct_logical_less_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
                backend_aot_write_c_direct_logical_less_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                backend_aot_write_c_direct_logical_less_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
                backend_aot_write_c_direct_logical_greater_equal_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
                backend_aot_write_c_direct_logical_greater_equal_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
                backend_aot_write_c_direct_logical_greater_equal_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
                backend_aot_write_c_direct_logical_less_equal_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
                backend_aot_write_c_direct_logical_less_equal_unsigned(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
                backend_aot_write_c_direct_logical_less_equal_float(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
                backend_aot_write_c_direct_bitwise_not(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
                backend_aot_write_c_direct_bitwise_and(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
                backend_aot_write_c_direct_bitwise_or(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
                backend_aot_write_c_direct_bitwise_xor(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
                backend_aot_write_c_direct_bitwise_shift_left(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
                backend_aot_write_c_direct_bitwise_shift_right(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
                backend_aot_write_c_unsupported_meta_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
                backend_aot_write_c_unsupported_meta_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
                backend_aot_write_c_unsupported_meta_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
                backend_aot_write_c_unsupported_meta_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                backend_aot_write_c_unsupported_meta_call(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
                backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
                backend_aot_write_c_dynamic_function_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
                backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
                backend_aot_write_c_dynamic_function_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(META_GET):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "META_GET",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(META_SET):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "META_SET",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "SUPER_META_GET_CACHED",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "SUPER_META_SET_CACHED",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "SUPER_META_GET_STATIC_CACHED",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
                backend_aot_write_c_unsupported_meta_value_access(file,
                                                                  "SUPER_META_SET_STATIC_CACHED",
                                                                  destinationSlot,
                                                                  operandA1,
                                                                  operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                backend_aot_write_c_direct_logical_not_equal(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
                backend_aot_write_c_direct_own_unique(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_BORROW):
                backend_aot_write_c_direct_own_borrow(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_LOAN):
                backend_aot_write_c_direct_own_loan(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):
                backend_aot_write_c_direct_own_return_loan(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
                backend_aot_write_c_direct_own_share(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
                backend_aot_write_c_direct_own_weak(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_DETACH):
                backend_aot_write_c_direct_own_detach(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
                backend_aot_write_c_direct_own_upgrade(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
                backend_aot_write_c_direct_own_release(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TRY):
                backend_aot_write_c_try(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(END_TRY):
                backend_aot_write_c_end_try(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(THROW):
                backend_aot_write_c_throw(file, entry->flatIndex, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(CATCH):
                backend_aot_write_c_catch(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(END_FINALLY):
                backend_aot_write_c_end_finally(file, entry->flatIndex, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
                backend_aot_write_c_set_pending_return(file, entry->flatIndex, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
                backend_aot_write_c_set_pending_break(file, entry->flatIndex, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
                backend_aot_write_c_set_pending_continue(file, entry->flatIndex, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "GET_MEMBER",
                                                                     operandA1,
                                                                     destinationSlot,
                                                                     operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(file,
                                                                             state,
                                                                             module,
                                                                             functionIr,
                                                                             instructionIndex,
                                                                             ZR_AOT_INVALID_FUNCTION_INDEX,
                                                                             ZR_FALSE)) {
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                    break;
                }
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "GET_MEMBER_SLOT",
                                                                     operandA1,
                                                                     destinationSlot,
                                                                     operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "GET_BY_INDEX",
                                                                     operandA1,
                                                                     operandB1,
                                                                     destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
                backend_aot_write_c_direct_super_array_get_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
                backend_aot_write_c_direct_super_array_get_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "SET_MEMBER",
                                                                     operandA1,
                                                                     destinationSlot,
                                                                     operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(file,
                                                                             state,
                                                                             module,
                                                                             functionIr,
                                                                             instructionIndex,
                                                                             ZR_AOT_INVALID_FUNCTION_INDEX,
                                                                             ZR_FALSE)) {
                    break;
                }
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "SET_MEMBER_SLOT",
                                                                     operandA1,
                                                                     destinationSlot,
                                                                     operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                backend_aot_write_c_unsupported_dynamic_value_access(file,
                                                                     "SET_BY_INDEX",
                                                                     operandA1,
                                                                     operandB1,
                                                                     destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
                backend_aot_write_c_direct_super_array_set_int(file, destinationSlot, operandA1, operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
                backend_aot_write_c_direct_super_array_add_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
                backend_aot_write_c_direct_super_array_add_int4(file, operandA1, operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
                backend_aot_write_c_direct_super_array_add_int4_const(file, entry->function, operandA1, operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
                backend_aot_write_c_direct_super_array_fill_int4_const(file, entry->function, operandA1, operandB1, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
                backend_aot_write_c_direct_iter_init(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
                backend_aot_write_c_direct_iter_move_next(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                backend_aot_write_c_direct_iter_current(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_iter_move_next_jump_if_false(
                            file,
                            entry->flatIndex,
                            destinationSlot,
                            operandA1,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1));
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump(file,
                                                    entry->flatIndex,
                                                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if(file,
                                                       entry->flatIndex,
                                                       destinationSlot,
                                                       (TZrUInt32)((TZrInt64)instructionIndex +
                                                                   (TZrInt64)operandA2 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if_bool_false(
                            file,
                            functionIr,
                            entry->flatIndex,
                            destinationSlot,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                if ((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if_greater_signed(
                            file,
                            functionIr,
                            entry->flatIndex,
                            destinationSlot,
                            operandA1,
                            instructionIndex,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
                if ((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if_less_equal_signed(
                            file,
                            functionIr,
                            entry->flatIndex,
                            destinationSlot,
                            operandA1,
                            instructionIndex,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
                if ((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if_not_equal_signed(
                            file,
                            functionIr,
                            entry->flatIndex,
                            destinationSlot,
                            operandA1,
                            instructionIndex,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
                if ((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    backend_aot_write_c_unsupported_instruction(file,
                                                                entry->flatIndex,
                                                                instructionIndex,
                                                                instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if_not_equal_signed_const(
                            file,
                            functionIr,
                            entry->function,
                            entry->flatIndex,
                            destinationSlot,
                            operandA1,
                            instructionIndex,
                            (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)(TZrInt16)operandB1 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            {
                const SZrAotExecIrInstruction *execIrInstruction =
                        backend_aot_find_exec_ir_instruction(module, functionIr, instructionIndex);
                TZrBool semirDynamicCall = backend_aot_exec_ir_instruction_is_dynamic_call(execIrInstruction);
                TZrUInt32 calleeFunctionIndex = backend_aot_get_callable_slot_function_index(callableSlotFunctionIndices,
                                                                                              entry->function,
                                                                                              operandA1);
                if (calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
                    calleeFunctionIndex =
                            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                                state,
                                                                                                entry->function,
                                                                                                instructionIndex,
                                                                                                operandA1,
                                                                                                0);
                }
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(file,
                                                                             state,
                                                                             module,
                                                                             functionIr,
                                                                             instructionIndex,
                                                                             calleeFunctionIndex,
                                                                             ZR_FALSE)) {
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                    break;
                }
                if (calleeFunctionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    backend_aot_write_c_static_direct_function_call(file,
                                                                    destinationSlot,
                                                                    operandA1,
                                                                    operandB1,
                                                                    calleeFunctionIndex);
                } else if (semirDynamicCall ||
                           instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
                           instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)) {
                    backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, operandB1);
                } else {
                    backend_aot_write_c_direct_function_call(file, destinationSlot, operandA1, operandB1);
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                    instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL) ||
                    instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL) ||
                    instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            {
                const SZrAotExecIrInstruction *execIrInstruction =
                        backend_aot_find_exec_ir_instruction(module, functionIr, instructionIndex);
                TZrBool semirDynamicCall = backend_aot_exec_ir_instruction_is_dynamic_call(execIrInstruction);
                TZrUInt32 calleeFunctionIndex = backend_aot_get_callable_slot_function_index(callableSlotFunctionIndices,
                                                                                              entry->function,
                                                                                              operandA1);
                if (calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
                    calleeFunctionIndex =
                            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                                state,
                                                                                                entry->function,
                                                                                                instructionIndex,
                                                                                                operandA1,
                                                                                                0);
                }
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(file,
                                                                             state,
                                                                             module,
                                                                             functionIr,
                                                                             instructionIndex,
                                                                             calleeFunctionIndex,
                                                                             ZR_FALSE)) {
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                    break;
                }
                if (calleeFunctionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    backend_aot_write_c_static_direct_function_call(file,
                                                                    destinationSlot,
                                                                    operandA1,
                                                                    0,
                                                                    calleeFunctionIndex);
                } else if (semirDynamicCall) {
                    backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);
                } else {
                    backend_aot_write_c_direct_function_call(file, destinationSlot, operandA1, 0);
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) ||
                    instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS) ||
                    instruction->instruction.operationCode ==
                            ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(TO_INT):
                backend_aot_write_c_direct_to_int(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
                backend_aot_write_c_direct_mark_to_be_closed(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
                backend_aot_write_c_direct_close_scope(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                if (backend_aot_try_write_c_value_semir_for_exec_instruction(
                            file,
                            state,
                            module,
                            functionIr,
                            instructionIndex,
                            ZR_AOT_INVALID_FUNCTION_INDEX,
                            (TZrBool)(!publishExports && entry->function->exceptionHandlerCount == 0))) {
                    break;
                }
                if (publishExports) {
                    backend_aot_write_c_publish_exports(file);
                }
                backend_aot_write_c_direct_return(file, operandA1);
                break;
            default:
                backend_aot_write_c_unsupported_instruction(file,
                                                            entry->flatIndex,
                                                            instructionIndex,
                                                            instruction->instruction.operationCode);
                break;
        }
    }

    backend_aot_write_c_unsupported_instruction(file, entry->flatIndex, entry->function->instructionsLength, 0);
    fprintf(file, "zr_aot_function_exit:\n");
    fprintf(file, "    if (zr_aot_frame_started) {\n");
    if (functionIr != ZR_NULL) {
        backend_aot_write_c_frame_cleanup(file, &functionIr->frameLayout);
    }
    fprintf(file, "    }\n");
    fprintf(file, "    return zr_aot_return_value;\n");
    fprintf(file, "}\n");
    backend_aot_release_callable_slot_function_indices(state, entry->function, callableSlotFunctionIndices);
}
