#include "zr_vm_parser/exec_ir_loops.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ZR_LOOP_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_LOOP_FNV_PRIME UINT64_C(1099511628211)

static void zr_loop_diagnostic(SZrExecIrDiagnostic *diagnostic,
                               EZrExecutionDiagnosticCode code,
                               const SZrExecIrFunction *function,
                               TZrExecIrBlockId blockId,
                               TZrExecIrInstructionId instructionId,
                               TZrUInt64 expected,
                               TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (function != ZR_NULL) {
        diagnostic->functionToken = function->functionToken;
    }
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static TZrBool zr_loop_storage_valid(const SZrExecIrLoopInfo *info) {
    TZrUInt32 magic = 0u;
    TZrUInt32 loopCount = 0u;
    TZrUInt32 loopCapacity = 0u;
    TZrUInt32 memberCount = 0u;
    TZrUInt32 memberCapacity = 0u;
    TZrUInt32 inductionCount = 0u;
    TZrUInt32 inductionCapacity = 0u;
    SZrExecIrLoop *loops = ZR_NULL;
    TZrExecIrBlockId *members = ZR_NULL;
    SZrExecIrInduction *inductions = ZR_NULL;

    if (info == ZR_NULL) return ZR_FALSE;
    /* LoopInfoInit is allowed on first-use stack storage.  Do not evaluate
     * indeterminate fields until the lifecycle marker has been established. */
    memcpy(&magic, &info->magic, sizeof(magic));
    if (magic != ZR_EXEC_IR_LOOP_INFO_MAGIC) return ZR_FALSE;
    memcpy(&loopCount, &info->loopCount, sizeof(loopCount));
    memcpy(&loopCapacity, &info->loopCapacity, sizeof(loopCapacity));
    memcpy(&memberCount, &info->memberCount, sizeof(memberCount));
    memcpy(&memberCapacity, &info->memberCapacity, sizeof(memberCapacity));
    memcpy(&inductionCount, &info->inductionCount, sizeof(inductionCount));
    memcpy(&inductionCapacity, &info->inductionCapacity, sizeof(inductionCapacity));
    if (loopCount > loopCapacity || memberCount > memberCapacity ||
        inductionCount > inductionCapacity) return ZR_FALSE;
    memcpy(&loops, &info->loops, sizeof(loops));
    memcpy(&members, &info->memberBlocks, sizeof(members));
    memcpy(&inductions, &info->inductions, sizeof(inductions));
    return (TZrBool)(((loopCapacity == 0u && loops == ZR_NULL) ||
                      (loopCapacity != 0u && loops != ZR_NULL)) &&
                     ((memberCapacity == 0u && members == ZR_NULL) ||
                      (memberCapacity != 0u && members != ZR_NULL)) &&
                     ((inductionCapacity == 0u && inductions == ZR_NULL) ||
                      (inductionCapacity != 0u && inductions != ZR_NULL)));
}

static void zr_loop_release(SZrExecIrLoopInfo *info) {
    SZrExecIrLoop *loops = ZR_NULL;
    TZrExecIrBlockId *members = ZR_NULL;
    SZrExecIrInduction *inductions = ZR_NULL;
    if (!zr_loop_storage_valid(info)) return;
    memcpy(&loops, &info->loops, sizeof(loops));
    memcpy(&members, &info->memberBlocks, sizeof(members));
    memcpy(&inductions, &info->inductions, sizeof(inductions));
    free(loops);
    free(members);
    free(inductions);
}

void ZrParser_ExecIr_LoopInfoInit(SZrExecIrLoopInfo *info) {
    if (info == ZR_NULL) return;
    zr_loop_release(info);
    memset(info, 0, sizeof(*info));
    info->magic = ZR_EXEC_IR_LOOP_INFO_MAGIC;
    info->schemaVersion = ZR_EXEC_IR_LOOP_SCHEMA_VERSION;
}

void ZrParser_ExecIr_LoopInfoFree(SZrExecIrLoopInfo *info) {
    if (info == ZR_NULL) return;
    zr_loop_release(info);
    memset(info, 0, sizeof(*info));
}

static TZrBool zr_loop_reserve(void **storage, TZrUInt32 *capacity,
                               TZrUInt32 required, size_t elementSize) {
    TZrUInt32 oldCapacity;
    TZrUInt32 newCapacity;
    void *replacement;
    if (storage == ZR_NULL || capacity == ZR_NULL) return ZR_FALSE;
    if (required <= *capacity) return ZR_TRUE;
    oldCapacity = *capacity;
    newCapacity = oldCapacity == 0u ? 4u : oldCapacity;
    while (newCapacity < required) {
        if (newCapacity > UINT32_MAX / 2u) {
            newCapacity = required;
            break;
        }
        newCapacity *= 2u;
    }
    if ((size_t)newCapacity > SIZE_MAX / elementSize) return ZR_FALSE;
    replacement = realloc(*storage, (size_t)newCapacity * elementSize);
    if (replacement == ZR_NULL) return ZR_FALSE;
    *storage = replacement;
    *capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_loop_append_loop(SZrExecIrLoopInfo *info,
                                   const SZrExecIrLoop *loop) {
    if (info == ZR_NULL || loop == ZR_NULL || info->loopCount == UINT32_MAX ||
        !zr_loop_reserve((void **)&info->loops, &info->loopCapacity,
                         info->loopCount + 1u, sizeof(*info->loops))) {
        return ZR_FALSE;
    }
    info->loops[info->loopCount++] = *loop;
    return ZR_TRUE;
}

static TZrBool zr_loop_append_member(SZrExecIrLoopInfo *info,
                                     TZrExecIrBlockId blockId) {
    if (info == ZR_NULL || info->memberCount == UINT32_MAX ||
        !zr_loop_reserve((void **)&info->memberBlocks, &info->memberCapacity,
                         info->memberCount + 1u,
                         sizeof(*info->memberBlocks))) {
        return ZR_FALSE;
    }
    info->memberBlocks[info->memberCount++] = blockId;
    return ZR_TRUE;
}

static TZrBool zr_loop_append_induction(SZrExecIrLoopInfo *info,
                                        const SZrExecIrInduction *induction) {
    if (info == ZR_NULL || induction == ZR_NULL ||
        info->inductionCount == UINT32_MAX ||
        !zr_loop_reserve((void **)&info->inductions,
                         &info->inductionCapacity,
                         info->inductionCount + 1u,
                         sizeof(*info->inductions))) {
        return ZR_FALSE;
    }
    info->inductions[info->inductionCount++] = *induction;
    return ZR_TRUE;
}

static void zr_loop_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt8)(value >> (byte * 8u));
        *hash *= ZR_LOOP_FNV_PRIME;
    }
}

