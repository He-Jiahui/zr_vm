#include "zr_vm_parser/exec_ir_escape.h"

#include <stdint.h>

/* The hash is a deterministic input witness for the escape facts.  It is
 * deliberately kept in its own translation unit so the analyzer orchestration
 * does not also own cache-key serialization details. */

static TZrUInt64 zr_escape_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        hash ^= (value >> (index * 8u)) & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static TZrUInt64 zr_escape_hash_function(const SZrExecIrFunction *function) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;

    if (function == ZR_NULL) return 0u;
    if ((function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->memoryTokenCount != 0u &&
         function->memoryTokenPool == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity) return 0u;
    hash = zr_escape_hash_u64(hash, function->id);
    hash = zr_escape_hash_u64(hash, function->functionToken);
    hash = zr_escape_hash_u64(hash, function->signatureHash);
    hash = zr_escape_hash_u64(hash, function->entryBlockId);
    hash = zr_escape_hash_u64(hash, function->contract.schemaVersion);
    hash = zr_escape_hash_u64(hash, function->contract.abiVersion);
    hash = zr_escape_hash_u64(hash, function->contract.logicalVersion);
    hash = zr_escape_hash_u64(hash, function->contract.generation);
    hash = zr_escape_hash_u64(hash, function->contract.targetToken);
    hash = zr_escape_hash_u64(hash, function->contract.signatureHash);
    hash = zr_escape_hash_u64(hash, function->contract.layoutHash);
    hash = zr_escape_hash_u64(hash, function->contract.moduleHash);
    hash = zr_escape_hash_u64(hash, function->contract.requiredCapabilities);
    hash = zr_escape_hash_u64(hash, function->contract.declaredEffects);
    hash = zr_escape_hash_u64(hash, function->sealed);
    hash = zr_escape_hash_u64(hash, function->valueCount);
    hash = zr_escape_hash_u64(hash, function->instructionCount);
    hash = zr_escape_hash_u64(hash, function->blockCount);
    hash = zr_escape_hash_u64(hash, function->operandCount);
    hash = zr_escape_hash_u64(hash, function->resultCount);
    hash = zr_escape_hash_u64(hash, function->memoryTokenCount);
    hash = zr_escape_hash_u64(hash, function->phiCount);
    hash = zr_escape_hash_u64(hash, function->phiIncomingCount);
    hash = zr_escape_hash_u64(hash, function->predecessorCount);
    hash = zr_escape_hash_u64(hash, function->successorCount);
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        hash = zr_escape_hash_u64(hash, value->id);
        hash = zr_escape_hash_u64(hash, value->typeToken);
        hash = zr_escape_hash_u64(hash, (TZrUInt32)value->ownership);
        hash = zr_escape_hash_u64(hash, (TZrUInt32)value->nullability);
        hash = zr_escape_hash_u64(hash, value->flags);
        hash = zr_escape_hash_u64(hash, value->definition);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        hash = zr_escape_hash_u64(hash, instruction->opcode);
        hash = zr_escape_hash_u64(hash, instruction->flags);
        hash = zr_escape_hash_u64(hash, instruction->results.start);
        hash = zr_escape_hash_u64(hash, instruction->results.count);
        hash = zr_escape_hash_u64(hash, instruction->operands.start);
        hash = zr_escape_hash_u64(hash, instruction->operands.count);
        hash = zr_escape_hash_u64(hash, instruction->phiRange.start);
        hash = zr_escape_hash_u64(hash, instruction->phiRange.count);
        hash = zr_escape_hash_u64(hash, instruction->successorRange.start);
        hash = zr_escape_hash_u64(hash, instruction->successorRange.count);
        hash = zr_escape_hash_u64(hash, instruction->typeToken);
        hash = zr_escape_hash_u64(hash, instruction->matchTypeToken);
        hash = zr_escape_hash_u64(hash, instruction->layoutId);
        hash = zr_escape_hash_u64(hash, instruction->memoryIn.start);
        hash = zr_escape_hash_u64(hash, instruction->memoryIn.count);
        hash = zr_escape_hash_u64(hash, instruction->memoryOut.start);
        hash = zr_escape_hash_u64(hash, instruction->memoryOut.count);
        hash = zr_escape_hash_u64(hash, instruction->effectIn);
        hash = zr_escape_hash_u64(hash, instruction->effectOut);
        hash = zr_escape_hash_u64(hash, instruction->sourceId);
        hash = zr_escape_hash_u64(hash, instruction->deoptId);
        hash = zr_escape_hash_u64(hash, instruction->bindingRow);
    }
    for (index = 0u; index < function->operandCount; ++index)
        hash = zr_escape_hash_u64(hash, function->operands[index]);
    for (index = 0u; index < function->resultCount; ++index)
        hash = zr_escape_hash_u64(hash, function->results[index]);
    for (index = 0u; index < function->memoryTokenCount; ++index)
        hash = zr_escape_hash_u64(hash, function->memoryTokenPool[index]);
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        hash = zr_escape_hash_u64(hash, block->id);
        hash = zr_escape_hash_u64(hash, block->flags);
        hash = zr_escape_hash_u64(hash, block->instructionRange.start);
        hash = zr_escape_hash_u64(hash, block->instructionRange.count);
        hash = zr_escape_hash_u64(hash, block->predecessorRange.start);
        hash = zr_escape_hash_u64(hash, block->predecessorRange.count);
        hash = zr_escape_hash_u64(hash, block->successorRange.start);
        hash = zr_escape_hash_u64(hash, block->successorRange.count);
        hash = zr_escape_hash_u64(hash, block->phis.start);
        hash = zr_escape_hash_u64(hash, block->phis.count);
        hash = zr_escape_hash_u64(hash, block->terminatorInstructionId);
    }
    for (index = 0u; index < function->predecessorCount; ++index)
        hash = zr_escape_hash_u64(hash, function->predecessors[index]);
    for (index = 0u; index < function->successorCount; ++index)
        hash = zr_escape_hash_u64(hash, function->successors[index]);
    for (index = 0u; index < function->phiCount; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        hash = zr_escape_hash_u64(hash, phi->result);
        hash = zr_escape_hash_u64(hash, phi->incomings.start);
        hash = zr_escape_hash_u64(hash, phi->incomings.count);
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        hash = zr_escape_hash_u64(hash, function->phiIncoming[index].predecessor);
        hash = zr_escape_hash_u64(hash, function->phiIncoming[index].value);
    }
    return hash;
}

TZrUInt64 ZrParser_ExecIr_EscapeInputHash(
        const SZrExecIrFunction *function) {
    return zr_escape_hash_function(function);
}
