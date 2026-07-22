#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

static void artifact_write_type_def_row(TZrByte *bytes, const SZrArtifactTypeDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->flags);
    zr_artifact_write_u32(bytes + 8u, row->canonicalTypeId);
    zr_artifact_write_u32(bytes + 12u, row->constructorToken);
    zr_artifact_write_u32(bytes + 16u, row->constructorSignatureToken);
    zr_artifact_write_u32(bytes + 20u, 0u);
    zr_artifact_write_u64(bytes + 24u, row->typeSignatureHash);
    zr_artifact_write_u64(bytes + 32u, row->constructorContractHash);
    zr_artifact_write_u64(bytes + 40u, 0u);
}

static void artifact_write_type_identity_row(TZrByte *bytes, const SZrArtifactTypeIdentityRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->signatureToken);
    zr_artifact_write_u32(bytes + 8u, row->canonicalTypeId);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->signatureOffset);
    zr_artifact_write_u32(bytes + 20u, row->signatureLength);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u32(bytes + 32u, row->layoutVersion);
    zr_artifact_write_u32(bytes + 36u, 0u);
    zr_artifact_write_u64(bytes + 40u, row->layoutHash);
}

static void artifact_write_member_row(TZrByte *bytes, const SZrArtifactMemberDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->ownerTypeToken);
    zr_artifact_write_u32(bytes + 8u, row->signatureToken);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->nameStringOffset);
    zr_artifact_write_u32(bytes + 20u, 0u);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
}

static void artifact_write_property_row(TZrByte *bytes, const SZrArtifactPropertyDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->ownerTypeToken);
    zr_artifact_write_u32(bytes + 8u, row->getterToken);
    zr_artifact_write_u32(bytes + 12u, row->setterToken);
    zr_artifact_write_u32(bytes + 16u, row->signatureToken);
    zr_artifact_write_u32(bytes + 20u, row->flags);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
    zr_artifact_write_u64(bytes + 40u, 0u);
}

static void artifact_write_contract_row(TZrByte *bytes, const SZrArtifactContractRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->memberToken);
    zr_artifact_write_u32(bytes + 4u, row->signatureToken);
    zr_artifact_write_u32(bytes + 8u, row->parameterCount);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->receiverEffect);
    zr_artifact_write_u32(bytes + 20u, row->refExportEffect);
    zr_artifact_write_u32(bytes + 24u, row->escapeFlags);
    zr_artifact_write_u32(bytes + 28u, (TZrUInt32)row->abiLoweringKind);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
}

static void artifact_write_layout_row(TZrByte *bytes, const SZrArtifactLayoutRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->typeToken);
    zr_artifact_write_u32(bytes + 4u, row->version);
    zr_artifact_write_u32(bytes + 8u, row->byteSize);
    zr_artifact_write_u32(bytes + 12u, row->byteAlignment);
    zr_artifact_write_u32(bytes + 16u, row->gcScanKind);
    zr_artifact_write_u32(bytes + 20u, row->capabilityFlags);
    zr_artifact_write_u32(bytes + 24u, row->ownershipMapOffset);
    zr_artifact_write_u32(bytes + 28u, row->ownershipMapLength);
    zr_artifact_write_u64(bytes + 32u, row->layoutHash);
    zr_artifact_write_u64(bytes + 40u, row->stableSlotContractHash);
}

static void artifact_write_domain_transfer_row(
        TZrByte *bytes,
        const SZrArtifactDomainTransferRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->typeToken);
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)row->kind);
    zr_artifact_write_u32(bytes + 8u, row->schemaVersion);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->schemaOffset);
    zr_artifact_write_u32(bytes + 20u, row->schemaLength);
    zr_artifact_write_u32(bytes + 24u, row->providerToken);
    zr_artifact_write_u32(bytes + 28u, 0u);
    zr_artifact_write_u64(bytes + 32u, row->schemaHash);
    zr_artifact_write_u64(bytes + 40u, row->providerContractHash);
}

static void artifact_write_relocation_row(TZrByte *bytes, const SZrArtifactRelocationRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->codeOffset);
    zr_artifact_write_u32(bytes + 4u, row->kind);
    zr_artifact_write_u32(bytes + 8u, row->targetToken);
    zr_artifact_write_u32(bytes + 12u, row->targetSignatureToken);
    zr_artifact_write_u64(bytes + 16u, row->expectedSignatureHash);
    zr_artifact_write_u64(bytes + 24u, row->expectedContractHash);
    zr_artifact_write_u64(bytes + 32u, row->expectedModuleHash);
}

