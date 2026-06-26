#include "backend_aot_c_generic_monomorphization.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include <string.h>

static const SZrFunctionTypedTypeRef *backend_aot_c_generic_find_typed_local_binding(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return &binding->type;
        }
    }

    return ZR_NULL;
}

static const TZrChar *backend_aot_c_generic_type_name_text(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(typeRef->typeName);
}

static TZrBool backend_aot_c_generic_type_ref_is_closed_instance(
        const SZrFunctionTypedTypeRef *typeRef) {
    const TZrChar *typeName = backend_aot_c_generic_type_name_text(typeRef);

    return (TZrBool)(typeName != ZR_NULL &&
                     strchr(typeName, '<') != ZR_NULL &&
                     strchr(typeName, '>') != ZR_NULL);
}

static TZrBool backend_aot_c_generic_slot_is_monomorphized_value(
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *slotLayout) {
    const SZrFunctionTypedTypeRef *typeRef;

    if (function == ZR_NULL ||
        slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeRef = backend_aot_c_generic_find_typed_local_binding(function, slotLayout->stackSlot);
    return backend_aot_c_generic_type_ref_is_closed_instance(typeRef);
}

static TZrBool backend_aot_c_generic_slot_matches_type_name(
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *slotLayout,
        const TZrChar *typeName) {
    const SZrFunctionTypedTypeRef *typeRef;
    const TZrChar *candidateTypeName;

    if (typeName == ZR_NULL ||
        !backend_aot_c_generic_slot_is_monomorphized_value(function, slotLayout)) {
        return ZR_FALSE;
    }

    typeRef = backend_aot_c_generic_find_typed_local_binding(function, slotLayout->stackSlot);
    candidateTypeName = backend_aot_c_generic_type_name_text(typeRef);
    return (TZrBool)(candidateTypeName != ZR_NULL && strcmp(candidateTypeName, typeName) == 0);
}

static TZrBool backend_aot_c_generic_seen_before(const SZrAotFunctionTable *table,
                                                 TZrUInt32 entryIndex,
                                                 TZrUInt32 slotIndex,
                                                 const TZrChar *typeName) {
    if (table == ZR_NULL || typeName == ZR_NULL) {
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
            if (backend_aot_c_generic_slot_matches_type_name(function,
                                                             &function->frameSlotLayouts[previousSlotIndex],
                                                             typeName)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void backend_aot_c_generic_sanitize_base_name(const TZrChar *typeName,
                                                     TZrChar *buffer,
                                                     TZrSize bufferSize) {
    TZrSize writeIndex = 0u;

    if (buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }

    if (typeName == ZR_NULL) {
        typeName = "generic";
    }

    for (TZrSize readIndex = 0u; typeName[readIndex] != '\0' && typeName[readIndex] != '<'; readIndex++) {
        TZrChar ch = typeName[readIndex];

        if (writeIndex + 1u >= bufferSize) {
            break;
        }

        if (ch >= 'A' && ch <= 'Z') {
            buffer[writeIndex++] = (TZrChar)(ch - 'A' + 'a');
        } else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            buffer[writeIndex++] = ch;
        } else if (writeIndex > 0u && buffer[writeIndex - 1u] != '_') {
            buffer[writeIndex++] = '_';
        }
    }

    while (writeIndex > 0u && buffer[writeIndex - 1u] == '_') {
        writeIndex--;
    }
    if (writeIndex == 0u && bufferSize > 1u) {
        buffer[writeIndex++] = 'g';
    }
    buffer[writeIndex] = '\0';
}

static void backend_aot_c_generic_format_symbol_base(const TZrChar *typeName,
                                                     TZrUInt32 instanceId,
                                                     TZrBool stripGeneratedSymbols,
                                                     TZrChar *buffer,
                                                     TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }

    if (stripGeneratedSymbols) {
        snprintf(buffer, bufferSize, "g%u", (unsigned)instanceId);
        return;
    }

    backend_aot_c_generic_sanitize_base_name(typeName, buffer, bufferSize);
}

static TZrBool backend_aot_c_generic_layout_has_native_struct(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    return (TZrBool)(typeLayout != ZR_NULL && typeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT);
}

static void backend_aot_c_generic_write_fallback_layout(FILE *file,
                                                        const SZrFunctionFrameSlotLayout *slotLayout) {
    TZrUInt32 byteAlign;

    if (file == ZR_NULL ||
        slotLayout == ZR_NULL ||
        slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        slotLayout->byteSize == 0u) {
        return;
    }

    byteAlign = slotLayout->byteAlign != 0u ? slotLayout->byteAlign : (TZrUInt32)sizeof(TZrPtr);

    fprintf(file,
            "typedef ZR_AOT_C_LAYOUT_STRUCT(ZrLayout_%u, %u) {\n"
            "    TZrByte zr_payload[%u];\n"
            "} ZrLayout_%u;\n",
            (unsigned)slotLayout->typeLayoutId,
            (unsigned)byteAlign,
            (unsigned)slotLayout->byteSize,
            (unsigned)slotLayout->typeLayoutId);
    fprintf(file,
            "_Static_assert(sizeof(ZrLayout_%u) == %u, \"ZrLayout_%u size drift\");\n",
            (unsigned)slotLayout->typeLayoutId,
            (unsigned)slotLayout->byteSize,
            (unsigned)slotLayout->typeLayoutId);
    fprintf(file,
            "_Static_assert(_Alignof(ZrLayout_%u) == %u, \"ZrLayout_%u align drift\");\n\n",
            (unsigned)slotLayout->typeLayoutId,
            (unsigned)byteAlign,
            (unsigned)slotLayout->typeLayoutId);
}

void backend_aot_write_c_generic_monomorphization_layouts(FILE *file,
                                                          SZrState *state,
                                                          const SZrAotFunctionTable *table) {
    if (file == ZR_NULL || state == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];
            const SZrFunctionTypedTypeRef *typeRef =
                    backend_aot_c_generic_find_typed_local_binding(function, slotLayout->stackSlot);
            const TZrChar *typeName = backend_aot_c_generic_type_name_text(typeRef);

            if (!backend_aot_c_generic_slot_is_monomorphized_value(function, slotLayout) ||
                backend_aot_c_generic_seen_before(table, entryIndex, slotIndex, typeName) ||
                backend_aot_c_generic_layout_has_native_struct(state, function, slotLayout->typeLayoutId)) {
                continue;
            }

            backend_aot_c_generic_write_fallback_layout(file, slotLayout);
        }
    }
}

