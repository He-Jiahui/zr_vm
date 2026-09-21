#include "zr_vm_parser/exec_ir_frame_roots.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void root_diag(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
                      TZrUInt32 valueId) {
    if (d != ZR_NULL) { memset(d, 0, sizeof(*d)); d->code = code; d->actualVersion = valueId; }
}

static TZrBool root_kind_valid(EZrExecIrFrameRootKind kind) {
    switch (kind) {
        case ZR_EXEC_IR_FRAME_ROOT_MANAGED:
        case ZR_EXEC_IR_FRAME_ROOT_DERIVED:
        case ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

void ZrParser_ExecIr_FrameRootMapInit(SZrExecIrFrameRootMap *map) {
    if (map != ZR_NULL) memset(map, 0, sizeof(*map));
}

void ZrParser_ExecIr_FrameRootMapFree(SZrExecIrFrameRootMap *map) {
    if (map == ZR_NULL) return;
    free(map->roots); memset(map, 0, sizeof(*map));
}

static TZrBool find_physical(const SZrExecIrPackedFrameLayout *layout,
                             TZrExecIrValueId valueId, TZrUInt32 *out,
                             EZrExecIrPackedSlotClass *outClass) {
    TZrUInt32 i;
    if (layout == ZR_NULL || out == ZR_NULL || valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        layout->logicalValueIds == ZR_NULL || layout->logicalToPhysical == ZR_NULL) {
        return ZR_FALSE;
    }
    for (i = 0u; i < layout->frame.logicalSlotCount; ++i) {
        if (layout->logicalValueIds[i] == valueId &&
            layout->logicalToPhysical[i] < layout->frame.storageSlotCount) {
            *out = layout->logicalToPhysical[i];
            if (outClass != ZR_NULL) *outClass = layout->slotClasses[i];
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_BuildFrameRootMap(const SZrExecIrPackedFrameLayout *layout,
                                          const SZrExecIrFrameRootSpec *specs,
                                          TZrUInt32 specCount,
                                          SZrExecIrFrameRootMap *map,
                                          SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFrameRootMap candidate;
    TZrUInt32 i;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (layout == ZR_NULL || map == ZR_NULL || (specCount != 0u && specs == ZR_NULL) ||
        specCount > layout->frame.logicalSlotCount ||
        layout->frame.slotCount > layout->frame.slotCapacity ||
        layout->frame.storageSlotCount > layout->frame.slotCount ||
        (layout->frame.slotCount != 0u && layout->frame.slots == ZR_NULL) ||
        (layout->frame.logicalSlotCount != 0u &&
         (layout->logicalValueIds == ZR_NULL ||
          layout->logicalToPhysical == ZR_NULL ||
          layout->slotClasses == ZR_NULL))) {
        root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); return ZR_FALSE;
    }
    ZrParser_ExecIr_FrameRootMapInit(&candidate);
    if (specCount != 0u) {
        candidate.roots = (SZrExecIrFrameRoot *)calloc(specCount, sizeof(*candidate.roots));
        if (candidate.roots == ZR_NULL) { root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u); return ZR_FALSE; }
    }
    for (i = 0u; i < specCount; ++i) {
        TZrUInt32 physical, basePhysical = UINT32_MAX, prior;
        EZrExecIrPackedSlotClass slotClass;
        const SZrExecIrFrameRootSpec *s = &specs[i];
        if (!find_physical(layout, s->valueId, &physical, &slotClass) ||
            !root_kind_valid(s->kind) ||
            ((s->kind == ZR_EXEC_IR_FRAME_ROOT_MANAGED ||
              s->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) &&
             slotClass != ZR_EXEC_IR_PACKED_SLOT_REF &&
             slotClass != ZR_EXEC_IR_PACKED_SLOT_BOXED) ||
            (s->kind == ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD &&
             slotClass != ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN &&
             slotClass != ZR_EXEC_IR_PACKED_SLOT_BOXED) ||
            (s->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED &&
             !find_physical(layout, s->baseValueId, &basePhysical, ZR_NULL)) ||
            (s->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED &&
             s->baseValueId == s->valueId)) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, s->valueId);
            ZrParser_ExecIr_FrameRootMapFree(&candidate); return ZR_FALSE;
        }
        if (physical >= layout->frame.slotCount ||
            layout->frame.slots[physical].byteOffset > layout->frame.frameByteSize ||
            layout->frame.slots[physical].byteSize >
                    layout->frame.frameByteSize -
                            layout->frame.slots[physical].byteOffset ||
            (s->kind == ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD &&
             (s->fieldByteOffset > layout->frame.slots[physical].byteSize ||
              sizeof(TZrPtr) >
                      layout->frame.slots[physical].byteSize -
                              s->fieldByteOffset)) ||
            (s->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED &&
             (basePhysical >= layout->frame.slotCount ||
              layout->frame.slots[basePhysical].byteOffset >
                      layout->frame.frameByteSize ||
              layout->frame.slots[basePhysical].byteSize >
                      layout->frame.frameByteSize -
                              layout->frame.slots[basePhysical].byteOffset))) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                      s->valueId);
            ZrParser_ExecIr_FrameRootMapFree(&candidate); return ZR_FALSE;
        }
        for (prior = 0u; prior < i; ++prior) {
            if (candidate.roots[prior].valueId == s->valueId &&
                candidate.roots[prior].kind == s->kind &&
                candidate.roots[prior].fieldByteOffset ==
                        s->fieldByteOffset) {
                root_diag(diagnostic,
                          ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                          s->valueId);
                ZrParser_ExecIr_FrameRootMapFree(&candidate);
                return ZR_FALSE;
            }
        }
        candidate.roots[i].valueId = s->valueId;
        candidate.roots[i].physicalSlot = physical;
        candidate.roots[i].frameByteOffset = layout->frame.slots[physical].byteOffset;
        candidate.roots[i].kind = s->kind;
        candidate.roots[i].fieldByteOffset = s->fieldByteOffset;
        candidate.roots[i].basePhysicalSlot = basePhysical;
        candidate.roots[i].baseFrameByteOffset = basePhysical < UINT32_MAX
                                                     ? layout->frame.slots[basePhysical].byteOffset
                                                     : 0u;
        candidate.roots[i].derivedOffset = s->derivedOffset;
        candidate.roots[i].initialized = s->initialized;
    }
    candidate.rootCount = specCount; candidate.rootCapacity = specCount;
    ZrParser_ExecIr_FrameRootMapFree(map); *map = candidate; return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_VisitFrameRoots(const SZrExecIrFrameRootMap *map,
                                        TZrByte *frameBase,
                                        FZrExecIrFrameRootVisitor visitor,
                                        TZrPtr userData,
                                        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (map == ZR_NULL || map->rootCount > map->rootCapacity ||
        (map->rootCount != 0u && map->roots == ZR_NULL) ||
        frameBase == ZR_NULL || visitor == ZR_NULL) {
        root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); return ZR_FALSE;
    }
    for (i = 0u; i < map->rootCount; ++i) {
        SZrExecIrFrameRoot *root = &map->roots[i];
        TZrUInt32 addressOffset = root->frameByteOffset;
        TZrPtr address;
        if (!root_kind_valid(root->kind)) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                      root->valueId);
            return ZR_FALSE;
        }
        if (!root->initialized) continue;
        if (root->kind == ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD) {
            if (root->fieldByteOffset > UINT32_MAX - addressOffset) {
                root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, root->valueId); return ZR_FALSE;
            }
            addressOffset += root->fieldByteOffset;
        }
        address = root->initialized ? (TZrPtr)(frameBase + addressOffset) : ZR_NULL;
        TZrPtr base = ZR_NULL;
        if (root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) {
            if (root->basePhysicalSlot == UINT32_MAX) { root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, root->valueId); return ZR_FALSE; }
            base = (TZrPtr)(frameBase + root->baseFrameByteOffset);
        }
        if (!visitor(root, address, base, userData)) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, root->valueId); return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ObserveFrame(SZrExecIrFrameObservation *observation,
                                     SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrPackedFrameLayout *layout;
    TZrUInt32 uniquePhysicalCount = 0u;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (observation == ZR_NULL || (layout = observation->layout) == ZR_NULL ||
        layout->logicalToPhysical == ZR_NULL || layout->slotClasses == ZR_NULL ||
        layout->frame.slotCount > layout->frame.slotCapacity ||
        layout->frame.storageSlotCount > layout->frame.slotCount ||
        observation->frameBase == ZR_NULL ||
        (layout->frame.slotCount != 0u && layout->frame.slots == ZR_NULL) ||
        observation->writebackValues == ZR_NULL ||
        observation->writebackCapacity < layout->frame.logicalSlotCount ||
        (observation->invalidatedPhysicalSlots == ZR_NULL &&
         observation->invalidatedCapacity != 0u)) {
        root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); return ZR_FALSE;
    }
    for (TZrUInt32 logical = 0u; logical < layout->frame.logicalSlotCount; ++logical) {
        TZrUInt32 physical = layout->logicalToPhysical[logical];
        TZrBool seenPhysical = ZR_FALSE;
        if (physical >= layout->frame.slotCount) { root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, logical + 1u); return ZR_FALSE; }
        if (layout->frame.slots[physical].byteOffset > layout->frame.frameByteSize ||
            layout->frame.slots[physical].byteSize > layout->frame.frameByteSize - layout->frame.slots[physical].byteOffset) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, logical + 1u); return ZR_FALSE;
        }
        for (TZrUInt32 prior = 0u; prior < logical; ++prior) {
            if (layout->logicalToPhysical[prior] == physical) {
                seenPhysical = ZR_TRUE;
                break;
            }
        }
        if (!seenPhysical) ++uniquePhysicalCount;
    }
    if (observation->invalidatedPhysicalSlots != ZR_NULL &&
        observation->invalidatedCapacity < uniquePhysicalCount) {
        root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                  uniquePhysicalCount);
        return ZR_FALSE;
    }
    observation->invalidatedCount = 0u;
    for (TZrUInt32 logical = 0u; logical < layout->frame.logicalSlotCount; ++logical) {
        TZrUInt32 physical = layout->logicalToPhysical[logical];
        TZrUInt32 byteSize;
        byteSize = layout->frame.slots[physical].byteSize < sizeof(TZrUInt64)
                       ? layout->frame.slots[physical].byteSize : sizeof(TZrUInt64);
        if (observation->scalarValues != ZR_NULL &&
            logical < observation->scalarValueCount &&
            layout->slotClasses[logical] == ZR_EXEC_IR_PACKED_SLOT_SCALAR) {
            memcpy(observation->frameBase + layout->frame.slots[physical].byteOffset,
                   &observation->scalarValues[logical], byteSize);
        }
        observation->writebackValues[logical] = 0u;
        memcpy(&observation->writebackValues[logical],
               observation->frameBase + layout->frame.slots[physical].byteOffset,
               byteSize);
        if (observation->invalidatedPhysicalSlots != ZR_NULL) {
            TZrBool seenPhysical = ZR_FALSE;
            for (TZrUInt32 prior = 0u; prior < logical; ++prior) {
                if (layout->logicalToPhysical[prior] == physical) {
                    seenPhysical = ZR_TRUE;
                    break;
                }
            }
            if (!seenPhysical) {
                observation->invalidatedPhysicalSlots[
                        observation->invalidatedCount++] = physical;
            }
        }
    }
    return ZR_TRUE;
}
