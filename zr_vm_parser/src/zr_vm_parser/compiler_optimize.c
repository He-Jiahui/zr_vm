#include "compiler_internal.h"

#include <stdlib.h>

typedef struct SZrOptimizerInstructionInfo {
    TZrUInt16 readSlots[4];
    TZrUInt8 readCount;
    TZrUInt16 writeSlots[2];
    TZrUInt8 writeCount;
    TZrUInt16 rangeReadStart;
    TZrUInt16 rangeReadCount;
    TZrBool hasRangeRead;
    TZrBool readsAllSlots;
    TZrBool hasRelativeJump;
    TZrBool conditionalJump;
    TZrBool terminator;
    TZrBool allowSlotReuse;
    TZrBool operand1Index1IsSlot;
} SZrOptimizerInstructionInfo;

typedef struct SZrOptimizerBlock {
    TZrSize start;
    TZrSize end;
    TZrSize successors[2];
    TZrUInt8 successorCount;
    TZrBool allowSlotReuse;
} SZrOptimizerBlock;

typedef struct SZrOptimizerInterval {
    TZrUInt16 originalSlot;
    TZrUInt16 assignedSlot;
    TZrSize start;
    TZrSize end;
    TZrBool fixed;
    TZrBool valid;
} SZrOptimizerInterval;

#define ZR_OPTIMIZER_INDEX_NONE ((TZrSize)-1)

static TZrBool optimizer_is_exception_control_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TRY):
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(CATCH):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool optimizer_function_contains_exception_control(const TZrInstruction *instructions, TZrSize count) {
    TZrSize instructionIndex;

    if (instructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0; instructionIndex < count; instructionIndex++) {
        EZrInstructionCode opcode = (EZrInstructionCode)instructions[instructionIndex].instruction.operationCode;
        if (optimizer_is_exception_control_opcode(opcode)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrSize optimizer_clamp_instruction_boundary(TZrMemoryOffset offset, TZrSize instructionCount) {
    TZrSize boundary = (TZrSize)offset;

    if (boundary > instructionCount) {
        return instructionCount;
    }

    return boundary;
}

static void optimizer_remap_local_variable_instruction_offsets(SZrCompilerState *cs,
                                                               const TZrSize *newBoundaryIndex,
                                                               TZrSize instructionCount) {
    TZrSize index;

    if (cs == ZR_NULL || newBoundaryIndex == ZR_NULL) {
        return;
    }

    for (index = 0; index < cs->localVars.length; index++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, index);
        TZrSize oldActivate;
        TZrSize oldDead;

        if (localVar == ZR_NULL) {
            continue;
        }

        oldActivate = optimizer_clamp_instruction_boundary(localVar->offsetActivate, instructionCount);
        oldDead = optimizer_clamp_instruction_boundary(localVar->offsetDead, instructionCount);
        localVar->offsetActivate = (TZrMemoryOffset)newBoundaryIndex[oldActivate];
        localVar->offsetDead = (TZrMemoryOffset)newBoundaryIndex[oldDead];
        if (localVar->offsetDead < localVar->offsetActivate) {
            localVar->offsetDead = localVar->offsetActivate;
        }
    }
}

static void optimizer_info_add_read(SZrOptimizerInstructionInfo *info, TZrUInt16 slot) {
    if (info != ZR_NULL && info->readCount < ZR_ARRAY_COUNT(info->readSlots)) {
        info->readSlots[info->readCount++] = slot;
    }
}

static void optimizer_info_add_write(SZrOptimizerInstructionInfo *info, TZrUInt16 slot) {
    if (info != ZR_NULL && info->writeCount < ZR_ARRAY_COUNT(info->writeSlots)) {
        info->writeSlots[info->writeCount++] = slot;
    }
}

static void optimizer_info_init(SZrOptimizerInstructionInfo *info) {
    if (info == ZR_NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));
    info->allowSlotReuse = ZR_TRUE;
    info->operand1Index1IsSlot = ZR_TRUE;
}

static void optimizer_classify_instruction(const TZrInstruction *instruction, SZrOptimizerInstructionInfo *info) {
    EZrInstructionCode opcode;

    optimizer_info_init(info);
    if (instruction == ZR_NULL || info == ZR_NULL) {
        return;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
            optimizer_info_add_read(info, (TZrUInt16)instruction->instruction.operand.operand2[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            optimizer_info_add_read(info, (TZrUInt16)instruction->instruction.operand.operand2[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
            optimizer_info_add_read(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            if (opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) ||
                opcode == ZR_INSTRUCTION_ENUM(META_GET) ||
                opcode == ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED) ||
                opcode == ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)) {
                info->operand1Index1IsSlot = ZR_FALSE;
            }
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            info->operand1Index1IsSlot = ZR_FALSE;
            optimizer_info_add_read(info, instruction->instruction.operandExtra);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            return;
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
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
        case ZR_INSTRUCTION_ENUM(POW):
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
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
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[1]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            info->operand1Index1IsSlot = ZR_FALSE;
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            return;
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            optimizer_info_add_read(info, instruction->instruction.operandExtra);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[1]);
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[1]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 1u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 2u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 3u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[1]);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 1u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 2u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 3u);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 1u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 2u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0] + 3u);
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[1]);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(JUMP):
            info->hasRelativeJump = ZR_TRUE;
            info->terminator = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            optimizer_info_add_read(info, instruction->instruction.operandExtra);
            info->hasRelativeJump = ZR_TRUE;
            info->conditionalJump = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->hasRelativeJump = ZR_TRUE;
            info->conditionalJump = ZR_TRUE;
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            info->terminator = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            info->readsAllSlots = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            info->readsAllSlots = ZR_TRUE;
            info->terminator = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
            info->hasRangeRead = ZR_TRUE;
            info->rangeReadStart = instruction->instruction.operand.operand1[0];
            info->rangeReadCount = (TZrUInt16)(instruction->instruction.operand.operand1[1] + 1);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            return;
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            info->hasRangeRead = ZR_TRUE;
            info->rangeReadStart = instruction->instruction.operand.operand1[0];
            info->rangeReadCount = (TZrUInt16)(instruction->instruction.operand.operand1[1] + 1);
            optimizer_info_add_write(info, instruction->instruction.operandExtra);
            info->allowSlotReuse = ZR_FALSE;
            info->terminator = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            if (instruction->instruction.operandExtra > 1) {
                info->hasRangeRead = ZR_TRUE;
                info->rangeReadStart = instruction->instruction.operand.operand1[0];
                info->rangeReadCount = instruction->instruction.operandExtra;
                info->allowSlotReuse = ZR_FALSE;
            } else if (instruction->instruction.operandExtra == 1) {
                optimizer_info_add_read(info, instruction->instruction.operand.operand1[0]);
            }
            info->terminator = ZR_TRUE;
            return;
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
            info->allowSlotReuse = ZR_FALSE;
            return;
        default:
            info->allowSlotReuse = ZR_FALSE;
            info->readsAllSlots = ZR_TRUE;
            return;
    }
}

