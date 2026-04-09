//
// ExecBC quickening.
// Name-based access rewrites are intentionally disabled: access semantics must
// remain explicit in emitted instructions and artifacts.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "compiler_internal.h"

#define ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE ((TZrUInt8)0)

typedef struct ZrCompilerQuickeningSlotAlias {
    TZrUInt32 rootSlot;
    TZrBool valid;
} ZrCompilerQuickeningSlotAlias;

typedef enum EZrCompilerQuickeningSlotKind {
    ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN = 0,
    ZR_COMPILER_QUICKENING_SLOT_KIND_INT = 1,
    ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT = 2
} EZrCompilerQuickeningSlotKind;

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot);
static TZrBool compiler_quickening_function_constant_is_int(const SZrFunction *function,
                                                            TZrUInt32 constantIndex);
static const SZrFunctionLocalVariable *compiler_quickening_find_active_local_variable(const SZrFunction *function,
                                                                                       TZrUInt32 stackSlot,
                                                                                       TZrUInt32 instructionIndex);
static const TZrInstruction *compiler_quickening_find_latest_writer_in_range(const SZrFunction *function,
                                                                             TZrUInt32 rangeStart,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot,
                                                                             TZrUInt32 *outWriterIndex);
static TZrBool compiler_quickening_slot_is_int_before_instruction_in_range(const SZrFunction *function,
                                                                           TZrUInt32 rangeStart,
                                                                           TZrUInt32 instructionIndex,
                                                                           TZrUInt32 slot,
                                                                           TZrUInt32 depth);
static TZrBool compiler_quickening_slot_has_only_int_writers_in_range(const SZrFunction *function,
                                                                      TZrUInt32 rangeStart,
                                                                      TZrUInt32 rangeEnd,
                                                                      TZrUInt32 slot,
                                                                      TZrUInt32 depth);
static TZrInstruction *compiler_quickening_find_latest_block_writer(SZrFunction *function,
                                                                    const TZrBool *blockStarts,
                                                                    TZrUInt32 instructionIndex,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 *outWriterIndex);
static TZrBool compiler_quickening_slot_is_overwritten_before_read(const SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex,
                                                                   TZrUInt32 slot);
static TZrBool compiler_quickening_slot_is_overwritten_before_any_read_linear(const SZrFunction *function,
                                                                              TZrUInt32 instructionIndex,
                                                                              TZrUInt32 slot);
static TZrBool compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot);
static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(SZrFunction *function,
                                                                                      const TZrBool *blockStarts,
                                                                                      TZrUInt32 instructionIndex);

static const TZrChar *compiler_quickening_type_name_text(SZrString *typeName) {
    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(typeName);
    }

    return ZrCore_String_GetNativeString(typeName);
}

static const SZrFunctionTypedLocalBinding *compiler_quickening_find_typed_local_binding(const SZrFunction *function,
                                                                                         TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_local_variable_is_active_at_instruction(
        const SZrFunctionLocalVariable *localVariable,
        TZrUInt32 instructionIndex) {
    if (localVariable == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrUInt32)localVariable->offsetActivate <= instructionIndex &&
           instructionIndex < (TZrUInt32)localVariable->offsetDead;
}

static const SZrFunctionLocalVariable *compiler_quickening_find_active_local_variable(const SZrFunction *function,
                                                                                       TZrUInt32 stackSlot,
                                                                                       TZrUInt32 instructionIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *localVariable = &function->localVariableList[index];
        if (localVariable->stackSlot == stackSlot &&
            compiler_quickening_local_variable_is_active_at_instruction(localVariable, instructionIndex)) {
            return localVariable;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *compiler_quickening_find_active_typed_local_binding(
        const SZrFunction *function,
        TZrUInt32 stackSlot,
        TZrUInt32 instructionIndex) {
    const SZrFunctionLocalVariable *activeLocal = ZR_NULL;
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        return compiler_quickening_find_typed_local_binding(function, stackSlot);
    }

    for (index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *localVariable = &function->localVariableList[index];
        if (localVariable->stackSlot != stackSlot ||
            !compiler_quickening_local_variable_is_active_at_instruction(localVariable, instructionIndex)) {
            continue;
        }

        activeLocal = localVariable;
        if (index < function->typedLocalBindingLength) {
            const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
            if (binding->stackSlot == stackSlot &&
                ((binding->name == localVariable->name) ||
                 (binding->name != ZR_NULL && localVariable->name != ZR_NULL &&
                  ZrCore_String_Equal(binding->name, localVariable->name)))) {
                return binding;
            }
        }
        break;
    }

    if (activeLocal == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot &&
            ((binding->name == activeLocal->name) ||
             (binding->name != ZR_NULL && activeLocal->name != ZR_NULL &&
              ZrCore_String_Equal(binding->name, activeLocal->name)))) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_binding_is_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "int") == 0;
}

static TZrBool compiler_quickening_opcode_produces_known_int(const SZrFunction *function,
                                                             const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_is_int(function,
                                                                (TZrUInt32)instruction->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_slot_has_only_int_writers_in_range(const SZrFunction *function,
                                                                      TZrUInt32 rangeStart,
                                                                      TZrUInt32 rangeEnd,
                                                                      TZrUInt32 slot,
                                                                      TZrUInt32 depth) {
    TZrBool foundWriter = ZR_FALSE;
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || depth > function->stackSize + 4u) {
        return ZR_FALSE;
    }

    if (rangeStart >= function->instructionsLength) {
        return ZR_FALSE;
    }
    if (rangeEnd > function->instructionsLength) {
        rangeEnd = function->instructionsLength;
    }
    if (rangeStart >= rangeEnd) {
        return ZR_FALSE;
    }

    for (index = rangeStart; index < rangeEnd; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 sourceSlot;

        if (instruction->instruction.operandExtra != slot || opcode == ZR_INSTRUCTION_ENUM(NOP)) {
            continue;
        }

        foundWriter = ZR_TRUE;
        if (compiler_quickening_opcode_produces_known_int(function, instruction)) {
            continue;
        }

        if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK) && opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            return ZR_FALSE;
        }

        sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
        if (sourceSlot == slot) {
            return ZR_FALSE;
        }

        if (!compiler_quickening_slot_is_int_before_instruction_in_range(function,
                                                                         rangeStart,
                                                                         index,
                                                                         sourceSlot,
                                                                         depth + 1u)) {
            return ZR_FALSE;
        }
    }

    return foundWriter;
}

