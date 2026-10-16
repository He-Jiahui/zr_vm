#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_owner_state.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include "exec_ir_verify_ssa.h"
#include "exec_ir_deopt_aggregate.h"
#include "exec_ir_edge_identity.h"
#include "exec_ir_verify_constants.h"

#include <string.h>

static void zr_exec_ir_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                      EZrExecutionDiagnosticCode code,
                                      const SZrExecIrFunction *function,
                                      TZrUInt32 instructionId,
                                      TZrUInt32 blockId,
                                      TZrUInt32 expected,
                                      TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = function != ZR_NULL && function->instructions != ZR_NULL &&
                           instructionId != 0u && instructionId <= function->instructionCount
                               ? function->instructions[instructionId - 1u].sourceId : 0u;
    diagnostic->blockId = blockId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrBool zr_exec_ir_range_is_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_exec_ir_is_effect_phi_incoming(
        const SZrExecIrFunction *function,
        TZrUInt32 incomingIndex) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (block->effectPhiResult != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
            incomingIndex >= block->effectPhiIncomings.start &&
            incomingIndex - block->effectPhiIncomings.start <
                block->effectPhiIncomings.count) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_is_memory_phi_incoming(
        const SZrExecIrFunction *function,
        TZrUInt32 incomingIndex) {
    TZrUInt32 blockIndex;
    TZrUInt32 region;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (block->memoryPhiResults[region] != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                incomingIndex >= block->memoryPhiIncomings[region].start &&
                incomingIndex - block->memoryPhiIncomings[region].start <
                    block->memoryPhiIncomings[region].count) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

/* Pool shapes have already been validated. Resolve the referring instruction
 * only on failure; unreferenced side-table records still carry source context. */
static void zr_exec_ir_set_deopt_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecIrFunction *function,
        const SZrExecIrDeoptState *state,
        TZrUInt32 expected,
        TZrUInt32 actual) {
    TZrUInt32 index;
    zr_exec_ir_set_diagnostic(diagnostic, code, function, 0u, 0u,
                              expected, actual);
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->sourceId = state->sourceId;
    if (state->id != 0u) {
        for (index = 0u; index < function->instructionCount; ++index) {
            if (function->instructions[index].deoptId == state->id) {
                diagnostic->instructionId = index + 1u;
                break;
            }
        }
    }
}

static TZrBool zr_exec_ir_validate_deopt_values(
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 stateIndex;
    for (stateIndex = 0u; stateIndex < function->deoptStateCount; ++stateIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[stateIndex];
        SZrExecIrRange range = state->valueRange;
        TZrUInt32 index;
        if (!zr_exec_ir_range_is_valid(range, function->deoptValueCount)) {
            TZrBool invalidStart = (TZrBool)(range.start > function->deoptValueCount);
            zr_exec_ir_set_deopt_diagnostic(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    function, state,
                    invalidStart ? function->deoptValueCount
                                 : function->deoptValueCount - range.start,
                    invalidStart ? range.start : range.count);
            return ZR_FALSE;
        }
        for (index = range.start; index < range.start + range.count; ++index) {
            TZrExecIrValueId valueId = function->deoptValues[index];
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                valueId > function->valueCount) {
                zr_exec_ir_set_deopt_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        function, state, function->valueCount, valueId);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_verify_exception_result_terminator(
        const SZrExecIrFunction *function,
        const SZrExecIrBlock *block,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOpcodeInfo *info,
        TZrExecIrInstructionId instructionId,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrRange instructionEdges;
    TZrExecIrBlockId normalTarget;
    TZrExecIrBlockId exceptionTarget;

    if (info == ZR_NULL ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) == 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) == 0u ||
        instruction->resultRange.count == 0u) {
        return ZR_TRUE;
    }

    instructionEdges = instruction->successorRange.count != 0u
                               ? instruction->successorRange
                               : block->successorRange;
    if (block->successorRange.count != 2u || instructionEdges.count != 2u) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE,
                                  function,
                                  instructionId,
                                  block->id,
                                  2u,
                                  instructionEdges.count);
        return ZR_FALSE;
    }

    normalTarget = function->successors[instructionEdges.start];
    exceptionTarget = function->successors[instructionEdges.start + 1u];
    if (function->successors[block->successorRange.start] != normalTarget ||
        function->successors[block->successorRange.start + 1u] !=
                exceptionTarget ||
        normalTarget == exceptionTarget ||
        (function->blocks[normalTarget - 1u].flags &
         ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u ||
        (function->blocks[exceptionTarget - 1u].flags &
         ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 0u) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE,
                                  function,
                                  instructionId,
                                  block->id,
                                  ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION,
                                  function->blocks[exceptionTarget - 1u].flags);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_block_has_only_exception_predecessors(
        const SZrExecIrFunction *function,
        const SZrExecIrBlock *block) {
    TZrUInt32 predecessorIndex;
    TZrBool foundPredecessor = ZR_FALSE;

    for (predecessorIndex = block->predecessorRange.start;
         predecessorIndex < block->predecessorRange.start +
                                    block->predecessorRange.count;
         ++predecessorIndex) {
        TZrExecIrBlockId predecessorId =
                function->predecessors[predecessorIndex];
        const SZrExecIrBlock *predecessor =
                &function->blocks[predecessorId - 1u];
        const SZrExecIrInstruction *terminator;
        const SZrExecIrOpcodeInfo *info;
        SZrExecIrRange edges;
        TZrBool isExceptionPredecessor = ZR_FALSE;

        if (predecessor->instructionRange.count == 0u) {
            return ZR_FALSE;
        }
        terminator = &function->instructions[
                predecessor->instructionRange.start +
                predecessor->instructionRange.count - 1u];
        info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)terminator->opcode);
        if (info == ZR_NULL ||
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) == 0u ||
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) == 0u) {
            return ZR_FALSE;
        }
        edges = terminator->successorRange.count != 0u
                        ? terminator->successorRange
                        : predecessor->successorRange;
        if (terminator->opcode == ZR_EXEC_IR_OPCODE_THROW) {
            if (edges.count == 1u &&
                function->successors[edges.start] == block->id) {
                isExceptionPredecessor = ZR_TRUE;
            }
        } else if (edges.count == 2u &&
                   function->successors[edges.start + 1u] == block->id) {
            isExceptionPredecessor = ZR_TRUE;
        }
        if (!isExceptionPredecessor) {
            return ZR_FALSE;
        }
        foundPredecessor = ZR_TRUE;
    }
    return foundPredecessor;
}