static TZrInt32 optimizer_relative_jump_offset(const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return 0;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
        return (TZrInt16)instruction->instruction.operand.operand1[1];
    }

    return instruction->instruction.operand.operand2[0];
}

static void optimizer_store_relative_jump_offset(TZrInstruction *instruction, TZrInt32 offset) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
        instruction->instruction.operand.operand1[1] = (TZrUInt16)(TZrInt16)offset;
        return;
    }

    instruction->instruction.operand.operand2[0] = offset;
}

static void optimizer_mark_use_set(TZrUInt8 *useSet,
                                   TZrUInt8 *defSet,
                                   TZrSize slotCount,
                                   const SZrOptimizerInstructionInfo *info) {
    TZrSize index;

    if (useSet == ZR_NULL || defSet == ZR_NULL || info == ZR_NULL) {
        return;
    }

    if (info->readsAllSlots) {
        for (index = 0; index < slotCount; index++) {
            if (!defSet[index]) {
                useSet[index] = 1;
            }
        }
    }

    if (info->hasRangeRead) {
        TZrSize end = (TZrSize)info->rangeReadStart + info->rangeReadCount;
        for (index = info->rangeReadStart; index < end && index < slotCount; index++) {
            if (!defSet[index]) {
                useSet[index] = 1;
            }
        }
    }

    for (index = 0; index < info->readCount; index++) {
        TZrSize slot = info->readSlots[index];
        if (slot < slotCount && !defSet[slot]) {
            useSet[slot] = 1;
        }
    }

    for (index = 0; index < info->writeCount; index++) {
        TZrSize slot = info->writeSlots[index];
        if (slot < slotCount) {
            defSet[slot] = 1;
        }
    }
}

static void optimizer_live_remove_writes(TZrUInt8 *live,
                                         TZrSize slotCount,
                                         const SZrOptimizerInstructionInfo *info) {
    TZrSize index;

    if (live == ZR_NULL || info == ZR_NULL) {
        return;
    }

    for (index = 0; index < info->writeCount; index++) {
        TZrSize slot = info->writeSlots[index];
        if (slot < slotCount) {
            live[slot] = 0;
        }
    }
}

static void optimizer_live_add_reads(TZrUInt8 *live,
                                     TZrSize slotCount,
                                     const SZrOptimizerInstructionInfo *info) {
    TZrSize index;

    if (live == ZR_NULL || info == ZR_NULL) {
        return;
    }

    if (info->readsAllSlots) {
        memset(live, 1, slotCount);
    }

    if (info->hasRangeRead) {
        TZrSize end = (TZrSize)info->rangeReadStart + info->rangeReadCount;
        for (index = info->rangeReadStart; index < end && index < slotCount; index++) {
            live[index] = 1;
        }
    }

    for (index = 0; index < info->readCount; index++) {
        TZrSize slot = info->readSlots[index];
        if (slot < slotCount) {
            live[slot] = 1;
        }
    }
}

static TZrBool optimizer_slot_is_local(const TZrUInt8 *localSlots, TZrSize slotCount, TZrUInt16 slot) {
    return localSlots != ZR_NULL && slot < slotCount && localSlots[slot] != 0;
}

static TZrBool optimizer_slot_is_active_local_at_instruction(const SZrCompilerState *cs,
                                                             TZrUInt16 slot,
                                                             TZrSize instructionIndex) {
    TZrSize index;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < cs->localVars.length; index++) {
        const SZrFunctionLocalVariable *localVar =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get((SZrArray *)&cs->localVars, index);
        if (localVar == ZR_NULL || localVar->stackSlot != slot) {
            continue;
        }

        if ((TZrSize)localVar->offsetActivate <= instructionIndex && instructionIndex < (TZrSize)localVar->offsetDead) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void optimizer_mark_local_slots(const SZrCompilerState *cs, TZrUInt8 *localSlots, TZrSize slotCount) {
    TZrSize index;

    if (cs == ZR_NULL || localSlots == ZR_NULL) {
        return;
    }

    for (index = 0; index < cs->localVars.length; index++) {
        const SZrFunctionLocalVariable *localVar =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get((SZrArray *)&cs->localVars, index);
        if (localVar != ZR_NULL && localVar->stackSlot < slotCount) {
            localSlots[localVar->stackSlot] = 1;
        }
    }
}

static TZrBool optimizer_is_dead_null_clear(SZrCompilerState *cs,
                                            const TZrInstruction *instruction,
                                            const TZrUInt8 *liveAfter,
                                            TZrSize slotCount,
                                            const TZrUInt8 *localSlots) {
    TZrUInt32 constantIndex;
    TZrUInt16 destSlot;
    const SZrTypeValue *constantValue;

    if (cs == ZR_NULL || instruction == ZR_NULL || liveAfter == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return ZR_FALSE;
    }

    destSlot = instruction->instruction.operandExtra;
    if (destSlot >= slotCount || liveAfter[destSlot] || optimizer_slot_is_local(localSlots, slotCount, destSlot)) {
        return ZR_FALSE;
    }

    constantIndex = (TZrUInt32)instruction->instruction.operand.operand2[0];
    if (constantIndex >= cs->constants.length) {
        return ZR_FALSE;
    }

    constantValue = (const SZrTypeValue *)ZrCore_Array_Get(&cs->constants, constantIndex);
    return constantValue != ZR_NULL && ZR_VALUE_IS_TYPE_NULL(constantValue->type);
}

static TZrBool optimizer_try_rewrite_dead_result_to_ret(TZrInstruction *instruction,
                                                        const TZrUInt8 *liveAfter,
                                                        TZrSize slotCount,
                                                        const TZrUInt8 *localSlots) {
    TZrUInt16 destSlot;
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL || liveAfter == ZR_NULL) {
        return ZR_FALSE;
    }
    ZR_UNUSED_PARAMETER(localSlots);

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) &&
        opcode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) {
        return ZR_FALSE;
    }

    destSlot = instruction->instruction.operandExtra;
    if (destSlot >= slotCount || liveAfter[destSlot]) {
        return ZR_FALSE;
    }

    instruction->instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    return ZR_TRUE;
}

