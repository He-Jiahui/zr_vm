#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_common/zr_type_conf.h"
#include "exec_ir_execbc_vm_internal.h"

void execbc_vm_set_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrUInt32 expected,
        TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = projection != ZR_NULL
            ? projection->functionToken
            : 0u;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

TZrBool execbc_vm_fail(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrUInt32 expected,
        TZrUInt32 actual) {
    execbc_vm_set_diagnostic(diagnostic, code, projection, blockId,
                             instructionId, sourceId, expected, actual);
    return ZR_FALSE;
}

static TZrBool execbc_vm_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool execbc_vm_edge_occurrence_matches(
        const TZrExecIrBlockId *edges,
        SZrExecIrRange range,
        TZrUInt32 edgeIndex,
        const TZrExecIrBlockId *reverseEdges,
        SZrExecIrRange reverseRange,
        TZrExecIrBlockId reverseBlockId) {
    TZrExecIrBlockId adjacent = edges[edgeIndex];
    TZrUInt32 occurrence = 0u;
    TZrUInt32 index;

    /* Match parallel edges by their one-based occurrence within this row. */
    for (index = range.start; index <= edgeIndex; ++index) {
        if (edges[index] == adjacent) {
            ++occurrence;
        }
    }
    for (index = reverseRange.start;
         index < reverseRange.start + reverseRange.count;
         ++index) {
        if (reverseEdges[index] == reverseBlockId && --occurrence == 0u) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool execbc_vm_scalar_type(TZrExecIrTypeToken typeToken) {
    return (TZrBool)(typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                     typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL);
}

TZrBool execbc_vm_value_slot(
        const SZrExecBcProjection *projection,
        TZrExecIrValueId valueId,
        TZrUInt32 *slot,
        TZrExecIrTypeToken *typeToken) {
    TZrUInt32 physicalSlot;
    const SZrExecIrValue *value;
    if (projection == ZR_NULL || slot == ZR_NULL || typeToken == ZR_NULL ||
        valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        valueId > projection->valueSlotCount || projection->valueSlots == ZR_NULL) {
        return ZR_FALSE;
    }
    physicalSlot = projection->valueSlots[valueId - 1u];
    if (physicalSlot >= projection->physicalSlotCount || projection->slotValues == ZR_NULL) {
        return ZR_FALSE;
    }
    value = &projection->slotValues[physicalSlot];
    if (value->id != valueId) {
        return ZR_FALSE;
    }
    *slot = physicalSlot;
    *typeToken = value->typeToken;
    return ZR_TRUE;
}

const SZrExecBcBlock *execbc_vm_block_at(
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId) {
    if (projection == ZR_NULL || blockId == 0u || blockId > projection->blockCount ||
        projection->blocks == ZR_NULL) {
        return ZR_NULL;
    }
    return &projection->blocks[blockId - 1u];
}

TZrBool execbc_vm_is_synthetic_block(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block) {
    TZrUInt32 originalBlockCount;
    if (projection == ZR_NULL || block == ZR_NULL ||
        projection->syntheticBlockCount > projection->blockCount) {
        return ZR_FALSE;
    }
    originalBlockCount = projection->blockCount - projection->syntheticBlockCount;
    return (TZrBool)(block->id > originalBlockCount);
}
static TZrBool execbc_vm_validate_instruction_shape(
        const SZrExecBcProjection *projection,
        TZrUInt32 instructionIndex,
        TZrExecIrBlockId blockId,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecBcInstruction *instruction = &projection->instructions[instructionIndex];
    const TZrUInt32 instructionId = instructionIndex + 1u;
    TZrUInt32 operandSlots[2] = {0u, 0u};
    TZrExecIrTypeToken operandTypes[2] = {0u, 0u};
    TZrUInt32 resultSlot = 0u;
    TZrExecIrTypeToken resultType = 0u;
    TZrUInt32 operandCount;
    TZrUInt32 resultCount;

    if (instruction->opcode >= (TZrUInt16)ZR_EXEC_IR_OPCODE_COUNT ||
        instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_INVALID) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                              projection, blockId, instructionId,
                              instruction->sourceId,
                              (TZrUInt32)ZR_EXEC_IR_OPCODE_COUNT,
                              (TZrUInt32)instruction->opcode);
    }
    if (projection->opcodes[instructionIndex] != (TZrUInt32)instruction->opcode ||
        instruction->pc != instructionIndex ||
        !execbc_vm_range_valid(instruction->operands, projection->operandCount) ||
        !execbc_vm_range_valid(instruction->results, projection->resultCount) ||
        !execbc_vm_range_valid(instruction->phiRange, projection->phiIncomingCount) ||
        !execbc_vm_range_valid(instruction->successorRange, projection->successorCount) ||
        !execbc_vm_range_valid(instruction->memoryIn, projection->memoryTokenCount) ||
        !execbc_vm_range_valid(instruction->memoryOut, projection->memoryTokenCount)) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                              projection, blockId, instructionId,
                              instruction->sourceId, projection->instructionCount,
                              instructionIndex);
    }
    if (instruction->deoptId != 0u) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                              projection, blockId, instructionId,
                              instruction->sourceId, 0u,
                              instruction->deoptId);
    }
    if (instruction->bindingRow != 0u || instruction->flags != 0u ||
        instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
        instruction->effectIn != 0u || instruction->effectOut != 0u) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                              projection, blockId, instructionId,
                              instruction->sourceId, 0u,
                              (TZrUInt32)instruction->opcode);
    }

    operandCount = instruction->operands.count;
    resultCount = instruction->results.count;
    switch ((EZrExecIrOpcode)instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_NOP:
            if (operandCount == 0u && resultCount == 0u) return ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_PLACE_BASE:
            /* The projection-wide dead-use proof validates this exact shape. */
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_PHI:
            /* Phi values are implemented by the projection's scheduled moves. */
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_CONSTANT:
            if (operandCount != 0u || resultCount != 1u ||
                !execbc_vm_value_slot(projection,
                                      projection->results[instruction->results.start],
                                      &resultSlot, &resultType)) {
                break;
            }
            if (!execbc_vm_scalar_type(resultType) || instruction->typeToken != resultType) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId,
                                      (TZrUInt32)ZR_VALUE_TYPE_INT64,
                                      instruction->typeToken);
            }
            if (projection->constantCount != 0u &&
                instruction->layoutId >= projection->constantCount) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      projection, blockId, instructionId,
                                      instruction->sourceId,
                                      projection->constantCount,
                                      instruction->layoutId);
            }
            if (projection->constantCount != 0u &&
                projection->constants[instruction->layoutId].flags != 0u) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId, 0u,
                                      projection->constants[instruction->layoutId].flags);
            }
            if (resultType == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL &&
                ((projection->constantCount != 0u &&
                  projection->constants[instruction->layoutId].bits > 1u) ||
                 (projection->constantCount == 0u && instruction->layoutId > 1u))) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      projection, blockId, instructionId,
                                      instruction->sourceId, 1u,
                                      instruction->layoutId);
            }
            if (projection->constantCount != 0u &&
                projection->constants[instruction->layoutId].typeToken != resultType) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      projection, blockId, instructionId,
                                      instruction->sourceId,
                                      resultType,
                                      projection->constants[instruction->layoutId].typeToken);
            }
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
            if (operandCount != 1u || resultCount != 1u ||
                !execbc_vm_value_slot(projection,
                                      projection->operands[instruction->operands.start],
                                      &operandSlots[0], &operandTypes[0]) ||
                !execbc_vm_value_slot(projection,
                                      projection->results[instruction->results.start],
                                      &resultSlot, &resultType)) {
                break;
            }
            if (operandTypes[0] != resultType || !execbc_vm_scalar_type(resultType)) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId, resultType,
                                      operandTypes[0]);
            }
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
            if (operandCount != 2u || resultCount != 1u ||
                !execbc_vm_value_slot(projection,
                                      projection->operands[instruction->operands.start],
                                      &operandSlots[0], &operandTypes[0]) ||
                !execbc_vm_value_slot(projection,
                                      projection->operands[instruction->operands.start + 1u],
                                      &operandSlots[1], &operandTypes[1]) ||
                !execbc_vm_value_slot(projection,
                                      projection->results[instruction->results.start],
                                      &resultSlot, &resultType)) {
                break;
            }
            if (instruction->typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                operandTypes[0] != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                operandTypes[1] != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                resultType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId,
                                      (TZrUInt32)ZR_VALUE_TYPE_INT64,
                                      instruction->typeToken);
            }
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_COMPARE:
            if (operandCount != 2u || resultCount != 1u ||
                !execbc_vm_value_slot(projection,
                                      projection->operands[instruction->operands.start],
                                      &operandSlots[0], &operandTypes[0]) ||
                !execbc_vm_value_slot(projection,
                                      projection->operands[instruction->operands.start + 1u],
                                      &operandSlots[1], &operandTypes[1]) ||
                !execbc_vm_value_slot(projection,
                                      projection->results[instruction->results.start],
                                      &resultSlot, &resultType)) {
                break;
            }
            if (instruction->typeToken != ZR_EXEC_IR_COMPARE_KIND_LESS &&
                instruction->typeToken != ZR_EXEC_IR_COMPARE_KIND_GREATER) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId, 1u,
                                      instruction->typeToken);
            }
            if (instruction->matchTypeToken !=
                        (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                operandTypes[0] != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                operandTypes[1] != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                resultType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL) {
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                      projection, blockId, instructionId,
                                      instruction->sourceId,
                                      (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                                      resultType);
            }
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_BRANCH:
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
        case ZR_EXEC_IR_OPCODE_RETURN:
            /* Terminator arity and CFG correspondence are checked per block. */
            return ZR_TRUE;
        default:
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                  projection, blockId, instructionId,
                                  instruction->sourceId, 0u,
                                  (TZrUInt32)instruction->opcode);
    }
    return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                          projection, blockId, instructionId,
                          instruction->sourceId, 0u, operandCount);
}
TZrBool execbc_vm_validate_projection(
        const SZrExecBcProjection *projection,
        TZrUInt32 **outInstructionOwners,
        TZrUInt32 *outStackSize,
        TZrExecIrTypeToken **outSlotTypes,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 *instructionOwners = ZR_NULL;
    TZrExecIrTypeToken *slotTypes = ZR_NULL;
    TZrUInt32 stackSize;
    TZrUInt32 index;
    TZrBool phiTemporaryRead = ZR_FALSE;
    TZrBool phiTemporaryWritten = ZR_FALSE;

    *outInstructionOwners = ZR_NULL;
    *outStackSize = 0u;
    *outSlotTypes = ZR_NULL;

    if (projection == ZR_NULL || projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG ||
        !projection->runnable || projection->instructionCount == 0u ||
        projection->instructions == ZR_NULL || projection->opcodes == ZR_NULL ||
        projection->blockCount == 0u || projection->blocks == ZR_NULL ||
        projection->entryBlockId == 0u || projection->entryBlockId > projection->blockCount ||
        projection->valueSlotCount == 0u || projection->valueSlots == ZR_NULL ||
        projection->physicalSlotCount < projection->valueSlotCount ||
        projection->slotValues == ZR_NULL ||
        (projection->operandCount != 0u && projection->operands == ZR_NULL) ||
        (projection->resultCount != 0u && projection->results == ZR_NULL) ||
        (projection->phiCount != 0u && projection->phis == ZR_NULL) ||
        (projection->phiIncomingCount != 0u && projection->phiIncomings == ZR_NULL) ||
        (projection->phiCopyCount != 0u &&
         (projection->phiCopySources == ZR_NULL ||
          projection->phiCopyDestinations == ZR_NULL ||
          projection->phiCopyEdges == ZR_NULL)) ||
        (projection->predecessorCount != 0u && projection->predecessors == ZR_NULL) ||
        (projection->successorCount != 0u && projection->successors == ZR_NULL) ||
        (projection->phiMoveCount != 0u && projection->phiMoves == ZR_NULL) ||
        (projection->sourceMapCount != 0u && projection->sourceMaps == ZR_NULL) ||
        (projection->constantCount != 0u && projection->constants == ZR_NULL) ||
        (projection->memoryTokenCount != 0u && projection->memoryTokens == ZR_NULL) ||
        (projection->gcRootCount != 0u && projection->gcRoots == ZR_NULL) ||
        (projection->deoptStateCount != 0u && projection->deoptStates == ZR_NULL) ||
        (projection->deoptValueCount != 0u && projection->deoptValues == ZR_NULL) ||
        (projection->deoptAggregateCount != 0u &&
         projection->deoptAggregates == ZR_NULL) ||
        (projection->deoptAggregateFieldCount != 0u &&
         projection->deoptAggregateFields == ZR_NULL)) {
        return execbc_vm_fail(diagnostic,
                              projection != ZR_NULL && !projection->runnable
                                      ? ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED
                                      : ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                              projection, 0u, 0u, 0u, 1u, 0u);
    }
    if (projection->frameLayoutHash != 0u ||
        projection->frameByteSize != 0u || projection->frameByteAlign != 0u ||
        projection->frameSlotCount != 0u || projection->frameSlots != ZR_NULL ||
        projection->logicalSlotCount != 0u || projection->storageSlotCount != 0u ||
        projection->parameterPrefixBytes != 0u ||
        projection->returnAreaOffset != 0u || projection->layoutCount != 0u ||
        projection->layouts != ZR_NULL || projection->gcMapPresent != ZR_FALSE ||
        projection->gcMapCount != 0u || projection->gcRootCount != 0u ||
        projection->gcMap.entryCount != 0u ||
        projection->gcMap.entryCapacity != 0u ||
        projection->gcMap.entries != ZR_NULL ||
        projection->gcMap.slotIndexCount != 0u ||
        projection->gcMap.slotIndexCapacity != 0u ||
        projection->gcMap.slotIndexPool != ZR_NULL ||
        projection->gcMap.inlineRefOffsetCount != 0u ||
        projection->gcMap.inlineRefOffsetCapacity != 0u ||
        projection->gcMap.inlineRefOffsetPool != ZR_NULL ||
        projection->gcMap.safepointId != 0u ||
        projection->gcMap.sourceId != 0u ||
        projection->gcMap.rootRange.start != 0u ||
        projection->gcMap.rootRange.count != 0u ||
        projection->deoptStateCount != 0u || projection->deoptValueCount != 0u ||
        projection->deoptAggregateCount != 0u ||
        projection->deoptAggregateFieldCount != 0u ||
        (projection->stateMapPresent != ZR_FALSE &&
         projection->stateMapEmpty == ZR_FALSE) ||
        (projection->stateMapPresent == ZR_FALSE &&
         projection->stateMapEmpty != ZR_FALSE) ||
        projection->memoryTokenCount != 0u ||
        projection->memoryTokens != ZR_NULL || projection->gcRoots != ZR_NULL ||
        projection->deoptStates != ZR_NULL || projection->deoptValues != ZR_NULL ||
        projection->deoptAggregates != ZR_NULL ||
        projection->deoptAggregateFields != ZR_NULL) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                              projection, 0u, 0u, 0u, 0u, 1u);
    }
    if (!execbc_vm_validate_dead_place_uses(projection, diagnostic)) {
        return ZR_FALSE;
    }
    if (projection->syntheticBlockCount > projection->blockCount ||
        projection->physicalSlotCount > UINT16_MAX + 1u ||
        projection->temporarySlotCount > 1u ||
        projection->physicalSlotCount > UINT32_MAX - projection->temporarySlotCount) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                              projection, 0u, 0u, 0u,
                              UINT16_MAX + 1u, projection->physicalSlotCount);
    }
    if (projection->entryBlockId >
        projection->blockCount - projection->syntheticBlockCount) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                              projection, projection->entryBlockId, 0u, 0u,
                              projection->blockCount - projection->syntheticBlockCount,
                              projection->entryBlockId);
    }
    stackSize = projection->physicalSlotCount + projection->temporarySlotCount;
    if (projection->temporarySlotCount != 0u) {
        if (projection->phiTemporarySlot < projection->physicalSlotCount ||
            projection->phiTemporarySlot == UINT32_MAX ||
            projection->phiTemporarySlot >= stackSize) {
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                                  projection, 0u, 0u, 0u, stackSize,
                                  projection->phiTemporarySlot);
        }
    }
    if (stackSize > UINT16_MAX + 1u || stackSize >= UINT32_MAX) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                              projection, 0u, 0u, 0u,
                              UINT16_MAX + 1u, stackSize);
    }

    if ((size_t)projection->instructionCount >
        SIZE_MAX / sizeof(*instructionOwners)) {
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              projection, 0u, 0u, 0u,
                              (TZrUInt32)(SIZE_MAX / sizeof(*instructionOwners)),
                              projection->instructionCount);
    }
    instructionOwners = (TZrUInt32 *)calloc(projection->instructionCount,
                                             sizeof(*instructionOwners));
    if (instructionOwners == ZR_NULL) {
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              projection, 0u, 0u, 0u,
                              projection->instructionCount, 0u);
    }
    if (stackSize != 0u) {
        slotTypes = (TZrExecIrTypeToken *)malloc((size_t)stackSize * sizeof(*slotTypes));
        if (slotTypes == ZR_NULL) {
            free(instructionOwners);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  projection, 0u, 0u, 0u, stackSize, 0u);
        }
        for (index = 0u; index < stackSize; ++index) slotTypes[index] = UINT32_MAX;
    }

    for (index = 0u; index < projection->valueSlotCount; ++index) {
        TZrUInt32 slot = projection->valueSlots[index];
        const SZrExecIrValue *value;
        if (slot >= projection->physicalSlotCount || slot > UINT16_MAX) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                  projection, 0u, 0u, 0u,
                                  projection->physicalSlotCount, slot);
        }
        value = &projection->slotValues[slot];
        if (value->id != index + 1u) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                  projection, 0u, 0u, 0u,
                                  index + 1u, value->id);
        }
        if (!execbc_vm_scalar_type(value->typeToken)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                  projection, 0u, value->definition, 0u,
                                  (TZrUInt32)ZR_VALUE_TYPE_INT64,
                                  value->typeToken);
        }
        slotTypes[slot] = value->typeToken;
    }
    for (index = 0u; index < projection->physicalSlotCount; ++index) {
        const SZrExecIrValue *value = &projection->slotValues[index];
        if (value->id != 0u &&
            (value->id > projection->valueSlotCount ||
             projection->valueSlots[value->id - 1u] != index)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                  projection, 0u, 0u, 0u,
                                  projection->valueSlotCount, value->id);
        }
    }

    for (index = 0u; index < projection->blockCount; ++index) {
        const SZrExecBcBlock *block = &projection->blocks[index];
        TZrUInt32 item;
        if (block->id != index + 1u ||
            !execbc_vm_range_valid(block->instructions, projection->instructionCount) ||
            !execbc_vm_range_valid(block->predecessors, projection->predecessorCount) ||
            !execbc_vm_range_valid(block->successors, projection->successorCount) ||
            !execbc_vm_range_valid(block->phis, projection->phiCount)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  projection, block->id, 0u, 0u,
                                  index + 1u, block->id);
        }
        for (item = 0u; item < block->instructions.count; ++item) {
            TZrUInt32 instructionIndex = block->instructions.start + item;
            if (instructionOwners[instructionIndex] != 0u) {
                TZrUInt32 previousOwner = instructionOwners[instructionIndex];
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id, instructionIndex + 1u,
                                      projection->instructions[instructionIndex].sourceId,
                                      0u, previousOwner);
            }
            instructionOwners[instructionIndex] = block->id;
        }
        if (execbc_vm_is_synthetic_block(projection, block) &&
            (block->predecessors.count != 1u ||
             block->instructions.count != 0u ||
             block->terminatorInstructionId !=
                     ZR_EXEC_IR_INSTRUCTION_ID_INVALID)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  projection, block->id, 0u, 0u, 1u,
                                  block->instructions.count != 0u
                                          ? block->instructions.count
                                          : block->terminatorInstructionId);
        }
        for (item = 0u; item < block->predecessors.count; ++item) {
            TZrExecIrBlockId predecessor =
                    projection->predecessors[block->predecessors.start + item];
            if (predecessor == 0u || predecessor > projection->blockCount) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id, 0u, 0u,
                                      projection->blockCount, predecessor);
            }
        }
        for (item = 0u; item < block->successors.count; ++item) {
            TZrExecIrBlockId successor =
                    projection->successors[block->successors.start + item];
            if (successor == 0u || successor > projection->blockCount) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id, 0u, 0u,
                                      projection->blockCount, successor);
            }
        }
        if (block->terminatorInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrUInt32 terminatorIndex = block->terminatorInstructionId - 1u;
            if (block->instructions.count == 0u ||
                block->terminatorInstructionId < block->instructions.start + 1u ||
                block->terminatorInstructionId !=
                        block->instructions.start + block->instructions.count) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id,
                                      block->terminatorInstructionId, 0u,
                                      block->instructions.start + block->instructions.count,
                                      block->terminatorInstructionId);
            }
            if (terminatorIndex >= projection->instructionCount) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id,
                                      block->terminatorInstructionId, 0u,
                                      projection->instructionCount, terminatorIndex);
            }
        } else if (block->successors.count != 1u ||
                   (execbc_vm_is_synthetic_block(projection, block) &&
                    block->instructions.count != 0u)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                                  projection, block->id, 0u, 0u,
                                  1u, block->successors.count);
        }
    }
    /* Every CFG adjacency occurrence must have its matching reverse
     * occurrence. This preserves distinct parallel edges with the same IDs. */
    for (index = 0u; index < projection->blockCount; ++index) {
        const SZrExecBcBlock *block = &projection->blocks[index];
        TZrUInt32 item;

        for (item = 0u; item < block->successors.count; ++item) {
            TZrUInt32 edgeIndex = block->successors.start + item;
            TZrExecIrBlockId successor = projection->successors[edgeIndex];
            const SZrExecBcBlock *target =
                    &projection->blocks[successor - 1u];
            if (!execbc_vm_edge_occurrence_matches(
                        projection->successors, block->successors, edgeIndex,
                        projection->predecessors, target->predecessors,
                        block->id)) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                        projection, block->id, 0u, 0u, 1u, successor);
            }
        }
        for (item = 0u; item < block->predecessors.count; ++item) {
            TZrUInt32 edgeIndex = block->predecessors.start + item;
            TZrExecIrBlockId predecessor =
                    projection->predecessors[edgeIndex];
            const SZrExecBcBlock *source =
                    &projection->blocks[predecessor - 1u];
            if (!execbc_vm_edge_occurrence_matches(
                        projection->predecessors, block->predecessors,
                        edgeIndex, projection->successors, source->successors,
                        block->id)) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                        projection, block->id, 0u, 0u, 1u, predecessor);
            }
        }
    }
    for (index = 0u; index < projection->instructionCount; ++index) {
        if (instructionOwners[index] == 0u) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  projection, 0u, index + 1u,
                                  projection->instructions[index].sourceId,
                                  1u, 0u);
        }
        if (!execbc_vm_validate_instruction_shape(
                    projection, index, instructionOwners[index], diagnostic)) {
            free(instructionOwners);
            free(slotTypes);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < projection->blockCount; ++index) {
        const SZrExecBcBlock *block = &projection->blocks[index];
        if (block->terminatorInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            continue;
        }
        {
            const SZrExecBcInstruction *terminator =
                    &projection->instructions[block->terminatorInstructionId - 1u];
            TZrUInt32 expectedSuccessors = 0u;
            TZrUInt32 expectedOperands = 0u;
            TZrUInt32 expectedResults = 0u;
            switch ((EZrExecIrOpcode)terminator->opcode) {
                case ZR_EXEC_IR_OPCODE_BRANCH:
                    expectedSuccessors = 1u;
                    break;
                case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
                    expectedSuccessors = 2u;
                    expectedOperands = 1u;
                    break;
                case ZR_EXEC_IR_OPCODE_RETURN:
                    expectedOperands = 1u;
                    break;
                default:
                    free(instructionOwners);
                    free(slotTypes);
                    return execbc_vm_fail(diagnostic,
                                          ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                                          projection, block->id,
                                          block->terminatorInstructionId,
                                          terminator->sourceId, 0u,
                                          terminator->opcode);
            }
            if (block->successors.count != expectedSuccessors ||
                terminator->successorRange.start != block->successors.start ||
                terminator->successorRange.count != block->successors.count ||
                terminator->operands.count != expectedOperands ||
                terminator->results.count != expectedResults) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      projection, block->id,
                                      block->terminatorInstructionId,
                                      terminator->sourceId, expectedSuccessors,
                                      block->successors.count);
            }
            if (terminator->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
                TZrUInt32 conditionSlot;
                TZrExecIrTypeToken conditionType = 0u;
                if (!execbc_vm_value_slot(projection,
                                          projection->operands[terminator->operands.start],
                                          &conditionSlot, &conditionType) ||
                    conditionType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL) {
                    free(instructionOwners);
                    free(slotTypes);
                    return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                          projection, block->id,
                                          block->terminatorInstructionId,
                                          terminator->sourceId,
                                          (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                                          conditionType);
                }
            } else if (terminator->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN) {
                TZrUInt32 returnSlot;
                TZrExecIrTypeToken returnType = 0u;
                if (!execbc_vm_value_slot(projection,
                                          projection->operands[terminator->operands.start],
                                          &returnSlot, &returnType) ||
                    returnType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) {
                    free(instructionOwners);
                    free(slotTypes);
                    return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                          projection, block->id,
                                          block->terminatorInstructionId,
                                          terminator->sourceId,
                                          (TZrUInt32)ZR_VALUE_TYPE_INT64,
                                          returnType);
                }
            }
        }
    }
    for (index = 0u; index < projection->sourceMapCount; ++index) {
        if (projection->sourceMaps[index].pc >= projection->instructionCount) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                  projection, 0u,
                                  projection->sourceMaps[index].pc == UINT32_MAX
                                          ? UINT32_MAX
                                          : projection->sourceMaps[index].pc + 1u,
                                  projection->sourceMaps[index].sourceId,
                                  projection->instructionCount,
                                  projection->sourceMaps[index].pc);
        }
    }

    for (index = 0u; index < projection->phiMoveCount; ++index) {
        const SZrExecBcPhiMove *move = &projection->phiMoves[index];
        const SZrExecBcBlock *edge = execbc_vm_block_at(projection, move->edge);
        TZrExecIrTypeToken sourceType;
        TZrExecIrTypeToken destinationType;
        TZrBool temporaryWrittenOnEdge = ZR_FALSE;
        if (edge == ZR_NULL || edge->successors.count != 1u ||
            move->sourceSlot >= stackSize || move->destinationSlot >= stackSize ||
            move->sourceSlot > UINT16_MAX || move->destinationSlot > UINT16_MAX ||
            (projection->temporarySlotCount != 0u &&
             move->sourceSlot >= projection->physicalSlotCount &&
             move->sourceSlot != projection->phiTemporarySlot) ||
            (projection->temporarySlotCount != 0u &&
             move->destinationSlot >= projection->physicalSlotCount &&
             move->destinationSlot != projection->phiTemporarySlot)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                                  projection, move->edge, 0u, 0u,
                                  stackSize, move->sourceSlot);
        }
        if (projection->temporarySlotCount != 0u &&
            move->sourceSlot == projection->phiTemporarySlot) {
            TZrUInt32 priorIndex;
            for (priorIndex = 0u; priorIndex < index; ++priorIndex) {
                const SZrExecBcPhiMove *prior = &projection->phiMoves[priorIndex];
                if (prior->edge == move->edge &&
                    prior->destinationSlot == projection->phiTemporarySlot) {
                    temporaryWrittenOnEdge = ZR_TRUE;
                    break;
                }
            }
            if (temporaryWrittenOnEdge == ZR_FALSE) {
                free(instructionOwners);
                free(slotTypes);
                return execbc_vm_fail(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                      projection, move->edge, 0u, 0u,
                                      projection->phiTemporarySlot,
                                      move->sourceSlot);
            }
        }
        sourceType = slotTypes[move->sourceSlot];
        destinationType = slotTypes[move->destinationSlot];
        if (projection->temporarySlotCount != 0u &&
            sourceType == UINT32_MAX &&
            move->sourceSlot == projection->phiTemporarySlot) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                  projection, move->edge, 0u, 0u,
                                  projection->phiTemporarySlot,
                                  move->sourceSlot);
        }
        if (sourceType == UINT32_MAX || !execbc_vm_scalar_type(sourceType)) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                  projection, move->edge, 0u, 0u,
                                  (TZrUInt32)ZR_VALUE_TYPE_INT64,
                                  move->sourceSlot);
        }
        if (destinationType == UINT32_MAX) {
            slotTypes[move->destinationSlot] = sourceType;
        } else if (destinationType != sourceType) {
            free(instructionOwners);
            free(slotTypes);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                  projection, move->edge, 0u, 0u,
                                  destinationType, sourceType);
        }
        if (projection->temporarySlotCount != 0u) {
            if (move->sourceSlot == projection->phiTemporarySlot) {
                phiTemporaryRead = ZR_TRUE;
            }
            if (move->destinationSlot == projection->phiTemporarySlot) {
                phiTemporaryWritten = ZR_TRUE;
            }
        }
    }
    if (projection->temporarySlotCount != 0u &&
        (phiTemporaryRead == ZR_FALSE || phiTemporaryWritten == ZR_FALSE)) {
        free(instructionOwners);
        free(slotTypes);
        return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                              projection, 0u, 0u, 0u, 1u,
                              (TZrUInt32)(phiTemporaryRead && phiTemporaryWritten));
    }
    if (!execbc_vm_validate_phi_data(
                projection, instructionOwners, stackSize, diagnostic)) {
        free(instructionOwners);
        free(slotTypes);
        return ZR_FALSE;
    }

    *outInstructionOwners = instructionOwners;
    *outStackSize = stackSize;
    *outSlotTypes = slotTypes;
    return ZR_TRUE;
}
