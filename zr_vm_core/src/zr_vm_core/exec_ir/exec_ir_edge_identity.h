#ifndef ZR_VM_CORE_EXEC_IR_EDGE_IDENTITY_H
#define ZR_VM_CORE_EXEC_IR_EDGE_IDENTITY_H

#include "zr_vm_core/exec_ir.h"

/* Ranges and block IDs must be validated before matching an edge occurrence. */
static TZrBool zr_exec_ir_edge_occurrence_matches(
        const TZrExecIrBlockId *edges, SZrExecIrRange range,
        TZrUInt32 edgeIndex, const TZrExecIrBlockId *reverseEdges,
        SZrExecIrRange reverseRange, TZrExecIrBlockId reverseBlockId) {
    TZrExecIrBlockId adjacent = edges[edgeIndex];
    TZrUInt32 occurrence = 0u;
    TZrUInt32 index;

    for (index = range.start; index <= edgeIndex; ++index) {
        if (edges[index] == adjacent) {
            ++occurrence;
        }
    }
    for (index = reverseRange.start;
         index < reverseRange.start + reverseRange.count; ++index) {
        if (reverseEdges[index] == reverseBlockId && --occurrence == 0u) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

#endif
