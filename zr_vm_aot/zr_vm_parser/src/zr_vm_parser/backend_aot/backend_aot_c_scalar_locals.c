#include "backend_aot_c_scalar_locals.h"

#include <stdlib.h>

#include "zr_vm_core/string.h"

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

static TZrBool backend_aot_c_scalar_locals_record_slot_changed(EZrAotScalarLocalKind *slotKinds,
                                                               TZrUInt32 slotCount,
                                                               TZrUInt32 slot,
                                                               EZrAotScalarLocalKind kind);

static void backend_aot_c_scalar_locals_record_slot(EZrAotScalarLocalKind *slotKinds,
                                                    TZrUInt32 slotCount,
                                                    TZrUInt32 slot,
                                                    EZrAotScalarLocalKind kind) {
    (void)backend_aot_c_scalar_locals_record_slot_changed(slotKinds, slotCount, slot, kind);
}

static TZrBool backend_aot_c_scalar_locals_record_slot_changed(EZrAotScalarLocalKind *slotKinds,
                                                               TZrUInt32 slotCount,
                                                               TZrUInt32 slot,
                                                               EZrAotScalarLocalKind kind) {
    EZrAotScalarLocalKind previousKind;
    EZrAotScalarLocalKind nextKind;

    if (slotKinds == ZR_NULL ||
        kind == ZR_AOT_SCALAR_LOCAL_KIND_NONE ||
        !backend_aot_c_scalar_locals_slot_is_valid(slot) ||
        slot >= slotCount) {
        return ZR_FALSE;
    }

    previousKind = slotKinds[slot];
    nextKind = (EZrAotScalarLocalKind)(previousKind | kind);
    slotKinds[slot] = nextKind;
    return (TZrBool)(nextKind != previousKind);
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
        case ZR_INSTRUCTION_OP_TO_BOOL:
            return ZR_AOT_SCALAR_LOCAL_KIND_BOOL;
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

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_power_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_POW_SIGNED:
            return ZR_AOT_SCALAR_LOCAL_KIND_I64;
        case ZR_INSTRUCTION_OP_POW_UNSIGNED:
            return ZR_AOT_SCALAR_LOCAL_KIND_U64;
        case ZR_INSTRUCTION_OP_POW_FLOAT:
            return ZR_AOT_SCALAR_LOCAL_KIND_F64;
        default:
            return ZR_AOT_SCALAR_LOCAL_KIND_NONE;
    }
}

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_result_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_NEG_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED:
        case ZR_INSTRUCTION_OP_MOD_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST_PLAIN_DEST:
            return ZR_AOT_SCALAR_LOCAL_KIND_I64;

        case ZR_INSTRUCTION_OP_ADD_UNSIGNED:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST_PLAIN_DEST:
            return ZR_AOT_SCALAR_LOCAL_KIND_U64;

        case ZR_INSTRUCTION_OP_ADD_FLOAT:
        case ZR_INSTRUCTION_OP_NEG_FLOAT:
        case ZR_INSTRUCTION_OP_SUB_FLOAT:
        case ZR_INSTRUCTION_OP_MUL_FLOAT:
        case ZR_INSTRUCTION_OP_DIV_FLOAT:
        case ZR_INSTRUCTION_OP_MOD_FLOAT:
            return ZR_AOT_SCALAR_LOCAL_KIND_F64;

        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_FLOAT:
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
            return ZR_AOT_SCALAR_LOCAL_KIND_BOOL;

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

static TZrBool backend_aot_c_scalar_locals_instruction_is_call_result_write(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_slot_is_call_result_destination(const SZrFunction *function,
                                                                           TZrUInt32 slot) {
    TZrUInt32 instructionIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        if (backend_aot_c_scalar_locals_instruction_is_call_result_write(opcode) &&
            instruction->instruction.operandExtra == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrTypeValue *backend_aot_c_scalar_locals_get_constant_value(const SZrFunction *function,
                                                                          TZrInt32 constantIndex);

static EZrAotScalarLocalKind backend_aot_c_scalar_locals_kind_from_immediate_constant(
        const SZrTypeValue *constantValue);

static TZrBool backend_aot_c_scalar_locals_slot_is_power_operand(const SZrFunction *function,
                                                                 TZrUInt32 slot) {
    TZrUInt32 instructionIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (backend_aot_c_scalar_locals_kind_from_power_opcode(opcode) == ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
            continue;
        }

        if (instruction->instruction.operand.operand1[0] == slot ||
            instruction->instruction.operand.operand1[1] == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
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

static void backend_aot_c_scalar_locals_record_immediate_constant_destinations(EZrAotScalarLocalKind *slotKinds,
                                                                               TZrUInt32 slotCount,
                                                                               const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        const SZrTypeValue *constantValue;
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode != ZR_INSTRUCTION_OP_GET_CONSTANT) {
            continue;
        }

        if (!backend_aot_c_scalar_locals_slot_is_power_operand(function, instruction->instruction.operandExtra)) {
            continue;
        }

        constantValue = backend_aot_c_scalar_locals_get_constant_value(
                function, instruction->instruction.operand.operand2[0]);
        backend_aot_c_scalar_locals_record_slot(slotKinds,
                                                slotCount,
                                                instruction->instruction.operandExtra,
                                                backend_aot_c_scalar_locals_kind_from_immediate_constant(constantValue));
    }
}

static void backend_aot_c_scalar_locals_record_power_destinations(EZrAotScalarLocalKind *slotKinds,
                                                                  TZrUInt32 slotCount,
                                                                  const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_from_power_opcode(opcode);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
    }
}

static void backend_aot_c_scalar_locals_record_result_destinations(EZrAotScalarLocalKind *slotKinds,
                                                                   TZrUInt32 slotCount,
                                                                   const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (slotKinds == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrAotScalarLocalKind kind = backend_aot_c_scalar_locals_kind_from_result_opcode(opcode);
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
        if (sourceSlot >= slotCount || destinationSlot >= slotCount) {
            continue;
        }

        sourceKind = slotKinds[sourceSlot];
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, destinationSlot, sourceKind);
        if (backend_aot_c_scalar_locals_slot_is_call_result_destination(function, sourceSlot)) {
            backend_aot_c_scalar_locals_record_slot(
                    slotKinds, slotCount, sourceSlot, slotKinds[destinationSlot]);
        }
    }
}

static void backend_aot_c_scalar_locals_record_all(EZrAotScalarLocalKind *slotKinds,
                                                   TZrUInt32 slotCount,
                                                   const SZrFunction *function) {
    backend_aot_c_scalar_locals_record_typed_locals(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_semir(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_immediate_constant_destinations(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_conversion_destinations(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_power_destinations(slotKinds, slotCount, function);
    backend_aot_c_scalar_locals_record_result_destinations(slotKinds, slotCount, function);
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

static TZrBool backend_aot_c_scalar_locals_find_exec_block_bounds(const SZrAotExecIrFunction *functionIr,
                                                                  TZrUInt32 execInstructionIndex,
                                                                  TZrUInt32 *outBlockStart,
                                                                  TZrUInt32 *outBlockEnd);

static TZrBool backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 *outBlockIndex,
        TZrUInt32 *outBlockStart,
        TZrUInt32 *outBlockEnd);

static TZrBool backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex,
        EZrAotScalarLocalKind expectedKind);

static TZrBool backend_aot_c_scalar_locals_find_exec_block_bounds(const SZrAotExecIrFunction *functionIr,
                                                                  TZrUInt32 execInstructionIndex,
                                                                  TZrUInt32 *outBlockStart,
                                                                  TZrUInt32 *outBlockEnd) {
    TZrUInt32 blockIndex;

    return backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
            functionIr, execInstructionIndex, &blockIndex, outBlockStart, outBlockEnd);
}

static TZrBool backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 *outBlockIndex,
        TZrUInt32 *outBlockStart,
        TZrUInt32 *outBlockEnd) {
    TZrUInt32 blockIndex;

    if (functionIr == ZR_NULL ||
        functionIr->basicBlocks == ZR_NULL ||
        outBlockIndex == ZR_NULL ||
        outBlockStart == ZR_NULL ||
        outBlockEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    for (blockIndex = 0u; blockIndex < functionIr->basicBlockCount; blockIndex++) {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 blockEnd = block->firstExecInstructionIndex + block->instructionCount;
        if (execInstructionIndex >= block->firstExecInstructionIndex && execInstructionIndex < blockEnd) {
            *outBlockIndex = blockIndex;
            *outBlockStart = block->firstExecInstructionIndex;
            *outBlockEnd = blockEnd;
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
                                                                      const EZrAotScalarLocalKind *declaredSlotKinds,
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
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        const SZrTypeValue *constantValue = backend_aot_c_scalar_locals_get_constant_value(
                function, instruction->instruction.operand.operand2[0]);
        kind = backend_aot_c_scalar_locals_kind_from_immediate_constant(constantValue);
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, destinationSlot, kind);
        if (constantValue != ZR_NULL &&
            ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
            declaredSlotKinds != ZR_NULL &&
            destinationSlot < slotCount &&
            (declaredSlotKinds[destinationSlot] & ZR_AOT_SCALAR_LOCAL_KIND_U64) != 0) {
            backend_aot_c_scalar_locals_record_slot(
                    slotKinds, slotCount, destinationSlot, ZR_AOT_SCALAR_LOCAL_KIND_U64);
        }
        return;
    }

    kind = backend_aot_c_scalar_locals_kind_from_result_opcode(opcode);
    if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
        return;
    }

    kind = backend_aot_c_scalar_locals_kind_from_conversion_opcode(opcode);
    if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
        return;
    }

    kind = backend_aot_c_scalar_locals_kind_from_power_opcode(opcode);
    if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->instruction.operandExtra, kind);
        return;
    }

    if (backend_aot_c_scalar_locals_instruction_is_call_result_write(opcode)) {
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        if (declaredSlotKinds != ZR_NULL && destinationSlot < slotCount) {
            backend_aot_c_scalar_locals_record_slot(
                    slotKinds, slotCount, destinationSlot, declaredSlotKinds[destinationSlot]);
        }
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
        if (sourceKind == ZR_AOT_SCALAR_LOCAL_KIND_NONE &&
            declaredSlotKinds != ZR_NULL &&
            sourceSlot < slotCount) {
            sourceKind = declaredSlotKinds[sourceSlot];
        }
        backend_aot_c_scalar_locals_record_slot(
                slotKinds, slotCount, instruction->instruction.operandExtra, sourceKind);
    }
}