static void zr_loop_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 byte;
    for (byte = 0u; byte < 8u; ++byte) {
        *hash ^= (TZrUInt8)(value >> (byte * 8u));
        *hash *= ZR_LOOP_FNV_PRIME;
    }
}

static TZrUInt64 zr_loop_function_hash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_LOOP_FNV_OFFSET;
    TZrUInt32 index;
    if (function == ZR_NULL) return 0u;
    zr_loop_hash_u32(&hash, function->id);
    zr_loop_hash_u32(&hash, function->functionToken);
    zr_loop_hash_u64(&hash, function->signatureHash);
    zr_loop_hash_u32(&hash, function->instructionCount);
    zr_loop_hash_u32(&hash, function->valueCount);
    zr_loop_hash_u32(&hash, function->blockCount);
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        zr_loop_hash_u32(&hash, instruction->opcode);
        zr_loop_hash_u32(&hash, instruction->flags);
        zr_loop_hash_u32(&hash, instruction->layoutId);
        zr_loop_hash_u32(&hash, instruction->sourceId);
        zr_loop_hash_u32(&hash, instruction->operands.start);
        zr_loop_hash_u32(&hash, instruction->operands.count);
        zr_loop_hash_u32(&hash, instruction->results.start);
        zr_loop_hash_u32(&hash, instruction->results.count);
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        zr_loop_hash_u32(&hash, block->id);
        zr_loop_hash_u32(&hash, block->flags);
        zr_loop_hash_u32(&hash, block->instructionRange.start);
        zr_loop_hash_u32(&hash, block->instructionRange.count);
        zr_loop_hash_u32(&hash, block->predecessorRange.start);
        zr_loop_hash_u32(&hash, block->predecessorRange.count);
        zr_loop_hash_u32(&hash, block->successorRange.start);
        zr_loop_hash_u32(&hash, block->successorRange.count);
    }
    return hash == 0u ? 1u : hash;
}

