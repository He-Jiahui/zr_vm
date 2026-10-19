#ifndef ZR_TEST_AOT_C_SCALAR_CALL_PROOF_OBSERVATION_H
#define ZR_TEST_AOT_C_SCALAR_CALL_PROOF_OBSERVATION_H

#include "backend_aot/backend_aot_c_scalar_call_result.h"
#include "backend_aot/backend_aot_c_scalar_locals.h"
#include "backend_aot/backend_aot_c_scalar_stack_copy.h"
#include "backend_aot/backend_aot_callable_provenance.h"

/* Observe the real failed fixture without changing its assertions or metadata. */
static void aot_c_guardrail_observe_scalar_call_proof(SZrState *state,
                                                    SZrFunction *function) {
    SZrAotExecIrModule module = {0};
    SZrAotFunctionTable table = {0};
    const SZrAotExecIrFunction *functionIr;

    if (!backend_aot_exec_ir_build_module(state, function, &module)) {
        fprintf(stderr, "AOT scalar proof prerequisite: module build failed\n");
        return;
    }
    if (!backend_aot_build_function_table(state, function, &table)) {
        fprintf(stderr, "AOT scalar proof prerequisite: function table failed\n");
        backend_aot_exec_ir_release_module(state, &module);
        return;
    }
    functionIr = backend_aot_exec_ir_find_function(&module, 0u);
    if (functionIr == ZR_NULL || functionIr->function != function) {
        fprintf(stderr, "AOT scalar proof prerequisite: root identity failed\n");
    } else {
        for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
            const TZrInstruction *instruction = &function->instructionsList[index];
            TZrUInt32 slot = instruction->instruction.operandExtra;
            TZrUInt32 calleeIndex;
            const SZrAotExecIrFunction *calleeIr;

            if (!backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot)) {
                continue;
            }
            fprintf(stderr,
                    "AOT scalar proof exec=%u opcode=%u dst=%u operand=%d "
                    "primitive=%u provenance_before=%u bool_written_before=%u "
                    "bool_value_before=%u local_source=%u skip_value=%u\n",
                    (unsigned)index,
                    (unsigned)instruction->instruction.operationCode,
                    (unsigned)slot,
                    (int)instruction->instruction.operand.operand2[0],
                    (unsigned)backend_aot_c_scalar_locals_instruction_writes_primitive(functionIr, index, slot),
                    (unsigned)backend_aot_c_scalar_stack_copy_has_scalar_provenance_before(functionIr, slot, index),
                    (unsigned)backend_aot_c_scalar_locals_bool_written_before(functionIr, slot, index),
                    (unsigned)backend_aot_c_scalar_locals_bool_value_written_before(functionIr, slot, index),
                    (unsigned)backend_aot_c_scalar_stack_copy_source_can_use_local(
                            functionIr, slot, index, ZR_STATIC_C_TYPE_BOOL),
                    (unsigned)backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, slot, index));

            if (instruction->instruction.operationCode == ZR_INSTRUCTION_OP_GET_STACK ||
                instruction->instruction.operationCode == ZR_INSTRUCTION_OP_SET_STACK) {
                TZrInt32 sourceSlot = instruction->instruction.operand.operand2[0];

                if (sourceSlot >= 0) {
                    fprintf(stderr,
                            "AOT scalar copy exec=%u dst=%u src=%u provenance=%u local_source=%u local_only=%u\n",
                            (unsigned)index, (unsigned)slot, (unsigned)sourceSlot,
                            (unsigned)backend_aot_c_scalar_stack_copy_has_scalar_provenance_before(
                                    functionIr, (TZrUInt32)sourceSlot, index),
                            (unsigned)backend_aot_c_scalar_stack_copy_source_can_use_local(
                                    functionIr, (TZrUInt32)sourceSlot, index, ZR_STATIC_C_TYPE_BOOL),
                            (unsigned)backend_aot_c_scalar_stack_copy_can_use_local_only(
                                    functionIr, slot, (TZrUInt32)sourceSlot, index));
                }
            }

            if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_CALL) &&
                instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL) &&
                instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS) &&
                instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)) {
                continue;
            }
            calleeIndex = backend_aot_resolve_callable_slot_function_index_before_instruction(
                    &table, state, function, index, instruction->instruction.operand.operand1[0], 0u);
            calleeIr = backend_aot_exec_ir_find_function(&module, calleeIndex);
            fprintf(stderr,
                    "AOT scalar call exec=%u dst=%u callee=%u name=%s nonprimitive=%u\n",
                    (unsigned)index,
                    (unsigned)slot,
                    (unsigned)calleeIndex,
                    calleeIr != ZR_NULL && calleeIr->function->functionName != ZR_NULL
                            ? ZrCore_String_GetNativeString(calleeIr->function->functionName) : "<unresolved>",
                    (unsigned)backend_aot_c_scalar_call_result_has_nonprimitive_type(
                            function, calleeIr != ZR_NULL ? calleeIr->function : ZR_NULL, index, slot));
            for (TZrUInt32 semIndex = 0u; semIndex < function->semIrInstructionLength; semIndex++) {
                const SZrSemIrInstruction *sem = &function->semIrInstructions[semIndex];
                const SZrFunctionTypedTypeRef *type;

                if (sem->execInstructionIndex != index || sem->destinationSlot != slot ||
                    sem->typeTableIndex >= function->semIrTypeTableLength) {
                    continue;
                }
                type = &function->semIrTypeTable[sem->typeTableIndex];
                fprintf(stderr,
                        "AOT scalar SemIR exec=%u row=%u opcode=%u dst=%u type=%u "
                        "base=%u static=%u owner=%u nullable=%u array=%u name=%s\n",
                        (unsigned)index, (unsigned)semIndex, (unsigned)sem->opcode,
                        (unsigned)slot, (unsigned)sem->typeTableIndex,
                        (unsigned)type->baseType, (unsigned)type->staticCType,
                        (unsigned)type->ownershipQualifier, (unsigned)type->isNullable,
                        (unsigned)type->isArray,
                        type->typeName != ZR_NULL ? ZrCore_String_GetNativeString(type->typeName) : "<none>");
            }
        }
    }
    backend_aot_release_function_table(state, &table);
    backend_aot_exec_ir_release_module(state, &module);
}

#endif
