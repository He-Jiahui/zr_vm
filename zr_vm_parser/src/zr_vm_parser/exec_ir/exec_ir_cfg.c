#include "zr_vm_parser/exec_ir_builder.h"

#include <stdlib.h>
#include <string.h>

static TZrExecIrBlockId intersect(const SZrExecIrFunction *f, const TZrUInt32 *rpo,
                                  TZrExecIrBlockId a, TZrExecIrBlockId b) {
    while (a != b) {
        while (rpo[a - 1u] < rpo[b - 1u]) a = f->blocks[a - 1u].immediateDominator;
        while (rpo[b - 1u] < rpo[a - 1u]) b = f->blocks[b - 1u].immediateDominator;
    }
    return a;
}

TZrBool ZrParser_ExecIr_ComputeDominators(SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 n, *rpo, *order, *stack, sp = 0u, orderCount = 0u, i, changed;
    unsigned char *visited;
    if (d != ZR_NULL) memset(d, 0, sizeof(*d));
    if (f == ZR_NULL || f->blockCount == 0u || f->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID || f->entryBlockId > f->blockCount) return ZR_FALSE;
    n = f->blockCount;
    rpo = (TZrUInt32 *)malloc((size_t)n * sizeof(*rpo)); order = (TZrUInt32 *)malloc((size_t)n * sizeof(*order)); stack = (TZrUInt32 *)malloc((size_t)n * sizeof(*stack)); visited = (unsigned char *)calloc(n, 1u);
    if (rpo == ZR_NULL || order == ZR_NULL || stack == ZR_NULL || visited == ZR_NULL) { free(rpo); free(order); free(stack); free(visited); return ZR_FALSE; }
    for (i = 0u; i < n; ++i) rpo[i] = UINT32_MAX;
    stack[sp++] = f->entryBlockId;
    while (sp != 0u) {
        TZrExecIrBlockId b = stack[--sp];
        if (b == 0u || b > n || visited[b - 1u] != 0u) continue;
        visited[b - 1u] = 1u; order[orderCount++] = b;
        for (i = f->blocks[b - 1u].successorRange.count; i != 0u; --i) {
            TZrExecIrBlockId s = f->successors[f->blocks[b - 1u].successorRange.start + i - 1u];
            if (s != 0u && s <= n && rpo[s - 1u] == UINT32_MAX) stack[sp++] = s;
        }
    }
    for (i = 0u; i < n; ++i) if (visited[i] == 0u) rpo[i] = UINT32_MAX;
    for (i = 0u; i < orderCount; ++i) rpo[order[i] - 1u] = i + 1u;
    for (i = 0u; i < n; ++i) f->blocks[i].immediateDominator = ZR_EXEC_IR_BLOCK_ID_INVALID;
    f->blocks[f->entryBlockId - 1u].immediateDominator = f->entryBlockId;
    do {
        changed = 0u;
        for (i = 0u; i < orderCount; ++i) {
            TZrExecIrBlockId b = order[i], newIdom = ZR_EXEC_IR_BLOCK_ID_INVALID; TZrUInt32 j;
            if (b == 0u || b == f->entryBlockId) continue;
            for (j = 0u; j < f->blocks[b - 1u].predecessorRange.count; ++j) {
                TZrExecIrBlockId p = f->predecessors[f->blocks[b - 1u].predecessorRange.start + j];
                if (p == 0u || p > n || f->blocks[p - 1u].immediateDominator == ZR_EXEC_IR_BLOCK_ID_INVALID) continue;
                newIdom = newIdom == ZR_EXEC_IR_BLOCK_ID_INVALID ? p : intersect(f, rpo, newIdom, p);
            }
            if (newIdom != f->blocks[b - 1u].immediateDominator) { f->blocks[b - 1u].immediateDominator = newIdom; changed = 1u; }
        }
    } while (changed != 0u);
    free(rpo); free(order); free(stack); free(visited); return ZR_TRUE;
}
