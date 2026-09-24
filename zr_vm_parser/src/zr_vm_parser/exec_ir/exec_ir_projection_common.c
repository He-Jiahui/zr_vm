#include "zr_vm_parser/exec_ir_projections.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void zr_projection_diag(SZrExecIrDiagnostic *d,
                               EZrExecutionDiagnosticCode code,
                               const SZrExecIrFunction *f,
                               TZrUInt32 block,
                               TZrUInt32 instruction,
                               TZrUInt32 expected,
                               TZrUInt32 actual) {
    if (d == ZR_NULL) return;
    memset(d, 0, sizeof(*d));
    d->code = code;
    d->functionToken = f != ZR_NULL ? f->functionToken : 0u;
    d->blockId = block;
    d->instructionId = instruction;
    if (f != ZR_NULL && instruction != 0u && instruction <= f->instructionCount) {
        d->sourceId = f->instructions[instruction - 1u].sourceId;
    }
    d->expectedVersion = expected;
    d->actualVersion = actual;
}

static TZrBool zr_projection_range(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_projection_bytes(TZrUInt32 count, size_t element, size_t *bytes) {
    if (bytes == ZR_NULL || element == 0u || (size_t)count > SIZE_MAX / element) return ZR_FALSE;
    *bytes = (size_t)count * element;
    return ZR_TRUE;
}

/* The first projection is intentionally a no-optimization scalar/control
 * slice.  PLACE_BASE/PLACE_PROJECT, ALLOC, INVOKE, and iterator operations are
 * copied as metadata; executable layout, allocation, invoke/landing-pad, and
 * iterator ABI handling remain separate backend concerns. */
static TZrBool zr_projection_opcode_supported(EZrExecIrOpcode opcode) {
    switch (opcode) {
        default:
            return ZR_TRUE;
    }
}

static TZrBool zr_projection_value_id_valid(const SZrExecIrFunction *f,
                                            TZrExecIrValueId valueId) {
    return (TZrBool)(valueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                     valueId <= f->valueCount);
}

static TZrBool zr_projection_block_contains_predecessor(const SZrExecIrFunction *f,
                                                         const SZrExecIrBlock *block,
                                                         TZrExecIrBlockId predecessor) {
    TZrUInt32 i;
    for (i = 0u; i < block->predecessorRange.count; ++i) {
        if (f->predecessors[block->predecessorRange.start + i] == predecessor) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_projection_validate(const SZrExecIrFunction *f,
                                      SZrExecIrDiagnostic *d) {
    TZrUInt32 i;
    if (f == ZR_NULL || f->instructionCount > f->instructionCapacity ||
        f->valueCount > f->valueCapacity || f->blockCount > f->blockCapacity ||
        f->operandCount > f->operandCapacity || f->resultCount > f->resultCapacity ||
        f->predecessorCount > f->predecessorCapacity || f->successorCount > f->successorCapacity ||
        f->memoryTokenCount > f->memoryTokenCapacity ||
        f->phiCount > f->phiCapacity || f->phiIncomingCount > f->phiIncomingCapacity ||
        f->gcMapCount > f->gcMapCapacity || f->gcRootCount > f->gcRootCapacity ||
        f->deoptStateCount > f->deoptStateCapacity || f->deoptValueCount > f->deoptValueCapacity ||
        f->sourceMapCount > f->sourceMapCapacity ||
        (f->instructionCount != 0u && f->instructions == ZR_NULL) ||
        (f->valueCount != 0u && f->values == ZR_NULL) ||
        (f->blockCount != 0u && f->blocks == ZR_NULL) ||
        (f->operandCount != 0u && f->operands == ZR_NULL) ||
        (f->resultCount != 0u && f->results == ZR_NULL) ||
        (f->memoryTokenCount != 0u && f->memoryTokenPool == ZR_NULL) ||
        (f->predecessorCount != 0u && f->predecessors == ZR_NULL) ||
        (f->successorCount != 0u && f->successors == ZR_NULL) ||
        (f->phiCount != 0u && f->phiPool == ZR_NULL) ||
        (f->phiIncomingCount != 0u && f->phiIncoming == ZR_NULL) ||
        (f->gcMapCount != 0u && f->gcMap == ZR_NULL) ||
        (f->gcRootCount != 0u && f->gcRoots == ZR_NULL) ||
        (f->deoptStateCount != 0u && f->deoptStates == ZR_NULL) ||
        (f->deoptValueCount != 0u && f->deoptValues == ZR_NULL) ||
        (f->sourceMapCount != 0u && f->sourceMaps == ZR_NULL) ||
        (f->frameLayout != ZR_NULL &&
         (f->frameLayout->slotCount > f->frameLayout->slotCapacity ||
          (f->frameLayout->slotCount != 0u && f->frameLayout->slots == ZR_NULL))) ||
        (f->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
         (f->entryBlockId > f->blockCount || f->entryBlockId == 0u))) {
        zr_projection_diag(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, f, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_ValidateDeoptAggregates(f, d)) return ZR_FALSE;
    for (i = 0u; i < f->operandCount; ++i) {
        if (!zr_projection_value_id_valid(f, f->operands[i])) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u,
                               f->valueCount, f->operands[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->resultCount; ++i) {
        if (!zr_projection_value_id_valid(f, f->results[i])) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u,
                               f->valueCount, f->results[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->predecessorCount; ++i) {
        if (f->predecessors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            f->predecessors[i] > f->blockCount) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u,
                               f->blockCount, f->predecessors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->successorCount; ++i) {
        if (f->successors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            f->successors[i] > f->blockCount) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u,
                               f->blockCount, f->successors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (b->id != i + 1u || !zr_projection_range(b->instructionRange, f->instructionCount) ||
            !zr_projection_range(b->predecessorRange, f->predecessorCount) ||
            !zr_projection_range(b->successorRange, f->successorCount) ||
            !zr_projection_range(b->phis, f->phiCount) ||
            (b->terminatorInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
             (b->instructionRange.count == 0u ||
              b->terminatorInstructionId <= b->instructionRange.start ||
              b->terminatorInstructionId - b->instructionRange.start >
                      b->instructionRange.count))) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, i + 1u, b->id);
            return ZR_FALSE;
        }
    }
    /* The two adjacency pools are a bidirectional contract.  Reject a
     * one-sided edge before a projection can manufacture an invalid block. */
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        TZrUInt32 j;
        for (j = 0u; j < b->successorRange.count; ++j) {
            TZrExecIrBlockId successor = f->successors[b->successorRange.start + j];
            const SZrExecIrBlock *target = &f->blocks[successor - 1u];
            TZrUInt32 k;
            TZrUInt32 outgoingOccurrence = 0u;
            TZrUInt32 incomingOccurrences = 0u;
            for (k = 0u; k <= j; ++k) {
                if (f->successors[b->successorRange.start + k] == successor) {
                    ++outgoingOccurrence;
                }
            }
            for (k = 0u; k < target->predecessorRange.count; ++k) {
                if (f->predecessors[target->predecessorRange.start + k] == b->id) {
                    ++incomingOccurrences;
                }
            }
            if (outgoingOccurrence > incomingOccurrences) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, outgoingOccurrence, incomingOccurrences);
                return ZR_FALSE;
            }
        }
        for (j = 0u; j < b->predecessorRange.count; ++j) {
            TZrExecIrBlockId predecessor = f->predecessors[b->predecessorRange.start + j];
            const SZrExecIrBlock *source = &f->blocks[predecessor - 1u];
            TZrUInt32 k;
            TZrUInt32 incomingOccurrence = 0u;
            TZrUInt32 outgoingOccurrences = 0u;
            for (k = 0u; k <= j; ++k) {
                if (f->predecessors[b->predecessorRange.start + k] == predecessor) {
                    ++incomingOccurrence;
                }
            }
            for (k = 0u; k < source->successorRange.count; ++k) {
                if (f->successors[source->successorRange.start + k] == b->id) {
                    ++outgoingOccurrences;
                }
            }
            if (incomingOccurrence > outgoingOccurrences) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, incomingOccurrence, outgoingOccurrences);
                return ZR_FALSE;
            }
        }
    }
    for (i = 0u; i < f->phiIncomingCount; ++i) {
        if (f->phiIncoming[i].predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            f->phiIncoming[i].predecessor > f->blockCount ||
            !zr_projection_value_id_valid(f, f->phiIncoming[i].value)) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                               f, f->phiIncoming[i].predecessor, 0u,
                               f->valueCount, f->phiIncoming[i].value);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->phiCount; ++i) {
        const SZrExecIrPhi *phi = &f->phiPool[i];
        if (!zr_projection_value_id_valid(f, phi->result) ||
            !zr_projection_range(phi->incomings, f->phiIncomingCount)) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, 0u,
                               f->phiIncomingCount, phi->incomings.start + phi->incomings.count);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *ins = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)ins->opcode);
        if (info == ZR_NULL || ins->opcode == ZR_EXEC_IR_OPCODE_INVALID) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE, f, 0u, i + 1u,
                               ZR_EXEC_IR_OPCODE_COUNT - 1u, ins->opcode);
            return ZR_FALSE;
        }
        if ((ins->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !zr_projection_range(ins->operands, f->operandCount) ||
            !zr_projection_range(ins->results, f->resultCount) ||
            !zr_projection_range(ins->successorRange, f->successorCount) ||
            !zr_projection_range(ins->phiRange, f->phiCount) ||
            !zr_projection_range(ins->memoryIn, f->memoryTokenCount) ||
            !zr_projection_range(ins->memoryOut, f->memoryTokenCount) ||
            ins->operands.count < info->minimumOperands ||
            ins->operands.count > info->maximumOperands ||
            (info->resultArity != ZR_EXEC_IR_VARIADIC &&
             ins->results.count != info->resultArity)) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, i + 1u,
                               0u, 0u);
            return ZR_FALSE;
        }
        if (!zr_projection_opcode_supported((EZrExecIrOpcode)ins->opcode)) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, f, 0u, i + 1u,
                               0u, ins->opcode);
            return ZR_FALSE;
        }
        for (TZrUInt32 j = 0u; j < ins->operands.count; ++j) {
            if (!zr_projection_value_id_valid(f, f->operands[ins->operands.start + j])) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u,
                                   i + 1u, f->valueCount,
                                   f->operands[ins->operands.start + j]);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 j = 0u; j < ins->results.count; ++j) {
            if (!zr_projection_value_id_valid(f, f->results[ins->results.start + j])) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u,
                                   i + 1u, f->valueCount,
                                   f->results[ins->results.start + j]);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 j = 0u; j < ins->successorRange.count; ++j) {
            TZrExecIrBlockId target = f->successors[ins->successorRange.start + j];
            if (target == ZR_EXEC_IR_BLOCK_ID_INVALID || target > f->blockCount) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u,
                                   i + 1u, f->blockCount, target);
                return ZR_FALSE;
            }
        }
        {
            TZrUInt32 successorCount = ins->successorRange.count;
            EZrExecIrOpcode opcode = (EZrExecIrOpcode)ins->opcode;
            if ((opcode == ZR_EXEC_IR_OPCODE_BRANCH && successorCount != 1u) ||
                (opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH && successorCount != 2u) ||
                (opcode == ZR_EXEC_IR_OPCODE_SWITCH && successorCount == 0u)) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u,
                                   i + 1u, 1u, successorCount);
                return ZR_FALSE;
            }
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        TZrUInt32 j;
        for (j = 0u; j < b->phis.count; ++j) {
            const SZrExecIrPhi *phi = &f->phiPool[b->phis.start + j];
            TZrUInt32 k;
            if (phi->incomings.count != b->predecessorRange.count) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                   f, b->id, 0u, b->predecessorRange.count,
                                   phi->incomings.count);
                return ZR_FALSE;
            }
            for (k = 0u; k < phi->incomings.count; ++k) {
                TZrExecIrBlockId predecessor =
                        f->phiIncoming[phi->incomings.start + k].predecessor;
                if (!zr_projection_block_contains_predecessor(f, b, predecessor) ||
                    predecessor != f->predecessors[b->predecessorRange.start + k]) {
                    zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                       f, b->id, 0u, predecessor, phi->result);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (i = 0u; i < f->sourceMapCount; ++i) {
        if (f->sourceMaps[i].instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            f->sourceMaps[i].instructionId > f->instructionCount) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u,
                               f->sourceMaps[i].instructionId, f->instructionCount,
                               f->sourceMaps[i].instructionId);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static const SZrExecIrBlock *zr_projection_block(const SZrExecIrFunction *f,
                                                 TZrExecIrBlockId id) {
    return (id == ZR_EXEC_IR_BLOCK_ID_INVALID || id > f->blockCount)
               ? ZR_NULL : &f->blocks[id - 1u];
}

typedef struct SZrProjectionSplitEdge {
    TZrExecIrBlockId source;
    TZrExecIrBlockId destination;
    TZrUInt32 sourceOrdinal;
    TZrUInt32 destinationOrdinal;
    TZrExecIrBlockId syntheticBlock;
} SZrProjectionSplitEdge;

/* The nth source->destination successor is paired with the nth matching
 * destination predecessor.  Both adjacency rows retain their own order. */
static TZrUInt32 zr_projection_incoming_ordinal(const SZrExecIrFunction *f,
                                                const SZrExecIrBlock *source,
                                                TZrUInt32 sourceOrdinal) {
    TZrExecIrBlockId destination =
            f->successors[source->successorRange.start + sourceOrdinal];
    const SZrExecIrBlock *target = &f->blocks[destination - 1u];
    TZrUInt32 i, occurrence = 0u;
    for (i = 0u; i <= sourceOrdinal; ++i) {
        if (f->successors[source->successorRange.start + i] == destination) {
            ++occurrence;
        }
    }
    for (i = 0u; i < target->predecessorRange.count; ++i) {
        if (f->predecessors[target->predecessorRange.start + i] == source->id &&
            --occurrence == 0u) {
            return i;
        }
    }
    /* The bidirectional multiplicity check in zr_projection_validate
     * ensures every source edge has a matching target predecessor. */
    return UINT32_MAX;
}

static TZrBool zr_projection_count_critical_edges(const SZrExecIrFunction *f,
                                                  TZrUInt32 *outCount,
                                                  SZrExecIrDiagnostic *d) {
    TZrUInt32 i, j;
    TZrUInt32 count = 0u;
    if (outCount == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *pred = &f->blocks[i];
        if (pred->successorRange.count <= 1u) continue;
        for (j = 0u; j < pred->successorRange.count; ++j) {
            TZrExecIrBlockId successorId = f->successors[pred->successorRange.start + j];
            const SZrExecIrBlock *successor = zr_projection_block(f, successorId);
            if (successor != ZR_NULL && successor->predecessorRange.count > 1u) {
                if (count == UINT32_MAX) {
                    zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f,
                                       pred->id, pred->terminatorInstructionId,
                                       UINT32_MAX, count);
                    return ZR_FALSE;
                }
                ++count;
            }
        }
    }
    *outCount = count;
    return ZR_TRUE;
}

