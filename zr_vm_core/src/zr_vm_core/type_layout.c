//
// Created by Codex on 2026/5/16.
//

#include "zr_vm_core/type_layout.h"

#include <string.h>

#include "zr_vm_core/ownership.h"

#define ZR_TYPE_LAYOUT_HASH_OFFSET_BASIS UINT64_C(1469598103934665603)
#define ZR_TYPE_LAYOUT_HASH_PRIME UINT64_C(1099511628211)
static TZrUInt32 type_layout_normalize_align(TZrUInt32 align) {
    return align > 0u ? align : 1u;
}

static TZrBool type_layout_checked_add(TZrUInt32 left, TZrUInt32 right, TZrUInt32 *out) {
    if (out == ZR_NULL || left > UINT32_MAX - right) {
        return ZR_FALSE;
    }

    *out = left + right;
    return ZR_TRUE;
}

static TZrBool type_layout_align_offset(TZrUInt32 offset, TZrUInt32 align, TZrUInt32 *out) {
    TZrUInt32 normalizedAlign = type_layout_normalize_align(align);
    TZrUInt32 remainder;
    TZrUInt32 padding;

    if (out == ZR_NULL) {
        return ZR_FALSE;
    }

    remainder = offset % normalizedAlign;
    if (remainder == 0u) {
        *out = offset;
        return ZR_TRUE;
    }

    padding = normalizedAlign - remainder;
    return type_layout_checked_add(offset, padding, out);
}

static TZrBool type_layout_field_is_value_slot(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL && (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);
}

static TZrBool type_layout_field_is_gc_value(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL && (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
}

static TZrBool type_layout_field_is_ownership_value(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL && (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);
}

static TZrBool type_layout_is_value(const SZrTypeLayout *layout) {
    return (TZrBool)(layout != ZR_NULL && layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE);
}

static TZrBool type_layout_is_union(const SZrTypeLayout *layout) {
    return (TZrBool)(layout != ZR_NULL && layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION);
}

static TZrBool type_layout_field_is_nested(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL &&
                     (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT) != 0u);
}

static const SZrTypeLayout *type_layout_registry_resolve(
        const SZrTypeLayoutRegistryView *registry,
        TZrUInt32 typeLayoutIndex) {
    const SZrTypeLayout *layout;

    if (registry == ZR_NULL || registry->layouts == ZR_NULL ||
        typeLayoutIndex >= registry->count) {
        return ZR_NULL;
    }
    layout = registry->layouts[typeLayoutIndex];
    return layout != ZR_NULL && ZrCore_TypeLayout_Validate(layout) ? layout : ZR_NULL;
}

static TZrBool type_layout_is_blittable(EZrTypeLayoutCopyKind copyKind,
                                        EZrTypeLayoutDropKind dropKind,
                                        TZrUInt32 gcFieldCount,
                                        TZrUInt32 ownershipFieldCount,
                                        TZrUInt32 refFieldCount) {
    return (TZrBool)(copyKind == ZR_TYPE_LAYOUT_COPY_KIND_BITWISE &&
                     dropKind == ZR_TYPE_LAYOUT_DROP_KIND_NONE &&
                     gcFieldCount == 0u &&
                     ownershipFieldCount == 0u &&
                     refFieldCount == 0u);
}

static TZrUInt64 type_layout_hash_byte(TZrUInt64 hash, TZrUInt8 value) {
    hash ^= (TZrUInt64)value;
    return hash * ZR_TYPE_LAYOUT_HASH_PRIME;
}

static TZrUInt64 type_layout_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    for (TZrUInt32 shift = 0u; shift < 32u; shift += 8u) {
        hash = type_layout_hash_byte(hash, (TZrUInt8)((value >> shift) & 0xffu));
    }
    return hash;
}

static TZrBool type_layout_try_get_flagged_field_offset(const SZrTypeLayout *layout,
                                                        TZrUInt32 flag,
                                                        TZrUInt32 mapIndex,
                                                        TZrUInt32 *outOffset) {
    TZrUInt32 found = 0u;

    if (layout == ZR_NULL || layout->fields == ZR_NULL || outOffset == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 fieldIndex = 0u; fieldIndex < layout->fieldCount; fieldIndex++) {
        if ((layout->fields[fieldIndex].flags & flag) == 0u) {
            continue;
        }
        if (found == mapIndex) {
            *outOffset = layout->fields[fieldIndex].byteOffset;
            return ZR_TRUE;
        }
        found++;
    }
    return ZR_FALSE;
}