static TZrBool backend_aot_c_scalar_locals_range_writes_kind(const SZrFunction *function,
                                                             TZrUInt32 slotCount,
                                                             TZrUInt32 slot,
                                                             TZrUInt32 firstExecInstructionIndex,
                                                             TZrUInt32 endExecInstructionIndex,
                                                             EZrAotScalarLocalKind expectedKind) {
    EZrAotScalarLocalKind *writtenKinds;
    EZrAotScalarLocalKind *declaredKinds;
    TZrUInt32 instructionIndex;
    TZrBool result;

    if (function == ZR_NULL ||
        slot >= slotCount ||
        firstExecInstructionIndex > endExecInstructionIndex ||
        expectedKind == ZR_AOT_SCALAR_LOCAL_KIND_NONE) {
        return ZR_FALSE;
    }

    writtenKinds = (EZrAotScalarLocalKind *)calloc((size_t)slotCount, sizeof(EZrAotScalarLocalKind));
    declaredKinds = (EZrAotScalarLocalKind *)calloc((size_t)slotCount, sizeof(EZrAotScalarLocalKind));
    if (writtenKinds == ZR_NULL || declaredKinds == ZR_NULL) {
        free(writtenKinds);
        free(declaredKinds);
        return ZR_FALSE;
    }

    backend_aot_c_scalar_locals_record_all(declaredKinds, slotCount, function);
    for (instructionIndex = firstExecInstructionIndex; instructionIndex < endExecInstructionIndex; instructionIndex++) {
        backend_aot_c_scalar_locals_record_exec_instruction_write(
                writtenKinds, slotCount, declaredKinds, function, instructionIndex);
    }

    result = (TZrBool)((writtenKinds[slot] & expectedKind) == expectedKind);
    free(writtenKinds);
    free(declaredKinds);
    return result;
}

static void backend_aot_c_scalar_locals_mark_reachable_blocks(const SZrAotExecIrFunction *functionIr,
                                                              TZrBool *reachableBlocks) {
    TZrBool changed;

    if (functionIr == ZR_NULL ||
        functionIr->basicBlocks == ZR_NULL ||
        functionIr->basicBlockCount == 0u ||
        reachableBlocks == ZR_NULL) {
        return;
    }

    reachableBlocks[0] = ZR_TRUE;
    do {
        TZrUInt32 blockIndex;

        changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < functionIr->basicBlockCount; blockIndex++) {
            const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
            TZrUInt32 successorIndex;

            if (!reachableBlocks[blockIndex]) {
                continue;
            }

            for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
                TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
                if (successorBlockIndex < functionIr->basicBlockCount &&
                    !reachableBlocks[successorBlockIndex]) {
                    reachableBlocks[successorBlockIndex] = ZR_TRUE;
                    changed = ZR_TRUE;
                }
            }
        }
    } while (changed);
}

static TZrBool backend_aot_c_scalar_locals_block_entry_has_kind(const SZrAotExecIrFunction *functionIr,
                                                                const TZrBool *reachableBlocks,
                                                                const TZrBool *blockOut,
                                                                TZrUInt32 blockIndex) {
    TZrUInt32 predecessorIndex;
    TZrBool hasReachablePredecessor = ZR_FALSE;
    TZrBool result = ZR_TRUE;

    if (functionIr == ZR_NULL ||
        functionIr->basicBlocks == ZR_NULL ||
        reachableBlocks == ZR_NULL ||
        blockOut == ZR_NULL ||
        blockIndex >= functionIr->basicBlockCount ||
        blockIndex == 0u ||
        !reachableBlocks[blockIndex]) {
        return ZR_FALSE;
    }

    for (predecessorIndex = 0u; predecessorIndex < functionIr->basicBlockCount; predecessorIndex++) {
        const SZrAotExecIrBasicBlock *predecessor = &functionIr->basicBlocks[predecessorIndex];
        TZrUInt32 successorIndex;

        if (!reachableBlocks[predecessorIndex]) {
            continue;
        }

        for (successorIndex = 0u; successorIndex < predecessor->successorCount; successorIndex++) {
            if (predecessor->successorBlockIndices[successorIndex] != blockIndex) {
                continue;
            }

            hasReachablePredecessor = ZR_TRUE;
            if (!blockOut[predecessorIndex]) {
                result = ZR_FALSE;
            }
            break;
        }
    }

    return (TZrBool)(hasReachablePredecessor && result);
}