static TZrExecIrBlockId zr_projection_split_id(
        const SZrProjectionSplitEdge *splits, TZrUInt32 count,
        TZrExecIrBlockId source, TZrExecIrBlockId destination,
        TZrUInt32 ordinal, TZrBool outgoing) {
    TZrUInt32 i;
    for (i = 0u; i < count; ++i) {
        if (splits[i].source == source && splits[i].destination == destination &&
            (outgoing ? splits[i].sourceOrdinal : splits[i].destinationOrdinal) == ordinal) {
            return splits[i].syntheticBlock;
        }
    }
    return outgoing ? destination : source;
}

static TZrBool zr_projection_edge_has_cycle(const SZrExecBcProjection *p,
                                            TZrUInt32 first,
                                            TZrUInt32 count) {
    TZrUInt32 start;
    for (start = 0u; start < count; ++start) {
        TZrExecIrValueId value = p->phiCopyDestinations[first + start];
        TZrUInt32 steps;
        for (steps = 0u; steps < count; ++steps) {
            TZrUInt32 i;
            TZrBool advanced = ZR_FALSE;
            for (i = 0u; i < count; ++i) {
                if (p->phiCopySources[first + i] == value) {
                    value = p->phiCopyDestinations[first + i];
                    advanced = ZR_TRUE;
                    if (value == p->phiCopyDestinations[first + start]) return ZR_TRUE;
                    break;
                }
            }
            if (!advanced) break;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_projection_append_phi_copies(SZrExecBcProjection *p,
                                               const SZrExecIrFunction *f,
                                               SZrExecIrDiagnostic *d) {
    TZrUInt32 i, j, k, total = 0u;
    size_t bytes;

    /* Validate the complete incoming matrix and count real moves.  A
     * self-copy is already satisfied and must not manufacture a cycle. */
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (b->phis.count != 0u && b->predecessorRange.count == 0u) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                               f, b->id, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        for (j = 0u; j < b->phis.count; ++j) {
            const SZrExecIrPhi *phi = &f->phiPool[b->phis.start + j];
            if (phi->incomings.count != b->predecessorRange.count) {
                zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                   f, b->id, 0u, b->predecessorRange.count,
                                   phi->incomings.count);
                return ZR_FALSE;
            }
            for (k = 0u; k < b->predecessorRange.count; ++k) {
                const SZrExecIrPhiIncoming *incoming =
                        &f->phiIncoming[phi->incomings.start + k];
                if (incoming->value != phi->result) {
                    if (total == UINT32_MAX) {
                        zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                           f, b->id, 0u, UINT32_MAX, total);
                        return ZR_FALSE;
                    }
                    ++total;
                }
            }
        }
    }
    p->phiCopyCount = total;
    if (total == 0u) return ZR_TRUE;
    if (!zr_projection_bytes(total, sizeof(*p->phiCopySources), &bytes)) {
        zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           f, 0u, 0u, UINT32_MAX, total);
        return ZR_FALSE;
    }
    p->phiCopySources = (TZrExecIrValueId *)malloc(bytes);
    p->phiCopyDestinations = (TZrExecIrValueId *)malloc(bytes);
    if (!zr_projection_bytes(total, sizeof(*p->phiCopyEdges), &bytes)) {
        zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           f, 0u, 0u, UINT32_MAX, total);
        return ZR_FALSE;
    }
    p->phiCopyEdges = (TZrExecIrBlockId *)malloc(bytes);
    if (p->phiCopySources == ZR_NULL || p->phiCopyDestinations == ZR_NULL ||
        p->phiCopyEdges == ZR_NULL) {
        zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           f, 0u, 0u, total, 0u);
        return ZR_FALSE;
    }
    total = 0u;
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        for (k = 0u; k < b->predecessorRange.count; ++k) {
            TZrExecIrBlockId edge = p->predecessors[b->predecessorRange.start + k];
            TZrUInt32 edgeFirst = total;
            for (j = 0u; j < b->phis.count; ++j) {
                const SZrExecIrPhi *phi = &f->phiPool[b->phis.start + j];
                const SZrExecIrPhiIncoming *incoming =
                        &f->phiIncoming[phi->incomings.start + k];
                if (incoming->value != phi->result) {
                    p->phiCopySources[total] = incoming->value;
                    p->phiCopyDestinations[total] = phi->result;
                    p->phiCopyEdges[total] = edge;
                    ++total;
                }
            }
            if (total > edgeFirst &&
                zr_projection_edge_has_cycle(p, edgeFirst, total - edgeFirst)) {
                p->temporarySlotCount = 1u;
            }
        }
    }
    return ZR_TRUE;
}