static TZrBool zr_loop_function_storage_valid(const SZrExecIrFunction *function,
                                              SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL)) {
        zr_loop_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_loop_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_loop_build_reachability(const SZrExecIrFunction *function,
                                          TZrUInt8 *reachable,
                                          TZrUInt8 *reachability,
                                          TZrUInt32 count) {
    TZrExecIrBlockId *stack;
    TZrUInt32 stackCount = 0u;
    TZrUInt32 index;
    if (count == 0u) return ZR_TRUE;
    stack = (TZrExecIrBlockId *)malloc((size_t)count * sizeof(*stack));
    if (stack == ZR_NULL) return ZR_FALSE;
    if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        function->entryBlockId > count) {
        free(stack);
        return ZR_FALSE;
    }
    stack[stackCount++] = function->entryBlockId;
    reachable[function->entryBlockId - 1u] = 2u; /* queued */
    while (stackCount != 0u) {
        TZrExecIrBlockId block = stack[--stackCount];
        const SZrExecIrBlock *record;
        if (block == 0u || block > count || reachable[block - 1u] == 1u) continue;
        reachable[block - 1u] = 1u;
        record = &function->blocks[block - 1u];
        for (index = 0u; index < record->successorRange.count; ++index) {
            TZrExecIrBlockId successor = function->successors[
                    record->successorRange.start + index];
            if (successor != 0u && successor <= count &&
                reachable[successor - 1u] == 0u) {
                reachable[successor - 1u] = 2u;
                stack[stackCount++] = successor;
            }
        }
    }
    /* Transitive closure is small and deterministic for the analysis result. */
    for (index = 0u; index < count; ++index) {
        TZrUInt32 column;
        if (reachable[index] == 0u) continue;
        reachability[index * count + index] = 1u;
        for (column = 0u; column < function->blocks[index].successorRange.count;
             ++column) {
            TZrExecIrBlockId successor = function->successors[
                    function->blocks[index].successorRange.start + column];
            if (successor != 0u && successor <= count &&
                reachable[successor - 1u] != 0u) {
                reachability[index * count + successor - 1u] = 1u;
            }
        }
    }
    /* Floyd-Warshall closure keeps SCC classification deterministic even
     * when a path's intermediate block id is numerically greater than its
     * source/target ids. */
    for (TZrUInt32 middle = 0u; middle < count; ++middle) {
        for (index = 0u; index < count; ++index) {
            TZrUInt32 column;
            if (reachability[index * count + middle] == 0u) continue;
            for (column = 0u; column < count; ++column) {
                if (reachability[middle * count + column] != 0u)
                    reachability[index * count + column] = 1u;
            }
        }
    }
    free(stack);
    return ZR_TRUE;
}

