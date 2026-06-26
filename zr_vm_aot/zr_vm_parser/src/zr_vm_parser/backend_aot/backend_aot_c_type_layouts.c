#include "backend_aot_c_type_layouts.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include <string.h>

typedef struct SZrAotCTypeLayoutEmitContext {
    FILE *file;
    TZrUInt32 cursor;
    TZrUInt32 fieldIndex;
    TZrUInt32 paddingIndex;
    TZrBool failed;
} SZrAotCTypeLayoutEmitContext;

static TZrBool backend_aot_c_type_layout_seen_before(const SZrAotFunctionTable *table,
                                                     TZrUInt32 entryIndex,
                                                     TZrUInt32 slotIndex,
                                                     TZrUInt32 typeLayoutId) {
    if (table == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    for (TZrUInt32 previousEntryIndex = 0u; previousEntryIndex <= entryIndex; previousEntryIndex++) {
        const SZrFunction *function = table->entries[previousEntryIndex].function;
        TZrUInt32 slotLimit;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        slotLimit = previousEntryIndex == entryIndex ? slotIndex : function->frameSlotLayoutLength;
        for (TZrUInt32 previousSlotIndex = 0u; previousSlotIndex < slotLimit; previousSlotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[previousSlotIndex];
            if (slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                slotLayout->typeLayoutId == typeLayoutId) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

TZrUInt32 backend_aot_c_type_layout_count_referenced(const SZrAotFunctionTable *table) {
    TZrUInt32 count = 0u;

    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                backend_aot_c_type_layout_seen_before(table, entryIndex, slotIndex, slotLayout->typeLayoutId)) {
                continue;
            }

            count++;
        }
    }

    return count;
}

unsigned long long backend_aot_c_type_layout_payload_bytes_referenced(const SZrAotFunctionTable *table) {
    unsigned long long totalBytes = 0u;

    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                backend_aot_c_type_layout_seen_before(table, entryIndex, slotIndex, slotLayout->typeLayoutId)) {
                continue;
            }

            totalBytes += (unsigned long long)slotLayout->byteSize;
        }
    }

    return totalBytes;
}

const SZrTypeLayout *backend_aot_c_type_layout_resolve_from_table(SZrState *state,
                                                                  const SZrAotFunctionTable *table,
                                                                  TZrUInt32 typeLayoutId) {
    if (state == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_NULL;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];
            const SZrTypeLayout *typeLayout;

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId != typeLayoutId) {
                continue;
            }

            typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
            if (typeLayout != ZR_NULL &&
                (typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT ||
                 typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
                return typeLayout;
            }
        }
    }

    return ZR_NULL;
}

TZrUInt32 backend_aot_c_type_layout_index_space(SZrState *state, const SZrAotFunctionTable *table) {
    TZrUInt32 indexSpace = 0u;

    if (state == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                backend_aot_c_type_layout_resolve_from_table(state, table, slotLayout->typeLayoutId) == ZR_NULL) {
                continue;
            }

            if (slotLayout->typeLayoutId + 1u > indexSpace) {
                indexSpace = slotLayout->typeLayoutId + 1u;
            }
        }
    }

    return indexSpace;
}

