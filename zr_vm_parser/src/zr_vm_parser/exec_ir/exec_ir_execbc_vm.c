#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "exec_ir_execbc_vm_internal.h"

static TZrUInt32 execbc_vm_count_edge_moves(
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId edge) {
    TZrUInt32 count = 0u;
    TZrUInt32 index;
    for (index = 0u; index < projection->phiMoveCount; ++index) {
        if (projection->phiMoves[index].edge == edge) ++count;
    }
    return count;
}

static TZrExecIrBlockId execbc_vm_ordered_block_id(
        const SZrExecBcProjection *projection,
        TZrUInt32 orderIndex) {
    if (orderIndex == 0u) return projection->entryBlockId;
    return orderIndex < projection->entryBlockId
            ? orderIndex
            : orderIndex + 1u;
}

static TZrBool execbc_vm_count_block_instructions(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block,
        TZrUInt64 *outCount) {
    TZrUInt64 count = 0u;
    TZrUInt32 localIndex;
    TZrBool synthetic = execbc_vm_is_synthetic_block(projection, block);
    if (synthetic != ZR_FALSE) {
        count = (TZrUInt64)execbc_vm_count_edge_moves(projection, block->id) + 1u;
        *outCount = count;
        return ZR_TRUE;
    }
    for (localIndex = 0u; localIndex < block->instructions.count; ++localIndex) {
        TZrUInt32 instructionIndex = block->instructions.start + localIndex;
        const SZrExecBcInstruction *instruction = &projection->instructions[instructionIndex];
        if (instructionIndex + 1u == block->terminatorInstructionId) continue;
        if (instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_PHI) ++count;
    }
    count += (TZrUInt64)execbc_vm_count_edge_moves(projection, block->id);
    if (block->terminatorInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
        ++count;
    } else {
        const SZrExecBcInstruction *terminator =
                &projection->instructions[block->terminatorInstructionId - 1u];
        if (terminator->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
            count += 2u;
        } else {
            ++count;
        }
    }
    *outCount = count;
    return ZR_TRUE;
}

