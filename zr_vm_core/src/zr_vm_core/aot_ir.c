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
    if (function->id == ZR_AOT_IR_ID_INVALID || function->functionToken == 0u ||
        function->signatureHash == 0u || function->frameLayout.layoutHash == 0u ||
        !aot_ir_alignment_valid(function->frameLayout.frameByteAlign) ||
        function->frameLayout.frameByteSize < function->frameLayout.returnAreaOffset ||
        function->blocks == ZR_NULL || function->instructions == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, function->id, 0u, 0u,
                           functionIndex, 1u, 0u);
    }
    if (function->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ||
        function->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION ||
        function->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION ||
        function->contract.signatureHash != function->signatureHash ||
        function->contract.layoutHash != function->frameLayout.layoutHash ||
        function->contract.targetToken != function->functionToken ||
        function->contract.moduleHash != module->contract.moduleHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, module->contract.moduleHash, function->contract.moduleHash);
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
    for (TZrUInt32 i = 0u; i < function->blockCount; ++i) {
        const SZrAotIrBlock *block = &function->blocks[i];
        if (block->id == ZR_AOT_IR_ID_INVALID ||
            !aot_ir_range_valid(block->instructions, function->instructionCount) ||
            !aot_ir_range_valid(block->predecessors, function->blockCount) ||
            !aot_ir_range_valid(block->successors, function->successorCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id, block->id, 0u,
                               i, function->instructionCount, block->instructions.offset);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->blocks[j].id == block->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, block->id, 0u,
                                   i, j, i);
            }
        }
        if (block->terminatorInstructionId != ZR_AOT_IR_ID_INVALID) {
            TZrBool found = ZR_FALSE;
            for (TZrUInt32 j = 0u; j < block->instructions.count; ++j) {
                if (function->instructions[block->instructions.offset + j].id ==
                    block->terminatorInstructionId) {
                    found = ZR_TRUE;
                    break;
                }
            }
            if (!found) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, block->id,
                                   block->terminatorInstructionId, i, 1u, 0u);
            }
        }
    }
    for (TZrUInt32 i = 0u; i < function->instructionCount; ++i) {
        const SZrAotIrInstruction *instruction = &function->instructions[i];
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
        if (!aot_ir_range_valid(instruction->phiIncoming, function->phiIncomingCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id, 0u,
                               instruction->id, i, function->phiIncomingCount,
                               instruction->phiIncoming.offset);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->instructions[j].id == instruction->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, 0u,
                                   instruction->id, i, j, i);
            }
        }
        if (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
            instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id, 0u,
                               instruction->id, i, instruction->effectIn, instruction->effectOut);
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
        target->endianness > 1u || target->abiHash == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           ZR_AOT_IR_TARGET_ABI_VERSION, target->abiVersion);
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
    if (module->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ||
        module->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION ||
        module->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION ||
        module->contract.moduleHash != module->moduleHash || module->moduleHash == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           module->moduleHash, module->contract.moduleHash);
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
