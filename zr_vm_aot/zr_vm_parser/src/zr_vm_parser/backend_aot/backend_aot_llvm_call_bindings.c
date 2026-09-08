#include "backend_aot_llvm_call_bindings.h"

void backend_aot_llvm_write_bound_method_infos(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 indexSpace = backend_aot_function_table_index_space(table);
    fprintf(file, "%%SZrAotMethodInfo = type { i32, ptr, i32, ptr, ptr, ptr, ptr, i8, i8, i8, i8 }\n");
    /* LLVM uses the runtime's ordinary value frame and GC roots. Only the
     * process-local method mapping is needed here; no compact C frame layout
     * or reflective signature is advertised. */
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        fprintf(file, "@zr_aot_method_info_%u = private constant %%SZrAotMethodInfo {\n"
                "  i32 %u, ptr null, i32 0, ptr null, ptr null, ptr null,\n"
                "  ptr @zr_aot_reflection_invoke_unsupported, i8 0, i8 %u, i8 0, i8 0\n}\n",
                (unsigned)table->entries[index].flatIndex,
                (unsigned)table->entries[index].flatIndex,
                (unsigned)ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING);
    }
    fprintf(file, "@zr_aot_method_infos = private constant [%u x ptr] [\n", (unsigned)indexSpace);
    for (TZrUInt32 index = 0u, entryIndex = 0u; index < indexSpace; ++index) {
        if (index != 0u) fputs(",\n", file);
        if (entryIndex < table->count && table->entries[entryIndex].flatIndex == index) {
            fprintf(file, "  ptr @zr_aot_method_info_%u", (unsigned)index);
            ++entryIndex;
        } else {
            fputs("  ptr null", file);
        }
    }
    fputs("\n]\n\n", file);
    fprintf(file, "@zr_aot_method_tokens = private constant [%u x i32] zeroinitializer\n\n",
            (unsigned)indexSpace);
}

TZrBool backend_aot_llvm_write_call_bindings(
        FILE *file, SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 count) {
    TZrUInt32 writtenCount = 0u;
    if (file == ZR_NULL || table == ZR_NULL) return ZR_FALSE;
    if (count == 0u) return ZR_TRUE;
    fprintf(file, "@zr_aot_call_binding_rows = private constant [%u x i8] [\n",
            (unsigned)(count * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE));
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            TZrByte bytes[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
            if (entry->function->callSiteCaches[cacheIndex].binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            if (!backend_aot_c_project_call_binding(state, table, entry, cacheIndex,
                    &row, &targetIndex, ZR_NULL) ||
                ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes), ZR_NULL) != ZR_ARTIFACT_STATUS_OK) return ZR_FALSE;
            for (TZrUInt32 byteIndex = 0u; byteIndex < sizeof(bytes); ++byteIndex) {
                if (writtenCount != 0u || byteIndex != 0u) fputs(", ", file);
                fprintf(file, "i8 %u", (unsigned)bytes[byteIndex]);
            }
            ++writtenCount;
        }
    }
    fprintf(file, "\n]\n@zr_aot_call_binding_target_function_indices = private constant [%u x i32] [\n",
            (unsigned)count);
    writtenCount = 0u;
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            if (entry->function->callSiteCaches[cacheIndex].binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            if (!backend_aot_c_project_call_binding(state, table, entry, cacheIndex,
                    &row, &targetIndex, ZR_NULL)) return ZR_FALSE;
            if (writtenCount++ != 0u) fputs(", ", file);
            fprintf(file, "i32 %u", (unsigned)targetIndex);
        }
    }
    fputs("\n]\n\n", file);
    return (TZrBool)(writtenCount == count && !ferror(file));
}

void backend_aot_llvm_write_call_binding_registration(FILE *file, TZrUInt32 count) {
    fprintf(file, "  %s, i32 %u, i32 %u, %s\n",
            count != 0u ? "ptr @zr_aot_call_binding_rows" : "ptr null",
            (unsigned)count, (unsigned)ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
            count != 0u ? "ptr @zr_aot_call_binding_target_function_indices" : "ptr null");
}
