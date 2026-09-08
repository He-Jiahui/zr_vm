#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

static EZrArtifactStatus artifact_binding_validate_row(
        const SZrArtifactCallBindingRow *row,
        TZrUInt32 rowIndex,
        TZrUInt32 byteOffset,
        SZrArtifactDiagnostic *diagnostic) {
    EZrCallBindingStatus bindingStatus;
    if (row->schemaVersion != ZR_CALL_BINDING_SCHEMA_VERSION) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
            diagnostic->actualVersion = row->schemaVersion;
        }
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, rowIndex, byteOffset);
    }
    bindingStatus = ZrCore_CallBinding_CheckContract(&row->contract, ZR_NULL);
    if (bindingStatus != ZR_CALL_BINDING_OK) {
        return zr_artifact_fail(diagnostic,
                bindingStatus == ZR_CALL_BINDING_INVALID_TOKEN
                        ? ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN : ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, rowIndex, byteOffset + 16u);
    }
    if (row->functionIndex == ZR_CALL_BINDING_SLOT_NONE ||
        row->cacheIndex == ZR_CALL_BINDING_SLOT_NONE ||
        row->instructionIndex == ZR_CALL_BINDING_SLOT_NONE ||
        ((row->contract.bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION &&
          (row->location.kind < ZR_CALL_BINDING_RELOCATION_CONSTANT ||
           row->location.kind > ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
           row->location.targetIndex == ZR_CALL_BINDING_SLOT_NONE)) ||
         (row->contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
          (row->location.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
           row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE || row->location.ownerDepth != 0u))) ||
        row->location.ownerDepth == ZR_CALL_BINDING_SLOT_NONE ||
        row->location.flags != 0u) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, rowIndex, byteOffset);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_WriteCallBindingRow(
        const SZrArtifactCallBindingRow *row,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        SZrArtifactDiagnostic *diagnostic) {
    EZrArtifactStatus status;
    zr_artifact_diagnostic_clear(diagnostic);
    if (row == ZR_NULL || buffer == ZR_NULL) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, 0u, 0u);
    }
    if (bufferCapacity < ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, 0u, 0u);
    }
    status = artifact_binding_validate_row(row, 0u, 0u, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    zr_artifact_write_u32(buffer + 0u, row->schemaVersion);
    zr_artifact_write_u32(buffer + 4u, row->functionIndex);
    zr_artifact_write_u32(buffer + 8u, row->cacheIndex);
    zr_artifact_write_u32(buffer + 12u, row->instructionIndex);
    ZrCore_CallBinding_EncodeContract(&row->contract, buffer + 16u,
            ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE);
    zr_artifact_write_u32(buffer + 80u, row->location.kind);
    zr_artifact_write_u32(buffer + 84u, row->location.targetIndex);
    zr_artifact_write_u32(buffer + 88u, row->location.ownerDepth);
    zr_artifact_write_u32(buffer + 92u, row->location.flags);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadCallBindingRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactCallBindingRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    TZrUInt64 rowOffset = (TZrUInt64)rowIndex * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
    SZrArtifactCallBindingRow row;
    EZrArtifactStatus status;
    zr_artifact_diagnostic_clear(diagnostic);
    if (outRow != ZR_NULL) memset(outRow, 0, sizeof(*outRow));
    if (section == ZR_NULL || outRow == ZR_NULL || section->data == ZR_NULL ||
        section->kind != ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE ||
        section->elementSize != ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE ||
        rowIndex >= section->elementCount) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, rowIndex, 0u);
    }
    if (rowOffset + ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE > section->byteLength) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_TRUNCATED,
                section->kind, rowIndex, section->byteOffset);
    }
    bytes = section->data + (TZrSize)rowOffset;
    memset(&row, 0, sizeof(row));
    row.schemaVersion = zr_artifact_read_u32(bytes + 0u);
    row.functionIndex = zr_artifact_read_u32(bytes + 4u);
    row.cacheIndex = zr_artifact_read_u32(bytes + 8u);
    row.instructionIndex = zr_artifact_read_u32(bytes + 12u);
    if (row.schemaVersion != ZR_CALL_BINDING_SCHEMA_VERSION) {
        return artifact_binding_validate_row(&row, rowIndex,
                section->byteOffset + (TZrUInt32)rowOffset, diagnostic);
    }
    if (!ZrCore_CallBinding_DecodeContract(bytes + 16u,
            ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE, &row.contract)) {
        EZrCallBindingStatus bindingStatus = ZrCore_CallBinding_CheckContract(&row.contract, ZR_NULL);
        return zr_artifact_fail(diagnostic,
                bindingStatus == ZR_CALL_BINDING_INVALID_TOKEN
                        ? ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN : ZR_ARTIFACT_STATUS_INVALID_SECTION,
                section->kind, rowIndex, section->byteOffset + (TZrUInt32)rowOffset + 16u);
    }
    row.location.kind = zr_artifact_read_u32(bytes + 80u);
    row.location.targetIndex = zr_artifact_read_u32(bytes + 84u);
    row.location.ownerDepth = zr_artifact_read_u32(bytes + 88u);
    row.location.flags = zr_artifact_read_u32(bytes + 92u);
    status = artifact_binding_validate_row(&row, rowIndex,
            section->byteOffset + (TZrUInt32)rowOffset, diagnostic);
    if (status == ZR_ARTIFACT_STATUS_OK) *outRow = row;
    return status;
}

static TZrBool artifact_binding_row_follows(
        const SZrArtifactCallBindingRow *previous, const SZrArtifactCallBindingRow *row) {
    return (TZrBool)(previous->functionIndex < row->functionIndex ||
            (previous->functionIndex == row->functionIndex && previous->cacheIndex < row->cacheIndex));
}

EZrArtifactStatus zr_artifact_call_binding_validate_input(
        const SZrArtifactSectionInput *section,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactCallBindingRow *rows = (const SZrArtifactCallBindingRow *)section->data;
    for (TZrUInt32 index = 0u; index < section->elementCount; ++index) {
        EZrArtifactStatus status = artifact_binding_validate_row(&rows[index], index, 0u, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        if (index != 0u && !artifact_binding_row_follows(&rows[index - 1u], &rows[index])) {
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                    section->kind, index, 0u);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus zr_artifact_call_binding_validate_decoded(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSectionView section;
    SZrArtifactCallBindingRow previous;
    if (ZrCore_Artifact_FindSection(view, ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE,
            &section, ZR_NULL) != ZR_ARTIFACT_STATUS_OK) return ZR_ARTIFACT_STATUS_OK;
    memset(&previous, 0, sizeof(previous));
    for (TZrUInt32 index = 0u; index < section.elementCount; ++index) {
        SZrArtifactCallBindingRow row;
        EZrArtifactStatus status = ZrCore_Artifact_ReadCallBindingRow(&section, index, &row, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        if (index != 0u && !artifact_binding_row_follows(&previous, &row)) {
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                    section.kind, index, section.byteOffset + index * section.elementSize);
        }
        previous = row;
    }
    return ZR_ARTIFACT_STATUS_OK;
}