void backend_aot_write_c_generic_monomorphization_entries(FILE *file,
                                                          const SZrAotFunctionTable *table,
                                                          TZrBool stripGeneratedSymbols) {
    TZrUInt32 nextInstanceId = 1u;
    TZrBool wroteHeader = ZR_FALSE;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];
            const SZrFunctionTypedTypeRef *typeRef =
                    backend_aot_c_generic_find_typed_local_binding(function, slotLayout->stackSlot);
            const TZrChar *typeName = backend_aot_c_generic_type_name_text(typeRef);
            TZrChar symbolBase[96];

            if (!backend_aot_c_generic_slot_is_monomorphized_value(function, slotLayout) ||
                backend_aot_c_generic_seen_before(table, entryIndex, slotIndex, typeName)) {
                continue;
            }

            if (!wroteHeader) {
                fprintf(file, "/* zr_aot_generic_monomorphization_table */\n");
                wroteHeader = ZR_TRUE;
            }

            backend_aot_c_generic_format_symbol_base(typeName,
                                                      nextInstanceId,
                                                      stripGeneratedSymbols,
                                                      symbolBase,
                                                      sizeof(symbolBase));
            if (stripGeneratedSymbols) {
                fprintf(file,
                        "/* instance=%u typeId=%u share=monomorphized layout=ZrLayout_%u target=zr_fn_%s__%u */\n",
                        (unsigned)nextInstanceId,
                        (unsigned)nextInstanceId,
                        (unsigned)slotLayout->typeLayoutId,
                        symbolBase,
                        (unsigned)nextInstanceId);
            } else {
                fprintf(file,
                        "/* instance=%u type=%s share=monomorphized layout=ZrLayout_%u target=zr_aot_fn_%u */\n",
                        (unsigned)nextInstanceId,
                        typeName,
                        (unsigned)slotLayout->typeLayoutId,
                        (unsigned)table->entries[entryIndex].flatIndex);
            }
            fprintf(file,
                    "static TZrInt64 zr_fn_%s__%u(struct SZrState *state) {\n"
                    "    /* zr_aot_generic_call_typed_monomorphized_direct */\n"
                    "    return zr_aot_fn_%u(state);\n"
                    "}\n\n",
                    symbolBase,
                    (unsigned)nextInstanceId,
                    (unsigned)table->entries[entryIndex].flatIndex);
            nextInstanceId++;
        }
    }

    if (wroteHeader) {
        fprintf(file, "\n");
    }
}
