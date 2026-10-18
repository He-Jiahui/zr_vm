#include "exec_ir_alias_availability.h"

#include <stdint.h>
#include <stdlib.h>

void *ZrParser_ExecIr_AliasAllocate(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrUInt64 count, size_t width,
        SZrExecIrDiagnostic *diagnostic) {
    void *allocation;
    if (width == 0u || count > (TZrUInt64)(SIZE_MAX / width)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, function,
                0u, 0u, UINT32_MAX, function->valueCount);
        return ZR_NULL;
    }
    if (!ZrParser_ExecIr_PassConsumeBudget(function, context, count)) return ZR_NULL;
    allocation = calloc((size_t)count, width);
    if (allocation == ZR_NULL)
        ZrParser_ExecIr_PassDiagnostic(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, function,
                0u, 0u, function->valueCount, 0u);
    return allocation;
}

static TZrBool zr_alias_work(SZrAliasAvailability *proof) {
    return ZrParser_ExecIr_PassConsumeBudget(proof->function, proof->context, 1u);
}

static TZrBool zr_alias_reset(SZrAliasAvailability *proof) {
    TZrUInt32 index;
    for (index = 0u; index < proof->function->blockCount; ++index) {
        if (!zr_alias_work(proof)) return ZR_FALSE;
        proof->visited[index + 1u] = 0u;
    }
    return ZR_TRUE;
}

/* Reachability with an optional excluded block proves dominance without
 * consulting or publishing the cached immediateDominator fields. */
