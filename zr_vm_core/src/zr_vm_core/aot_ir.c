#include "zr_vm_core/aot_ir.h"

#include <string.h>

static void aot_ir_diag_clear(SZrAotIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static EZrAotIrStatus aot_ir_fail(SZrAotIrDiagnostic *diagnostic,
                                  EZrAotIrStatus status,
                                  TZrUInt32 functionId,
                                  TZrUInt32 blockId,
                                  TZrUInt32 instructionId,
                                  TZrUInt32 index,
                                  TZrUInt64 expected,
                                  TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->functionId = functionId;
        diagnostic->blockId = blockId;
        diagnostic->instructionId = instructionId;
        diagnostic->index = index;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

static TZrBool aot_ir_range_valid(SZrAotIrRange range, TZrUInt32 length) {
    return (TZrBool)(range.offset <= length && range.count <= length - range.offset);
}

static TZrBool aot_ir_alignment_valid(TZrUInt32 alignment) {
    return (TZrBool)(alignment != 0u && (alignment & (alignment - 1u)) == 0u);
}

static TZrBool aot_ir_opcode_is_terminator(TZrUInt32 opcode) {
    switch ((EZrExecIrOpcode)opcode) {
        case ZR_EXEC_IR_OPCODE_BRANCH:
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
        case ZR_EXEC_IR_OPCODE_SWITCH:
        case ZR_EXEC_IR_OPCODE_INVOKE:
        case ZR_EXEC_IR_OPCODE_THROW:
        case ZR_EXEC_IR_OPCODE_SUSPEND:
        case ZR_EXEC_IR_OPCODE_RETURN:
        case ZR_EXEC_IR_OPCODE_ITER_INIT:
        case ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT:
        case ZR_EXEC_IR_OPCODE_ITER_CURRENT:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 aot_ir_required_instruction_flags(TZrUInt32 opcode) {
    switch ((EZrExecIrOpcode)opcode) {
        case ZR_EXEC_IR_OPCODE_DIV:
        case ZR_EXEC_IR_OPCODE_STORE:
        case ZR_EXEC_IR_OPCODE_THROW:
            return ZR_EXEC_IR_FLAG_MAY_THROW;
        case ZR_EXEC_IR_OPCODE_CALL:
        case ZR_EXEC_IR_OPCODE_INVOKE:
        case ZR_EXEC_IR_OPCODE_ITER_INIT:
        case ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT:
        case ZR_EXEC_IR_OPCODE_ITER_CURRENT:
            return ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
        case ZR_EXEC_IR_OPCODE_ALLOC:
            return ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_GC;
        case ZR_EXEC_IR_OPCODE_BARRIER:
            return ZR_EXEC_IR_FLAG_MAY_GC;
        case ZR_EXEC_IR_OPCODE_SUSPEND:
            return ZR_EXEC_IR_FLAG_MAY_SUSPEND;
        default:
            return 0u;
    }
}

static TZrBool aot_ir_block_id_exists(const SZrAotIrFunction *function,
                                      TZrUInt32 blockId) {
    for (TZrUInt32 index = 0u; index < function->blockCount; ++index) {
        if (function->blocks[index].id == blockId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt32 aot_ir_edge_occurrences(const SZrAotIrFunction *function,
                                          SZrAotIrRange range,
                                          TZrUInt32 targetBlockId) {
    TZrUInt32 count = 0u;
    for (TZrUInt32 index = 0u; index < range.count; ++index) {
        if (function->successorPool[range.offset + index] == targetBlockId) {
            ++count;
        }
    }
    return count;
}

static const SZrAotIrBlock *aot_ir_block_for_instruction(
        const SZrAotIrFunction *function, TZrUInt32 instructionIndex) {
    for (TZrUInt32 index = 0u; index < function->blockCount; ++index) {
        const SZrAotIrBlock *block = &function->blocks[index];
        if (instructionIndex >= block->instructions.offset &&
            instructionIndex - block->instructions.offset < block->instructions.count) {
            return block;
        }
    }
    return ZR_NULL;
}

static TZrUInt64 aot_ir_hash_byte(TZrUInt64 hash, TZrUInt8 byte) {
    return (hash ^ byte) * UINT64_C(1099511628211);
}

static TZrUInt64 aot_ir_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    for (TZrUInt32 i = 0u; i < 4u; ++i) {
        hash = aot_ir_hash_byte(hash, (TZrUInt8)(value >> (i * 8u)));
    }
    return hash;
}

static TZrUInt64 aot_ir_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    for (TZrUInt32 i = 0u; i < 8u; ++i) {
        hash = aot_ir_hash_byte(hash, (TZrUInt8)(value >> (i * 8u)));
    }
    return hash;
}

static TZrUInt64 aot_ir_hash_contract(TZrUInt64 hash,
                                      const SZrExecutionContract *contract) {
    hash = aot_ir_hash_u32(hash, contract->schemaVersion);
    hash = aot_ir_hash_u32(hash, contract->abiVersion);
    hash = aot_ir_hash_u32(hash, contract->logicalVersion);
    hash = aot_ir_hash_u64(hash, contract->generation);
    hash = aot_ir_hash_u32(hash, contract->targetToken);
    hash = aot_ir_hash_u64(hash, contract->signatureHash);
    hash = aot_ir_hash_u64(hash, contract->layoutHash);
    hash = aot_ir_hash_u64(hash, contract->moduleHash);
    hash = aot_ir_hash_u32(hash, contract->requiredCapabilities);
    return aot_ir_hash_u32(hash, contract->declaredEffects);
}

static EZrAotIrStatus aot_ir_validate_function(const SZrAotIrModule *module,
                                               const SZrAotIrFunction *function,
                                               TZrUInt32 functionIndex,
                                               SZrAotIrDiagnostic *diagnostic) {
    const TZrUInt32 knownBlockFlags =
            ZR_EXEC_IR_BLOCK_FLAG_ENTRY | ZR_EXEC_IR_BLOCK_FLAG_COLD |
            ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    TZrUInt32 entryBlockCount = 0u;
    if (function->id == ZR_AOT_IR_ID_INVALID || function->functionToken == 0u ||
        function->signatureHash == 0u || function->frameLayout.layoutHash == 0u ||
        !aot_ir_alignment_valid(function->frameLayout.frameByteAlign) ||
        function->blocks == ZR_NULL || function->instructions == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, function->id, 0u, 0u,
                           functionIndex, 1u, 0u);
    }
    if (function->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION,
                           function->contract.schemaVersion);
    }
    if (function->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_ABI_VERSION,
                           function->contract.abiVersion);
    }
    if (function->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION,
                           function->contract.logicalVersion);
    }
    if (function->contract.signatureHash != function->signatureHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->signatureHash,
                           function->contract.signatureHash);
    }
    if (function->contract.layoutHash != function->frameLayout.layoutHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.layoutHash,
                           function->contract.layoutHash);
    }
    if (function->contract.targetToken != function->functionToken) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->functionToken,
                           function->contract.targetToken);
    }
    if (function->contract.moduleHash != module->contract.moduleHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, module->contract.moduleHash,
                           function->contract.moduleHash);
    }
    if ((function->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           function->contract.requiredCapabilities);
    }
    if ((function->contract.declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_EFFECT_KNOWN_MASK,
                           function->contract.declaredEffects);
    }
    if (function->frameLayout.storageSlotCount > function->frameLayout.logicalSlotCount) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.logicalSlotCount,
                           function->frameLayout.storageSlotCount);
    }
    if (function->frameLayout.parameterPrefixBytes >
        function->frameLayout.returnAreaOffset) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.returnAreaOffset,
                           function->frameLayout.parameterPrefixBytes);
    }
    if (function->frameLayout.frameByteSize < function->frameLayout.returnAreaOffset ||
        (function->frameLayout.frameByteSize &
         (function->frameLayout.frameByteAlign - 1u)) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.frameByteAlign,
                           function->frameLayout.frameByteSize);
    }
    if (function->blockCount == 0u || function->instructionCount == 0u ||
        ((function->operandCount > 0u) && (function->operandPool == ZR_NULL)) ||
        ((function->resultCount > 0u) && (function->resultPool == ZR_NULL)) ||
        ((function->phiIncomingCount > 0u) && (function->phiIncomingPool == ZR_NULL)) ||
        ((function->successorCount > 0u) && (function->successorPool == ZR_NULL)) ||
        ((function->stateMapCount > 0u) && (function->stateMaps == ZR_NULL))) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, function->id, 0u, 0u,
                           functionIndex, 0u, 0u);
    }
    for (TZrUInt32 i = 0u; i < function->operandCount; ++i) {
        if (function->operandPool[i] == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u, 0u,
                               i, 1u, function->operandPool[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < function->resultCount; ++i) {
        if (function->resultPool[i] == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u, 0u,
                               i, 1u, function->resultPool[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < function->blockCount; ++i) {
        const SZrAotIrBlock *block = &function->blocks[i];
        if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u) {
            ++entryBlockCount;
        }
        if (block->id == ZR_AOT_IR_ID_INVALID ||
            (block->flags & ~knownBlockFlags) != 0u ||
            !aot_ir_range_valid(block->instructions, function->instructionCount) ||
            !aot_ir_range_valid(block->predecessors, function->successorCount) ||
            !aot_ir_range_valid(block->successors, function->successorCount)) {
            return aot_ir_fail(diagnostic,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? ZR_AOT_IR_INVALID_CFG
                                   : ZR_AOT_IR_INVALID_RANGE,
                               function->id, block->id, 0u, i,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? knownBlockFlags
                                   : function->instructionCount,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? block->flags
                                   : block->instructions.offset);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->blocks[j].id == block->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, block->id, 0u,
                                   i, j, i);
            }
        }
        if (block->terminatorInstructionId != ZR_AOT_IR_ID_INVALID) {
            TZrBool found = ZR_FALSE;
            TZrUInt32 terminatorOffset = 0u;
            const SZrAotIrInstruction *terminator = ZR_NULL;
            for (TZrUInt32 j = 0u; j < block->instructions.count; ++j) {
                if (function->instructions[block->instructions.offset + j].id ==
                    block->terminatorInstructionId) {
                    found = ZR_TRUE;
                    terminatorOffset = j;
                    terminator = &function->instructions[block->instructions.offset + j];
                    break;
                }
            }
            if (!found) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, block->id,
                                   block->terminatorInstructionId, i, 1u, 0u);
            }
            if (!aot_ir_opcode_is_terminator(terminator->opcode)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, terminator->id, i, 1u,
                                   terminator->opcode);
            }
            if (terminatorOffset != block->instructions.count - 1u) {
                const SZrAotIrInstruction *lastInstruction =
                        &function->instructions[block->instructions.offset +
                                                 block->instructions.count - 1u];
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, terminator->id, i, lastInstruction->id,
                                   terminator->id);
            }
        }
        for (TZrUInt32 j = 0u; j < block->successors.count; ++j) {
            TZrUInt32 target = function->successorPool[
                    block->successors.offset + j];
            if (!aot_ir_block_id_exists(function, target)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, 0u, j, function->blockCount, target);
            }
        }
        for (TZrUInt32 j = 0u; j < block->predecessors.count; ++j) {
            TZrUInt32 source = function->successorPool[
                    block->predecessors.offset + j];
            if (!aot_ir_block_id_exists(function, source)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, 0u, j, function->blockCount, source);
            }
        }
    }
    for (TZrUInt32 i = 0u; i < function->blockCount; ++i) {
        const SZrAotIrBlock *source = &function->blocks[i];
        for (TZrUInt32 j = 0u; j < function->blockCount; ++j) {
            const SZrAotIrBlock *target = &function->blocks[j];
            TZrUInt32 outgoing = aot_ir_edge_occurrences(
                    function, source->successors, target->id);
            TZrUInt32 incoming = aot_ir_edge_occurrences(
                    function, target->predecessors, source->id);
            if (outgoing != incoming) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   source->id, 0u, j, outgoing, incoming);
            }
        }
    }
    if (entryBlockCount != 1u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u, 0u,
                           functionIndex, 1u, entryBlockCount);
    }
    for (TZrUInt32 instructionIndex = 0u;
         instructionIndex < function->instructionCount; ++instructionIndex) {
        TZrUInt32 containingBlockCount = 0u;
        for (TZrUInt32 blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrAotIrBlock *block = &function->blocks[blockIndex];
            if (instructionIndex >= block->instructions.offset &&
                instructionIndex - block->instructions.offset < block->instructions.count) {
                ++containingBlockCount;
            }
        }
        if (containingBlockCount != 1u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u,
                               function->instructions[instructionIndex].id,
                               instructionIndex, 1u, containingBlockCount);
        }
    }
    for (TZrUInt32 i = 0u; i < function->instructionCount; ++i) {
        const SZrAotIrInstruction *instruction = &function->instructions[i];
        const SZrAotIrBlock *containingBlock =
                aot_ir_block_for_instruction(function, i);
        TZrUInt32 requiredFlags =
                aot_ir_required_instruction_flags(instruction->opcode);
        if (instruction->id == ZR_AOT_IR_ID_INVALID ||
            instruction->opcode == ZR_EXEC_IR_OPCODE_INVALID ||
            instruction->opcode >= ZR_EXEC_IR_OPCODE_COUNT ||
            (instruction->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !aot_ir_range_valid(instruction->results, function->resultCount) ||
            !aot_ir_range_valid(instruction->operands, function->operandCount) ||
            !aot_ir_range_valid(instruction->successors, function->successorCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_OPCODE, function->id, 0u,
                               instruction->id, i, ZR_EXEC_IR_OPCODE_COUNT, instruction->opcode);
        }
        if ((instruction->flags & requiredFlags) != requiredFlags) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i, requiredFlags, instruction->flags);
        }
        if (requiredFlags != 0u &&
            (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
             instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i, 1u,
                               instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID
                                   ? instruction->effectIn
                                   : instruction->effectOut);
        }
        if (!aot_ir_range_valid(instruction->phiIncoming, function->phiIncomingCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id, 0u,
                               instruction->id, i, function->phiIncomingCount,
                               instruction->phiIncoming.offset);
        }
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_PHI &&
            instruction->phiIncoming.count != 0u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u,
                               instruction->id, i, 0u,
                               instruction->phiIncoming.count);
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_PHI &&
            (containingBlock == ZR_NULL ||
             instruction->phiIncoming.count != containingBlock->predecessors.count)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i,
                               containingBlock != ZR_NULL ? containingBlock->predecessors.count : 0u,
                               instruction->phiIncoming.count);
        }
        for (TZrUInt32 j = 0u; j < instruction->phiIncoming.count; ++j) {
            const SZrAotIrPhiIncoming *incoming = &function->phiIncomingPool[
                    instruction->phiIncoming.offset + j];
            if (!aot_ir_block_id_exists(function, incoming->predecessorBlockId)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id,
                                   containingBlock != ZR_NULL ? containingBlock->id : 0u,
                                   instruction->id, j,
                                   function->blockCount,
                                   incoming->predecessorBlockId);
            }
            if (containingBlock == ZR_NULL ||
                function->successorPool[containingBlock->predecessors.offset + j] !=
                        incoming->predecessorBlockId) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id,
                                   containingBlock != ZR_NULL ? containingBlock->id : 0u,
                                   instruction->id, j,
                                   containingBlock != ZR_NULL
                                       ? function->successorPool[
                                                 containingBlock->predecessors.offset + j]
                                       : 0u,
                                   incoming->predecessorBlockId);
            }
            if (incoming->valueId == ZR_AOT_IR_ID_INVALID) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID,
                                   function->id, 0u, instruction->id, j, 1u,
                                   incoming->valueId);
            }
        }
        for (TZrUInt32 j = 0u; j < instruction->successors.count; ++j) {
            TZrUInt32 target = function->successorPool[
                    instruction->successors.offset + j];
            if (!aot_ir_block_id_exists(function, target)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id, 0u, instruction->id, j,
                                   function->blockCount, target);
            }
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->instructions[j].id == instruction->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, 0u,
                                   instruction->id, i, j, i);
            }
        }
        if ((instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
            (instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id, 0u,
                               instruction->id, i, instruction->effectIn, instruction->effectOut);
        }
        if (instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
            instruction->effectOut <= instruction->effectIn) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id, 0u,
                               instruction->id, i,
                               instruction->effectIn == UINT32_MAX
                                   ? instruction->effectIn
                                   : instruction->effectIn + 1u,
                               instruction->effectOut);
        }
    }
    for (TZrUInt32 blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrAotIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 latestEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        for (TZrUInt32 offset = 0u; offset < block->instructions.count; ++offset) {
            TZrUInt32 instructionIndex = block->instructions.offset + offset;
            const SZrAotIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            if (aot_ir_required_instruction_flags(instruction->opcode) == 0u) {
                continue;
            }
            if (latestEffect != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                instruction->effectIn != latestEffect) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                                   block->id, instruction->id, instructionIndex,
                                   latestEffect, instruction->effectIn);
            }
            latestEffect = instruction->effectOut;
        }
    }
    for (TZrUInt32 i = 0u; i < function->stateMapCount; ++i) {
        const SZrAotIrStateMapEntry *state = &function->stateMaps[i];
        TZrBool instructionFound = ZR_FALSE;
        if (state->resumeId == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u,
                               state->instructionId, i, 1u, state->resumeId);
        }
        if (state->instructionId == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u,
                               state->instructionId, i, 1u, state->instructionId);
        }
        for (TZrUInt32 j = 0u; j < function->instructionCount; ++j) {
            if (function->instructions[j].id == state->instructionId) {
                instructionFound = ZR_TRUE;
                break;
            }
        }
        if (!instructionFound) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u,
                               state->instructionId, i, function->instructionCount,
                               state->instructionId);
        }
    }
    return ZR_AOT_IR_OK;
}