static const TZrInstruction *compiler_quickening_find_latest_writer_in_range(const SZrFunction *function,
                                                                             TZrUInt32 rangeStart,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot,
                                                                             TZrUInt32 *outWriterIndex) {
    if (outWriterIndex != ZR_NULL) {
        *outWriterIndex = UINT32_MAX;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || instructionIndex == 0 ||
        rangeStart >= function->instructionsLength) {
        return ZR_NULL;
    }

    if (instructionIndex > function->instructionsLength) {
        instructionIndex = function->instructionsLength;
    }

    for (TZrUInt32 scan = instructionIndex; scan > rangeStart; scan--) {
        TZrUInt32 candidate = scan - 1u;
        const TZrInstruction *writer = &function->instructionsList[candidate];
        if (writer->instruction.operandExtra != slot ||
            (EZrInstructionCode)writer->instruction.operationCode == ZR_INSTRUCTION_ENUM(NOP)) {
            continue;
        }

        if (outWriterIndex != ZR_NULL) {
            *outWriterIndex = candidate;
        }
        return writer;
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_slot_is_int_before_instruction_in_range(const SZrFunction *function,
                                                                           TZrUInt32 rangeStart,
                                                                           TZrUInt32 instructionIndex,
                                                                           TZrUInt32 slot,
                                                                           TZrUInt32 depth) {
    const SZrFunctionTypedLocalBinding *binding;
    const TZrInstruction *writer;
    TZrUInt32 writerIndex = UINT32_MAX;
    EZrInstructionCode opcode;
    TZrUInt32 sourceSlot;

    if (function == ZR_NULL || depth > function->stackSize + 4u) {
        return ZR_FALSE;
    }

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (compiler_quickening_binding_is_int(binding)) {
        return ZR_TRUE;
    }

    writer = compiler_quickening_find_latest_writer_in_range(function,
                                                             rangeStart,
                                                             instructionIndex,
                                                             slot,
                                                             &writerIndex);
    if (writer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compiler_quickening_opcode_produces_known_int(function, writer)) {
        return ZR_TRUE;
    }

    opcode = (EZrInstructionCode)writer->instruction.operationCode;
    if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK) && opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
    if (sourceSlot == slot) {
        return ZR_FALSE;
    }

    return compiler_quickening_slot_is_int_before_instruction_in_range(function,
                                                                       rangeStart,
                                                                       writerIndex,
                                                                       sourceSlot,
                                                                       depth + 1u);
}

static TZrBool compiler_quickening_binding_is_array_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;
    const TZrChar *elementTypeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    elementTypeName = compiler_quickening_type_name_text(binding->type.elementTypeName);
    if ((strcmp(typeName, "Array") == 0 || strcmp(typeName, "container.Array") == 0 ||
         strcmp(typeName, "zr.container.Array") == 0) &&
        (ZR_VALUE_IS_TYPE_INT(binding->type.elementBaseType) ||
         (elementTypeName != ZR_NULL && strcmp(elementTypeName, "int") == 0))) {
        return ZR_TRUE;
    }

    return strcmp(typeName, "Array<int>") == 0 ||
           strcmp(typeName, "container.Array<int>") == 0 ||
           strcmp(typeName, "zr.container.Array<int>") == 0;
}

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_binding(
        const SZrFunctionTypedLocalBinding *binding) {
    if (compiler_quickening_binding_is_array_int(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
    }
    if (compiler_quickening_binding_is_int(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_INT;
    }
    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static void compiler_quickening_clear_aliases(ZrCompilerQuickeningSlotAlias *aliases, TZrUInt32 aliasCount) {
    if (aliases != ZR_NULL && aliasCount > 0) {
        memset(aliases, 0, sizeof(*aliases) * aliasCount);
    }
}

static void compiler_quickening_clear_slot_kinds(EZrCompilerQuickeningSlotKind *slotKinds, TZrUInt32 slotCount) {
    if (slotKinds != ZR_NULL && slotCount > 0) {
        memset(slotKinds, 0, sizeof(*slotKinds) * slotCount);
    }
}

static void compiler_quickening_clear_alias(ZrCompilerQuickeningSlotAlias *aliases,
                                            TZrUInt32 aliasCount,
                                            TZrUInt32 slot) {
    if (aliases != ZR_NULL && slot < aliasCount) {
        aliases[slot].valid = ZR_FALSE;
        aliases[slot].rootSlot = 0;
    }
}

static TZrBool compiler_quickening_opcode_uses_call_argument_slots(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 compiler_quickening_call_argument_count(const SZrFunction *function,
                                                         const TZrInstruction *instruction) {
    EZrInstructionCode opcode;
    TZrUInt32 cacheIndex;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return 0;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            return (TZrUInt32)instruction->instruction.operand.operand1[1];
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            cacheIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
            if (function->callSiteCaches != ZR_NULL && cacheIndex < function->callSiteCacheLength) {
                return function->callSiteCaches[cacheIndex].argumentCount;
            }
            return 0;
        default:
            return 0;
    }
}

static void compiler_quickening_clear_slot_tracking_range(ZrCompilerQuickeningSlotAlias *aliases,
                                                          EZrCompilerQuickeningSlotKind *slotKinds,
                                                          TZrBool *constantSlotsValid,
                                                          TZrUInt32 *constantSlotIndices,
                                                          TZrUInt32 aliasCount,
                                                          TZrUInt32 firstSlot,
                                                          TZrUInt32 slotCount) {
    TZrUInt32 slotLimit;
    TZrUInt32 slot;

    if (slotCount == 0 || firstSlot >= aliasCount) {
        return;
    }

    slotLimit = firstSlot + slotCount;
    if (slotLimit < firstSlot || slotLimit > aliasCount) {
        slotLimit = aliasCount;
    }

    for (slot = firstSlot; slot < slotLimit; slot++) {
        compiler_quickening_clear_alias(aliases, aliasCount, slot);
        if (slotKinds != ZR_NULL) {
            slotKinds[slot] = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
        }
        if (constantSlotsValid != ZR_NULL) {
            constantSlotsValid[slot] = ZR_FALSE;
        }
        if (constantSlotIndices != ZR_NULL) {
            constantSlotIndices[slot] = 0;
        }
    }
}

static void compiler_quickening_clear_call_argument_tracking(const SZrFunction *function,
                                                             const TZrInstruction *instruction,
                                                             ZrCompilerQuickeningSlotAlias *aliases,
                                                             EZrCompilerQuickeningSlotKind *slotKinds,
                                                             TZrBool *constantSlotsValid,
                                                             TZrUInt32 *constantSlotIndices,
                                                             TZrUInt32 aliasCount) {
    TZrUInt32 argumentCount;
    TZrUInt32 destinationSlot;

    if (function == ZR_NULL || instruction == ZR_NULL || aliasCount == 0) {
        return;
    }

    if (!compiler_quickening_opcode_uses_call_argument_slots(
                (EZrInstructionCode)instruction->instruction.operationCode)) {
        return;
    }

    argumentCount = compiler_quickening_call_argument_count(function, instruction);
    if (argumentCount == 0) {
        return;
    }

    destinationSlot = instruction->instruction.operandExtra;
    if (destinationSlot >= aliasCount - 1) {
        return;
    }

    compiler_quickening_clear_slot_tracking_range(aliases,
                                                  slotKinds,
                                                  constantSlotsValid,
                                                  constantSlotIndices,
                                                  aliasCount,
                                                  destinationSlot + 1u,
                                                  argumentCount);
}

static TZrBool compiler_quickening_resolve_alias_slot(const SZrFunction *function,
                                                      const ZrCompilerQuickeningSlotAlias *aliases,
                                                      TZrUInt32 aliasCount,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 slot,
                                                      TZrUInt32 *outRootSlot) {
    if (outRootSlot != ZR_NULL) {
        *outRootSlot = 0;
    }

    if (function == ZR_NULL || outRootSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex) != ZR_NULL) {
        *outRootSlot = slot;
        return ZR_TRUE;
    }

    if (aliases != ZR_NULL && slot < aliasCount && aliases[slot].valid) {
        *outRootSlot = aliases[slot].rootSlot;
        return ZR_TRUE;
    }

    if ((function->localVariableList == ZR_NULL || function->localVariableLength == 0) &&
        compiler_quickening_find_typed_local_binding(function, slot) != ZR_NULL) {
        *outRootSlot = slot;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_function_constant_is_int(const SZrFunction *function,
                                                            TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    return constantValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(constantValue->type);
}

static TZrBool compiler_quickening_function_constant_read_int64(const SZrFunction *function,
                                                                TZrUInt32 constantIndex,
                                                                TZrInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || outValue == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot);

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_for_slot(const SZrFunction *function,
                                                                             const ZrCompilerQuickeningSlotAlias *aliases,
                                                                             TZrUInt32 aliasCount,
                                                                             const EZrCompilerQuickeningSlotKind *slotKinds,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot) {
    TZrUInt32 rootSlot = 0;
    const SZrFunctionTypedLocalBinding *binding;

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (binding != ZR_NULL) {
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    if (slotKinds != ZR_NULL && slot < aliasCount &&
        slotKinds[slot] != ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN) {
        return slotKinds[slot];
    }

    if (compiler_quickening_resolve_alias_slot(function, aliases, aliasCount, instructionIndex, slot, &rootSlot)) {
        binding = compiler_quickening_find_active_typed_local_binding(function, rootSlot, instructionIndex);
        if (binding == ZR_NULL &&
            (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
            binding = compiler_quickening_find_typed_local_binding(function, rootSlot);
        }
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        binding = compiler_quickening_find_typed_local_binding(function, slot);
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static TZrBool compiler_quickening_slot_is_int(const SZrFunction *function,
                                               const ZrCompilerQuickeningSlotAlias *aliases,
                                               TZrUInt32 aliasCount,
                                               const EZrCompilerQuickeningSlotKind *slotKinds,
                                               const TZrBool *blockStarts,
                                               TZrUInt32 instructionIndex,
                                               TZrUInt32 slot) {
    const SZrFunctionLocalVariable *activeLocalVariable;

    activeLocalVariable = compiler_quickening_find_active_local_variable(function, slot, instructionIndex);
    return compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, instructionIndex, slot) ==
                   ZR_COMPILER_QUICKENING_SLOT_KIND_INT ||
           compiler_quickening_resolve_index_access_int_constant(function, blockStarts, instructionIndex, slot) ||
           (activeLocalVariable != ZR_NULL &&
            compiler_quickening_slot_has_only_int_writers_in_range(function,
                                                                   (TZrUInt32)activeLocalVariable->offsetActivate,
                                                                   (TZrUInt32)activeLocalVariable->offsetDead,
                                                                   slot,
                                                                   0)) ||
           compiler_quickening_slot_has_only_int_writers_in_range(function,
                                                                  0,
                                                                  function->instructionsLength,
                                                                  slot,
                                                                  0);
}

static TZrBool compiler_quickening_slot_is_array_int(const SZrFunction *function,
                                                      const ZrCompilerQuickeningSlotAlias *aliases,
                                                      TZrUInt32 aliasCount,
                                                      const EZrCompilerQuickeningSlotKind *slotKinds,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 slot) {
    return compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, instructionIndex, slot) ==
           ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
}

static const TZrChar *compiler_quickening_member_entry_symbol_text(const SZrFunction *function, TZrUInt16 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return compiler_quickening_type_name_text(function->memberEntries[memberEntryIndex].symbol);
}

static EZrInstructionCode compiler_quickening_specialized_int_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD):
            return ZR_INSTRUCTION_ENUM(ADD_INT);
        case ZR_INSTRUCTION_ENUM(SUB):
            return ZR_INSTRUCTION_ENUM(SUB_INT);
        case ZR_INSTRUCTION_ENUM(MUL):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED);
        case ZR_INSTRUCTION_ENUM(DIV):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED);
        case ZR_INSTRUCTION_ENUM(MOD):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED);
        default:
            return opcode;
    }
}

