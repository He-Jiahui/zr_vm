#include "backend_aot_c_type_layouts.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
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

static void backend_aot_c_type_layout_emit_one(FILE *file,
                                               SZrState *state,
                                               const SZrFunction *function,
                                               TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;
    SZrAotCTypeLayoutEmitContext context;

    if (file == ZR_NULL ||
        state == ZR_NULL ||
        function == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    if (typeLayout == ZR_NULL || typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT) {
        return;
    }

    memset(&context, 0, sizeof(context));
    context.file = file;

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
        fprintf(file, "    TZrByte zr_pad_%u[%u];\n", (unsigned)context.paddingIndex, (unsigned)typeLayout->byteSize);
        context.cursor = typeLayout->byteSize;
    }
    fprintf(file, "} ZrLayout_%u;\n", (unsigned)typeLayoutId);

    if (context.failed) {
        fprintf(file, "/* ZrLayout_%u metadata emission failed before static assertions. */\n\n", (unsigned)typeLayoutId);
        return;
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
}

void backend_aot_write_c_type_layout_declarations(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotFunctionTable *table) {
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

            backend_aot_c_type_layout_emit_one(file, state, function, slotLayout->typeLayoutId);
        }
    }
    fprintf(file, "\n");
}
