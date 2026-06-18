//
// Created by Codex on 2026/5/16.
//

#include "zr_vm_core/type_layout.h"

#include <string.h>

#include "zr_vm_core/ownership.h"

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

static SZrTypeValue *type_layout_value_at(TZrPtr storage, TZrUInt32 byteOffset) {
    return (SZrTypeValue *)((TZrBytePtr)storage + byteOffset);
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

static TZrBool type_layout_copy_inline_union(struct SZrState *state,
                                             const SZrTypeLayout *layout,
                                             TZrPtr destination,
                                             const void *source) {
    TZrBytePtr destinationBytes = (TZrBytePtr)destination;
    const TZrByte *sourceBytes = (const TZrByte *)source;
    TZrUInt32 activeTag;
    TZrUInt32 cursor = 0u;

    if (!type_layout_read_union_tag(layout, source, &activeTag)) {
        return ZR_FALSE;
    }

    ZrCore_TypeLayout_DropInline(state, layout, destination);
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

        if (type_layout_field_is_value_slot(field)) {
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

void ZrCore_TypeLayout_InitValue(SZrTypeLayout *layout) {
    if (layout == ZR_NULL) {
        return;
    }

    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = (TZrUInt32)ZR_ALIGN_SIZE;
    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    layout->copyKind = (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY;
    layout->dropKind = (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP;
    layout->reserved0 = 0u;
    layout->fields = ZR_NULL;
    layout->fieldCount = 0u;
    layout->gcFieldCount = 1u;
    layout->ownershipFieldCount = 1u;
    layout->tagOffset = 0u;
    layout->tagSize = 0u;
}

void ZrCore_TypeLayout_InitStruct(SZrTypeLayout *layout,
                                  TZrUInt32 byteSize,
                                  TZrUInt32 byteAlign,
                                  EZrTypeLayoutCopyKind copyKind,
                                  EZrTypeLayoutDropKind dropKind,
                                  const SZrTypeLayoutField *fields,
                                  TZrUInt32 fieldCount) {
    TZrUInt32 gcFieldCount = 0u;
    TZrUInt32 ownershipFieldCount = 0u;

    if (layout == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < fieldCount; index++) {
        if (type_layout_field_is_gc_value(&fields[index])) {
            gcFieldCount++;
        }
        if (type_layout_field_is_ownership_value(&fields[index])) {
            ownershipFieldCount++;
        }
    }

    layout->byteSize = byteSize;
    layout->byteAlign = type_layout_normalize_align(byteAlign);
    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    layout->copyKind = (TZrUInt8)copyKind;
    layout->dropKind = (TZrUInt8)dropKind;
    layout->reserved0 = 0u;
    layout->fields = fields;
    layout->fieldCount = fieldCount;
    layout->gcFieldCount = gcFieldCount;
    layout->ownershipFieldCount = ownershipFieldCount;
    layout->tagOffset = 0u;
    layout->tagSize = 0u;
}

void ZrCore_TypeLayout_InitUnion(SZrTypeLayout *layout,
                                 TZrUInt32 byteSize,
                                 TZrUInt32 byteAlign,
                                 TZrUInt32 tagOffset,
                                 TZrUInt32 tagSize,
                                 EZrTypeLayoutCopyKind copyKind,
                                 EZrTypeLayoutDropKind dropKind,
                                 const SZrTypeLayoutField *fields,
                                 TZrUInt32 fieldCount) {
    ZrCore_TypeLayout_InitStruct(layout, byteSize, byteAlign, copyKind, dropKind, fields, fieldCount);
    if (layout == ZR_NULL) {
        return;
    }

    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION;
    layout->tagOffset = tagOffset;
    layout->tagSize = tagSize;
}

TZrBool ZrCore_TypeLayout_CanRawCopy(const SZrTypeLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->copyKind == (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_POD &&
                     layout->dropKind == (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE &&
                     layout->gcFieldCount == 0u &&
                     layout->ownershipFieldCount == 0u);
}

TZrBool ZrCore_TypeLayout_CopyInline(struct SZrState *state,
                                     const SZrTypeLayout *layout,
                                     TZrPtr destination,
                                     const void *source) {
    TZrBytePtr destinationBytes;
    const TZrByte *sourceBytes;
    TZrUInt32 cursor = 0u;

    if (layout == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
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
        return type_layout_copy_inline_union(state, layout, destination, source);
    }

    if (layout->copyKind != (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY) {
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

        if (type_layout_field_is_value_slot(field)) {
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

void ZrCore_TypeLayout_DropInline(struct SZrState *state, const SZrTypeLayout *layout, TZrPtr storage) {
    TZrUInt32 activeTag = 0u;

    if (type_layout_is_value(layout)) {
        if (storage != ZR_NULL && layout->byteSize >= sizeof(SZrTypeValue)) {
            ZrCore_Ownership_ReleaseValue(state, (SZrTypeValue *)storage);
        }
        return;
    }

    if (layout == ZR_NULL || storage == ZR_NULL ||
        layout->dropKind != (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP) {
        return;
    }

    if (type_layout_is_union(layout) && !type_layout_read_union_tag(layout, storage, &activeTag)) {
        return;
    }

    for (TZrUInt32 index = 0; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (!type_layout_field_matches_active_tag(layout, field, activeTag) ||
            !type_layout_field_is_value_slot(field) ||
            !type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize ||
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
}

void ZrCore_TypeLayout_VisitGcValues(struct SZrState *state,
                                     const SZrTypeLayout *layout,
                                     TZrPtr storage,
                                     FZrTypeLayoutGcValueVisitor visitor,
                                     TZrPtr userData) {
    TZrUInt32 activeTag = 0u;

    if (layout == ZR_NULL || storage == ZR_NULL || visitor == ZR_NULL) {
        return;
    }

    if (type_layout_is_value(layout)) {
        if (layout->byteSize >= sizeof(SZrTypeValue)) {
            visitor(state, (SZrTypeValue *)storage, userData);
        }
        return;
    }

    if (type_layout_is_union(layout) && !type_layout_read_union_tag(layout, storage, &activeTag)) {
        return;
    }

    for (TZrUInt32 index = 0; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (!type_layout_field_matches_active_tag(layout, field, activeTag) ||
            !type_layout_field_is_value_slot(field) ||
            !type_layout_field_is_gc_value(field) ||
            !type_layout_checked_add(field->byteOffset, field->byteSize, &fieldEnd) ||
            fieldEnd > layout->byteSize ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        visitor(state, type_layout_value_at(storage, field->byteOffset), userData);
    }
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
