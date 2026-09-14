#include "zr_vm_core/execution_frame_layout.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void execution_frame_diag(SZrExecutionFrameDiagnostic *diagnostic,
                                 EZrExecutionFrameDiagnosticCode code,
                                 TZrUInt32 index,
                                 TZrUInt32 relatedIndex,
                                 TZrUInt64 expected,
                                 TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->index = index;
    diagnostic->relatedIndex = relatedIndex;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrBool execution_frame_is_power_of_two(TZrUInt32 value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool execution_frame_add_u32(TZrUInt32 left,
                                       TZrUInt32 right,
                                       TZrUInt32 *out) {
    if (out == ZR_NULL || right > UINT32_MAX - left) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool execution_frame_lifetimes_overlap(
        const SZrExecutionFrameSlot *left,
        const SZrExecutionFrameSlot *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->liveEnd <= left->liveStart || right->liveEnd <= right->liveStart) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->liveStart < right->liveEnd &&
                     right->liveStart < left->liveEnd);
}

static TZrBool execution_frame_find_slot_index(
        const SZrExecutionFrameLayout *layout,
        TZrUInt32 logicalSlot,
        TZrUInt32 *outIndex) {
    TZrUInt32 index;

    if (layout == ZR_NULL || outIndex == ZR_NULL || layout->slots == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < layout->slotCount; ++index) {
        if (layout->slots[index].logicalSlot == logicalSlot) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt64 execution_frame_hash_mix(TZrUInt64 hash, TZrUInt64 value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
    return hash;
}

static TZrUInt64 execution_frame_hash_slot(const SZrExecutionFrameSlot *slot,
                                           TZrUInt64 hash) {
    hash = execution_frame_hash_mix(hash, slot->logicalSlot);
    hash = execution_frame_hash_mix(hash, slot->physicalSlot);
    hash = execution_frame_hash_mix(hash, slot->byteOffset);
    hash = execution_frame_hash_mix(hash, slot->byteSize);
    hash = execution_frame_hash_mix(hash, slot->byteAlign);
    hash = execution_frame_hash_mix(hash, slot->liveStart);
    hash = execution_frame_hash_mix(hash, slot->liveEnd);
    hash = execution_frame_hash_mix(hash, slot->typeToken);
    hash = execution_frame_hash_mix(hash, slot->slotClass);
    hash = execution_frame_hash_mix(hash, slot->flags);
    return hash;
}

void ZrCore_ExecutionFrameLayout_Init(SZrExecutionFrameLayout *layout) {
    if (layout == ZR_NULL) {
        return;
    }
    memset(layout, 0, sizeof(*layout));
    layout->magic = ZR_EXECUTION_FRAME_LAYOUT_MAGIC;
    layout->schemaVersion = ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION;
}

TZrUInt64 ZrCore_ExecutionFrameLayout_Hash(
        const SZrExecutionFrameLayout *layout) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 logical;

    if (layout == ZR_NULL) {
        return 0u;
    }
    hash = execution_frame_hash_mix(hash, layout->magic);
    hash = execution_frame_hash_mix(hash, layout->schemaVersion);
    hash = execution_frame_hash_mix(hash, layout->logicalSlotCount);
    hash = execution_frame_hash_mix(hash, layout->storageSlotCount);
    hash = execution_frame_hash_mix(hash, layout->parameterPrefixCount);
    hash = execution_frame_hash_mix(hash, layout->returnBufferOffset);
    hash = execution_frame_hash_mix(hash, layout->frameByteSize);
    hash = execution_frame_hash_mix(hash, layout->frameByteAlign);
    hash = execution_frame_hash_mix(hash, layout->slotCount);

    /* Canonicalise by logical slot id.  Producer insertion order must not
     * alter a guard or artifact hash. */
    for (logical = 0u; logical < layout->slotCount; ++logical) {
        TZrUInt32 index;
        TZrUInt32 bestIndex = UINT32_MAX;
        TZrUInt32 bestLogical = UINT32_MAX;
        if (layout->slots == ZR_NULL) {
            return 0u;
        }
        for (index = 0u; index < layout->slotCount; ++index) {
            if (layout->slots[index].logicalSlot >= bestLogical) {
                continue;
            }
            /* Find the logical-th smallest id without allocating a sort
             * buffer.  Duplicate IDs are rejected by Validate. */
            TZrUInt32 lowerCount = 0u;
            TZrUInt32 probe;
            for (probe = 0u; probe < layout->slotCount; ++probe) {
                if (layout->slots[probe].logicalSlot <
                    layout->slots[index].logicalSlot) {
                    ++lowerCount;
                }
            }
            if (lowerCount == logical) {
                bestIndex = index;
                bestLogical = layout->slots[index].logicalSlot;
                break;
            }
        }
        if (bestIndex == UINT32_MAX) {
            return 0u;
        }
        hash = execution_frame_hash_slot(&layout->slots[bestIndex], hash);
    }
    return hash;
}

TZrBool ZrCore_ExecutionFrameLayout_Validate(
        const SZrExecutionFrameLayout *layout,
        SZrExecutionFrameDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (layout == ZR_NULL) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                             0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (layout->magic != ZR_EXECUTION_FRAME_LAYOUT_MAGIC) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_MAGIC,
                             0u, 0u, ZR_EXECUTION_FRAME_LAYOUT_MAGIC,
                             layout->magic);
        return ZR_FALSE;
    }
    if (layout->schemaVersion != ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
                             0u, 0u, ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION,
                             layout->schemaVersion);
        return ZR_FALSE;
    }
    if (layout->parameterPrefixCount > layout->logicalSlotCount ||
        layout->slotCount != layout->logicalSlotCount ||
        (layout->slotCount != 0u && layout->slots == ZR_NULL) ||
        (layout->storageSlotCount == 0u && layout->slotCount != 0u) ||
        !execution_frame_is_power_of_two(layout->frameByteAlign)) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT,
                             0u, 0u, layout->logicalSlotCount,
                             layout->slotCount);
        return ZR_FALSE;
    }
    if (layout->returnBufferOffset > layout->frameByteSize) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                             0u, 0u, layout->frameByteSize,
                             layout->returnBufferOffset);
        return ZR_FALSE;
    }

    for (index = 0u; index < layout->slotCount; ++index) {
        const SZrExecutionFrameSlot *slot = &layout->slots[index];
        TZrUInt32 end;
        TZrUInt32 prior;

        if (slot->slotClass >= ZR_EXECUTION_FRAME_SLOT_CLASS_COUNT ||
            (slot->flags & ~ZR_EXECUTION_FRAME_SLOT_FLAG_KNOWN_MASK) != 0u ||
            slot->byteSize == 0u ||
            !execution_frame_is_power_of_two(slot->byteAlign) ||
            (slot->byteOffset & (slot->byteAlign - 1u)) != 0u ||
            slot->physicalSlot >= layout->storageSlotCount ||
            slot->liveEnd < slot->liveStart ||
            !execution_frame_add_u32(slot->byteOffset, slot->byteSize, &end) ||
            end > layout->frameByteSize) {
            execution_frame_diag(diagnostic,
                                 !execution_frame_is_power_of_two(slot->byteAlign) ||
                                         (slot->byteOffset &
                                          (slot->byteAlign - 1u)) != 0u
                                     ? ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ALIGNMENT
                                     : ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT,
                                 index, 0u, layout->frameByteSize,
                                 slot->byteOffset);
            return ZR_FALSE;
        }
        for (prior = 0u; prior < index; ++prior) {
            const SZrExecutionFrameSlot *other = &layout->slots[prior];
            if (slot->logicalSlot == other->logicalSlot) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_DUPLICATE_SLOT,
                                     index, prior, other->logicalSlot,
                                     slot->logicalSlot);
                return ZR_FALSE;
            }
            if (slot->physicalSlot == other->physicalSlot) {
                if (slot->slotClass != other->slotClass ||
                    slot->byteSize != other->byteSize ||
                    slot->byteAlign != other->byteAlign ||
                    slot->typeToken != other->typeToken ||
                    (slot->flags & ZR_EXECUTION_FRAME_SLOT_FLAG_ADDRESS_ESCAPED) != 0u ||
                    (other->flags & ZR_EXECUTION_FRAME_SLOT_FLAG_ADDRESS_ESCAPED) != 0u ||
                    execution_frame_lifetimes_overlap(slot, other)) {
                    execution_frame_diag(diagnostic,
                                         execution_frame_lifetimes_overlap(slot, other)
                                             ? ZR_EXECUTION_FRAME_DIAGNOSTIC_SLOT_OVERLAP
                                             : ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT,
                                         index, prior, other->physicalSlot,
                                         slot->physicalSlot);
                    return ZR_FALSE;
                }
            }
        }
    }
    if (layout->layoutHash != 0u &&
        layout->layoutHash != ZrCore_ExecutionFrameLayout_Hash(layout)) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_LAYOUT_HASH,
                             0u, 0u, layout->layoutHash,
                             ZrCore_ExecutionFrameLayout_Hash(layout));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecutionFrameLayout_Finalize(
        SZrExecutionFrameLayout *layout,
        SZrExecutionFrameDiagnostic *diagnostic) {
    if (!ZrCore_ExecutionFrameLayout_Validate(layout, diagnostic)) {
        return ZR_FALSE;
    }
    layout->layoutHash = ZrCore_ExecutionFrameLayout_Hash(layout);
    return layout->layoutHash != 0u;
}