static TZrBool backend_aot_c_scalar_locals_kind_written_at_block_entry(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 slotCount,
        TZrUInt32 slot,
        TZrUInt32 targetBlockIndex,
        EZrAotScalarLocalKind expectedKind) {
    TZrBool *blockWrites;
    TZrBool *blockIn;
    TZrBool *blockOut;
    TZrBool *reachableBlocks;
    TZrUInt32 blockIndex;
    TZrBool changed;
    TZrBool result;

    if (functionIr == ZR_NULL ||
        function == ZR_NULL ||
        functionIr->basicBlocks == ZR_NULL ||
        functionIr->basicBlockCount == 0u ||
        targetBlockIndex >= functionIr->basicBlockCount ||
        slot >= slotCount) {
        return ZR_FALSE;
    }

    blockWrites = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    blockIn = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    blockOut = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    reachableBlocks = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    if (blockWrites == ZR_NULL || blockIn == ZR_NULL || blockOut == ZR_NULL || reachableBlocks == ZR_NULL) {
        free(blockWrites);
        free(blockIn);
        free(blockOut);
        free(reachableBlocks);
        return ZR_FALSE;
    }

    backend_aot_c_scalar_locals_mark_reachable_blocks(functionIr, reachableBlocks);
    for (blockIndex = 0u; blockIndex < functionIr->basicBlockCount; blockIndex++) {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 blockEnd = block->firstExecInstructionIndex + block->instructionCount;
        blockWrites[blockIndex] = backend_aot_c_scalar_locals_range_writes_kind(function,
                                                                                slotCount,
                                                                                slot,
                                                                                block->firstExecInstructionIndex,
                                                                                blockEnd,
                                                                                expectedKind);
    }

    do {
        changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < functionIr->basicBlockCount; blockIndex++) {
            TZrBool newIn = backend_aot_c_scalar_locals_block_entry_has_kind(
                    functionIr, reachableBlocks, blockOut, blockIndex);
            TZrBool newOut = (TZrBool)(newIn || blockWrites[blockIndex]);

            if (blockIn[blockIndex] != newIn || blockOut[blockIndex] != newOut) {
                blockIn[blockIndex] = newIn;
                blockOut[blockIndex] = newOut;
                changed = ZR_TRUE;
            }
        }
    } while (changed);

    result = blockIn[targetBlockIndex];
    free(blockWrites);
    free(blockIn);
    free(blockOut);
    free(reachableBlocks);
    return result;
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
            EZrAotScalarLocalKind resultKind = backend_aot_c_scalar_locals_kind_from_result_opcode(opcode);
            TZrUInt32 slot = instruction->instruction.operandExtra;
            TZrInt32 sourceSlotSigned = instruction->instruction.operand.operand2[0];
            if (kind != ZR_AOT_SCALAR_LOCAL_KIND_NONE &&
                backend_aot_c_scalar_locals_slot_is_valid(slot) &&
                slot + 1u > slotCount) {
                slotCount = slot + 1u;
            }
            if (resultKind != ZR_AOT_SCALAR_LOCAL_KIND_NONE &&
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

static TZrBool backend_aot_c_scalar_locals_kind_written_before(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex,
                                                               EZrAotScalarLocalKind expectedKind) {
    const SZrFunction *function;
    TZrUInt32 slotCount;
    TZrUInt32 blockIndex;
    TZrUInt32 blockStart;
    TZrUInt32 blockEnd;
    TZrBool result;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        !backend_aot_c_scalar_locals_slot_is_valid(slot)) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    slotCount = backend_aot_c_scalar_locals_slot_count(functionIr);
    if (slot >= slotCount ||
        !backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
                functionIr, execInstructionIndex, &blockIndex, &blockStart, &blockEnd)) {
        return ZR_FALSE;
    }

    (void)blockEnd;
    result = backend_aot_c_scalar_locals_range_writes_kind(function,
                                                          slotCount,
                                                          slot,
                                                          blockStart,
                                                          execInstructionIndex,
                                                          expectedKind);
    if (!result) {
        result = backend_aot_c_scalar_locals_kind_written_at_block_entry(
                functionIr, function, slotCount, slot, blockIndex, expectedKind);
    }
    return result;
}

TZrBool backend_aot_c_scalar_locals_bool_written_before(const SZrAotExecIrFunction *functionIr,
                                                        TZrUInt32 slot,
                                                        TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_kind_written_before(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_BOOL);
}

TZrBool backend_aot_c_scalar_locals_f64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_kind_written_before(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_F64);
}

TZrBool backend_aot_c_scalar_locals_i64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_kind_written_before(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_I64);
}

TZrBool backend_aot_c_scalar_locals_u64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_kind_written_before(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_U64);
}

static TZrBool backend_aot_c_scalar_locals_function_exports_slot(const SZrFunction *function, TZrUInt32 slot) {
    TZrUInt32 exportIndex;

    if (function == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (exportIndex = 0u; exportIndex < function->exportedVariableLength; exportIndex++) {
        if (function->exportedVariables[exportIndex].stackSlot == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_function_is_constructor(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(ZrCore_NativeString_Compare(ZrCore_String_GetNativeString(function->functionName),
                                                 "constructor") == 0);
}

static TZrBool backend_aot_c_scalar_locals_can_return_kind_local(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex,
        EZrAotScalarLocalKind expectedKind,
        TZrBool requireCallableReturnType) {
    const SZrFunction *function;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (!backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, expectedKind) ||
        function->exceptionHandlerCount > 0 ||
        backend_aot_c_scalar_locals_function_exports_slot(function, slot) ||
        backend_aot_c_scalar_locals_function_is_constructor(function)) {
        return ZR_FALSE;
    }

    if (requireCallableReturnType &&
        (!function->hasCallableReturnType ||
         backend_aot_c_scalar_locals_kind_from_type_ref(&function->callableReturnType) != expectedKind)) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_kind_written_before(functionIr,
                                                           slot,
                                                           execInstructionIndex,
                                                           expectedKind);
}

TZrBool backend_aot_c_scalar_locals_can_direct_return_i64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_I64,
                                                             ZR_FALSE);
}

TZrBool backend_aot_c_scalar_locals_can_direct_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 slot,
                                                                 TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_BOOL,
                                                             ZR_TRUE);
}

TZrBool backend_aot_c_scalar_locals_can_direct_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_U64,
                                                             ZR_TRUE);
}

TZrBool backend_aot_c_scalar_locals_can_direct_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_F64,
                                                             ZR_TRUE);
}

TZrBool backend_aot_c_scalar_locals_can_infer_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_BOOL,
                                                             ZR_FALSE);
}

TZrBool backend_aot_c_scalar_locals_can_infer_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_U64,
                                                             ZR_FALSE);
}

TZrBool backend_aot_c_scalar_locals_can_infer_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_can_return_kind_local(functionIr,
                                                             slot,
                                                             execInstructionIndex,
                                                             ZR_AOT_SCALAR_LOCAL_KIND_F64,
                                                             ZR_FALSE);
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_NEG_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED:
        case ZR_INSTRUCTION_OP_MOD_SIGNED:
        case ZR_INSTRUCTION_OP_POW_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED_CONST:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_signed_consumer_reads_slot(const TZrInstruction *instruction,
                                                                      TZrUInt32 slot) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_NEG_SIGNED:
            return (TZrBool)(leftSlot == slot);
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED_CONST:
            return (TZrBool)(leftSlot == slot);
        default:
            break;
    }

    return (TZrBool)(leftSlot == slot || rightSlot == slot);
}