EZrAotIrStatus ZrCore_AotIr_ValidateTarget(const SZrAotIrTargetContract *target,
                                           SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (target == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (target->abiVersion != ZR_AOT_IR_TARGET_ABI_VERSION ||
        (target->pointerSize != 4u && target->pointerSize != 8u) ||
        target->endianness > 1u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           ZR_AOT_IR_TARGET_ABI_VERSION, target->abiVersion);
    }
    if (target->targetTripleHash == 0u || target->abiHash == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           1u, target->targetTripleHash == 0u
                                       ? target->targetTripleHash
                                       : target->abiHash);
    }
    if ((target->requiredCapabilities & ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           target->requiredCapabilities);
    }
    return ZR_AOT_IR_OK;
}

EZrAotIrStatus ZrCore_AotIr_ValidateModule(const SZrAotIrModule *module,
                                           SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (module == ZR_NULL ||
        module->schemaVersion != ZR_AOT_IR_SCHEMA_VERSION ||
        module->functions == ZR_NULL || module->functionCount == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 0u, 0u,
                           ZR_AOT_IR_SCHEMA_VERSION, module != ZR_NULL ? module->schemaVersion : 0u);
    }
    if (ZrCore_AotIr_ValidateTarget(&module->target, diagnostic) != ZR_AOT_IR_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status : ZR_AOT_IR_INVALID_TARGET;
    }
    if (module->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_SCHEMA_VERSION,
                           module->contract.schemaVersion);
    }
    if (module->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_ABI_VERSION,
                           module->contract.abiVersion);
    }
    if (module->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_LOGICAL_VERSION,
                           module->contract.logicalVersion);
    }
    if (module->moduleHash == 0u ||
        module->contract.moduleHash != module->moduleHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           module->moduleHash, module->contract.moduleHash);
    }
    if ((module->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           module->contract.requiredCapabilities);
    }
    if ((module->contract.declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_EFFECT_KNOWN_MASK,
                           module->contract.declaredEffects);
    }
    for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
        EZrAotIrStatus status = aot_ir_validate_function(module, &module->functions[i], i, diagnostic);
        if (status != ZR_AOT_IR_OK) {
            return status;
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (module->functions[j].id == module->functions[i].id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, module->functions[i].id,
                                   0u, 0u, i, j, i);
            }
        }
    }
    if (module->relocationCount != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION, 0u, 0u, 0u, 0u, 0u,
                           module->relocationCount);
    }
    return ZR_AOT_IR_OK;
}