const SZrExecutionFrameSlot *ZrCore_ExecutionFrameLayout_FindLogical(
        const SZrExecutionFrameLayout *layout, TZrUInt32 logicalSlot) {
    TZrUInt32 index;
    if (!execution_frame_find_slot_index(layout, logicalSlot, &index)) {
        return ZR_NULL;
    }
    return &layout->slots[index];
}

void ZrCore_ExecutionFrameRootMap_Init(SZrExecutionFrameRootMap *map) {
    if (map == ZR_NULL) {
        return;
    }
    memset(map, 0, sizeof(*map));
    map->magic = ZR_EXECUTION_FRAME_LAYOUT_MAGIC;
    map->schemaVersion = ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION;
}

void ZrCore_ExecutionFrameRootMap_Free(SZrExecutionFrameRootMap *map) {
    if (map == ZR_NULL) {
        return;
    }
    free(map->roots);
    memset(map, 0, sizeof(*map));
}

TZrBool ZrCore_ExecutionFrameRootMap_Build(
        const SZrExecutionFrameLayout *layout,
        const SZrExecutionFrameRootSpec *specs,
        TZrUInt32 specCount,
        SZrExecutionFrameRootMap *map,
        SZrExecutionFrameDiagnostic *diagnostic) {
    SZrExecutionFrameRootMap candidate;
    TZrUInt32 index;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (layout == ZR_NULL || map == ZR_NULL ||
        (specCount != 0u && specs == ZR_NULL) ||
        !ZrCore_ExecutionFrameLayout_Validate(layout, diagnostic)) {
        if (layout == ZR_NULL || map == ZR_NULL ||
            (specCount != 0u && specs == ZR_NULL)) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                                 0u, 0u, 1u, 0u);
        }
        return ZR_FALSE;
    }
    ZrCore_ExecutionFrameRootMap_Init(&candidate);
    if (specCount > 0u) {
#if SIZE_MAX < UINT32_MAX
        if (specCount > (TZrUInt32)(SIZE_MAX / sizeof(*candidate.roots))) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
                                 0u, 0u, SIZE_MAX, specCount);
            return ZR_FALSE;
        }