static TZrBool backend_aot_c_scalar_locals_signed_consumer_destination_kind(
        EZrInstructionCode opcode,
        EZrAotScalarLocalKind *outKind) {
    if (outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_NEG_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED:
        case ZR_INSTRUCTION_OP_MOD_SIGNED:
        case ZR_INSTRUCTION_OP_POW_SIGNED:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST_PLAIN_DEST:
            *outKind = ZR_AOT_SCALAR_LOCAL_KIND_I64;
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_SIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED_CONST:
            *outKind = ZR_AOT_SCALAR_LOCAL_KIND_BOOL;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_signed_consumer_has_i64_operand_locals(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_NEG_SIGNED:
            return backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot);
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_SIGNED_CONST:
            return backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot);
        default:
            break;
    }

    return (TZrBool)(backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot));
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_i64_local_consumer(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return ZR_TRUE;
        default:
            return backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode);
    }
}

static TZrBool backend_aot_c_scalar_locals_i64_consumer_reads_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode)) {
        EZrAotScalarLocalKind destinationKind;

        return (TZrBool)(backend_aot_c_scalar_locals_signed_consumer_destination_kind(opcode, &destinationKind) &&
                         backend_aot_c_scalar_locals_has_slot_kind(
                                 functionIr, instruction->instruction.operandExtra, destinationKind) &&
                         backend_aot_c_scalar_locals_signed_consumer_has_i64_operand_locals(functionIr, instruction) &&
                         backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot) &&
                         backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex));
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return (TZrBool)((instruction->instruction.operandExtra == slot ||
                              instruction->instruction.operand.operand1[0] == slot) &&
                             backend_aot_c_scalar_locals_i64_written_before(
                                     functionIr, instruction->instruction.operandExtra, instructionIndex) &&
                             backend_aot_c_scalar_locals_i64_written_before(
                                     functionIr, instruction->instruction.operand.operand1[0], instructionIndex));
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return (TZrBool)(instruction->instruction.operandExtra == slot &&
                             backend_aot_c_scalar_locals_i64_written_before(
                                     functionIr, instruction->instruction.operandExtra, instructionIndex));
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot &&
                             backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
                             backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex));
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            return (TZrBool)((instruction->instruction.operand.operand1[0] == slot ||
                              instruction->instruction.operand.operand1[1] == slot) &&
                             backend_aot_c_scalar_locals_has_i64_slot(functionIr, instruction->instruction.operandExtra) &&
                             backend_aot_c_scalar_locals_has_i64_slot(functionIr, instruction->instruction.operand.operand1[0]) &&
                             backend_aot_c_scalar_locals_has_i64_slot(functionIr, instruction->instruction.operand.operand1[1]) &&
                             backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex));
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            return (TZrBool)(((instruction->instruction.operand.operand1[1] == slot &&
                               backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot)) ||
                              (instruction->instruction.operand.operand1[0] == slot &&
                               backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
                               backend_aot_c_scalar_locals_has_i64_slot(
                                       functionIr, instruction->instruction.operandExtra))) &&
                             backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex));
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot &&
                             backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex));
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot &&
                             backend_aot_c_scalar_locals_has_i64_slot(
                                     functionIr, instruction->instruction.operandExtra));
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_i64_consumer_mentions_slot(const TZrInstruction *instruction,
                                                                      TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode)) {
        return backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot);
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return (TZrBool)(instruction->instruction.operandExtra == slot ||
                             instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return (TZrBool)(instruction->instruction.operandExtra == slot);
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                             instruction->instruction.operand.operand1[1] == slot);
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                             instruction->instruction.operand.operand1[1] == slot);
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_bool_local_consumer(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_bool_consumer_reads_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(instruction->instruction.operandExtra == slot);
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                             instruction->instruction.operand.operand1[1] == slot);
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot &&
                             backend_aot_c_scalar_locals_has_bool_slot(
                                     functionIr, instruction->instruction.operandExtra));
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_bool_consumer_mentions_slot(const TZrInstruction *instruction,
                                                                       TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(instruction->instruction.operandExtra == slot);
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                             instruction->instruction.operand.operand1[1] == slot);
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_FLOAT:
        case ZR_INSTRUCTION_OP_NEG_FLOAT:
        case ZR_INSTRUCTION_OP_SUB_FLOAT:
        case ZR_INSTRUCTION_OP_MUL_FLOAT:
        case ZR_INSTRUCTION_OP_DIV_FLOAT:
        case ZR_INSTRUCTION_OP_MOD_FLOAT:
        case ZR_INSTRUCTION_OP_POW_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_UINT_FLOAT:
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_f64_consumer_reads_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_FLOAT:
        case ZR_INSTRUCTION_OP_SUB_FLOAT:
        case ZR_INSTRUCTION_OP_MUL_FLOAT:
        case ZR_INSTRUCTION_OP_DIV_FLOAT:
        case ZR_INSTRUCTION_OP_MOD_FLOAT:
        case ZR_INSTRUCTION_OP_POW_FLOAT:
            return (TZrBool)((leftSlot == slot || rightSlot == slot) &&
                             backend_aot_c_scalar_locals_has_f64_slot(
                                     functionIr, instruction->instruction.operandExtra) &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot));
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_FLOAT:
            return (TZrBool)((leftSlot == slot || rightSlot == slot) &&
                             backend_aot_c_scalar_locals_has_bool_slot(
                                     functionIr, instruction->instruction.operandExtra) &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot));
        case ZR_INSTRUCTION_OP_NEG_FLOAT:
            return (TZrBool)(leftSlot == slot &&
                             backend_aot_c_scalar_locals_has_f64_slot(
                                     functionIr, instruction->instruction.operandExtra) &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot));
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_UINT_FLOAT:
            return (TZrBool)(leftSlot == slot &&
                             backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot));
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot &&
                             backend_aot_c_scalar_locals_has_f64_slot(
                                     functionIr, instruction->instruction.operandExtra));
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_f64_consumer_mentions_slot(const TZrInstruction *instruction,
                                                                      TZrUInt32 slot) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_FLOAT:
        case ZR_INSTRUCTION_OP_SUB_FLOAT:
        case ZR_INSTRUCTION_OP_MUL_FLOAT:
        case ZR_INSTRUCTION_OP_DIV_FLOAT:
        case ZR_INSTRUCTION_OP_MOD_FLOAT:
        case ZR_INSTRUCTION_OP_POW_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_FLOAT:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_FLOAT:
            return (TZrBool)(leftSlot == slot || rightSlot == slot);
        case ZR_INSTRUCTION_OP_NEG_FLOAT:
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_UINT_FLOAT:
            return (TZrBool)(leftSlot == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED:
        case ZR_INSTRUCTION_OP_POW_UNSIGNED:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_BITWISE_NOT:
        case ZR_INSTRUCTION_OP_BITWISE_AND:
        case ZR_INSTRUCTION_OP_BITWISE_OR:
        case ZR_INSTRUCTION_OP_BITWISE_XOR:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT_INT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT_INT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_LOGICAL_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_NOT_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_LESS_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_UNSIGNED:
        case ZR_INSTRUCTION_OP_LOGICAL_GREATER_EQUAL_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_u64_consumer_reads_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_BITWISE_NOT:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT_INT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT_INT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
            return (TZrBool)(leftSlot == slot);
        case ZR_INSTRUCTION_OP_TO_UINT:
            return (TZrBool)(leftSlot == slot &&
                             backend_aot_c_scalar_locals_has_u64_slot(
                                     functionIr, instruction->instruction.operandExtra));
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot &&
                             backend_aot_c_scalar_locals_has_u64_slot(
                                     functionIr, instruction->instruction.operandExtra));
        default:
            break;
    }

    return (TZrBool)(leftSlot == slot || rightSlot == slot);
}