void ZrParser_ExecBcProjection_Free(SZrExecBcProjection *p) {
    if (p == ZR_NULL || p->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG) return;
    free(p->opcodes);
    free(p->instructions);
    free(p->operands);
    free(p->results);
    free(p->memoryTokens);
    free(p->valueSlots);
    free(p->blocks);
    free(p->predecessors);
    free(p->successors);
    free(p->phis);
    free(p->phiIncomings);
    free(p->phiCopySources);
    free(p->phiCopyDestinations);
    free(p->phiCopyEdges);
    free(p->sourceMaps);
    memset(p, 0, sizeof(*p));
}

TZrBool ZrParser_ExecIr_BuildProjection(const SZrExecIrFunction *f,
                                        SZrExecBcProjection *p,
                                        SZrExecIrDiagnostic *d) {
    TZrUInt32 i, j;
    TZrUInt32 splitCount = 0u;
    SZrProjectionSplitEdge *splits = ZR_NULL;
    size_t bytes;
    if (d != ZR_NULL) memset(d, 0, sizeof(*d));
    if (p == ZR_NULL || !zr_projection_validate(f, d)) return ZR_FALSE;
    if (!zr_projection_count_critical_edges(f, &splitCount, d)) return ZR_FALSE;
    if (splitCount != 0u) {
        if (!zr_projection_bytes(splitCount, sizeof(*splits), &bytes)) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               f, 0u, 0u, UINT32_MAX, splitCount);
            return ZR_FALSE;
        }
        splits = (SZrProjectionSplitEdge *)calloc(1u, bytes);
        if (splits == ZR_NULL) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               f, 0u, 0u, splitCount, 0u);
            return ZR_FALSE;
        }
        splitCount = 0u;
        for (i = 0u; i < f->blockCount; ++i) {
            const SZrExecIrBlock *pred = &f->blocks[i];
            if (pred->successorRange.count <= 1u) continue;
            for (j = 0u; j < pred->successorRange.count; ++j) {
                TZrExecIrBlockId destination =
                        f->successors[pred->successorRange.start + j];
                const SZrExecIrBlock *successor =
                        zr_projection_block(f, destination);
                if (successor != ZR_NULL && successor->predecessorRange.count > 1u) {
                    splits[splitCount].source = pred->id;
                    splits[splitCount].destination = destination;
                    splits[splitCount].sourceOrdinal = j;
                    splits[splitCount].destinationOrdinal =
                            zr_projection_incoming_ordinal(f, pred, j);
                    splits[splitCount].syntheticBlock =
                            f->blockCount + splitCount + 1u;
                    ++splitCount;
                }
            }
        }
    }
    if (splitCount > UINT32_MAX - f->blockCount ||
        splitCount > UINT32_MAX - f->predecessorCount ||
        splitCount > UINT32_MAX - f->successorCount) {
        zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           f, 0u, 0u, UINT32_MAX, splitCount);
        free(splits);
        return ZR_FALSE;
    }
    /* The shared helper is also callable by tests and future adapters.  Drop
     * a previously owned candidate only after all source validation passes. */
    ZrParser_ExecBcProjection_Free(p);
    memset(p, 0, sizeof(*p));
    p->instructionCount = f->instructionCount;
    p->functionToken = f->functionToken;
    p->signatureHash = f->signatureHash;
    p->entryBlockId = f->entryBlockId;
    p->frameLayoutHash = f->frameLayout != ZR_NULL ? f->frameLayout->layoutHash : 0u;
    p->operandCount = f->operandCount;
    p->resultCount = f->resultCount;
    p->memoryTokenCount = f->memoryTokenCount;
    p->valueSlotCount = f->valueCount;
    p->blockCount = f->blockCount + splitCount;
    p->syntheticBlockCount = splitCount;
    p->predecessorCount = f->predecessorCount + splitCount;
    p->successorCount = f->successorCount + splitCount;
    p->phiCount = f->phiCount;
    p->phiIncomingCount = f->phiIncomingCount;
    p->sourceMapCount = f->sourceMapCount;
    p->gcMapCount = f->gcMapCount;
    p->deoptStateCount = f->deoptStateCount;
    p->stateMapPresent = f->stateMap != ZR_NULL;
    p->runnable = ZR_TRUE;
    p->ownershipTag = ZR_EXEC_IR_PROJECTION_TAG;
    if (!zr_projection_bytes(p->instructionCount, sizeof(*p->opcodes), &bytes)) goto overflow;
    if (p->instructionCount != 0u) {
        p->opcodes = (TZrUInt32 *)malloc(bytes);
        p->instructions = (SZrExecBcInstruction *)malloc((size_t)p->instructionCount * sizeof(*p->instructions));
        if (p->opcodes == ZR_NULL || p->instructions == ZR_NULL) goto oom;
    }
    if (!zr_projection_bytes(p->operandCount, sizeof(*p->operands), &bytes)) goto overflow;
    if (p->operandCount != 0u) { p->operands = (TZrExecIrValueId *)malloc(bytes); if (p->operands == ZR_NULL) goto oom; memcpy(p->operands, f->operands, bytes); }
    if (!zr_projection_bytes(p->resultCount, sizeof(*p->results), &bytes)) goto overflow;
    if (p->resultCount != 0u) { p->results = (TZrExecIrValueId *)malloc(bytes); if (p->results == ZR_NULL) goto oom; memcpy(p->results, f->results, bytes); }
    if (!zr_projection_bytes(p->memoryTokenCount, sizeof(*p->memoryTokens), &bytes)) goto overflow;
    if (p->memoryTokenCount != 0u) {
        p->memoryTokens = (TZrExecIrMemoryTokenId *)malloc(bytes);
        if (p->memoryTokens == ZR_NULL) goto oom;
        memcpy(p->memoryTokens, f->memoryTokenPool, bytes);
    }
    if (!zr_projection_bytes(p->valueSlotCount, sizeof(*p->valueSlots), &bytes)) goto overflow;
    if (p->valueSlotCount != 0u) {
        p->valueSlots = (TZrUInt32 *)malloc(bytes);
        if (p->valueSlots == ZR_NULL) goto oom;
        for (i = 0u; i < p->valueSlotCount; ++i) {
            p->valueSlots[i] = f->frameLayout != ZR_NULL &&
                                       i < f->frameLayout->slotCount
                                   ? f->frameLayout->slots[i].slotId
                                   : i;
        }
        if (f->frameLayout != ZR_NULL &&
            f->frameLayout->slotCount < f->valueCount) {
            zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               f, 0u, 0u, f->valueCount,
                               f->frameLayout->slotCount);
            goto fail;
        }
    }
    if (!zr_projection_bytes(p->blockCount, sizeof(*p->blocks), &bytes)) goto overflow;
    if (p->blockCount != 0u) { p->blocks = (SZrExecBcBlock *)malloc(bytes); if (p->blocks == ZR_NULL) goto oom; }
    if (!zr_projection_bytes(p->predecessorCount, sizeof(*p->predecessors), &bytes)) goto overflow;
    if (p->predecessorCount != 0u) {
        p->predecessors = (TZrExecIrBlockId *)malloc(bytes);
        if (p->predecessors == ZR_NULL) goto oom;
        if (f->predecessorCount != 0u) {
            memcpy(p->predecessors, f->predecessors,
                   (size_t)f->predecessorCount * sizeof(*p->predecessors));
        }
    }
    if (!zr_projection_bytes(p->successorCount, sizeof(*p->successors), &bytes)) goto overflow;
    if (p->successorCount != 0u) {
        p->successors = (TZrExecIrBlockId *)malloc(bytes);
        if (p->successors == ZR_NULL) goto oom;
        if (f->successorCount != 0u) {
            memcpy(p->successors, f->successors,
                   (size_t)f->successorCount * sizeof(*p->successors));
        }
    }
    if (!zr_projection_bytes(p->phiCount, sizeof(*p->phis), &bytes)) goto overflow;
    if (p->phiCount != 0u) {
        p->phis = (SZrExecIrPhi *)malloc(bytes);
        if (p->phis == ZR_NULL) goto oom;
        memcpy(p->phis, f->phiPool, bytes);
    }
    if (!zr_projection_bytes(p->phiIncomingCount, sizeof(*p->phiIncomings), &bytes)) goto overflow;
    if (p->phiIncomingCount != 0u) {
        p->phiIncomings = (SZrExecIrPhiIncoming *)malloc(bytes);
        if (p->phiIncomings == ZR_NULL) goto oom;
        memcpy(p->phiIncomings, f->phiIncoming, bytes);
    }
    if (!zr_projection_bytes(p->sourceMapCount, sizeof(*p->sourceMaps), &bytes)) goto overflow;
    if (p->sourceMapCount != 0u) { p->sourceMaps = (SZrExecIrProjectionSourceMap *)malloc(bytes); if (p->sourceMaps == ZR_NULL) goto oom; }
    for (i = 0u; i < p->instructionCount; ++i) {
        const SZrExecIrInstruction *in = &f->instructions[i];
        p->opcodes[i] = in->opcode;
        p->instructions[i].opcode = in->opcode;
        p->instructions[i].flags = in->flags;
        p->instructions[i].pc = i;
        p->instructions[i].operands = in->operands;
        p->instructions[i].results = in->results;
        p->instructions[i].phiRange = in->phiRange;
        p->instructions[i].successorRange = in->successorRange;
        p->instructions[i].memoryIn = in->memoryIn;
        p->instructions[i].memoryOut = in->memoryOut;
        p->instructions[i].effectIn = in->effectIn;
        p->instructions[i].effectOut = in->effectOut;
        p->instructions[i].typeToken = in->typeToken;
        p->instructions[i].matchTypeToken = in->matchTypeToken;
        p->instructions[i].layoutId = in->layoutId;
        p->instructions[i].sourceId = in->sourceId;
        p->instructions[i].deoptId = in->deoptId;
        p->instructions[i].bindingRow = in->bindingRow;
        if (in->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE ||
            in->opcode == ZR_EXEC_IR_OPCODE_PLACE_PROJECT ||
            in->opcode == ZR_EXEC_IR_OPCODE_ALLOC ||
            in->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST ||
            in->opcode == ZR_EXEC_IR_OPCODE_INVOKE ||
            in->opcode == ZR_EXEC_IR_OPCODE_ITER_INIT ||
            in->opcode == ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT ||
            in->opcode == ZR_EXEC_IR_OPCODE_ITER_CURRENT ||
            in->opcode == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD) {
            /* The projection preserves canonical instruction metadata, but
             * executable layout, subtype, invoke/exception, or iterator
             * dispatch still belongs to a later backend ABI.  Do not
             * advertise this projection as runnable. */
            p->runnable = ZR_FALSE;
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *in = &f->blocks[i];
        p->blocks[i].id = in->id;
        p->blocks[i].instructions = in->instructionRange;
        p->blocks[i].predecessors = in->predecessorRange;
        for (j = 0u; j < in->predecessorRange.count; ++j) {
            TZrExecIrBlockId source =
                    f->predecessors[in->predecessorRange.start + j];
            p->predecessors[in->predecessorRange.start + j] =
                    zr_projection_split_id(splits, splitCount, source, in->id, j, ZR_FALSE);
        }
        p->blocks[i].successors = in->successorRange;
        p->blocks[i].phis = in->phis;
        for (j = 0u; j < in->phis.count; ++j) {
            const SZrExecIrPhi *phi = &f->phiPool[in->phis.start + j];
            TZrUInt32 k;
            for (k = 0u; k < phi->incomings.count; ++k) {
                p->phiIncomings[phi->incomings.start + k].predecessor =
                        p->predecessors[in->predecessorRange.start + k];
            }
        }
        for (j = 0u; j < in->successorRange.count; ++j) {
            TZrExecIrBlockId destination =
                    f->successors[in->successorRange.start + j];
            p->successors[in->successorRange.start + j] =
                    zr_projection_split_id(splits, splitCount, in->id, destination, j, ZR_TRUE);
        }
        p->blocks[i].terminatorInstructionId = in->terminatorInstructionId;
    }
    for (i = 0u; i < splitCount; ++i) {
        TZrUInt32 blockIndex = f->blockCount + i;
        p->blocks[blockIndex].id = splits[i].syntheticBlock;
        p->blocks[blockIndex].instructions.start = 0u;
        p->blocks[blockIndex].instructions.count = 0u;
        p->blocks[blockIndex].predecessors.start = f->predecessorCount + i;
        p->blocks[blockIndex].predecessors.count = 1u;
        p->predecessors[f->predecessorCount + i] = splits[i].source;
        p->blocks[blockIndex].successors.start = f->successorCount + i;
        p->blocks[blockIndex].successors.count = 1u;
        p->blocks[blockIndex].phis = (SZrExecIrRange){.start = 0u, .count = 0u};
        p->successors[f->successorCount + i] = splits[i].destination;
        p->blocks[blockIndex].terminatorInstructionId =
                ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
    }
    for (i = 0u; i < p->sourceMapCount; ++i) {
        p->sourceMaps[i].sourceId = f->sourceMaps[i].sourceId;
        p->sourceMaps[i].pc = f->sourceMaps[i].instructionId == 0u ? 0u : f->sourceMaps[i].instructionId - 1u;
    }
    if (!zr_projection_append_phi_copies(p, f, d)) goto fail;
    free(splits);
    return ZR_TRUE;
overflow:
    zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, 0u, 0u, UINT32_MAX, 0u);
    goto fail;
