#include "zr_vm_parser/exec_ir_builder.h"

#include <stdlib.h>
#include <string.h>

static TZrBool cfg_range_valid(SZrExecIrRange range, TZrUInt32 size) {
    return (TZrBool)(range.start <= size && range.count <= size - range.start);
}

static TZrBool cfg_validate_edges(const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 blockIndex;
    if (f->blockCount > f->blockCapacity || f->blocks == ZR_NULL ||
        f->predecessorCount > f->predecessorCapacity ||
        f->successorCount > f->successorCapacity ||
        (f->predecessorCount != 0u && f->predecessors == ZR_NULL) ||
        (f->successorCount != 0u && f->successors == ZR_NULL)) {
        if (d != ZR_NULL) d->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    for (blockIndex = 0u; blockIndex < f->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &f->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        if (block->id != blockIndex + 1u ||
            !cfg_range_valid(block->successorRange, f->successorCount) ||
            !cfg_range_valid(block->predecessorRange, f->predecessorCount)) {
            if (d != ZR_NULL) {
                d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                d->blockId = blockIndex + 1u;
            }
            return ZR_FALSE;
        }
        for (edgeIndex = block->successorRange.start;
             edgeIndex < block->successorRange.start + block->successorRange.count;
             ++edgeIndex) {
            TZrExecIrBlockId to = f->successors[edgeIndex];
            if (to == ZR_EXEC_IR_BLOCK_ID_INVALID || to > f->blockCount) {
                if (d != ZR_NULL) {
                    d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                    d->blockId = block->id;
                    d->expectedVersion = f->blockCount;
                    d->actualVersion = to;
                }
                return ZR_FALSE;
            }
        }
        for (edgeIndex = block->predecessorRange.start;
             edgeIndex < block->predecessorRange.start + block->predecessorRange.count;
             ++edgeIndex) {
            TZrExecIrBlockId from = f->predecessors[edgeIndex];
            if (from == ZR_EXEC_IR_BLOCK_ID_INVALID || from > f->blockCount) {
                if (d != ZR_NULL) {
                    d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                    d->blockId = block->id;
                    d->expectedVersion = f->blockCount;
                    d->actualVersion = from;
                }
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

typedef struct SZrCfgDfsFrame {
    TZrExecIrBlockId blockId;
    TZrUInt32 nextSuccessor;
} SZrCfgDfsFrame;

static TZrExecIrBlockId cfg_intersect(const TZrExecIrBlockId *idoms,
                                      const TZrUInt32 *rank,
                                      TZrExecIrBlockId a,
                                      TZrExecIrBlockId b,
                                      TZrUInt32 blockCount) {
    TZrSize steps = 0u;
    while (a != b) {
        if (++steps > (TZrSize)blockCount * 2u) {
            return ZR_EXEC_IR_BLOCK_ID_INVALID;
        }
        while (rank[a - 1u] > rank[b - 1u]) a = idoms[a - 1u];
        while (rank[b - 1u] > rank[a - 1u]) b = idoms[b - 1u];
    }
    return a;
}

TZrBool ZrParser_ExecIr_ComputeDominators(SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 n, i, postCount = 0u, depth = 0u;
    TZrUInt32 *rank = ZR_NULL;
    TZrExecIrBlockId *order = ZR_NULL;
    TZrExecIrBlockId *idoms = ZR_NULL;
    SZrCfgDfsFrame *stack = ZR_NULL;
    unsigned char *visited = ZR_NULL;
    TZrBool result = ZR_FALSE;

    if (d != ZR_NULL) memset(d, 0, sizeof(*d));
    if (f == ZR_NULL || f->blockCount == 0u ||
        f->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        f->entryBlockId > f->blockCount) {
        if (d != ZR_NULL) d->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    if (d != ZR_NULL) d->functionToken = f->functionToken;
    if (!cfg_validate_edges(f, d)) return ZR_FALSE;
    n = f->blockCount;
#if SIZE_MAX <= UINT32_MAX
    if ((TZrSize)n > SIZE_MAX / sizeof(*stack)) {
        if (d != ZR_NULL) d->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        return ZR_FALSE;
    }
#endif
    rank = (TZrUInt32 *)calloc(n, sizeof(*rank));
    order = (TZrExecIrBlockId *)malloc((TZrSize)n * sizeof(*order));
    idoms = (TZrExecIrBlockId *)calloc(n, sizeof(*idoms));
    stack = (SZrCfgDfsFrame *)malloc((TZrSize)n * sizeof(*stack));
    visited = (unsigned char *)calloc(n, sizeof(*visited));
    if (rank == ZR_NULL || order == ZR_NULL || idoms == ZR_NULL ||
        stack == ZR_NULL || visited == ZR_NULL) {
        if (d != ZR_NULL) d->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
        goto cleanup;
    }

    visited[f->entryBlockId - 1u] = 1u;
    stack[depth++] = (SZrCfgDfsFrame){f->entryBlockId, 0u};
    while (depth != 0u) {
        SZrCfgDfsFrame *frame = &stack[depth - 1u];
        const SZrExecIrBlock *block = &f->blocks[frame->blockId - 1u];
        if (frame->nextSuccessor < block->successorRange.count) {
            TZrExecIrBlockId next = f->successors[
                block->successorRange.start + frame->nextSuccessor++];
            if (visited[next - 1u] == 0u) {
                visited[next - 1u] = 1u;
                stack[depth++] = (SZrCfgDfsFrame){next, 0u};
            }
        } else {
            order[postCount++] = frame->blockId;
            --depth;
        }
    }
    /* DFS completion order reversed is the CHK iteration order. */
    for (i = 0u; i < postCount / 2u; ++i) {
        TZrExecIrBlockId temporary = order[i];
        order[i] = order[postCount - i - 1u];
        order[postCount - i - 1u] = temporary;
    }
    for (i = 0u; i < postCount; ++i) rank[order[i] - 1u] = i + 1u;
    idoms[f->entryBlockId - 1u] = f->entryBlockId;
    {
        TZrBool changed;
        do {
            changed = ZR_FALSE;
            for (i = 1u; i < postCount; ++i) {
                TZrExecIrBlockId blockId = order[i];
                const SZrExecIrBlock *block = &f->blocks[blockId - 1u];
                TZrExecIrBlockId newIdom = ZR_EXEC_IR_BLOCK_ID_INVALID;
                TZrUInt32 edgeIndex;
                for (edgeIndex = block->predecessorRange.start;
                     edgeIndex < block->predecessorRange.start + block->predecessorRange.count;
                     ++edgeIndex) {
                    TZrExecIrBlockId predecessor = f->predecessors[edgeIndex];
                    if (rank[predecessor - 1u] == 0u ||
                        idoms[predecessor - 1u] == ZR_EXEC_IR_BLOCK_ID_INVALID) continue;
                    newIdom = newIdom == ZR_EXEC_IR_BLOCK_ID_INVALID
                                   ? predecessor
                                   : cfg_intersect(idoms, rank, newIdom, predecessor, n);
                    if (newIdom == ZR_EXEC_IR_BLOCK_ID_INVALID) break;
                }
                if (newIdom == ZR_EXEC_IR_BLOCK_ID_INVALID) {
                    if (d != ZR_NULL) {
                        d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                        d->blockId = blockId;
                    }
                    goto cleanup;
                }
                if (idoms[blockId - 1u] != newIdom) {
                    idoms[blockId - 1u] = newIdom;
                    changed = ZR_TRUE;
                }
            }
        } while (changed);
    }
    for (i = 0u; i < n; ++i) f->blocks[i].immediateDominator = idoms[i];
    result = ZR_TRUE;

cleanup:
    free(rank);
    free(order);
    free(idoms);
    free(stack);
    free(visited);
    return result;
}
