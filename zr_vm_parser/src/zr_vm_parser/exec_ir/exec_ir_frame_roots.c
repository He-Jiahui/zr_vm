#include "exec_ir/exec_ir_frame_roots.h"

#include <stddef.h>
#include <stdint.h>
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

static TZrBool slot_class_valid(EZrExecIrPackedSlotClass slotClass) {
    switch (slotClass) {
        case ZR_EXEC_IR_PACKED_SLOT_BOXED:
        case ZR_EXEC_IR_PACKED_SLOT_SCALAR:
        case ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN:
        case ZR_EXEC_IR_PACKED_SLOT_REF:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool derived_pointer(TZrPtr base, TZrInt64 offset, TZrPtr *out) {
    uintptr_t bits;
    uintptr_t adjusted;
    if (out == ZR_NULL) return ZR_FALSE;
    if (base == ZR_NULL) {
        *out = ZR_NULL;
        return ZR_TRUE;
    }
    bits = (uintptr_t)base;
    if (offset >= 0) {
        if ((uintmax_t)offset >
            (uintmax_t)UINTPTR_MAX - (uintmax_t)bits) {
            return ZR_FALSE;
        }
        adjusted = bits + (uintptr_t)offset;
    } else {
        uintmax_t magnitude = (uintmax_t)(-(offset + 1)) + 1u;
        if (magnitude > (uintmax_t)bits) return ZR_FALSE;
        adjusted = bits - (uintptr_t)magnitude;
    }
    *out = (TZrPtr)adjusted;
    return ZR_TRUE;
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
    /* One logical inline value may contribute several distinct field roots. */
    if (specCount != 0u) {
        candidate.roots = (SZrExecIrFrameRoot *)calloc(specCount, sizeof(*candidate.roots));
        if (candidate.roots == ZR_NULL) { root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u); return ZR_FALSE; }
    }
    for (i = 0u; i < specCount; ++i) {
        TZrUInt32 physical, basePhysical = UINT32_MAX, prior;
        EZrExecIrPackedSlotClass slotClass, baseSlotClass;
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
             (!find_physical(layout, s->baseValueId, &basePhysical,
                             &baseSlotClass) ||
              (baseSlotClass != ZR_EXEC_IR_PACKED_SLOT_REF &&
               baseSlotClass != ZR_EXEC_IR_PACKED_SLOT_BOXED))) ||
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
            ((s->kind == ZR_EXEC_IR_FRAME_ROOT_MANAGED ||
              s->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) &&
             layout->frame.slots[physical].byteSize < sizeof(TZrPtr)) ||
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
                              layout->frame.slots[basePhysical].byteOffset ||
              layout->frame.slots[basePhysical].byteSize < sizeof(TZrPtr)))) {
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
    TZrUInt32 i, pass;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (map == ZR_NULL || map->rootCount > map->rootCapacity ||
        (map->rootCount != 0u && map->roots == ZR_NULL) ||
        frameBase == ZR_NULL || visitor == ZR_NULL) {
        root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); return ZR_FALSE;
    }
    for (i = 0u; i < map->rootCount; ++i) {
        const SZrExecIrFrameRoot *root = &map->roots[i];
        if (!root_kind_valid(root->kind) ||
            (root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED &&
             root->basePhysicalSlot == UINT32_MAX)) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                      root->valueId);
            return ZR_FALSE;
        }
        if (root->kind == ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD &&
            root->fieldByteOffset > UINT32_MAX - root->frameByteOffset) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                      root->valueId);
            return ZR_FALSE;
        }
    }
    for (pass = 0u; pass < 2u; ++pass) {
        for (i = 0u; i < map->rootCount; ++i) {
            SZrExecIrFrameRoot *root = &map->roots[i];
            TZrUInt32 addressOffset = root->frameByteOffset;
            TZrPtr address;
            TZrPtr base = ZR_NULL;
            if ((pass == 0u && root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) ||
                (pass == 1u && root->kind != ZR_EXEC_IR_FRAME_ROOT_DERIVED) ||
                !root->initialized) {
                continue;
            }
            if (root->kind == ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD) {
                addressOffset += root->fieldByteOffset;
            }
            address = (TZrPtr)(frameBase + addressOffset);
            if (root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) {
                base = (TZrPtr)(frameBase + root->baseFrameByteOffset);
            }
            if (!visitor(root, address, base, userData)) {
                root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                          root->valueId);
                return ZR_FALSE;
            }
            if (root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) {
                TZrPtr baseValue;
                TZrPtr derived;
                memcpy(&baseValue, base, sizeof(baseValue));
                if (!derived_pointer(baseValue, root->derivedOffset, &derived)) {
                    root_diag(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              root->valueId);
                    return ZR_FALSE;
                }
                memcpy(address, &derived, sizeof(derived));
            }
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
        if (!slot_class_valid(layout->slotClasses[logical])) {
            root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                      logical + 1u);
            return ZR_FALSE;
        }
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
    /* No active-occupant selector is available. Conflicting writes to reused
     * storage must fail before frame, writeback or invalidation outputs change. */
    if (observation->scalarValues != ZR_NULL) {
        for (TZrUInt32 logical = 0u;
             logical < layout->frame.logicalSlotCount &&
             logical < observation->scalarValueCount; ++logical) {
            TZrUInt32 physical = layout->logicalToPhysical[logical];
            TZrUInt32 byteSize;
            if (layout->slotClasses[logical] != ZR_EXEC_IR_PACKED_SLOT_SCALAR) {
                continue;
            }
            byteSize = layout->frame.slots[physical].byteSize < sizeof(TZrUInt64)
                           ? layout->frame.slots[physical].byteSize : sizeof(TZrUInt64);
            for (TZrUInt32 prior = 0u; prior < logical; ++prior) {
                if (layout->logicalToPhysical[prior] == physical &&
                    layout->slotClasses[prior] == ZR_EXEC_IR_PACKED_SLOT_SCALAR &&
                    memcmp(&observation->scalarValues[prior],
                           &observation->scalarValues[logical], byteSize) != 0) {
                    root_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                              logical + 1u);
                    return ZR_FALSE;
                }
            }
        }
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