static TZrBool optimizer_is_dead_pure_write(const TZrInstruction *instruction,
                                            const SZrOptimizerInstructionInfo *info,
                                            const TZrUInt8 *liveAfter,
                                            TZrSize slotCount,
                                            const TZrUInt8 *localSlots) {
    TZrUInt16 destSlot;
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL || info == ZR_NULL || liveAfter == ZR_NULL || info->writeCount != 1) {
        return ZR_FALSE;
    }

    destSlot = info->writeSlots[0];
    if (destSlot >= slotCount || liveAfter[destSlot]) {
        return ZR_FALSE;
    }
    ZR_UNUSED_PARAMETER(localSlots);

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrSize optimizer_find_previous_kept_instruction(const TZrUInt8 *keep,
                                                        TZrSize blockStart,
                                                        TZrSize instructionIndex) {
    TZrSize scan = instructionIndex;

    if (keep == ZR_NULL || instructionIndex == 0) {
        return ZR_OPTIMIZER_INDEX_NONE;
    }

    while (scan > blockStart) {
        scan--;
        if (keep[scan]) {
            return scan;
        }
    }

    return ZR_OPTIMIZER_INDEX_NONE;
}

static TZrBool optimizer_rewrite_instruction_read_slot(TZrInstruction *instruction,
                                                       const SZrOptimizerInstructionInfo *info,
                                                       TZrUInt16 fromSlot,
                                                       TZrUInt16 toSlot) {
    EZrInstructionCode opcode;
    TZrBool changed = ZR_FALSE;

    if (instruction == ZR_NULL || info == ZR_NULL || fromSlot == toSlot) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            if ((TZrUInt16)instruction->instruction.operand.operand2[0] == fromSlot) {
                instruction->instruction.operand.operand2[0] = (TZrInt32)toSlot;
                changed = ZR_TRUE;
            }
            return changed;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            if (instruction->instruction.operandExtra == fromSlot) {
                instruction->instruction.operandExtra = toSlot;
                changed = ZR_TRUE;
            }
            return changed;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            if (instruction->instruction.operand.operand1[0] == fromSlot) {
                instruction->instruction.operand.operand1[0] = toSlot;
                changed = ZR_TRUE;
            }
            return changed;
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            if (instruction->instruction.operandExtra == 1 &&
                instruction->instruction.operand.operand1[0] == fromSlot) {
                instruction->instruction.operand.operand1[0] = toSlot;
                changed = ZR_TRUE;
            }
            return changed;
        default:
            break;
    }

    if (instruction->instruction.operand.operand1[0] == fromSlot) {
        instruction->instruction.operand.operand1[0] = toSlot;
        changed = ZR_TRUE;
    }
    if (info->operand1Index1IsSlot &&
        !info->hasRelativeJump &&
        instruction->instruction.operand.operand1[1] == fromSlot) {
        instruction->instruction.operand.operand1[1] = toSlot;
        changed = ZR_TRUE;
    }

    return changed;
}