static TZrBool type_layout_try_get_map_offset(const SZrTypeLayout *layout,
                                              const TZrUInt32 *offsets,
                                              TZrUInt32 flag,
                                              TZrUInt32 mapIndex,
                                              TZrUInt32 *outOffset) {
    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (offsets != ZR_NULL) {
        *outOffset = offsets[mapIndex];
        return ZR_TRUE;
    }
    return type_layout_try_get_flagged_field_offset(layout, flag, mapIndex, outOffset);
}

TZrBool ZrCore_TypeLayout_TryGetGcFieldOffset(const SZrTypeLayout *layout,
                                              TZrUInt32 fieldIndex,
                                              TZrUInt32 *outOffset) {
    return (TZrBool)(layout != ZR_NULL &&
                     fieldIndex < layout->gcFieldCount &&
                     type_layout_try_get_map_offset(layout,
                                                    layout->gcFieldOffsets,
                                                    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE,
                                                    fieldIndex,
                                                    outOffset));
}

TZrBool ZrCore_TypeLayout_TryGetOwnershipFieldOffset(const SZrTypeLayout *layout,
                                                     TZrUInt32 fieldIndex,
                                                     TZrUInt32 *outOffset) {
    return (TZrBool)(layout != ZR_NULL &&
                     fieldIndex < layout->ownershipFieldCount &&
                     type_layout_try_get_map_offset(layout,
                                                    layout->ownershipFieldOffsets,
                                                    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE,
                                                    fieldIndex,
                                                    outOffset));
}

TZrBool ZrCore_TypeLayout_TryGetRefFieldOffset(const SZrTypeLayout *layout,
                                               TZrUInt32 fieldIndex,
                                               TZrUInt32 *outOffset) {
    return (TZrBool)(layout != ZR_NULL &&
                     fieldIndex < layout->refFieldCount &&
                     type_layout_try_get_map_offset(layout,
                                                    layout->refFieldOffsets,
                                                    ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE,
                                                    fieldIndex,
                                                    outOffset));
}

static TZrUInt64 type_layout_hash_map(TZrUInt64 hash,
                                      const SZrTypeLayout *layout,
                                      const TZrUInt32 *offsets,
                                      TZrUInt32 count,
                                      TZrUInt32 flag) {
    for (TZrUInt32 index = 0u; index < count; index++) {
        TZrUInt32 offset = UINT32_MAX;
        (void)type_layout_try_get_map_offset(layout, offsets, flag, index, &offset);
        hash = type_layout_hash_u32(hash, offset);
    }
    return hash;
}

TZrUInt64 ZrCore_TypeLayout_ComputeHash(const SZrTypeLayout *layout) {
    TZrUInt64 hash = ZR_TYPE_LAYOUT_HASH_OFFSET_BASIS;

    if (layout == ZR_NULL) {
        return 0u;
    }

    hash = type_layout_hash_u32(hash, ZR_TYPE_LAYOUT_SCHEMA_VERSION);
    hash = type_layout_hash_u32(hash, layout->byteSize);
    hash = type_layout_hash_u32(hash, layout->byteAlign);
    hash = type_layout_hash_u32(hash, layout->kind);
    hash = type_layout_hash_u32(hash, layout->copyKind);
    hash = type_layout_hash_u32(hash, layout->dropKind);
    hash = type_layout_hash_u32(hash, layout->gcScanKind);
    hash = type_layout_hash_u32(hash, layout->fieldCount);
    hash = type_layout_hash_u32(hash, layout->gcFieldCount);
    hash = type_layout_hash_u32(hash, layout->ownershipFieldCount);
    hash = type_layout_hash_u32(hash, layout->refFieldCount);
    hash = type_layout_hash_u32(hash, layout->tagOffset);
    hash = type_layout_hash_u32(hash, layout->tagSize);
    hash = type_layout_hash_u32(hash, layout->blittable ? 1u : 0u);

    for (TZrUInt32 index = 0u; layout->fields != ZR_NULL && index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        hash = type_layout_hash_u32(hash, field->byteOffset);
        hash = type_layout_hash_u32(hash, field->byteSize);
        hash = type_layout_hash_u32(hash, field->typeLayoutIndex);
        hash = type_layout_hash_u32(hash, field->flags);
        hash = type_layout_hash_u32(hash, field->activeTag);
    }

    hash = type_layout_hash_map(hash,
                                layout,
                                layout->gcFieldOffsets,
                                layout->gcFieldCount,
                                ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE);
    hash = type_layout_hash_map(hash,
                                layout,
                                layout->ownershipFieldOffsets,
                                layout->ownershipFieldCount,
                                ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE);
    hash = type_layout_hash_map(hash,
                                layout,
                                layout->refFieldOffsets,
                                layout->refFieldCount,
                                ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE);

    return hash != 0u ? hash : 1u;
}