static TZrBool backend_aot_c_scalar_locals_u64_consumer_mentions_slot(const TZrInstruction *instruction,
                                                                      TZrUInt32 slot) {
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_ADD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_SUB_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MUL_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_DIV_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST:
        case ZR_INSTRUCTION_OP_MOD_UNSIGNED_CONST_PLAIN_DEST:
        case ZR_INSTRUCTION_OP_BITWISE_NOT:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_SHIFT_LEFT_INT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_SHIFT_RIGHT_INT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_LEFT:
        case ZR_INSTRUCTION_OP_BITWISE_SHIFT_RIGHT:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT:
            return (TZrBool)(leftSlot == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
        default:
            break;
    }

    return (TZrBool)(leftSlot == slot || rightSlot == slot);
}

static TZrBool backend_aot_c_scalar_locals_instruction_mentions_slot_as_operand(const TZrInstruction *instruction,
                                                                                TZrUInt32 slot) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
        case ZR_INSTRUCTION_OP_RESET_STACK_NULL:
        case ZR_INSTRUCTION_OP_RESET_STACK_NULL2:
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
        case ZR_INSTRUCTION_ENUM(JUMP):
            return ZR_FALSE;
        case ZR_INSTRUCTION_OP_FUNCTION_RETURN:
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        default:
            break;
    }

    return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                     instruction->instruction.operand.operand1[1] == slot ||
                     instruction->instruction.operand.operand0[0] == slot ||
                     instruction->instruction.operand.operand0[1] == slot ||
                     instruction->instruction.operand.operand0[2] == slot ||
                     instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
}