TZrBool ZrCore_AotIr_IsRelocationFree(const SZrAotIrModule *module,
                                      SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (module == ZR_NULL || module->functions == ZR_NULL ||
        module->functionCount == 0u || module->relocationCount != 0u) {
        (void)aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION, 0u, 0u, 0u, 0u, 0u,
                          module != ZR_NULL ? module->relocationCount : 1u);
        return ZR_FALSE;
    }
    if (module->functions != ZR_NULL) {
        for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
            if (module->functions[i].relocationCount != 0u) {
                (void)aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION,
                                  module->functions[i].id, 0u, 0u, i, 0u,
                                  module->functions[i].relocationCount);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrUInt64 ZrCore_AotIr_HashModule(const SZrAotIrModule *module) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    if (ZrCore_AotIr_ValidateModule(module, ZR_NULL) != ZR_AOT_IR_OK) {
        return 0u;
    }
    hash = aot_ir_hash_u32(hash, module->schemaVersion);
    hash = aot_ir_hash_u32(hash, module->flags);
    hash = aot_ir_hash_u32(hash, module->target.abiVersion);
    hash = aot_ir_hash_u32(hash, module->target.pointerSize);
    hash = aot_ir_hash_u32(hash, module->target.endianness);
    hash = aot_ir_hash_u32(hash, module->target.requiredCapabilities);
    hash = aot_ir_hash_u64(hash, module->target.targetTripleHash);
    hash = aot_ir_hash_u64(hash, module->target.abiHash);
    hash = aot_ir_hash_contract(hash, &module->contract);
    hash = aot_ir_hash_u64(hash, module->moduleHash);
    hash = aot_ir_hash_u32(hash, module->functionCount);
    for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
        const SZrAotIrFunction *function = &module->functions[i];
        hash = aot_ir_hash_u32(hash, function->id);
        hash = aot_ir_hash_u32(hash, function->functionToken);
        hash = aot_ir_hash_contract(hash, &function->contract);
        hash = aot_ir_hash_u64(hash, function->signatureHash);
        hash = aot_ir_hash_u32(hash, function->frameLayout.logicalSlotCount);
        hash = aot_ir_hash_u32(hash, function->frameLayout.storageSlotCount);
        hash = aot_ir_hash_u32(hash, function->frameLayout.parameterPrefixBytes);
        hash = aot_ir_hash_u32(hash, function->frameLayout.returnAreaOffset);
        hash = aot_ir_hash_u32(hash, function->frameLayout.frameByteSize);
        hash = aot_ir_hash_u32(hash, function->frameLayout.frameByteAlign);
        hash = aot_ir_hash_u64(hash, function->frameLayout.layoutHash);
        hash = aot_ir_hash_u32(hash, function->blockCount);
        for (TZrUInt32 j = 0u; j < function->blockCount; ++j) {
            const SZrAotIrBlock *block = &function->blocks[j];
            hash = aot_ir_hash_u32(hash, block->id);
            hash = aot_ir_hash_u32(hash, block->flags);
            hash = aot_ir_hash_u32(hash, block->instructions.offset);
            hash = aot_ir_hash_u32(hash, block->instructions.count);
            hash = aot_ir_hash_u32(hash, block->predecessors.offset);
            hash = aot_ir_hash_u32(hash, block->predecessors.count);
            hash = aot_ir_hash_u32(hash, block->successors.offset);
            hash = aot_ir_hash_u32(hash, block->successors.count);
            hash = aot_ir_hash_u32(hash, block->terminatorInstructionId);
        }
        hash = aot_ir_hash_u32(hash, function->instructionCount);
        for (TZrUInt32 j = 0u; j < function->instructionCount; ++j) {
            const SZrAotIrInstruction *instruction = &function->instructions[j];
            const TZrUInt32 fields[] = {
                instruction->id, instruction->opcode, instruction->flags,
                instruction->results.offset, instruction->results.count,
                instruction->operands.offset, instruction->operands.count,
                instruction->successors.offset, instruction->successors.count,
                instruction->phiIncoming.offset, instruction->phiIncoming.count,
                instruction->effectIn, instruction->effectOut, instruction->sourceId,
                instruction->deoptId, instruction->layoutId, instruction->bindingRow};
            for (TZrUInt32 k = 0u; k < (TZrUInt32)(sizeof(fields) / sizeof(fields[0])); ++k) {
                hash = aot_ir_hash_u32(hash, fields[k]);
            }
        }
        hash = aot_ir_hash_u32(hash, function->operandCount);
        for (TZrUInt32 j = 0u; j < function->operandCount; ++j) hash = aot_ir_hash_u32(hash, function->operandPool[j]);
        hash = aot_ir_hash_u32(hash, function->resultCount);
        for (TZrUInt32 j = 0u; j < function->resultCount; ++j) hash = aot_ir_hash_u32(hash, function->resultPool[j]);
        hash = aot_ir_hash_u32(hash, function->phiIncomingCount);
        for (TZrUInt32 j = 0u; j < function->phiIncomingCount; ++j) {
            hash = aot_ir_hash_u32(hash, function->phiIncomingPool[j].predecessorBlockId);
            hash = aot_ir_hash_u32(hash, function->phiIncomingPool[j].valueId);
        }
        hash = aot_ir_hash_u32(hash, function->successorCount);
        for (TZrUInt32 j = 0u; j < function->successorCount; ++j) hash = aot_ir_hash_u32(hash, function->successorPool[j]);
        hash = aot_ir_hash_u32(hash, function->stateMapCount);
        for (TZrUInt32 j = 0u; j < function->stateMapCount; ++j) {
            hash = aot_ir_hash_u32(hash, function->stateMaps[j].resumeId);
            hash = aot_ir_hash_u32(hash, function->stateMaps[j].instructionId);
            hash = aot_ir_hash_u64(hash, function->stateMaps[j].stateHash);
        }
        hash = aot_ir_hash_u64(hash, function->gcMapHash);
        hash = aot_ir_hash_u64(hash, function->exceptionMapHash);
        hash = aot_ir_hash_u64(hash, function->debugMapHash);
    }
    return hash;
}