static const TZrChar *backend_aot_c_type_layout_primitive_c_type(
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod) {
        return ZR_NULL;
    }

    switch (fieldLayout->valueType) {
        case ZR_VALUE_TYPE_BOOL:
            return fieldLayout->byteSize == sizeof(TZrBool) ? "TZrBool" : ZR_NULL;
        case ZR_VALUE_TYPE_INT8:
            return fieldLayout->byteSize == sizeof(TZrInt8) ? "TZrInt8" : ZR_NULL;
        case ZR_VALUE_TYPE_INT16:
            return fieldLayout->byteSize == sizeof(TZrInt16) ? "TZrInt16" : ZR_NULL;
        case ZR_VALUE_TYPE_INT32:
            return fieldLayout->byteSize == sizeof(TZrInt32) ? "TZrInt32" : ZR_NULL;
        case ZR_VALUE_TYPE_INT64:
            return fieldLayout->byteSize == sizeof(TZrInt64) ? "TZrInt64" : ZR_NULL;
        case ZR_VALUE_TYPE_UINT8:
            return fieldLayout->byteSize == sizeof(TZrUInt8) ? "TZrUInt8" : ZR_NULL;
        case ZR_VALUE_TYPE_UINT16:
            return fieldLayout->byteSize == sizeof(TZrUInt16) ? "TZrUInt16" : ZR_NULL;
        case ZR_VALUE_TYPE_UINT32:
            return fieldLayout->byteSize == sizeof(TZrUInt32) ? "TZrUInt32" : ZR_NULL;
        case ZR_VALUE_TYPE_UINT64:
            return fieldLayout->byteSize == sizeof(TZrUInt64) ? "TZrUInt64" : ZR_NULL;
        case ZR_VALUE_TYPE_FLOAT:
            return fieldLayout->byteSize == sizeof(TZrFloat32) ? "TZrFloat32" : ZR_NULL;
        case ZR_VALUE_TYPE_DOUBLE:
            return fieldLayout->byteSize == sizeof(TZrDouble) ? "TZrDouble" : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static void backend_aot_c_type_layout_write_padding(SZrAotCTypeLayoutEmitContext *context,
                                                    TZrUInt32 targetOffset) {
    TZrUInt32 paddingSize;

    if (context == ZR_NULL || context->file == ZR_NULL || context->failed) {
        return;
    }

    if (targetOffset < context->cursor) {
        context->failed = ZR_TRUE;
        return;
    }

    paddingSize = targetOffset - context->cursor;
    if (paddingSize == 0u) {
        return;
    }

    fprintf(context->file,
            "    TZrByte zr_pad_%u[%u];\n",
            (unsigned)context->paddingIndex,
            (unsigned)paddingSize);
    context->paddingIndex++;
    context->cursor = targetOffset;
}

static TZrBool backend_aot_c_type_layout_write_field(SZrState *state,
                                                     const SZrFunction *function,
                                                     TZrUInt32 typeLayoutId,
                                                     SZrString *fieldName,
                                                     const SZrFunctionFrameFieldLayout *fieldLayout,
                                                     TZrPtr userData) {
    SZrAotCTypeLayoutEmitContext *context = (SZrAotCTypeLayoutEmitContext *)userData;
    const TZrChar *fieldType;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(typeLayoutId);
    ZR_UNUSED_PARAMETER(fieldName);

    if (context == ZR_NULL || context->file == ZR_NULL || fieldLayout == ZR_NULL || context->failed) {
        return ZR_FALSE;
    }

    backend_aot_c_type_layout_write_padding(context, fieldLayout->byteOffset);
    if (context->failed) {
        return ZR_FALSE;
    }

    fieldType = backend_aot_c_type_layout_primitive_c_type(fieldLayout);
    if (fieldLayout->isValueSlot && fieldLayout->byteSize == sizeof(SZrTypeValue)) {
        fieldType = "SZrTypeValue";
    }

    if (fieldType != ZR_NULL) {
        fprintf(context->file,
                "    %s zr_field_%u;\n",
                fieldType,
                (unsigned)context->fieldIndex);
    } else {
        fprintf(context->file,
                "    TZrByte zr_field_%u[%u];\n",
                (unsigned)context->fieldIndex,
                (unsigned)fieldLayout->byteSize);
    }

    context->cursor = fieldLayout->byteOffset + fieldLayout->byteSize;
    context->fieldIndex++;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_write_field_assert(SZrState *state,
                                                            const SZrFunction *function,
                                                            TZrUInt32 typeLayoutId,
                                                            SZrString *fieldName,
                                                            const SZrFunctionFrameFieldLayout *fieldLayout,
                                                            TZrPtr userData) {
    SZrAotCTypeLayoutEmitContext *context = (SZrAotCTypeLayoutEmitContext *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(fieldName);

    if (context == ZR_NULL || context->file == ZR_NULL || fieldLayout == ZR_NULL || context->failed) {
        return ZR_FALSE;
    }

    fprintf(context->file,
            "_Static_assert(offsetof(ZrLayout_%u, zr_field_%u) == %u, "
            "\"ZrLayout_%u field_%u offset drift\");\n",
            (unsigned)typeLayoutId,
            (unsigned)context->fieldIndex,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)typeLayoutId,
            (unsigned)context->fieldIndex);
    context->fieldIndex++;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_try_get_gc_offset(const SZrTypeLayout *typeLayout,
                                                           TZrUInt32 gcFieldIndex,
                                                           TZrUInt32 *outOffset) {
    TZrUInt32 matchedIndex = 0u;

    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOffset = 0u;
    if (typeLayout == ZR_NULL || gcFieldIndex >= typeLayout->gcFieldCount) {
        return ZR_FALSE;
    }

    if (typeLayout->gcFieldOffsets != ZR_NULL) {
        *outOffset = typeLayout->gcFieldOffsets[gcFieldIndex];
        return ZR_TRUE;
    }

    if (typeLayout->fields == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 fieldIndex = 0u; fieldIndex < typeLayout->fieldCount; fieldIndex++) {
        const SZrTypeLayoutField *field = &typeLayout->fields[fieldIndex];

        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) == 0u ||
            (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) == 0u ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        if (matchedIndex == gcFieldIndex) {
            *outOffset = field->byteOffset;
            return ZR_TRUE;
        }
        matchedIndex++;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_type_layout_can_emit_gc_descriptor(const SZrTypeLayout *typeLayout) {
    if (typeLayout == ZR_NULL ||
        typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
        typeLayout->gcFieldCount == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < typeLayout->gcFieldCount; index++) {
        TZrUInt32 byteOffset;

        if (!backend_aot_c_type_layout_try_get_gc_offset(typeLayout, index, &byteOffset) ||
            byteOffset > typeLayout->byteSize ||
            sizeof(SZrTypeValue) > typeLayout->byteSize - byteOffset) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void backend_aot_c_type_layout_write_gc_descriptor(FILE *file,
                                                          const SZrTypeLayout *typeLayout,
                                                          TZrUInt32 typeLayoutId) {
    if (file == ZR_NULL || typeLayout == ZR_NULL || typeLayout->gcFieldCount == 0u) {
        return;
    }

    if (!backend_aot_c_type_layout_can_emit_gc_descriptor(typeLayout)) {
        fprintf(file,
                "/* zr_aot_gc_descriptor_offsets_failed layout=%u */\n\n",
                (unsigned)typeLayoutId);
        return;
    }

    fprintf(file,
            "/* zr_aot_gc_descriptor_offsets layout=%u count=%u */\n"
            "static const TZrUInt32 ZrGcOffsets_%u[] = {\n",
            (unsigned)typeLayoutId,
            (unsigned)typeLayout->gcFieldCount,
            (unsigned)typeLayoutId);
    for (TZrUInt32 index = 0u; index < typeLayout->gcFieldCount; index++) {
        TZrUInt32 byteOffset = 0u;

        (void)backend_aot_c_type_layout_try_get_gc_offset(typeLayout, index, &byteOffset);
        fprintf(file, "    %uu,\n", (unsigned)byteOffset);
    }
    fprintf(file,
            "};\n"
            "static const SZrAotGcDescriptor ZrGcDescriptor_%u = {\n"
            "    %uu,\n"
            "    %uu,\n"
            "    ZrGcOffsets_%u,\n"
            "};\n\n",
            (unsigned)typeLayoutId,
            (unsigned)typeLayoutId,
            (unsigned)typeLayout->gcFieldCount,
            (unsigned)typeLayoutId);
}

static TZrBool backend_aot_c_type_layout_try_get_ownership_offset(const SZrTypeLayout *typeLayout,
                                                                  TZrUInt32 ownershipFieldIndex,
                                                                  TZrUInt32 *outOffset) {
    TZrUInt32 matchedIndex = 0u;

    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOffset = 0u;
    if (typeLayout == ZR_NULL || ownershipFieldIndex >= typeLayout->ownershipFieldCount) {
        return ZR_FALSE;
    }

    if (typeLayout->ownershipFieldOffsets != ZR_NULL) {
        *outOffset = typeLayout->ownershipFieldOffsets[ownershipFieldIndex];
        return ZR_TRUE;
    }

    if (typeLayout->fields == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 fieldIndex = 0u; fieldIndex < typeLayout->fieldCount; fieldIndex++) {
        const SZrTypeLayoutField *field = &typeLayout->fields[fieldIndex];

        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) == 0u ||
            (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) == 0u ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        if (matchedIndex == ownershipFieldIndex) {
            *outOffset = field->byteOffset;
            return ZR_TRUE;
        }
        matchedIndex++;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_type_layout_can_emit_ownership_offsets(const SZrTypeLayout *typeLayout) {
    if (typeLayout == ZR_NULL ||
        typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
        typeLayout->ownershipFieldCount == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < typeLayout->ownershipFieldCount; index++) {
        TZrUInt32 byteOffset;

        if (!backend_aot_c_type_layout_try_get_ownership_offset(typeLayout, index, &byteOffset) ||
            byteOffset > typeLayout->byteSize ||
            sizeof(SZrTypeValue) > typeLayout->byteSize - byteOffset) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void backend_aot_c_type_layout_write_ownership_offsets(FILE *file,
                                                              const SZrTypeLayout *typeLayout,
                                                              TZrUInt32 typeLayoutId) {
    if (file == ZR_NULL || typeLayout == ZR_NULL || typeLayout->ownershipFieldCount == 0u) {
        return;
    }

    if (!backend_aot_c_type_layout_can_emit_ownership_offsets(typeLayout)) {
        fprintf(file,
                "/* zr_aot_ownership_offsets_failed layout=%u */\n\n",
                (unsigned)typeLayoutId);
        return;
    }

    fprintf(file,
            "/* zr_aot_ownership_offsets layout=%u count=%u */\n"
            "static const TZrUInt32 ZrOwnershipOffsets_%u[] = {\n",
            (unsigned)typeLayoutId,
            (unsigned)typeLayout->ownershipFieldCount,
            (unsigned)typeLayoutId);
    for (TZrUInt32 index = 0u; index < typeLayout->ownershipFieldCount; index++) {
        TZrUInt32 byteOffset = 0u;

        (void)backend_aot_c_type_layout_try_get_ownership_offset(typeLayout, index, &byteOffset);
        fprintf(file, "    %uu,\n", (unsigned)byteOffset);
    }
    fprintf(file, "};\n\n");
}

static void backend_aot_c_type_layout_write_runtime_field_table(FILE *file,
                                                                const SZrTypeLayout *typeLayout,
                                                                TZrUInt32 typeLayoutId) {
    if (file == ZR_NULL ||
        typeLayout == ZR_NULL ||
        typeLayout->fields == ZR_NULL ||
        typeLayout->fieldCount == 0u) {
        return;
    }

    fprintf(file,
            "static const SZrTypeLayoutField ZrTypeLayoutFields_%u[] = {\n",
            (unsigned)typeLayoutId);
    for (TZrUInt32 fieldIndex = 0u; fieldIndex < typeLayout->fieldCount; fieldIndex++) {
        const SZrTypeLayoutField *field = &typeLayout->fields[fieldIndex];

        fprintf(file,
                "    { %uu, %uu, %uu, %uu, %uu },\n",
                (unsigned)field->byteOffset,
                (unsigned)field->byteSize,
                (unsigned)field->typeLayoutIndex,
                (unsigned)field->flags,
                (unsigned)field->activeTag);
    }
    fprintf(file, "};\n");
}

static void backend_aot_c_type_layout_write_runtime_descriptor(FILE *file,
                                                               const SZrTypeLayout *typeLayout,
                                                               TZrUInt32 typeLayoutId) {
    if (file == ZR_NULL || typeLayout == ZR_NULL) {
        return;
    }

    backend_aot_c_type_layout_write_runtime_field_table(file, typeLayout, typeLayoutId);
    fprintf(file,
            "static const SZrTypeLayout ZrTypeLayout_%u = {\n"
            "    .byteSize = %uu,\n"
            "    .byteAlign = %uu,\n"
            "    .kind = %uu,\n"
            "    .copyKind = %uu,\n"
            "    .dropKind = %uu,\n"
            "    .reserved0 = %uu,\n",
            (unsigned)typeLayoutId,
            (unsigned)typeLayout->byteSize,
            (unsigned)typeLayout->byteAlign,
            (unsigned)typeLayout->kind,
            (unsigned)typeLayout->copyKind,
            (unsigned)typeLayout->dropKind,
            (unsigned)typeLayout->reserved0);
    if (typeLayout->fields != ZR_NULL && typeLayout->fieldCount > 0u) {
        fprintf(file, "    .fields = ZrTypeLayoutFields_%u,\n", (unsigned)typeLayoutId);
    } else {
        fprintf(file, "    .fields = ZR_NULL,\n");
    }
    fprintf(file,
            "    .fieldCount = %uu,\n"
            "    .gcFieldCount = %uu,\n"
            "    .ownershipFieldCount = %uu,\n"
            "    .tagOffset = %uu,\n"
            "    .tagSize = %uu,\n"
            "    .blittable = %s,\n"
            "    .reserved1 = %uu,\n"
            "    .reserved2 = %uu,\n"
            "    .reserved3 = %uu,\n"
            "    .cTypeId = %uu,\n",
            (unsigned)typeLayout->fieldCount,
            (unsigned)typeLayout->gcFieldCount,
            (unsigned)typeLayout->ownershipFieldCount,
            (unsigned)typeLayout->tagOffset,
            (unsigned)typeLayout->tagSize,
            typeLayout->blittable ? "ZR_TRUE" : "ZR_FALSE",
            (unsigned)typeLayout->reserved1,
            (unsigned)typeLayout->reserved2,
            (unsigned)typeLayout->reserved3,
            (unsigned)typeLayoutId);
    if (backend_aot_c_type_layout_can_emit_gc_descriptor(typeLayout)) {
        fprintf(file, "    .gcFieldOffsets = ZrGcOffsets_%u,\n", (unsigned)typeLayoutId);
    } else {
        fprintf(file, "    .gcFieldOffsets = ZR_NULL,\n");
    }
    if (backend_aot_c_type_layout_can_emit_ownership_offsets(typeLayout)) {
        fprintf(file, "    .ownershipFieldOffsets = ZrOwnershipOffsets_%u,\n", (unsigned)typeLayoutId);
    } else {
        fprintf(file, "    .ownershipFieldOffsets = ZR_NULL,\n");
    }
    fprintf(file, "};\n\n");
}

static TZrBool backend_aot_c_type_layout_has_gc_descriptor(SZrState *state,
                                                           const SZrAotFunctionTable *table,
                                                           TZrUInt32 typeLayoutId) {
    if (state == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];
            const SZrTypeLayout *typeLayout;

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId != typeLayoutId) {
                continue;
            }

            typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
            return (TZrBool)(typeLayout != ZR_NULL &&
                             typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
                             backend_aot_c_type_layout_can_emit_gc_descriptor(typeLayout));
        }
    }

    return ZR_FALSE;
}

static unsigned long long backend_aot_c_type_layout_emit_one(FILE *file,
                                                             SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;
    SZrAotCTypeLayoutEmitContext context;
    long typeLayoutStart;
    long typeLayoutEnd;

    if (file == ZR_NULL ||
        state == ZR_NULL ||
        function == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    if (typeLayout == ZR_NULL ||
        (typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
         typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
        return 0u;
    }

    memset(&context, 0, sizeof(context));
    context.file = file;
    typeLayoutStart = ftell(file);

    if (typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT) {
        fprintf(file,
                "typedef ZR_AOT_C_LAYOUT_STRUCT(ZrLayout_%u, %u) {\n",
                (unsigned)typeLayoutId,
                (unsigned)typeLayout->byteAlign);
        if (!ZrCore_Function_VisitPrototypeFrameFieldLayouts(state,
                                                             function,
                                                             typeLayoutId,
                                                             backend_aot_c_type_layout_write_field,
                                                             &context)) {
            context.failed = ZR_TRUE;
        }
        backend_aot_c_type_layout_write_padding(&context, typeLayout->byteSize);
        if (context.fieldIndex == 0u && typeLayout->byteSize > 0u) {
            fprintf(file,
                    "    TZrByte zr_pad_%u[%u];\n",
                    (unsigned)context.paddingIndex,
                    (unsigned)typeLayout->byteSize);
            context.cursor = typeLayout->byteSize;
        }
        fprintf(file, "} ZrLayout_%u;\n", (unsigned)typeLayoutId);

        if (context.failed) {
            fprintf(file,
                    "/* ZrLayout_%u metadata emission failed before static assertions. */\n\n",
                    (unsigned)typeLayoutId);
            return 0u;
        }

        fprintf(file,
                "_Static_assert(sizeof(ZrLayout_%u) == %u, \"ZrLayout_%u size drift\");\n",
                (unsigned)typeLayoutId,
                (unsigned)typeLayout->byteSize,
                (unsigned)typeLayoutId);
        fprintf(file,
                "_Static_assert(_Alignof(ZrLayout_%u) == %u, \"ZrLayout_%u align drift\");\n",
                (unsigned)typeLayoutId,
                (unsigned)typeLayout->byteAlign,
                (unsigned)typeLayoutId);

        memset(&context, 0, sizeof(context));
        context.file = file;
        if (!ZrCore_Function_VisitPrototypeFrameFieldLayouts(state,
                                                             function,
                                                             typeLayoutId,
                                                             backend_aot_c_type_layout_write_field_assert,
                                                             &context)) {
            fprintf(file, "/* ZrLayout_%u field assert emission failed. */\n", (unsigned)typeLayoutId);
        }
        fprintf(file, "\n");
    } else {
        fprintf(file,
                "/* AOT typed union layout %u runtime descriptor. */\n",
                (unsigned)typeLayoutId);
    }

    backend_aot_c_type_layout_write_gc_descriptor(file, typeLayout, typeLayoutId);
    backend_aot_c_type_layout_write_ownership_offsets(file, typeLayout, typeLayoutId);
    backend_aot_c_type_layout_write_runtime_descriptor(file, typeLayout, typeLayoutId);
    typeLayoutEnd = ftell(file);
    if (typeLayoutStart >= 0 && typeLayoutEnd >= typeLayoutStart) {
        unsigned long long typeLayoutBytes = (unsigned long long)(typeLayoutEnd - typeLayoutStart);
        fprintf(file,
                "/* aot_size.typeLayoutBytes[%u] = %llu */\n\n",
                (unsigned)typeLayoutId,
                typeLayoutBytes);
        return typeLayoutBytes;
    }
    return 0u;
}

static unsigned long long backend_aot_c_type_layout_emit_referenced(FILE *file,
                                                                    SZrState *state,
                                                                    const SZrAotFunctionTable *table) {
    unsigned long long typeLayoutBytesTotal = 0u;

    if (file == ZR_NULL || state == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                backend_aot_c_type_layout_seen_before(table, entryIndex, slotIndex, slotLayout->typeLayoutId)) {
                continue;
            }

            typeLayoutBytesTotal += backend_aot_c_type_layout_emit_one(file, state, function, slotLayout->typeLayoutId);
        }
    }

    return typeLayoutBytesTotal;
}

unsigned long long backend_aot_c_type_layout_generated_bytes_referenced(SZrState *state,
                                                                        const SZrAotFunctionTable *table) {
    FILE *scratchFile;
    unsigned long long typeLayoutBytesTotal;

    if (state == ZR_NULL || table == ZR_NULL) {
        return 0u;
    }

    scratchFile = tmpfile();
    if (scratchFile == ZR_NULL) {
        return 0u;
    }

    typeLayoutBytesTotal = backend_aot_c_type_layout_emit_referenced(scratchFile, state, table);
    fclose(scratchFile);
    return typeLayoutBytesTotal;
}

void backend_aot_write_c_type_layout_declarations(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotFunctionTable *table) {
    unsigned long long typeLayoutBytesTotal = 0u;

    if (file == ZR_NULL || state == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file,
            "/* AOT typed value layout declarations. */\n"
            "#if defined(_MSC_VER)\n"
            "#define ZR_AOT_C_LAYOUT_STRUCT(name, bytes) __declspec(align(bytes)) struct name\n"
            "#elif defined(__GNUC__) || defined(__clang__)\n"
            "#define ZR_AOT_C_LAYOUT_STRUCT(name, bytes) struct __attribute__((aligned(bytes))) name\n"
            "#else\n"
            "#define ZR_AOT_C_LAYOUT_STRUCT(name, bytes) struct name\n"
            "#endif\n");
    typeLayoutBytesTotal = backend_aot_c_type_layout_emit_referenced(file, state, table);
    fprintf(file, "/* aot_size.typeLayoutBytesTotal = %llu */\n", typeLayoutBytesTotal);
    fprintf(file, "\n");
}

TZrUInt32 backend_aot_c_type_layout_gc_descriptor_index_space(SZrState *state,
                                                              const SZrAotFunctionTable *table) {
    TZrUInt32 indexSpace = 0u;

    if (state == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                !backend_aot_c_type_layout_has_gc_descriptor(state, table, slotLayout->typeLayoutId)) {
                continue;
            }

            if (slotLayout->typeLayoutId + 1u > indexSpace) {
                indexSpace = slotLayout->typeLayoutId + 1u;
            }
        }
    }

    return indexSpace;
}

void backend_aot_write_c_type_layout_gc_descriptor_table(FILE *file,
                                                         SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         TZrUInt32 descriptorIndexSpace) {
    if (file == ZR_NULL ||
        state == ZR_NULL ||
        table == ZR_NULL ||
        descriptorIndexSpace == 0u) {
        return;
    }

    fprintf(file,
            "/* AOT GC descriptor table indexed by typeLayoutId. */\n"
            "static const SZrAotGcDescriptor *const zr_aot_gc_descriptors[] = {\n");
    for (TZrUInt32 typeLayoutId = 0u; typeLayoutId < descriptorIndexSpace; typeLayoutId++) {
        if (backend_aot_c_type_layout_has_gc_descriptor(state, table, typeLayoutId)) {
            fprintf(file, "    &ZrGcDescriptor_%u,\n", (unsigned)typeLayoutId);
        } else {
            fprintf(file, "    ZR_NULL,\n");
        }
    }
    fprintf(file, "};\n\n");
}

void backend_aot_write_c_type_layout_registration_table(FILE *file,
                                                        SZrState *state,
                                                        const SZrAotFunctionTable *table,
                                                        TZrUInt32 typeLayoutIndexSpace) {
    if (file == ZR_NULL ||
        state == ZR_NULL ||
        table == ZR_NULL ||
        typeLayoutIndexSpace == 0u) {
        return;
    }

    fprintf(file,
            "/* AOT type layout table indexed by cTypeId/typeLayoutId. */\n"
            "static const SZrTypeLayout *const zr_aot_type_layouts[] = {\n");
    for (TZrUInt32 typeLayoutId = 0u; typeLayoutId < typeLayoutIndexSpace; typeLayoutId++) {
        if (backend_aot_c_type_layout_resolve_from_table(state, table, typeLayoutId) != ZR_NULL) {
            fprintf(file, "    &ZrTypeLayout_%u,\n", (unsigned)typeLayoutId);
        } else {
            fprintf(file, "    ZR_NULL,\n");
        }
    }
    fprintf(file, "};\n\n");
}