#endif
        candidate.roots = (SZrExecutionFrameRoot *)calloc(
                (size_t)specCount, sizeof(*candidate.roots));
        if (candidate.roots == ZR_NULL) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
                                 0u, 0u, specCount, 0u);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < specCount; ++index) {
        const SZrExecutionFrameRootSpec *spec = &specs[index];
        TZrUInt32 slotIndex;
        const SZrExecutionFrameSlot *slot;
        TZrUInt32 prior;

        if (spec->kind >= ZR_EXECUTION_FRAME_ROOT_KIND_COUNT ||
            spec->storage >= ZR_EXECUTION_FRAME_ROOT_STORAGE_COUNT ||
            !execution_frame_find_slot_index(layout, spec->logicalSlot,
                                             &slotIndex)) {
            execution_frame_diag(diagnostic,
                                 !execution_frame_find_slot_index(
                                         layout, spec->logicalSlot, &slotIndex)
                                     ? ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_UNMAPPED
                                     : ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                 index, 0u, spec->logicalSlot, 0u);
            ZrCore_ExecutionFrameRootMap_Free(&candidate);
            return ZR_FALSE;
        }
        slot = &layout->slots[slotIndex];
        if ((spec->kind == ZR_EXECUTION_FRAME_ROOT_MANAGED ||
             spec->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) &&
            (spec->storage != ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER ||
             slot->byteSize < (TZrUInt32)sizeof(TZrPtr) ||
             (slot->slotClass != ZR_EXECUTION_FRAME_SLOT_REFERENCE &&
              slot->slotClass != ZR_EXECUTION_FRAME_SLOT_BOXED))) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                 index, 0u, ZR_EXECUTION_FRAME_SLOT_REFERENCE,
                                 slot->slotClass);
            ZrCore_ExecutionFrameRootMap_Free(&candidate);
            return ZR_FALSE;
        }
        if (spec->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
            TZrUInt32 baseIndex;
            if (spec->storage != ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER ||
                slot->byteSize < (TZrUInt32)sizeof(TZrPtr) ||
                spec->baseLogicalSlot == spec->logicalSlot ||
                !execution_frame_find_slot_index(layout, spec->baseLogicalSlot,
                                                 &baseIndex)) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                     index, 0u, spec->baseLogicalSlot, 0u);
                ZrCore_ExecutionFrameRootMap_Free(&candidate);
                return ZR_FALSE;
            }
            (void)baseIndex;
        }
        if (spec->kind == ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD) {
            TZrUInt32 fieldEnd;
            if (spec->storage != ZR_EXECUTION_FRAME_ROOT_STORAGE_VALUE_BYTES ||
                (slot->slotClass != ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN &&
                 slot->slotClass != ZR_EXECUTION_FRAME_SLOT_BOXED)) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                     index, 0u, ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN,
                                     slot->slotClass);
                ZrCore_ExecutionFrameRootMap_Free(&candidate);
                return ZR_FALSE;
            }
            if (!execution_frame_add_u32(spec->fieldByteOffset,
                                         (TZrUInt32)sizeof(TZrPtr), &fieldEnd) ||
                fieldEnd > slot->byteSize) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                                     index, 0u, slot->byteSize, fieldEnd);
                ZrCore_ExecutionFrameRootMap_Free(&candidate);
                return ZR_FALSE;
            }
        }
        for (prior = 0u; prior < index; ++prior) {
            if (candidate.roots[prior].logicalSlot == spec->logicalSlot &&
                candidate.roots[prior].kind == spec->kind &&
                candidate.roots[prior].fieldByteOffset == spec->fieldByteOffset) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_DUPLICATE_SLOT,
                                     index, prior, spec->logicalSlot,
                                     spec->logicalSlot);
                ZrCore_ExecutionFrameRootMap_Free(&candidate);
                return ZR_FALSE;
            }
        }
        candidate.roots[index].logicalSlot = spec->logicalSlot;
        candidate.roots[index].physicalSlot = slot->physicalSlot;
        candidate.roots[index].frameByteOffset = slot->byteOffset;
        candidate.roots[index].byteSize = slot->byteSize;
        candidate.roots[index].kind = spec->kind;
        candidate.roots[index].storage = spec->storage;
        candidate.roots[index].fieldByteOffset = spec->fieldByteOffset;
        candidate.roots[index].baseLogicalSlot = spec->baseLogicalSlot;
        candidate.roots[index].basePhysicalSlot = UINT32_MAX;
        candidate.roots[index].baseFrameByteOffset = 0u;
        candidate.roots[index].derivedOffset = spec->derivedOffset;
        candidate.roots[index].initialized = spec->initialized;
        if (spec->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
            TZrUInt32 baseIndex;
            (void)execution_frame_find_slot_index(layout, spec->baseLogicalSlot,
                                                  &baseIndex);
            candidate.roots[index].basePhysicalSlot =
                    layout->slots[baseIndex].physicalSlot;
            candidate.roots[index].baseFrameByteOffset =
                    layout->slots[baseIndex].byteOffset;
        }
    }
    candidate.layoutHash = layout->layoutHash != 0u
                               ? layout->layoutHash
                               : ZrCore_ExecutionFrameLayout_Hash(layout);
    candidate.layout = layout;
    candidate.rootCount = specCount;
    candidate.rootCapacity = specCount;
    ZrCore_ExecutionFrameRootMap_Free(map);
    *map = candidate;
    return ZR_TRUE;
}

