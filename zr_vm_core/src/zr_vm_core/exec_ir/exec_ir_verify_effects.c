#include "zr_vm_core/exec_ir.h"

#include <string.h>

static void zr_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic,
                                   EZrExecutionDiagnosticCode code,
                                   const SZrExecIrFunction *function,
                                   TZrUInt32 blockId,
                                   TZrUInt32 instructionId,
                                   TZrUInt32 expected,
                                   TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = instructionId != 0u && instructionId <= function->instructionCount
                               ? function->instructions[instructionId - 1u].sourceId
                               : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrExecIrBlockId zr_exec_ir_containing_block(const SZrExecIrFunction *function,
                                                    TZrUInt32 instructionIndex) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (instructionIndex >= block->instructions.start &&
            instructionIndex - block->instructions.start < block->instructions.count) {
            return block->id;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool zr_exec_ir_range_valid(SZrExecIrRange range,
                                      TZrUInt32 count,
                                      const void *pool) {
    if (range.start > count || range.count > count - range.start) {
        return ZR_FALSE;
    }
    if (range.count != 0u && pool == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_effect_storage_valid(const SZrExecIrFunction *function,
                                               SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;

    if ((function->valueCount != 0u && function->values == ZR_NULL) ||
        function->valueCount > function->valueCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        function->instructionCount > function->instructionCapacity ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        function->blockCount > function->blockCapacity ||
        (function->operandCount != 0u && function->operandPool == ZR_NULL) ||
        function->operandCount > function->operandCapacity ||
        (function->resultCount != 0u && function->resultPool == ZR_NULL) ||
        function->resultCount > function->resultCapacity ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        function->phiCount > function->phiCapacity ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        function->predecessorCount > function->predecessorCapacity ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        function->successorCount > function->successorCapacity) {
        zr_exec_ir_effect_diag(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               function,
                               ZR_EXEC_IR_BLOCK_ID_INVALID,
                               0u,
                               0u,
                               0u);
        return ZR_FALSE;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (!zr_exec_ir_range_valid(block->instructions,
                                    function->instructionCount,
                                    function->instructions) ||
            !zr_exec_ir_range_valid(block->predecessors,
                                    function->predecessorCount,
                                    function->predecessors) ||
            !zr_exec_ir_range_valid(block->successors,
                                    function->successorCount,
                                    function->successors) ||
            !zr_exec_ir_range_valid(block->phis,
                                    function->phiCount,
                                    function->phiPool)) {
            zr_exec_ir_effect_diag(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function,
                                   block->id,
                                   0u,
                                   0u,
                                   0u);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_block_has_predecessor(const SZrExecIrFunction *function,
                                                const SZrExecIrBlock *block,
                                                TZrExecIrBlockId predecessor) {
    TZrUInt32 index;
    for (index = block->predecessors.start;
         index < block->predecessors.start + block->predecessors.count;
         ++index) {
        if (function->predecessors[index] == predecessor) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_verify_phi_predecessors(const SZrExecIrFunction *function,
                                                  SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        if (!zr_exec_ir_range_valid(block->predecessors, function->predecessorCount,
                                    function->predecessors) ||
            !zr_exec_ir_range_valid(block->phis, function->phiCount, function->phiPool)) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function, block->id, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            if (!zr_exec_ir_range_valid(phi->incomings, function->phiIncomingCount,
                                        function->phiIncoming)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                       function, block->id, 0u, 0u, 0u);
                return ZR_FALSE;
            }
            if (phi->incomings.count != block->predecessors.count) {
                zr_exec_ir_effect_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                       function,
                                       block->id,
                                       0u,
                                       block->predecessors.count,
                                       phi->incomings.count);
                return ZR_FALSE;
            }
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start + phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[incomingIndex];
                TZrUInt32 duplicateIndex;
                TZrUInt32 predecessorIndex = incomingIndex - phi->incomings.start;
                if (!zr_exec_ir_block_has_predecessor(function, block, incoming->predecessor)) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                           function,
                                           block->id,
                                           0u,
                                           block->predecessors.count,
                                           incoming->predecessor);
                    return ZR_FALSE;
                }
                if (function->predecessors[block->predecessors.start + predecessorIndex] !=
                    incoming->predecessor) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                           function,
                                           block->id,
                                           0u,
                                           function->predecessors[block->predecessors.start + predecessorIndex],
                                           incoming->predecessor);
                    return ZR_FALSE;
                }
                for (duplicateIndex = phi->incomings.start;
                     duplicateIndex < incomingIndex;
                     ++duplicateIndex) {
                    if (function->phiIncoming[duplicateIndex].predecessor == incoming->predecessor) {
                        zr_exec_ir_effect_diag(diagnostic,
                                               ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                               function,
                                               block->id,
                                               0u,
                                               block->predecessors.count,
                                               incoming->predecessor);
                        return ZR_FALSE;
                    }
                }
            }
        }
    }
    return ZR_TRUE;
}