static EZrInstructionCode compiler_quickening_specialized_int_const_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
            return ZR_INSTRUCTION_ENUM(ADD_INT_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_INT):
            return ZR_INSTRUCTION_ENUM(SUB_INT_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot) {
    TZrUInt32 currentSlot;
    TZrUInt32 blockStartIndex = 0;
    TZrUInt32 hop;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    for (TZrUInt32 scan = instructionIndex + 1; scan > 0; scan--) {
        TZrUInt32 candidate = scan - 1;
        if (blockStarts[candidate]) {
            blockStartIndex = candidate;
            break;
        }
    }

    currentSlot = slot;
    for (hop = 0; hop < function->stackSize; hop++) {
        TZrBool foundWriter = ZR_FALSE;

        for (TZrUInt32 scan = instructionIndex; scan > blockStartIndex; scan--) {
            const TZrInstruction *writer = &function->instructionsList[scan - 1];
            EZrInstructionCode writerOpcode = (EZrInstructionCode)writer->instruction.operationCode;

            if (writer->instruction.operandExtra != currentSlot) {
                continue;
            }

            foundWriter = ZR_TRUE;
            if (writerOpcode == ZR_INSTRUCTION_ENUM(GET_STACK) || writerOpcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
                TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
                if (sourceSlot == currentSlot) {
                    return ZR_FALSE;
                }
                currentSlot = sourceSlot;
                break;
            }

            if (writerOpcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
                return compiler_quickening_function_constant_is_int(function,
                                                                    (TZrUInt32)writer->instruction.operand.operand2[0]);
            }

            return ZR_FALSE;
        }

        if (!foundWriter) {
            return ZR_FALSE;
        }
    }

    return ZR_FALSE;
}