static TZrBool backend_aot_c_scalar_locals_instruction_resets_slot(const TZrInstruction *instruction,
                                                                   TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
            return (TZrBool)(instruction->instruction.operandExtra == slot);
        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):
            return (TZrBool)(instruction->instruction.operandExtra == slot ||
                             instruction->instruction.operand.operand1[0] == slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_overwrites_slot(const TZrInstruction *instruction,
                                                                       TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_resets_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    if (instruction->instruction.operandExtra != slot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
        case ZR_INSTRUCTION_OP_TO_INT:
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
            return ZR_TRUE;
        default:
            break;
    }

    if (backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_u64_instruction_overwrites_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_resets_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    if (instruction->instruction.operandExtra != slot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_UINT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot);
        default:
            break;
    }

    if (backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_u64_consumer_reads_slot(functionIr, instruction, slot)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_i64_instruction_overwrites_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_resets_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    if (instruction->instruction.operandExtra != slot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return ZR_FALSE;
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
        case ZR_INSTRUCTION_OP_TO_INT:
        case ZR_INSTRUCTION_OP_TO_INT_FLOAT:
        case ZR_INSTRUCTION_OP_TO_INT_UNSIGNED:
        case ZR_INSTRUCTION_OP_TO_UINT:
        case ZR_INSTRUCTION_OP_TO_UINT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot);
        default:
            break;
    }

    if (backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_bool_instruction_overwrites_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_resets_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    if (instruction->instruction.operandExtra != slot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return ZR_FALSE;
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot);
        default:
            break;
    }

    if (backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_u64_consumer_reads_slot(functionIr, instruction, slot)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_f64_consumer_reads_slot(functionIr, instruction, slot)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_scalar_locals_instruction_is_bool_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_bool_consumer_reads_slot(functionIr, instruction, slot)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_f64_instruction_overwrites_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_resets_slot(instruction, slot)) {
        return ZR_TRUE;
    }

    if (instruction->instruction.operandExtra != slot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
        case ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED:
        case ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED:
            return ZR_TRUE;
        case ZR_INSTRUCTION_OP_TO_FLOAT:
            return (TZrBool)(instruction->instruction.operand.operand1[0] != slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot);
        default:
            break;
    }

    if (backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(opcode) &&
        !backend_aot_c_scalar_locals_f64_consumer_reads_slot(functionIr, instruction, slot)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_result_local_consumer(
        EZrAotScalarLocalKind expectedKind,
        EZrInstructionCode opcode) {
    switch (expectedKind) {
        case ZR_AOT_SCALAR_LOCAL_KIND_BOOL:
            return backend_aot_c_scalar_locals_instruction_is_bool_local_consumer(opcode);
        case ZR_AOT_SCALAR_LOCAL_KIND_F64:
            return backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(opcode);
        case ZR_AOT_SCALAR_LOCAL_KIND_I64:
            return backend_aot_c_scalar_locals_instruction_is_i64_local_consumer(opcode);
        case ZR_AOT_SCALAR_LOCAL_KIND_U64:
            return backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_result_consumer_reads_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        EZrAotScalarLocalKind expectedKind) {
    switch (expectedKind) {
        case ZR_AOT_SCALAR_LOCAL_KIND_BOOL:
            return backend_aot_c_scalar_locals_bool_consumer_reads_slot(functionIr, instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_F64:
            return backend_aot_c_scalar_locals_f64_consumer_reads_slot(functionIr, instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_I64:
            return backend_aot_c_scalar_locals_i64_consumer_reads_slot(functionIr, instruction, instructionIndex, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_U64:
            return backend_aot_c_scalar_locals_u64_consumer_reads_slot(functionIr, instruction, slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_instruction_is_any_local_consumer(EZrInstructionCode opcode) {
    return (TZrBool)(backend_aot_c_scalar_locals_instruction_is_bool_local_consumer(opcode) ||
                     backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(opcode) ||
                     backend_aot_c_scalar_locals_instruction_is_i64_local_consumer(opcode) ||
                     backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode));
}

static TZrBool backend_aot_c_scalar_locals_any_local_consumer_mentions_slot(
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    return (TZrBool)((backend_aot_c_scalar_locals_instruction_is_bool_local_consumer(opcode) &&
                      backend_aot_c_scalar_locals_bool_consumer_mentions_slot(instruction, slot)) ||
                     (backend_aot_c_scalar_locals_instruction_is_f64_local_consumer(opcode) &&
                      backend_aot_c_scalar_locals_f64_consumer_mentions_slot(instruction, slot)) ||
                     (backend_aot_c_scalar_locals_instruction_is_i64_local_consumer(opcode) &&
                      backend_aot_c_scalar_locals_i64_consumer_mentions_slot(instruction, slot)) ||
                     (backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode) &&
                      backend_aot_c_scalar_locals_u64_consumer_mentions_slot(instruction, slot)));
}

static TZrBool backend_aot_c_scalar_locals_result_consumer_mentions_slot(
        const TZrInstruction *instruction,
        TZrUInt32 slot,
        EZrAotScalarLocalKind expectedKind) {
    switch (expectedKind) {
        case ZR_AOT_SCALAR_LOCAL_KIND_BOOL:
            return backend_aot_c_scalar_locals_bool_consumer_mentions_slot(instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_F64:
            return backend_aot_c_scalar_locals_f64_consumer_mentions_slot(instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_I64:
            return backend_aot_c_scalar_locals_i64_consumer_mentions_slot(instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_U64:
            return backend_aot_c_scalar_locals_u64_consumer_mentions_slot(instruction, slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot,
        EZrAotScalarLocalKind expectedKind) {
    switch (expectedKind) {
        case ZR_AOT_SCALAR_LOCAL_KIND_BOOL:
            return backend_aot_c_scalar_locals_bool_instruction_overwrites_slot(functionIr, instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_F64:
            return backend_aot_c_scalar_locals_f64_instruction_overwrites_slot(functionIr, instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_I64:
            return backend_aot_c_scalar_locals_i64_instruction_overwrites_slot(functionIr, instruction, slot);
        case ZR_AOT_SCALAR_LOCAL_KIND_U64:
            return backend_aot_c_scalar_locals_u64_instruction_overwrites_slot(functionIr, instruction, slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_locals_result_scan_live_value_block(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 slot,
        TZrUInt32 firstInstructionIndex,
        TZrUInt32 endInstructionIndex,
        EZrAotScalarLocalKind expectedKind,
        TZrBool *outValueStillLive) {
    TZrUInt32 instructionIndex;

    if (functionIr == ZR_NULL ||
        function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        outValueStillLive == ZR_NULL ||
        firstInstructionIndex > endInstructionIndex ||
        endInstructionIndex > function->instructionsLength) {
        return ZR_FALSE;
    }

    for (instructionIndex = firstInstructionIndex; instructionIndex < endInstructionIndex; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_OP_FUNCTION_RETURN) {
            if (instruction->instruction.operand.operand1[0] == slot) {
                if (backend_aot_c_scalar_locals_can_return_kind_local(
                            functionIr, slot, instructionIndex, expectedKind, ZR_FALSE)) {
                    *outValueStillLive = ZR_FALSE;
                    return ZR_TRUE;
                }
                return ZR_FALSE;
            }
            continue;
        }

        if (backend_aot_c_scalar_locals_instruction_is_result_local_consumer(expectedKind, opcode)) {
            TZrBool readsSlot = backend_aot_c_scalar_locals_result_consumer_reads_slot(
                    functionIr, instruction, instructionIndex, slot, expectedKind);

            if (readsSlot && backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
                                     functionIr, instruction, slot, expectedKind)) {
                *outValueStillLive = ZR_FALSE;
                return ZR_TRUE;
            }

            if (readsSlot) {
                continue;
            }

            if (backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
                        functionIr, instruction, slot, expectedKind)) {
                *outValueStillLive = ZR_FALSE;
                return ZR_TRUE;
            }

            if (!backend_aot_c_scalar_locals_result_consumer_mentions_slot(instruction, slot, expectedKind)) {
                continue;
            }

            return ZR_FALSE;
        }

        if (backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
                    functionIr, instruction, slot, expectedKind)) {
            *outValueStillLive = ZR_FALSE;
            return ZR_TRUE;
        }

        if (backend_aot_c_scalar_locals_instruction_is_any_local_consumer(opcode)) {
            if (!backend_aot_c_scalar_locals_any_local_consumer_mentions_slot(instruction, slot)) {
                continue;
            }
            return ZR_FALSE;
        }

        if (backend_aot_c_scalar_locals_instruction_mentions_slot_as_operand(instruction, slot)) {
            return ZR_FALSE;
        }
    }

    *outValueStillLive = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_locals_constant_can_skip_value_slot_kind(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex,
        EZrAotScalarLocalKind expectedKind) {
    const SZrFunction *function;
    TZrUInt32 blockStart;
    TZrUInt32 blockEnd;
    TZrUInt32 instructionIndex;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, expectedKind) ||
        !backend_aot_c_scalar_locals_find_exec_block_bounds(functionIr, execInstructionIndex, &blockStart, &blockEnd)) {
        return ZR_FALSE;
    }

    (void)blockStart;
    function = functionIr->function;
    if (backend_aot_c_scalar_locals_function_exports_slot(function, slot) ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex >= function->instructionsLength ||
        blockEnd > function->instructionsLength) {
        return ZR_FALSE;
    }

    for (instructionIndex = execInstructionIndex + 1u; instructionIndex < blockEnd; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_OP_FUNCTION_RETURN && instruction->instruction.operand.operand1[0] == slot) {
            return backend_aot_c_scalar_locals_can_return_kind_local(
                    functionIr, slot, instructionIndex, expectedKind, ZR_FALSE);
        }

        if (backend_aot_c_scalar_locals_instruction_is_result_local_consumer(expectedKind, opcode)) {
            TZrBool readsSlot = backend_aot_c_scalar_locals_result_consumer_reads_slot(
                    functionIr, instruction, instructionIndex, slot, expectedKind);

            if (readsSlot && backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
                                     functionIr, instruction, slot, expectedKind)) {
                return ZR_TRUE;
            }

            if (readsSlot) {
                continue;
            }

            if (backend_aot_c_scalar_locals_result_instruction_overwrites_slot(
                        functionIr, instruction, slot, expectedKind)) {
                return ZR_TRUE;
            }

            if (!backend_aot_c_scalar_locals_result_consumer_mentions_slot(instruction, slot, expectedKind)) {
                continue;
            }

            return ZR_FALSE;
        }

        if (backend_aot_c_scalar_locals_instruction_mentions_slot_as_operand(instruction, slot)) {
            if (backend_aot_c_scalar_locals_instruction_is_any_local_consumer(opcode) &&
                !backend_aot_c_scalar_locals_any_local_consumer_mentions_slot(instruction, slot)) {
                continue;
            }
            return ZR_FALSE;
        }

        if (backend_aot_c_scalar_locals_instruction_overwrites_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    if (blockEnd >= function->instructionsLength) {
        return ZR_TRUE;
    }

    return backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, expectedKind);
}

static TZrBool backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex,
        EZrAotScalarLocalKind expectedKind) {
    const SZrFunction *function;
    TZrBool *queuedBlocks;
    TZrBool *visitedBlocks;
    TZrUInt32 *workQueue;
    TZrUInt32 queueRead;
    TZrUInt32 queueWrite;
    TZrUInt32 blockIndex;
    TZrUInt32 blockStart;
    TZrUInt32 blockEnd;
    TZrBool valueStillLive;
    TZrBool result;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_slot_kind(functionIr, slot, expectedKind) ||
        !backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
                functionIr, execInstructionIndex, &blockIndex, &blockStart, &blockEnd)) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (backend_aot_c_scalar_locals_function_exports_slot(function, slot) ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex >= function->instructionsLength ||
        blockEnd > function->instructionsLength ||
        functionIr->basicBlocks == ZR_NULL ||
        functionIr->basicBlockCount == 0u ||
        blockIndex >= functionIr->basicBlockCount) {
        return ZR_FALSE;
    }

    queuedBlocks = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    visitedBlocks = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    workQueue = (TZrUInt32 *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrUInt32));
    if (queuedBlocks == ZR_NULL || visitedBlocks == ZR_NULL || workQueue == ZR_NULL) {
        free(queuedBlocks);
        free(visitedBlocks);
        free(workQueue);
        return ZR_FALSE;
    }

    result = backend_aot_c_scalar_locals_result_scan_live_value_block(functionIr,
                                                                     function,
                                                                     slot,
                                                                     execInstructionIndex + 1u,
                                                                     blockEnd,
                                                                     expectedKind,
                                                                     &valueStillLive);
    if (!result || !valueStillLive) {
        free(queuedBlocks);
        free(visitedBlocks);
        free(workQueue);
        return result;
    }

    queueRead = 0u;
    queueWrite = 0u;
    {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 successorIndex;
        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedBlocks[successorBlockIndex]) {
                queuedBlocks[successorBlockIndex] = ZR_TRUE;
                workQueue[queueWrite++] = successorBlockIndex;
            }
        }
    }

    while (queueRead < queueWrite) {
        const SZrAotExecIrBasicBlock *block;
        TZrUInt32 successorIndex;
        TZrUInt32 activeBlockIndex = workQueue[queueRead++];

        if (activeBlockIndex >= functionIr->basicBlockCount || visitedBlocks[activeBlockIndex]) {
            continue;
        }

        visitedBlocks[activeBlockIndex] = ZR_TRUE;
        block = &functionIr->basicBlocks[activeBlockIndex];
        result = backend_aot_c_scalar_locals_result_scan_live_value_block(
                functionIr,
                function,
                slot,
                block->firstExecInstructionIndex,
                block->firstExecInstructionIndex + block->instructionCount,
                expectedKind,
                &valueStillLive);
        if (!result) {
            break;
        }

        if (!valueStillLive) {
            continue;
        }

        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedBlocks[successorBlockIndex]) {
                queuedBlocks[successorBlockIndex] = ZR_TRUE;
                workQueue[queueWrite++] = successorBlockIndex;
            }
        }
    }

    free(queuedBlocks);
    free(visitedBlocks);
    free(workQueue);
    return result;
}

TZrBool backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_constant_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_I64);
}

TZrBool backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_constant_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_U64);
}