void zr_artifact_write_section_payload(TZrByte *bytes, const SZrArtifactSectionInput *section) {
    TZrUInt32 index;
    TZrUInt32 elementSize = zr_artifact_section_element_size(section->kind);

    switch (section->kind) {
        case ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_type_def_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactTypeDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_TYPE_REF_TABLE:
        case ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_type_identity_row(bytes + (TZrSize)index * elementSize,
                                                 &((const SZrArtifactTypeIdentityRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_member_row(bytes + (TZrSize)index * elementSize,
                                          &((const SZrArtifactMemberDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_property_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactPropertyDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_CONTRACT_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_contract_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactContractRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_LAYOUT_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_layout_row(bytes + (TZrSize)index * elementSize,
                                          &((const SZrArtifactLayoutRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_domain_transfer_row(
                        bytes + (TZrSize)index * elementSize,
                        &((const SZrArtifactDomainTransferRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_relocation_row(bytes + (TZrSize)index * elementSize,
                                              &((const SZrArtifactRelocationRow *)section->data)[index]);
            break;
        default:
            if (section->elementCount > 0u) memcpy(bytes, section->data, section->elementCount);
            break;
    }
}

static EZrArtifactStatus artifact_get_row_bytes(const SZrArtifactSectionView *section,
                                                TZrUInt32 rowIndex,
                                                TZrUInt32 expectedSize,
                                                const TZrByte **outBytes,
                                                SZrArtifactDiagnostic *diagnostic) {
    if (section == ZR_NULL || outBytes == ZR_NULL ||
        section->elementSize != expectedSize || rowIndex >= section->elementCount) {
        return zr_artifact_fail(diagnostic,
                                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                                section != ZR_NULL ? section->kind : 0u,
                                rowIndex,
                                0u);
    }
    *outBytes = section->data + (TZrSize)rowIndex * expectedSize;
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadTypeDefRow(const SZrArtifactSectionView *section,
                                                 TZrUInt32 rowIndex,
                                                 SZrArtifactTypeDefRow *outRow,
                                                 SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->flags = zr_artifact_read_u32(bytes + 4u);
    outRow->canonicalTypeId = zr_artifact_read_u32(bytes + 8u);
    outRow->constructorToken = zr_artifact_read_u32(bytes + 12u);
    outRow->constructorSignatureToken = zr_artifact_read_u32(bytes + 16u);
    outRow->typeSignatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->constructorContractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadTypeIdentityRow(const SZrArtifactSectionView *section,
                                                      TZrUInt32 rowIndex,
                                                      SZrArtifactTypeIdentityRow *outRow,
                                                      SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 4u);
    outRow->canonicalTypeId = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->signatureOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->signatureLength = zr_artifact_read_u32(bytes + 20u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->layoutVersion = zr_artifact_read_u32(bytes + 32u);
    outRow->layoutHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadMemberDefRow(const SZrArtifactSectionView *section,
                                                   TZrUInt32 rowIndex,
                                                   SZrArtifactMemberDefRow *outRow,
                                                   SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->ownerTypeToken = zr_artifact_read_u32(bytes + 4u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->nameStringOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadPropertyDefRow(const SZrArtifactSectionView *section,
                                                     TZrUInt32 rowIndex,
                                                     SZrArtifactPropertyDefRow *outRow,
                                                     SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->ownerTypeToken = zr_artifact_read_u32(bytes + 4u);
    outRow->getterToken = zr_artifact_read_u32(bytes + 8u);
    outRow->setterToken = zr_artifact_read_u32(bytes + 12u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 16u);
    outRow->flags = zr_artifact_read_u32(bytes + 20u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadContractRow(const SZrArtifactSectionView *section,
                                                  TZrUInt32 rowIndex,
                                                  SZrArtifactContractRow *outRow,
                                                  SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->memberToken = zr_artifact_read_u32(bytes + 0u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 4u);
    outRow->parameterCount = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->receiverEffect = zr_artifact_read_u32(bytes + 16u);
    outRow->refExportEffect = zr_artifact_read_u32(bytes + 20u);
    outRow->escapeFlags = zr_artifact_read_u32(bytes + 24u);
    outRow->abiLoweringKind = (EZrArtifactAbiLoweringKind)zr_artifact_read_u32(bytes + 28u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadLayoutRow(const SZrArtifactSectionView *section,
                                                TZrUInt32 rowIndex,
                                                SZrArtifactLayoutRow *outRow,
                                                SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->typeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->version = zr_artifact_read_u32(bytes + 4u);
    outRow->byteSize = zr_artifact_read_u32(bytes + 8u);
    outRow->byteAlignment = zr_artifact_read_u32(bytes + 12u);
    outRow->gcScanKind = zr_artifact_read_u32(bytes + 16u);
    outRow->capabilityFlags = zr_artifact_read_u32(bytes + 20u);
    outRow->ownershipMapOffset = zr_artifact_read_u32(bytes + 24u);
    outRow->ownershipMapLength = zr_artifact_read_u32(bytes + 28u);
    outRow->layoutHash = zr_artifact_read_u64(bytes + 32u);
    outRow->stableSlotContractHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadDomainTransferRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactDomainTransferRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;

    if (outRow == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                0u,
                rowIndex,
                0u);
    }
    status = artifact_get_row_bytes(
            section,
            rowIndex,
            ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE,
            &bytes,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    memset(outRow, 0, sizeof(*outRow));
    outRow->typeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->kind = (EZrArtifactDomainTransferKind)zr_artifact_read_u32(bytes + 4u);
    outRow->schemaVersion = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->schemaOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->schemaLength = zr_artifact_read_u32(bytes + 20u);
    outRow->providerToken = zr_artifact_read_u32(bytes + 24u);
    outRow->schemaHash = zr_artifact_read_u64(bytes + 32u);
    outRow->providerContractHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadRelocationRow(const SZrArtifactSectionView *section,
                                                    TZrUInt32 rowIndex,
                                                    SZrArtifactRelocationRow *outRow,
                                                    SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->codeOffset = zr_artifact_read_u32(bytes + 0u);
    outRow->kind = zr_artifact_read_u32(bytes + 4u);
    outRow->targetToken = zr_artifact_read_u32(bytes + 8u);
    outRow->targetSignatureToken = zr_artifact_read_u32(bytes + 12u);
    outRow->expectedSignatureHash = zr_artifact_read_u64(bytes + 16u);
    outRow->expectedContractHash = zr_artifact_read_u64(bytes + 24u);
    outRow->expectedModuleHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}
