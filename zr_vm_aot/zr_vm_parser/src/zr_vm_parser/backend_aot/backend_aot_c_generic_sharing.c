#include "backend_aot_c_generic_sharing.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <ctype.h>
#include <string.h>

typedef struct SZrAotCGenericSharingCandidate {
    const SZrFunction *function;
    TZrUInt32 entryIndex;
    TZrUInt32 bindingIndex;
    const TZrChar *typeName;
    TZrUInt32 typeLayoutId;
    TZrChar symbolBase[96];
} SZrAotCGenericSharingCandidate;

static const TZrChar *backend_aot_c_generic_sharing_type_name_text(
        const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(typeRef->typeName);
}

static const SZrFunctionFrameSlotLayout *backend_aot_c_generic_sharing_find_slot_layout(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        if (function->frameSlotLayouts[index].stackSlot == stackSlot) {
            return &function->frameSlotLayouts[index];
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_c_generic_sharing_type_name_is_closed(const TZrChar *typeName) {
    return (TZrBool)(typeName != ZR_NULL &&
                     strchr(typeName, '<') != ZR_NULL &&
                     strchr(typeName, '>') != ZR_NULL);
}

static TZrBool backend_aot_c_generic_sharing_text_equals(const TZrChar *text,
                                                         TZrSize length,
                                                         const TZrChar *needle) {
    return (TZrBool)(needle != ZR_NULL &&
                     strlen(needle) == length &&
                     strncmp(text, needle, length) == 0);
}

static TZrBool backend_aot_c_generic_sharing_arg_is_value_or_const(const TZrChar *text,
                                                                   TZrSize length) {
    while (length > 0u && isspace((unsigned char)*text)) {
        text++;
        length--;
    }
    while (length > 0u && isspace((unsigned char)text[length - 1u])) {
        length--;
    }

    if (length == 0u) {
        return ZR_FALSE;
    }

    if (isdigit((unsigned char)text[0]) ||
        ((text[0] == '-' || text[0] == '+') && length > 1u && isdigit((unsigned char)text[1]))) {
        return ZR_TRUE;
    }

    return (TZrBool)(backend_aot_c_generic_sharing_text_equals(text, length, "int") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "uint") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "float") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "double") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "bool") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "char") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "byte") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "true") ||
                     backend_aot_c_generic_sharing_text_equals(text, length, "false"));
}

