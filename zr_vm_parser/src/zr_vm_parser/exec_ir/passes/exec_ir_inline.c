#include "zr_vm_parser/exec_ir_interprocedural.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static TZrExecIrBlockId inline_block_for_instruction(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId) {
    TZrUInt32 i;
    if (function == ZR_NULL || instructionId == 0u ||
        function->blocks == ZR_NULL || function->blockCount > function->blockCapacity ||
        instructionId > function->instructionCount) return ZR_EXEC_IR_BLOCK_ID_INVALID;
    for (i = 0u; i < function->blockCount; ++i) {
        const SZrExecIrBlock *block = &function->blocks[i];
        if (block->instructionRange.start > function->instructionCount ||
            block->instructionRange.count > function->instructionCount -
                block->instructionRange.start) continue;
        if (instructionId - 1u >= block->instructionRange.start &&
            instructionId - 1u < block->instructionRange.start +
                                  block->instructionRange.count) return block->id;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static void inline_diag(SZrExecIrDiagnostic *diagnostic,
                        EZrExecutionDiagnosticCode code,
                        const SZrExecIrFunction *function,
                        TZrUInt32 instructionId,
                        TZrUInt64 expected,
                        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = inline_block_for_instruction(function, instructionId);
    if (function != ZR_NULL && instructionId != 0u &&
        instructionId <= function->instructionCount &&
        instructionId <= function->instructionCapacity &&
        function->instructions != ZR_NULL) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static TZrBool range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool allocation_count_fits(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (size_t)count <= SIZE_MAX / elementSize);
}

static TZrBool pure_opcode(EZrExecIrOpcode opcode) {
    switch (opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_NEG:
        case ZR_EXEC_IR_OPCODE_COMPARE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool instruction_is_pure(const SZrExecIrFunction *function,
                                   const SZrExecIrInstruction *instruction) {
    const SZrExecIrOpcodeInfo *info;
    if (function == ZR_NULL || instruction == ZR_NULL ||
        !pure_opcode((EZrExecIrOpcode)instruction->opcode) ||
        instruction->flags != 0u || instruction->memoryIn.count != 0u ||
        instruction->memoryOut.count != 0u || instruction->effectIn != 0u ||
        instruction->effectOut != 0u || instruction->phiRange.count != 0u ||
        instruction->successorRange.count != 0u ||
        !range_valid(instruction->operandRange, function->operandCount) ||
        !range_valid(instruction->resultRange, function->resultCount)) return ZR_FALSE;
    info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    return (TZrBool)(info != ZR_NULL && info->effects == 0u);
}

static TZrBool value_storage_valid(const SZrExecIrFunction *function) {
    return (TZrBool)(function != ZR_NULL &&
                     function->valueCount <= function->valueCapacity &&
                     function->instructionCount <= function->instructionCapacity &&
                     function->blockCount <= function->blockCapacity &&
                     function->operandCount <= function->operandCapacity &&
                     function->resultCount <= function->resultCapacity &&
                     function->memoryTokenCount <= function->memoryTokenCapacity &&
                     function->phiCount <= function->phiCapacity &&
                     function->phiIncomingCount <= function->phiIncomingCapacity &&
                     function->predecessorCount <= function->predecessorCapacity &&
                     function->successorCount <= function->successorCapacity &&
                     function->gcMapCount <= function->gcMapCapacity &&
                     function->gcRootCount <= function->gcRootCapacity &&
                     function->deoptStateCount <= function->deoptStateCapacity &&
                     function->deoptValueCount <= function->deoptValueCapacity &&
                     function->sourceMapCount <= function->sourceMapCapacity &&
                     (function->valueCount == 0u || function->values != ZR_NULL) &&
                     (function->instructionCount == 0u || function->instructions != ZR_NULL) &&
                     (function->blockCount == 0u || function->blocks != ZR_NULL) &&
                     (function->operandCount == 0u || function->operands != ZR_NULL) &&
                     (function->resultCount == 0u || function->results != ZR_NULL) &&
                     (function->memoryTokenCount == 0u || function->memoryTokenPool != ZR_NULL) &&
                     (function->phiCount == 0u || function->phiPool != ZR_NULL) &&
                     (function->phiIncomingCount == 0u || function->phiIncoming != ZR_NULL) &&
                     (function->predecessorCount == 0u || function->predecessors != ZR_NULL) &&
                     (function->successorCount == 0u || function->successors != ZR_NULL) &&
                     (function->gcRootCount == 0u || function->gcRoots != ZR_NULL) &&
                     (function->deoptStateCount == 0u || function->deoptStates != ZR_NULL) &&
                     (function->deoptValueCount == 0u || function->deoptValues != ZR_NULL) &&
                     (function->sourceMapCount == 0u || function->sourceMaps != ZR_NULL) &&
                     (function->gcMapCount == 0u || function->gcMap != ZR_NULL));
}

static TZrBool has_non_linear_block_state(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL) return ZR_TRUE;
    if (function->blockCount > 1u || function->phiCount != 0u ||
        function->phiIncomingCount != 0u || function->predecessorCount != 0u ||
        function->successorCount != 0u) return ZR_TRUE;
    if (function->blocks == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < function->blockCount; ++i) {
        if ((function->blocks[i].flags &
             (ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION)) != 0u) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool is_block_terminator(const SZrExecIrFunction *function,
                                   TZrExecIrInstructionId instructionId) {
    TZrUInt32 i;
    if (function == ZR_NULL || instructionId == 0u ||
        function->blocks == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < function->blockCount; ++i) {
        if (function->blocks[i].terminatorInstructionId == instructionId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool has_instruction_deopt(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL || function->instructions == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < function->instructionCount; ++i) {
        if (function->instructions[i].deoptId != 0u) return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool source_maps_well_formed(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL) ||
        function->sourceMapCount > function->sourceMapCapacity) return ZR_FALSE;
    for (i = 0u; i < function->sourceMapCount; ++i) {
        TZrExecIrInstructionId instructionId = function->sourceMaps[i].instructionId;
        if (instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            instructionId > function->instructionCount) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool find_return(const SZrExecIrFunction *callee,
                           TZrUInt32 *returnIndex,
                           TZrExecIrValueId *returnValue) {
    TZrUInt32 i;
    if (!value_storage_valid(callee) || returnIndex == ZR_NULL || returnValue == ZR_NULL ||
        callee->instructionCount == 0u) return ZR_FALSE;
    for (i = 0u; i < callee->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &callee->instructions[i];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_RETURN) continue;
        if (i + 1u != callee->instructionCount || instruction->flags != 0u ||
            instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
            instruction->effectIn != 0u || instruction->effectOut != 0u ||
            instruction->phiRange.count != 0u || instruction->successorRange.count != 0u ||
            instruction->deoptId != 0u || instruction->operandRange.count != 1u ||
            instruction->operandRange.start >= callee->operandCount) return ZR_FALSE;
        *returnIndex = i;
        *returnValue = callee->operands[instruction->operandRange.start];
        return *returnValue != ZR_EXEC_IR_VALUE_ID_INVALID && *returnValue <= callee->valueCount;
    }
    return ZR_FALSE;
}

static TZrUInt32 parameter_count(const SZrExecIrFunction *callee,
                                 TZrExecIrValueId *parameters,
                                 TZrUInt32 capacity) {
    TZrUInt32 i, count = 0u;
    if (callee == ZR_NULL || parameters == ZR_NULL) return 0u;
    for (i = 0u; i < callee->valueCount; ++i) {
        if ((callee->values[i].flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) == 0u) continue;
        if (count < capacity) parameters[count] = i + 1u;
        ++count;
    }
    return count;
}

static TZrBool map_operand(const SZrExecIrFunction *caller,
                           const TZrExecIrValueId *map,
                           TZrUInt32 mapCount,
                           TZrExecIrValueId source,
                           TZrExecIrValueId *mapped) {
    if (mapped == ZR_NULL || source == ZR_EXEC_IR_VALUE_ID_INVALID || source >= mapCount ||
        map[source] == ZR_EXEC_IR_VALUE_ID_INVALID ||
        (caller != ZR_NULL && map[source] > caller->valueCount)) return ZR_FALSE;
    *mapped = map[source];
    return ZR_TRUE;
}

static TZrBool append_remapped_instruction(
        SZrExecIrFunction *caller, const SZrExecIrFunction *callee,
        const SZrExecIrInstruction *source, const TZrExecIrValueId *map,
        TZrUInt32 mapCount, TZrExecIrValueId callerResult,
        SZrExecIrInstruction *out) {
    TZrUInt32 i;
    TZrExecIrValueId *operands = ZR_NULL;
    TZrExecIrValueId *results = ZR_NULL;
    SZrExecIrRange operandRange;
    SZrExecIrRange resultRange;
    memset(&operandRange, 0, sizeof(operandRange));
    memset(&resultRange, 0, sizeof(resultRange));
    if (caller == ZR_NULL || callee == ZR_NULL || source == ZR_NULL || out == ZR_NULL ||
        !instruction_is_pure(callee, source)) return ZR_FALSE;
    if (!range_valid(source->operandRange, callee->operandCount) ||
        !range_valid(source->resultRange, callee->resultCount) ||
        (source->operandRange.count != 0u && callee->operands == ZR_NULL) ||
        (source->resultRange.count != 0u && callee->results == ZR_NULL)) return ZR_FALSE;
    if (source->operandRange.count != 0u) {
        if (!allocation_count_fits(source->operandRange.count,
                                   sizeof(*operands))) return ZR_FALSE;
        operands = (TZrExecIrValueId *)malloc((size_t)source->operandRange.count * sizeof(*operands));
        if (operands == ZR_NULL) return ZR_FALSE;
        for (i = 0u; i < source->operandRange.count; ++i) {
            TZrExecIrValueId value;
            TZrExecIrValueId input = callee->operands[source->operandRange.start + i];
            if (!map_operand(caller, map, mapCount, input, &value)) {
                free(operands);
                return ZR_FALSE;
            }
            operands[i] = value;
        }
        if (!ZrCore_ExecIr_FunctionAppendOperands(caller, operands,
                                                   source->operandRange.count,
                                                   &operandRange)) {
            free(operands);
            return ZR_FALSE;
        }
    }
    free(operands);
    if (source->resultRange.count != 0u) {
        if (source->resultRange.count != 1u) return ZR_FALSE;
        if (!allocation_count_fits(1u, sizeof(*results))) return ZR_FALSE;
        results = (TZrExecIrValueId *)malloc(sizeof(*results));
        if (results == ZR_NULL) return ZR_FALSE;
        if (callerResult != ZR_EXEC_IR_VALUE_ID_INVALID) {
            results[0] = callerResult;
        } else {
            const TZrExecIrValueId sourceValue = callee->results[source->resultRange.start];
            TZrExecIrValueId value;
            if (sourceValue == ZR_EXEC_IR_VALUE_ID_INVALID || sourceValue > callee->valueCount ||
                ZrCore_ExecIr_FunctionAddValue(caller,
                    callee->values[sourceValue - 1u].typeToken,
                    callee->values[sourceValue - 1u].ownership,
                    callee->values[sourceValue - 1u].nullability) == ZR_EXEC_IR_VALUE_ID_INVALID) {
                free(results);
                return ZR_FALSE;
            }
            value = caller->valueCount;
            results[0] = value;
        }
        if (!ZrCore_ExecIr_FunctionAppendResults(caller, results, 1u, &resultRange)) {
            free(results);
            return ZR_FALSE;
        }
        free(results);
    }
    *out = *source;
    out->operands = operandRange;
    out->results = resultRange;
    out->phiRange.start = 0u;
    out->phiRange.count = 0u;
    out->successorRange.start = 0u;
    out->successorRange.count = 0u;
    out->memoryIn.start = 0u;
    out->memoryIn.count = 0u;
    out->memoryOut.start = 0u;
    out->memoryOut.count = 0u;
    out->effectIn = 0u;
    out->effectOut = 0u;
    out->bindingRow = 0u;
    return ZR_TRUE;
}

/* A tiny identity callee (`return parameter`) has no value-producing body
 * instruction to clone.  Materialize the forwarding operation explicitly as
 * COPY so the caller's result value remains a valid SSA definition and its
 * ownership/type contract is still visible to later passes. */
static TZrBool append_forward_copy(SZrExecIrFunction *caller,
                                   TZrExecIrValueId argument,
                                   TZrExecIrValueId callerResult,
                                   SZrExecIrRange resultRange,
                                   SZrExecIrInstruction *out) {
    SZrExecIrRange operands;
    memset(&operands, 0, sizeof(operands));
    if (caller == ZR_NULL || out == ZR_NULL ||
        argument == ZR_EXEC_IR_VALUE_ID_INVALID || argument > caller->valueCount ||
        callerResult == ZR_EXEC_IR_VALUE_ID_INVALID || callerResult > caller->valueCount ||
        !ZrCore_ExecIr_FunctionAppendOperands(caller, &argument, 1u, &operands)) {
        return ZR_FALSE;
    }
    memset(out, 0, sizeof(*out));
    out->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_COPY;
    out->operands = operands;
    /* Reuse the original CALL result slot.  No result-pool append is needed
     * because the call's one-element range remains in the caller pool. */
    out->results = resultRange;
    out->typeToken = caller->values[callerResult - 1u].typeToken;
    return ZR_TRUE;
}

static void recompute_definitions(SZrExecIrFunction *function) {
    TZrUInt32 i, j;
    if (function == ZR_NULL || function->values == ZR_NULL) return;
    for (i = 0u; i < function->valueCount; ++i) {
        function->values[i].definition = ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
    }
    for (i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        if (!range_valid(instruction->resultRange, function->resultCount)) continue;
        for (j = instruction->resultRange.start;
             j < instruction->resultRange.start + instruction->resultRange.count; ++j) {
            TZrExecIrValueId value = function->results[j];
            if (value != 0u && value <= function->valueCount) function->values[value - 1u].definition = i + 1u;
        }
    }
}

static TZrBool shift_side_table_ids(SZrExecIrFunction *function,
                                    TZrExecIrInstructionId pivot,
                                    TZrUInt32 delta) {
    TZrUInt32 i;
    TZrUInt32 pivotIndex;
    if (function == ZR_NULL) return ZR_FALSE;
    if (delta == 0u) return ZR_TRUE;
    /* Ranges are zero-based, while source/GC/terminator ids are one-based. */
    pivotIndex = pivot == 0u ? 0u : pivot - 1u;
    /* Preflight additions so a malformed side table cannot wrap an id and
     * leave the caller in a state that no longer names its source/GC site. */
    for (i = 0u; i < function->sourceMapCount; ++i) {
        if (function->sourceMaps[i].instructionId > pivot &&
            function->sourceMaps[i].instructionId > UINT32_MAX - delta) return ZR_FALSE;
    }
    if (function->gcMap != ZR_NULL) {
        for (i = 0u; i < function->gcMap->entryCount; ++i) {
            if (function->gcMap->entries[i].site > pivot &&
                function->gcMap->entries[i].site > UINT32_MAX - delta) return ZR_FALSE;
        }
    }
    for (i = 0u; i < function->blockCount; ++i) {
        const SZrExecIrBlock *block = &function->blocks[i];
        if (block->instructionRange.start > pivotIndex &&
            block->instructionRange.start > UINT32_MAX - delta) return ZR_FALSE;
        if (block->instructionRange.start <= pivotIndex &&
            pivotIndex < block->instructionRange.start + block->instructionRange.count &&
            block->instructionRange.count > UINT32_MAX - delta) return ZR_FALSE;
        if (block->terminatorInstructionId > pivot &&
            block->terminatorInstructionId > UINT32_MAX - delta) return ZR_FALSE;
    }
    for (i = 0u; i < function->sourceMapCount; ++i) {
        if (function->sourceMaps[i].instructionId > pivot) function->sourceMaps[i].instructionId += delta;
    }
    if (function->gcMap != ZR_NULL) {
        for (i = 0u; i < function->gcMap->entryCount; ++i) {
            if (function->gcMap->entries[i].site > pivot) function->gcMap->entries[i].site += delta;
        }
    }
    for (i = 0u; i < function->deoptStateCount; ++i) {
        /* DeoptState stores stable logical ids; only reconstruction values are
         * copied by this restricted pass, so no instruction id rewrite here. */
        (void)function->deoptStates[i];
    }
    for (i = 0u; i < function->blockCount; ++i) {
        SZrExecIrBlock *block = &function->blocks[i];
        if (block->instructionRange.start > pivotIndex) block->instructionRange.start += delta;
        if (block->instructionRange.start <= pivotIndex &&
            pivotIndex < block->instructionRange.start + block->instructionRange.count) {
            block->instructionRange.count += delta;
        }
        if (block->terminatorInstructionId > pivot) block->terminatorInstructionId += delta;
    }
    return ZR_TRUE;
}

static TZrBool append_source_map_for_clone(SZrExecIrFunction *caller,
                                           const SZrExecIrFunction *callee,
                                           TZrUInt32 calleeInstructionIndex,
                                           TZrExecIrInstructionId newId) {
    TZrUInt32 i;
    SZrExecIrSourceMap *replacement;
    TZrUInt32 capacity;
    if (caller == ZR_NULL || callee == ZR_NULL || callee->sourceMaps == ZR_NULL) return ZR_TRUE;
    for (i = 0u; i < callee->sourceMapCount; ++i) {
        if (callee->sourceMaps[i].instructionId != calleeInstructionIndex + 1u) continue;
        if (caller->sourceMapCount == caller->sourceMapCapacity) {
            if (caller->sourceMapCapacity > UINT32_MAX / 2u) {
                capacity = caller->sourceMapCount + 1u;
            } else {
                capacity = caller->sourceMapCapacity == 0u ? 8u : caller->sourceMapCapacity * 2u;
            }
            if (capacity <= caller->sourceMapCount ||
                !allocation_count_fits(capacity, sizeof(*replacement))) return ZR_FALSE;
            replacement = (SZrExecIrSourceMap *)realloc(caller->sourceMaps,
                                                        (size_t)capacity * sizeof(*replacement));
            if (replacement == ZR_NULL) return ZR_FALSE;
            caller->sourceMaps = replacement;
            caller->sourceMapCapacity = capacity;
        }
        caller->sourceMaps[caller->sourceMapCount] = callee->sourceMaps[i];
        caller->sourceMaps[caller->sourceMapCount].instructionId = newId;
        ++caller->sourceMapCount;
    }
    return ZR_TRUE;
}

static TZrBool inline_one(SZrExecIrFunction *caller,
                          const SZrExecIrFunction *callee,
                          TZrExecIrInstructionId callId,
                          TZrUInt32 maxCost,
                          TZrUInt32 *growth,
                          SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *parameters = ZR_NULL;
    TZrExecIrValueId *map = ZR_NULL;
    TZrExecIrValueId returnValue;
    TZrUInt32 returnIndex, parameterTotal, i, bodyCount, cloneCount, oldCount, extra;
    TZrUInt32 inlineCost;
    TZrUInt32 callIndex;
    TZrExecIrValueId callerResult = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrBool returnProduced = ZR_FALSE;
    SZrExecIrInstruction *clones = ZR_NULL;
    SZrExecIrInstruction callCopy;
    if (growth != ZR_NULL) *growth = 0u;
    if (!value_storage_valid(caller) || !value_storage_valid(callee) ||
        caller->sealed || callId == 0u || callId > caller->instructionCount ||
        maxCost == 0u || caller->frameLayout != ZR_NULL || callee->frameLayout != ZR_NULL ||
        caller->gcMap != ZR_NULL || callee->gcMap != ZR_NULL ||
        caller->stateMap != ZR_NULL || callee->stateMap != ZR_NULL ||
        caller->gcRootCount != 0u || callee->gcRootCount != 0u ||
        caller->deoptStateCount != 0u || callee->deoptStateCount != 0u ||
        caller->deoptValueCount != 0u || callee->deoptValueCount != 0u ||
        has_instruction_deopt(caller) || has_instruction_deopt(callee) ||
        !source_maps_well_formed(caller) || !source_maps_well_formed(callee) ||
        has_non_linear_block_state(caller) || has_non_linear_block_state(callee)) {
        inline_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                    caller, callId, 0u, 0u);
        return ZR_FALSE;
    }
    callIndex = callId - 1u;
    callCopy = caller->instructions[callIndex];
    /* A call that is itself the block terminator owns control-flow semantics
     * that this scalar splicer does not recreate (the cloned body ends in a
     * value operation, not a branch/return). */
    if (is_block_terminator(caller, callId)) return ZR_FALSE;
    /* INVOKE owns an exceptional successor/cleanup edge.  This minimal
     * splicer deliberately handles only ordinary CALL sites; the graph pass
     * marks INVOKE as STATE_MAP-blocked as an additional safety net. */
    if (callCopy.opcode != ZR_EXEC_IR_OPCODE_CALL) return ZR_FALSE;
    if (callCopy.flags != 0u || callCopy.memoryIn.count != 0u ||
        callCopy.memoryOut.count != 0u || callCopy.effectIn != 0u ||
        callCopy.effectOut != 0u) return ZR_FALSE;
    if (callCopy.resultRange.count != 1u ||
        !range_valid(callCopy.operandRange, caller->operandCount) ||
        !range_valid(callCopy.resultRange, caller->resultCount) ||
        (caller->operandCount != 0u && caller->operands == ZR_NULL) ||
        (caller->resultCount != 0u && caller->results == ZR_NULL)) return ZR_FALSE;
    if (!find_return(callee, &returnIndex, &returnValue)) return ZR_FALSE;
    bodyCount = returnIndex;
    /* A zero-length value body is the identity/return-forwarding form.  It
     * is represented by one synthetic COPY below, so it still consumes one
     * per-call cost unit and remains disabled by a zero budget. */
    inlineCost = bodyCount == 0u ? 1u : bodyCount;
    if (inlineCost > maxCost) return ZR_FALSE;
    for (i = 0u; i < bodyCount; ++i) {
        if (!instruction_is_pure(callee, &callee->instructions[i])) return ZR_FALSE;
    }
    if (callee->valueCount == UINT32_MAX) return ZR_FALSE;
    parameterTotal = callee->valueCount;
    parameters = parameterTotal == 0u ? ZR_NULL :
        (allocation_count_fits(parameterTotal, sizeof(*parameters))
             ? (TZrExecIrValueId *)malloc((size_t)parameterTotal * sizeof(*parameters))
             : ZR_NULL);
    if (!allocation_count_fits(callee->valueCount + 1u, sizeof(*map))) {
        free(parameters);
        return ZR_FALSE;
    }
    map = (TZrExecIrValueId *)calloc((size_t)callee->valueCount + 1u,
                                     sizeof(*map));
    if ((parameterTotal != 0u && parameters == ZR_NULL) || map == ZR_NULL) {
        free(parameters); free(map); return ZR_FALSE;
    }
    parameterTotal = parameter_count(callee, parameters, callee->valueCount);
    /* The restricted scalar form has no hidden closure/receiver operands.  A
     * mismatch is therefore a signature failure, not an invitation to guess
     * which prefix of the call arguments belongs to the callee. */
    if (parameterTotal != callCopy.operandRange.count) {
        free(parameters); free(map); return ZR_FALSE;
    }
    for (i = 0u; i < parameterTotal; ++i) {
        TZrExecIrValueId argument = caller->operands[callCopy.operandRange.start + i];
        if (argument == ZR_EXEC_IR_VALUE_ID_INVALID || argument > caller->valueCount) {
            free(parameters); free(map); return ZR_FALSE;
        }
        /* The call graph pins the callee signature hash, but the compact
         * fixture does not carry a separate argument-signature record.  Use
         * the value type tokens as an additional local proof whenever both
         * sides publish one; an absent token remains an intentionally opaque
         * (and therefore compatible) type. */
        if (callee->values[parameters[i] - 1u].typeToken != 0u &&
            caller->values[argument - 1u].typeToken != 0u &&
            callee->values[parameters[i] - 1u].typeToken !=
                caller->values[argument - 1u].typeToken) {
            free(parameters); free(map); return ZR_FALSE;
        }
        if (callee->values[parameters[i] - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
            caller->values[argument - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
            callee->values[parameters[i] - 1u].ownership !=
                caller->values[argument - 1u].ownership) {
            /* Ownership conversion/retain/release is deliberately outside
             * this scalar splicer.  Refuse a mismatched pair instead of
             * changing drop order or creating an extra owner implicitly. */
            free(parameters); free(map); return ZR_FALSE;
        }
        if (callee->values[parameters[i] - 1u].nullability !=
                ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
            caller->values[argument - 1u].nullability !=
                ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
            callee->values[parameters[i] - 1u].nullability !=
                caller->values[argument - 1u].nullability) {
            free(parameters); free(map); return ZR_FALSE;
        }
        map[parameters[i]] = argument;
    }
    callerResult = caller->results[callCopy.resultRange.start];
    if (callerResult == ZR_EXEC_IR_VALUE_ID_INVALID || callerResult > caller->valueCount ||
        returnValue == ZR_EXEC_IR_VALUE_ID_INVALID || returnValue > callee->valueCount ||
        (caller->values[callerResult - 1u].typeToken != 0u &&
         callee->values[returnValue - 1u].typeToken != 0u &&
         caller->values[callerResult - 1u].typeToken !=
             callee->values[returnValue - 1u].typeToken) ||
        (caller->values[callerResult - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
         callee->values[returnValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
         caller->values[callerResult - 1u].ownership !=
             callee->values[returnValue - 1u].ownership)) {
        free(parameters); free(map); return ZR_FALSE;
    }
    if (caller->values[callerResult - 1u].nullability !=
            ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
        callee->values[returnValue - 1u].nullability !=
            ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
        caller->values[callerResult - 1u].nullability !=
            callee->values[returnValue - 1u].nullability) {
        free(parameters); free(map); return ZR_FALSE;
    }
    for (i = 0u; i < bodyCount; ++i) {
        const SZrExecIrInstruction *source = &callee->instructions[i];
        if (source->resultRange.count == 1u &&
            callee->results[source->resultRange.start] == returnValue) {
            returnProduced = ZR_TRUE;
            break;
        }
    }
    /* Returning a parameter is safe only for the identity form in this
     * restricted pass.  A body that performs other work and then returns a
     * parameter would need an explicit ownership-aware forwarding step after
     * the cloned operations; leave that shape for a later state-map pass. */
    if (!returnProduced && bodyCount != 0u) {
        free(parameters); free(map); return ZR_FALSE;
    }
    cloneCount = bodyCount == 0u ? 1u : bodyCount;
    clones = allocation_count_fits(cloneCount, sizeof(*clones))
             ? (SZrExecIrInstruction *)calloc(cloneCount, sizeof(*clones))
             : ZR_NULL;
    if (clones == ZR_NULL) { free(parameters); free(map); return ZR_FALSE; }
    if (!returnProduced) {
        /* returnValue is a parameter in this branch; map[] was populated
         * above after the signature/ownership checks. */
        if (map[returnValue] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            !append_forward_copy(caller, map[returnValue], callerResult,
                                  callCopy.resultRange, &clones[0])) {
            free(clones); free(parameters); free(map); return ZR_FALSE;
        }
    }
    for (i = 0u; i < bodyCount; ++i) {
        const SZrExecIrInstruction *source = &callee->instructions[i];
        TZrExecIrValueId sourceResult = source->resultRange.count == 1u
            ? callee->results[source->resultRange.start] : ZR_EXEC_IR_VALUE_ID_INVALID;
        TZrExecIrValueId targetResult = sourceResult == returnValue ? callerResult : ZR_EXEC_IR_VALUE_ID_INVALID;
        TZrUInt32 resultCountBefore = caller->resultCount;
        if (sourceResult != ZR_EXEC_IR_VALUE_ID_INVALID && targetResult == ZR_EXEC_IR_VALUE_ID_INVALID) {
            TZrExecIrValueId newValue = ZrCore_ExecIr_FunctionAddValue(
                    caller, callee->values[sourceResult - 1u].typeToken,
                    callee->values[sourceResult - 1u].ownership,
                    callee->values[sourceResult - 1u].nullability);
            if (newValue == ZR_EXEC_IR_VALUE_ID_INVALID) {
                free(clones); free(parameters); free(map); return ZR_FALSE;
            }
            targetResult = newValue;
        }
        if (!append_remapped_instruction(caller, callee, source, map,
                                          callee->valueCount + 1u, targetResult,
                                          &clones[i])) {
            free(clones); free(parameters); free(map); return ZR_FALSE;
        }
        if (sourceResult == returnValue && targetResult == callerResult) {
            /* Reuse the original call result slot instead of leaving an
             * orphan duplicate in the side pool.  The helper appended a
             * temporary one-element range; it is always the last entry. */
            if (caller->resultCount != resultCountBefore + 1u) {
                free(clones); free(parameters); free(map); return ZR_FALSE;
            }
            --caller->resultCount;
            clones[i].results = callCopy.resultRange;
        }
        if (sourceResult != ZR_EXEC_IR_VALUE_ID_INVALID) map[sourceResult] = targetResult;
        clones[i].sourceId = source->sourceId;
        clones[i].deoptId = source->deoptId;
    }
    oldCount = caller->instructionCount;
    extra = cloneCount > 1u ? cloneCount - 1u : 0u;
    if (extra != 0u) {
        if (oldCount > UINT32_MAX - extra ||
            !ZrCore_ExecIr_FunctionReserveInstructions(caller, oldCount + extra)) {
            free(clones); free(parameters); free(map); return ZR_FALSE;
        }
        memmove(&caller->instructions[callIndex + cloneCount],
                &caller->instructions[callIndex + 1u],
                (size_t)(oldCount - callIndex - 1u) * sizeof(*caller->instructions));
    }
    for (i = 0u; i < cloneCount; ++i) caller->instructions[callIndex + i] = clones[i];
    caller->instructions[callIndex].sourceId = callCopy.sourceId;
    caller->instructions[callIndex].deoptId = callCopy.deoptId;
    caller->instructions[callIndex].bindingRow = 0u;
    caller->instructionCount = oldCount + extra;
    if (extra != 0u && !shift_side_table_ids(caller, callId, extra)) {
        free(clones); free(parameters); free(map); return ZR_FALSE;
    }
    for (i = 0u; i < bodyCount; ++i) {
        if (!append_source_map_for_clone(caller, callee, i, callId + i)) {
            free(clones); free(parameters); free(map); return ZR_FALSE;
        }
    }
    /* RETURN is removed by the splice, but its source location still matters
     * for traceback/diagnostic consumers.  Attach that mapping to the final
     * cloned value instruction instead of dropping the callee's return site. */
    if (!append_source_map_for_clone(caller, callee, returnIndex,
                                     callId + cloneCount - 1u)) {
        free(clones); free(parameters); free(map); return ZR_FALSE;
    }
    recompute_definitions(caller);
    if (growth != ZR_NULL) *growth = extra;
    free(clones); free(parameters); free(map);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_InlineCalls(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    TZrUInt32 moduleGrowth = 0u;
    TZrUInt32 *functionGrowth = ZR_NULL;
    TZrBool *modified = ZR_NULL;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (module == ZR_NULL || graph == ZR_NULL ||
        !ZrParser_ExecIr_CallGraphValidate(graph, module, diagnostic)) return ZR_FALSE;
    if (module->functionCount != 0u) {
        if (!allocation_count_fits(module->functionCount,
                                    sizeof(*functionGrowth)) ||
            !allocation_count_fits(module->functionCount, sizeof(*modified))) {
            inline_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                        ZR_NULL, 0u, module->functionCount, 0u);
            return ZR_FALSE;
        }
        functionGrowth = (TZrUInt32 *)calloc(module->functionCount,
                                             sizeof(*functionGrowth));
        modified = (TZrBool *)calloc(module->functionCount, sizeof(*modified));
        if (functionGrowth == ZR_NULL || modified == ZR_NULL) {
            free(functionGrowth);
            free(modified);
            inline_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                        ZR_NULL, 0u, module->functionCount, 0u);
            return ZR_FALSE;
        }
    }
    for (i = graph->edgeCount; i != 0u; --i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i - 1u];
        SZrExecIrFunction *caller;
        const SZrExecIrFunction *callee;
        const SZrExecIrFunctionSummary *summary;
        SZrExecIrFunction backup;
        TZrUInt32 callerIndex;
        TZrUInt32 growth = 0u;
        if (!edge->inlineEligible || edge->callerId == 0u || edge->calleeId == 0u ||
            edge->callerId > module->functionCount || edge->calleeId > module->functionCount) continue;
        callerIndex = edge->callerId - 1u;
        caller = &module->functions[edge->callerId - 1u];
        callee = &module->functions[edge->calleeId - 1u];
        summary = ZrParser_ExecIr_CallGraphSummaryAt(graph, edge->calleeId);
        /* Treat edge eligibility as a cache hint, not authority.  A graph
         * with graphHash==0 can be assembled by a diagnostic consumer; it
         * must not be able to forge a virtual/guarded/patchable edge into a
         * direct scalar splice merely by setting inlineEligible. */
        if (summary == ZR_NULL || summary->validity != ZR_EXEC_IR_SUMMARY_STABLE ||
            !summary->pure || summary->unknownEffects || summary->mayThrow ||
            summary->maySuspend || summary->mayEscape ||
            (edge->kind != ZR_EXEC_IR_CALL_EDGE_DIRECT &&
             edge->kind != ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT &&
             edge->kind != ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT) ||
            !edge->exactReceiver ||
            edge->guarded || edge->patchableTarget || edge->nativeEffectsUnknown ||
            edge->inlineReason != ZR_EXEC_IR_INLINE_REASON_NONE ||
            summary->patchable ||
            summary->sccId == graph->summaries[edge->callerId - 1u].sccId) {
            continue;
        }
        if (edge->targetGeneration != callee->contract.generation ||
            edge->targetToken != (callee->contract.targetToken != 0u
                                      ? callee->contract.targetToken
                                      : callee->functionToken) ||
            edge->expectedSignatureHash != callee->signatureHash) {
            continue;
        }
        if (caller->instructions == ZR_NULL ||
            edge->callInstructionId == 0u ||
            edge->callInstructionId > caller->instructionCount) {
            continue;
        }
        {
            const SZrExecIrInstruction *edgeCall =
                    &caller->instructions[edge->callInstructionId - 1u];
            if (edgeCall->bindingRow != 0u &&
                edgeCall->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
                (edgeCall->layoutId != 0u ||
                 (edgeCall->bindingRow & ZR_EXEC_IR_BINDING_TARGET_MASK) !=
                     edge->calleeId)) {
                /* Preserve ordinary BindingFacts row indices when this pass
                 * is called without the facts table that could prove them. */
                continue;
            }
        }
        /* A callee that was already rewritten in this pass no longer matches
         * the body hash used to classify this edge.  Rebuild the graph before
         * considering it again; using the stale summary would make the pass
         * order observable. */
        if (caller == callee || caller->sealed ||
            modified[edge->calleeId - 1u] ||
            moduleGrowth > graph->inlineBudget.maxGrowthPerModule ||
            functionGrowth[callerIndex] > graph->inlineBudget.maxGrowthPerFunction) continue;
        if (callee->instructionCount == 0u) continue;
        if ((callee->instructionCount == 1u ? 1u : callee->instructionCount - 1u) >
            graph->inlineBudget.maxCostPerCall) continue;
        ZrCore_ExecIr_FunctionInit(&backup);
        if (!ZrCore_ExecIr_CloneFunction(caller, &backup, diagnostic)) {
            free(functionGrowth);
            free(modified);
            return ZR_FALSE;
        }
        if (!inline_one(caller, callee, edge->callInstructionId,
                        graph->inlineBudget.maxCostPerCall, &growth, diagnostic) ||
            growth > graph->inlineBudget.maxGrowthPerFunction - functionGrowth[callerIndex] ||
            growth > graph->inlineBudget.maxGrowthPerModule ||
            moduleGrowth > graph->inlineBudget.maxGrowthPerModule - growth) {
            ZrCore_ExecIr_FreeFunction(caller);
            *caller = backup;
            if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
            continue;
        }
        ZrCore_ExecIr_FreeFunction(&backup);
        functionGrowth[callerIndex] += growth;
        moduleGrowth += growth;
        modified[callerIndex] = ZR_TRUE;
    }
    free(functionGrowth);
    free(modified);
    return ZrCore_ExecIr_ValidateModule(module, diagnostic);
}

TZrBool ZrParser_ExecIr_RunInline(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_InlineCalls(module, graph, diagnostic);
}