static TZrInstruction *compiler_quickening_find_latest_block_writer(SZrFunction *function,
                                                                    const TZrBool *blockStarts,
                                                                    TZrUInt32 instructionIndex,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 *outWriterIndex) {
    TZrUInt32 blockStartIndex = 0;

    if (outWriterIndex != ZR_NULL) {
        *outWriterIndex = UINT32_MAX;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex > function->instructionsLength) {
        return ZR_NULL;
    }

    for (TZrUInt32 scan = instructionIndex; scan > 0; scan--) {
        TZrUInt32 candidate = scan - 1;
        if (blockStarts[candidate]) {
            blockStartIndex = candidate;
            break;
        }
    }

    for (TZrInt64 scan = (TZrInt64)instructionIndex - 1; scan >= (TZrInt64)blockStartIndex; scan--) {
        TZrInstruction *writer = &function->instructionsList[scan];
        if (writer->instruction.operandExtra != slot) {
            continue;
        }
        if (outWriterIndex != ZR_NULL) {
            *outWriterIndex = (TZrUInt32)scan;
        }
        return writer;
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_is_control_only_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(TRY):
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(CATCH):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void compiler_quickening_mark_jump_target(TZrBool *blockStarts,
                                                 TZrUInt32 instructionLength,
                                                 TZrInt32 targetIndex) {
    if (blockStarts == ZR_NULL || targetIndex < 0) {
        return;
    }

    if ((TZrUInt32)targetIndex < instructionLength) {
        blockStarts[targetIndex] = ZR_TRUE;
    }
}

static TZrBool compiler_quickening_build_block_starts(const SZrFunction *function,
                                                      TZrBool *blockStarts) {
    TZrUInt32 index;

    if (function == ZR_NULL || blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(blockStarts, 0, sizeof(*blockStarts) * function->instructionsLength);
    if (function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    blockStarts[0] = ZR_TRUE;
    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(JUMP) || opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            TZrInt32 targetIndex = (TZrInt32)index + instruction->instruction.operand.operand2[0] + 1;
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            TZrInt32 targetIndex = (TZrInt32)index + (TZrInt16)instruction->instruction.operand.operand1[1] + 1;
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (compiler_quickening_is_control_only_opcode(opcode) && index + 1 < function->instructionsLength) {
            blockStarts[index + 1] = ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

static void compiler_quickening_write_nop(TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return;
    }

    instruction->value = 0;
    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
}

static TZrUInt32 compiler_quickening_remap_instruction_index(const TZrUInt32 *oldToNew,
                                                             TZrUInt32 oldLength,
                                                             TZrUInt32 newLength,
                                                             TZrUInt32 oldIndex) {
    if (oldToNew == ZR_NULL) {
        return newLength;
    }

    if (oldIndex >= oldLength) {
        return newLength;
    }

    while (oldIndex < oldLength) {
        if (oldToNew[oldIndex] != UINT32_MAX) {
            return oldToNew[oldIndex];
        }
        oldIndex++;
    }

    return newLength;
}

static TZrBool compiler_quickening_rewrite_compacted_branches(TZrInstruction *instructions,
                                                              const TZrUInt32 *oldToNew,
                                                              TZrUInt32 oldLength,
                                                              TZrUInt32 newLength) {
    TZrUInt32 oldIndex;

    if (instructions == ZR_NULL || oldToNew == ZR_NULL) {
        return ZR_FALSE;
    }

    for (oldIndex = 0; oldIndex < oldLength; oldIndex++) {
        TZrUInt32 newIndex;
        TZrInstruction *instruction;
        EZrInstructionCode opcode;
        TZrInt64 targetIndex;
        TZrUInt32 remappedTarget;
        TZrInt64 newOffset;

        if (oldToNew[oldIndex] == UINT32_MAX) {
            continue;
        }

        newIndex = oldToNew[oldIndex];
        instruction = &instructions[newIndex];
        opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        if (opcode != ZR_INSTRUCTION_ENUM(JUMP) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF) &&
            opcode != ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            continue;
        }

        targetIndex = (TZrInt64)oldIndex + 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            targetIndex += (TZrInt16)instruction->instruction.operand.operand1[1];
        } else {
            targetIndex += instruction->instruction.operand.operand2[0];
        }

        if (targetIndex < 0 || (TZrUInt64)targetIndex >= (TZrUInt64)oldLength) {
            return ZR_FALSE;
        }

        remappedTarget = compiler_quickening_remap_instruction_index(oldToNew,
                                                                     oldLength,
                                                                     newLength,
                                                                     (TZrUInt32)targetIndex);
        if (remappedTarget >= newLength) {
            return ZR_FALSE;
        }

        newOffset = (TZrInt64)remappedTarget - (TZrInt64)newIndex - 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            if (newOffset < INT16_MIN || newOffset > INT16_MAX) {
                return ZR_FALSE;
            }
            instruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)newOffset);
        } else {
            instruction->instruction.operand.operand2[0] = (TZrInt32)newOffset;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_compact_nops(SZrState *state, SZrFunction *function) {
    TZrUInt32 *oldToNew = ZR_NULL;
    TZrInstruction *newInstructions = ZR_NULL;
    TZrUInt32 *newLineInSourceList = ZR_NULL;
    SZrFunctionExecutionLocationInfo *newExecutionLocationInfoList = ZR_NULL;
    TZrUInt32 newExecutionLocationInfoLength = 0;
    TZrUInt32 liveCount = 0;
    TZrUInt32 oldIndex;
    TZrUInt32 executionInfoIndex;
    TZrBool hasNop = ZR_FALSE;
    TZrSize instructionBytes;
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    global = state->global;
    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        if ((EZrInstructionCode)function->instructionsList[oldIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(NOP)) {
            hasNop = ZR_TRUE;
            continue;
        }
        liveCount++;
    }

    if (!hasNop) {
        return ZR_TRUE;
    }

    oldToNew = (TZrUInt32 *)malloc(sizeof(*oldToNew) * function->instructionsLength);
    if (oldToNew == ZR_NULL) {
        return ZR_FALSE;
    }

    liveCount = 0;
    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        if ((EZrInstructionCode)function->instructionsList[oldIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(NOP)) {
            oldToNew[oldIndex] = UINT32_MAX;
        } else {
            oldToNew[oldIndex] = liveCount++;
        }
    }

    if (liveCount == 0) {
        free(oldToNew);
        return ZR_FALSE;
    }

    instructionBytes = sizeof(*newInstructions) * liveCount;
    newInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global,
                                                                        instructionBytes,
                                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newInstructions == ZR_NULL) {
        free(oldToNew);
        return ZR_FALSE;
    }

    if (function->lineInSourceList != ZR_NULL) {
        newLineInSourceList = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                           sizeof(*newLineInSourceList) * liveCount,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newLineInSourceList == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newInstructions,
                                          instructionBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            free(oldToNew);
            return ZR_FALSE;
        }
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        newExecutionLocationInfoList = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newExecutionLocationInfoList == ZR_NULL) {
            if (newLineInSourceList != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              newLineInSourceList,
                                              sizeof(*newLineInSourceList) * liveCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrCore_Memory_RawFreeWithType(global,
                                          newInstructions,
                                          instructionBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            free(oldToNew);
            return ZR_FALSE;
        }
    }

    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        TZrUInt32 newIndex;

        if (oldToNew[oldIndex] == UINT32_MAX) {
            continue;
        }

        newIndex = oldToNew[oldIndex];
        newInstructions[newIndex] = function->instructionsList[oldIndex];
        if (newLineInSourceList != ZR_NULL) {
            newLineInSourceList[newIndex] = function->lineInSourceList[oldIndex];
        }
    }

    if (!compiler_quickening_rewrite_compacted_branches(newInstructions,
                                                        oldToNew,
                                                        function->instructionsLength,
                                                        liveCount)) {
        if (newExecutionLocationInfoList != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newExecutionLocationInfoList,
                                          sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (newLineInSourceList != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newLineInSourceList,
                                          sizeof(*newLineInSourceList) * liveCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      newInstructions,
                                      instructionBytes,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        free(oldToNew);
        return ZR_FALSE;
    }

    if (newExecutionLocationInfoList != ZR_NULL) {
        for (executionInfoIndex = 0; executionInfoIndex < function->executionLocationInfoLength; executionInfoIndex++) {
            const SZrFunctionExecutionLocationInfo *oldInfo = &function->executionLocationInfoList[executionInfoIndex];
            TZrUInt32 remappedIndex = compiler_quickening_remap_instruction_index(oldToNew,
                                                                                  function->instructionsLength,
                                                                                  liveCount,
                                                                                  oldInfo->currentInstructionOffset);

            if (remappedIndex >= liveCount) {
                continue;
            }
            if (newExecutionLocationInfoLength > 0 &&
                newExecutionLocationInfoList[newExecutionLocationInfoLength - 1].currentInstructionOffset == remappedIndex) {
                continue;
            }

            newExecutionLocationInfoList[newExecutionLocationInfoLength] = *oldInfo;
            newExecutionLocationInfoList[newExecutionLocationInfoLength].currentInstructionOffset = remappedIndex;
            newExecutionLocationInfoLength++;
        }
    }

    for (oldIndex = 0; oldIndex < function->catchClauseCount; oldIndex++) {
        function->catchClauseList[oldIndex].targetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->catchClauseList[oldIndex].targetInstructionOffset);
    }

    for (oldIndex = 0; oldIndex < function->exceptionHandlerCount; oldIndex++) {
        function->exceptionHandlerList[oldIndex].protectedStartInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .protectedStartInstructionOffset);
        function->exceptionHandlerList[oldIndex].finallyTargetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .finallyTargetInstructionOffset);
        function->exceptionHandlerList[oldIndex].afterFinallyInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .afterFinallyInstructionOffset);
    }

    for (oldIndex = 0; oldIndex < function->semIrInstructionLength; oldIndex++) {
        function->semIrInstructions[oldIndex].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->semIrInstructions[oldIndex].execInstructionIndex);
    }

    for (oldIndex = 0; oldIndex < function->semIrDeoptTableLength; oldIndex++) {
        function->semIrDeoptTable[oldIndex].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->semIrDeoptTable[oldIndex].execInstructionIndex);
    }

    for (oldIndex = 0; oldIndex < function->callSiteCacheLength; oldIndex++) {
        function->callSiteCaches[oldIndex].instructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->callSiteCaches[oldIndex].instructionIndex);
    }

    ZR_MEMORY_RAW_FREE_LIST(global, function->instructionsList, function->instructionsLength);
    if (function->lineInSourceList != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->lineInSourceList, function->instructionsLength);
    }
    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->executionLocationInfoList, function->executionLocationInfoLength);
    }

    function->instructionsList = newInstructions;
    function->instructionsLength = liveCount;
    function->lineInSourceList = newLineInSourceList;
    function->executionLocationInfoList = newExecutionLocationInfoList;
    function->executionLocationInfoLength = newExecutionLocationInfoLength;

    free(oldToNew);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_dead_super_array_add_setup(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *addInstruction;
    TZrInstruction *constantInstruction;
    TZrInstruction *receiverReloadInstruction;
    TZrInstruction *receiverStageInstruction;
    TZrInstruction *receiverLoadInstruction;
    TZrUInt32 destinationSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 valueSlot;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 4 || instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (blockStarts[instructionIndex] || blockStarts[instructionIndex - 1] || blockStarts[instructionIndex - 2] ||
        blockStarts[instructionIndex - 3] || blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    addInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
        addInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    destinationSlot = addInstruction->instruction.operandExtra;
    receiverSlot = addInstruction->instruction.operand.operand1[0];
    valueSlot = addInstruction->instruction.operand.operand1[1];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    receiverReloadInstruction = &function->instructionsList[instructionIndex - 2];
    receiverStageInstruction = &function->instructionsList[instructionIndex - 3];
    receiverLoadInstruction = &function->instructionsList[instructionIndex - 4];

    if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != valueSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverReloadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        receiverReloadInstruction->instruction.operandExtra != destinationSlot ||
        (TZrUInt32)receiverReloadInstruction->instruction.operand.operand2[0] != receiverSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverStageInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        receiverStageInstruction->instruction.operandExtra != receiverSlot ||
        (TZrUInt32)receiverStageInstruction->instruction.operand.operand2[0] != destinationSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        receiverLoadInstruction->instruction.operandExtra != destinationSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 1, destinationSlot) &&
        !compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
                function,
                blockStarts,
                instructionIndex + 1,
                destinationSlot)) {
        return ZR_FALSE;
    }

    addInstruction->instruction.operand.operand1[0] =
            (TZrUInt16)receiverLoadInstruction->instruction.operand.operand2[0];
    addInstruction->instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    compiler_quickening_write_nop(receiverLoadInstruction);
    compiler_quickening_write_nop(receiverStageInstruction);
    compiler_quickening_write_nop(receiverReloadInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_add_int4_burst(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *firstInstruction;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 valueSlot;
    TZrUInt32 burstIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 3 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    firstInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)firstInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
        firstInstruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    receiverBaseSlot = firstInstruction->instruction.operand.operand1[0];
    valueSlot = firstInstruction->instruction.operand.operand1[1];
    for (burstIndex = 0; burstIndex < 4; burstIndex++) {
        TZrInstruction *instruction = &function->instructionsList[instructionIndex + burstIndex];

        if (burstIndex > 0 && blockStarts[instructionIndex + burstIndex]) {
            return ZR_FALSE;
        }
        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
            instruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG ||
            instruction->instruction.operand.operand1[0] != receiverBaseSlot + burstIndex ||
            instruction->instruction.operand.operand1[1] != valueSlot) {
            return ZR_FALSE;
        }
    }

    firstInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4);
    firstInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    firstInstruction->instruction.operand.operand1[1] = (TZrUInt16)valueSlot;
    for (burstIndex = 1; burstIndex < 4; burstIndex++) {
        compiler_quickening_write_nop(&function->instructionsList[instructionIndex + burstIndex]);
    }
    return ZR_TRUE;
}