static TZrInstruction execbc_vm_instruction(EZrInstructionCode opcode,
                                             TZrUInt16 operandExtra) {
    TZrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

static TZrBool execbc_vm_append_plan(
        SZrExecBcVmPlannedInstruction *plan,
        TZrUInt32 capacity,
        TZrUInt32 *count,
        TZrInstruction instruction,
        TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrExecIrBlockId targetBlockId) {
    SZrExecBcVmPlannedInstruction *planned;
    if (plan == ZR_NULL || count == ZR_NULL || *count >= capacity || blockId == 0u) {
        return ZR_FALSE;
    }
    planned = &plan[*count];
    memset(planned, 0, sizeof(*planned));
    planned->instruction = instruction;
    planned->targetBlockId = targetBlockId;
    planned->hasRelativeTarget = (TZrBool)(targetBlockId != 0u);
    planned->pcMap.pc = *count;
    planned->pcMap.instructionId = instructionId;
    planned->pcMap.blockId = blockId;
    planned->pcMap.sourceId = sourceId;
    ++*count;
    return ZR_TRUE;
}

static void execbc_vm_instruction_origin(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block,
        TZrExecIrInstructionId *instructionId,
        TZrExecIrSourceId *sourceId) {
    const SZrExecBcBlock *originBlock = block;
    if (block->terminatorInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
        block->predecessors.count != 0u) {
        TZrExecIrBlockId predecessor = projection->predecessors[block->predecessors.start];
        const SZrExecBcBlock *candidate = execbc_vm_block_at(projection, predecessor);
        if (candidate != ZR_NULL) originBlock = candidate;
    }
    if (originBlock->terminatorInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
        *instructionId = originBlock->terminatorInstructionId;
        *sourceId = projection->instructions[*instructionId - 1u].sourceId;
        return;
    }
    if (originBlock->instructions.count != 0u) {
        *instructionId = originBlock->instructions.start + originBlock->instructions.count;
        *sourceId = projection->instructions[*instructionId - 1u].sourceId;
        return;
    }
    *instructionId = 0u;
    *sourceId = 0u;
}

static TZrBool execbc_vm_append_move_group(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block,
        SZrExecBcVmPlannedInstruction *plan,
        TZrUInt32 capacity,
        TZrUInt32 *count) {
    TZrUInt32 moveIndex;
    TZrExecIrInstructionId originInstruction;
    TZrExecIrSourceId originSource;
    execbc_vm_instruction_origin(projection, block,
                                 &originInstruction, &originSource);
    for (moveIndex = 0u; moveIndex < projection->phiMoveCount; ++moveIndex) {
        const SZrExecBcPhiMove *move = &projection->phiMoves[moveIndex];
        TZrInstruction instruction;
        if (move->edge != block->id) continue;
        instruction = execbc_vm_instruction(
                ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)move->destinationSlot);
        instruction.instruction.operand.operand2[0] = (TZrInt32)move->sourceSlot;
        if (!execbc_vm_append_plan(plan, capacity, count, instruction,
                                   block->id, originInstruction, originSource, 0u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool execbc_vm_build_plan(
        const SZrExecBcProjection *projection,
        SZrExecBcVmPlannedInstruction **outPlan,
        TZrUInt32 **outBlockStarts,
        SZrExecBcVmConstant **outConstants,
        TZrUInt32 *outConstantCount,
        TZrUInt32 *outInstructionCount,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt64 total = 0u;
    TZrUInt32 *blockStarts = ZR_NULL;
    TZrUInt32 *poolMap = ZR_NULL;
    SZrExecBcVmPlannedInstruction *plan = ZR_NULL;
    SZrExecBcVmConstant *constants = ZR_NULL;
    TZrUInt32 constantCapacity = projection->constantCount;
    TZrUInt32 constantCount = 0u;
    TZrUInt32 planCount = 0u;
    TZrUInt32 blockIndex;
    TZrUInt32 constantIndex;

    *outPlan = ZR_NULL;
    *outBlockStarts = ZR_NULL;
    *outConstants = ZR_NULL;
    *outConstantCount = 0u;
    *outInstructionCount = 0u;

    if (projection->constantCount == 0u) {
        for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
            const SZrExecBcBlock *block = &projection->blocks[blockIndex];
            TZrUInt32 localIndex;
            for (localIndex = 0u; localIndex < block->instructions.count; ++localIndex) {
                const SZrExecBcInstruction *instruction =
                        &projection->instructions[block->instructions.start + localIndex];
                if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT) {
                    if (constantCapacity == UINT32_MAX) goto capacity_error;
                    ++constantCapacity;
                }
            }
        }
    }
    if (constantCapacity != 0u) {
        if (constantCapacity > SIZE_MAX / sizeof(*constants)) goto capacity_error;
        constants = (SZrExecBcVmConstant *)calloc(constantCapacity, sizeof(*constants));
        if (constants == ZR_NULL) goto oom;
    }
    if (projection->constantCount != 0u) {
        if (projection->constantCount > SIZE_MAX / sizeof(*poolMap)) goto capacity_error;
        poolMap = (TZrUInt32 *)malloc((size_t)projection->constantCount * sizeof(*poolMap));
        if (poolMap == ZR_NULL) goto oom;
        for (blockIndex = 0u; blockIndex < projection->constantCount; ++blockIndex)
            poolMap[blockIndex] = UINT32_MAX;
    }

    for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
        TZrUInt64 blockCount;
        if (!execbc_vm_count_block_instructions(
                    projection, &projection->blocks[blockIndex], &blockCount) ||
            blockCount > UINT32_MAX - total) goto capacity_error;
        total += blockCount;
    }
    if (total == 0u || total >= UINT32_MAX ||
        total > SIZE_MAX / sizeof(*plan) ||
        (TZrUInt64)projection->blockCount + 1u > SIZE_MAX / sizeof(*blockStarts)) {
        goto capacity_error;
    }
    plan = (SZrExecBcVmPlannedInstruction *)calloc((size_t)total, sizeof(*plan));
    blockStarts = (TZrUInt32 *)calloc((size_t)projection->blockCount + 1u,
                                      sizeof(*blockStarts));
    if (plan == ZR_NULL || blockStarts == ZR_NULL) goto oom;

    {
        TZrUInt64 cursor = 0u;
        for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
            TZrExecIrBlockId blockId =
                    execbc_vm_ordered_block_id(projection, blockIndex);
            const SZrExecBcBlock *block = &projection->blocks[blockId - 1u];
            TZrUInt64 blockCount;
            blockStarts[blockId - 1u] = (TZrUInt32)cursor;
            if (!execbc_vm_count_block_instructions(
                        projection, block, &blockCount) ||
                blockCount > total - cursor) goto invalid_projection;
            cursor += blockCount;
        }
        blockStarts[projection->blockCount] = (TZrUInt32)cursor;
        if (cursor != total) goto invalid_projection;
    }

    for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
        TZrExecIrBlockId blockId =
                execbc_vm_ordered_block_id(projection, blockIndex);
        const SZrExecBcBlock *block = &projection->blocks[blockId - 1u];
        TZrUInt32 localIndex;
        TZrBool synthetic = execbc_vm_is_synthetic_block(projection, block);
        if (synthetic == ZR_FALSE) {
            for (localIndex = 0u; localIndex < block->instructions.count; ++localIndex) {
                TZrUInt32 instructionIndex = block->instructions.start + localIndex;
                const SZrExecBcInstruction *source =
                        &projection->instructions[instructionIndex];
                TZrInstruction instruction;
                TZrUInt32 destinationSlot = 0u;
                TZrUInt32 leftSlot = 0u;
                TZrUInt32 rightSlot = 0u;
                TZrExecIrTypeToken ignoredType;
                TZrExecIrBlockId noTarget = 0u;
                if (instructionIndex + 1u == block->terminatorInstructionId ||
                    source->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_PHI) continue;
                switch ((EZrExecIrOpcode)source->opcode) {
                    case ZR_EXEC_IR_OPCODE_NOP:
                        instruction = execbc_vm_instruction(ZR_INSTRUCTION_ENUM(NOP), 0u);
                        break;
                    case ZR_EXEC_IR_OPCODE_CONSTANT: {
                        TZrUInt32 constantIndex;
                        TZrExecIrValueId destinationId =
                                projection->results[source->results.start];
                        if (!execbc_vm_value_slot(projection, destinationId,
                                                  &destinationSlot, &ignoredType))
                            goto invalid_projection;
                        if (projection->constantCount != 0u) {
                            TZrUInt32 poolIndex = source->layoutId;
                            if (poolMap[poolIndex] == UINT32_MAX) {
                                if (constantCount >= constantCapacity) goto invalid_projection;
                                poolMap[poolIndex] = constantCount;
                                constants[constantCount].typeToken =
                                        projection->constants[poolIndex].typeToken;
                                constants[constantCount].bits =
                                        projection->constants[poolIndex].bits;
                                ++constantCount;
                            }
                            constantIndex = poolMap[poolIndex];
                        } else {
                            constantIndex = constantCount;
                            if (constantIndex >= constantCapacity || constantIndex > INT32_MAX)
                                goto capacity_error;
                            constants[constantIndex].typeToken = source->typeToken;
                            constants[constantIndex].bits = source->layoutId;
                            ++constantCount;
                        }
                        if (constantIndex > INT32_MAX) goto capacity_error;
                        instruction = execbc_vm_instruction(
                                ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                (TZrUInt16)destinationSlot);
                        instruction.instruction.operand.operand2[0] =
                                (TZrInt32)constantIndex;
                        break;
                    }
                    case ZR_EXEC_IR_OPCODE_COPY:
                    case ZR_EXEC_IR_OPCODE_MOVE:
                        if (!execbc_vm_value_slot(
                                projection,
                                projection->results[source->results.start],
                                &destinationSlot, &ignoredType) ||
                            !execbc_vm_value_slot(
                                projection,
                                projection->operands[source->operands.start],
                                &leftSlot, &ignoredType)) goto invalid_projection;
                        instruction = execbc_vm_instruction(
                                ZR_INSTRUCTION_ENUM(SET_STACK),
                                (TZrUInt16)destinationSlot);
                        instruction.instruction.operand.operand2[0] = (TZrInt32)leftSlot;
                        break;
                    case ZR_EXEC_IR_OPCODE_ADD:
                    case ZR_EXEC_IR_OPCODE_SUB:
                        if (!execbc_vm_value_slot(
                                projection,
                                projection->results[source->results.start],
                                &destinationSlot, &ignoredType) ||
                            !execbc_vm_value_slot(
                                projection,
                                projection->operands[source->operands.start],
                                &leftSlot, &ignoredType) ||
                            !execbc_vm_value_slot(
                                projection,
                                projection->operands[source->operands.start + 1u],
                                &rightSlot, &ignoredType)) goto invalid_projection;
                        instruction = execbc_vm_instruction(
                                source->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD
                                        ? ZR_INSTRUCTION_ENUM(ADD_SIGNED)
                                        : ZR_INSTRUCTION_ENUM(SUB_SIGNED),
                                (TZrUInt16)destinationSlot);
                        instruction.instruction.operand.operand1[0] = (TZrUInt16)leftSlot;
                        instruction.instruction.operand.operand1[1] = (TZrUInt16)rightSlot;
                        break;
                    case ZR_EXEC_IR_OPCODE_COMPARE:
                        if (!execbc_vm_value_slot(
                                projection,
                                projection->results[source->results.start],
                                &destinationSlot, &ignoredType) ||
                            !execbc_vm_value_slot(
                                projection,
                                projection->operands[source->operands.start],
                                &leftSlot, &ignoredType) ||
                            !execbc_vm_value_slot(
                                projection,
                                projection->operands[source->operands.start + 1u],
                                &rightSlot, &ignoredType)) goto invalid_projection;
                        instruction = execbc_vm_instruction(
                                source->typeToken == ZR_EXEC_IR_COMPARE_KIND_LESS
                                        ? ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)
                                        : ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED),
                                (TZrUInt16)destinationSlot);
                        instruction.instruction.operand.operand1[0] = (TZrUInt16)leftSlot;
                        instruction.instruction.operand.operand1[1] = (TZrUInt16)rightSlot;
                        break;
                    default:
                        goto unsupported;
                }
                if (!execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                           instruction, block->id,
                                           instructionIndex + 1u, source->sourceId,
                                           noTarget)) goto invalid_projection;
            }
        }
        if (!execbc_vm_append_move_group(projection, block, plan,
                                         (TZrUInt32)total, &planCount))
            goto invalid_projection;
        if (synthetic != ZR_FALSE ||
            block->terminatorInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrExecIrInstructionId originInstruction;
            TZrExecIrSourceId originSource;
            TZrExecIrBlockId target =
                    projection->successors[block->successors.start];
            TZrInstruction instruction = execbc_vm_instruction(
                    ZR_INSTRUCTION_ENUM(JUMP), 0u);
            execbc_vm_instruction_origin(projection, block,
                                         &originInstruction, &originSource);
            if (!execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                       instruction, block->id, originInstruction,
                                       originSource, target))
                goto invalid_projection;
            continue;
        }
        {
            TZrUInt32 terminatorIndex = block->terminatorInstructionId - 1u;
            const SZrExecBcInstruction *terminator =
                    &projection->instructions[terminatorIndex];
            if (terminator->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_BRANCH) {
                TZrInstruction instruction = execbc_vm_instruction(
                        ZR_INSTRUCTION_ENUM(JUMP), 0u);
                TZrExecIrBlockId target =
                        projection->successors[block->successors.start];
                if (!execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                           instruction, block->id,
                                           block->terminatorInstructionId,
                                           terminator->sourceId, target))
                    goto invalid_projection;
            } else if (terminator->opcode ==
                       (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
                TZrUInt32 conditionSlot;
                TZrExecIrTypeToken conditionType = 0u;
                TZrInstruction falseBranch;
                TZrInstruction trueBranch;
                if (!execbc_vm_value_slot(
                        projection,
                        projection->operands[terminator->operands.start],
                        &conditionSlot, &conditionType)) goto invalid_projection;
                ZR_UNUSED_PARAMETER(conditionType);
                falseBranch = execbc_vm_instruction(
                        ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE),
                        (TZrUInt16)conditionSlot);
                trueBranch = execbc_vm_instruction(ZR_INSTRUCTION_ENUM(JUMP), 0u);
                if (!execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                           falseBranch, block->id,
                                           block->terminatorInstructionId,
                                           terminator->sourceId,
                                           projection->successors[
                                                   block->successors.start + 1u]) ||
                    !execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                           trueBranch, block->id,
                                           block->terminatorInstructionId,
                                           terminator->sourceId,
                                           projection->successors[
                                                   block->successors.start]))
                    goto invalid_projection;
            } else if (terminator->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN) {
                TZrUInt32 returnSlot;
                TZrExecIrTypeToken returnType = 0u;
                TZrInstruction instruction;
                if (!execbc_vm_value_slot(
                        projection,
                        projection->operands[terminator->operands.start],
                        &returnSlot, &returnType)) goto invalid_projection;
                ZR_UNUSED_PARAMETER(returnType);
                instruction = execbc_vm_instruction(
                        ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u);
                instruction.instruction.operand.operand1[0] = (TZrUInt16)returnSlot;
                if (!execbc_vm_append_plan(plan, (TZrUInt32)total, &planCount,
                                           instruction, block->id,
                                           block->terminatorInstructionId,
                                           terminator->sourceId, 0u))
                    goto invalid_projection;
            } else {
                goto unsupported;
            }
        }
    }
    if (planCount != (TZrUInt32)total) goto invalid_projection;
    for (blockIndex = 0u; blockIndex < planCount; ++blockIndex) {
        SZrExecBcVmPlannedInstruction *planned = &plan[blockIndex];
        if (planned->hasRelativeTarget != ZR_FALSE) {
            TZrInt64 delta;
            if (planned->targetBlockId == 0u ||
                planned->targetBlockId > projection->blockCount) goto invalid_projection;
            delta = (TZrInt64)blockStarts[planned->targetBlockId - 1u] -
                    (TZrInt64)blockIndex - 1;
            if (delta < INT32_MIN || delta > INT32_MAX) goto capacity_error;
            planned->instruction.instruction.operand.operand2[0] = (TZrInt32)delta;
        }
    }
    for (constantIndex = 0u; constantIndex < constantCount; ++constantIndex) {
        if ((constants[constantIndex].typeToken !=
                     (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 &&
             constants[constantIndex].typeToken !=
                     (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL) ||
            (constants[constantIndex].typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL &&
             constants[constantIndex].bits > 1u)) {
            execbc_vm_set_diagnostic(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u, 0u,
                    (TZrUInt32)ZR_VALUE_TYPE_INT64,
                    constants[constantIndex].typeToken);
            goto invalid_constant;
        }
    }
    free(poolMap);
    *outPlan = plan;
    *outBlockStarts = blockStarts;
    *outConstants = constants;
    *outConstantCount = constantCount;
    *outInstructionCount = planCount;
    return ZR_TRUE;

invalid_constant:
    free(poolMap);
    free(constants);
    free(plan);
    free(blockStarts);
    return ZR_FALSE;
unsupported:
    execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                             projection, 0u, 0u, 0u, 0u, 0u);
    free(poolMap);
    free(constants);
    free(plan);
    free(blockStarts);
    return ZR_FALSE;
invalid_projection:
    execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                             projection, 0u, 0u, 0u, 0u, 0u);
    free(poolMap);
    free(constants);
    free(plan);
    free(blockStarts);
    return ZR_FALSE;
