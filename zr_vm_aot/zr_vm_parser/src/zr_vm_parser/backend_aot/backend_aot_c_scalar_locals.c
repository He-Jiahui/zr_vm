#include "backend_aot_c_scalar_locals.h"

#include <stdlib.h>

typedef enum EZrAotScalarLocalKind {
    ZR_AOT_SCALAR_LOCAL_KIND_NONE = 0,
    ZR_AOT_SCALAR_LOCAL_KIND_BOOL = 1u << 0,
    ZR_AOT_SCALAR_LOCAL_KIND_I64 = 1u << 1,
    ZR_AOT_SCALAR_LOCAL_KIND_U64 = 1u << 2,
    ZR_AOT_SCALAR_LOCAL_KIND_F64 = 1u << 3
} EZrAotScalarLocalKind;

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_static_type(EZrStaticCType staticCType) {
    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            return ZR_AOT_SCALAR_LOCAL_KIND_BOOL;
        case ZR_STATIC_C_TYPE_I8:
        case ZR_STATIC_C_TYPE_I16:
        case ZR_STATIC_C_TYPE_I32:
        case ZR_STATIC_C_TYPE_I64:
            return ZR_AOT_SCALAR_LOCAL_KIND_I64;
        case ZR_STATIC_C_TYPE_U8:
        case ZR_STATIC_C_TYPE_U16:
        case ZR_STATIC_C_TYPE_U32:
        case ZR_STATIC_C_TYPE_U64:
            return ZR_AOT_SCALAR_LOCAL_KIND_U64;
        case ZR_STATIC_C_TYPE_F32:
        case ZR_STATIC_C_TYPE_F64:
            return ZR_AOT_SCALAR_LOCAL_KIND_F64;
        default:
            return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }
}

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_type_ref(
        const SZrFunctionTypedTypeRef *typeRef) {
    EZrAotScalarLocalKind kind;

    if (typeRef == ZR_NULL) {
        return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }

    kind = backend_aot_c_scalar_locals_kind_from_static_type(typeRef->staticCType);
    if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        return kind;
    }

    switch (typeRef->baseType) {
        case ZR_VALUE_TYPE_BOOL:
            return ZR_AOT_SCALAR_LOCAL_KIND_BOOL;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return ZR_AOT_SCALAR_LOCAL_KIND_I64;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return ZR_AOT_SCALAR_LOCAL_KIND_U64;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return ZR_AOT_SCALAR_LOCAL_KIND_F64;
        default:
            return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }
}

static TZrBool backend_aot_c_scalar_locals_slot_is_valid(TZrUInt32 slot) {
    return slot != UINT32_MAX;
}

static void backend_aot_c_scalar_locals_record_slot(EZrAotScalarLocalKind *slotKinds,
                                                    TZrUInt32 slotCount,
                                                    TZrUInt32 slot,
                                                    EZrAotScalarLocalKind kind) {
    if (slotKinds == ZR_NULL ||
        kind == ZR_AOT_SCALAR_LOCAL_KIND_NONE ||
        !backend_aot_c_scalar_locals_slot_is_valid(slot) ||
        slot >= slotCount) {
        return;
    }

    slotKinds[slot] = (EZrAotScalarLocalKind)(slotKinds[slot] | kind);
}

static void backend_aot_c_scalar_locals_record_typed_locals(EZrAotScalarLocalKind *slotKinds,
                                                            TZrUInt32 slotCount,
                                                            const SZrFunction *function) {
    TZrUInt32 bindingIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return;
    }

    for (bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[bindingIndex];
        EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_from_type_ref(&binding->type);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, binding->stackSlot, kind);
    }
}

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_for_semir_instruction(
        const SZrFunction *function,
        const SZrSemIrInstruction *instruction) {
    if (function == ZR_NULL ||
        instruction == ZR_NULL ||
        function->semIrTypeTable == ZR_NULL ||
        instruction->typeTableIndex >= function->semIrTypeTableLength) {
        return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }

    return backend_aot_c_scalar_locals_kind_from_type_ref(&function->semIrTypeTable[instruction->typeTableIndex]);
}

static void backend_aot_c_scalar_locals_record_semir(EZrAotScalarLocalKind *slotKinds,
                                                     TZrUInt32 slotCount,
                                                     const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->semIrInstructionLength; instructionIndex++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[instructionIndex];
        EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_for_semir_instruction(function, instruction);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->destinationSlot, kind);
    }
}

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_conversion_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_TO_INT:
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
            return ZR_AOT_SCALAR_LOCAL_KIND_I64;
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_UINT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
            return ZR_AOT_SCALAR_LOCAL_KIND_U64;
        case ZR_INSTRUCTION_OP_TO_FLOAT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
            return ZR_AOT_SCALAR_LOCAL_KIND_F64;
        default:
            return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_stack_copy(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void backend_aot_c_scalar_locals_record_conversion_destinations(EZrAotScalarLocalKind *slotKinds,
                                                                       TZrUInt32 slotCount,
                                                                       const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_from_conversion_opcode(opcode);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
    }
}

