#include "zr_vm_parser/artifact_projection.h"

#include <string.h>

#include "zr_vm_core/function.h"

static EZrArtifactStatus projection_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt32 cacheIndex) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
        diagnostic->rowIndex = cacheIndex;
    }
    return status;
}

static void projection_copy_row(
        SZrArtifactCallBindingRow *row, const SZrFunctionCallSiteCacheEntry *cache,
        TZrUInt32 functionIndex, TZrUInt32 cacheIndex) {
    memset(row, 0, sizeof(*row));
    row->schemaVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    row->functionIndex = functionIndex;
    row->cacheIndex = cacheIndex;
    row->instructionIndex = cache->instructionIndex;
    row->contract = cache->binding.contract;
    row->location = cache->bindingLocation;
}

EZrArtifactStatus ZrParser_ArtifactCallBinding_BuildRows(
        const SZrFunction *function,
        TZrUInt32 functionIndex,
        SZrArtifactCallBindingRow *rows,
        TZrUInt32 rowCapacity,
        TZrUInt32 *outRowCount,
        SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 count = 0u;
    TZrByte encoded[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (outRowCount != ZR_NULL) *outRowCount = 0u;
    if (function == ZR_NULL || outRowCount == ZR_NULL ||
        functionIndex == ZR_CALL_BINDING_SLOT_NONE ||
        (rows == ZR_NULL && rowCapacity != 0u) ||
        (function->callSiteCacheLength != 0u && function->callSiteCaches == ZR_NULL)) {
        return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    }
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *cache = &function->callSiteCaches[index];
        SZrArtifactCallBindingRow row;
        EZrArtifactStatus status;
        if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (cache->instructionIndex >= function->instructionsLength ||
            (cache->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT &&
             cache->bindingLocation.ownerDepth == 0u &&
             cache->bindingLocation.targetIndex >= function->constantValueLength)) {
            return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, index);
        }
        projection_copy_row(&row, cache, functionIndex, index);
        status = ZrCore_Artifact_WriteCallBindingRow(&row, encoded, sizeof(encoded), diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return projection_fail(diagnostic, status, index);
        ++count;
    }
    if (rows != ZR_NULL && count > rowCapacity) {
        return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
    }
    *outRowCount = count;
    if (rows == ZR_NULL) return ZR_ARTIFACT_STATUS_OK;
    count = 0u;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *cache = &function->callSiteCaches[index];
        if (cache->binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
            projection_copy_row(&rows[count++], cache, functionIndex, index);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}