static TZrBool zr_alias_reaches(SZrAliasAvailability *proof,
        TZrExecIrBlockId target, TZrExecIrBlockId excluded,
        TZrBool recordReachable) {
    SZrExecIrFunction *function = proof->function;
    TZrUInt32 count = 0u;
    if (!zr_alias_reset(proof)) return ZR_FALSE;
    if (function->entryBlockId == excluded) return ZR_FALSE;
    proof->visited[function->entryBlockId] = 1u;
    proof->stack[count++] = function->entryBlockId;
    while (count != 0u) {
        TZrExecIrBlockId id = proof->stack[--count];
        const SZrExecIrBlock *block = &function->blocks[id - 1u];
        TZrUInt32 at;
        if (!zr_alias_work(proof)) return ZR_FALSE;
        if (recordReachable) proof->reachable[id] = 1u;
        if (id == target && !recordReachable) return ZR_TRUE;
        for (at = 0u; at < block->successors.count; ++at) {
            TZrExecIrBlockId next = function->successors[block->successors.start + at];
            if (!zr_alias_work(proof)) return ZR_FALSE;
            if (next == excluded || proof->visited[next] != 0u) continue;
            proof->visited[next] = 1u;
            proof->stack[count++] = next;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_AliasAvailabilityPrepare(SZrAliasAvailability *proof,
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    proof->function = function;
    proof->context = context;
    proof->instructionBlocks = (TZrExecIrBlockId *)ZrParser_ExecIr_AliasAllocate(
            function, context, (TZrUInt64)function->instructionCount + 1u,
            sizeof(*proof->instructionBlocks), diagnostic);
    if (proof->instructionBlocks == ZR_NULL) return ZR_FALSE;
    if (function->blockCount == 0u) {
        proof->valid = ZR_TRUE;
        return ZR_TRUE;
    }
    proof->stack = (TZrExecIrBlockId *)ZrParser_ExecIr_AliasAllocate(function,
            context, function->blockCount, sizeof(*proof->stack), diagnostic);
    if (proof->stack == ZR_NULL) return ZR_FALSE;
    proof->visited = (TZrUInt8 *)ZrParser_ExecIr_AliasAllocate(function,
            context, (TZrUInt64)function->blockCount + 1u,
            sizeof(*proof->visited), diagnostic);
    if (proof->visited == ZR_NULL) return ZR_FALSE;
    proof->reachable = (TZrUInt8 *)ZrParser_ExecIr_AliasAllocate(function,
            context, (TZrUInt64)function->blockCount + 1u,
            sizeof(*proof->reachable), diagnostic);
    if (proof->reachable == ZR_NULL) return ZR_FALSE;
    if (function->entryBlockId == 0u || function->entryBlockId > function->blockCount)
        return ZR_TRUE;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 at;
        if (!zr_alias_work(proof)) return ZR_TRUE;
        if (block->id != index + 1u ||
            !ZrParser_ExecIr_PassRangeValid(block->instructions, function->instructionCount) ||
            !ZrParser_ExecIr_PassRangeValid(block->successors, function->successorCount) ||
            !ZrParser_ExecIr_PassRangeValid(block->predecessors, function->predecessorCount) ||
            (block->terminatorInstructionId != 0u &&
             (block->instructions.count == 0u || block->terminatorInstructionId !=
                     block->instructions.start + block->instructions.count)))
            return ZR_TRUE;
        for (at = 0u; at < block->instructions.count; ++at) {
            TZrUInt32 instruction = block->instructions.start + at + 1u;
            const SZrExecIrInstruction *body = &function->instructions[instruction - 1u];
            const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(body->opcode);
            if (!zr_alias_work(proof)) return ZR_TRUE;
            if (proof->instructionBlocks[instruction] != 0u || info == ZR_NULL ||
                (((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u) !=
                 (at == block->instructions.count - 1u))) return ZR_TRUE;
            proof->instructionBlocks[instruction] = block->id;
        }
        if (block->instructions.count != 0u) {
            const SZrExecIrInstruction *terminator = &function->instructions[
                    block->instructions.start + block->instructions.count - 1u];
            if (terminator->successorRange.count != block->successors.count) return ZR_TRUE;
            for (at = 0u; at < block->successors.count; ++at) {
                if (!zr_alias_work(proof)) return ZR_TRUE;
                if (function->successors[terminator->successorRange.start + at] !=
                    function->successors[block->successors.start + at]) return ZR_TRUE;
            }
        }
        for (at = 0u; at < block->successors.count; ++at) {
            TZrExecIrBlockId next = function->successors[block->successors.start + at];
            if (!zr_alias_work(proof)) return ZR_TRUE;
            if (next == 0u || next > function->blockCount) return ZR_TRUE;
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        if (!zr_alias_work(proof)) return ZR_TRUE;
        if (proof->instructionBlocks[index + 1u] == 0u) return ZR_TRUE;
    }
    (void)zr_alias_reaches(proof, 0u, 0u, ZR_TRUE);
    if (context == ZR_NULL || !context->budgetExhausted) proof->valid = ZR_TRUE;
    return ZR_TRUE;
}

void ZrParser_ExecIr_AliasAvailabilityDispose(SZrAliasAvailability *proof) {
    free(proof->instructionBlocks);
    free(proof->stack);
    free(proof->visited);
    free(proof->reachable);
}

static TZrBool zr_alias_dominates(SZrAliasAvailability *proof,
        TZrExecIrBlockId definition, TZrExecIrBlockId use) {
    if (definition == 0u || use == 0u ||
        definition > proof->function->blockCount || use > proof->function->blockCount ||
        proof->reachable[definition] == 0u || proof->reachable[use] == 0u)
        return ZR_FALSE;
    if (definition == use) return ZR_TRUE;
    if (zr_alias_reaches(proof, use, definition, ZR_FALSE)) return ZR_FALSE;
    return (TZrBool)(proof->context == ZR_NULL || !proof->context->budgetExhausted);
}

/* Mirror Core's exceptional successor closure. Re-entry to the definition
 * stops the earlier exception path: a later normal execution yields a result. */
static TZrBool zr_alias_exception_safe(SZrAliasAvailability *proof,
        const SZrAliasDefinition *definition, TZrExecIrBlockId use,
        TZrExecIrBlockId successor) {
    SZrExecIrFunction *function = proof->function;
    const SZrExecIrInstruction *instruction =
            &function->instructions[definition->instruction - 1u];
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
    SZrExecIrRange edges;
    TZrUInt32 at, count = 0u;
    if (info == ZR_NULL) return ZR_FALSE;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) == 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) == 0u) return ZR_TRUE;
    edges = instruction->successorRange.count != 0u ? instruction->successorRange :
            function->blocks[definition->block - 1u].successors;
    if (!ZrParser_ExecIr_PassRangeValid(edges, function->successorCount) ||
        !zr_alias_reset(proof)) return ZR_FALSE;
    for (at = 0u; at < edges.count; ++at) {
        TZrExecIrBlockId next = function->successors[edges.start + at];
        if (!zr_alias_work(proof)) return ZR_FALSE;
        if (next == 0u || next > function->blockCount) return ZR_FALSE;
        if ((function->blocks[next - 1u].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 0u)
            continue;
        if (use == definition->block && successor == next) return ZR_FALSE;
        if (next == definition->block || proof->visited[next] != 0u) continue;
        proof->visited[next] = 1u;
        proof->stack[count++] = next;
    }
    while (count != 0u) {
        TZrExecIrBlockId id = proof->stack[--count];
        const SZrExecIrBlock *block = &function->blocks[id - 1u];
        if (!zr_alias_work(proof)) return ZR_FALSE;
        if (id == definition->block) continue;
        if (id == use) return ZR_FALSE;
        for (at = 0u; at < block->successors.count; ++at) {
            TZrExecIrBlockId next = function->successors[block->successors.start + at];
            if (!zr_alias_work(proof)) return ZR_FALSE;
            if (next == definition->block || proof->visited[next] != 0u) continue;
            proof->visited[next] = 1u;
            proof->stack[count++] = next;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_AliasAvailable(SZrAliasAvailability *proof,
        const SZrAliasDefinition *definition, TZrExecIrBlockId block,
        TZrExecIrInstructionId instruction, TZrExecIrBlockId successor) {
    SZrExecIrFunction *function = proof->function;
    if (!proof->valid || !zr_alias_work(proof) || definition->kind == ZR_ALIAS_MISSING)
        return ZR_FALSE;
    if (function->blockCount == 0u) {
        if (definition->kind == ZR_ALIAS_EXTERNAL) return ZR_TRUE;
        return (TZrBool)(definition->kind == ZR_ALIAS_INSTRUCTION &&
                definition->instruction < instruction);
    }
    if (block == 0u || block > function->blockCount || proof->reachable[block] == 0u)
        return ZR_FALSE;
    if (definition->kind == ZR_ALIAS_EXTERNAL) return ZR_TRUE;
    if (!zr_alias_dominates(proof, definition->block, block)) return ZR_FALSE;
    if (definition->kind == ZR_ALIAS_PHI_ENTRY) return ZR_TRUE;
    if (definition->kind != ZR_ALIAS_INSTRUCTION || definition->instruction == 0u ||
        definition->instruction > function->instructionCount) return ZR_FALSE;
    if (definition->block == block && successor == 0u &&
        (instruction == 0u || definition->instruction >= instruction)) return ZR_FALSE;
    return zr_alias_exception_safe(proof, definition, block, successor);
}

TZrBool ZrParser_ExecIr_AliasPhiEdge(SZrAliasAvailability *proof,
        const SZrExecIrBlock *target, TZrUInt32 slot,
        TZrExecIrBlockId predecessor) {
    SZrExecIrFunction *function = proof->function;
    const SZrExecIrBlock *block;
    TZrUInt32 occurrence = 0u, matched = 0u, at;
    if (!zr_alias_work(proof) || predecessor == 0u || predecessor > function->blockCount ||
        slot >= target->predecessors.count ||
        function->predecessors[target->predecessors.start + slot] != predecessor)
        return ZR_FALSE;
    block = &function->blocks[predecessor - 1u];
    for (at = 0u; at < slot; ++at) {
        if (!zr_alias_work(proof)) return ZR_FALSE;
        if (function->predecessors[target->predecessors.start + at] == predecessor)
            ++occurrence;
    }
    for (at = 0u; at < block->successors.count; ++at) {
        if (!zr_alias_work(proof)) return ZR_FALSE;
        if (function->successors[block->successors.start + at] == target->id &&
            matched++ == occurrence) return ZR_TRUE;
    }
    return ZR_FALSE;
}