/* Borrowed storage is live and typed by caller contract. Spans check arithmetic
 * and aliasing; they do not establish allocation ownership or readability. */
typedef struct SActiveFrameSpan {
    uintptr_t begin;
    uintptr_t end;
} SActiveFrameSpan;

enum {
    ACTIVE_READ_REQUEST, ACTIVE_READ_LAYOUT, ACTIVE_READ_IDS,
    ACTIVE_READ_MAPPING, ACTIVE_READ_CLASSES, ACTIVE_READ_SLOTS,
    ACTIVE_READ_SELECTION, ACTIVE_READ_SCALARS, ACTIVE_READ_COUNT
};
enum {
    ACTIVE_WRITE_FRAME, ACTIVE_WRITE_VALUES, ACTIVE_WRITE_INVALIDATIONS,
    ACTIVE_WRITE_RESULT, ACTIVE_WRITE_COUNT
};

static TZrBool active_frame_fail(SZrExecIrDiagnostic *diagnostic,
                                 EZrExecutionDiagnosticCode code,
                                 TZrUInt32 valueId) {
    SZrExecIrDiagnostic candidate;
    root_diag(&candidate, code, valueId);
    if (diagnostic != ZR_NULL) memcpy(diagnostic, &candidate, sizeof(candidate));
    return ZR_FALSE;
}

static TZrBool active_frame_span(const void *base, uintmax_t count,
                                 size_t elementSize, SActiveFrameSpan *span,
                                 SZrExecIrDiagnostic *diagnostic) {
    size_t bytes;
    uintptr_t begin;
    if (elementSize == 0u || count > (uintmax_t)(SIZE_MAX / elementSize)) {
        return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    }
    bytes = (size_t)count * elementSize;
    if (bytes != 0u && base == ZR_NULL) {
        return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    }
    begin = (uintptr_t)base;
    if ((uintmax_t)bytes > (uintmax_t)(UINTPTR_MAX - begin)) {
        return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    }
    span->begin = begin;
    span->end = begin + (uintptr_t)bytes;
    return ZR_TRUE;
}

static TZrBool active_frame_overlap(const SActiveFrameSpan *a,
                                    const SActiveFrameSpan *b) {
    return (TZrBool)(a->begin != a->end && b->begin != b->end &&
                     a->begin < b->end && b->begin < a->end);
}

static TZrUInt32 active_frame_logical(const SZrExecIrPackedFrameLayout *layout,
                                     TZrExecIrValueId valueId) {
    TZrUInt32 logical;
    for (logical = 0u; logical < layout->frame.logicalSlotCount; ++logical) {
        if (layout->logicalValueIds[logical] == valueId) return logical;
    }
    return layout->frame.logicalSlotCount;
}

static TZrExecIrValueId active_frame_slot_id(const SZrExecIrPackedFrameLayout *layout,
                                            TZrUInt32 physical) {
    TZrUInt32 logical;
    for (logical = 0u; logical < layout->frame.logicalSlotCount; ++logical) {
        if (layout->logicalToPhysical[logical] == physical) return layout->logicalValueIds[logical];
    }
    return ZR_EXEC_IR_VALUE_ID_INVALID;
}