static void backend_aot_c_scalar_locals_record_stack_copy_destinations(EZrAotScalarLocalKind *slotKinds,
                                                                       TZrUInt32 slotCount,
                                                                       const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrInt32 sourceSlotSigned = instruction->instruction.operand.operand2[0];
        TZrUInt32 sourceSlot;
        TZrUInt32 destinationSlot;
        EZrAotScalarLocalKind sourceKind;

        if (!backend_aot_c_scalar_locals_instruction_is_stack_copy(opcode) || sourceSlotSigned < 0) {
            continue;
        }

        sourceSlot = (TZrUInt32)sourceSlotSigned;
        destinationSlot = instruction->instruction.operandExtra;
        if (sourceSlot >= slotCount) {
            continue;
        }

        sourceKind = slotKinds[sourceSlot];
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, destinationSlot, sourceKind);
    }
}

static void backend_aot_c_scalar_locals_record_all(EZrAotScalarLocalKind *slotKinds,
                                                   TZrUInt32 slotCount,
                                                   const SZrFunction *function) {
    backend_aot_c_scalar_locals_record_typed_locals(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_semir(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_conversion_destinations(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_stack_copy_destinations(slotKinds, slotCount, function);
}

static const SZrTypeValue *backend_aot_c_scalar_locals_get_constant_value(const SZrFunction *function,
                                                                          TZrInt32 constantIndex) {
    if (function == ZR_NULL || constantIndex < 0 || (TZrUInt32)constantIndex >= function->constantValueLength ||
        function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    return &function->constantValueList[constantIndex];
}

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_immediate_constant(
        const SZrTypeValue *constantValue) {
    if (constantValue == ZR_NULL) {
        return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        return ZR_AOT_SCALAR_LOCAL_KIND_BOOL;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_AOT_SCALAR_LOCAL_KIND_I64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        return ZR_AOT_SCALAR_LOCAL_KIND_U64;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_AOT_SCALAR_LOCAL_KIND_F64;
    }

    return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
}

static TZrBool backend_aot_c_scalar_locals_find_exec_block_start(const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 execInstructionIndex,
                                                                 TZrUInt32 *outBlockStart) {
    TZrUInt32 blockIndex;

    if (functionIr == ZR_NULL || functionIr->basicBlocks == ZR_NULL || outBlockStart == ZR_NULL) {
        return ZR_FALSE;
    }

    for (blockIndex = 0u; blockIndex < functionIr->basicBlockCount; blockIndex++) {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 blockEnd = block->firstExecInstructionIndex + block->instructionCount;
        if (execInstructionIndex >= block->firstExecInstructionIndex && execInstructionIndex < blockEnd) {
            *outBlockStart = block->firstExecInstructionIndex;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_c_scalar_locals_record_semir_writes_for_exec_instruction(
        EZrAotScalarLocalKind *slotKinds,
        TZrUInt32 slotCount,
        const SZrFunction *function,
        TZrUInt32 execInstructionIndex) {
    TZrUInt32 semIrInstructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return;
    }

    for (semIrInstructionIndex = 0u;
         semIrInstructionIndex < function->semIrInstructionLength;
         semIrInstructionIndex++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[semIrInstructionIndex];
        EZrAotScalarLocalKind kind;

        if (instruction->execInstructionIndex != execInstructionIndex) {
            continue;
        }

        kind = backend_aot_c_scalar_locals_kind_for_semir_instruction(function, instruction);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->destinationSlot, kind);
    }
}

static void backend_aot_c_scalar_locals_record_exec_instruction_write(EZrAotScalarLocalKind *slotKinds,
                                                                      TZrUInt32 slotCount,
                                                                      const SZrFunction *function,
                                                                      TZrUInt32 execInstructionIndex) {
    const TZrInstruction *instruction;
    EZrInstructionCode opcode;
    EZrAotScalarLocalKind kind;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        execInstructionIndex >= function->instructionsLength) {
        return;
    }

    backend_aot_c_scalar_locals_record_semir_writes_for_exec_instruction(
            slotKinds, slotCount, function, execInstructionIndex);

    instruction = &function->instructionsList[execInstructionIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;

    if (opcode == ZR_INSTRUCTION_OP_GET_CONSTANT) {
        const SZrTypeValue *constantValue = backend_aot_c_scalar_locals_get_constant_value(
                function, instruction->instruction.operand.operand2[0]);
        kind = backend_aot_c_scalar_locals_kind_from_immediate_constant(constantValue);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
        return;
    }

    kind = backend_aot_c_scalar_locals_kind_from_conversion_opcode(opcode);
    if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
        return;
    }

    if (backend_aot_c_scalar_locals_instruction_is_stack_copy(opcode)) {
        TZrInt32 sourceSlotSigned = instruction->instruction.operand.operand2[0];
        TZrUInt32 sourceSlot;
        EZrAotScalarLocalKind sourceKind;

        if (sourceSlotSigned < 0) {
            return;
        }

        sourceSlot = (TZrUInt32)sourceSlotSigned;
        if (sourceSlot >= slotCount) {
            return;
        }

        sourceKind = slotKinds[sourceSlot];
        backend_aot_c_scalar_locals_record_slot(
                slotKinds, slotCount, instruction->instruction.operandExtra, sourceKind);
    }
}

static TZrUInt32 backend_aot_c_scalar_locals_slot_count(const SZrAotExecIrFunction *functionIr) {
    const SZrFunction *function;
    TZrUInt32 slotCount;
    TZrUInt32 index;

    if (functionIr == ZR_NULL) {
        return 0u;
    }

    function = functionIr->function;
    slotCount = functionIr->frameLayout.generatedFrameSlotCount;
    if (function != ZR_NULL && function->stackSize > slotCount) {
        slotCount = function->stackSize;
    }

    if (function != ZR_NULL && function->typedLocalBindings != ZR_NULL) {
        for (index = 0u; index < function->typedLocalBindingLength; index++) {
            TZrUInt32 slot = function->typedLocalBindings[index].stackSlot;
            if (backend_aot_c_scalar_locals_slot_is_valid(slot) && slot + 1u > slotCount) {
                slotCount = slot + 1u;
            }
        }
    }

    if (function != ZR_NULL && function->semIrInstructions != ZR_NULL) {
        for (index = 0u; index < function->semIrInstructionLength; index++) {
            TZrUInt32 slot = function->semIrInstructions[index].destinationSlot;
            if (backend_aot_c_scalar_locals_slot_is_valid(slot) && slot + 1u > slotCount) {
                slotCount = slot + 1u;
            }
        }
    }

    if (function != ZR_NULL && function->instructionsList != ZR_NULL) {
        for (index = 0u; index < function->instructionsLength; index++) {
            const TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_from_conversion_opcode(opcode);
            TZrUInt32 slot = instruction->instruction.operandExtra;
            TZrInt32 sourceSlotSigned = instruction->instruction.operand.operand2[0];
            if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE &&
                backend_aot_c_scalar_locals_slot_is_valid(slot) &&
                slot + 1u > slotCount) {
                slotCount = slot + 1u;
            }
            if (backend_aot_c_scalar_locals_instruction_is_stack_copy(opcode)) {
                if (backend_aot_c_scalar_locals_slot_is_valid(slot) && slot + 1u > slotCount) {
                    slotCount = slot + 1u;
                }
                if (sourceSlotSigned >= 0) {
                    TZrUInt32 sourceSlot = (TZrUInt32)sourceSlotSigned;
                    if (backend_aot_c_scalar_locals_slot_is_valid(sourceSlot) && sourceSlot + 1u > slotCount) {
                        slotCount = sourceSlot + 1u;
                    }
                }
            }
        }
    }

    return slotCount;
}

static TZrBool backend_aot_c_scalar_locals_has_any(const EZrAotScalarLocalKind *slotKinds, TZrUInt32 slotCount) {
    TZrUInt32 slot;

    if (slotKinds == ZR_NULL) {
        return ZR_FALSE;
    }

    for (slot = 0u; slot < slotCount; slot++) {
        if (slotKinds[slot] != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_c_scalar_locals_write_declaration(FILE *file,
                                                          TZrUInt32 slot,
                                                          EZrAotScalarLocalKind kind) {
    if ((kind & ZR_AOT_SCALAR_LOCAL_KIND_BOOL) != 0) {
        fprintf(file, "    TZrBool zr_aot_b%u = ZR_FALSE;\n", (unsigned)slot);
    }
    if ((kind & ZR_AOT_SCALAR_LOCAL_KIND_I64) != 0) {
        fprintf(file, "    TZrInt64 zr_aot_s%u = (TZrInt64)0;\n", (unsigned)slot);
    }
    if ((kind & ZR_AOT_SCALAR_LOCAL_KIND_U64) != 0) {
        fprintf(file, "    TZrUInt64 zr_aot_u%u = (TZrUInt64)0u;\n", (unsigned)slot);
    }
    if ((kind & ZR_AOT_SCALAR_LOCAL_KIND_F64) != 0) {
        fprintf(file, "    TZrFloat64 zr_aot_f%u = 0.0;\n", (unsigned)slot);
    }
}

void backend_aot_write_c_scalar_locals(FILE *file, const SZrAotExecIrFunction *functionIr) {
    const SZrFunction *function;
    EZrAotScalarLocalKind *slotKinds;
    TZrUInt32 slotCount;
    TZrUInt32 slot;

    if (file == ZR_NULL || functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return;
    }

    function = functionIr->function;
    slotCount = backend_aot_c_scalar_locals_slot_count(functionIr);
    if (slotCount == 0u) {
        return;
    }

    slotKinds = (EZrAotScalarLocalKind *)calloc((size_t)slotCount, sizeof(EZrAotScalarLocalKind));
    if (slotKinds == ZR_NULL) {
        return;
    }

    backend_aot_c_scalar_locals_record_all(slotKinds, slotCount, function);

    if (backend_aot_c_scalar_locals_has_any(slotKinds, slotCount)) {
        fprintf(file, "    /* zr_aot_scalar_locals_begin */\n");
        for (slot = 0u; slot < slotCount; slot++) {
            backend_aot_c_scalar_locals_write_declaration(file, slot, slotKinds[slot]);
        }
        fprintf(file, "    /* zr_aot_scalar_locals_end */\n");
    }

    free(slotKinds);
}

static TZrBool backend_aot_c_scalar_locals_has_slot_kind(const SZrAotExecIrFunction *functionIr,
                                                         TZrUInt32 slot,
                                                         EZrAotScalarLocalKind expectedKind) {
    const SZrFunction *function;
    EZrAotScalarLocalKind *slotKinds;
    EZrAotScalarLocalKind kind;
    TZrUInt32 slotCount;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        !backend_aot_c_scalar_locals_slot_is_valid(slot)) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    slotCount = backend_aot_c_scalar_locals_slot_count(functionIr);
    if (slot >= slotCount) {
        return ZR_FALSE;
    }

    slotKinds = (EZrAotScalarLocalKind *)calloc((size_t)slotCount, sizeof(EZrAotScalarLocalKind));
    if (slotKinds == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_c_scalar_locals_record_all(slotKinds, slotCount, function);
    kind = slotKinds[slot];
    free(slotKinds);

    return (TZrBool)((kind & expectedKind) == expectedKind);
}

TZrBool backend_aot_c_scalar_locals_has_bool_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot) {
    return backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, ZR_AOT_SCALAR_LOCAL_KIND_BOOL);
}

TZrBool backend_aot_c_scalar_locals_has_f64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot) {
    return backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, ZR_AOT_SCALAR_LOCAL_KIND_F64);
}

TZrBool backend_aot_c_scalar_locals_has_i64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot) {
    return backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, ZR_AOT_SCALAR_LOCAL_KIND_I64);
}