static TZrUInt16 zr_exec_ir_required_flags(const SZrExecIrOpcodeInfo *info,
                                           const SZrExecIrInstruction *instruction) {
    TZrUInt16 required = 0u;
    (void)instruction;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_THROW;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_GC;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    return required;
}

TZrBool ZrCore_ExecIr_VerifyEffects(const SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrMemoryTokenId latestMemory = ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
    TZrExecIrEffectTokenId latestEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (!zr_exec_ir_effect_storage_valid(function, diagnostic)) {
        return ZR_FALSE;
    }
    if (!zr_exec_ir_verify_phi_predecessors(function, diagnostic)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
        TZrExecIrBlockId blockId = zr_exec_ir_containing_block(function, index);
        TZrUInt16 required;
        TZrUInt32 tokenIndex;
        TZrBool observable;
        if (info == ZR_NULL) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                   function, blockId, index + 1u, 0u, instruction->opcode);
            return ZR_FALSE;
        }
        required = zr_exec_ir_required_flags(info, instruction);
        if ((instruction->flags & required) != required) {
            zr_exec_ir_effect_diag(diagnostic,
                                   (required & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u
                                       ? ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE
                                       : ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                   function, blockId, index + 1u, required, instruction->flags);
            return ZR_FALSE;
        }
        if (!zr_exec_ir_range_valid(instruction->memoryIn, function->memoryTokenCount,
                                    function->memoryTokenPool) ||
            !zr_exec_ir_range_valid(instruction->memoryOut, function->memoryTokenCount,
                                    function->memoryTokenPool) ||
            !zr_exec_ir_range_valid(instruction->operands,
                                    function->operandCount,
                                    function->operandPool) ||
            !zr_exec_ir_range_valid(instruction->results,
                                    function->resultCount,
                                    function->resultPool) ||
            !zr_exec_ir_range_valid(instruction->phiRange,
                                    function->phiIncomingCount,
                                    function->phiIncoming) ||
            !zr_exec_ir_range_valid(instruction->successorRange,
                                    function->successorCount,
                                    function->successors)) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function, blockId, index + 1u, 0u, 0u);
            return ZR_FALSE;
        }
        if (info->memoryReads != 0u && instruction->memoryIn.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        if (info->memoryWrites != 0u && instruction->memoryOut.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        for (tokenIndex = instruction->memoryIn.start;
             tokenIndex < instruction->memoryIn.start + instruction->memoryIn.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (token == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
                (tokenIndex > instruction->memoryIn.start &&
                 token < function->memoryTokenPool[tokenIndex - 1u]) ||
                (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID && token < latestMemory)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, blockId, index + 1u, latestMemory, token);
                return ZR_FALSE;
            }
            latestMemory = token;
        }
        for (tokenIndex = instruction->memoryOut.start;
             tokenIndex < instruction->memoryOut.start + instruction->memoryOut.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (token == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
                (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID && token <= latestMemory)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, blockId, index + 1u,
                                       latestMemory == UINT32_MAX ? latestMemory : latestMemory + 1u,
                                       token);
                return ZR_FALSE;
            }
            if (token > latestMemory) latestMemory = token;
        }
        observable = (TZrBool)(info->memoryWrites != 0u || required != 0u ||
                               (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u);
        if (observable) {
            if (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut <= instruction->effectIn ||
                (latestEffect != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                 instruction->effectIn < latestEffect)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                       function, blockId, index + 1u, latestEffect,
                                       instruction->effectIn);
                return ZR_FALSE;
            }
            latestEffect = instruction->effectOut;
        }
    }
    return ZR_TRUE;
}
