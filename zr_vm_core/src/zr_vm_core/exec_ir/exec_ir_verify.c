#include "zr_vm_core/exec_ir.h"

#include "exec_ir_verify_ssa.h"

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
    diagnostic->blockId = blockId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrBool zr_exec_ir_range_is_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

/* Only call after validating both the side-pool range and every block ID. */
static TZrBool zr_exec_ir_edge_contains(const TZrExecIrBlockId *edges,
                                        SZrExecIrRange range,
                                        TZrExecIrBlockId blockId) {
    TZrUInt32 index;
    for (index = range.start; index < range.start + range.count; ++index) {
        if (edges[index] == blockId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
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
            function->values[index].ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
            function->values[index].nullability >= ZR_EXEC_IR_NULLABILITY_COUNT ||
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
        if (function->phiIncoming[index].predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->phiIncoming[index].predecessor > function->blockCount ||
            function->phiIncoming[index].value == ZR_EXEC_IR_VALUE_ID_INVALID ||
            function->phiIncoming[index].value > function->valueCount) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                      function,
                                      0u,
                                      function->phiIncoming[index].predecessor,
                                      function->valueCount,
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
            !zr_exec_ir_range_is_valid(block->phis, function->phiCount)) {
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                      function,
                                      0u,
                                      block->id,
                                      index + 1u,
                                      block->id);
            return ZR_FALSE;
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
                    if (!zr_exec_ir_edge_contains(function->successors,
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
                    if (!zr_exec_ir_edge_contains(function->predecessors,
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
    return ZR_TRUE;
}