TZrBool backend_aot_c_scalar_locals_has_u64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot) {
    return backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, ZR_AOT_SCALAR_LOCAL_KIND_U64);
}

TZrBool backend_aot_c_scalar_locals_i64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    EZrAotScalarLocalKind *writtenKinds;
    TZrUInt32 slotCount;
    TZrUInt32 blockStart;
    TZrUInt32 instructionIndex;
    TZrBool result;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        !backend_aot_c_scalar_locals_slot_is_valid(slot)) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    slotCount = backend_aot_c_scalar_locals_slot_count(functionIr);
    if (slot >= slotCount ||
        !backend_aot_c_scalar_locals_find_exec_block_start(functionIr, execInstructionIndex, &blockStart)) {
        return ZR_FALSE;
    }

    writtenKinds = (EZrAotScalarLocalKind *)calloc((size_t)slotCount, sizeof(EZrAotScalarLocalKind));
    if (writtenKinds == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = blockStart; instructionIndex < execInstructionIndex; instructionIndex++) {
        backend_aot_c_scalar_locals_record_exec_instruction_write(
                writtenKinds, slotCount, function, instructionIndex);
    }

    result = (TZrBool)((writtenKinds[slot] & ZR_AOT_SCALAR_LOCAL_KIND_I64) == ZR_AOT_SCALAR_LOCAL_KIND_I64);
    free(writtenKinds);
    return result;
}
