#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

/* 公共行约束供单行 API 和完整 artifact 校验共用，防止写入与读取接受不同契约。 */
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

/* AOT 投影与通用 artifact writer 只写固定宽度的契约和重定位坐标。 */
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

/* metadata loader 和 artifact reader 共用此入口，失败时不向调用者发布部分行。 */
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
        /* BUG: DecodeContract 失败会清零 row.contract；再次检查只会得到 MISSING_CONTRACT。
         * 将合法行的 signatureToken 字节替换成 targetMetadataToken 后，读取端返回
         * INVALID_SECTION 而非写入端给出的 ILLEGAL_TOKEN。后续需保留原始解码字段来分类。 */
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

/* section 中按函数和缓存下标严格递增，重复调用点不得覆盖前一行。 */
static TZrBool artifact_binding_row_follows(
        const SZrArtifactCallBindingRow *previous, const SZrArtifactCallBindingRow *row) {
    return (TZrBool)(previous->functionIndex < row->functionIndex ||
            (previous->functionIndex == row->functionIndex && previous->cacheIndex < row->cacheIndex));
}

/* 通用 artifact writer 在编码前检查所有绑定行及顺序。 */
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

/* 通用 artifact reader 在目录和字节边界通过后复核逐行内容。 */
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