TZrBool backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_constant_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_F64);
}

TZrBool backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_U64);
}

TZrBool backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_I64);
}

TZrBool backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_F64);
}

TZrBool backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 execInstructionIndex) {
    return backend_aot_c_scalar_locals_result_can_skip_value_slot_kind(
            functionIr, slot, execInstructionIndex, ZR_AOT_SCALAR_LOCAL_KIND_BOOL);
}

static TZrBool backend_aot_c_scalar_locals_instruction_mentions_slot_as_source_operand(
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_OP_GET_CONSTANT:
        case ZR_INSTRUCTION_OP_RESET_STACK_NULL:
        case ZR_INSTRUCTION_OP_RESET_STACK_NULL2:
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
        case ZR_INSTRUCTION_ENUM(JUMP):
            return ZR_FALSE;
        case ZR_INSTRUCTION_OP_FUNCTION_RETURN:
            return (TZrBool)(instruction->instruction.operand.operand1[0] == slot);
        case ZR_INSTRUCTION_OP_GET_STACK:
        case ZR_INSTRUCTION_OP_SET_STACK:
            return (TZrBool)(instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
        default:
            break;
    }

    return (TZrBool)(instruction->instruction.operand.operand1[0] == slot ||
                     instruction->instruction.operand.operand1[1] == slot ||
                     instruction->instruction.operand.operand0[0] == slot ||
                     instruction->instruction.operand.operand0[1] == slot ||
                     instruction->instruction.operand.operand0[2] == slot ||
                     instruction->instruction.operand.operand2[0] == (TZrInt32)slot);
}

static TZrBool backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (!backend_aot_c_scalar_locals_instruction_is_any_local_consumer(opcode)) {
        return ZR_FALSE;
    }

    return (TZrBool)(backend_aot_c_scalar_locals_i64_consumer_reads_slot(
                             functionIr, instruction, instructionIndex, slot) ||
                     backend_aot_c_scalar_locals_u64_consumer_reads_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_f64_consumer_reads_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_bool_consumer_reads_slot(functionIr, instruction, slot));
}

static TZrBool backend_aot_c_scalar_locals_instruction_overwrites_slot_as_any_scalar(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 slot) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(backend_aot_c_scalar_locals_i64_instruction_overwrites_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_u64_instruction_overwrites_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_f64_instruction_overwrites_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_bool_instruction_overwrites_slot(functionIr, instruction, slot) ||
                     backend_aot_c_scalar_locals_instruction_overwrites_slot(instruction, slot));
}

static TZrBool backend_aot_c_scalar_locals_reset_scan_dead_slot_block(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 slot,
        TZrUInt32 firstInstructionIndex,
        TZrUInt32 endInstructionIndex,
        TZrBool *outResetStillLive) {
    TZrUInt32 instructionIndex;

    if (functionIr == ZR_NULL ||
        function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        outResetStillLive == ZR_NULL ||
        firstInstructionIndex > endInstructionIndex ||
        endInstructionIndex > function->instructionsLength) {
        return ZR_FALSE;
    }

    for (instructionIndex = firstInstructionIndex; instructionIndex < endInstructionIndex; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];

        if (backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local(
                    functionIr, instruction, instructionIndex, slot) ||
            backend_aot_c_scalar_locals_instruction_mentions_slot_as_source_operand(instruction, slot)) {
            return ZR_FALSE;
        }

        if (backend_aot_c_scalar_locals_instruction_overwrites_slot_as_any_scalar(
                    functionIr, instruction, slot)) {
            *outResetStillLive = ZR_FALSE;
            return ZR_TRUE;
        }

        if (backend_aot_c_scalar_locals_any_local_consumer_mentions_slot(instruction, slot) ||
            backend_aot_c_scalar_locals_instruction_mentions_slot_as_operand(instruction, slot)) {
            return ZR_FALSE;
        }
    }

    *outResetStillLive = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_locals_has_any_scalar_slot(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot) {
    return (TZrBool)(backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) ||
                     backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) ||
                     backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) ||
                     backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot));
}