static TZrBool optimizer_info_writes_slot(const SZrOptimizerInstructionInfo *info, TZrUInt16 slot) {
    TZrSize index;

    if (info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < info->writeCount; index++) {
        if (info->writeSlots[index] == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool optimizer_opcode_supports_adjacent_get_stack_forwarding(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool optimizer_adjacent_forward_rewrite_preserves_operand_distinctness(
        const SZrOptimizerInstructionInfo *info,
        TZrUInt16 tempSlot,
        TZrUInt16 sourceSlot) {
    TZrSize index;

    if (info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < info->readCount; index++) {
        if (info->readSlots[index] == sourceSlot && info->readSlots[index] != tempSlot) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < info->writeCount; index++) {
        if (info->writeSlots[index] == sourceSlot && info->writeSlots[index] != tempSlot) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool optimizer_try_forward_adjacent_get_stack_reads(TZrInstruction *instructions,
                                                              const SZrCompilerState *cs,
                                                              const SZrOptimizerBlock *block,
                                                              TZrUInt8 *keep,
                                                              const TZrUInt8 *liveAfter,
                                                              TZrSize slotCount,
                                                              const TZrUInt8 *localSlots,
                                                              TZrSize instructionIndex,
                                                              SZrOptimizerInstructionInfo *info) {
    TZrBool changed = ZR_FALSE;

    if (instructions == ZR_NULL || block == ZR_NULL || keep == ZR_NULL || liveAfter == ZR_NULL ||
        localSlots == ZR_NULL || info == ZR_NULL || !keep[instructionIndex] ||
        info->readCount == 0 || info->hasRangeRead || info->readsAllSlots) {
        return ZR_FALSE;
    }

    if (!optimizer_opcode_supports_adjacent_get_stack_forwarding(
                (EZrInstructionCode)instructions[instructionIndex].instruction.operationCode)) {
        return ZR_FALSE;
    }

    for (;;) {
        TZrSize writerIndex = optimizer_find_previous_kept_instruction(keep, block->start, instructionIndex);
        TZrInstruction *writer;
        EZrInstructionCode writerOpcode;
        TZrUInt16 tempSlot;
        TZrUInt16 sourceSlot;

        if (writerIndex == ZR_OPTIMIZER_INDEX_NONE) {
            break;
        }

        writer = &instructions[writerIndex];
        writerOpcode = (EZrInstructionCode)writer->instruction.operationCode;
        if (writerOpcode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
            break;
        }

        tempSlot = writer->instruction.operandExtra;
        if (tempSlot >= slotCount ||
            optimizer_slot_is_active_local_at_instruction(cs, tempSlot, writerIndex) ||
            (liveAfter[tempSlot] && !optimizer_info_writes_slot(info, tempSlot))) {
            break;
        }

        sourceSlot = (TZrUInt16)writer->instruction.operand.operand2[0];
        if (!optimizer_adjacent_forward_rewrite_preserves_operand_distinctness(info, tempSlot, sourceSlot)) {
            break;
        }
        if (!optimizer_rewrite_instruction_read_slot(&instructions[instructionIndex], info, tempSlot, sourceSlot)) {
            break;
        }

        keep[writerIndex] = 0;
        changed = ZR_TRUE;
        optimizer_classify_instruction(&instructions[instructionIndex], info);
        if (info->readCount == 0 || info->hasRangeRead || info->readsAllSlots) {
            break;
        }
    }

    return changed;
}

static void optimizer_remap_slot_value(TZrUInt16 *slot, const TZrUInt16 *slotMap, TZrSize slotCount) {
    if (slot == ZR_NULL || slotMap == ZR_NULL) {
        return;
    }

    if (*slot < slotCount) {
        *slot = slotMap[*slot];
    }
}

static void optimizer_remap_instruction_slots(TZrInstruction *instruction,
                                              const SZrOptimizerInstructionInfo *info,
                                              const TZrUInt16 *slotMap,
                                              TZrSize slotCount) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL || info == ZR_NULL || slotMap == ZR_NULL) {
        return;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            if ((TZrSize)instruction->instruction.operand.operand2[0] < slotCount) {
                instruction->instruction.operand.operand2[0] =
                        (TZrInt32)slotMap[(TZrSize)instruction->instruction.operand.operand2[0]];
            }
            return;
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            if ((TZrSize)instruction->instruction.operand.operand2[0] < slotCount) {
                instruction->instruction.operand.operand2[0] =
                        (TZrInt32)slotMap[(TZrSize)instruction->instruction.operand.operand2[0]];
            }
            return;
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            return;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            optimizer_remap_slot_value(&instruction->instruction.operand.operand1[0], slotMap, slotCount);
            return;
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            optimizer_remap_slot_value(&instruction->instruction.operand.operand1[0], slotMap, slotCount);
            return;
        case ZR_INSTRUCTION_ENUM(JUMP):
            return;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            return;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
            optimizer_remap_slot_value(&instruction->instruction.operand.operand1[0], slotMap, slotCount);
            return;
        default:
            break;
    }

    optimizer_remap_slot_value(&instruction->instruction.operandExtra, slotMap, slotCount);
    if (instruction->instruction.operand.operand1[0] < slotCount) {
        instruction->instruction.operand.operand1[0] = slotMap[instruction->instruction.operand.operand1[0]];
    }
    if (info->operand1Index1IsSlot && !info->hasRelativeJump &&
        instruction->instruction.operand.operand1[1] < slotCount) {
        instruction->instruction.operand.operand1[1] = slotMap[instruction->instruction.operand.operand1[1]];
    }
}

static TZrUInt32 optimizer_compute_max_stack_slot(const TZrInstruction *instructions,
                                                  TZrSize instructionCount,
                                                  TZrSize localFloor,
                                                  TZrSize oldMaxStackSlotCount) {
    TZrUInt32 maxSlot = (TZrUInt32)localFloor;
    TZrSize instructionIndex;

    for (instructionIndex = 0; instructionIndex < instructionCount; instructionIndex++) {
        SZrOptimizerInstructionInfo info;
        TZrSize readIndex;

        optimizer_classify_instruction(&instructions[instructionIndex], &info);
        if (info.readsAllSlots) {
            return (TZrUInt32)oldMaxStackSlotCount;
        }

        if (info.hasRangeRead && info.rangeReadCount > 0) {
            TZrUInt32 rangeEnd = (TZrUInt32)info.rangeReadStart + info.rangeReadCount;
            if (rangeEnd > maxSlot) {
                maxSlot = rangeEnd;
            }
        }

        for (readIndex = 0; readIndex < info.readCount; readIndex++) {
            TZrUInt32 nextSlot = (TZrUInt32)info.readSlots[readIndex] + 1;
            if (nextSlot > maxSlot) {
                maxSlot = nextSlot;
            }
        }

        for (readIndex = 0; readIndex < info.writeCount; readIndex++) {
            TZrUInt32 nextSlot = (TZrUInt32)info.writeSlots[readIndex] + 1;
            if (nextSlot > maxSlot) {
                maxSlot = nextSlot;
            }
        }
    }

    return maxSlot;
}

static TZrBool optimizer_can_dense_compact_slots(const TZrInstruction *instructions, TZrSize instructionCount) {
    TZrSize instructionIndex;

    if (instructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0; instructionIndex < instructionCount; instructionIndex++) {
        SZrOptimizerInstructionInfo info;
        optimizer_classify_instruction(&instructions[instructionIndex], &info);
        if (info.hasRangeRead || info.readsAllSlots) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void optimizer_remap_compiler_metadata_slots(SZrCompilerState *cs,
                                                    const TZrUInt16 *slotMap,
                                                    TZrSize slotCount) {
    TZrSize index;

    if (cs == ZR_NULL || slotMap == ZR_NULL) {
        return;
    }

    for (index = 0; index < cs->localVars.length; index++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, index);
        if (localVar != ZR_NULL && localVar->stackSlot < slotCount) {
            localVar->stackSlot = slotMap[localVar->stackSlot];
        }
    }

    if (cs->currentFunction != ZR_NULL && cs->currentFunction->typedLocalBindings != ZR_NULL) {
        for (index = 0; index < cs->currentFunction->typedLocalBindingLength; index++) {
            SZrFunctionTypedLocalBinding *binding = &cs->currentFunction->typedLocalBindings[index];
            if (binding->stackSlot < slotCount) {
                binding->stackSlot = slotMap[binding->stackSlot];
            }
        }
    }

    for (index = 0; index < cs->pubVariables.length; index++) {
        SZrExportedVariable *exported = (SZrExportedVariable *)ZrCore_Array_Get(&cs->pubVariables, index);
        if (exported != ZR_NULL && exported->stackSlot < slotCount) {
            exported->stackSlot = slotMap[exported->stackSlot];
        }
    }

    for (index = 0; index < cs->proVariables.length; index++) {
        SZrExportedVariable *exported = (SZrExportedVariable *)ZrCore_Array_Get(&cs->proVariables, index);
        if (exported != ZR_NULL && exported->stackSlot < slotCount) {
            exported->stackSlot = slotMap[exported->stackSlot];
        }
    }

    for (index = 0; index < cs->childFunctions.length; index++) {
        SZrFunction **childPtr = (SZrFunction **)ZrCore_Array_Get(&cs->childFunctions, index);
        SZrFunction *childFunction;
        TZrUInt32 closureIndex;

        if (childPtr == ZR_NULL || *childPtr == ZR_NULL) {
            continue;
        }

        childFunction = *childPtr;
        if (childFunction->closureValueList == ZR_NULL || childFunction->closureValueLength == 0) {
            continue;
        }

        for (closureIndex = 0; closureIndex < childFunction->closureValueLength; closureIndex++) {
            SZrFunctionClosureVariable *closureVar = &childFunction->closureValueList[closureIndex];
            if (closureVar->inStack && closureVar->index < slotCount) {
                closureVar->index = slotMap[closureVar->index];
            }
        }
    }
}

static void optimizer_dense_compact_slots(SZrCompilerState *cs,
                                          TZrInstruction *instructions,
                                          TZrSize instructionCount,
                                          TZrSize slotCount) {
    TZrUInt8 *isLocalSlot = ZR_NULL;
    TZrUInt16 *slotMap = ZR_NULL;
    TZrSize nextSlot = 0;
    TZrSize slot;
    TZrSize instructionIndex;
    TZrBool changed = ZR_FALSE;

    if (cs == ZR_NULL || instructions == ZR_NULL || slotCount == 0 ||
        !optimizer_can_dense_compact_slots(instructions, instructionCount)) {
        return;
    }

    isLocalSlot = (TZrUInt8 *)calloc(slotCount, sizeof(TZrUInt8));
    slotMap = (TZrUInt16 *)malloc(sizeof(TZrUInt16) * slotCount);
    if (isLocalSlot == ZR_NULL || slotMap == ZR_NULL) {
        free(isLocalSlot);
        free(slotMap);
        return;
    }

    for (slot = 0; slot < slotCount; slot++) {
        slotMap[slot] = (TZrUInt16)slot;
    }

    for (slot = 0; slot < cs->localVars.length; slot++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, slot);
        if (localVar != ZR_NULL && localVar->stackSlot < slotCount) {
            isLocalSlot[localVar->stackSlot] = 1;
        }
    }

    for (slot = 0; slot < cs->localVars.length; slot++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, slot);
        if (localVar == ZR_NULL || localVar->stackSlot >= slotCount) {
            continue;
        }
        slotMap[localVar->stackSlot] = (TZrUInt16)nextSlot++;
    }

    for (slot = 0; slot < slotCount; slot++) {
        if (isLocalSlot[slot]) {
            continue;
        }
        slotMap[slot] = (TZrUInt16)nextSlot++;
    }

    for (slot = 0; slot < slotCount; slot++) {
        if (slotMap[slot] != slot) {
            changed = ZR_TRUE;
            break;
        }
    }

    if (!changed) {
        free(isLocalSlot);
        free(slotMap);
        return;
    }

    optimizer_remap_compiler_metadata_slots(cs, slotMap, slotCount);
    for (instructionIndex = 0; instructionIndex < instructionCount; instructionIndex++) {
        SZrOptimizerInstructionInfo info;
        optimizer_classify_instruction(&instructions[instructionIndex], &info);
        optimizer_remap_instruction_slots(&instructions[instructionIndex], &info, slotMap, slotCount);
    }

    free(isLocalSlot);
    free(slotMap);
}

static void optimizer_build_block_intervals(const TZrInstruction *instructions,
                                            const SZrOptimizerBlock *block,
                                            const TZrUInt8 *liveIn,
                                            const TZrUInt8 *liveOut,
                                            TZrSize slotCount,
                                            const TZrUInt8 *localSlots,
                                            TZrUInt16 *slotMap) {
    TZrSize *firstDef = ZR_NULL;
    TZrSize *lastDef = ZR_NULL;
    TZrSize *lastUse = ZR_NULL;
    TZrSize *defCount = ZR_NULL;
    TZrUInt8 *physicalInUse = ZR_NULL;
    SZrOptimizerInterval *intervals = ZR_NULL;
    TZrSize intervalCount = 0;
    TZrSize slot;
    TZrSize instructionIndex;

    if (instructions == ZR_NULL || block == ZR_NULL || liveIn == ZR_NULL || liveOut == ZR_NULL ||
        slotMap == ZR_NULL || !block->allowSlotReuse || slotCount == 0) {
        return;
    }

    firstDef = (TZrSize *)malloc(sizeof(TZrSize) * slotCount);
    lastDef = (TZrSize *)malloc(sizeof(TZrSize) * slotCount);
    lastUse = (TZrSize *)malloc(sizeof(TZrSize) * slotCount);
    defCount = (TZrSize *)calloc(slotCount, sizeof(TZrSize));
    physicalInUse = (TZrUInt8 *)calloc(slotCount, sizeof(TZrUInt8));
    intervals = (SZrOptimizerInterval *)calloc(slotCount, sizeof(SZrOptimizerInterval));
    if (firstDef == ZR_NULL || lastDef == ZR_NULL || lastUse == ZR_NULL || defCount == ZR_NULL ||
        physicalInUse == ZR_NULL || intervals == ZR_NULL) {
        free(firstDef);
        free(lastDef);
        free(lastUse);
        free(defCount);
        free(physicalInUse);
        free(intervals);
        return;
    }

    for (slot = 0; slot < slotCount; slot++) {
        firstDef[slot] = ZR_OPTIMIZER_INDEX_NONE;
        lastDef[slot] = ZR_OPTIMIZER_INDEX_NONE;
        lastUse[slot] = ZR_OPTIMIZER_INDEX_NONE;
        slotMap[slot] = (TZrUInt16)slot;
    }

    for (instructionIndex = block->start; instructionIndex < block->end; instructionIndex++) {
        SZrOptimizerInstructionInfo info;
        TZrSize index;

        optimizer_classify_instruction(&instructions[instructionIndex], &info);
        if (!info.allowSlotReuse || info.readsAllSlots || info.hasRangeRead) {
            free(firstDef);
            free(lastDef);
            free(lastUse);
            free(defCount);
            free(physicalInUse);
            free(intervals);
            return;
        }

        for (index = 0; index < info.readCount; index++) {
            slot = info.readSlots[index];
            if (slot < slotCount && !optimizer_slot_is_local(localSlots, slotCount, (TZrUInt16)slot)) {
                lastUse[slot] = instructionIndex;
            }
        }

        for (index = 0; index < info.writeCount; index++) {
            slot = info.writeSlots[index];
            if (slot >= slotCount || optimizer_slot_is_local(localSlots, slotCount, (TZrUInt16)slot)) {
                continue;
            }

            if (firstDef[slot] == ZR_OPTIMIZER_INDEX_NONE) {
                firstDef[slot] = instructionIndex;
            }
            lastDef[slot] = instructionIndex;
            defCount[slot]++;
        }
    }

    for (slot = 0; slot < slotCount; slot++) {
        TZrBool slotLiveIn = liveIn[slot] != 0;
        TZrBool slotLiveOut = liveOut[slot] != 0;
        TZrBool hasDefinition = firstDef[slot] != ZR_OPTIMIZER_INDEX_NONE;
        TZrBool hasUsage = lastUse[slot] != ZR_OPTIMIZER_INDEX_NONE;
        TZrSize intervalStart;
        TZrSize intervalEnd;

        if (optimizer_slot_is_local(localSlots, slotCount, (TZrUInt16)slot)) {
            continue;
        }

        if (!slotLiveIn && !slotLiveOut && !hasDefinition && !hasUsage) {
            continue;
        }

        intervalStart = slotLiveIn ? block->start : firstDef[slot];
        if (intervalStart == ZR_OPTIMIZER_INDEX_NONE) {
            intervalStart = block->start;
        }

        intervalEnd = lastUse[slot];
        if (intervalEnd == ZR_OPTIMIZER_INDEX_NONE || (hasDefinition && lastDef[slot] > intervalEnd)) {
            intervalEnd = lastDef[slot];
        }
        if (intervalEnd == ZR_OPTIMIZER_INDEX_NONE) {
            intervalEnd = block->start;
        }
        if (slotLiveOut && block->end > 0) {
            intervalEnd = block->end - 1;
        }

        intervals[intervalCount].originalSlot = (TZrUInt16)slot;
        intervals[intervalCount].assignedSlot = (TZrUInt16)slot;
        intervals[intervalCount].start = intervalStart;
        intervals[intervalCount].end = intervalEnd;
        intervals[intervalCount].fixed = slotLiveIn || slotLiveOut || defCount[slot] != 1;
        intervals[intervalCount].valid = ZR_TRUE;
        intervalCount++;
    }

    for (slot = 0; slot < intervalCount; slot++) {
        TZrSize other;
        for (other = slot + 1; other < intervalCount; other++) {
            TZrBool shouldSwap = ZR_FALSE;
            if (intervals[other].start < intervals[slot].start) {
                shouldSwap = ZR_TRUE;
            } else if (intervals[other].start == intervals[slot].start &&
                       intervals[other].fixed && !intervals[slot].fixed) {
                shouldSwap = ZR_TRUE;
            } else if (intervals[other].start == intervals[slot].start &&
                       intervals[other].fixed == intervals[slot].fixed &&
                       intervals[other].end < intervals[slot].end) {
                shouldSwap = ZR_TRUE;
            }

            if (shouldSwap) {
                SZrOptimizerInterval temp = intervals[slot];
                intervals[slot] = intervals[other];
                intervals[other] = temp;
            }
        }
    }

    for (slot = 0; slot < intervalCount; slot++) {
        TZrSize activeIndex;
        memset(physicalInUse, 0, slotCount);

        for (activeIndex = 0; activeIndex < slot; activeIndex++) {
            if (intervals[activeIndex].valid &&
                intervals[activeIndex].start <= intervals[slot].start &&
                intervals[activeIndex].end > intervals[slot].start &&
                intervals[activeIndex].assignedSlot < slotCount) {
                physicalInUse[intervals[activeIndex].assignedSlot] = 1;
            }
        }

        if (intervals[slot].fixed) {
            intervals[slot].assignedSlot = intervals[slot].originalSlot;
            continue;
        }

        for (activeIndex = 0; activeIndex < slotCount; activeIndex++) {
            if (optimizer_slot_is_local(localSlots, slotCount, (TZrUInt16)activeIndex)) {
                continue;
            }
            if (!physicalInUse[activeIndex]) {
                intervals[slot].assignedSlot = (TZrUInt16)activeIndex;
                break;
            }
        }
    }

    for (slot = 0; slot < intervalCount; slot++) {
        slotMap[intervals[slot].originalSlot] = intervals[slot].assignedSlot;
    }

    free(firstDef);
    free(lastDef);
    free(lastUse);
    free(defCount);
    free(physicalInUse);
    free(intervals);
}

void optimize_instructions(SZrCompilerState *cs) {
    TZrInstruction *instructions;
    TZrSize instructionCount;
    TZrSize executionStart;
    TZrSize slotCount;
    TZrSize localFloor;
    TZrUInt8 *leaders = ZR_NULL;
    TZrSize *boundaryToBlock = ZR_NULL;
    SZrOptimizerBlock *blocks = ZR_NULL;
    TZrUInt8 *useSets = ZR_NULL;
    TZrUInt8 *defSets = ZR_NULL;
    TZrUInt8 *liveInSets = ZR_NULL;
    TZrUInt8 *liveOutSets = ZR_NULL;
    TZrUInt8 *localSlots = ZR_NULL;
    TZrUInt8 *keep = ZR_NULL;
    TZrUInt16 *slotMap = ZR_NULL;
    TZrSize *newBoundaryIndex = ZR_NULL;
    TZrSize blockCount = 0;
    TZrSize blockIndex;
    TZrBool changed = ZR_FALSE;
    TZrBool dataflowChanged = ZR_TRUE;

    if (cs == ZR_NULL || cs->hasError || cs->instructions.length == 0 || cs->instructions.head == ZR_NULL) {
        return;
    }

    instructions = (TZrInstruction *)cs->instructions.head;
    instructionCount = cs->instructions.length;
    if (cs->executionLocations.length < instructionCount) {
        return;
    }

    if (optimizer_function_contains_exception_control(instructions, instructionCount)) {
        return;
    }

    optimizer_dense_compact_slots(cs, instructions, instructionCount, cs->maxStackSlotCount);

    executionStart = cs->executionLocations.length - instructionCount;
    slotCount = cs->maxStackSlotCount;
    localFloor = ZrParser_Compiler_GetLocalStackFloor(cs);
    if (slotCount == 0) {
        return;
    }

    leaders = (TZrUInt8 *)calloc(instructionCount + 1, sizeof(TZrUInt8));
    boundaryToBlock = (TZrSize *)malloc(sizeof(TZrSize) * (instructionCount + 1));
    blocks = (SZrOptimizerBlock *)calloc(instructionCount, sizeof(SZrOptimizerBlock));
    useSets = (TZrUInt8 *)calloc(instructionCount * slotCount, sizeof(TZrUInt8));
    defSets = (TZrUInt8 *)calloc(instructionCount * slotCount, sizeof(TZrUInt8));
    liveInSets = (TZrUInt8 *)calloc(instructionCount * slotCount, sizeof(TZrUInt8));
    liveOutSets = (TZrUInt8 *)calloc(instructionCount * slotCount, sizeof(TZrUInt8));
    localSlots = (TZrUInt8 *)calloc(slotCount, sizeof(TZrUInt8));
    keep = (TZrUInt8 *)malloc(sizeof(TZrUInt8) * instructionCount);
    slotMap = (TZrUInt16 *)malloc(sizeof(TZrUInt16) * slotCount);
    newBoundaryIndex = (TZrSize *)malloc(sizeof(TZrSize) * (instructionCount + 1));
    if (leaders == ZR_NULL || boundaryToBlock == ZR_NULL || blocks == ZR_NULL || useSets == ZR_NULL ||
        defSets == ZR_NULL || liveInSets == ZR_NULL || liveOutSets == ZR_NULL || localSlots == ZR_NULL ||
        keep == ZR_NULL ||
        slotMap == ZR_NULL || newBoundaryIndex == ZR_NULL) {
        goto cleanup;
    }

    memset(boundaryToBlock, 0xFF, sizeof(TZrSize) * (instructionCount + 1));
    memset(keep, 1, sizeof(TZrUInt8) * instructionCount);
    optimizer_mark_local_slots(cs, localSlots, slotCount);

    leaders[0] = 1;
    for (blockIndex = 0; blockIndex < instructionCount; blockIndex++) {
        SZrOptimizerInstructionInfo info;

        optimizer_classify_instruction(&instructions[blockIndex], &info);
        if (info.hasRelativeJump) {
            TZrInt32 target = (TZrInt32)blockIndex + 1 + optimizer_relative_jump_offset(&instructions[blockIndex]);
            if (target >= 0 && (TZrSize)target <= instructionCount) {
                leaders[target] = 1;
            }
            if ((info.conditionalJump || !info.terminator) && blockIndex + 1 < instructionCount) {
                leaders[blockIndex + 1] = 1;
            }
        } else if (info.terminator && blockIndex + 1 < instructionCount) {
            leaders[blockIndex + 1] = 1;
        }
    }

    for (blockIndex = 0; blockIndex < instructionCount; blockIndex++) {
        if (!leaders[blockIndex]) {
            continue;
        }

        blocks[blockCount].start = blockIndex;
        blocks[blockCount].end = blockIndex + 1;
        blocks[blockCount].allowSlotReuse = ZR_TRUE;
        boundaryToBlock[blockIndex] = blockCount;
        blockCount++;
    }

    if (blockCount == 0) {
        goto cleanup;
    }

    for (blockIndex = 0; blockIndex < blockCount; blockIndex++) {
        TZrSize nextIndex = (blockIndex + 1 < blockCount) ? blocks[blockIndex + 1].start : instructionCount;
        TZrSize instructionIndex;
        SZrOptimizerInstructionInfo tailInfo;

        blocks[blockIndex].end = nextIndex;
        for (instructionIndex = blocks[blockIndex].start; instructionIndex < blocks[blockIndex].end; instructionIndex++) {
            SZrOptimizerInstructionInfo info;
            optimizer_classify_instruction(&instructions[instructionIndex], &info);
            optimizer_mark_use_set(useSets + (blockIndex * slotCount),
                                   defSets + (blockIndex * slotCount),
                                   slotCount,
                                   &info);
            if (!info.allowSlotReuse || info.hasRangeRead || info.readsAllSlots) {
                blocks[blockIndex].allowSlotReuse = ZR_FALSE;
            }
        }

        optimizer_classify_instruction(&instructions[blocks[blockIndex].end - 1], &tailInfo);
        if (tailInfo.hasRelativeJump) {
            TZrInt32 target =
                    (TZrInt32)(blocks[blockIndex].end - 1) + 1 +
                    optimizer_relative_jump_offset(&instructions[blocks[blockIndex].end - 1]);
            if (target >= 0 && (TZrSize)target < instructionCount && boundaryToBlock[target] != ZR_OPTIMIZER_INDEX_NONE) {
                blocks[blockIndex].successors[blocks[blockIndex].successorCount++] = boundaryToBlock[target];
            }
            if (tailInfo.conditionalJump && blockIndex + 1 < blockCount) {
                blocks[blockIndex].successors[blocks[blockIndex].successorCount++] = blockIndex + 1;
            }
        } else if (!tailInfo.terminator && blockIndex + 1 < blockCount) {
            blocks[blockIndex].successors[blocks[blockIndex].successorCount++] = blockIndex + 1;
        }
    }

    while (dataflowChanged) {
        dataflowChanged = ZR_FALSE;
        for (blockIndex = blockCount; blockIndex > 0; blockIndex--) {
            TZrSize currentBlockIndex = blockIndex - 1;
            TZrUInt8 *blockUse = useSets + (currentBlockIndex * slotCount);
            TZrUInt8 *blockDef = defSets + (currentBlockIndex * slotCount);
            TZrUInt8 *blockLiveIn = liveInSets + (currentBlockIndex * slotCount);
            TZrUInt8 *blockLiveOut = liveOutSets + (currentBlockIndex * slotCount);
            TZrUInt8 *previousLiveIn = (TZrUInt8 *)malloc(slotCount);
            TZrUInt8 *previousLiveOut = (TZrUInt8 *)malloc(slotCount);
            TZrSize slotIndex;
            TZrSize successorIndex;

            if (previousLiveIn == ZR_NULL || previousLiveOut == ZR_NULL) {
                free(previousLiveIn);
                free(previousLiveOut);
                goto cleanup;
            }

            memcpy(previousLiveIn, blockLiveIn, slotCount);
            memcpy(previousLiveOut, blockLiveOut, slotCount);
            memset(blockLiveOut, 0, slotCount);

            for (successorIndex = 0; successorIndex < blocks[currentBlockIndex].successorCount; successorIndex++) {
                TZrUInt8 *successorLiveIn =
                        liveInSets + (blocks[currentBlockIndex].successors[successorIndex] * slotCount);
                for (slotIndex = 0; slotIndex < slotCount; slotIndex++) {
                    if (successorLiveIn[slotIndex]) {
                        blockLiveOut[slotIndex] = 1;
                    }
                }
            }

            for (slotIndex = 0; slotIndex < slotCount; slotIndex++) {
                blockLiveIn[slotIndex] =
                        (TZrUInt8)(blockUse[slotIndex] || (blockLiveOut[slotIndex] && !blockDef[slotIndex]));
            }

            if (memcmp(previousLiveIn, blockLiveIn, slotCount) != 0 ||
                memcmp(previousLiveOut, blockLiveOut, slotCount) != 0) {
                dataflowChanged = ZR_TRUE;
            }

            free(previousLiveIn);
            free(previousLiveOut);
        }
    }

    for (blockIndex = 0; blockIndex < blockCount; blockIndex++) {
        TZrUInt8 *live = (TZrUInt8 *)malloc(slotCount);
        TZrSize instructionIndex;
        TZrSize slotIndex;

        if (live == ZR_NULL) {
            goto cleanup;
        }

        memcpy(live, liveOutSets + (blockIndex * slotCount), slotCount);
        for (instructionIndex = blocks[blockIndex].end; instructionIndex > blocks[blockIndex].start; instructionIndex--) {
            TZrSize currentInstructionIndex = instructionIndex - 1;
            SZrOptimizerInstructionInfo info;
            TZrBool removeCurrent = ZR_FALSE;

            if (!keep[currentInstructionIndex]) {
                continue;
            }

            optimizer_classify_instruction(&instructions[currentInstructionIndex], &info);
            if (optimizer_try_rewrite_dead_result_to_ret(&instructions[currentInstructionIndex],
                                                         live,
                                                         slotCount,
                                                         localSlots)) {
                changed = ZR_TRUE;
                optimizer_classify_instruction(&instructions[currentInstructionIndex], &info);
            }
            if (optimizer_try_forward_adjacent_get_stack_reads(instructions,
                                                               cs,
                                                               &blocks[blockIndex],
                                                               keep,
                                                               live,
                                                               slotCount,
                                                               localSlots,
                                                               currentInstructionIndex,
                                                               &info)) {
                changed = ZR_TRUE;
                optimizer_classify_instruction(&instructions[currentInstructionIndex], &info);
            }
            if (optimizer_is_dead_null_clear(cs,
                                             &instructions[currentInstructionIndex],
                                             live,
                                             slotCount,
                                             localSlots) ||
                optimizer_is_dead_pure_write(&instructions[currentInstructionIndex],
                                             &info,
                                             live,
                                             slotCount,
                                             localSlots)) {
                keep[currentInstructionIndex] = 0;
                changed = ZR_TRUE;
                removeCurrent = ZR_TRUE;
            }

            if (removeCurrent) {
                continue;
            }

            optimizer_live_remove_writes(live, slotCount, &info);
            optimizer_live_add_reads(live, slotCount, &info);
        }
        free(live);

        for (slotIndex = 0; slotIndex < slotCount; slotIndex++) {
            slotMap[slotIndex] = (TZrUInt16)slotIndex;
        }
        optimizer_build_block_intervals(instructions,
                                        &blocks[blockIndex],
                                        liveInSets + (blockIndex * slotCount),
                                        liveOutSets + (blockIndex * slotCount),
                                        slotCount,
                                        localSlots,
                                        slotMap);

        for (instructionIndex = blocks[blockIndex].start; instructionIndex < blocks[blockIndex].end; instructionIndex++) {
            SZrOptimizerInstructionInfo info;

            if (!keep[instructionIndex]) {
                continue;
            }

            optimizer_classify_instruction(&instructions[instructionIndex], &info);
            optimizer_remap_instruction_slots(&instructions[instructionIndex], &info, slotMap, slotCount);
        }
    }

    newBoundaryIndex[0] = 0;
    for (blockIndex = 0; blockIndex < instructionCount; blockIndex++) {
        newBoundaryIndex[blockIndex + 1] = newBoundaryIndex[blockIndex] + (keep[blockIndex] ? 1 : 0);
    }
    optimizer_remap_local_variable_instruction_offsets(cs, newBoundaryIndex, instructionCount);

    for (blockIndex = 0; blockIndex < instructionCount; blockIndex++) {
        if (!keep[blockIndex]) {
            continue;
        }

        {
            SZrOptimizerInstructionInfo info;
            optimizer_classify_instruction(&instructions[blockIndex], &info);
            if (info.hasRelativeJump) {
                TZrInt32 oldTarget = (TZrInt32)blockIndex + 1 + optimizer_relative_jump_offset(&instructions[blockIndex]);
                if (oldTarget >= 0 && (TZrSize)oldTarget <= instructionCount) {
                    TZrInt32 newOffset =
                            (TZrInt32)newBoundaryIndex[oldTarget] - (TZrInt32)newBoundaryIndex[blockIndex] - 1;
                    optimizer_store_relative_jump_offset(&instructions[blockIndex], newOffset);
                }
            }
        }
    }

    if (changed) {
        TZrSize writeIndex = 0;
        TZrSize executionWriteIndex = executionStart;

        for (blockIndex = 0; blockIndex < instructionCount; blockIndex++) {
            if (!keep[blockIndex]) {
                continue;
            }

            if (writeIndex != blockIndex) {
                instructions[writeIndex] = instructions[blockIndex];
            }

            if (executionWriteIndex != executionStart + blockIndex) {
                SZrFunctionExecutionLocationInfo *executionLocations =
                        (SZrFunctionExecutionLocationInfo *)cs->executionLocations.head;
                executionLocations[executionWriteIndex] = executionLocations[executionStart + blockIndex];
            }

            ((SZrFunctionExecutionLocationInfo *)cs->executionLocations.head)[executionWriteIndex].currentInstructionOffset =
                    (TZrMemoryOffset)writeIndex;
            executionWriteIndex++;
            writeIndex++;
        }

        cs->instructions.length = writeIndex;
        cs->instructionCount = writeIndex;
        cs->executionLocations.length = executionStart + writeIndex;
    }

    cs->maxStackSlotCount =
            optimizer_compute_max_stack_slot((TZrInstruction *)cs->instructions.head,
                                             cs->instructions.length,
                                             localFloor,
                                             cs->maxStackSlotCount);

cleanup:
    free(leaders);
    free(boundaryToBlock);
    free(blocks);
    free(useSets);
    free(defSets);
    free(liveInSets);
    free(liveOutSets);
    free(localSlots);
    free(keep);
    free(slotMap);
    free(newBoundaryIndex);
}
