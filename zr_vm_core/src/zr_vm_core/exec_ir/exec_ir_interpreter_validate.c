#include "exec_ir_interpreter_internal.h"

static TZrBool zr_oracle_range(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_oracle_is_effect_phi_incoming(
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

static TZrBool zr_oracle_is_memory_phi_incoming(
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

static TZrBool zr_oracle_block_has_successor(const SZrExecIrFunction *f,
                                              const SZrExecIrBlock *block,
                                              TZrExecIrBlockId id) {
    TZrUInt32 i;
    for (i = 0u; i < block->successorRange.count; ++i) {
        if (f->successors[block->successorRange.start + i] == id) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_oracle_block_has_predecessor(const SZrExecIrFunction *f,
                                               const SZrExecIrBlock *block,
                                               TZrExecIrBlockId id) {
    TZrUInt32 i;
    for (i = 0u; i < block->predecessorRange.count; ++i) {
        if (f->predecessors[block->predecessorRange.start + i] == id) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool zr_oracle_value_kind_valid(EZrExecIrOracleValueKind kind) {
    return (TZrBool)(kind >= ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
                     kind < ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT);
}

TZrBool zr_oracle_validate(const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 i;
    if (f == ZR_NULL || f->valueCount > f->valueCapacity ||
        f->instructionCount > f->instructionCapacity || f->blockCount > f->blockCapacity ||
        f->operandCount > f->operandCapacity || f->resultCount > f->resultCapacity ||
        f->successorCount > f->successorCapacity || f->predecessorCount > f->predecessorCapacity ||
        f->phiCount > f->phiCapacity || f->phiIncomingCount > f->phiIncomingCapacity ||
        f->memoryTokenCount > f->memoryTokenCapacity ||
        f->gcMapCount > f->gcMapCapacity || f->gcRootCount > f->gcRootCapacity ||
        f->deoptStateCount > f->deoptStateCapacity || f->deoptValueCount > f->deoptValueCapacity ||
        f->sourceMapCount > f->sourceMapCapacity ||
        (f->valueCount && f->values == ZR_NULL) ||
        (f->instructionCount && f->instructions == ZR_NULL) ||
        (f->blockCount && f->blocks == ZR_NULL) ||
        (f->operandCount && f->operands == ZR_NULL) ||
        (f->resultCount && f->results == ZR_NULL) ||
        (f->successorCount && f->successors == ZR_NULL) ||
        (f->predecessorCount && f->predecessors == ZR_NULL) ||
        (f->phiCount && f->phiPool == ZR_NULL) ||
        (f->phiIncomingCount && f->phiIncoming == ZR_NULL) ||
        (f->memoryTokenCount && f->memoryTokenPool == ZR_NULL) ||
        (f->gcMapCount && f->gcMap == ZR_NULL) ||
        (f->gcRootCount && f->gcRoots == ZR_NULL) ||
        (f->deoptStateCount && f->deoptStates == ZR_NULL) ||
        (f->deoptValueCount && f->deoptValues == ZR_NULL) ||
        (f->sourceMapCount && f->sourceMaps == ZR_NULL) ||
        (f->entryBlockId > f->blockCount && f->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID)) {
        zr_oracle_diag(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, f, 0u, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (i = 0u; i < f->valueCount; ++i) {
        if (f->values[i].id != i + 1u) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, i + 1u, f->values[i].id);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->operandCount; ++i) {
        if (f->operands[i] == ZR_EXEC_IR_VALUE_ID_INVALID || f->operands[i] > f->valueCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, f->valueCount, f->operands[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->resultCount; ++i) {
        if (f->results[i] == ZR_EXEC_IR_VALUE_ID_INVALID || f->results[i] > f->valueCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, f->valueCount, f->results[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->predecessorCount; ++i) {
        if (f->predecessors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID || f->predecessors[i] > f->blockCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u, 0u, f->blockCount, f->predecessors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->successorCount; ++i) {
        if (f->successors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID || f->successors[i] > f->blockCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u, 0u, f->blockCount, f->successors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->phiIncomingCount; ++i) {
        if (f->phiIncoming[i].predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            f->phiIncoming[i].predecessor > f->blockCount ||
            f->phiIncoming[i].value == ZR_EXEC_IR_VALUE_ID_INVALID ||
            (!zr_oracle_is_effect_phi_incoming(f, i) &&
             !zr_oracle_is_memory_phi_incoming(f, i) &&
             f->phiIncoming[i].value > f->valueCount)) {
            zr_oracle_diag(d, zr_oracle_is_memory_phi_incoming(f, i)
                                  ? ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN
                                  : (zr_oracle_is_effect_phi_incoming(f, i)
                                         ? ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN
                                         : ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE),
                           f,
                           f->phiIncoming[i].predecessor, 0u, 0u, f->valueCount,
                           f->phiIncoming[i].value);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->phiCount; ++i) {
        const SZrExecIrPhi *p = &f->phiPool[i];
        if (p->result == ZR_EXEC_IR_VALUE_ID_INVALID || p->result > f->valueCount ||
            !zr_oracle_range(p->incomings, f->phiIncomingCount)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, 0u, 0u,
                           f->phiIncomingCount, p->incomings.start + p->incomings.count);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (b->id != i + 1u || !zr_oracle_range(b->instructionRange, f->instructionCount) ||
            !zr_oracle_range(b->predecessorRange, f->predecessorCount) ||
            !zr_oracle_range(b->successorRange, f->successorCount) ||
            !zr_oracle_range(b->phis, f->phiCount) ||
            !zr_oracle_range(b->effectPhiIncomings, f->phiIncomingCount)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, 0u, i + 1u, b->id);
            return ZR_FALSE;
        }
        {
            TZrUInt32 region;
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (!zr_oracle_range(b->memoryPhiIncomings[region],
                                     f->phiIncomingCount) ||
                    ((b->memoryPhiResults[region] ==
                      ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) !=
                     (b->memoryPhiIncomings[region].count == 0u))) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN, f,
                                   b->id, 0u, 0u, f->phiIncomingCount,
                                   b->memoryPhiIncomings[region].count);
                    return ZR_FALSE;
                }
                if (b->memoryPhiResults[region] !=
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    (!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(
                             b->memoryPhiResults[region]) ||
                     ZR_EXEC_IR_MEMORY_TOKEN_REGION(
                             b->memoryPhiResults[region]) != region ||
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                             b->memoryPhiResults[region]) == 0u)) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN, f,
                                   b->id, 0u, 0u, region,
                                   b->memoryPhiResults[region]);
                    return ZR_FALSE;
                }
            }
        }
        {
            TZrUInt32 j;
            for (j = 0u; j < b->successorRange.count; ++j) {
                TZrExecIrBlockId successor = f->successors[b->successorRange.start + j];
                const SZrExecIrBlock *target = &f->blocks[successor - 1u];
                if (!zr_oracle_block_has_predecessor(f, target, b->id)) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, 0u, 1u, successor);
                    return ZR_FALSE;
                }
            }
            for (j = 0u; j < b->predecessorRange.count; ++j) {
                TZrExecIrBlockId predecessor = f->predecessors[b->predecessorRange.start + j];
                const SZrExecIrBlock *source = &f->blocks[predecessor - 1u];
                if (!zr_oracle_block_has_successor(f, source, b->id)) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, 0u, 1u, predecessor);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        TZrUInt32 j;
        for (j = 0u; j < b->phis.count; ++j) {
            const SZrExecIrPhi *phi = &f->phiPool[b->phis.start + j];
            if (phi->incomings.count != b->predecessorRange.count) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                               f, b->id, 0u, 0u, b->predecessorRange.count,
                               phi->incomings.count);
                return ZR_FALSE;
            }
            for (TZrUInt32 k = 0u; k < phi->incomings.count; ++k) {
                if (f->phiIncoming[phi->incomings.start + k].predecessor !=
                    f->predecessors[b->predecessorRange.start + k]) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                   f, b->id, 0u, 0u,
                                   f->predecessors[b->predecessorRange.start + k],
                                   f->phiIncoming[phi->incomings.start + k].predecessor);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *ins = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)ins->opcode);
        if (info == ZR_NULL || ins->opcode == ZR_EXEC_IR_OPCODE_INVALID) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE, f, 0u, i + 1u,
                           ins->sourceId, ZR_EXEC_IR_OPCODE_COUNT - 1u, ins->opcode);
            return ZR_FALSE;
        }
        if ((ins->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !zr_oracle_range(ins->operands, f->operandCount) ||
            !zr_oracle_range(ins->results, f->resultCount) ||
            !zr_oracle_range(ins->successorRange, f->successorCount) ||
            !zr_oracle_range(ins->phiRange, f->phiCount) ||
            !zr_oracle_range(ins->memoryIn, f->memoryTokenCount) ||
            !zr_oracle_range(ins->memoryOut, f->memoryTokenCount) ||
            ins->operands.count < info->minimumOperands ||
            ins->operands.count > info->maximumOperands ||
            (info->resultArity != ZR_EXEC_IR_VARIADIC && ins->results.count != info->resultArity)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, i + 1u,
                           ins->sourceId, info->minimumOperands, ins->operands.count);
            return ZR_FALSE;
        }
        if (ins->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED &&
            !ZrCore_ExecIr_ConditionalCleanupValid(f, i + 1u)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, i + 1u,
                           ins->sourceId, 0u, 0u);
            return ZR_FALSE;
        }
        for (TZrUInt32 j = 0u; j < ins->results.count; ++j) {
            TZrExecIrValueId valueId = f->results[ins->results.start + j];
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > f->valueCount) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u,
                               i + 1u, ins->sourceId, f->valueCount, valueId);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 j = 0u; j < ins->successorRange.count; ++j) {
            TZrExecIrBlockId target = f->successors[ins->successorRange.start + j];
            if (target == ZR_EXEC_IR_BLOCK_ID_INVALID || target > f->blockCount) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u,
                               i + 1u, ins->sourceId, f->blockCount, target);
                return ZR_FALSE;
            }
        }
        {
            EZrExecIrOpcode opcode = (EZrExecIrOpcode)ins->opcode;
            TZrUInt32 successorCount = ins->successorRange.count;
            if ((opcode == ZR_EXEC_IR_OPCODE_BRANCH && successorCount != 1u) ||
                (opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH && successorCount != 2u) ||
                (opcode == ZR_EXEC_IR_OPCODE_SWITCH && successorCount == 0u)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u,
                               i + 1u, ins->sourceId, 1u, successorCount);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}
