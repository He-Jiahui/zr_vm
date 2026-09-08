#include "backend_aot_c_call_bindings.h"

#include <string.h>

#include "backend_aot_internal.h"
#include "zr_vm_parser/artifact_projection.h"

static TZrBool call_binding_projection_fail(
        SZrArtifactDiagnostic *diagnostic, TZrUInt32 cacheIndex) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_ARTIFACT_STATUS_INVALID_SECTION;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
        diagnostic->rowIndex = cacheIndex;
    }
    return ZR_FALSE;
}

TZrBool backend_aot_c_project_call_binding(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotFunctionEntry *entry,
        TZrUInt32 cacheIndex,
        SZrArtifactCallBindingRow *outRow,
        TZrUInt32 *outTargetFunctionIndex,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrFunctionCallSiteCacheEntry *cache;
    const SZrFunction *owner;
    SZrArtifactCallBindingRow row;
    TZrByte bytes[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    TZrUInt32 targetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    if (outRow != ZR_NULL) memset(outRow, 0, sizeof(*outRow));
    if (outTargetFunctionIndex != ZR_NULL) *outTargetFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    if (state == ZR_NULL || table == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL ||
        outRow == ZR_NULL || outTargetFunctionIndex == ZR_NULL ||
        entry->function->callSiteCaches == ZR_NULL || cacheIndex >= entry->function->callSiteCacheLength) {
        return call_binding_projection_fail(diagnostic, cacheIndex);
    }
    cache = &entry->function->callSiteCaches[cacheIndex];
    if (cache->instructionIndex >= entry->function->instructionsLength) {
        return call_binding_projection_fail(diagnostic, cacheIndex);
    }
    memset(&row, 0, sizeof(row));
    row.schemaVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    row.functionIndex = entry->flatIndex;
    row.cacheIndex = cacheIndex;
    row.instructionIndex = cache->instructionIndex;
    row.contract = cache->binding.contract;
    row.location = cache->bindingLocation;
    if (ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes), diagnostic) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    if (row.location.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT) {
        owner = entry->function;
        for (TZrUInt32 depth = 0u; depth < row.location.ownerDepth && owner != ZR_NULL; ++depth) {
            owner = owner->ownerFunction;
        }
        if (row.location.targetIndex > 0x7fffffffu ||
            !backend_aot_resolve_callable_constant_function_index(table, state, owner,
                    (TZrInt32)row.location.targetIndex, &targetIndex)) {
            return call_binding_projection_fail(diagnostic, cacheIndex);
        }
    } else if (row.location.kind == ZR_CALL_BINDING_RELOCATION_AOT) {
        for (TZrUInt32 index = 0u; index < table->count; ++index) {
            if (table->entries[index].flatIndex == row.location.targetIndex) {
                targetIndex = row.location.targetIndex;
                break;
            }
        }
        if (targetIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            return call_binding_projection_fail(diagnostic, cacheIndex);
        }
    }
    if (targetIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
        const SZrFunction *target = ZR_NULL;
        TZrUInt64 signatureHash;
        for (TZrUInt32 index = 0u; index < table->count; ++index) {
            if (table->entries[index].flatIndex == targetIndex) {
                target = table->entries[index].function;
                break;
            }
        }
        signatureHash = ZrCore_CallBinding_FunctionSignatureHash(target);
        if (signatureHash != row.contract.signatureHash) {
            call_binding_projection_fail(diagnostic, cacheIndex);
            if (diagnostic != ZR_NULL) {
                diagnostic->status = ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH;
                diagnostic->expectedHash = row.contract.signatureHash;
                diagnostic->actualHash = signatureHash;
            }
            return ZR_FALSE;
        }
    }
    *outRow = row;
    *outTargetFunctionIndex = targetIndex;
    return ZR_TRUE;
}

TZrBool backend_aot_c_validate_call_bindings(
        SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 *outCount) {
    TZrUInt32 count = 0u;
    if (outCount != ZR_NULL) *outCount = 0u;
    if (table == ZR_NULL || outCount == ZR_NULL || (table->count != 0u && table->entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrUInt32 functionRowCount;
        if (index != 0u && table->entries[index - 1u].flatIndex >= entry->flatIndex) return ZR_FALSE;
        if (ZrParser_ArtifactCallBinding_BuildRows(entry->function, entry->flatIndex,
                ZR_NULL, 0u, &functionRowCount, ZR_NULL) != ZR_ARTIFACT_STATUS_OK ||
            functionRowCount > ZR_ARTIFACT_MAX_ROW_COUNT - count) return ZR_FALSE;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            if (entry->function->callSiteCaches[cacheIndex].binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            if (!backend_aot_c_project_call_binding(state, table, entry, cacheIndex,
                    &row, &targetIndex, ZR_NULL)) return ZR_FALSE;
        }
        count += functionRowCount;
    }
    *outCount = count;
    return ZR_TRUE;
}

TZrBool backend_aot_c_write_call_bindings(
        FILE *file, SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 count) {
    TZrUInt32 writtenCount = 0u;
    if (file == ZR_NULL || table == ZR_NULL) return ZR_FALSE;
    if (count == 0u) return ZR_TRUE;
    fprintf(file, "static const TZrByte zr_aot_call_binding_rows[] = {\n");
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
                if (byteIndex % 16u == 0u) fprintf(file, "    ");
                fprintf(file, "0x%02xu,%s", (unsigned)bytes[byteIndex], byteIndex % 16u == 15u ? "\n" : " ");
            }
            ++writtenCount;
        }
    }
    fprintf(file, "};\n\nstatic const TZrUInt32 zr_aot_call_binding_target_function_indices[] = {\n");
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            if (entry->function->callSiteCaches[cacheIndex].binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            if (!backend_aot_c_project_call_binding(state, table, entry, cacheIndex,
                    &row, &targetIndex, ZR_NULL)) return ZR_FALSE;
            fprintf(file, "    %uu,\n", (unsigned)targetIndex);
        }
    }
    fprintf(file, "};\n\n");
    return (TZrBool)(writtenCount == count && !ferror(file));
}

void backend_aot_c_write_call_binding_registration(FILE *file, TZrUInt32 count) {
    fprintf(file, "    .callBindingRows = %s,\n", count != 0u ? "zr_aot_call_binding_rows" : "ZR_NULL");
    fprintf(file, "    .callBindingRowCount = %uu,\n", (unsigned)count);
    fprintf(file, "    .callBindingRowSize = %uu,\n", (unsigned)ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE);
    fprintf(file, "    .callBindingTargetFunctionIndices = %s,\n",
            count != 0u ? "zr_aot_call_binding_target_function_indices" : "ZR_NULL");
}