static TZrUInt32 type_layout_count_flagged_fields(const SZrTypeLayout *layout, TZrUInt32 flag) {
    TZrUInt32 count = 0u;

    for (TZrUInt32 index = 0u;
         layout != ZR_NULL && layout->fields != ZR_NULL && index < layout->fieldCount;
         index++) {
        if ((layout->fields[index].flags & flag) != 0u) {
            count++;
        }
    }
    return count;
}

static TZrBool type_layout_validate_map(const SZrTypeLayout *layout,
                                        const TZrUInt32 *offsets,
                                        TZrUInt32 count,
                                        TZrUInt32 flag) {
    for (TZrUInt32 index = 0u; index < count; index++) {
        TZrUInt32 byteOffset;

        if (!type_layout_try_get_map_offset(layout, offsets, flag, index, &byteOffset) ||
            byteOffset > layout->byteSize ||
            sizeof(SZrTypeValue) > layout->byteSize - byteOffset) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_TypeLayout_Validate(const SZrTypeLayout *layout) {
    TZrUInt32 cursor = 0u;

    if (layout == ZR_NULL ||
        layout->layoutVersion != ZR_TYPE_LAYOUT_SCHEMA_VERSION ||
        layout->layoutHash == 0u ||
        layout->byteSize == 0u ||
        layout->byteAlign == 0u ||
        (layout->byteAlign & (layout->byteAlign - 1u)) != 0u ||
        layout->kind > (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
        layout->copyKind > (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY ||
        layout->dropKind > (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS ||
        layout->gcScanKind > (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_BARRIERED ||
        (layout->fieldCount > 0u && layout->fields == ZR_NULL)) {
        return ZR_FALSE;
    }

    if ((layout->dropKind == (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS) !=
        (layout->customDrop != ZR_NULL)) {
        return ZR_FALSE;
    }
    if ((layout->gcFieldCount == 0u) !=
        (layout->gcScanKind == (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_FREE)) {
        return ZR_FALSE;
    }

    if (!type_layout_is_value(layout)) {
        if (type_layout_count_flagged_fields(layout, ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) !=
                layout->gcFieldCount ||
            type_layout_count_flagged_fields(layout, ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) !=
                layout->ownershipFieldCount ||
            type_layout_count_flagged_fields(layout, ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE) !=
                layout->refFieldCount) {
            return ZR_FALSE;
        }

        for (TZrUInt32 index = 0u; index < layout->fieldCount; index++) {
            const SZrTypeLayoutField *field = &layout->fields[index];
            TZrUInt32 fieldEnd;

            if (!type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
                fieldEnd > layout->byteSize ||
                (!type_layout_is_union(layout) && field->byteOffset < cursor) ||
                (field->flags & ~(TZrUInt32)(ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                             ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                             ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE |
                                             ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE |
                                             ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT)) != 0u ||
                (type_layout_field_is_nested(field) &&
                 type_layout_field_is_value_slot(field)) ||
                ((field->flags & (ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                  ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE |
                                  ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE)) != 0u &&
                 !type_layout_field_is_value_slot(field)) ||
                (type_layout_field_is_value_slot(field) &&
                 field->byteSize < sizeof(SZrTypeValue))) {
                return ZR_FALSE;
            }
            if (!type_layout_is_union(layout)) {
                cursor = fieldEnd;
            }
        }
    }

    if ((!type_layout_is_value(layout) &&
         (!type_layout_validate_map(layout,
                                    layout->gcFieldOffsets,
                                    layout->gcFieldCount,
                                    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) ||
          !type_layout_validate_map(layout,
                                    layout->ownershipFieldOffsets,
                                    layout->ownershipFieldCount,
                                    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) ||
          !type_layout_validate_map(layout,
                                    layout->refFieldOffsets,
                                    layout->refFieldCount,
                                    ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE))) ||
        layout->blittable !=
                type_layout_is_blittable((EZrTypeLayoutCopyKind)layout->copyKind,
                                         (EZrTypeLayoutDropKind)layout->dropKind,
                                         layout->gcFieldCount,
                                         layout->ownershipFieldCount,
                                         layout->refFieldCount) ||
        layout->layoutHash != ZrCore_TypeLayout_ComputeHash(layout)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static SZrTypeValue *type_layout_value_at(TZrPtr storage, TZrUInt32 byteOffset) {
    return (SZrTypeValue *)((TZrBytePtr)storage + byteOffset);
}

static TZrBool type_layout_can_visit_gc_offset_table(const SZrTypeLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     !type_layout_is_union(layout) &&
                     layout->gcFieldCount > 0u &&
                     layout->gcFieldOffsets != ZR_NULL);
}

static const SZrTypeValue *type_layout_const_value_at(const void *storage, TZrUInt32 byteOffset) {
    return (const SZrTypeValue *)((const TZrByte *)storage + byteOffset);
}

static void type_layout_copy_bytes(TZrBytePtr destination,
                                   const TZrByte *source,
                                   TZrUInt32 offset,
                                   TZrUInt32 size) {
    if (size == 0u) {
        return;
    }

    memmove(destination + offset, source + offset, size);
}

static TZrBool type_layout_read_union_tag(const SZrTypeLayout *layout,
                                          const void *storage,
                                          TZrUInt32 *outTag) {
    const TZrByte *bytes;

    if (outTag != ZR_NULL) {
        *outTag = 0u;
    }
    if (layout == ZR_NULL ||
        storage == ZR_NULL ||
        outTag == ZR_NULL ||
        layout->tagSize == 0u ||
        layout->tagOffset > layout->byteSize ||
        layout->tagSize > layout->byteSize - layout->tagOffset) {
        return ZR_FALSE;
    }

    bytes = (const TZrByte *)storage + layout->tagOffset;
    switch (layout->tagSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 tag;
            memcpy(&tag, bytes, sizeof(tag));
            *outTag = (TZrUInt32)tag;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 tag;
            memcpy(&tag, bytes, sizeof(tag));
            *outTag = (TZrUInt32)tag;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 tag;
            memcpy(&tag, bytes, sizeof(tag));
            *outTag = tag;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_layout_field_matches_active_tag(const SZrTypeLayout *layout,
                                                    const SZrTypeLayoutField *field,
                                                    TZrUInt32 activeTag) {
    return (TZrBool)(!type_layout_is_union(layout) ||
                     (field != ZR_NULL && field->activeTag == activeTag));
}

static TZrBool type_layout_copy_inline_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr destination,
        const void *source,
        TZrUInt32 depth);

static TZrBool type_layout_drop_inline_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount,
        TZrBool partial,
        TZrUInt32 depth);

static TZrBool type_layout_copy_inline_union(struct SZrState *state,
                                             const SZrTypeLayout *layout,
                                             const SZrTypeLayoutRegistryView *registry,
                                             TZrPtr destination,
                                             const void *source,
                                             TZrUInt32 depth) {
    TZrBytePtr destinationBytes = (TZrBytePtr)destination;
    const TZrByte *sourceBytes = (const TZrByte *)source;
    TZrUInt32 activeTag;
    TZrUInt32 cursor = 0u;

    if (!type_layout_read_union_tag(layout, source, &activeTag)) {
        return ZR_FALSE;
    }

    if (!type_layout_drop_inline_with_registry(
                state, layout, registry, destination, ZR_NULL, 0u, ZR_FALSE, depth)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (!type_layout_field_matches_active_tag(layout, field, activeTag)) {
            continue;
        }
        if (!type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize ||
            field->byteOffset < cursor) {
            return ZR_FALSE;
        }

        type_layout_copy_bytes(destinationBytes, sourceBytes, cursor, field->byteOffset - cursor);

        if (type_layout_field_is_nested(field)) {
            const SZrTypeLayout *nestedLayout = type_layout_registry_resolve(
                    registry, field->typeLayoutIndex);
            if (nestedLayout == ZR_NULL || nestedLayout->byteSize != field->byteSize ||
                !type_layout_copy_inline_with_registry(
                        state,
                        nestedLayout,
                        registry,
                        destinationBytes + field->byteOffset,
                        sourceBytes + field->byteOffset,
                        depth + 1u)) {
                return ZR_FALSE;
            }
        } else if (type_layout_field_is_value_slot(field)) {
            SZrTypeValue *destinationValue = type_layout_value_at(destination, field->byteOffset);
            const SZrTypeValue *sourceValue = type_layout_const_value_at(source, field->byteOffset);
            ZrCore_Value_Copy(state, destinationValue, sourceValue);
        } else {
            type_layout_copy_bytes(destinationBytes, sourceBytes, field->byteOffset, field->byteSize);
        }

        cursor = fieldEnd;
    }

    type_layout_copy_bytes(destinationBytes, sourceBytes, cursor, layout->byteSize - cursor);
    return ZR_TRUE;
}

TZrBool ZrCore_TypeLayout_CanRawCopy(const SZrTypeLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->blittable);
}

static TZrBool type_layout_copy_inline_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr destination,
        const void *source,
        TZrUInt32 depth) {
    TZrBytePtr destinationBytes;
    const TZrByte *sourceBytes;
    TZrUInt32 cursor = 0u;

    if (layout == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH) {
        return ZR_FALSE;
    }

    if (layout->copyKind == (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY) {
        return ZR_FALSE;
    }

    if (destination == source || layout->byteSize == 0u) {
        return ZR_TRUE;
    }

    destinationBytes = (TZrBytePtr)destination;
    sourceBytes = (const TZrByte *)source;

    if (type_layout_is_value(layout)) {
        if (layout->byteSize < sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }

        ZrCore_Value_Copy(state, (SZrTypeValue *)destination, (const SZrTypeValue *)source);
        return ZR_TRUE;
    }

    if (ZrCore_TypeLayout_CanRawCopy(layout)) {
        memmove(destinationBytes, sourceBytes, layout->byteSize);
        return ZR_TRUE;
    }

    if (type_layout_is_union(layout)) {
        return type_layout_copy_inline_union(
                state, layout, registry, destination, source, depth);
    }

    if (layout->copyKind != (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (!type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize ||
            field->byteOffset < cursor) {
            return ZR_FALSE;
        }

        type_layout_copy_bytes(destinationBytes, sourceBytes, cursor, field->byteOffset - cursor);

        if (type_layout_field_is_nested(field)) {
            const SZrTypeLayout *nestedLayout = type_layout_registry_resolve(
                    registry, field->typeLayoutIndex);
            if (nestedLayout == ZR_NULL || nestedLayout->byteSize != field->byteSize ||
                !type_layout_copy_inline_with_registry(
                        state,
                        nestedLayout,
                        registry,
                        destinationBytes + field->byteOffset,
                        sourceBytes + field->byteOffset,
                        depth + 1u)) {
                return ZR_FALSE;
            }
        } else if (type_layout_field_is_value_slot(field)) {
            SZrTypeValue *destinationValue = type_layout_value_at(destination, field->byteOffset);
            const SZrTypeValue *sourceValue = type_layout_const_value_at(source, field->byteOffset);
            ZrCore_Value_Copy(state, destinationValue, sourceValue);
        } else {
            type_layout_copy_bytes(destinationBytes, sourceBytes, field->byteOffset, field->byteSize);
        }

        cursor = fieldEnd;
    }

    type_layout_copy_bytes(destinationBytes, sourceBytes, cursor, layout->byteSize - cursor);
    return ZR_TRUE;
}

TZrBool ZrCore_TypeLayout_CopyInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr destination,
        const void *source) {
    return type_layout_copy_inline_with_registry(
            state, layout, registry, destination, source, 0u);
}

TZrBool ZrCore_TypeLayout_CopyInline(struct SZrState *state,
                                     const SZrTypeLayout *layout,
                                     TZrPtr destination,
                                     const void *source) {
    return ZrCore_TypeLayout_CopyInlineWithRegistry(
            state, layout, ZR_NULL, destination, source);
}

static TZrBool type_layout_partial_field_is_initialized(
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount,
        TZrUInt32 fieldIndex) {
    TZrUInt32 wordIndex = fieldIndex / 64u;
    TZrUInt32 bitIndex = fieldIndex % 64u;

    return (TZrBool)(initializedFieldWords != ZR_NULL &&
                     wordIndex < initializedFieldWordCount &&
                     (initializedFieldWords[wordIndex] & ((TZrUInt64)1u << bitIndex)) != 0u);
}

static TZrBool type_layout_drop_inline_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount,
        TZrBool partial,
        TZrUInt32 depth) {
    TZrUInt32 activeTag = 0u;
    TZrBool success = ZR_TRUE;

    if (type_layout_is_value(layout)) {
        if (storage != ZR_NULL && layout->byteSize >= sizeof(SZrTypeValue)) {
            ZrCore_Ownership_ReleaseValue(state, (SZrTypeValue *)storage);
        }
        return ZR_TRUE;
    }

    if (layout == ZR_NULL || storage == ZR_NULL ||
        depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH ||
        layout->dropKind == (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE) {
        return (TZrBool)(layout != ZR_NULL && storage != ZR_NULL &&
                         depth <= ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH);
    }

    if (!partial &&
        layout->dropKind == (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS) {
        if (layout->customDrop == ZR_NULL) {
            return ZR_FALSE;
        }
        layout->customDrop(state, storage, layout->customDropUserData);
    } else if (layout->dropKind != (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE &&
               layout->dropKind != (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS) {
        return ZR_FALSE;
    }

    if (type_layout_is_union(layout) && !type_layout_read_union_tag(layout, storage, &activeTag)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = layout->fieldCount; index > 0u; index--) {
        const SZrTypeLayoutField *field = &layout->fields[index - 1u];
        TZrUInt32 fieldEnd;

        if ((partial && !type_layout_partial_field_is_initialized(
                                initializedFieldWords,
                                initializedFieldWordCount,
                                index - 1u)) ||
            !type_layout_field_matches_active_tag(layout, field, activeTag) ||
            !type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize) {
            continue;
        }

        if (type_layout_field_is_nested(field)) {
            const SZrTypeLayout *nestedLayout = type_layout_registry_resolve(
                    registry, field->typeLayoutIndex);
            if (nestedLayout == ZR_NULL || nestedLayout->byteSize != field->byteSize ||
                !type_layout_drop_inline_with_registry(
                        state,
                        nestedLayout,
                        registry,
                        (TZrBytePtr)storage + field->byteOffset,
                        ZR_NULL,
                        0u,
                        ZR_FALSE,
                        depth + 1u)) {
                success = ZR_FALSE;
            }
            continue;
        }
        if (!type_layout_field_is_value_slot(field) ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        {
            SZrTypeValue *value = type_layout_value_at(storage, field->byteOffset);
            if (value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
                type_layout_field_is_ownership_value(field)) {
                ZrCore_Ownership_ReleaseValue(state, value);
            }
        }
    }
    return success;
}

TZrBool ZrCore_TypeLayout_DropInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage) {
    return type_layout_drop_inline_with_registry(
            state, layout, registry, storage, ZR_NULL, 0u, ZR_FALSE, 0u);
}

TZrBool ZrCore_TypeLayout_DropPartialInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount) {
    if (layout == ZR_NULL || storage == ZR_NULL ||
        (layout->fieldCount > 0u && initializedFieldWords == ZR_NULL)) {
        return ZR_FALSE;
    }
    return type_layout_drop_inline_with_registry(
            state,
            layout,
            registry,
            storage,
            initializedFieldWords,
            initializedFieldWordCount,
            ZR_TRUE,
            0u);
}

void ZrCore_TypeLayout_DropInline(struct SZrState *state, const SZrTypeLayout *layout, TZrPtr storage) {
    (void)ZrCore_TypeLayout_DropInlineWithRegistry(
            state, layout, ZR_NULL, storage);
}

static TZrBool type_layout_visit_gc_values_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr userData,
        TZrUInt32 depth) {
    TZrUInt32 activeTag = 0u;

    if (layout == ZR_NULL || storage == ZR_NULL || visitor == ZR_NULL ||
        depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH) {
        return ZR_FALSE;
    }

    if (type_layout_is_value(layout)) {
        if (layout->byteSize >= sizeof(SZrTypeValue)) {
            visitor(state, (SZrTypeValue *)storage, userData);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (type_layout_is_union(layout) && !type_layout_read_union_tag(layout, storage, &activeTag)) {
        return ZR_FALSE;
    }

    if (type_layout_can_visit_gc_offset_table(layout)) {
        for (TZrUInt32 index = 0; index < layout->gcFieldCount; index++) {
            TZrUInt32 byteOffset = layout->gcFieldOffsets[index];

            if (byteOffset > layout->byteSize ||
                sizeof(SZrTypeValue) > layout->byteSize - byteOffset) {
                continue;
            }

            visitor(state, type_layout_value_at(storage, byteOffset), userData);
        }
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (!type_layout_field_matches_active_tag(layout, field, activeTag) ||
            !type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize) {
            continue;
        }

        if (type_layout_field_is_nested(field)) {
            const SZrTypeLayout *nestedLayout = type_layout_registry_resolve(
                    registry, field->typeLayoutIndex);
            if (nestedLayout == ZR_NULL || nestedLayout->byteSize != field->byteSize ||
                !type_layout_visit_gc_values_with_registry(
                        state,
                        nestedLayout,
                        registry,
                        (TZrBytePtr)storage + field->byteOffset,
                        visitor,
                        userData,
                        depth + 1u)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (!type_layout_field_is_value_slot(field) ||
            !type_layout_field_is_gc_value(field) ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        visitor(state, type_layout_value_at(storage, field->byteOffset), userData);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_TypeLayout_VisitGcValuesWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr userData) {
    return type_layout_visit_gc_values_with_registry(
            state, layout, registry, storage, visitor, userData, 0u);
}

void ZrCore_TypeLayout_VisitGcValues(struct SZrState *state,
                                     const SZrTypeLayout *layout,
                                     TZrPtr storage,
                                     FZrTypeLayoutGcValueVisitor visitor,
                                     TZrPtr userData) {
    (void)ZrCore_TypeLayout_VisitGcValuesWithRegistry(
            state, layout, ZR_NULL, storage, visitor, userData);
}

TZrBool ZrCore_StackFrameLayout_BuildSequential(SZrStackFrameLayout *frameLayout,
                                                SZrStackFrameLayoutSlot *slots,
                                                TZrUInt32 slotCount) {
    TZrUInt32 cursor = 0u;
    TZrUInt32 maxAlign = 1u;

    if (frameLayout == ZR_NULL || (slotCount > 0u && slots == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < slotCount; index++) {
        const SZrTypeLayout *layout = slots[index].typeLayout;
        TZrUInt32 alignedOffset;

        if (layout == ZR_NULL ||
            !type_layout_align_offset(cursor, layout->byteAlign, &alignedOffset)) {
            return ZR_FALSE;
        }

        slots[index].byteOffset = alignedOffset;
        cursor = alignedOffset;
        if (!type_layout_checked_add(cursor, layout->byteSize, &cursor)) {
            return ZR_FALSE;
        }
        if (layout->byteAlign > maxAlign) {
            maxAlign = layout->byteAlign;
        }
    }

    if (!type_layout_align_offset(cursor, maxAlign, &cursor)) {
        return ZR_FALSE;
    }

    frameLayout->byteSize = cursor;
    frameLayout->maxAlign = maxAlign;
    frameLayout->slotCount = slotCount;
    return ZR_TRUE;
}
