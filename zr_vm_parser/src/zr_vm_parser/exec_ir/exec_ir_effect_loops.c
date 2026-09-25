#include "exec_ir_effects_internal.h"

#include <stdlib.h>
#include <string.h>

/* Walk predecessors from each latch up to its dominating header. The union
 * of those paths is the natural loop, including nested loops and side paths. */
TZrBool zr_parser_exec_ir_collect_loop_effects(
        const SZrExecIrFunction *function, TZrUInt32 *loopWrites,
        TZrBool *loopEffects, TZrExecIrBlockId *blockOrder,
        TZrBool **outBackedges, TZrBool *supported,
        SZrExecIrDiagnostic *diagnostic) {
    TZrBool *backedges = ZR_NULL;
    TZrBool *visited = ZR_NULL;
    TZrExecIrBlockId *stack = ZR_NULL;
    TZrUInt32 blockIndex;
    TZrBool result = ZR_TRUE;

    *outBackedges = ZR_NULL;
    *supported = ZR_TRUE;
    if (!ZrCore_ExecIr_ClassifyBackedges(function, &backedges, diagnostic))
        return ZR_FALSE;
    visited = (TZrBool *)calloc(function->blockCount, sizeof(*visited));
    stack = (TZrExecIrBlockId *)malloc((size_t)function->blockCount * sizeof(*stack));
    if (visited == ZR_NULL || stack == ZR_NULL) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        result = ZR_FALSE;
        goto cleanup;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *header = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = header->predecessors.start;
             edgeIndex < header->predecessors.start + header->predecessors.count;
             ++edgeIndex) {
            TZrExecIrBlockId latch = function->predecessors[edgeIndex];
            TZrUInt32 forwardIndex;
            TZrUInt32 count = 0u;
            TZrBool hasForward = ZR_FALSE;
            if (!backedges[edgeIndex]) continue;
            if (header->id == function->entryBlockId) {
                *supported = ZR_FALSE;
                goto cleanup;
            }
            for (forwardIndex = header->predecessors.start;
                 forwardIndex < header->predecessors.start + header->predecessors.count;
                 ++forwardIndex) {
                if (!backedges[forwardIndex])
                    hasForward = ZR_TRUE;
            }
            if (!hasForward) {
                *supported = ZR_FALSE;
                goto cleanup;
            }
            for (forwardIndex = header->instructions.start;
                 forwardIndex < header->instructions.start +
                                        header->instructions.count;
                 ++forwardIndex) {
                const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                        (EZrExecIrOpcode)function->instructions[forwardIndex].opcode);
                loopWrites[blockIndex] |= info->memoryWrites;
                if (zr_parser_exec_ir_is_observable(info))
                    loopEffects[blockIndex] = ZR_TRUE;
            }
            memset(visited, 0, (size_t)function->blockCount * sizeof(*visited));
            visited[blockIndex] = ZR_TRUE;
            if (latch != header->id) {
                stack[count++] = latch;
                visited[latch - 1u] = ZR_TRUE;
            }
            while (count != 0u) {
                TZrExecIrBlockId current = stack[--count];
                const SZrExecIrBlock *block = &function->blocks[current - 1u];
                TZrUInt32 instructionIndex;
                TZrUInt32 predecessorIndex;
                for (instructionIndex = block->instructions.start;
                     instructionIndex < block->instructions.start +
                                                block->instructions.count;
                     ++instructionIndex) {
                    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                            (EZrExecIrOpcode)function->instructions[instructionIndex].opcode);
                    loopWrites[blockIndex] |= info->memoryWrites;
                    if (zr_parser_exec_ir_is_observable(info))
                        loopEffects[blockIndex] = ZR_TRUE;
                }
                for (predecessorIndex = block->predecessors.start;
                     predecessorIndex < block->predecessors.start +
                                                block->predecessors.count;
                     ++predecessorIndex) {
                    TZrExecIrBlockId predecessor =
                            function->predecessors[predecessorIndex];
                    if (!visited[predecessor - 1u]) {
                        visited[predecessor - 1u] = ZR_TRUE;
                        stack[count++] = predecessor;
                    }
                }
            }
        }
    }

    /* Removing genuine backedges leaves a dependency graph. Visit a block
     * only when every remaining predecessor has its terminal state ready. */
    memset(visited, 0, (size_t)function->blockCount * sizeof(*visited));
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        TZrUInt32 candidate;
        TZrBool found = ZR_FALSE;
        for (candidate = 0u; candidate < function->blockCount; ++candidate) {
            const SZrExecIrBlock *block = &function->blocks[candidate];
            TZrUInt32 edgeIndex;
            if (visited[candidate]) continue;
            for (edgeIndex = block->predecessors.start;
                 edgeIndex < block->predecessors.start + block->predecessors.count;
                 ++edgeIndex) {
                if (!backedges[edgeIndex] &&
                    !visited[function->predecessors[edgeIndex] - 1u]) break;
            }
            if (edgeIndex == block->predecessors.start +
                                     block->predecessors.count) {
                blockOrder[blockIndex] = block->id;
                visited[candidate] = ZR_TRUE;
                found = ZR_TRUE;
                break;
            }
        }
        if (!found) {
            *supported = ZR_FALSE;
            goto cleanup;
        }
    }

cleanup:
    if (result && *supported) *outBackedges = backedges;
    else free(backedges);
    free(visited);
    free(stack);
    return result;
}