static TZrBool compiler_quickening_instruction_may_read_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return ZR_FALSE;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return (TZrUInt32)instruction->instruction.operand.operand2[0] == slot;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            return instruction->instruction.operandExtra == slot;
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            return instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        default:
            if (instruction->instruction.operandExtra == slot ||
                instruction->instruction.operand.operand1[0] == slot ||
                instruction->instruction.operand.operand1[1] == slot ||
                (TZrUInt32)instruction->instruction.operand.operand2[0] == slot) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_instruction_writes_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return instruction->instruction.operandExtra == slot;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 compiler_quickening_find_block_end_index(const SZrFunction *function,
                                                          const TZrBool *blockStarts,
                                                          TZrUInt32 instructionIndex) {
    TZrUInt32 scan;

    if (function == ZR_NULL || blockStarts == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return 0;
    }

    for (scan = instructionIndex + 1; scan < function->instructionsLength; scan++) {
        if (blockStarts[scan]) {
            return scan - 1;
        }
    }

    return function->instructionsLength - 1;
}

static TZrBool compiler_quickening_block_ends_without_fallthrough(const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_slot_is_overwritten_before_any_read_linear(const SZrFunction *function,
                                                                              TZrUInt32 instructionIndex,
                                                                              TZrUInt32 slot) {
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex; scan < function->instructionsLength; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot) {
    TZrUInt32 blockEndIndex;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex) != ZR_NULL) {
        return ZR_FALSE;
    }

    blockEndIndex = compiler_quickening_find_block_end_index(function, blockStarts, instructionIndex);
    for (scan = instructionIndex; scan <= blockEndIndex; scan++) {
        if (compiler_quickening_instruction_may_read_slot(&function->instructionsList[scan], slot)) {
            return ZR_FALSE;
        }
    }

    return compiler_quickening_block_ends_without_fallthrough(&function->instructionsList[blockEndIndex]);
}