static TZrBool zr_exec_ir_verify_exception_payload(
        const SZrExecIrFunction *function,
        const SZrExecIrBlock *block,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 instructionIndex;
    TZrUInt32 payloadCount = 0u;

    for (instructionIndex = block->instructionRange.start;
         instructionIndex < block->instructionRange.start +
                                    block->instructionRange.count;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD) {
            continue;
        }
        ++payloadCount;
        if (function->entryBlockId == block->id ||
            (block->flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u ||
            (block->flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 0u ||
            !zr_exec_ir_block_has_only_exception_predecessors(function, block) ||
            payloadCount > 1u) {
            zr_exec_ir_set_diagnostic(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE,
                    function,
                    instructionIndex + 1u,
                    block->id,
                    payloadCount > 1u
                            ? 1u
                            : ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION,
                    payloadCount > 1u ? payloadCount : block->flags);
            if (diagnostic != ZR_NULL) {
                diagnostic->sourceId = instruction->sourceId;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_value_range_is_valid(const SZrExecIrFunction *function,
                                               SZrExecIrRange range,
                                               TZrUInt32 count,
                                               SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (!zr_exec_ir_range_is_valid(range, count)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                  function,
                                  0u,
                                  0u,
                                  count,
                                  range.start + range.count);
        return ZR_FALSE;
    }
    for (index = range.start; index < range.start + range.count; ++index) {
        TZrExecIrValueId valueId = function->operands == ZR_NULL
                                       ? ZR_EXEC_IR_VALUE_ID_INVALID
                                       : function->operands[index];
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      function,
                                      0u,
                                      0u,
                                      function->valueCount,
                                      valueId);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_validate_function(const SZrExecIrFunction *function,
                                             SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->id == ZR_EXEC_IR_FUNCTION_ID_INVALID ||
        function->functionToken == 0u || function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->gcMapCount > function->gcMapCapacity ||
        function->gcRootCount > function->gcRootCapacity ||
        function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        function->sourceMapCount > function->sourceMapCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->phiCount > function->phiCapacity ||
        (function->frameLayout != ZR_NULL &&
         function->frameLayout->slotCount > function->frameLayout->slotCapacity) ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->gcMapCount != 0u && function->gcMaps == ZR_NULL) ||
        (function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL) ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL) ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->frameLayout != ZR_NULL && function->frameLayout->slotCount != 0u &&
         function->frameLayout->slots == ZR_NULL)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  function,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_FunctionValidateBindingRows(function, diagnostic)) {
        return ZR_FALSE;
    }
    if (function->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
        function->entryBlockId > function->blockCount) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  function,
                                  0u,
                                  function->entryBlockId,
                                  function->blockCount,
                                  function->entryBlockId);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->valueCount; ++index) {
        if (function->values[index].id != index + 1u ||
            (TZrUInt32)function->values[index].ownership >=
                ZR_EXEC_IR_OWNERSHIP_COUNT ||
            (TZrUInt32)function->values[index].nullability >=
                ZR_EXEC_IR_NULLABILITY_COUNT ||
            (function->values[index].flags &
             ~ZR_EXEC_IR_VALUE_FLAG_MASK) != 0u ||
            ((function->values[index].flags &
              ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u &&
             (function->values[index].flags &
              ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) == 0u)) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      function,
                                      0u,
                                      0u,
                                      index + 1u,
                                      function->values[index].id);
            return ZR_FALSE;
        }
    }
    if (!zr_exec_ir_validate_deopt_values(function, diagnostic) ||
        !zr_exec_ir_deopt_aggregates_validate(function, diagnostic)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->predecessorCount; ++index) {
        if (function->predecessors[index] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->predecessors[index] > function->blockCount) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      0u,
                                      function->blockCount,
                                      function->predecessors[index]);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->successorCount; ++index) {
        if (function->successors[index] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->successors[index] > function->blockCount) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      0u,
                                      function->blockCount,
                                      function->successors[index]);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        TZrBool effectPhiIncoming = zr_exec_ir_is_effect_phi_incoming(function, index);
        TZrBool memoryPhiIncoming = zr_exec_ir_is_memory_phi_incoming(function, index);
        if (function->phiIncoming[index].predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->phiIncoming[index].predecessor > function->blockCount ||
            (!effectPhiIncoming && !memoryPhiIncoming &&
             (function->phiIncoming[index].value == ZR_EXEC_IR_VALUE_ID_INVALID ||
              function->phiIncoming[index].value > function->valueCount))) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      memoryPhiIncoming
                                          ? ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN
                                          : (effectPhiIncoming
                                                 ? ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN
                                                 : ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE),
                                      function,
                                      0u,
                                      function->phiIncoming[index].predecessor,
                                      (effectPhiIncoming || memoryPhiIncoming)
                                          ? UINT32_MAX
                                          : function->valueCount,
                                      function->phiIncoming[index].value);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->phiCount; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID || phi->result > function->valueCount ||
            !zr_exec_ir_range_is_valid(phi->incomings,
                                       function->phiIncomingCount)) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      function,
                                      0u,
                                      0u,
                                      function->phiIncomingCount,
                                      phi->incomings.offset + phi->incomings.count);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->memoryTokenCount; ++index) {
        if (function->memoryTokenPool[index] == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                      function,
                                      0u,
                                      0u,
                                      index + 1u,
                                      function->memoryTokenPool[index]);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (block->id != index + 1u ||
            !zr_exec_ir_range_is_valid(block->instructionRange, function->instructionCount) ||
            !zr_exec_ir_range_is_valid(block->predecessorRange, function->predecessorCount) ||
            !zr_exec_ir_range_is_valid(block->successorRange, function->successorCount) ||
            !zr_exec_ir_range_is_valid(block->phis, function->phiCount) ||
            !zr_exec_ir_range_is_valid(block->effectPhiIncomings,
                                       function->phiIncomingCount) ||
            ((block->effectPhiResult == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
             (block->effectPhiIncomings.count == 0u))) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      block->id,
                                      index + 1u,
                                      block->id);
            return ZR_FALSE;
        }
        {
            TZrUInt32 region;
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (!zr_exec_ir_range_is_valid(block->memoryPhiIncomings[region],
                                               function->phiIncomingCount) ||
                    ((block->memoryPhiResults[region] ==
                      ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) !=
                     (block->memoryPhiIncomings[region].count == 0u))) {
                    zr_exec_ir_set_diagnostic(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                            function, 0u, block->id,
                            function->phiIncomingCount,
                            block->memoryPhiIncomings[region].count);
                    return ZR_FALSE;
                }
                if (block->memoryPhiResults[region] !=
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    (!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(
                             block->memoryPhiResults[region]) ||
                     ZR_EXEC_IR_MEMORY_TOKEN_REGION(
                             block->memoryPhiResults[region]) !=
                        (EZrExecIrMemoryClass)region ||
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                             block->memoryPhiResults[region]) == 0u)) {
                    zr_exec_ir_set_diagnostic(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                            function, 0u, block->id, region,
                            block->memoryPhiResults[region]);
                    return ZR_FALSE;
                }
            }
        }
        if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u &&
            function->entryBlockId != block->id) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      block->id,
                                      function->entryBlockId,
                                      block->id);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
        TZrUInt32 operandCount;

        if (info == ZR_NULL) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                      function,
                                      index + 1u,
                                      0u,
                                      ZR_EXEC_IR_OPCODE_COUNT - 1u,
                                      (TZrUInt32)instruction->opcode);
            return ZR_FALSE;
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE &&
            instruction->typeToken > ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL) {
            zr_exec_ir_set_diagnostic(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    function, index + 1u, 0u,
                    ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL, instruction->typeToken);
            return ZR_FALSE;
        }
        if ((instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
             instruction->matchTypeToken == 0u) ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_TYPE_TEST &&
             instruction->matchTypeToken != 0u)) {
            zr_exec_ir_set_diagnostic(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    function,
                    index + 1u,
                    0u,
                    instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST
                            ? 1u
                            : 0u,
                    instruction->matchTypeToken);
            return ZR_FALSE;
        }
        if ((instruction->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !zr_exec_ir_range_is_valid(instruction->operandRange, function->operandCount) ||
            !zr_exec_ir_range_is_valid(instruction->resultRange, function->resultCount) ||
            !zr_exec_ir_range_is_valid(instruction->phiRange, function->phiIncomingCount) ||
            !zr_exec_ir_range_is_valid(instruction->successorRange, function->successorCount) ||
            !zr_exec_ir_range_is_valid(instruction->memoryIn, function->memoryTokenCount) ||
            !zr_exec_ir_range_is_valid(instruction->memoryOut, function->memoryTokenCount)) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      function,
                                      index + 1u,
                                      0u,
                                      0u,
                                      0u);
            return ZR_FALSE;
        }
        operandCount = instruction->operandRange.count;
        if (operandCount < info->minimumOperands || operandCount > info->maximumOperands ||
            !zr_exec_ir_value_range_is_valid(function,
                                              instruction->operandRange,
                                              function->operandCount,
                                              diagnostic)) {
            if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
                zr_exec_ir_set_diagnostic(diagnostic,
                                          ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                          function,
                                          index + 1u,
                                          0u,
                                          info->minimumOperands,
                                          operandCount);
            }
            return ZR_FALSE;
        }
        if (instruction->resultRange.count != info->resultArity &&
            info->resultArity != ZR_EXEC_IR_VARIADIC) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      function,
                                      index + 1u,
                                      0u,
                                      info->resultArity,
                                      instruction->resultRange.count);
            return ZR_FALSE;
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED &&
            !ZrCore_ExecIr_ConditionalCleanupValid(function, index + 1u)) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      function, index + 1u, 0u, 0u, 0u);
            if (diagnostic != ZR_NULL) diagnostic->sourceId = instruction->sourceId;
            return ZR_FALSE;
        }
        for (TZrUInt32 resultIndex = instruction->resultRange.start;
             resultIndex < instruction->resultRange.start + instruction->resultRange.count;
             ++resultIndex) {
            if (function->results[resultIndex] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                function->results[resultIndex] > function->valueCount) {
                zr_exec_ir_set_diagnostic(diagnostic,
                                          ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                          function,
                                          index + 1u,
                                          0u,
                                          function->valueCount,
                                          function->results[resultIndex]);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ValidateModule(const SZrExecIrModule *module,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    zr_exec_ir_clear_diagnostic(diagnostic);
    if (module == ZR_NULL || module->functionCount > module->functionCapacity ||
        module->constantCount > module->constantCapacity ||
        module->layoutCount > module->layoutCapacity ||
        module->sourceMapCount > module->sourceMapCapacity ||
        (module->constantCount != 0u && module->constants == ZR_NULL) ||
        (module->layoutCount != 0u && module->layouts == ZR_NULL) ||
        (module->sourceMapCount != 0u && module->sourceMaps == ZR_NULL) ||
        (module->functionCount != 0u && module->functions == ZR_NULL)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < module->functionCount; ++index) {
        if (module->functions[index].id != index + 1u) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      &module->functions[index],
                                      0u,
                                      0u,
                                      index + 1u,
                                      module->functions[index].id);
            return ZR_FALSE;
        }
        if (!zr_exec_ir_validate_function(&module->functions[index], diagnostic)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_VerifyModule(const SZrExecIrModule *module,
                                   SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (!ZrCore_ExecIr_ValidateModule(module, diagnostic)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < module->functionCount; ++index) {
        if (!ZrCore_ExecIr_VerifyFunction(&module->functions[index],
                                          ZR_EXEC_IR_VERIFY_ALL,
                                          diagnostic)) {
            return ZR_FALSE;
        }
        if (!zr_exec_ir_verify_owned_constants(
                    module, &module->functions[index], diagnostic)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
                                     EZrExecIrVerifyLevel level,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;

    zr_exec_ir_clear_diagnostic(diagnostic);
    if (function == ZR_NULL || level == 0u ||
        (level & ~(EZrExecIrVerifyLevel)ZR_EXEC_IR_VERIFY_ALL) != 0u) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  function,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_validate_function(function, diagnostic)) {
        return ZR_FALSE;
    }

    if ((level & ZR_EXEC_IR_VERIFY_STRUCTURE) != 0u) {
        if (function->blockCount != 0u &&
            function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      0u,
                                      ZR_EXEC_IR_BLOCK_ID_ENTRY,
                                      ZR_EXEC_IR_BLOCK_ID_INVALID);
            return ZR_FALSE;
        }
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 edgeIndex;
            if (block->predecessorRange.count != 0u && function->predecessors == ZR_NULL) {
                zr_exec_ir_set_diagnostic(diagnostic,
                                          ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                          function,
                                          0u,
                                          block->id,
                                          0u,
                                          0u);
                return ZR_FALSE;
            }
            for (edgeIndex = block->predecessorRange.start;
                 edgeIndex < block->predecessorRange.start + block->predecessorRange.count;
                 ++edgeIndex) {
                if (function->predecessors[edgeIndex] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                    function->predecessors[edgeIndex] > function->blockCount) {
                    zr_exec_ir_set_diagnostic(diagnostic,
                                              ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                              function,
                                              0u,
                                              block->id,
                                              function->blockCount,
                                              function->predecessors[edgeIndex]);
                    return ZR_FALSE;
                }
                {
                    TZrExecIrBlockId from = function->predecessors[edgeIndex];
                    const SZrExecIrBlock *predecessor = &function->blocks[from - 1u];
                    if (!zr_exec_ir_edge_occurrence_matches(
                                function->predecessors, block->predecessorRange,
                                edgeIndex, function->successors,
                                predecessor->successorRange, block->id)) {
                        zr_exec_ir_set_diagnostic(diagnostic,
                                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                                  function, 0u, block->id, from, block->id);
                        return ZR_FALSE;
                    }
                }
            }
            for (edgeIndex = block->successorRange.start;
                 edgeIndex < block->successorRange.start + block->successorRange.count;
                 ++edgeIndex) {
                if (function->successors[edgeIndex] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                    function->successors[edgeIndex] > function->blockCount) {
                    zr_exec_ir_set_diagnostic(diagnostic,
                                              ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                              function,
                                              0u,
                                              block->id,
                                              function->blockCount,
                                              function->successors[edgeIndex]);
                    return ZR_FALSE;
                }
                {
                    TZrExecIrBlockId to = function->successors[edgeIndex];
                    const SZrExecIrBlock *successor = &function->blocks[to - 1u];
                    if (!zr_exec_ir_edge_occurrence_matches(
                                function->successors, block->successorRange,
                                edgeIndex, function->predecessors,
                                successor->predecessorRange, block->id)) {
                        zr_exec_ir_set_diagnostic(diagnostic,
                                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                                  function, 0u, block->id, block->id, to);
                        return ZR_FALSE;
                    }
                }
            }
            if (!zr_exec_ir_verify_exception_payload(
                        function, block, diagnostic)) {
                return ZR_FALSE;
            }
            if (block->instructionRange.count != 0u) {
                TZrUInt32 lastIndex = block->instructionRange.start +
                                      block->instructionRange.count - 1u;
                const SZrExecIrInstruction *terminator = &function->instructions[lastIndex];
                const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(terminator->opcode);
                for (TZrUInt32 instructionIndex = block->instructionRange.start;
                     instructionIndex < lastIndex; ++instructionIndex) {
                    const SZrExecIrOpcodeInfo *earlier = ZrCore_ExecIr_OpcodeInfo(
                            function->instructions[instructionIndex].opcode);
                    if ((earlier->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u) {
                        zr_exec_ir_set_diagnostic(diagnostic,
                                                  ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                                                  function, instructionIndex + 1u, block->id,
                                                  lastIndex + 1u, instructionIndex + 1u);
                        return ZR_FALSE;
                    }
                }
                if (info == ZR_NULL || (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) == 0u) {
                    zr_exec_ir_set_diagnostic(diagnostic,
                                              ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                                              function,
                                              lastIndex + 1u,
                                              block->id,
                                              1u,
                                              0u);
                    return ZR_FALSE;
                }
                if (!zr_exec_ir_verify_exception_result_terminator(
                            function,
                            block,
                            terminator,
                            info,
                            lastIndex + 1u,
                            diagnostic)) {
                    return ZR_FALSE;
                }
                if (terminator->successorRange.count != block->successorRange.count) {
                    zr_exec_ir_set_diagnostic(diagnostic,
                                              ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                              function, lastIndex + 1u, block->id,
                                              block->successorRange.count,
                                              terminator->successorRange.count);
                    return ZR_FALSE;
                }
                for (TZrUInt32 ordinal = 0u;
                     ordinal < block->successorRange.count; ++ordinal) {
                    TZrExecIrBlockId expected = function->successors[
                            block->successorRange.start + ordinal];
                    TZrExecIrBlockId actual = function->successors[
                            terminator->successorRange.start + ordinal];
                    if (expected != actual) {
                        zr_exec_ir_set_diagnostic(diagnostic,
                                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                                  function, lastIndex + 1u, block->id,
                                                  expected, actual);
                        return ZR_FALSE;
                    }
                }
                if (block->terminatorInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                    block->terminatorInstructionId != lastIndex + 1u) {
                    zr_exec_ir_set_diagnostic(diagnostic,
                                              ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                                              function,
                                              block->terminatorInstructionId,
                                              block->id,
                                              lastIndex + 1u,
                                              block->terminatorInstructionId);
                    return ZR_FALSE;
                }
            }
        }
    }

    if ((level & ZR_EXEC_IR_VERIFY_SSA) != 0u) {
        if (!zr_exec_ir_verify_ssa(function, diagnostic)) {
            return ZR_FALSE;
        }
    }

    if ((level & ZR_EXEC_IR_VERIFY_EFFECT) != 0u) {
        if (!ZrCore_ExecIr_VerifyEffects(function, diagnostic)) {
            return ZR_FALSE;
        }
    }
    if (function->stateMap != ZR_NULL &&
        !ZrCore_ExecIr_ValidateStateMap(function, function->stateMap, diagnostic)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @fn static void zr_exec_ir_clear_diagnostic(SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为本文件的公开验证入口开始一轮独立诊断；可选记录非空时整体清零，使随后首个失败可以按本轮状态填写位置与预期/实际值。
 *
 * ## Parameters
 * @param[out] diagnostic 可为空；非空时指向本轮独占可写的完整 SZrExecIrDiagnostic 记录，原字段不作为输入。
 *
 * ## Returns
 * 无 C 返回值；空指针是无副作用的空操作，非空记录的全部字节被清零。
 *
 * ## Constraints and lifecycle
 * 清零在 ValidateModule 读取 module、VerifyFunction 检查 function/level 之前执行。记录必须与待读取的模块、函数、所属池和附带 stateMap 分离；本助手不检测任意重叠。只使用调用期间的输出借用，不保存地址、不分配或释放资源、不提供同步；并发验证须各自使用独立诊断记录。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:12：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:14：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:662：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:724：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/execution_contract.h:111：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 blockId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 在验证的已选失败分支写入标量诊断上下文；保留调用方选定的 code、函数/块/指令身份和 expected/actual 值，并在可定位指令时取得 sourceId。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可为空；非空时为独立可写诊断，未被本助手赋值的字段保持原值。
 * @param[in] code 直接写入的诊断码；此处不重新校验该枚举。
 * @param[in] function 可为空；非空且需要诊断时借用其 functionToken，以及可定位指令的 sourceId，实际读取的描述符和池必须有效。
 * @param[in] instructionId 直接写入的从 1 开始指令身份；为 0、越过 instructionCount 或没有 instructions 池时 sourceId 取 0。
 * @param[in] blockId 直接写入的块上下文，0 表示调用方没有提供块位置；不在此查找或验证。
 * @param[in] expected 写入 expectedVersion 的 32 位上下文，可表示 ID 上界、数量、标志或 selector，并非总是版本号。
 * @param[in] actual 写入 actualVersion 的实际 32 位上下文。
 *
 * ## Returns
 * 无 C 返回值；diagnostic 为空时立即返回且不读取 function，其余路径写完本助手负责的七个字段。
 *
 * ## Constraints and lifecycle
 * 本助手不 memset，不清除 expectedHash/actualHash；需要全零初态的公开入口先调用 clear helper。instructionId 的逻辑范围判断不认证池真实分配长度或可读性。输出不得覆盖仍将读取的 function、instructions 或其它验证输入；在填写 functionToken 之前已经写 code，不能依赖任意别名安全。无分配、释放、输入修改或持久借用；输入在调用期间稳定可读，诊断由调用方同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:18：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:25：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:28：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:31：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:35：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/execution_contract.h:119：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:698：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_compare_predicate_domain.c:153：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_range_is_valid(SZrExecIrRange range, TZrUInt32 count)
 *
 * ## Summary
 * 在读取侧池之前判定 start/count 是否属于逻辑已占用范围；用减法界限避免为验证先形成可能溢出的 start + count。
 *
 * ## Parameters
 * @param[in] range 按值传入的从 0 开始 start 和元素 count；不包含池指针或所有权。
 * @param[in] count 逻辑已占用元素数，不是底层 capacity 或字节数。
 *
 * ## Returns
 * @return start <= count 且 range.count <= count - start 时为 ZR_TRUE，否则为 ZR_FALSE；start == count 的空区间可通过，空区间起点越过 count 仍被拒绝。
 *
 * ## Constraints and lifecycle
 * 纯标量谓词，不检查池是否存在、实际字节长度、元素内容、跨池重叠或分配能力，不写诊断，也不分配、释放或保留资源。TRUE 才允许所属调用者安全形成逻辑区间终点；实际存储有效性仍由前置形状检查及调用方负责。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:39：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:40：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:112：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_deopt_validation.c:61：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:68：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:73：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:81：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_is_effect_phi_incoming( const SZrExecIrFunction *function, TZrUInt32 incomingIndex)
 *
 * ## Summary
 * 识别共享 phiIncoming 表中被某个有效 effectPhiResult 的 incoming 选区声明为 effect token 的下标，防止通用值预检把 effect token 当作普通 valueId。
 *
 * ## Parameters
 * @param[in] function 非空只读函数及其 blockCount 个有效可读 block；本阶段已通过池形状检查，block 中标量选区尚可能畸形。
 * @param[in] incomingIndex 待分类的共享 incoming 表从 0 开始下标；实际调用处来自整个 phiIncomingCount 的循环。
 *
 * ## Returns
 * @return 至少一个 block 的非 INVALID effectPhiResult 及其 effectPhiIncomings 选区包含该下标时为 ZR_TRUE，否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 使用先比较下标与 start、再比较差值与 count 的包含判定，不解引用 phiIncoming 元素；本助手本身不检查 incomingIndex < phiIncomingCount，不认证选区合法、唯一或互斥。它在完整 block range 检查之前服务于分类，TRUE 只是声明域归属；effect token 语义由后续选中阶段负责。输入只借用，不修改、保存、分配或释放；调用期间 block 描述符不得被修改或释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:43：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:49：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:433：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:437：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:490：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905：本文件的定义或配套验证规则。
 */

/**
 * @fn static TZrBool zr_exec_ir_is_memory_phi_incoming( const SZrExecIrFunction *function, TZrUInt32 incomingIndex)
 *
 * ## Summary
 * 按每个 block 的 memory class 选区识别共享 phiIncoming 元素是否承载 memory token，使通用值检查使用正确的 ID 域。
 *
 * ## Parameters
 * @param[in] function 非空函数与稳定可读的 blocks；读取固定 memory class 槽的 result 与 incoming range，不读取 incoming 载荷。
 * @param[in] incomingIndex 共享 incoming 表的从 0 开始候选下标；调用处枚举已发布 phiIncomingCount。
 *
 * ## Returns
 * @return 任一 block、任一 memory class 的非 INVALID result 对应选区包含该下标时为 ZR_TRUE，未命中为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 分类先于完整 block range 验证，用下标减 start 的方式避免包含判断中的加法溢出；不认证 range 上界、不检查 token 的 tag/region/version，也不拒绝与 effect/其它 class 选区重叠。调用者稍后检查 block 选区及 memory phi result 的标签关系，完整 effect 语义由选中的 verifier 阶段处理。仅借用输入，无输出对象、诊断、分配、释放或同步；读取期间输入必须稳定有效。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:59：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:66：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:67：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:434：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:441：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:506：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:310：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static void zr_exec_ir_set_deopt_diagnostic( SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, const SZrExecIrDeoptState *state, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 为 deopt 选区或 reconstruction value 的失败补充侧表 sourceId；按 state.id 找到第一条引用指令，仅在失败诊断需要时扫描 instructions。未被指令引用的 state 仍可报告侧表来源。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可为空；非空时独立可写，先由局部 setter 写通用字段，再写 deopt 来源与可找到的指令身份。
 * @param[in] code 调用者选定的 INVALID_RANGE 或 INVALID_VALUE 错误码。
 * @param[in] function 诊断非空时借用的非空函数；其 instructions[0..instructionCount) 已通过形状预检且保持可读。
 * @param[in] state 诊断非空时必须有效的当前 deopt state；读取 sourceId 和 id，不获得该记录的所有权。
 * @param[in] expected 写入 expectedVersion 的逻辑上界或剩余可用元素数。
 * @param[in] actual 写入 actualVersion 的越界 start/count 或无效 valueId。
 *
 * ## Returns
 * 无 C 返回值；diagnostic 为空时通用 setter 空操作后立即结束，不读取 state 或扫描 function；非空时 blockId 保持本助手初始化的 0，未找到引用时 instructionId 保持 0。
 *
 * ## Constraints and lifecycle
 * state.id == 0 不执行引用扫描；非零时采用 instructions 数组中的第一条 deoptId 相等指令，并不验证 state ID 唯一性或计算包含块。sourceId 始终来自 state，而不是找到的指令。本助手和所用 setter 都不整体清零，未写哈希字段仍保留原值；公共入口负责本轮清零。诊断不得与 state、function 或其池重叠。无分配、释放、输入改写或持久借用，扫描期间输入不得并发变化。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:80：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:88：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:90：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:93：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:96：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_deopt_validation.c:50：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:98：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:100：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_validate_deopt_values( const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在分析或附带 map 验证之前检查每条 deopt state 的普通 reconstruction 选区，拒绝指向逻辑 deoptValueCount 之外的范围及不属于函数 value 表的 ID，包括没有指令引用的 state。
 *
 * ## Parameters
 * @param[in] function 非空函数；deoptStates/deoptValues 及相关 count/capacity/非空关系已经预检，实际读取的存储在本调用期间有效。
 * @param[in,out] diagnostic 可为空；非空为独立可写诊断，失败由 deopt diagnostic helper 覆盖其负责字段，成功本助手不写。
 *
 * ## Returns
 * @return 按 state 数组顺序遇到的第一条非法范围或 valueId 导致 ZR_FALSE；所有选区及其中 ID 均满足逻辑界限时为 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 选区先通过减法界限检查再形成终点；可接受逻辑末尾的空区间，capacity 中尚未发布的空间不是有效内容。范围失败时按 start 是否越界选择 expected/actual，不依赖溢出的 start+count。只检查 1..valueCount 的普通 ID，不证明 SSA availability、owner/liveness、deopt state ID 唯一性或 aggregate 图语义；aggregate 由紧随其后的专用 validator 负责。只读借用、不分配、不释放或修改任何池，失败仅影响诊断；输入不得并发修改或释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:104：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:108：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:112：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:124：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:402：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_deopt_validation.c:59：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:68：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:89：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_validation.c:100：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_exception_result_terminator( const SZrExecIrFunction *function, const SZrExecIrBlock *block, const SZrExecIrInstruction *instruction, const SZrExecIrOpcodeInfo *info, TZrExecIrInstructionId instructionId, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 对能够抛出且产生结果的 terminator 约束 normal/exception 两条有序边，使结果的普通继续路径与异常路径具有明确区分；这是结构验证的一部分。
 *
 * ## Parameters
 * @param[in] function 稳定的非空函数，所读 block/successor 池和区间及 block ID 已通过通用预检。
 * @param[in] block 包含该 terminator 的可读 block，所属后继 range 使用函数的 successors 池。
 * @param[in] instruction 可读的候选末指令，其 resultRange 和 successorRange 已通过逻辑范围检查。
 * @param[in] info OpcodeInfo 借用；可为空，或 schema 未标记 TERMINATOR/MAY_THROW 时本助手直接跳过。
 * @param[in] instructionId 调用方给出的从 1 开始的末指令 ID，用于失败诊断，不在此核对与指针的对应关系。
 * @param[in,out] diagnostic 可为空；非空须与全部读取输入分离，失败局部填写 EXCEPTION_EDGE。
 *
 * ## Returns
 * @return 不适用该规则（info 为空、schema 不是可抛 terminator、或没有结果）时为 ZR_TRUE；适用时，块及所选指令边均恰有两条、两序列相同、normal 与 exception 目标不同且标记相符才返回 ZR_TRUE，否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 指令 successorRange 非空时使用它，否则暂时借用 block 的后继范围；上层之后仍单独核对指令与 block 后继数量及逐 ordinal 一致，fallback 不免除该后置规则。ordinal 0 的目标不得带 EXCEPTION 标志，ordinal 1 的目标必须带该标志。本助手不建立 SSA availability、实际异常对象或运行时控制流证明；前置 ID/range 检查是索引目标 block 的必要条件。无分配、释放或输入修改，诊断 setter 不整体清零；所有借用仅持续到调用返回。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:136：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:147：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:154：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:157：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:170：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:852：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:861：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_builder_iterator_invokes.c:250：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_iterator_invokes.c:258：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_block_has_only_exception_predecessors( const SZrExecIrFunction *function, const SZrExecIrBlock *block)
 *
 * ## Summary
 * 为 EXCEPTION_PAYLOAD 的处理器入口检查每个已列出的前驱是否以异常边到达该 block，要求至少一个前驱，避免把零前驱或普通控制流入口视为异常来源。
 *
 * ## Parameters
 * @param[in] function 非空只读函数，其所有 blocks、instruction 与前驱/后继池的 range 和 ID 已预检；OpcodeInfo 能安全查询。
 * @param[in] block 待检查 block，predecessorRange 位于函数前驱池内，id 对应当前目标。
 *
 * ## Returns
 * @return 前驱非空且每条前驱末指令均为可抛 terminator，并满足 THROW 的单一异常目标或其它可抛 terminator 的第二条目标等于 block.id 时为 ZR_TRUE；任一不符或没有前驱为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 前驱指令区间为空会拒绝；优先使用非空指令 successorRange，否则使用前驱 block 后继。THROW 分支必须恰好一条边，不能按通用两边分支处理；其它 opcode 必须恰好两条且 ordinal 1 命中。此谓词不写诊断、不证明全部 CFG 对称性/重复边 multiplicity 或运行时可达性；其调用发生在所属 block 的双向边匹配之后，通用预检已使跨 block 索引可读。只读且无分配、释放、保活或持久借用，调用期间输入稳定。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:190：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:209：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:217：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:222：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:225：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:230：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:239：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:262：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_builder_control_edges.c:754：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_exception_payload( const SZrExecIrFunction *function, const SZrExecIrBlock *block, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在结构验证中将 EXCEPTION_PAYLOAD 限制为 handler 内的局部定义：不能位于函数入口或 ENTRY block，block 必须标记 EXCEPTION、只由异常边进入，并且同一 block 最多一条 payload。
 *
 * ## Parameters
 * @param[in] function 通用预检后的非空稳定函数，池/range/ID 可读；这里只借用描述符。
 * @param[in] block 当前结构验证 block，其指令及前驱选区有效。
 * @param[in,out] diagnostic 可为空；非空独立可写，失败写 EXCEPTION_EDGE、当前指令和 block，并明确使用该指令 sourceId。
 *
 * ## Returns
 * @return 没有 payload 或每个遇到的 payload 均满足入口、标志、前驱与数量规则时为 ZR_TRUE，第一条违反规则的 payload 导致 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 本助手不要求每个 handler 必须有 payload；只对实际出现的 opcode 计数。短路检查先看入口和 EXCEPTION 标志，再检查异常前驱，最后检查已遇到数量；进入失败分支后，诊断重新按 payloadCount > 1 选择数量上界 1/实际 count，否则选择 EXCEPTION 标志/实际 flags。诊断字段表达这个复合拒绝规则，不能仅凭 expected/actual 反推全部失败条件或其检查顺序。普通 opcode 在此跳过，operand/result arity 和 ID 已由通用预检检查，SSA 使用规则由后续阶段负责。无分配、释放、输入修改或持久借用；失败只更新诊断字段，输出须与输入分离。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:242：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:255：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:259：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:264：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:274：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:821：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_builder_control_edges.c:673：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_control_edges.c:690：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_control_edges.c:730：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_control_edges.c:754：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn static TZrBool zr_exec_ir_value_range_is_valid(const SZrExecIrFunction *function, SZrExecIrRange range, TZrUInt32 count, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为指令 operand 选区检查逻辑范围及普通 valueId，先拒绝越界再逐元素读取；该助手实际只从 function->operands 取值，不能作为任意侧池的通用元素校验器。
 *
 * ## Parameters
 * @param[in] function 有效只读函数；实际调用前已检查 operand 池形状，其 valueCount 是合法 ID 的上界。
 * @param[in] range 按值传入的 operand start/count。
 * @param[in] count 该 operand 池的逻辑已占用数；现有调用传 function->operandCount，非 capacity。
 * @param[in,out] diagnostic 可为空；非空独立可写，失败写 INVALID_RANGE 或 INVALID_VALUE，本助手不整体清零。
 *
 * ## Returns
 * @return 选区符合 count 且其中每个 operand ID 属于 1..function->valueCount 时为 ZR_TRUE，否则遇到第一处错误返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 非法范围诊断的 actualVersion 以无符号 start+count 表示，可能回绕；它不参与是否接受的判定。合法范围下 operands 为空会把读取值视为 INVALID，非空地址的真实可读性仍是调用前提；空合法区间不读取元素。此处不检查 values[id-1].id、定义/支配关系或 ownership。range/ID 失败诊断使用 instructionId/blockId 0；上层 operand arity 分支仅在 diagnostic.code 仍为 NONE 时补充当前位置，不覆盖本助手已写的错误。无分配、释放、池修改或持久借用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:283：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:289：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:296：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:300：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:303：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:606：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:610：本文件的定义或配套验证规则。
 */

/**
 * @fn static TZrBool zr_exec_ir_validate_function(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为模块轻量验证和所有函数验证 level 提供共同的无分析分配预检：按固定顺序检查函数身份、逻辑池形状、binding rows、value 元数据、deopt reconstruction、侧表 ID/选区、block 描述符以及 instruction schema、标志、operand/result 引用。
 *
 * ## Parameters
 * @param[in] function 待验证函数，可为空并返回失败；非空描述符及所有声称存在的记录/池必须可读。count/capacity/NULL 判断不认证任意地址的真实分配范围。
 * @param[in,out] diagnostic 可为空；非空是与函数、所有池及 stateMap 独立的可写诊断。最初形状失败使用局部 setter；形状通过后 binding-row validator 会自行清零诊断。
 *
 * ## Returns
 * @return 任一按源码顺序遇到的前置条件、侧表或指令检查失败即为 ZR_FALSE；本层所有预检通过为 ZR_TRUE，不等价于完整 CFG/SSA/effect/stateMap 或模块常量语义已验证。
 *
 * ## Constraints and lifecycle
 * 先检查非零 function.id/functionToken、各已列池 count<=capacity 与非零 count 的非空指针及 frame slots 形状；之后依次是 binding rows、可选 entryBlockId 上界、value 的顺序 ID/ownership/nullability/flags、普通 deopt 值及 aggregate recipes、全前驱与后继 ID、共享 phiIncoming 域、phi records、非零 memory token、blocks、instructions。entryBlockId 为 INVALID 在本层可保留，STRUCTURE/SSA 阶段另有规则。块 ID/value ID 使用数组位置加 1；PROMOTABLE_PLACE 必须伴随 PLACE_ADDRESS。
 * 本层检查所有已发布 predecessor/successor、phiIncoming、phi、memory token 记录，但普通 operands/results 的元素 ID 仅在实际 instruction 选区中逐个检查；不能写成全部未引用侧表内容都已认证。gcMap/gcRoot/sourceMap 与 frame slots 在此主要检查已列形状，不建立 payload 活性、运行时类型、GC 保活或完整执行契约证明。COMPARE/TYPE_TEST、memory phi 与 guarded cleanup 的特殊规则各有独立块说明。
 * 本层及所调用的 binding-row、deopt recipe、conditional-cleanup 预检不取得分析工作区，不分配/释放函数资源，也不改写函数或其池；失败不会修复输入。局部 setter 不整体清零，专用 callee 可能重建诊断，首失败按实际顺序决定输出。所有借用在调用期间必须稳定，诊断别名不受检查，不建立锁或保活。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:318：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:322：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:366：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:380：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:402：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:432：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:483：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:549：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:605：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:639：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:120：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:175：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:547：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:582：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_value_validation.c:210：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_value_validation.c:234：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par zr_exec_ir_validate_function::shared_phi_incoming_value_domains（所属函数内的独立契约块）
 *
 * ## Summary
 * 共享 phiIncoming 池同时承载普通 value、effect token 和分 region 的 memory token；在逐项 ID 检查时先由 block 的声明选区分类，避免把 token 数值误当成普通 valueId。
 *
 * ## Constraints and lifecycle
 * 所有项的 predecessor 都必须是合法 block ID；只有不属于 effect 或 memory 选区的项才要求 value 在 1..valueCount。分类本身不认证选区合法或互斥，后面的 block 检查才核对范围与 result/count 关系。错误分类优先 memory、其次 effect、最后普通 value；token 域 expectedVersion 为 UINT32_MAX，并非已证明任意 token 值语义有效。该顺序不建立 SSA 或 effect 流证明。块不分配、修改或保存输入，诊断沿用 owner 的独立输出约束。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:432：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:433：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:434：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:435：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:441：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:490：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_core_model.c:784：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par zr_exec_ir_validate_function::memory_phi_region_tag_invariants（所属函数内的独立契约块）
 *
 * ## Summary
 * 把每个 block 的固定 memory class 槽与其 memory phi result 的编码绑定，先核 incoming 逻辑范围和空/INVALID 对应关系，再核标签、region 与非零 version，供后续 memory effect 校验安全解释。
 *
 * ## Constraints and lifecycle
 * 每个 class 的 result 为 INVALID 当且仅当 incoming count 为 0；非 INVALID result 必须 tagged、解出的 region 恰等于当前槽且 version 非零。这里不校验每个 incoming token 的 flow、predecessor 对齐、唯一性或版本单调性，也不把先前全 memoryTokenPool 的非零检查写成完整 tag 验证。错误先由 range/presence 分支拒绝，再进入编码分支；块只读，不建立 root 或资源所有权。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:503：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:506：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:508：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:519：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:521：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:525：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:87：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:90：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:93：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par zr_exec_ir_validate_function::opcode_specific_metadata_domains（所属函数内的独立契约块）
 *
 * ## Summary
 * 在已识别 opcode 后限定两个复用标量字段的专用域：COMPARE 的 typeToken 是已知比较 selector；TYPE_TEST 的 matchTypeToken 是非零 canonical match 身份，而其它 opcode 不得携带隐藏的 match 元数据。
 *
 * ## Constraints and lifecycle
 * COMPARE 仅要求 selector 不大于 NOT_EQUAL，按当前从 0 连续的枚举定义覆盖该已知域；不在这里解析普通类型。TYPE_TEST 要求 matchTypeToken 非零，其它 opcode 要求为 0，本层不认证非零 token 是否在外部类型库存在。检查先于 instruction flags/ranges/arity 及 SSA/effect 阶段，因此错误诊断可在这些后续问题之前出现。只读记录并写可选诊断，expectedVersion 分别是 selector 上界或 1/0 需求。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:564：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:572：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:27：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:32：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:267：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_compare_predicate_domain.c:191：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_cfg.c:586：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_builder_cfg.c:593：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par zr_exec_ir_validate_function::guarded_cleanup_site_precondition（所属函数内的独立契约块）
 *
 * ## Summary
 * 把 DROP_IF_INITIALIZED 的 owner/定义/cleanup-block 前提放入所有验证 level 共同经过的指令预检，避免仅靠合法 operand ID 就接纳无法关联初始化标志的 guarded drop。
 *
 * ## Constraints and lifecycle
 * Opcode 已知、instruction range/arity 与 operand valueId 已在前面检查；专用 ConditionalCleanupValid 继续要求 UNIQUE/SHARED operand、EXTERNAL_ENTRY 标志或实际 instruction/phi 定义以及包含当前指令的 cleanup block。它扫描首个包含块，并不自行证明完整 SSA 支配关系或执行时初始化标志。专用谓词失败时本块写 INVALID_VALUE、当前 instructionId/sourceId，blockId 为 0。即使没有选择 SSA/EFFECT 位也会执行；无分配、IR 修复或 owner 转移。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:632：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:633：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:636：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:35：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:55：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:59：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:64：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_conditional_cleanup.c:558：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_conditional_cleanup.c:566：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_conditional_cleanup.c:573：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ValidateModule(const SZrExecIrModule *module, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查模块池形状、函数数组身份和每个函数的通用预检，供需要安全遍历 IR 的模块分析/变换使用。它与执行全部验证阶段的 VerifyModule 分层，调用成功不表示 CFG/SSA/effect、附带 stateMap 或 owned constants 语义均有效。
 *
 * ## Parameters
 * @param[in] module 待验证模块，可为空并被拒绝；非空记录、逻辑 functions/constants/layouts/sourceMaps 池须在实际读取期间有效稳定。
 * @param[out] diagnostic 可为空；非空时本轮先整体清零，失败写首个可报告错误。必须与 module、其 functions、所有相关池和 stateMap 分离。
 *
 * ## Returns
 * @return 模块形状、functions[index].id == index+1 和每个函数通用预检都通过时为 ZR_TRUE；遇到第一项错误返回 ZR_FALSE，空的形状合法模块可以成功。
 *
 * ## Constraints and lifecycle
 * 先清诊断再检查 module；模块层检查四类 count/capacity 与非零 count/NULL 关系，不在此验证 module.id/moduleToken/contract 的全部内容，也不认证实际内存分配。函数 ID 不连续时使用现有 INVALID_BLOCK 诊断码，不把该字段命名推成函数验证的另一 API。函数按数组顺序预检，binding rows/deopt 等可在后续结构分析之前拒绝。
 * 本层及共同预检无动态分析分配，不转移、释放、缓存或修改模块/函数资源；FALSE 后输入保持调用前内容，诊断可变且不提供跨失败事务恢复。输入是只读借用，实际对象与池不得在验证期间并发修改或释放；本层不建立同步或 GC 根。需要完整验证的调用方继续使用 VerifyModule，仓外公共 API 消费者无法由仓内搜索穷尽。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:658：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:662：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:663：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:681：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:691：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:690：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c:1612：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c:816：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:547：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:582：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_VerifyModule(const SZrExecIrModule *module, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在轻量模块预检全部成功之后，按函数数组顺序执行 VerifyFunction(ALL)，再检查该函数 CONSTANT 指令与模块 owned constant pool 的引用/类型关系，作为 Parser、artifact 与 hotpatch 的模块级校验入口。
 *
 * ## Parameters
 * @param[in] module 只读模块借用；可为空并由前置 ValidateModule 拒绝，所读函数、池及 stateMap 在全调用期间保持有效稳定。
 * @param[out] diagnostic 可为空；非空为独立可写记录，ValidateModule 及后续函数入口会开始各自诊断轮次，失败保留实际首先拒绝阶段的结果。
 *
 * ## Returns
 * @return 前置模块预检及每个函数的 ALL 验证和 owned-constant 检查均通过时为 ZR_TRUE；任一阶段失败立即为 ZR_FALSE，后面的函数和阶段不再执行。
 *
 * ## Constraints and lifecycle
 * 顺序是先验证所有函数的通用形状，再对每个函数依次完整验证与常量交叉检查，不能把调用位置之后的阶段视为已运行。module.constantCount 为 0 时专用 owned-constant helper 有意跳过，不要求外部 Oracle 常量池必须成为模块 owned pool；单函数 verifier 本身没有模块常量上下文。
 * 本函数不改写或取得 module/function/stateMap 的所有权，但 SSA dominance、effect backedges、附带 map 的 owner/liveness 等下层可能分配工作区，FALSE 可能是分配失败；各下层负责其临时资源清理，调用方仍拥有原模块和池。本函数不缓存分析、不封存输入、不执行 IR、也不认证任意指针可读性或所有 ABI/hash 契约。诊断不得与任何输入重叠；输入不能并发修改、重分配或释放，并发只读验证须独立诊断且遵守下层分配器/测试钩子的同步约束。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:698：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:702：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:706：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:711：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:34：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:692：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c:630：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c:408：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:85：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:118：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:120：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:496：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_VerifyFunction(const SZrExecIrFunction *function, EZrExecIrVerifyLevel level, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 统一函数级验证顺序：先清本轮诊断并执行共同预检，再按 level 选择 STRUCTURE、SSA、EFFECT 阶段，最后只要函数附带 stateMap 就验证其与当前 IR 的一致性。该入口被 builder、pass 边界、分析、seal 和模块验证共同复用。
 *
 * ## Parameters
 * @param[in] function 只读函数借用；NULL 返回 FALSE，非空描述符与全部实际读取池和附带 map 在调用期间有效稳定。
 * @param[in] level 非零且只含 ZR_EXEC_IR_VERIFY_ALL 中的已知位；各位选择附加阶段，不关闭共同预检或已有 map 校验。
 * @param[out] diagnostic 可为空；非空先整体清零，再由首个失败阶段填写标量结果，必须与 function、所有池及 stateMap 分离。
 *
 * ## Returns
 * @return 无效 function/level、共同预检、所选阶段或附带 map 的任一检查失败时立即返回 ZR_FALSE；全都通过返回 ZR_TRUE。TRUE 表示所选静态契约通过，不表示 IR 已执行、被 seal、已建立 GC 根或模块 owned constants 已验证。
 *
 * ## Constraints and lifecycle
 * 共同预检始终先运行。STRUCTURE 要求有 block 时存在 entryBlockId，按 block 顺序校验两侧 CFG occurrence、payload、非空块末 terminator/无提前 terminator，以及 block 与末指令后继的有序一致性；空 instructionRange 跳过末指令规则。其后按固定次序运行 SSA 再 EFFECT，未选择的阶段不执行；已有 stateMap 的校验位于这些阶段之后且不受 level 选择免除。旧 map 不匹配当前 IR 时调用会失败，builder 如需重建必须按自身契约明确构造去掉旧 map 的验证视图，不能假定本入口替它丢弃或更新旧 map。
 * 本层不分配或释放 function/pools/stateMap，不修改或缓存 IR；委托的 SSA/effect/map 分析可能分配临时工作区并返回 OOM，连 STRUCTURE-only 携带 map 时也不能保证无分配，各 callee 负责清理其临时对象。失败保留原 IR；没有对诊断输出别名的事务保证。输入不得并发修改、重分配或释放；本层不提供锁、保活或原子快照，并发只读使用独立诊断且遵守下层设施约束。诊断只提供实际首失败阶段，不能由后续断言意图推断下游已经到达。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:719：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:724：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:725：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:740：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:898：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:904：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909：本文件的定义或配套验证规则。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:538：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:694：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:935：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:858：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:690：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/ssa_state_map_verifier_cases.inc:11：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_aggregates.c:534：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par ZrCore_ExecIr_VerifyFunction::cfg_edge_occurrence_reciprocity（所属函数内的独立契约块）
 *
 * ## Summary
 * STRUCTURE 阶段按边在同一相邻 block 中出现的 ordinal 匹配反向表，要求 predecessor 与 successor 的重复 occurrence 成对存在；不以只找到一个相同目标的集合成员测试替代多重边身份。
 *
 * ## Constraints and lifecycle
 * 共同预检已建立所有 block ID 与两侧逻辑区间可索引。逐 predecessor 检查相应前驱 block 的 successor，再逐 successor 检查相应目标的 predecessor，任何一侧缺少对应 occurrence 都拒绝。匹配的重复边允许存在，不在此要求 CFG 为简单图，也不证明边顺序等于 terminator successor 顺序；后者由后面的独立一致性块检查。直接调用私有 edge-occurrence helper，不增加该只读依赖的定义信用；无分析分配或持久资源。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:765：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:782：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:810：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_edge_identity.h:7：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_edge_identity.h:15：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_edge_identity.h:22：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:695：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:706：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:719：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:730：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:751：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par ZrCore_ExecIr_VerifyFunction::terminator_successor_coherence（所属函数内的独立契约块）
 *
 * ## Summary
 * 对非空 block 让最后一条指令成为唯一末 terminator，再把它的 successor 列表与 block CFG 按数量和 ordinal 逐项绑定，并核对可选的 terminatorInstructionId。
 *
 * ## Constraints and lifecycle
 * 先拒绝末指令之前出现的 schema terminator，再核末指令是 terminator，接着检查有结果的可抛 terminator 的 normal/exception 约束，然后比较 successor 数量、逐 ordinal 内容，最后检查非 INVALID 的 terminatorInstructionId 是否等于实际末指令 ID。这个顺序决定首诊断；例如不能把后来数量检查的意图当成已经执行。instructionRange 为空时整块跳过，不因此单独报告 missing terminator；ID 为 INVALID 时不要求补写。共同预检保证 opcode/ranges 可读，本块不改变 IR、分配资源或建立运行时控制流证明。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:825：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:834：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:842：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:852：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:861：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:875：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:883：本文件的定义或配套验证规则。
 * - tests/parser/test_ssa_core_model.c:755：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_core_model.c:816：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */

/**
 * @par ZrCore_ExecIr_VerifyFunction::attached_state_map_after_selected_levels（所属函数内的独立契约块）
 *
 * ## Summary
 * 在所有被选择的验证阶段成功后，仍要求已有 stateMap 描述当前函数；level 不提供忽略旧 map 的开关，map 是否存在决定是否进入这个跨 owner/liveness 的验证边界。
 *
 * ## Constraints and lifecycle
 * stateMap 为 NULL 时跳过；非空时调用 ValidateStateMap，失败原样返回 FALSE，不丢弃、释放、替换或修复旧 map。该 callee 重建诊断并可能为 owner/liveness 分配临时空间，自己在成功或失败路径释放临时分析；因此只选 STRUCTURE 也不能对携带 map 的函数宣称无分配。重建 map 的 Parser builder 显式使用 stateMap=NULL 的浅验证视图，但本入口不替其它调用者建立此视图。输入与 map 是稳定只读借用，diagnostic 独立；旧 map 引起的失败不等于生产 verifier 的缺陷。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:910：本文件的定义或配套验证规则。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:565：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/ssa_state_map_verifier_cases.inc:11：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/ssa_state_map_verifier_cases.inc:30：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/ssa_state_map_verifier_cases.inc:63：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 * - tests/parser/test_ssa_deopt_aggregates.c:534：只读调用、声明、依赖或有限测试断言证据，按正文所述边界使用。
 */