static TZrBool backend_aot_c_scalar_locals_reset_pair_scan_live_slot(
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *instruction,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 liveBit,
        TZrUInt32 *inOutLiveMask) {
    if (inOutLiveMask == ZR_NULL || ((*inOutLiveMask & liveBit) == 0u)) {
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local(
                functionIr, instruction, instructionIndex, slot) ||
        backend_aot_c_scalar_locals_instruction_mentions_slot_as_source_operand(instruction, slot)) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_instruction_overwrites_slot_as_any_scalar(
                functionIr, instruction, slot)) {
        *inOutLiveMask &= ~liveBit;
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_any_local_consumer_mentions_slot(instruction, slot) ||
        backend_aot_c_scalar_locals_instruction_mentions_slot_as_operand(instruction, slot)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_locals_reset_scan_dead_slot_pair_block(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 firstSlot,
        TZrUInt32 secondSlot,
        TZrUInt32 firstInstructionIndex,
        TZrUInt32 endInstructionIndex,
        TZrUInt32 *inOutLiveMask) {
    TZrUInt32 instructionIndex;

    if (functionIr == ZR_NULL ||
        function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        inOutLiveMask == ZR_NULL ||
        firstInstructionIndex > endInstructionIndex ||
        endInstructionIndex > function->instructionsLength) {
        return ZR_FALSE;
    }

    for (instructionIndex = firstInstructionIndex; instructionIndex < endInstructionIndex; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];

        if (!backend_aot_c_scalar_locals_reset_pair_scan_live_slot(
                    functionIr, instruction, instructionIndex, firstSlot, 1u, inOutLiveMask) ||
            !backend_aot_c_scalar_locals_reset_pair_scan_live_slot(
                    functionIr, instruction, instructionIndex, secondSlot, 2u, inOutLiveMask)) {
            return ZR_FALSE;
        }

        if (*inOutLiveMask == 0u) {
            return ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_scalar_locals_reset2_can_skip_value_slots(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 firstSlot,
                                                                TZrUInt32 secondSlot,
                                                                TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    TZrBool *queuedStates;
    TZrBool *visitedStates;
    TZrUInt32 *blockQueue;
    TZrUInt32 *liveMaskQueue;
    size_t stateCount;
    TZrUInt32 queueRead;
    TZrUInt32 queueWrite;
    TZrUInt32 blockIndex;
    TZrUInt32 blockStart;
    TZrUInt32 blockEnd;
    TZrUInt32 liveMask;
    TZrBool result;

    if (firstSlot == secondSlot) {
        return backend_aot_c_scalar_locals_reset_can_skip_value_slot(
                functionIr, firstSlot, execInstructionIndex);
    }

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_scalar_locals_function_exports_slot(function, firstSlot) ||
        backend_aot_c_scalar_locals_function_exports_slot(function, secondSlot) ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex >= function->instructionsLength ||
        functionIr->basicBlocks == ZR_NULL ||
        functionIr->basicBlockCount == 0u ||
        !backend_aot_c_scalar_locals_has_any_scalar_slot(functionIr, firstSlot) ||
        !backend_aot_c_scalar_locals_has_any_scalar_slot(functionIr, secondSlot) ||
        !backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
                functionIr, execInstructionIndex, &blockIndex, &blockStart, &blockEnd) ||
        blockIndex >= functionIr->basicBlockCount ||
        blockEnd > function->instructionsLength) {
        return ZR_FALSE;
    }

    (void)blockStart;

    liveMask = 3u;
    result = backend_aot_c_scalar_locals_reset_scan_dead_slot_pair_block(
            functionIr,
            function,
            firstSlot,
            secondSlot,
            execInstructionIndex + 1u,
            blockEnd,
            &liveMask);
    if (!result || liveMask == 0u) {
        return result;
    }

    stateCount = (size_t)functionIr->basicBlockCount * 4u;
    queuedStates = (TZrBool *)calloc(stateCount, sizeof(TZrBool));
    visitedStates = (TZrBool *)calloc(stateCount, sizeof(TZrBool));
    blockQueue = (TZrUInt32 *)calloc(stateCount, sizeof(TZrUInt32));
    liveMaskQueue = (TZrUInt32 *)calloc(stateCount, sizeof(TZrUInt32));
    if (queuedStates == ZR_NULL ||
        visitedStates == ZR_NULL ||
        blockQueue == ZR_NULL ||
        liveMaskQueue == ZR_NULL) {
        free(queuedStates);
        free(visitedStates);
        free(blockQueue);
        free(liveMaskQueue);
        return ZR_FALSE;
    }

    queueRead = 0u;
    queueWrite = 0u;
    {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 successorIndex;
        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            size_t stateIndex = (size_t)successorBlockIndex * 4u + liveMask;
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedStates[stateIndex]) {
                queuedStates[stateIndex] = ZR_TRUE;
                blockQueue[queueWrite] = successorBlockIndex;
                liveMaskQueue[queueWrite] = liveMask;
                queueWrite++;
            }
        }
    }

    while (queueRead < queueWrite) {
        const SZrAotExecIrBasicBlock *block;
        TZrUInt32 successorIndex;
        TZrUInt32 activeBlockIndex = blockQueue[queueRead];
        TZrUInt32 activeLiveMask = liveMaskQueue[queueRead];
        size_t activeStateIndex = (size_t)activeBlockIndex * 4u + activeLiveMask;
        queueRead++;

        if (activeBlockIndex >= functionIr->basicBlockCount ||
            activeLiveMask == 0u ||
            visitedStates[activeStateIndex]) {
            continue;
        }

        visitedStates[activeStateIndex] = ZR_TRUE;
        block = &functionIr->basicBlocks[activeBlockIndex];
        if (block->firstExecInstructionIndex + block->instructionCount > function->instructionsLength) {
            result = ZR_FALSE;
            break;
        }

        result = backend_aot_c_scalar_locals_reset_scan_dead_slot_pair_block(
                functionIr,
                function,
                firstSlot,
                secondSlot,
                block->firstExecInstructionIndex,
                block->firstExecInstructionIndex + block->instructionCount,
                &activeLiveMask);
        if (!result) {
            break;
        }

        if (activeLiveMask == 0u) {
            continue;
        }

        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            size_t stateIndex = (size_t)successorBlockIndex * 4u + activeLiveMask;
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedStates[stateIndex]) {
                queuedStates[stateIndex] = ZR_TRUE;
                blockQueue[queueWrite] = successorBlockIndex;
                liveMaskQueue[queueWrite] = activeLiveMask;
                queueWrite++;
            }
        }
    }

    free(queuedStates);
    free(visitedStates);
    free(blockQueue);
    free(liveMaskQueue);
    return result;
}

TZrBool backend_aot_c_scalar_locals_reset_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                              TZrUInt32 slot,
                                                              TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    TZrBool *queuedBlocks;
    TZrBool *visitedBlocks;
    TZrUInt32 *workQueue;
    TZrUInt32 queueRead;
    TZrUInt32 queueWrite;
    TZrUInt32 blockIndex;
    TZrUInt32 blockStart;
    TZrUInt32 blockEnd;
    TZrBool resetStillLive;
    TZrBool result;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_scalar_locals_function_exports_slot(function, slot) ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex >= function->instructionsLength ||
        functionIr->basicBlocks == ZR_NULL ||
        functionIr->basicBlockCount == 0u ||
        !(backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) ||
          backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) ||
          backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) ||
          backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot)) ||
        !backend_aot_c_scalar_locals_find_exec_block_index_and_bounds(
                functionIr, execInstructionIndex, &blockIndex, &blockStart, &blockEnd) ||
        blockIndex >= functionIr->basicBlockCount ||
        blockEnd > function->instructionsLength) {
        return ZR_FALSE;
    }

    (void)blockStart;

    queuedBlocks = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    visitedBlocks = (TZrBool *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrBool));
    workQueue = (TZrUInt32 *)calloc((size_t)functionIr->basicBlockCount, sizeof(TZrUInt32));
    if (queuedBlocks == ZR_NULL || visitedBlocks == ZR_NULL || workQueue == ZR_NULL) {
        free(queuedBlocks);
        free(visitedBlocks);
        free(workQueue);
        return ZR_FALSE;
    }

    result = backend_aot_c_scalar_locals_reset_scan_dead_slot_block(functionIr,
                                                                   function,
                                                                   slot,
                                                                   execInstructionIndex + 1u,
                                                                   blockEnd,
                                                                   &resetStillLive);
    if (!result || !resetStillLive) {
        free(queuedBlocks);
        free(visitedBlocks);
        free(workQueue);
        return result;
    }

    queueRead = 0u;
    queueWrite = 0u;
    {
        const SZrAotExecIrBasicBlock *block = &functionIr->basicBlocks[blockIndex];
        TZrUInt32 successorIndex;
        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedBlocks[successorBlockIndex]) {
                queuedBlocks[successorBlockIndex] = ZR_TRUE;
                workQueue[queueWrite++] = successorBlockIndex;
            }
        }
    }

    while (queueRead < queueWrite) {
        const SZrAotExecIrBasicBlock *block;
        TZrUInt32 successorIndex;
        TZrUInt32 activeBlockIndex = workQueue[queueRead++];

        if (activeBlockIndex >= functionIr->basicBlockCount || visitedBlocks[activeBlockIndex]) {
            continue;
        }

        visitedBlocks[activeBlockIndex] = ZR_TRUE;
        block = &functionIr->basicBlocks[activeBlockIndex];
        result = backend_aot_c_scalar_locals_reset_scan_dead_slot_block(
                functionIr,
                function,
                slot,
                block->firstExecInstructionIndex,
                block->firstExecInstructionIndex + block->instructionCount,
                &resetStillLive);
        if (!result) {
            break;
        }

        if (!resetStillLive) {
            continue;
        }

        for (successorIndex = 0u; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorBlockIndex = block->successorBlockIndices[successorIndex];
            if (successorBlockIndex < functionIr->basicBlockCount && !queuedBlocks[successorBlockIndex]) {
                queuedBlocks[successorBlockIndex] = ZR_TRUE;
                workQueue[queueWrite++] = successorBlockIndex;
            }
        }
    }

    free(queuedBlocks);
    free(visitedBlocks);
    free(workQueue);
    return result;
}
