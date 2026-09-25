#include "exec_ir_verify_effect_backedges.h"

#include <stdlib.h>
#include <string.h>

static void zr_exec_ir_backedge_diag(SZrExecIrDiagnostic *diagnostic,
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

/* A reverse-declared edge is loop-carried only when its target dominates its
 * reachable predecessor. Search backward while excluding the target to find
 * any entry path that bypasses it. */
TZrBool zr_exec_ir_classify_backedges(
        const SZrExecIrFunction *function,
        TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic) {
    TZrBool *backedges = ZR_NULL;
    TZrBool *reachable = ZR_NULL;
    TZrBool *visited = ZR_NULL;
    TZrExecIrBlockId *stack = ZR_NULL;
    TZrUInt32 blockIndex;
    TZrUInt32 iteration;
    TZrBool valid = ZR_TRUE;

    *outBackedges = ZR_NULL;
    if (function->predecessorCount == 0u) return ZR_TRUE;
#if SIZE_MAX <= UINT32_MAX
    if (function->blockCount > SIZE_MAX / sizeof(*stack)) {
        zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               function, 0u, 0u, UINT32_MAX, function->blockCount);
        return ZR_FALSE;
    }
#endif
    backedges = (TZrBool *)calloc(function->predecessorCount, sizeof(*backedges));
    reachable = (TZrBool *)calloc(function->blockCount, sizeof(*reachable));
    visited = (TZrBool *)calloc(function->blockCount, sizeof(*visited));
    stack = (TZrExecIrBlockId *)malloc((size_t)function->blockCount * sizeof(*stack));
    if (backedges == ZR_NULL || reachable == ZR_NULL || visited == ZR_NULL ||
        stack == ZR_NULL) {
        zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, 0u, 0u, 0u, 0u);
        valid = ZR_FALSE;
        goto cleanup;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = block->predecessors.start;
             edgeIndex < block->predecessors.start + block->predecessors.count;
             ++edgeIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[edgeIndex];
            if (predecessor == 0u || predecessor > function->blockCount) {
                zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                       function, block->id, 0u,
                                       function->blockCount, predecessor);
                valid = ZR_FALSE;
                goto cleanup;
            }
        }
    }
    if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        function->entryBlockId > function->blockCount) goto cleanup;
    reachable[function->entryBlockId - 1u] = ZR_TRUE;
    for (iteration = 0u; iteration < function->blockCount; ++iteration) {
        TZrBool changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 edgeIndex;
            if (reachable[blockIndex]) continue;
            for (edgeIndex = block->predecessors.start;
                 edgeIndex < block->predecessors.start + block->predecessors.count;
                 ++edgeIndex) {
                if (reachable[function->predecessors[edgeIndex] - 1u]) {
                    reachable[blockIndex] = changed = ZR_TRUE;
                    break;
                }
            }
        }
        if (!changed) break;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = block->predecessors.start;
             edgeIndex < block->predecessors.start + block->predecessors.count;
             ++edgeIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[edgeIndex];
            TZrUInt32 count = 0u;
            TZrBool bypass = ZR_FALSE;
            if (predecessor < block->id || !reachable[predecessor - 1u]) continue;
            if (predecessor == block->id) {
                backedges[edgeIndex] = ZR_TRUE;
                continue;
            }
            memset(visited, 0, (size_t)function->blockCount * sizeof(*visited));
            visited[predecessor - 1u] = ZR_TRUE;
            stack[count++] = predecessor;
            while (count != 0u && !bypass) {
                TZrExecIrBlockId current = stack[--count];
                const SZrExecIrBlock *currentBlock = &function->blocks[current - 1u];
                TZrUInt32 incomingIndex;
                if (current == function->entryBlockId) {
                    bypass = ZR_TRUE;
                    break;
                }
                for (incomingIndex = currentBlock->predecessors.start;
                     incomingIndex < currentBlock->predecessors.start +
                                         currentBlock->predecessors.count;
                     ++incomingIndex) {
                    TZrExecIrBlockId incoming = function->predecessors[incomingIndex];
                    if (incoming != block->id && !visited[incoming - 1u]) {
                        visited[incoming - 1u] = ZR_TRUE;
                        stack[count++] = incoming;
                    }
                }
            }
            backedges[edgeIndex] = (TZrBool)!bypass;
        }
    }

cleanup:
    free(reachable);
    free(visited);
    free(stack);
    if (valid) *outBackedges = backedges;
    else free(backedges);
    return valid;
}

TZrBool ZrCore_ExecIr_ClassifyBackedges(
        const SZrExecIrFunction *function, TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic) {
    return zr_exec_ir_classify_backedges(function, outBackedges, diagnostic);
}