static TZrBool active_frame_layout(const SZrExecIrPackedFrameLayout *layout,
                                   SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFrameLayout *frame = &layout->frame;
    TZrUInt32 logical, physical;
    if (frame->slotCount > frame->slotCapacity ||
        frame->storageSlotCount != frame->slotCount ||
        frame->parameterPrefixCount > frame->logicalSlotCount ||
        frame->parameterCount != frame->parameterPrefixCount ||
        frame->localCount != frame->logicalSlotCount - frame->parameterPrefixCount ||
        frame->frameByteAlign == 0u ||
        (frame->frameByteAlign & (frame->frameByteAlign - 1u)) != 0u ||
        frame->frameByteSize % frame->frameByteAlign != 0u ||
        frame->returnBufferOffset > frame->frameByteSize) {
        return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    }
    for (logical = 0u; logical < frame->logicalSlotCount; ++logical) {
        TZrExecIrValueId id = layout->logicalValueIds[logical];
        EZrExecIrPackedSlotClass slotClass = layout->slotClasses[logical];
        TZrUInt32 prior;
        physical = layout->logicalToPhysical[logical];
        if (id == ZR_EXEC_IR_VALUE_ID_INVALID || !slot_class_valid(slotClass) ||
            physical >= frame->storageSlotCount) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, id);
        }
        for (prior = 0u; prior < logical; ++prior) {
            if (layout->logicalValueIds[prior] == id) {
                return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION, id);
            }
        }
        if (frame->slots[physical].kind != (slotClass == ZR_EXEC_IR_PACKED_SLOT_REF
                ? ZR_EXEC_IR_FRAME_SLOT_REFERENCE : ZR_EXEC_IR_FRAME_SLOT_VALUE)) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, id);
        }
    }
    for (physical = 0u; physical < frame->slotCount; ++physical) {
        const SZrExecIrFrameSlot *slot = &frame->slots[physical];
        TZrExecIrValueId id = active_frame_slot_id(layout, physical);
        TZrUInt32 prior;
        if (id == ZR_EXEC_IR_VALUE_ID_INVALID) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        }
        if (slot->byteSize == 0u || slot->byteAlign == 0u ||
            (slot->byteAlign & (slot->byteAlign - 1u)) != 0u ||
            slot->byteAlign > frame->frameByteAlign ||
            slot->byteOffset % slot->byteAlign != 0u ||
            slot->byteOffset > frame->frameByteSize ||
            slot->byteSize > frame->frameByteSize - slot->byteOffset ||
            slot->byteOffset > frame->returnBufferOffset ||
            slot->byteSize > frame->returnBufferOffset - slot->byteOffset) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, id);
        }
        /* Subtractive bounds above make these UInt32 additions representable. */
        for (prior = 0u; prior < physical; ++prior) {
            const SZrExecIrFrameSlot *other = &frame->slots[prior];
            if (slot->byteOffset < other->byteOffset + other->byteSize &&
                other->byteOffset < slot->byteOffset + slot->byteSize) {
                return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, id);
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ObserveActiveFrame(
        const SZrExecIrActiveFrameObservation *request,
        SZrExecIrActiveFrameObservationResult *result,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrActiveFrameObservation input;
    SZrExecIrPackedFrameLayout layout;
    SActiveFrameSpan readonly[ACTIVE_READ_COUNT], writable[ACTIVE_WRITE_COUNT];
    TZrUInt32 selected;
    size_t i, prior;
    if (request == ZR_NULL || result == ZR_NULL || request->layout == ZR_NULL) {
        return active_frame_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    }
    memcpy(&input, request, sizeof(input));
    memcpy(&layout, input.layout, sizeof(layout));

    /* Use only initialized slotCount rows. slotCapacity is owner bookkeeping. */
    if (!active_frame_span(request, 1u, sizeof(*request), &readonly[ACTIVE_READ_REQUEST], diagnostic) ||
        !active_frame_span(input.layout, 1u, sizeof(*input.layout), &readonly[ACTIVE_READ_LAYOUT], diagnostic) ||
        !active_frame_span(layout.logicalValueIds, layout.frame.logicalSlotCount,
                           sizeof(*layout.logicalValueIds), &readonly[ACTIVE_READ_IDS], diagnostic) ||
        !active_frame_span(layout.logicalToPhysical, layout.frame.logicalSlotCount,
                           sizeof(*layout.logicalToPhysical), &readonly[ACTIVE_READ_MAPPING], diagnostic) ||
        !active_frame_span(layout.slotClasses, layout.frame.logicalSlotCount,
                           sizeof(*layout.slotClasses), &readonly[ACTIVE_READ_CLASSES], diagnostic) ||
        !active_frame_span(layout.frame.slots, layout.frame.slotCount,
                           sizeof(*layout.frame.slots), &readonly[ACTIVE_READ_SLOTS], diagnostic) ||
        !active_frame_span(input.activeValueIds, input.activeValueCount,
                           sizeof(*input.activeValueIds), &readonly[ACTIVE_READ_SELECTION], diagnostic) ||
        !active_frame_span(input.scalarValues, input.scalarValueCount,
                           sizeof(*input.scalarValues), &readonly[ACTIVE_READ_SCALARS], diagnostic) ||
        !active_frame_span(input.frameBase, input.frameCapacityBytes, sizeof(*input.frameBase),
                           &writable[ACTIVE_WRITE_FRAME], diagnostic) ||
        !active_frame_span(input.writebackValues, input.writebackCapacity,
                           sizeof(*input.writebackValues), &writable[ACTIVE_WRITE_VALUES], diagnostic) ||
        !active_frame_span(input.invalidatedPhysicalSlots, input.invalidatedCapacity,
                           sizeof(*input.invalidatedPhysicalSlots), &writable[ACTIVE_WRITE_INVALIDATIONS], diagnostic) ||
        !active_frame_span(result, 1u, sizeof(*result), &writable[ACTIVE_WRITE_RESULT], diagnostic)) {
        return ZR_FALSE;
    }
    if (!active_frame_layout(&layout, diagnostic)) return ZR_FALSE;
    if ((input.scalarValues == ZR_NULL && input.scalarValueCount != 0u) ||
        (input.scalarValues != ZR_NULL && input.scalarValueCount < layout.frame.logicalSlotCount) ||
        input.frameCapacityBytes < layout.frame.frameByteSize ||
        (layout.frame.frameByteSize != 0u && input.frameBase == ZR_NULL) ||
        input.invalidatedCapacity < input.activeValueCount ||
        (input.activeValueCount != 0u && (input.writebackValues == ZR_NULL ||
                                        input.invalidatedPhysicalSlots == ZR_NULL))) {
        return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    }
    for (selected = 0u; selected < input.activeValueCount; ++selected) {
        TZrExecIrValueId id = input.activeValueIds[selected];
        TZrUInt32 logical = active_frame_logical(&layout, id);
        TZrUInt32 physical, previous;
        const SZrExecIrFrameSlot *slot;
        if (id == ZR_EXEC_IR_VALUE_ID_INVALID || logical == layout.frame.logicalSlotCount) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, id);
        }
        physical = layout.logicalToPhysical[logical];
        slot = &layout.frame.slots[physical];
        for (previous = 0u; previous < selected; ++previous) {
            TZrExecIrValueId oldId = input.activeValueIds[previous];
            TZrUInt32 oldLogical;
            if (oldId == id) {
                return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION, id);
            }
            oldLogical = active_frame_logical(&layout, oldId);
            if (layout.logicalToPhysical[oldLogical] == physical) {
                return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, id);
            }
        }
        if (layout.slotClasses[logical] != ZR_EXEC_IR_PACKED_SLOT_SCALAR ||
            slot->kind != ZR_EXEC_IR_FRAME_SLOT_VALUE || slot->byteSize > sizeof(TZrUInt64)) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, id);
        }
        if (logical >= input.writebackCapacity) {
            return active_frame_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, id);
        }
    }
    /* Full declared writable extents include inactive cells and unused tails. */
    for (i = 0u; i < ACTIVE_WRITE_COUNT; ++i) {
        for (prior = 0u; prior < i; ++prior) {
            if (active_frame_overlap(&writable[i], &writable[prior])) {
                return active_frame_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
            }
        }
        for (prior = 0u; prior < ACTIVE_READ_COUNT; ++prior) {
            if (active_frame_overlap(&writable[i], &readonly[prior])) {
                return active_frame_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
            }
        }
    }

    /* All possible refusals are above. Live serialized inputs remain unchanged,
     * so re-resolution needs no allocation or saved per-row state. The physical
     * width belongs to the reused slot, not a reconstructed logical type. */
    for (selected = 0u; selected < input.activeValueCount; ++selected) {
        TZrUInt32 logical = active_frame_logical(&layout, input.activeValueIds[selected]);
        TZrUInt32 physical = layout.logicalToPhysical[logical];
        const SZrExecIrFrameSlot *slot = &layout.frame.slots[physical];
        if (input.scalarValues != ZR_NULL) {
            memcpy(input.frameBase + slot->byteOffset, &input.scalarValues[logical], slot->byteSize);
        }
        input.writebackValues[logical] = 0u;
        memcpy(&input.writebackValues[logical], input.frameBase + slot->byteOffset, slot->byteSize);
        input.invalidatedPhysicalSlots[selected] = physical;
    }
    result->observedValueCount = input.activeValueCount;
    result->invalidatedCount = input.activeValueCount;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    return ZR_TRUE;
}
