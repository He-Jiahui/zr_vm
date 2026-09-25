#include "exec_ir_projection_phi.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrPendingPhiMove {
    TZrUInt32 source;
    TZrUInt32 destination;
    TZrUInt32 readers;
    TZrBool active;
} SZrPendingPhiMove;

static int zr_phi_slot_compare(const void *left, const void *right) {
    TZrUInt32 a = *(const TZrUInt32 *)left;
    TZrUInt32 b = *(const TZrUInt32 *)right;
    return (a > b) - (a < b);
}

static TZrBool zr_phi_schedule_error(SZrExecIrDiagnostic *diagnostic,
                                     const SZrExecIrFunction *function,
                                     EZrExecutionDiagnosticCode code,
                                     TZrExecIrBlockId edge) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = function->functionToken;
        diagnostic->blockId = edge;
    }
    return ZR_FALSE;
}

static void zr_phi_emit(SZrExecBcProjection *projection, TZrExecIrBlockId edge,
                        TZrUInt32 source, TZrUInt32 destination) {
    SZrExecBcPhiMove *move = &projection->phiMoves[projection->phiMoveCount++];
    move->edge = edge;
    move->sourceSlot = source;
    move->destinationSlot = destination;
}

static TZrBool zr_phi_bytes(TZrUInt32 count, size_t elementSize, size_t *bytes) {
    if ((size_t)count > SIZE_MAX / elementSize) return ZR_FALSE;
    *bytes = (size_t)count * elementSize;
    return ZR_TRUE;
}

TZrBool zr_projection_schedule_phi_copies(SZrExecBcProjection *projection,
                                           const SZrExecIrFunction *function,
                                           SZrExecIrDiagnostic *diagnostic) {
    SZrPendingPhiMove *pending;
    TZrUInt32 *sortedSlots = ZR_NULL;
    TZrUInt32 first, maximum = 0u;
    size_t pendingBytes, moveBytes, slotBytes;
    if (projection->phiCopyCount == 0u) return ZR_TRUE;
    if (projection->phiCopyCount > UINT32_MAX / 2u ||
        !zr_phi_bytes(projection->phiCopyCount, sizeof(*pending), &pendingBytes) ||
        !zr_phi_bytes(projection->phiCopyCount * 2u,
                      sizeof(*projection->phiMoves), &moveBytes)) {
        return zr_phi_schedule_error(diagnostic, function,
                                     ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    }
    for (TZrUInt32 i = 0u; i < projection->valueSlotCount; ++i) {
        if (projection->valueSlots[i] > maximum) maximum = projection->valueSlots[i];
    }
    if (function->frameLayout != ZR_NULL) {
        if (!zr_phi_bytes(projection->valueSlotCount, sizeof(*sortedSlots), &slotBytes))
            return zr_phi_schedule_error(diagnostic, function,
                                         ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
        sortedSlots = (TZrUInt32 *)malloc(slotBytes);
        if (sortedSlots == ZR_NULL)
            return zr_phi_schedule_error(diagnostic, function,
                                         ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
        memcpy(sortedSlots, projection->valueSlots, slotBytes);
        qsort(sortedSlots, projection->valueSlotCount, sizeof(*sortedSlots),
              zr_phi_slot_compare);
        for (TZrUInt32 i = 1u; i < projection->valueSlotCount; ++i) {
            if (sortedSlots[i] == sortedSlots[i - 1u]) {
                free(sortedSlots);
                return zr_phi_schedule_error(diagnostic, function,
                                             ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, 0u);
            }
        }
        free(sortedSlots);
    }
    if (function->frameLayout != ZR_NULL &&
        function->frameLayout->storageSlotCount != 0u &&
        function->frameLayout->storageSlotCount - 1u > maximum) {
        maximum = function->frameLayout->storageSlotCount - 1u;
    }
    pending = (SZrPendingPhiMove *)malloc(pendingBytes);
    projection->phiMoves = (SZrExecBcPhiMove *)malloc(moveBytes);
    if (pending == ZR_NULL || projection->phiMoves == ZR_NULL) {
        free(pending);
        return zr_phi_schedule_error(diagnostic, function,
                                     ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
    }
    for (first = 0u; first < projection->phiCopyCount;) {
        TZrExecIrBlockId edge = projection->phiCopyEdges[first];
        TZrUInt32 end = first, remaining;
        while (end < projection->phiCopyCount && projection->phiCopyEdges[end] == edge) {
            TZrExecIrValueId source = projection->phiCopySources[end];
            TZrExecIrValueId destination = projection->phiCopyDestinations[end];
            pending[end].source = projection->valueSlots[source - 1u];
            pending[end].destination = projection->valueSlots[destination - 1u];
            pending[end].readers = 0u;
            pending[end].active = ZR_TRUE;
            ++end;
        }
        for (TZrUInt32 i = first; i < end; ++i) {
            for (TZrUInt32 j = first; j < end; ++j) {
                if (i != j && pending[i].destination == pending[j].source)
                    ++pending[i].readers;
            }
        }
        remaining = end - first;
        while (remaining != 0u) {
            TZrUInt32 ready = end;
            for (TZrUInt32 i = first; i < end; ++i) {
                if (pending[i].active && pending[i].readers == 0u) {
                    ready = i;
                    break;
                }
            }
            if (ready != end) {
                zr_phi_emit(projection, edge, pending[ready].source,
                            pending[ready].destination);
                pending[ready].active = ZR_FALSE;
                for (TZrUInt32 i = first; i < end; ++i) {
                    if (pending[i].active &&
                        pending[i].destination == pending[ready].source)
                        --pending[i].readers;
                }
                --remaining;
                continue;
            }
            /* No destination can be overwritten: preserve one cycle input
             * and redirect every remaining reader to the temporary slot. */
            if (maximum >= UINT32_MAX - 1u) {
                free(pending);
                return zr_phi_schedule_error(diagnostic, function,
                                             ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, edge);
            }
            for (ready = first; !pending[ready].active; ++ready) { }
            projection->temporarySlotCount = 1u;
            projection->phiTemporarySlot = maximum + 1u;
            zr_phi_emit(projection, edge, pending[ready].destination,
                        projection->phiTemporarySlot);
            for (TZrUInt32 i = first; i < end; ++i) {
                if (pending[i].active && pending[i].source == pending[ready].destination) {
                    pending[i].source = projection->phiTemporarySlot;
                    --pending[ready].readers;
                }
            }
        }
        first = end;
    }
    free(pending);
    return ZR_TRUE;
}