static TZrBool compiler_quickening_slot_is_overwritten_before_read(const SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex,
                                                                   TZrUInt32 slot) {
    TZrUInt32 scan;

    if (function == ZR_NULL || blockStarts == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex; scan < function->instructionsLength; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (scan > instructionIndex && blockStarts[scan]) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_temp_slot_is_dead_after_instruction(const SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex,
                                                                       TZrUInt32 slot) {
    if (function == ZR_NULL || blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instructionIndex + 1 >= function->instructionsLength) {
        return ZR_TRUE;
    }

    if (compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 1, slot)) {
        return ZR_TRUE;
    }

    return compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(function,
                                                                                     blockStarts,
                                                                                     instructionIndex + 1,
                                                                                     slot);
}

static TZrBool compiler_quickening_try_fold_stack_self_update_int_const(SZrFunction *function,
                                                                        const TZrBool *blockStarts,
                                                                        TZrUInt32 instructionIndex) {
    TZrInstruction *loadInstruction;
    TZrInstruction *arithmeticInstruction;
    TZrInstruction *storeInstruction;
    EZrInstructionCode arithmeticOpcode;
    TZrUInt32 sourceSlot;
    TZrUInt32 loadTempSlot;
    TZrUInt32 arithmeticTempSlot;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 2 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    loadInstruction = &function->instructionsList[instructionIndex];
    arithmeticInstruction = &function->instructionsList[instructionIndex + 1];
    storeInstruction = &function->instructionsList[instructionIndex + 2];
    arithmeticOpcode = (EZrInstructionCode)arithmeticInstruction->instruction.operationCode;
    if ((EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        ((EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
        (arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_INT_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(SUB_INT_CONST)) ||
        blockStarts[instructionIndex + 1] ||
        blockStarts[instructionIndex + 2]) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)loadInstruction->instruction.operand.operand2[0];
    loadTempSlot = loadInstruction->instruction.operandExtra;
    arithmeticTempSlot = arithmeticInstruction->instruction.operandExtra;
    if ((TZrUInt32)arithmeticInstruction->instruction.operand.operand1[0] != loadTempSlot ||
        (TZrUInt32)storeInstruction->instruction.operand.operand2[0] != arithmeticTempSlot ||
        storeInstruction->instruction.operandExtra != sourceSlot ||
        sourceSlot == loadTempSlot ||
        sourceSlot == arithmeticTempSlot) {
        return ZR_FALSE;
    }

    /*
     * This rewrite only bypasses a redundant GET_STACK/SET_STACK pair around an
     * existing *_INT_CONST op. The source slot type may be proven in a
     * predecessor block (for example loop counters initialized in a preheader),
     * so requiring an in-block int proof leaves valid self-updates behind
     * without improving safety.
     */
    if (!compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                 blockStarts,
                                                                 instructionIndex + 1,
                                                                 loadTempSlot) ||
        !compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                 blockStarts,
                                                                 instructionIndex + 2,
                                                                 arithmeticTempSlot)) {
        return ZR_FALSE;
    }

    arithmeticInstruction->instruction.operand.operand1[0] = (TZrUInt16)sourceSlot;
    arithmeticInstruction->instruction.operandExtra = (TZrUInt16)sourceSlot;
    compiler_quickening_write_nop(loadInstruction);
    compiler_quickening_write_nop(storeInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_right_const_arithmetic(SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *constantInstruction;
    EZrInstructionCode opcode;
    EZrInstructionCode constOpcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 constantSlot;
    TZrInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    constOpcode = compiler_quickening_specialized_int_const_opcode(opcode);
    if (constOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return ZR_FALSE;
    }

    constantIndex = constantInstruction->instruction.operand.operand2[0];
    if (constantIndex < 0 || constantIndex > UINT16_MAX ||
        !compiler_quickening_function_constant_is_int(function, (TZrUInt32)constantIndex)) {
        return ZR_FALSE;
    }

    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];
    constantSlot = constantInstruction->instruction.operandExtra;
    if (rightSlot != constantSlot || leftSlot == constantSlot) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operandExtra != constantSlot &&
        !compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, instructionIndex, constantSlot)) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)constOpcode;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_fold_right_const_arithmetic(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_right_const_arithmetic(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_stack_self_update_int_const(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 3) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 2 < function->instructionsLength; index++) {
        compiler_quickening_try_fold_stack_self_update_int_const(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(SZrFunction *function,
                                                                                      const TZrBool *blockStarts,
                                                                                      TZrUInt32 instructionIndex) {
    TZrInstruction *initConstantInstruction;
    TZrInstruction *initSetInstruction;
    TZrInstruction *minusOneInstruction;
    TZrInstruction *subtractInstruction;
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrInstruction *fillInstruction;
    TZrInstruction *incrementOneInstruction;
    TZrInstruction *incrementInstruction;
    TZrInstruction *indexStoreInstruction;
    TZrInstruction *jumpBackInstruction;
    TZrUInt32 indexSlot;
    TZrUInt32 countSlot;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 fillConstantIndex;
    TZrUInt32 minusOneConstantIndex;
    TZrUInt32 incrementOneConstantIndex;
    TZrInt64 constantValue;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 2 || instructionIndex + 8 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    initConstantInstruction = &function->instructionsList[instructionIndex - 2];
    initSetInstruction = &function->instructionsList[instructionIndex - 1];
    minusOneInstruction = &function->instructionsList[instructionIndex];
    subtractInstruction = &function->instructionsList[instructionIndex + 1];
    compareInstruction = &function->instructionsList[instructionIndex + 2];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 3];
    fillInstruction = &function->instructionsList[instructionIndex + 4];
    incrementOneInstruction = &function->instructionsList[instructionIndex + 5];
    incrementInstruction = &function->instructionsList[instructionIndex + 6];
    indexStoreInstruction = &function->instructionsList[instructionIndex + 7];
    jumpBackInstruction = &function->instructionsList[instructionIndex + 8];

    for (scan = instructionIndex + 1; scan <= instructionIndex + 8; scan++) {
        if (blockStarts[scan] && scan != instructionIndex + 4) {
            return ZR_FALSE;
        }
    }

    if ((EZrInstructionCode)initConstantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)initSetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)minusOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_INT) ||
        (EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        (EZrInstructionCode)fillInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST) ||
        (EZrInstructionCode)incrementOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)incrementInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_INT) ||
        (EZrInstructionCode)indexStoreInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)jumpBackInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_function_constant_read_int64(function,
                                                          (TZrUInt32)initConstantInstruction->instruction.operand.operand2[0],
                                                          &constantValue) ||
        constantValue != 0) {
        return ZR_FALSE;
    }

    indexSlot = initSetInstruction->instruction.operandExtra;
    if ((TZrUInt32)initSetInstruction->instruction.operand.operand2[0] != initConstantInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[0] != indexSlot ||
        incrementInstruction->instruction.operand.operand1[0] != indexSlot ||
        indexStoreInstruction->instruction.operandExtra != indexSlot ||
        (TZrUInt32)indexStoreInstruction->instruction.operand.operand2[0] != incrementInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    countSlot = subtractInstruction->instruction.operand.operand1[0];
    receiverBaseSlot = fillInstruction->instruction.operand.operand1[0];
    fillConstantIndex = fillInstruction->instruction.operand.operand1[1];
    minusOneConstantIndex = (TZrUInt32)minusOneInstruction->instruction.operand.operand2[0];
    incrementOneConstantIndex = (TZrUInt32)incrementOneInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_read_int64(function, minusOneConstantIndex, &constantValue) ||
        constantValue != 1 ||
        !compiler_quickening_function_constant_read_int64(function, incrementOneConstantIndex, &constantValue) ||
        constantValue != 1) {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[1] != minusOneInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[1] != subtractInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operand.operand2[0] != 6 ||
        incrementInstruction->instruction.operand.operand1[1] != incrementOneInstruction->instruction.operandExtra ||
        jumpBackInstruction->instruction.operand.operand2[0] != -9) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_any_read_linear(function, instructionIndex + 9, indexSlot)) {
        return ZR_FALSE;
    }

    minusOneInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST);
    minusOneInstruction->instruction.operandExtra = (TZrUInt16)fillConstantIndex;
    minusOneInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    minusOneInstruction->instruction.operand.operand1[1] = (TZrUInt16)countSlot;

    compiler_quickening_write_nop(initConstantInstruction);
    compiler_quickening_write_nop(initSetInstruction);
    compiler_quickening_write_nop(subtractInstruction);
    compiler_quickening_write_nop(compareInstruction);
    compiler_quickening_write_nop(jumpIfInstruction);
    compiler_quickening_write_nop(fillInstruction);
    compiler_quickening_write_nop(incrementOneInstruction);
    compiler_quickening_write_nop(incrementInstruction);
    compiler_quickening_write_nop(indexStoreInstruction);
    compiler_quickening_write_nop(jumpBackInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_add_int4_const(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *constantInstruction;
    TZrInstruction *burstInstruction;
    TZrUInt32 valueSlot;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[instructionIndex];
    burstInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)burstInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4) ||
        blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    valueSlot = constantInstruction->instruction.operandExtra;
    constantIndex = (TZrUInt32)constantInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_is_int(function, constantIndex) ||
        burstInstruction->instruction.operand.operand1[1] != valueSlot ||
        !compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 2, valueSlot)) {
        return ZR_FALSE;
    }

    constantInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST);
    constantInstruction->instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    constantInstruction->instruction.operand.operand1[0] = burstInstruction->instruction.operand.operand1[0];
    constantInstruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(burstInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop(SZrFunction *function,
                                                                             const TZrBool *blockStarts,
                                                                             TZrUInt32 instructionIndex) {
    TZrInstruction *initConstantInstruction;
    TZrInstruction *initSetInstruction;
    TZrInstruction *loopIndexLoadInstruction;
    TZrInstruction *countLoadInstruction;
    TZrInstruction *minusOneInstruction;
    TZrInstruction *subtractInstruction;
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrInstruction *fillInstruction;
    TZrInstruction *incrementIndexLoadInstruction;
    TZrInstruction *incrementOneInstruction;
    TZrInstruction *incrementInstruction;
    TZrInstruction *indexStoreInstruction;
    TZrInstruction *jumpBackInstruction;
    TZrUInt32 indexSlot;
    TZrUInt32 countSlot;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 fillConstantIndex;
    TZrUInt32 minusOneConstantIndex;
    TZrUInt32 incrementOneConstantIndex;
    TZrInt64 constantValue;
    TZrUInt32 scan;

    if (compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(function,
                                                                              blockStarts,
                                                                              instructionIndex)) {
        return ZR_TRUE;
    }

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 2 || instructionIndex + 11 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    initConstantInstruction = &function->instructionsList[instructionIndex - 2];
    initSetInstruction = &function->instructionsList[instructionIndex - 1];
    loopIndexLoadInstruction = &function->instructionsList[instructionIndex];
    countLoadInstruction = &function->instructionsList[instructionIndex + 1];
    minusOneInstruction = &function->instructionsList[instructionIndex + 2];
    subtractInstruction = &function->instructionsList[instructionIndex + 3];
    compareInstruction = &function->instructionsList[instructionIndex + 4];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 5];
    fillInstruction = &function->instructionsList[instructionIndex + 6];
    incrementIndexLoadInstruction = &function->instructionsList[instructionIndex + 7];
    incrementOneInstruction = &function->instructionsList[instructionIndex + 8];
    incrementInstruction = &function->instructionsList[instructionIndex + 9];
    indexStoreInstruction = &function->instructionsList[instructionIndex + 10];
    jumpBackInstruction = &function->instructionsList[instructionIndex + 11];

    for (scan = instructionIndex + 1; scan <= instructionIndex + 11; scan++) {
        if (blockStarts[scan] && scan != instructionIndex + 6) {
            return ZR_FALSE;
        }
    }

    if ((EZrInstructionCode)initConstantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)initSetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)loopIndexLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)countLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)minusOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_INT) ||
        (EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        (EZrInstructionCode)fillInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST) ||
        (EZrInstructionCode)incrementIndexLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)incrementOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)incrementInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_INT) ||
        (EZrInstructionCode)indexStoreInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)jumpBackInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_function_constant_read_int64(function,
                                                          (TZrUInt32)initConstantInstruction->instruction.operand.operand2[0],
                                                          &constantValue) ||
        constantValue != 0) {
        return ZR_FALSE;
    }

    indexSlot = initSetInstruction->instruction.operandExtra;
    if ((TZrUInt32)initSetInstruction->instruction.operand.operand2[0] != initConstantInstruction->instruction.operandExtra ||
        (TZrUInt32)loopIndexLoadInstruction->instruction.operand.operand2[0] != indexSlot ||
        (TZrUInt32)incrementIndexLoadInstruction->instruction.operand.operand2[0] != indexSlot ||
        indexStoreInstruction->instruction.operandExtra != indexSlot ||
        (TZrUInt32)indexStoreInstruction->instruction.operand.operand2[0] != incrementInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    countSlot = (TZrUInt32)countLoadInstruction->instruction.operand.operand2[0];
    receiverBaseSlot = fillInstruction->instruction.operand.operand1[0];
    fillConstantIndex = fillInstruction->instruction.operand.operand1[1];
    minusOneConstantIndex = (TZrUInt32)minusOneInstruction->instruction.operand.operand2[0];
    incrementOneConstantIndex = (TZrUInt32)incrementOneInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_read_int64(function, minusOneConstantIndex, &constantValue) ||
        constantValue != 1 ||
        !compiler_quickening_function_constant_read_int64(function, incrementOneConstantIndex, &constantValue) ||
        constantValue != 1) {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[0] != countLoadInstruction->instruction.operandExtra ||
        subtractInstruction->instruction.operand.operand1[1] != minusOneInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[0] != loopIndexLoadInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[1] != subtractInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operand.operand2[0] != 6 ||
        incrementInstruction->instruction.operand.operand1[0] != incrementIndexLoadInstruction->instruction.operandExtra ||
        incrementInstruction->instruction.operand.operand1[1] != incrementOneInstruction->instruction.operandExtra ||
        jumpBackInstruction->instruction.operand.operand2[0] != -12) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_any_read_linear(function, instructionIndex + 12, indexSlot)) {
        return ZR_FALSE;
    }

    loopIndexLoadInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST);
    loopIndexLoadInstruction->instruction.operandExtra = (TZrUInt16)fillConstantIndex;
    loopIndexLoadInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    loopIndexLoadInstruction->instruction.operand.operand1[1] = (TZrUInt16)countSlot;

    compiler_quickening_write_nop(initConstantInstruction);
    compiler_quickening_write_nop(initSetInstruction);
    compiler_quickening_write_nop(countLoadInstruction);
    compiler_quickening_write_nop(minusOneInstruction);
    compiler_quickening_write_nop(subtractInstruction);
    compiler_quickening_write_nop(compareInstruction);
    compiler_quickening_write_nop(jumpIfInstruction);
    compiler_quickening_write_nop(fillInstruction);
    compiler_quickening_write_nop(incrementIndexLoadInstruction);
    compiler_quickening_write_nop(incrementOneInstruction);
    compiler_quickening_write_nop(incrementInstruction);
    compiler_quickening_write_nop(indexStoreInstruction);
    compiler_quickening_write_nop(jumpBackInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quicken_array_int_index_accesses(SZrFunction *function) {
    ZrCompilerQuickeningSlotAlias *aliases = ZR_NULL;
    EZrCompilerQuickeningSlotKind *slotKinds = ZR_NULL;
    TZrBool *blockStarts = ZR_NULL;
    TZrBool *constantSlotsValid = ZR_NULL;
    TZrUInt32 *constantSlotIndices = ZR_NULL;
    TZrUInt32 aliasCount;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    aliasCount = function->stackSize;
    if (aliasCount > 0) {
        aliases = (ZrCompilerQuickeningSlotAlias *)malloc(sizeof(*aliases) * aliasCount);
        slotKinds = (EZrCompilerQuickeningSlotKind *)malloc(sizeof(*slotKinds) * aliasCount);
        constantSlotsValid = (TZrBool *)malloc(sizeof(*constantSlotsValid) * aliasCount);
        constantSlotIndices = (TZrUInt32 *)malloc(sizeof(*constantSlotIndices) * aliasCount);
        if (aliases == ZR_NULL || slotKinds == ZR_NULL || constantSlotsValid == ZR_NULL || constantSlotIndices == ZR_NULL) {
            free(constantSlotIndices);
            free(constantSlotsValid);
            free(slotKinds);
            free(aliases);
            return ZR_FALSE;
        }
        compiler_quickening_clear_aliases(aliases, aliasCount);
        compiler_quickening_clear_slot_kinds(slotKinds, aliasCount);
        memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
        memset(constantSlotIndices, 0, sizeof(*constantSlotIndices) * aliasCount);
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        free(constantSlotIndices);
        free(constantSlotsValid);
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        free(constantSlotIndices);
        free(constantSlotsValid);
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

        if (blockStarts[index]) {
            compiler_quickening_clear_aliases(aliases, aliasCount);
            compiler_quickening_clear_slot_kinds(slotKinds, aliasCount);
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX) || opcode == ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) {
            TZrUInt32 receiverSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 keySlot = instruction->instruction.operand.operand1[1];
            if (compiler_quickening_slot_is_array_int(function,
                                                      aliases,
                                                      aliasCount,
                                                      slotKinds,
                                                      index,
                                                      receiverSlot) &&
                compiler_quickening_slot_is_int(function,
                                                aliases,
                                                aliasCount,
                                                slotKinds,
                                                blockStarts,
                                                index,
                                                keySlot)) {
                instruction->instruction.operationCode =
                        (TZrUInt16)(opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX)
                                            ? ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)
                                            : ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT));
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) && index > 0) {
            TZrUInt32 getMemberInstructionIndex = UINT32_MAX;
            TZrInstruction *getMemberInstruction = compiler_quickening_find_latest_block_writer(function,
                                                                                                blockStarts,
                                                                                                index,
                                                                                                instruction->instruction.operand.operand1[0],
                                                                                                &getMemberInstructionIndex);
            EZrInstructionCode getMemberOpcode =
                    getMemberInstruction != ZR_NULL
                            ? (EZrInstructionCode)getMemberInstruction->instruction.operationCode
                            : ZR_INSTRUCTION_ENUM(ENUM_MAX);
            TZrUInt16 functionSlot = instruction->instruction.operand.operand1[0];
            TZrUInt16 parameterCount = instruction->instruction.operand.operand1[1];

            if (getMemberInstruction != ZR_NULL &&
                getMemberInstructionIndex < index &&
                getMemberOpcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) &&
                getMemberInstruction->instruction.operandExtra == functionSlot &&
                parameterCount == 2) {
                const TZrChar *memberName = compiler_quickening_member_entry_symbol_text(
                        function,
                        getMemberInstruction->instruction.operand.operand1[1]);
                TZrUInt32 receiverArgumentSlot = (TZrUInt32)functionSlot + 1u;
                TZrUInt32 valueArgumentSlot = (TZrUInt32)functionSlot + 2u;

                if (memberName != ZR_NULL &&
                    strcmp(memberName, "add") == 0 &&
                    compiler_quickening_slot_is_array_int(function,
                                                          aliases,
                                                          aliasCount,
                                                          slotKinds,
                                                          index,
                                                          receiverArgumentSlot) &&
                    compiler_quickening_slot_is_int(function,
                                                    aliases,
                                                    aliasCount,
                                                    slotKinds,
                                                    blockStarts,
                                                    index,
                                                    valueArgumentSlot)) {
                    getMemberInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
                    getMemberInstruction->instruction.operand.operand2[0] = (TZrInt32)receiverArgumentSlot;

                    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT);
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)receiverArgumentSlot;
                    instruction->instruction.operand.operand1[1] = (TZrUInt16)valueArgumentSlot;
                    opcode = ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT);
                }
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(ADD) ||
            opcode == ZR_INSTRUCTION_ENUM(SUB) ||
            opcode == ZR_INSTRUCTION_ENUM(MUL) ||
            opcode == ZR_INSTRUCTION_ENUM(DIV) ||
            opcode == ZR_INSTRUCTION_ENUM(MOD)) {
            TZrUInt32 leftSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 rightSlot = instruction->instruction.operand.operand1[1];

            if (compiler_quickening_slot_is_int(function,
                                                aliases,
                                                aliasCount,
                                                slotKinds,
                                                blockStarts,
                                                index,
                                                leftSlot) &&
                compiler_quickening_slot_is_int(function,
                                                aliases,
                                                aliasCount,
                                                slotKinds,
                                                blockStarts,
                                                index,
                                                rightSlot)) {
                instruction->instruction.operationCode =
                        (TZrUInt16)compiler_quickening_specialized_int_opcode(opcode);
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
            TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
            TZrUInt32 rootSlot = 0;
            if (compiler_quickening_resolve_alias_slot(function,
                                                       aliases,
                                                       aliasCount,
                                                       index,
                                                       sourceSlot,
                                                       &rootSlot)) {
                if (aliases != ZR_NULL && destinationSlot < aliasCount) {
                    aliases[destinationSlot].valid = ZR_TRUE;
                    aliases[destinationSlot].rootSlot = rootSlot;
                }
            } else {
                compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
            }
            if (slotKinds != ZR_NULL && destinationSlot < aliasCount) {
                slotKinds[destinationSlot] = compiler_quickening_slot_kind_for_slot(function,
                                                                                    aliases,
                                                                                    aliasCount,
                                                                                    slotKinds,
                                                                                    index,
                                                                                    sourceSlot);
            }
            continue;
        }

        compiler_quickening_clear_call_argument_tracking(function,
                                                         instruction,
                                                         aliases,
                                                         slotKinds,
                                                         ZR_NULL,
                                                         ZR_NULL,
                                                         aliasCount);

        if (slotKinds != ZR_NULL && destinationSlot < aliasCount) {
            switch (opcode) {
                case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                    slotKinds[destinationSlot] = compiler_quickening_function_constant_is_int(
                                                         function,
                                                         (TZrUInt32)instruction->instruction.operand.operand2[0])
                                                         ? ZR_COMPILER_QUICKENING_SLOT_KIND_INT
                                                         : ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
                    break;
                case ZR_INSTRUCTION_ENUM(TO_INT):
                case ZR_INSTRUCTION_ENUM(ADD_INT):
                case ZR_INSTRUCTION_ENUM(SUB_INT):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_INT;
                    break;
                default:
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
                    break;
            }
        }

        if (!compiler_quickening_is_control_only_opcode(opcode)) {
            compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
        }
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_dead_super_array_add_setup(function, blockStarts, index);
    }

    if (constantSlotsValid != ZR_NULL && constantSlotIndices != ZR_NULL && aliasCount > 0) {
        memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
        memset(constantSlotIndices, 0, sizeof(*constantSlotIndices) * aliasCount);
        for (index = 0; index < function->instructionsLength; index++) {
            TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

            if (blockStarts[index]) {
                memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
            }

            if (opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) && destinationSlot < aliasCount) {
                TZrUInt32 constantIndex = (TZrUInt32)instruction->instruction.operand.operand2[0];

                if (constantSlotsValid[destinationSlot] && constantSlotIndices[destinationSlot] == constantIndex) {
                    compiler_quickening_write_nop(instruction);
                    continue;
                }

                constantSlotsValid[destinationSlot] = ZR_TRUE;
                constantSlotIndices[destinationSlot] = constantIndex;
                continue;
            }

            compiler_quickening_clear_call_argument_tracking(function,
                                                             instruction,
                                                             ZR_NULL,
                                                             ZR_NULL,
                                                             constantSlotsValid,
                                                             constantSlotIndices,
                                                             aliasCount);

            if (!compiler_quickening_is_control_only_opcode(opcode) && destinationSlot < aliasCount) {
                constantSlotsValid[destinationSlot] = ZR_FALSE;
                constantSlotIndices[destinationSlot] = 0;
            }
        }
    }

    success = ZR_TRUE;
    free(blockStarts);
    free(constantSlotIndices);
    free(constantSlotsValid);
    free(slotKinds);
    free(aliases);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_add_int4_bursts(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 4) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_add_int4_burst(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_add_int4_const_bursts(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_add_int4_const(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_fill_int4_const_loops(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 12) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_fill_int4_const_loop(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrUInt32 compiler_quickening_find_deopt_id(const SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if (instruction->execInstructionIndex == instructionIndex) {
            return instruction->deoptId;
        }
    }

    return ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;
}

static TZrUInt8 compiler_quickening_member_entry_flags(const SZrFunction *function, TZrUInt32 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE;
    }

    return function->memberEntries[memberEntryIndex].reserved0;
}

static TZrBool compiler_quickening_function_matches_inline_child(const SZrFunction *left, const SZrFunction *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    return left->functionName == right->functionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static void compiler_quickening_rebind_constant_function_values_to_children(SZrFunction *function) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || function->childFunctionList == ZR_NULL ||
        function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrRawObject *rawObject;
        SZrFunction *constantFunction = ZR_NULL;

        if ((constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) ||
            constant->value.object == ZR_NULL) {
            continue;
        }

        rawObject = constant->value.object;
        if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            constantFunction = ZR_CAST(SZrFunction *, rawObject);
        } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
            SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
            if (closure != ZR_NULL) {
                constantFunction = closure->function;
            }
        }

        if (constantFunction == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            SZrFunction *childFunction = &function->childFunctionList[childIndex];
            if (!compiler_quickening_function_matches_inline_child(constantFunction, childFunction)) {
                continue;
            }

            if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                constant->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(childFunction);
            } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
                SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
                if (closure != ZR_NULL) {
                    closure->function = childFunction;
                }
            }
            break;
        }
    }
}