static TZrBool backend_aot_c_generic_sharing_all_args_reference_like(const TZrChar *typeName) {
    const TZrChar *open;
    const TZrChar *cursor;
    const TZrChar *argStart;
    int depth = 0;

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    open = strchr(typeName, '<');
    if (open == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = open + 1;
    argStart = cursor;
    for (; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            depth++;
        } else if (*cursor == '>') {
            if (depth == 0) {
                return (TZrBool)!backend_aot_c_generic_sharing_arg_is_value_or_const(
                        argStart,
                        (TZrSize)(cursor - argStart));
            }
            depth--;
        } else if (*cursor == ',' && depth == 0) {
            if (backend_aot_c_generic_sharing_arg_is_value_or_const(argStart,
                                                                    (TZrSize)(cursor - argStart))) {
                return ZR_FALSE;
            }
            argStart = cursor + 1;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_c_generic_sharing_sanitize_base_name(const TZrChar *typeName,
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

static TZrBool backend_aot_c_generic_sharing_candidate_from_binding(
        const SZrAotFunctionTable *table,
        TZrUInt32 entryIndex,
        TZrUInt32 bindingIndex,
        SZrAotCGenericSharingCandidate *outCandidate) {
    const SZrFunction *function;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionFrameSlotLayout *slotLayout;
    const TZrChar *typeName;

    if (outCandidate != ZR_NULL) {
        memset(outCandidate, 0, sizeof(*outCandidate));
    }

    if (table == ZR_NULL ||
        table->entries == ZR_NULL ||
        entryIndex >= table->count ||
        outCandidate == ZR_NULL) {
        return ZR_FALSE;
    }

    function = table->entries[entryIndex].function;
    if (function == ZR_NULL ||
        function->typedLocalBindings == ZR_NULL ||
        bindingIndex >= function->typedLocalBindingLength) {
        return ZR_FALSE;
    }

    binding = &function->typedLocalBindings[bindingIndex];
    typeName = backend_aot_c_generic_sharing_type_name_text(&binding->type);
    slotLayout = backend_aot_c_generic_sharing_find_slot_layout(function, binding->stackSlot);

    if (!backend_aot_c_generic_sharing_type_name_is_closed(typeName) ||
        binding->type.baseType != ZR_VALUE_TYPE_OBJECT ||
        !backend_aot_c_generic_sharing_all_args_reference_like(typeName) ||
        (slotLayout != ZR_NULL &&
         slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT)) {
        return ZR_FALSE;
    }

    outCandidate->function = function;
    outCandidate->entryIndex = entryIndex;
    outCandidate->bindingIndex = bindingIndex;
    outCandidate->typeName = typeName;
    outCandidate->typeLayoutId =
            slotLayout != ZR_NULL ? slotLayout->typeLayoutId : ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    backend_aot_c_generic_sharing_sanitize_base_name(typeName,
                                                     outCandidate->symbolBase,
                                                     sizeof(outCandidate->symbolBase));
    return ZR_TRUE;
}

static TZrBool backend_aot_c_generic_sharing_type_seen_before(const SZrAotFunctionTable *table,
                                                              TZrUInt32 entryIndex,
                                                              TZrUInt32 bindingIndex,
                                                              const TZrChar *typeName) {
    if (table == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 previousEntryIndex = 0u; previousEntryIndex <= entryIndex; previousEntryIndex++) {
        const SZrFunction *function = table->entries[previousEntryIndex].function;
        TZrUInt32 bindingLimit;

        if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
            continue;
        }

        bindingLimit = previousEntryIndex == entryIndex ? bindingIndex : function->typedLocalBindingLength;
        for (TZrUInt32 previousBindingIndex = 0u; previousBindingIndex < bindingLimit; previousBindingIndex++) {
            SZrAotCGenericSharingCandidate previous;
            if (backend_aot_c_generic_sharing_candidate_from_binding(table,
                                                                     previousEntryIndex,
                                                                     previousBindingIndex,
                                                                     &previous) &&
                strcmp(previous.typeName, typeName) == 0) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_generic_sharing_base_seen_before(const SZrAotFunctionTable *table,
                                                              TZrUInt32 entryIndex,
                                                              TZrUInt32 bindingIndex,
                                                              const TZrChar *symbolBase) {
    if (table == ZR_NULL || symbolBase == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 previousEntryIndex = 0u; previousEntryIndex <= entryIndex; previousEntryIndex++) {
        const SZrFunction *function = table->entries[previousEntryIndex].function;
        TZrUInt32 bindingLimit;

        if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
            continue;
        }

        bindingLimit = previousEntryIndex == entryIndex ? bindingIndex : function->typedLocalBindingLength;
        for (TZrUInt32 previousBindingIndex = 0u; previousBindingIndex < bindingLimit; previousBindingIndex++) {
            SZrAotCGenericSharingCandidate previous;
            if (backend_aot_c_generic_sharing_candidate_from_binding(table,
                                                                     previousEntryIndex,
                                                                     previousBindingIndex,
                                                                     &previous) &&
                strcmp(previous.symbolBase, symbolBase) == 0) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 backend_aot_c_generic_sharing_base_symbol_id(
        const SZrAotFunctionTable *table,
        const SZrAotCGenericSharingCandidate *candidate) {
    TZrUInt32 nextBaseId = 1u;

    if (table == ZR_NULL ||
        table->entries == ZR_NULL ||
        candidate == ZR_NULL ||
        candidate->symbolBase[0] == '\0') {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
            SZrAotCGenericSharingCandidate current;

            if (!backend_aot_c_generic_sharing_candidate_from_binding(table, entryIndex, bindingIndex, &current) ||
                backend_aot_c_generic_sharing_base_seen_before(table, entryIndex, bindingIndex, current.symbolBase)) {
                continue;
            }

            if (strcmp(current.symbolBase, candidate->symbolBase) == 0) {
                return nextBaseId;
            }
            nextBaseId++;
        }
    }

    return 0u;
}

static void backend_aot_c_generic_sharing_format_symbol_base(
        const SZrAotFunctionTable *table,
        const SZrAotCGenericSharingCandidate *candidate,
        TZrBool stripGeneratedSymbols,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrUInt32 baseId;

    if (buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }

    if (!stripGeneratedSymbols) {
        snprintf(buffer,
                 bufferSize,
                 "%s",
                 candidate != ZR_NULL && candidate->symbolBase[0] != '\0' ? candidate->symbolBase : "generic");
        return;
    }

    baseId = backend_aot_c_generic_sharing_base_symbol_id(table, candidate);
    if (baseId == 0u) {
        baseId = 1u;
    }
    snprintf(buffer, bufferSize, "g%u", (unsigned)baseId);
}

static void backend_aot_c_generic_sharing_format_debug_name(TZrUInt32 dictionaryId,
                                                            const TZrChar *typeName,
                                                            TZrBool stripGeneratedSymbols,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }

    if (stripGeneratedSymbols) {
        snprintf(buffer, bufferSize, "generic#%u", (unsigned)dictionaryId);
        return;
    }

    snprintf(buffer, bufferSize, "%s", typeName != ZR_NULL ? typeName : "generic");
}

void backend_aot_write_c_generic_dictionary_macros(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "#define ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, slotIndex) \\\n"
            "    ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, (dict), (metadataRuntime), (slotIndex))\n"
            "#define ZrAot_GenericSlot_SizeOf(metadataRuntime, dict, slotIndex, outSize) \\\n"
            "    ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(state, (dict), (metadataRuntime), (slotIndex), (outSize))\n"
            "#define ZrAot_GenericSlot_Method(dict, slotIndex) \\\n"
            "    ZrLibrary_AotRuntime_GenericSlot_Method(state, (dict), ZR_NULL, (slotIndex))\n");
}

void backend_aot_write_c_generic_sharing_entries(FILE *file,
                                                 const SZrAotFunctionTable *table,
                                                 TZrBool stripGeneratedSymbols) {
    TZrUInt32 nextDictionaryId = 1u;
    TZrBool wroteHeader = ZR_FALSE;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
            SZrAotCGenericSharingCandidate candidate;
            TZrChar symbolBase[96];
            TZrChar debugName[64];

            if (!backend_aot_c_generic_sharing_candidate_from_binding(table,
                                                                      entryIndex,
                                                                      bindingIndex,
                                                                      &candidate) ||
                backend_aot_c_generic_sharing_type_seen_before(table,
                                                               entryIndex,
                                                               bindingIndex,
                                                               candidate.typeName)) {
                continue;
            }

            if (!wroteHeader) {
                fprintf(file, "/* zr_aot_generic_dictionary_table */\n");
                wroteHeader = ZR_TRUE;
            }

            backend_aot_c_generic_sharing_format_symbol_base(table,
                                                             &candidate,
                                                             stripGeneratedSymbols,
                                                             symbolBase,
                                                             sizeof(symbolBase));
            backend_aot_c_generic_sharing_format_debug_name(nextDictionaryId,
                                                            candidate.typeName,
                                                            stripGeneratedSymbols,
                                                            debugName,
                                                            sizeof(debugName));
            if (stripGeneratedSymbols) {
                fprintf(file,
                        "/* instance=%u typeId=%u share=shared dictionary=zr_aot_generic_dict_%u target=zr_fn_%s__shared */\n",
                        (unsigned)nextDictionaryId,
                        (unsigned)nextDictionaryId,
                        (unsigned)nextDictionaryId,
                        symbolBase);
            } else {
                fprintf(file,
                        "/* instance=%u type=%s share=shared dictionary=zr_aot_generic_dict_%u target=zr_fn_%s__shared */\n",
                        (unsigned)nextDictionaryId,
                        candidate.typeName,
                        (unsigned)nextDictionaryId,
                        symbolBase);
            }
            fprintf(file,
                    "static TZrInt64 zr_aot_generic_dict_%u_method_0(struct SZrState *state);\n",
                    (unsigned)nextDictionaryId);
            fprintf(file, "static const SZrAotGenericSlot zr_aot_generic_dict_%u_slots[] = {\n",
                    (unsigned)nextDictionaryId);
            fprintf(file,
                    "    { .kind = ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT, .typeLayoutId = ");
            if (candidate.typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
                fprintf(file, "ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE");
            } else {
                fprintf(file, "%uu", (unsigned)candidate.typeLayoutId);
            }
            fprintf(file,
                    ", .metadataToken = 0u, .methodIndex = 0u, .flags = 0u, .debugName = \"%s\", .staticTypeLayout = ZR_NULL, .staticMethod = ZR_NULL },\n"
                    "    { .kind = ZR_AOT_GENERIC_SLOT_METHOD, .typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, .metadataToken = 0u, .methodIndex = 0u, .flags = 0u, .debugName = \"%s\", .staticTypeLayout = ZR_NULL, .staticMethod = zr_aot_generic_dict_%u_method_0 },\n"
                    "};\n",
                    debugName,
                    debugName,
                    (unsigned)nextDictionaryId);
            fprintf(file,
                    "static SZrAotGenericResolvedSlot zr_aot_generic_dict_%u_cache[2];\n"
                    "static SZrAotGenericDictionary zr_aot_generic_dict_%u = {\n"
                    "    .slotCount = 2u,\n"
                    "    .slots = zr_aot_generic_dict_%u_slots,\n"
                    "    .resolvedSlots = zr_aot_generic_dict_%u_cache,\n"
                    "};\n\n",
                    (unsigned)nextDictionaryId,
                    (unsigned)nextDictionaryId,
                    (unsigned)nextDictionaryId,
                    (unsigned)nextDictionaryId);
            fprintf(file,
                    "static TZrInt64 zr_aot_generic_dict_%u_method_0(struct SZrState *state) {\n"
                    "    /* zr_aot_generic_call_typed_shared_method_target */\n"
                    "    (void)state;\n"
                    "    return 0;\n"
                    "}\n\n",
                    (unsigned)nextDictionaryId);
            nextDictionaryId++;
        }
    }

    if (!wroteHeader) {
        return;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
            SZrAotCGenericSharingCandidate candidate;
            TZrChar symbolBase[96];

            if (!backend_aot_c_generic_sharing_candidate_from_binding(table,
                                                                      entryIndex,
                                                                      bindingIndex,
                                                                      &candidate) ||
                backend_aot_c_generic_sharing_base_seen_before(table,
                                                               entryIndex,
                                                               bindingIndex,
                                                               candidate.symbolBase)) {
                continue;
            }

            backend_aot_c_generic_sharing_format_symbol_base(table,
                                                             &candidate,
                                                             stripGeneratedSymbols,
                                                             symbolBase,
                                                             sizeof(symbolBase));
            fprintf(file,
                    "static TZrInt64 zr_fn_%s__shared(struct SZrState *state, SZrMetadataRuntime *metadataRuntime, const SZrAotGenericDictionary *dict) {\n"
                    "    const SZrTypeLayout *zr_aot_generic_layout_0 = ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, 0u);\n"
                    "    FZrAotEntryThunk zr_aot_generic_method_1 = ZrAot_GenericSlot_Method(dict, 1u);\n"
                    "    /* zr_aot_generic_call_typed_shared_method_slot */\n"
                    "    if (zr_aot_generic_method_1 != ZR_NULL) {\n"
                    "        (void)zr_aot_generic_method_1(state);\n"
                    "    }\n"
                    "    (void)state;\n"
                    "    (void)metadataRuntime;\n"
                    "    (void)dict;\n"
                    "    (void)zr_aot_generic_layout_0;\n"
                    "    return 0;\n"
                    "}\n\n",
                    symbolBase);
        }
    }
}

TZrUInt32 backend_aot_c_generic_sharing_dictionary_id_for_function(const SZrAotFunctionTable *table,
                                                                   const SZrFunction *function) {
    TZrUInt32 nextDictionaryId = 1u;

    if (table == ZR_NULL || table->entries == ZR_NULL || function == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *entryFunction = table->entries[entryIndex].function;

        if (entryFunction == ZR_NULL || entryFunction->typedLocalBindings == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 bindingIndex = 0u; bindingIndex < entryFunction->typedLocalBindingLength; bindingIndex++) {
            SZrAotCGenericSharingCandidate candidate;

            if (!backend_aot_c_generic_sharing_candidate_from_binding(table,
                                                                      entryIndex,
                                                                      bindingIndex,
                                                                      &candidate) ||
                backend_aot_c_generic_sharing_type_seen_before(table,
                                                               entryIndex,
                                                               bindingIndex,
                                                               candidate.typeName)) {
                continue;
            }

            if (entryFunction == function) {
                return nextDictionaryId;
            }
            nextDictionaryId++;
        }
    }

    return 0u;
}