TZrBool ZrCore_ExecutionFrameRootMap_Validate(
        const SZrExecutionFrameLayout *layout,
        const SZrExecutionFrameRootMap *map,
        SZrExecutionFrameDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 expectedHash;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (map == ZR_NULL) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                             0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (layout == ZR_NULL) {
        layout = map->layout;
    }
    if (layout != ZR_NULL &&
        !ZrCore_ExecutionFrameLayout_Validate(layout, diagnostic)) {
        return ZR_FALSE;
    }
    if (map->magic != ZR_EXECUTION_FRAME_LAYOUT_MAGIC ||
        map->schemaVersion != ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION ||
        map->rootCount > map->rootCapacity ||
        (map->rootCount != 0u && map->roots == ZR_NULL)) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                             0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    expectedHash = layout != ZR_NULL
                       ? (layout->layoutHash != 0u
                              ? layout->layoutHash
                              : ZrCore_ExecutionFrameLayout_Hash(layout))
                       : map->layoutHash;
    if (map->layoutHash != expectedHash) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_LAYOUT_HASH,
                             0u, 0u, expectedHash, map->layoutHash);
        return ZR_FALSE;
    }
    for (index = 0u; index < map->rootCount; ++index) {
        const SZrExecutionFrameRoot *root = &map->roots[index];
        const SZrExecutionFrameSlot *slot = layout != ZR_NULL
                ? ZrCore_ExecutionFrameLayout_FindLogical(layout,
                                                           root->logicalSlot)
                : ZR_NULL;
        TZrUInt32 fieldEnd;
        if ((layout != ZR_NULL &&
             (slot == ZR_NULL || slot->physicalSlot != root->physicalSlot ||
               slot->byteOffset != root->frameByteOffset ||
               slot->byteSize != root->byteSize ||
               (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED &&
                (slot->slotClass != ZR_EXECUTION_FRAME_SLOT_REFERENCE &&
                 slot->slotClass != ZR_EXECUTION_FRAME_SLOT_BOXED)))) ||
            root->kind >= ZR_EXECUTION_FRAME_ROOT_KIND_COUNT ||
            root->storage >= ZR_EXECUTION_FRAME_ROOT_STORAGE_COUNT ||
            !execution_frame_add_u32(root->frameByteOffset,
                                     root->byteSize, &fieldEnd) ||
            (layout != ZR_NULL && fieldEnd > layout->frameByteSize)) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                 index, 0u,
                                 layout != ZR_NULL ? layout->frameByteSize : 0u,
                                 root->frameByteOffset);
            return ZR_FALSE;
        }
        if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED && layout != ZR_NULL &&
            (root->basePhysicalSlot >= layout->storageSlotCount ||
             ZrCore_ExecutionFrameLayout_FindLogical(layout,
                                                     root->baseLogicalSlot) == ZR_NULL ||
             root->baseLogicalSlot == root->logicalSlot)) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                 index, 0u, root->baseLogicalSlot, 0u);
            return ZR_FALSE;
        }
        if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED && layout != ZR_NULL) {
            const SZrExecutionFrameSlot *baseSlot =
                    ZrCore_ExecutionFrameLayout_FindLogical(
                            layout, root->baseLogicalSlot);
            if (baseSlot == ZR_NULL ||
                baseSlot->physicalSlot != root->basePhysicalSlot ||
                baseSlot->byteOffset != root->baseFrameByteOffset ||
                baseSlot->byteSize < (TZrUInt32)sizeof(TZrPtr) ||
                (baseSlot->slotClass != ZR_EXECUTION_FRAME_SLOT_REFERENCE &&
                 baseSlot->slotClass != ZR_EXECUTION_FRAME_SLOT_BOXED)) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                     index, 0u, root->baseLogicalSlot,
                                     root->basePhysicalSlot);
                return ZR_FALSE;
            }
        }
        if (root->kind == ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD &&
            (!execution_frame_add_u32(root->fieldByteOffset,
                                      (TZrUInt32)sizeof(TZrPtr), &fieldEnd) ||
             fieldEnd > root->byteSize)) {
            execution_frame_diag(diagnostic,
                                 ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                                 index, 0u, root->byteSize, fieldEnd);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool execution_frame_derived_pointer(TZrPtr base,
                                               TZrInt64 offset,
                                               TZrPtr *out) {
    uintptr_t bits;
    uintptr_t adjusted;

    if (out == ZR_NULL) {
        return ZR_FALSE;
    }
    if (base == ZR_NULL) {
        *out = ZR_NULL;
        return ZR_TRUE;
    }
    bits = (uintptr_t)base;
    if (offset >= 0) {
        if ((uintmax_t)offset > (uintmax_t)UINTPTR_MAX -
                                  (uintmax_t)bits) {
            return ZR_FALSE;
        }
        adjusted = bits + (uintptr_t)offset;
    } else {
        uintmax_t magnitude = (uintmax_t)(-(offset + 1)) + 1u;
        if (magnitude > (uintmax_t)bits) {
            return ZR_FALSE;
        }
        adjusted = bits - (uintptr_t)magnitude;
    }
    *out = (TZrPtr)adjusted;
    return ZR_TRUE;
}

TZrBool ZrCore_Execution_VisitFrameRoots(
        struct SZrState *state,
        const SZrFrameRootVisitor *visitor,
        SZrExecutionDiagnostic *diagnostic) {
    TZrUInt32 pass;
    TZrUInt32 index;

    (void)state;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (visitor == ZR_NULL || visitor->rootMap == ZR_NULL ||
        visitor->frameBase == ZR_NULL || visitor->visit == ZR_NULL) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                             0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecutionFrameRootMap_Validate(
            ZR_NULL, visitor->rootMap, diagnostic)) {
        return ZR_FALSE;
    }
    if (visitor->frameByteSize == 0u && visitor->rootMap->rootCount != 0u) {
        execution_frame_diag(diagnostic,
                             ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                             0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    for (pass = 0u; pass < 2u; ++pass) {
        for (index = 0u; index < visitor->rootMap->rootCount; ++index) {
            SZrExecutionFrameRoot *root = &visitor->rootMap->roots[index];
            TZrUInt32 offset;
            TZrPtr address;
            TZrPtr baseAddress = ZR_NULL;
            TZrPtr baseValue = ZR_NULL;
            TZrPtr derived = ZR_NULL;

            if ((pass == 0u && root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) ||
                (pass == 1u && root->kind != ZR_EXECUTION_FRAME_ROOT_DERIVED)) {
                continue;
            }
            if (!root->initialized) {
                continue;
            }
            if (!execution_frame_add_u32(root->frameByteOffset,
                                         root->fieldByteOffset, &offset) ||
                root->byteSize < sizeof(TZrPtr) ||
                offset > visitor->frameByteSize ||
                root->byteSize > visitor->frameByteSize - offset) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                                     index, 0u, visitor->frameByteSize, offset);
                return ZR_FALSE;
            }
            address = (TZrPtr)(visitor->frameBase + offset);
            if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
                if (root->baseFrameByteOffset > visitor->frameByteSize ||
                    sizeof(TZrPtr) > visitor->frameByteSize -
                                      root->baseFrameByteOffset) {
                    execution_frame_diag(diagnostic,
                                         ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                                         index, 0u, visitor->frameByteSize,
                                         root->baseFrameByteOffset);
                    return ZR_FALSE;
                }
                baseAddress = (TZrPtr)(visitor->frameBase +
                                       root->baseFrameByteOffset);
                memcpy(&baseValue, baseAddress, sizeof(baseValue));
            }
            if (!visitor->visit(root, address, baseAddress,
                                visitor->userData)) {
                execution_frame_diag(diagnostic,
                                     ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
                                     index, 0u, 1u, 0u);
                return ZR_FALSE;
            }
            if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
                memcpy(&baseValue, baseAddress, sizeof(baseValue));
                if (!execution_frame_derived_pointer(baseValue,
                                                     root->derivedOffset,
                                                     &derived)) {
                    execution_frame_diag(diagnostic,
                                         ZR_EXECUTION_FRAME_DIAGNOSTIC_OVERFLOW,
                                         index, 0u, UINTPTR_MAX, 0u);
                    return ZR_FALSE;
                }
                memcpy(address, &derived, sizeof(derived));
            }
        }
    }
    return ZR_TRUE;
}