static TZrBool compiler_quickening_append_callsite_cache(SZrState *state,
                                                         SZrFunction *function,
                                                         EZrFunctionCallSiteCacheKind kind,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 memberEntryIndex,
                                                         TZrUInt32 deoptId,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt16 *outCacheIndex) {
    SZrFunctionCallSiteCacheEntry *newEntries;
    TZrSize newCount;
    TZrSize copyBytes;

    if (outCacheIndex != ZR_NULL) {
        *outCacheIndex = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outCacheIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    newCount = (TZrSize)function->callSiteCacheLength + 1;
    if (newCount > UINT16_MAX) {
        return ZR_FALSE;
    }

    newEntries = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallSiteCacheEntry) * newCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }

    copyBytes = sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength;
    if (function->callSiteCaches != ZR_NULL && copyBytes > 0) {
        memcpy(newEntries, function->callSiteCaches, copyBytes);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->callSiteCaches,
                                      sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    memset(&newEntries[newCount - 1], 0, sizeof(SZrFunctionCallSiteCacheEntry));
    newEntries[newCount - 1].kind = (TZrUInt32)kind;
    newEntries[newCount - 1].instructionIndex = instructionIndex;
    newEntries[newCount - 1].memberEntryIndex = memberEntryIndex;
    newEntries[newCount - 1].deoptId = deoptId;
    newEntries[newCount - 1].argumentCount = argumentCount;

    function->callSiteCaches = newEntries;
    function->callSiteCacheLength = (TZrUInt32)newCount;
    *outCacheIndex = (TZrUInt16)(newCount - 1);
    return ZR_TRUE;
}

