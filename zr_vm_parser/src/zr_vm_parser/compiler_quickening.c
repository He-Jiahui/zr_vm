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

static TZrBool compiler_quickening_resolve_alias_slot(const SZrFunction *function,
                                                      const ZrCompilerQuickeningSlotAlias *aliases,
                                                      TZrUInt32 aliasCount,
                                                      TZrUInt32 slot,
                                                      TZrUInt32 *outRootSlot) {
    if (outRootSlot != ZR_NULL) {
        *outRootSlot = 0;
    }

    if (function == ZR_NULL || outRootSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (aliases != ZR_NULL && slot < aliasCount && aliases[slot].valid) {
        *outRootSlot = aliases[slot].rootSlot;
        return ZR_TRUE;
    }

    if (compiler_quickening_find_typed_local_binding(function, slot) != ZR_NULL) {
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

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot);

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_for_slot(const SZrFunction *function,
                                                                             const ZrCompilerQuickeningSlotAlias *aliases,
                                                                             TZrUInt32 aliasCount,
                                                                             const EZrCompilerQuickeningSlotKind *slotKinds,
                                                                             TZrUInt32 slot) {
    TZrUInt32 rootSlot = 0;
    const SZrFunctionTypedLocalBinding *binding;

    if (slotKinds != ZR_NULL && slot < aliasCount &&
        slotKinds[slot] != ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN) {
        return slotKinds[slot];
    }

    if (compiler_quickening_resolve_alias_slot(function, aliases, aliasCount, slot, &rootSlot)) {
        binding = compiler_quickening_find_typed_local_binding(function, rootSlot);
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    binding = compiler_quickening_find_typed_local_binding(function, slot);
    return compiler_quickening_slot_kind_from_binding(binding);
}

static TZrBool compiler_quickening_slot_is_int(const SZrFunction *function,
                                               const ZrCompilerQuickeningSlotAlias *aliases,
                                               TZrUInt32 aliasCount,
                                               const EZrCompilerQuickeningSlotKind *slotKinds,
                                               const TZrBool *blockStarts,
                                               TZrUInt32 instructionIndex,
                                               TZrUInt32 slot) {
    return compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, slot) ==
                   ZR_COMPILER_QUICKENING_SLOT_KIND_INT ||
           compiler_quickening_resolve_index_access_int_constant(function, blockStarts, instructionIndex, slot);
}

static TZrBool compiler_quickening_slot_is_array_int(const SZrFunction *function,
                                                     const ZrCompilerQuickeningSlotAlias *aliases,
                                                     TZrUInt32 aliasCount,
                                                     const EZrCompilerQuickeningSlotKind *slotKinds,
                                                     TZrUInt32 slot) {
    return compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, slot) ==
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
            TZrInt32 targetIndex = (TZrInt32)index + instruction->instruction.operand.operand2[0];
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            TZrInt32 targetIndex = (TZrInt32)index + (TZrInt16)instruction->instruction.operand.operand1[1];
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

static TZrBool compiler_quicken_array_int_index_accesses(SZrFunction *function) {
    ZrCompilerQuickeningSlotAlias *aliases = ZR_NULL;
    EZrCompilerQuickeningSlotKind *slotKinds = ZR_NULL;
    TZrBool *blockStarts = ZR_NULL;
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
        if (aliases == ZR_NULL || slotKinds == ZR_NULL) {
            free(slotKinds);
            free(aliases);
            return ZR_FALSE;
        }
        compiler_quickening_clear_aliases(aliases, aliasCount);
        compiler_quickening_clear_slot_kinds(slotKinds, aliasCount);
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
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
            if (compiler_quickening_slot_is_array_int(function, aliases, aliasCount, slotKinds, receiverSlot) &&
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
            if (compiler_quickening_resolve_alias_slot(function, aliases, aliasCount, sourceSlot, &rootSlot)) {
                if (aliases != ZR_NULL && destinationSlot < aliasCount) {
                    aliases[destinationSlot].valid = ZR_TRUE;
                    aliases[destinationSlot].rootSlot = rootSlot;
                }
            } else {
                compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
            }
            if (slotKinds != ZR_NULL && destinationSlot < aliasCount) {
                slotKinds[destinationSlot] =
                        compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, sourceSlot);
            }
            continue;
        }

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

    success = ZR_TRUE;
    free(blockStarts);
    free(slotKinds);
    free(aliases);
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