oom:
    zr_projection_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f, 0u, 0u, 0u, 0u);
fail:
    free(splits);
    ZrParser_ExecBcProjection_Free(p);
    return ZR_FALSE;
}

void ZrParser_ExecIr_MoveProjectionToAot(SZrExecBcProjection *source,
                                         SZrAotIrProjection *destination) {
    if (source == ZR_NULL || destination == ZR_NULL) return;
    destination->instructionCount = source->instructionCount;
    destination->functionToken = source->functionToken;
    destination->signatureHash = source->signatureHash;
    destination->entryBlockId = source->entryBlockId;
    destination->frameLayoutHash = source->frameLayoutHash;
    destination->opcodes = source->opcodes;
    destination->instructions = source->instructions;
    destination->operands = source->operands;
    destination->operandCount = source->operandCount;
    destination->results = source->results;
    destination->resultCount = source->resultCount;
    destination->memoryTokens = source->memoryTokens;
    destination->memoryTokenCount = source->memoryTokenCount;
    destination->valueSlots = source->valueSlots;
    destination->valueSlotCount = source->valueSlotCount;
    destination->blocks = source->blocks;
    destination->blockCount = source->blockCount;
    destination->syntheticBlockCount = source->syntheticBlockCount;
    destination->predecessors = source->predecessors;
    destination->predecessorCount = source->predecessorCount;
    destination->successors = source->successors;
    destination->successorCount = source->successorCount;
    destination->phis = source->phis;
    destination->phiCount = source->phiCount;
    destination->phiIncomings = source->phiIncomings;
    destination->phiIncomingCount = source->phiIncomingCount;
    destination->phiCopyCount = source->phiCopyCount;
    destination->phiCopySources = source->phiCopySources;
    destination->phiCopyDestinations = source->phiCopyDestinations;
    destination->phiCopyEdges = source->phiCopyEdges;
    destination->temporarySlotCount = source->temporarySlotCount;
    destination->sourceMaps = source->sourceMaps;
    destination->sourceMapCount = source->sourceMapCount;
    destination->gcMapCount = source->gcMapCount;
    destination->deoptStateCount = source->deoptStateCount;
    destination->stateMapPresent = source->stateMapPresent;
    destination->unsupportedInstructionId = source->unsupportedInstructionId;
    destination->runnable = source->runnable;
    destination->ownershipTag = ZR_EXEC_IR_PROJECTION_TAG;
    source->opcodes = ZR_NULL;
    source->instructions = ZR_NULL;
    source->operands = ZR_NULL;
    source->results = ZR_NULL;
    source->memoryTokens = ZR_NULL;
    source->valueSlots = ZR_NULL;
    source->blocks = ZR_NULL;
    source->predecessors = ZR_NULL;
    source->successors = ZR_NULL;
    source->phis = ZR_NULL;
    source->phiIncomings = ZR_NULL;
    source->phiCopySources = ZR_NULL;
    source->phiCopyDestinations = ZR_NULL;
    source->phiCopyEdges = ZR_NULL;
    source->sourceMaps = ZR_NULL;
    source->ownershipTag = 0u;
}