static TZrBool compiler_quicken_cached_calls(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 argumentCount;

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(META_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED);
                break;
            default:
                continue;
        }

        argumentCount = instruction->instruction.operand.operand1[1];
        if (argumentCount == 0) {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       argumentCount,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_dynamic_iter_loop_guards(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        TZrInstruction *iterMoveNextInst = &function->instructionsList[index];
        TZrInstruction *jumpIfInst = &function->instructionsList[index + 1];
        TZrInt32 jumpOffset;

        if ((EZrInstructionCode)iterMoveNextInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)) {
            continue;
        }
        if ((EZrInstructionCode)jumpIfInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            continue;
        }
        if (jumpIfInst->instruction.operandExtra != iterMoveNextInst->instruction.operandExtra) {
            continue;
        }

        jumpOffset = jumpIfInst->instruction.operand.operand2[0];
        if (jumpOffset < INT16_MIN || jumpOffset > INT16_MAX) {
            continue;
        }

        iterMoveNextInst->instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE);
        iterMoveNextInst->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)jumpOffset);
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_zero_arg_calls(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *callInst = &function->instructionsList[index];

        if (callInst->instruction.operand.operand1[1] != 0) {
            continue;
        }

        switch ((EZrInstructionCode)callInst->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS);
                break;
            default:
                break;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_meta_access(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 memberEntryIndex;
        TZrUInt8 memberFlags;
        TZrBool isStaticAccessor;

        if (opcode == ZR_INSTRUCTION_ENUM(META_GET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED);
        } else if (opcode == ZR_INSTRUCTION_ENUM(META_SET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED);
        } else {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       memberEntryIndex,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       0,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_child_functions(SZrState *state, SZrFunction *function) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!compiler_quicken_meta_access(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_cached_calls(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_dynamic_iter_loop_guards(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_zero_arg_calls(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_array_int_index_accesses(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_fold_super_array_add_int4_bursts(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_fold_super_array_add_int4_const_bursts(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_fold_super_array_fill_int4_const_loops(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_fold_right_const_arithmetic(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_fold_stack_self_update_int_const(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_compact_nops(state, function)) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_quicken_child_functions(state, &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }

    compiler_quickening_rebind_constant_function_values_to_children(function);

    return ZR_TRUE;
}

TZrBool compiler_quicken_execbc_function(SZrState *state, SZrFunction *function) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    return compiler_quicken_child_functions(state, function);
}