const TZrChar *ZrCore_AotIr_StatusName(EZrAotIrStatus status) {
    switch (status) {
        case ZR_AOT_IR_OK: return "ok";
        case ZR_AOT_IR_INVALID_ARGUMENT: return "invalid_argument";
        case ZR_AOT_IR_VERSION_MISMATCH: return "version_mismatch";
        case ZR_AOT_IR_INVALID_TARGET: return "invalid_target";
        case ZR_AOT_IR_INVALID_CONTRACT: return "invalid_contract";
        case ZR_AOT_IR_INVALID_ID: return "invalid_id";
        case ZR_AOT_IR_DUPLICATE_ID: return "duplicate_id";
        case ZR_AOT_IR_INVALID_RANGE: return "invalid_range";
        case ZR_AOT_IR_INVALID_CFG: return "invalid_cfg";
        case ZR_AOT_IR_INVALID_OPCODE: return "invalid_opcode";
        case ZR_AOT_IR_INVALID_EFFECT: return "invalid_effect";
        case ZR_AOT_IR_INVALID_LAYOUT: return "invalid_layout";
        case ZR_AOT_IR_INVALID_SIGNATURE: return "invalid_signature";
        case ZR_AOT_IR_RELOCATION: return "relocation";
        case ZR_AOT_IR_UNSUPPORTED: return "unsupported";
        default: return "unknown";
    }
}