static TZrBool zr_loop_compute_dominators(const SZrExecIrFunction *function,
                                          const TZrUInt8 *reachable,
                                          TZrUInt8 *dominators,
                                          TZrUInt32 count) {
    TZrUInt32 row;
    TZrUInt32 column;
    TZrBool changed = ZR_TRUE;
    for (row = 0u; row < count; ++row) {
        if (reachable[row] == 0u) continue;
        if (row + 1u == function->entryBlockId) {
            dominators[row * count + row] = 1u;
        } else {
            for (column = 0u; column < count; ++column)
                dominators[row * count + column] = reachable[column];
        }
    }
    while (changed) {
        changed = ZR_FALSE;
        for (row = 0u; row < count; ++row) {
            const SZrExecIrBlock *block;
            TZrUInt8 *destination;
            TZrUInt8 *intersection;
            TZrUInt32 predecessorIndex;
            TZrBool sawPredecessor = ZR_FALSE;
            if (reachable[row] == 0u || row + 1u == function->entryBlockId)
                continue;
            block = &function->blocks[row];
            destination = &dominators[row * count];
            intersection = (TZrUInt8 *)calloc(count, sizeof(*intersection));
            if (intersection == ZR_NULL) return ZR_FALSE;
            for (predecessorIndex = 0u;
                 predecessorIndex < block->predecessorRange.count;
                 ++predecessorIndex) {
                TZrExecIrBlockId predecessor = function->predecessors[
                        block->predecessorRange.start + predecessorIndex];
                TZrUInt32 candidate;
                if (predecessor == 0u || predecessor > count ||
                    reachable[predecessor - 1u] == 0u) continue;
                if (!sawPredecessor) {
                    for (candidate = 0u; candidate < count; ++candidate)
                        intersection[candidate] =
                                dominators[(predecessor - 1u) * count + candidate];
                    sawPredecessor = ZR_TRUE;
                } else {
                    for (candidate = 0u; candidate < count; ++candidate)
                        intersection[candidate] = (TZrUInt8)(
                                intersection[candidate] != 0u &&
                                dominators[(predecessor - 1u) * count + candidate] != 0u);
                }
            }
            if (!sawPredecessor) {
                memset(intersection, 0, count * sizeof(*intersection));
            }
            intersection[row] = 1u;
            for (column = 0u; column < count; ++column) {
                if (destination[column] != intersection[column]) {
                    destination[column] = intersection[column];
                    changed = ZR_TRUE;
                }
            }
            free(intersection);
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_loop_constant_value(const SZrExecIrFunction *function,
                                      TZrExecIrValueId valueId,
                                      TZrUInt64 *bits) {
    TZrExecIrInstructionId definition;
    const SZrExecIrInstruction *instruction;
    if (function == ZR_NULL || bits == ZR_NULL || valueId == 0u ||
        valueId > function->valueCount || function->values == ZR_NULL)
        return ZR_FALSE;
    definition = function->values[valueId - 1u].definition;
    if (definition == 0u || definition > function->instructionCount ||
        function->instructions == ZR_NULL)
        return ZR_FALSE;
    instruction = &function->instructions[definition - 1u];
    if (instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT ||
        instruction->results.count != 1u) return ZR_FALSE;
    *bits = instruction->layoutId;
    return ZR_TRUE;
}

static TZrBool zr_loop_value_defined_in_loop(const SZrExecIrFunction *function,
                                             const SZrExecIrLoopInfo *info,
                                             const SZrExecIrLoop *loop,
                                             TZrExecIrValueId valueId) {
    TZrExecIrInstructionId definition;
    TZrUInt32 memberIndex;
    const TZrExecIrBlockId *members;
    if (function == ZR_NULL || info == ZR_NULL || loop == ZR_NULL ||
        valueId == 0u || valueId > function->valueCount ||
        function->values == ZR_NULL) return ZR_TRUE;
    definition = function->values[valueId - 1u].definition;
    if (definition == 0u) return ZR_FALSE;
    members = info->memberBlocks + loop->memberOffset;
    for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
        const SZrExecIrBlock *block = &function->blocks[members[memberIndex] - 1u];
        if (definition - 1u >= block->instructionRange.start &&
            definition - 1u < block->instructionRange.start +
                                      block->instructionRange.count)
            return ZR_TRUE;
    }
    return ZR_FALSE;
}

static void zr_loop_trip_count(const SZrExecIrFunction *function,
                               SZrExecIrLoop *loop) {
    const SZrExecIrBlock *header;
    const SZrExecIrInstruction *terminator;
    TZrExecIrValueId condition;
    TZrUInt64 value;
    if (function == ZR_NULL || loop == ZR_NULL || loop->headerBlockId == 0u ||
        loop->headerBlockId > function->blockCount) return;
    loop->tripCountMin = 0u;
    loop->tripCountMax = ZR_EXEC_IR_LOOP_TRIP_COUNT_UNKNOWN;
    loop->tripCountKnown = ZR_FALSE;
    loop->zeroTrip = ZR_FALSE;
    header = &function->blocks[loop->headerBlockId - 1u];
    if (header->instructionRange.count == 0u) return;
    terminator = &function->instructions[header->instructionRange.start +
                                          header->instructionRange.count - 1u];
    if (terminator->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        terminator->operands.count != 1u || function->operands == ZR_NULL)
        return;
    condition = function->operands[terminator->operands.start];
    if (!zr_loop_constant_value(function, condition, &value)) return;
    loop->tripCountKnown = ZR_TRUE;
    if (value == 0u) {
        loop->tripCountMax = 0u;
        loop->zeroTrip = ZR_TRUE;
    } else {
        loop->tripCountMin = 1u;
    }
}

static TZrBool zr_loop_record_induction(const SZrExecIrFunction *function,
                                        const TZrUInt8 *members,
                                        TZrUInt32 blockCount,
                                        const SZrExecIrLoop *loop,
                                        SZrExecIrLoopInfo *info) {
    TZrUInt32 memberIndex;
    if (function == ZR_NULL || members == ZR_NULL || loop == ZR_NULL ||
        info == ZR_NULL) return ZR_FALSE;
    (void)members;
    for (memberIndex = 0u;
         memberIndex < loop->memberCount;
         ++memberIndex) {
        TZrExecIrBlockId blockId = info->memberBlocks[loop->memberOffset + memberIndex];
        const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
        TZrUInt32 instructionIndex;
        (void)blockCount;
        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                 block->instructionRange.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            TZrExecIrValueId left;
            TZrExecIrValueId right;
            TZrUInt64 constant;
            TZrExecIrValueId base;
            SZrExecIrInduction induction;
            if ((instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD &&
                 instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB &&
                 instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_MUL) ||
                instruction->operands.count != 2u ||
                instruction->results.count != 1u || function->operands == ZR_NULL ||
                function->results == ZR_NULL)
                continue;
            left = function->operands[instruction->operands.start];
            right = function->operands[instruction->operands.start + 1u];
            if (zr_loop_constant_value(function, right, &constant) &&
                !zr_loop_value_defined_in_loop(function, info, loop, right)) {
                base = left;
            } else if (instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB &&
                       zr_loop_constant_value(function, left, &constant) &&
                       !zr_loop_value_defined_in_loop(function, info, loop, left)) {
                base = right;
            } else {
                continue;
            }
            memset(&induction, 0, sizeof(induction));
            induction.loopId = loop->id;
            induction.valueId = function->results[instruction->results.start];
            induction.baseValueId = base;
            induction.instructionId = instructionIndex + 1u;
            induction.sourceId = instruction->sourceId;
            induction.operation = instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD
                                      ? ZR_EXEC_IR_INDUCTION_ADD
                                      : instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB
                                            ? ZR_EXEC_IR_INDUCTION_SUB
                                            : ZR_EXEC_IR_INDUCTION_MUL;
            induction.factor = (TZrInt64)constant;
            induction.step = induction.operation == ZR_EXEC_IR_INDUCTION_SUB
                                 ? -(TZrInt64)constant
                                 : (TZrInt64)constant;
            /* Without a range proof, only identity/zero recurrences are
             * overflow-safe.  The candidate remains visible for a later
             * range-aware pass, but is never rewritten here. */
            induction.overflowSafe = (TZrBool)(constant == 0u || constant == 1u);
            induction.strengthReducible = (TZrBool)(
                    induction.operation == ZR_EXEC_IR_INDUCTION_MUL &&
                    (constant == 0u || constant == 1u));
            if (!zr_loop_append_induction(info, &induction)) return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_loop_find_existing_header(const SZrExecIrLoopInfo *info,
                                            TZrExecIrBlockId header,
                                            TZrExecIrLoopId *loopId) {
    TZrUInt32 index;
    if (info == ZR_NULL || loopId == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < info->loopCount; ++index) {
        if (info->loops[index].headerBlockId == header) {
            *loopId = info->loops[index].id;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_loop_add_natural_loop(const SZrExecIrFunction *function,
                                        const TZrUInt8 *dominators,
                                        TZrUInt32 count,
                                        TZrExecIrBlockId headerId,
                                        TZrExecIrBlockId latchId,
                                        SZrExecIrLoopInfo *info) {
    TZrUInt8 *members;
    TZrExecIrBlockId *stack;
    TZrUInt32 stackCount = 0u;
    TZrUInt32 memberIndex;
    TZrExecIrLoopId existingId;
    SZrExecIrLoop loop;
    if (zr_loop_find_existing_header(info, headerId, &existingId)) {
        SZrExecIrLoop *existing = &info->loops[existingId - 1u];
        if (latchId > existing->latchBlockId) existing->latchBlockId = latchId;
        return ZR_TRUE;
    }
    members = (TZrUInt8 *)calloc(count, sizeof(*members));
    stack = (TZrExecIrBlockId *)malloc((size_t)count * sizeof(*stack));
    if (members == ZR_NULL || stack == ZR_NULL) {
        free(members);
        free(stack);
        return ZR_FALSE;
    }
    members[headerId - 1u] = 1u;
    members[latchId - 1u] = 1u;
    stack[stackCount++] = latchId;
    while (stackCount != 0u) {
        TZrExecIrBlockId blockId = stack[--stackCount];
        const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
        for (memberIndex = 0u; memberIndex < block->predecessorRange.count;
             ++memberIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[
                    block->predecessorRange.start + memberIndex];
            if (predecessor == 0u || predecessor > count ||
                members[predecessor - 1u] != 0u) continue;
            members[predecessor - 1u] = 1u;
            if (predecessor != headerId) stack[stackCount++] = predecessor;
        }
    }
    memset(&loop, 0, sizeof(loop));
    loop.id = info->loopCount + 1u;
    loop.headerBlockId = headerId;
    loop.latchBlockId = latchId;
    loop.memberOffset = info->memberCount;
    for (memberIndex = 0u; memberIndex < count; ++memberIndex) {
        TZrUInt32 predecessorIndex;
        TZrUInt32 outside = 0u;
        if (members[memberIndex] == 0u) continue;
        if (!zr_loop_append_member(info, memberIndex + 1u)) {
            free(members);
            free(stack);
            return ZR_FALSE;
        }
        loop.memberCount++;
        for (predecessorIndex = 0u;
             predecessorIndex < function->blocks[memberIndex].predecessorRange.count;
             ++predecessorIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[
                    function->blocks[memberIndex].predecessorRange.start + predecessorIndex];
            if (predecessor == 0u || predecessor > count ||
                members[predecessor - 1u] == 0u) outside++;
        }
        if (outside != 0u) {
            if (memberIndex + 1u == headerId && outside == 1u) {
                loop.preheaderBlockId = function->predecessors[
                        function->blocks[memberIndex].predecessorRange.start];
                loop.hasPreheader = ZR_TRUE;
            }
            if (memberIndex + 1u != headerId || outside > 1u)
                loop.multipleEntry = ZR_TRUE;
        }
    }
    loop.reducible = (TZrBool)(!loop.multipleEntry && loop.hasPreheader);
    if (!loop.hasPreheader)
        loop.blockedReason = ZR_EXEC_IR_LOOP_REASON_NO_PREHEADER;
    else if (loop.multipleEntry)
        loop.blockedReason = ZR_EXEC_IR_LOOP_REASON_MULTIPLE_ENTRY;
    zr_loop_trip_count(function, &loop);
    if (loop.zeroTrip) loop.blockedReason = ZR_EXEC_IR_LOOP_REASON_ZERO_TRIP;
    loop.inductionOffset = info->inductionCount;
    if (!zr_loop_append_loop(info, &loop)) {
        free(members);
        free(stack);
        return ZR_FALSE;
    }
    if (!zr_loop_record_induction(function, members, count, &loop, info)) {
        free(members);
        free(stack);
        return ZR_FALSE;
    }
    /* The induction rows appended above belong only to this loop. */
    info->loops[loop.id - 1u].inductionOffset = loop.inductionOffset;
    info->loops[loop.id - 1u].inductionCount = info->inductionCount -
                                                loop.inductionOffset;
    free(members);
    free(stack);
    (void)dominators;
    return ZR_TRUE;
}

static TZrBool zr_loop_add_irreducible_scc(const SZrExecIrFunction *function,
                                           const TZrUInt8 *reachable,
                                           const TZrUInt8 *reachability,
                                           TZrUInt32 count,
                                           SZrExecIrLoopInfo *info) {
    TZrUInt8 *assigned = (TZrUInt8 *)calloc(count, sizeof(*assigned));
    TZrUInt32 seed;
    if (assigned == ZR_NULL) return ZR_FALSE;
    for (seed = 0u; seed < count; ++seed) {
        TZrUInt32 member;
        SZrExecIrLoop loop;
        TZrBool coveredByNatural = ZR_FALSE;
        if (reachable[seed] == 0u || assigned[seed] != 0u) continue;
        if (reachability[seed * count + seed] == 0u) continue;
        /* A self-reachability bit is always present; require an actual cycle
         * (a distinct mutually reachable node or an explicit self edge). */
        {
            TZrBool cyclic = ZR_FALSE;
            for (member = 0u; member < count; ++member) {
                if (member != seed &&
                    reachability[seed * count + member] != 0u &&
                    reachability[member * count + seed] != 0u) {
                    cyclic = ZR_TRUE;
                    break;
                }
            }
            if (!cyclic) {
                const SZrExecIrBlock *block = &function->blocks[seed];
                for (member = 0u; member < block->successorRange.count; ++member) {
                    if (function->successors[block->successorRange.start + member] ==
                        seed + 1u) {
                        cyclic = ZR_TRUE;
                        break;
                    }
                }
            }
            if (!cyclic) continue;
        }
        /* Natural-loop discovery already describes reducible SCCs.  Do not
         * publish a duplicate irreducible record for the same component. */
        for (member = 0u; member < info->loopCount; ++member) {
            if (info->loops[member].headerBlockId == seed + 1u) {
                coveredByNatural = ZR_TRUE;
                break;
            }
        }
        if (coveredByNatural) {
            for (member = seed; member < count; ++member) {
                if (reachable[member] != 0u &&
                    reachability[seed * count + member] != 0u &&
                    reachability[member * count + seed] != 0u)
                    assigned[member] = 1u;
            }
            continue;
        }
        for (member = 0u; member < info->loopCount && !coveredByNatural;
             ++member) {
            const SZrExecIrLoop *existing = &info->loops[member];
            TZrUInt32 componentCount = 0u;
            TZrUInt32 existingIndex;
            for (existingIndex = 0u; existingIndex < count; ++existingIndex) {
                if (reachable[existingIndex] != 0u &&
                    reachability[seed * count + existingIndex] != 0u &&
                    reachability[existingIndex * count + seed] != 0u)
                    componentCount++;
            }
            if (existing->memberCount != componentCount) continue;
            {
                const TZrExecIrBlockId *existingMembers =
                        info->memberBlocks + existing->memberOffset;
                TZrUInt32 existingMemberIndex;
                coveredByNatural = ZR_TRUE;
                for (existingMemberIndex = 0u;
                     existingMemberIndex < existing->memberCount;
                     ++existingMemberIndex) {
                    TZrUInt32 blockIndex = existingMembers[existingMemberIndex];
                    if (blockIndex == 0u || blockIndex > count ||
                        reachability[seed * count + blockIndex - 1u] == 0u ||
                        reachability[(blockIndex - 1u) * count + seed] == 0u) {
                        coveredByNatural = ZR_FALSE;
                        break;
                    }
                }
            }
        }
        if (coveredByNatural) {
            for (member = seed; member < count; ++member) {
                if (reachable[member] != 0u &&
                    reachability[seed * count + member] != 0u &&
                    reachability[member * count + seed] != 0u)
                    assigned[member] = 1u;
            }
            continue;
        }
        memset(&loop, 0, sizeof(loop));
        loop.id = info->loopCount + 1u;
        loop.headerBlockId = seed + 1u;
        loop.latchBlockId = seed + 1u;
        loop.memberOffset = info->memberCount;
        loop.reducible = ZR_FALSE;
        loop.multipleEntry = ZR_TRUE;
        loop.blockedReason = ZR_EXEC_IR_LOOP_REASON_IRREDUCIBLE;
        for (member = seed; member < count; ++member) {
            if (reachable[member] != 0u &&
                reachability[seed * count + member] != 0u &&
                reachability[member * count + seed] != 0u) {
                assigned[member] = 1u;
                if (!zr_loop_append_member(info, member + 1u)) {
                    free(assigned);
                    return ZR_FALSE;
                }
                loop.memberCount++;
            }
        }
        if (!zr_loop_append_loop(info, &loop)) {
            free(assigned);
            return ZR_FALSE;
        }
    }
    free(assigned);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_AnalyzeLoops(const SZrExecIrFunction *function,
                                     SZrExecIrLoopInfo *info,
                                     SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrLoopInfo temporary;
    TZrUInt32 blockCount;
    TZrUInt8 *reachable = ZR_NULL;
    TZrUInt8 *reachability = ZR_NULL;
    TZrUInt8 *dominators = ZR_NULL;
    TZrUInt32 source;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (info == ZR_NULL ||
        !zr_loop_function_storage_valid(function, diagnostic)) return ZR_FALSE;
    if (!zr_loop_storage_valid(info)) ZrParser_ExecIr_LoopInfoInit(info);
    ZrParser_ExecIr_LoopInfoInit(&temporary);
    temporary.irHash = zr_loop_function_hash(function);
    blockCount = function->blockCount;
    if (blockCount == 0u) {
        ZrParser_ExecIr_LoopInfoFree(info);
        *info = temporary;
        memset(&temporary, 0, sizeof(temporary));
        return ZR_TRUE;
    }
    if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        function->entryBlockId > blockCount ||
        function->instructionCount > function->instructionCapacity) {
        zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                           function, function->entryBlockId, 0u, blockCount,
                           function->entryBlockId);
        ZrParser_ExecIr_LoopInfoFree(&temporary);
        return ZR_FALSE;
    }
    for (source = 0u; source < blockCount; ++source) {
        const SZrExecIrBlock *block = &function->blocks[source];
        TZrUInt32 edgeIndex;
        if (!zr_loop_range_valid(block->instructionRange,
                                 function->instructionCount) ||
            !zr_loop_range_valid(block->predecessorRange,
                                 function->predecessorCount) ||
            !zr_loop_range_valid(block->successorRange,
                                 function->successorCount)) {
            zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                               function, block->id, 0u, 0u, 0u);
            ZrParser_ExecIr_LoopInfoFree(&temporary);
            return ZR_FALSE;
        }
        for (edgeIndex = 0u; edgeIndex < block->successorRange.count;
             ++edgeIndex) {
            TZrExecIrBlockId successor = function->successors[
                    block->successorRange.start + edgeIndex];
            if (successor == ZR_EXEC_IR_BLOCK_ID_INVALID || successor > blockCount) {
                zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                   function, block->id, 0u, blockCount, successor);
                ZrParser_ExecIr_LoopInfoFree(&temporary);
                return ZR_FALSE;
            }
        }
        for (edgeIndex = 0u; edgeIndex < block->predecessorRange.count;
             ++edgeIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[
                    block->predecessorRange.start + edgeIndex];
            if (predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID || predecessor > blockCount) {
                zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                   function, block->id, 0u, blockCount, predecessor);
                ZrParser_ExecIr_LoopInfoFree(&temporary);
                return ZR_FALSE;
            }
        }
    }
    if ((size_t)blockCount > SIZE_MAX / (size_t)blockCount) {
        zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           function, 0u, 0u, SIZE_MAX, blockCount);
        ZrParser_ExecIr_LoopInfoFree(&temporary);
        return ZR_FALSE;
    }
    reachable = (TZrUInt8 *)calloc(blockCount, sizeof(*reachable));
    reachability = (TZrUInt8 *)calloc((size_t)blockCount * blockCount,
                                      sizeof(*reachability));
    dominators = (TZrUInt8 *)calloc((size_t)blockCount * blockCount,
                                    sizeof(*dominators));
    if (reachable == ZR_NULL || reachability == ZR_NULL || dominators == ZR_NULL ||
        !zr_loop_build_reachability(function, reachable, reachability,
                                     blockCount) ||
        !zr_loop_compute_dominators(function, reachable, dominators,
                                     blockCount)) {
        free(reachable);
        free(reachability);
        free(dominators);
        zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, 0u, 0u, blockCount, 0u);
        ZrParser_ExecIr_LoopInfoFree(&temporary);
        return ZR_FALSE;
    }
    for (source = 0u; source < blockCount; ++source) {
        const SZrExecIrBlock *block = &function->blocks[source];
        TZrUInt32 edgeIndex;
        for (edgeIndex = 0u; edgeIndex < block->successorRange.count;
             ++edgeIndex) {
            TZrExecIrBlockId target = function->successors[
                    block->successorRange.start + edgeIndex];
            if (target != 0u && target <= blockCount &&
                reachable[target - 1u] != 0u &&
                dominators[(source) * blockCount + target - 1u] != 0u) {
                if (!zr_loop_add_natural_loop(function, dominators, blockCount,
                                               target, source + 1u, &temporary)) {
                    free(reachable);
                    free(reachability);
                    free(dominators);
                    ZrParser_ExecIr_LoopInfoFree(&temporary);
                    zr_loop_diagnostic(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, target, 0u, 0u, 0u);
                    return ZR_FALSE;
                }
            }
        }
    }
    if (!zr_loop_add_irreducible_scc(function, reachable, reachability,
                                     blockCount, &temporary)) {
        free(reachable);
        free(reachability);
        free(dominators);
        ZrParser_ExecIr_LoopInfoFree(&temporary);
        zr_loop_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    free(reachable);
    free(reachability);
    free(dominators);
    ZrParser_ExecIr_LoopInfoFree(info);
    *info = temporary;
    memset(&temporary, 0, sizeof(temporary));
    return ZR_TRUE;
}

const SZrExecIrLoop *ZrParser_ExecIr_LoopAt(const SZrExecIrLoopInfo *info,
                                            TZrExecIrLoopId loopId) {
    if (!zr_loop_storage_valid(info) || loopId == 0u ||
        loopId > info->loopCount) return ZR_NULL;
    return &info->loops[loopId - 1u];
}

const TZrExecIrBlockId *ZrParser_ExecIr_LoopMembers(
        const SZrExecIrLoopInfo *info, const SZrExecIrLoop *loop) {
    if (!zr_loop_storage_valid(info) || loop == ZR_NULL ||
        loop->memberOffset > info->memberCount ||
        loop->memberCount > info->memberCount - loop->memberOffset ||
        (loop->memberCount != 0u && info->memberBlocks == ZR_NULL)) return ZR_NULL;
    return info->memberBlocks + loop->memberOffset;
}

const SZrExecIrInduction *ZrParser_ExecIr_InductionAt(
        const SZrExecIrLoopInfo *info, TZrUInt32 index) {
    if (!zr_loop_storage_valid(info) || index >= info->inductionCount ||
        info->inductions == ZR_NULL) return ZR_NULL;
    return &info->inductions[index];
}

const TZrChar *ZrParser_ExecIr_LoopReasonName(EZrExecIrLoopReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_LOOP_REASON_NONE: return "none";
        case ZR_EXEC_IR_LOOP_REASON_NO_PREHEADER: return "no-preheader";
        case ZR_EXEC_IR_LOOP_REASON_MULTIPLE_ENTRY: return "multiple-entry";
        case ZR_EXEC_IR_LOOP_REASON_IRREDUCIBLE: return "irreducible";
        case ZR_EXEC_IR_LOOP_REASON_ZERO_TRIP: return "zero-trip";
        case ZR_EXEC_IR_LOOP_REASON_TRAPPING: return "trapping";
        case ZR_EXEC_IR_LOOP_REASON_EFFECT: return "effect";
        case ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT: return "not-invariant";
        case ZR_EXEC_IR_LOOP_REASON_OVERFLOW: return "overflow";
        case ZR_EXEC_IR_LOOP_REASON_BUDGET: return "budget";
        case ZR_EXEC_IR_LOOP_REASON_SEALED: return "sealed";
        case ZR_EXEC_IR_LOOP_REASON_INVALID:
        default: return "invalid";
    }
}

/* The LICM implementation lives in exec_ir_licm.c.  Keeping this weakly
 * coupled wrapper in the analysis module avoids exposing its mutation helpers
 * in the public analysis API. */