capacity_error:
    execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                             projection, 0u, 0u, 0u, UINT32_MAX,
                             total > UINT32_MAX ? UINT32_MAX : (TZrUInt32)total);
    free(poolMap);
    free(constants);
    free(plan);
    free(blockStarts);
    return ZR_FALSE;
oom:
    execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                             projection, 0u, 0u, 0u, 1u, 0u);
    free(poolMap);
    free(constants);
    free(plan);
    free(blockStarts);
    return ZR_FALSE;
}

static TZrBool execbc_vm_fill_core_function(
        SZrState *state,
        const SZrExecBcProjection *projection,
        const SZrExecBcVmPlannedInstruction *plan,
        TZrUInt32 instructionCount,
        const SZrExecBcVmConstant *constants,
        TZrUInt32 constantCount,
        TZrUInt32 stackSize,
        SZrFunction **outFunction,
        TZrBool *outRootAdded,
        SZrExecIrDiagnostic *diagnostic) {
    SZrFunction *function;
    TZrBool rootAdded = ZR_FALSE;
    TZrSize instructionBytes;
    TZrSize constantBytes;
    TZrUInt32 index;

    *outFunction = ZR_NULL;
    *outRootAdded = ZR_FALSE;
    if (instructionCount == 0u || stackSize >= UINT32_MAX ||
        instructionCount > SIZE_MAX / sizeof(TZrInstruction) ||
        constantCount > SIZE_MAX / sizeof(SZrTypeValue)) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              projection, 0u, 0u, 0u, UINT32_MAX,
                              instructionCount);
    }
    instructionBytes = (TZrSize)((size_t)instructionCount * sizeof(TZrInstruction));
    constantBytes = (TZrSize)((size_t)constantCount * sizeof(SZrTypeValue));

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              projection, 0u, 0u, 0u, 1u, 0u);
    }
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                state->global, state, ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                &rootAdded)) {
        ZrCore_Function_Free(state, function);
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              projection, 0u, 0u, 0u, 1u, 0u);
    }

    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->stackSize = stackSize;
    function->vmEntryClearStackSizePlusOne = stackSize + 1u;
    function->frameByteSize = 0u;
    function->frameByteAlign = 0u;
    function->frameSlotLayoutLength = 0u;
    function->frameSlotLayouts = ZR_NULL;

    if (instructionBytes != 0u) {
        function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global, instructionBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->instructionsList == ZR_NULL) goto oom;
        for (index = 0u; index < instructionCount; ++index) {
            function->instructionsList[index] = plan[index].instruction;
        }
        function->instructionsLength = instructionCount;
    }
    if (constantBytes != 0u) {
        function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                state->global, constantBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) goto oom;
        function->constantValueLength = constantCount;
        for (index = 0u; index < constantCount; ++index) {
            if (constants[index].typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) {
                TZrInt64 value;
                memcpy(&value, &constants[index].bits, sizeof(value));
                ZrCore_Value_InitAsInt(state, &function->constantValueList[index], value);
            } else {
                ZrCore_Value_InitAsBool(state, &function->constantValueList[index],
                                        (TZrBool)constants[index].bits);
            }
        }
    }

    if (rootAdded != ZR_FALSE &&
        !ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function))) {
        /* Root registration is membership based. If revocation fails, keep
         * this object alive rather than risk freeing a still-registered root. */
        execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                 projection, 0u, 0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    *outFunction = function;
    *outRootAdded = ZR_FALSE;
    return ZR_TRUE;

oom:
    execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                             projection, 0u, 0u, 0u, 1u, 0u);
    ZrCore_Function_Free(state, function);
    if (rootAdded != ZR_FALSE) {
        (void)ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecBcProjection_MaterializeVmFunction(
        SZrState *state,
        const SZrExecBcProjection *projection,
        SZrExecBcVmEmission *output,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 *instructionOwners = ZR_NULL;
    TZrUInt32 *blockStarts = ZR_NULL;
    TZrExecIrTypeToken *slotTypes = ZR_NULL;
    SZrExecBcVmPlannedInstruction *plan = ZR_NULL;
    SZrExecBcVmConstant *constants = ZR_NULL;
    SZrExecBcVmPcMapEntry *pcMap = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    TZrBool functionRootAdded = ZR_FALSE;
    TZrUInt32 stackSize = 0u;
    TZrUInt32 constantCount = 0u;
    TZrUInt32 instructionCount = 0u;
    TZrBool succeeded = ZR_FALSE;

    if (output != ZR_NULL) memset(output, 0, sizeof(*output));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || projection == ZR_NULL || output == ZR_NULL) {
        return execbc_vm_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              projection, 0u, 0u, 0u, 1u, 0u);
    }
    if (!execbc_vm_validate_projection(projection, &instructionOwners,
                                       &stackSize, &slotTypes, diagnostic)) {
        goto cleanup;
    }
    if (!execbc_vm_build_plan(projection, &plan,
                              &blockStarts, &constants, &constantCount,
                              &instructionCount, diagnostic)) {
        goto cleanup;
    }
    if ((size_t)instructionCount > SIZE_MAX / sizeof(*pcMap)) {
        execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                 projection, 0u, 0u, 0u, UINT32_MAX,
                                 instructionCount);
        goto cleanup;
    }
    pcMap = (SZrExecBcVmPcMapEntry *)malloc(
            (size_t)instructionCount * sizeof(*pcMap));
    if (pcMap == ZR_NULL) {
        execbc_vm_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                 projection, 0u, 0u, 0u, instructionCount, 0u);
        goto cleanup;
    }
    for (TZrUInt32 index = 0u; index < instructionCount; ++index) {
        pcMap[index] = plan[index].pcMap;
    }
    if (!execbc_vm_fill_core_function(state, projection, plan,
                                      instructionCount, constants,
                                      constantCount, stackSize,
                                      &function, &functionRootAdded,
                                      diagnostic)) {
        goto cleanup;
    }
    output->function = function;
    output->pcMap = pcMap;
    output->pcMapCount = instructionCount;
    function = ZR_NULL;
    pcMap = ZR_NULL;
    succeeded = ZR_TRUE;

cleanup:
    if (function != ZR_NULL) {
        ZrCore_Function_Free(state, function);
        if (functionRootAdded != ZR_FALSE) {
            (void)ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
        }
    }
    free(instructionOwners);
    free(blockStarts);
    free(slotTypes);
    free(plan);
    free(constants);
    free(pcMap);
    if (succeeded == ZR_FALSE && output != ZR_NULL) {
        memset(output, 0, sizeof(*output));
    }
    return succeeded;
}

void ZrParser_ExecBcVmEmission_Free(
        SZrState *state,
        SZrExecBcVmEmission *emission) {
    ZR_UNUSED_PARAMETER(state);
    if (emission == ZR_NULL) return;
    free(emission->pcMap);
    memset(emission, 0, sizeof(*emission));
}
