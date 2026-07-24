#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

#define ZR_ARTIFACT_MAGIC_0 ((TZrByte)'Z')
#define ZR_ARTIFACT_MAGIC_1 ((TZrByte)'R')
#define ZR_ARTIFACT_MAGIC_2 ((TZrByte)'A')
#define ZR_ARTIFACT_MAGIC_3 ((TZrByte)'F')

#define ZR_ARTIFACT_HEADER_VERSION_OFFSET 4u
#define ZR_ARTIFACT_HEADER_SIZE_OFFSET 6u
#define ZR_ARTIFACT_HEADER_KIND_OFFSET 8u
#define ZR_ARTIFACT_HEADER_FLAGS_OFFSET 12u
#define ZR_ARTIFACT_HEADER_DIRECTORY_SIZE_OFFSET 20u
#define ZR_ARTIFACT_HEADER_TOTAL_SIZE_OFFSET 24u
#define ZR_ARTIFACT_HEADER_IDENTITY_OFFSET 32u

static TZrBool artifact_token_is(TZrMetadataToken token, TZrUInt32 table) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == table &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool artifact_type_token_is_valid(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_RID(token) != 0u &&
                     (table == ZR_METADATA_TABLE_TYPE_DEF ||
                      table == ZR_METADATA_TABLE_TYPE_REF ||
                      table == ZR_METADATA_TABLE_TYPE_SPEC));
}

static TZrBool artifact_metadata_token_is_valid(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_RID(token) != 0u &&
                     table >= ZR_METADATA_TABLE_MODULE && table <= ZR_METADATA_TABLE_SIGNATURE);
}

static TZrBool artifact_member_row_is_valid(const SZrArtifactMemberDefRow *row) {
    return (TZrBool)(artifact_token_is(row->token, ZR_METADATA_TABLE_MEMBER_DEF) &&
                     artifact_type_token_is_valid(row->ownerTypeToken) &&
                     artifact_token_is(row->signatureToken, ZR_METADATA_TABLE_SIGNATURE) &&
                     row->signatureHash != 0u && row->contractHash != 0u);
}

static TZrBool artifact_property_row_is_valid(const SZrArtifactPropertyDefRow *row) {
    return (TZrBool)(artifact_token_is(row->token, ZR_METADATA_TABLE_MEMBER_DEF) &&
                     artifact_type_token_is_valid(row->ownerTypeToken) &&
                     (row->getterToken == 0u || artifact_token_is(row->getterToken, ZR_METADATA_TABLE_MEMBER_DEF)) &&
                     (row->setterToken == 0u || artifact_token_is(row->setterToken, ZR_METADATA_TABLE_MEMBER_DEF)) &&
                     (row->initializerToken == 0u ||
                      artifact_token_is(row->initializerToken, ZR_METADATA_TABLE_MEMBER_DEF)) &&
                     (row->getterToken != 0u || row->setterToken != 0u ||
                      row->initializerToken != 0u) &&
                     artifact_token_is(row->signatureToken, ZR_METADATA_TABLE_SIGNATURE) &&
                     (row->flags & ~ZR_ARTIFACT_PROPERTY_FLAG_KNOWN_MASK) == 0u &&
                     row->signatureHash != 0u && row->contractHash != 0u);
}

static TZrBool artifact_relocation_row_is_valid(const SZrArtifactRelocationRow *row) {
    return (TZrBool)(artifact_metadata_token_is_valid(row->targetToken) &&
                     artifact_token_is(row->targetSignatureToken, ZR_METADATA_TABLE_SIGNATURE) &&
                     row->expectedSignatureHash != 0u && row->expectedContractHash != 0u &&
                     row->expectedModuleHash != 0u);
}

static TZrBool artifact_domain_transfer_row_is_valid(
        const SZrArtifactDomainTransferRow *row,
        TZrBool hasSchemaHeap,
        TZrUInt32 schemaHeapLength) {
    TZrUInt64 schemaEnd;

    if (row == ZR_NULL ||
        !artifact_type_token_is_valid(row->typeToken) ||
        row->kind > ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE ||
        (row->flags & ~ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }
    schemaEnd = (TZrUInt64)row->schemaOffset + row->schemaLength;
    if (row->schemaLength == 0u && row->schemaOffset != 0u) {
        return ZR_FALSE;
    }
    if (row->schemaLength > 0u &&
        (!hasSchemaHeap || schemaEnd > schemaHeapLength)) {
        return ZR_FALSE;
    }
    switch (row->kind) {
        case ZR_ARTIFACT_DOMAIN_TRANSFER_FORBIDDEN:
            return (TZrBool)(
                    row->schemaVersion == 0u && row->schemaOffset == 0u &&
                    row->schemaLength == 0u && row->schemaHash == 0u &&
                    row->providerToken == 0u &&
                    row->providerContractHash == 0u && row->flags == 0u);
        case ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY:
            return (TZrBool)(
                    row->schemaVersion != 0u && row->schemaHash != 0u &&
                    row->providerToken == 0u &&
                    row->providerContractHash == 0u);
        case ZR_ARTIFACT_DOMAIN_TRANSFER_STRUCTURED_CLONE:
            return (TZrBool)(
                    row->schemaVersion != 0u && row->schemaLength != 0u &&
                    row->schemaHash != 0u && row->providerToken == 0u &&
                    row->providerContractHash == 0u);
        case ZR_ARTIFACT_DOMAIN_TRANSFER_IMMUTABLE_HANDLE:
            return (TZrBool)(
                    row->schemaVersion != 0u && row->schemaHash != 0u &&
                    artifact_metadata_token_is_valid(row->providerToken) &&
                    row->providerContractHash != 0u);
        case ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE:
            return (TZrBool)(
                    row->schemaVersion != 0u && row->schemaHash != 0u &&
                    artifact_metadata_token_is_valid(row->providerToken) &&
                    row->providerContractHash != 0u &&
                    (row->flags &
                     ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE) != 0u);
        default:
            return ZR_FALSE;
    }
}

static EZrArtifactStatus artifact_validate_identity(EZrArtifactKind kind,
                                                    const SZrArtifactPublicIdentity *identity,
                                                    SZrArtifactDiagnostic *diagnostic) {
    if (kind == ZR_ARTIFACT_KIND_ZRS) {
        if (identity->canonicalTypeId != 0u || identity->typeRefToken != 0u ||
            identity->typeSpecToken != 0u || identity->signatureToken != 0u ||
            identity->typeRefHash != 0u || identity->typeSpecHash != 0u ||
            identity->signatureHash != 0u || identity->layoutVersion != 0u ||
            identity->layoutHash != 0u || identity->callableContractHash != 0u ||
            identity->moduleHash != 0u) {
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u, 0u);
        }
        return ZR_ARTIFACT_STATUS_OK;
    }

    if (identity->canonicalTypeId == 0u ||
        !artifact_token_is(identity->typeRefToken, ZR_METADATA_TABLE_TYPE_REF) ||
        !artifact_token_is(identity->typeSpecToken, ZR_METADATA_TABLE_TYPE_SPEC) ||
        !artifact_token_is(identity->signatureToken, ZR_METADATA_TABLE_SIGNATURE)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, 0u, 0u);
    }
    if (identity->typeRefHash == 0u || identity->typeSpecHash == 0u ||
        identity->signatureHash == 0u || identity->layoutVersion == 0u ||
        identity->layoutHash == 0u || identity->callableContractHash == 0u ||
        identity->moduleHash == 0u) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u, 0u);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static const SZrArtifactSectionInput *artifact_find_input(const SZrArtifactDocument *document,
                                                          EZrArtifactSectionKind kind) {
    TZrUInt32 index;
    for (index = 0u; index < document->sectionCount; ++index) {
        if (document->sections[index].kind == kind) {
            return &document->sections[index];
        }
    }
    return ZR_NULL;
}

static EZrArtifactStatus artifact_validate_signature_slice(const SZrArtifactSectionInput *heap,
                                                           const SZrArtifactTypeIdentityRow *row,
                                                           TZrUInt32 sectionKind,
                                                           TZrUInt32 rowIndex,
                                                           SZrArtifactDiagnostic *diagnostic) {
    TZrUInt64 end;
    EZrArtifactStatus status;

    if (heap == ZR_NULL) {
        return zr_artifact_fail(diagnostic,
                                ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                sectionKind,
                                rowIndex,
                                row->signatureOffset);
    }
    end = (TZrUInt64)row->signatureOffset + (TZrUInt64)row->signatureLength;
    if (row->signatureLength == 0u || end > heap->elementCount) {
        return zr_artifact_fail(diagnostic,
                                ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                sectionKind,
                                rowIndex,
                                row->signatureOffset);
    }
    status = ZrCore_Artifact_ValidateSignature(
            (const TZrByte *)heap->data + row->signatureOffset,
            row->signatureLength,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK && diagnostic != ZR_NULL) {
        diagnostic->sectionKind = sectionKind;
        diagnostic->rowIndex = rowIndex;
        diagnostic->byteOffset += row->signatureOffset;
    }
    return status;
}

static EZrArtifactStatus artifact_validate_type_def_input(const SZrArtifactSectionInput *section,
                                                          SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactTypeDefRow *rows = (const SZrArtifactTypeDefRow *)section->data;
    TZrUInt32 index;
    for (index = 0u; index < section->elementCount; ++index) {
        const SZrArtifactTypeDefRow *row = &rows[index];
        if (!artifact_token_is(row->token, ZR_METADATA_TABLE_TYPE_DEF) ||
            row->canonicalTypeId == 0u ||
            (row->flags & ~ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK) != 0u ||
            row->typeSignatureHash == 0u) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind,
                                    index,
                                    0u);
        }
        if ((row->flags & ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE) != 0u &&
            (!artifact_token_is(row->constructorToken, ZR_METADATA_TABLE_MEMBER_DEF) ||
             !artifact_token_is(row->constructorSignatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
             row->constructorContractHash == 0u)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind,
                                    index,
                                    0u);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_type_identity_input(const SZrArtifactSectionInput *section,
                                                               const SZrArtifactSectionInput *signatureHeap,
                                                               SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactTypeIdentityRow *rows = (const SZrArtifactTypeIdentityRow *)section->data;
    TZrUInt32 expectedTable = section->kind == ZR_ARTIFACT_SECTION_TYPE_REF_TABLE
                                     ? ZR_METADATA_TABLE_TYPE_REF
                                     : ZR_METADATA_TABLE_TYPE_SPEC;
    TZrUInt32 index;

    for (index = 0u; index < section->elementCount; ++index) {
        const SZrArtifactTypeIdentityRow *row = &rows[index];
        if (!artifact_token_is(row->token, expectedTable) ||
            !artifact_token_is(row->signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
            row->canonicalTypeId == 0u || row->signatureHash == 0u ||
            row->layoutVersion == 0u || row->layoutHash == 0u) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind,
                                    index,
                                    0u);
        }
        {
            EZrArtifactStatus status = artifact_validate_signature_slice(
                    signatureHeap, row, section->kind, index, diagnostic);
            if (status != ZR_ARTIFACT_STATUS_OK) {
                return status;
            }
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_contract_input(const SZrArtifactSectionInput *section,
                                                          SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactContractRow *rows = (const SZrArtifactContractRow *)section->data;
    TZrUInt32 index;
    for (index = 0u; index < section->elementCount; ++index) {
        const SZrArtifactContractRow *row = &rows[index];
        if (!artifact_token_is(row->memberToken, ZR_METADATA_TABLE_MEMBER_DEF) ||
            !artifact_token_is(row->signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
            row->parameterCount > ZR_ARTIFACT_MAX_ROW_COUNT ||
            (row->flags & ~ZR_ARTIFACT_CONTRACT_FLAG_KNOWN_MASK) != 0u ||
            row->receiverEffect > ZR_ARTIFACT_RECEIVER_MUTABLE ||
            row->refExportEffect > ZR_ARTIFACT_REF_EXPORT_WRITABLE ||
            (row->escapeFlags & ~ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_KNOWN_MASK) != 0u ||
            row->abiLoweringKind > ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT ||
            row->contractHash == 0u) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind,
                                    index,
                                    0u);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_layout_input(const SZrArtifactSectionInput *section,
                                                        SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactLayoutRow *rows = (const SZrArtifactLayoutRow *)section->data;
    TZrUInt32 index;
    for (index = 0u; index < section->elementCount; ++index) {
        const SZrArtifactLayoutRow *row = &rows[index];
        if (!artifact_type_token_is_valid(row->typeToken) || row->version == 0u ||
            row->byteAlignment == 0u || row->gcScanKind > ZR_ARTIFACT_GC_SCAN_BARRIERED ||
            (row->capabilityFlags & ~ZR_ARTIFACT_LAYOUT_CAPABILITY_KNOWN_MASK) != 0u ||
            row->layoutHash == 0u ||
            ((row->capabilityFlags & ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE) != 0u &&
             row->stableSlotContractHash == 0u)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind,
                                    index,
                                    0u);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_domain_transfer_input(
        const SZrArtifactSectionInput *section,
        const SZrArtifactSectionInput *schemaHeap,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactDomainTransferRow *rows =
            (const SZrArtifactDomainTransferRow *)section->data;
    TZrUInt32 index;

    for (index = 0u; index < section->elementCount; index++) {
        if (index > 0u && rows[index - 1u].typeToken >= rows[index].typeToken) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    section->kind,
                    index,
                    0u);
        }
        if (!artifact_domain_transfer_row_is_valid(
                    &rows[index],
                    schemaHeap != ZR_NULL,
                    schemaHeap != ZR_NULL ? schemaHeap->elementCount : 0u)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    section->kind,
                    index,
                    rows[index].schemaOffset);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_link_input(const SZrArtifactSectionInput *section,
                                                      const SZrArtifactSectionInput *code,
                                                      SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 index;
    for (index = 0u; index < section->elementCount; ++index) {
        TZrBool valid = ZR_TRUE;
        if (section->kind == ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE)
            valid = artifact_member_row_is_valid(&((const SZrArtifactMemberDefRow *)section->data)[index]);
        else if (section->kind == ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE)
            valid = artifact_property_row_is_valid(&((const SZrArtifactPropertyDefRow *)section->data)[index]);
        else if (section->kind == ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE) {
            const SZrArtifactRelocationRow *row = &((const SZrArtifactRelocationRow *)section->data)[index];
            valid = (TZrBool)(artifact_relocation_row_is_valid(row) && code != ZR_NULL &&
                              row->codeOffset < code->elementCount);
        }
        if (!valid)
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                    section->kind, index, 0u);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_input_identity_rows(const SZrArtifactDocument *document,
                                                               SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactSectionInput *typeRef = artifact_find_input(document, ZR_ARTIFACT_SECTION_TYPE_REF_TABLE);
    const SZrArtifactSectionInput *typeSpec = artifact_find_input(document, ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE);
    const SZrArtifactSectionInput *contract = artifact_find_input(document, ZR_ARTIFACT_SECTION_CONTRACT_TABLE);
    const SZrArtifactSectionInput *layout = artifact_find_input(document, ZR_ARTIFACT_SECTION_LAYOUT_TABLE);
    const SZrArtifactTypeIdentityRow *refRow = ZR_NULL;
    const SZrArtifactTypeIdentityRow *specRow = ZR_NULL;
    const SZrArtifactContractRow *contractRow = ZR_NULL;
    const SZrArtifactLayoutRow *layoutRow = ZR_NULL;
    TZrUInt32 index;
    for (index = 0u; typeRef != ZR_NULL && index < typeRef->elementCount; ++index) {
        const SZrArtifactTypeIdentityRow *row = &((const SZrArtifactTypeIdentityRow *)typeRef->data)[index];
        if (row->token == document->identity.typeRefToken) { refRow = row; break; }
    }
    for (index = 0u; typeSpec != ZR_NULL && index < typeSpec->elementCount; ++index) {
        const SZrArtifactTypeIdentityRow *row = &((const SZrArtifactTypeIdentityRow *)typeSpec->data)[index];
        if (row->token == document->identity.typeSpecToken) { specRow = row; break; }
    }
    for (index = 0u; contract != ZR_NULL && index < contract->elementCount; ++index) {
        const SZrArtifactContractRow *row = &((const SZrArtifactContractRow *)contract->data)[index];
        if (row->contractHash == document->identity.callableContractHash) { contractRow = row; break; }
    }
    for (index = 0u; layout != ZR_NULL && index < layout->elementCount; ++index) {
        const SZrArtifactLayoutRow *row = &((const SZrArtifactLayoutRow *)layout->data)[index];
        if (row->version == document->identity.layoutVersion && row->layoutHash == document->identity.layoutHash) {
            layoutRow = row;
            break;
        }
        if (row->version == document->identity.layoutVersion)
            layoutRow = row;
        else if (layoutRow == ZR_NULL && row->layoutHash == document->identity.layoutHash)
            layoutRow = row;
    }
#define ZR_ARTIFACT_INPUT_HASH_CHECK(COND, STATUS, EXPECTED, ACTUAL, SECTION) \
    if (!(COND)) { \
        zr_artifact_fail(diagnostic, STATUS, SECTION, 0u, 0u); \
        if (diagnostic != ZR_NULL) { diagnostic->expectedHash = (EXPECTED); diagnostic->actualHash = (ACTUAL); } \
        return STATUS; \
    }
    ZR_ARTIFACT_INPUT_HASH_CHECK(refRow != ZR_NULL && refRow->canonicalTypeId == document->identity.canonicalTypeId &&
                                 refRow->signatureToken == document->identity.signatureToken &&
                                 refRow->signatureHash == document->identity.typeRefHash,
                                 ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
                                 document->identity.typeRefHash, refRow != ZR_NULL ? refRow->signatureHash : 0u,
                                 ZR_ARTIFACT_SECTION_TYPE_REF_TABLE);
    ZR_ARTIFACT_INPUT_HASH_CHECK(specRow != ZR_NULL && specRow->canonicalTypeId == document->identity.canonicalTypeId &&
                                 specRow->signatureToken == document->identity.signatureToken &&
                                 specRow->signatureHash == document->identity.typeSpecHash,
                                 ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
                                 document->identity.typeSpecHash, specRow != ZR_NULL ? specRow->signatureHash : 0u,
                                 ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE);
    ZR_ARTIFACT_INPUT_HASH_CHECK(contractRow != ZR_NULL &&
                                 contractRow->contractHash == document->identity.callableContractHash,
                                 ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
                                 document->identity.callableContractHash,
                                 contractRow != ZR_NULL ? contractRow->contractHash : 0u,
                                 ZR_ARTIFACT_SECTION_CONTRACT_TABLE);
    ZR_ARTIFACT_INPUT_HASH_CHECK(layoutRow != ZR_NULL && layoutRow->version == document->identity.layoutVersion &&
                                 layoutRow->layoutHash == document->identity.layoutHash,
                                 layoutRow != ZR_NULL && layoutRow->version != document->identity.layoutVersion
                                         ? ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH
                                         : ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                                 document->identity.layoutHash, layoutRow != ZR_NULL ? layoutRow->layoutHash : 0u,
                                 ZR_ARTIFACT_SECTION_LAYOUT_TABLE);
#undef ZR_ARTIFACT_INPUT_HASH_CHECK
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_document(const SZrArtifactDocument *document,
                                                    TZrSize *outSize,
                                                    SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactSectionInput *signatureHeap;
    TZrUInt64 totalSize;
    TZrUInt32 seenKinds = 0u;
    TZrUInt32 index;
    EZrArtifactStatus status;

    zr_artifact_diagnostic_clear(diagnostic);
    if (outSize != ZR_NULL) {
        *outSize = 0u;
    }
    if (document == ZR_NULL || outSize == ZR_NULL ||
        (document->sectionCount > 0u && document->sections == ZR_NULL)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    }
    if (!zr_artifact_kind_is_valid(document->kind)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_KIND, 0u, 0u, 0u);
    }
    if (document->sectionCount > ZR_ARTIFACT_MAX_SECTION_COUNT) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_COUNT_LIMIT, 0u, 0u, 0u);
    }
    status = artifact_validate_identity(document->kind, &document->identity, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }

    totalSize = ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                (TZrUInt64)document->sectionCount * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE;
    for (index = 0u; index < document->sectionCount; ++index) {
        const SZrArtifactSectionInput *section = &document->sections[index];
        TZrUInt32 elementSize;
        TZrUInt64 byteLength;

        if (!zr_artifact_section_is_known(section->kind)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_UNKNOWN_MANDATORY_SECTION,
                                    section->kind,
                                    0u,
                                    0u);
        }
        if (!zr_artifact_section_is_allowed(document->kind, section->kind)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
                                    section->kind,
                                    0u,
                                    0u);
        }
        elementSize = zr_artifact_section_element_size(section->kind);
        if ((section->flags & ~ZR_ARTIFACT_SECTION_FLAG_KNOWN_MASK) != 0u ||
            section->elementCount > (elementSize == 1u
                                             ? ZR_ARTIFACT_MAX_BYTE_LENGTH
                                             : ZR_ARTIFACT_MAX_ROW_COUNT) ||
            (section->elementCount > 0u && section->data == ZR_NULL)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                    section->kind,
                                    0u,
                                    0u);
        }
        if ((seenKinds & ((TZrUInt32)1u << section->kind)) != 0u) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_DUPLICATE_SECTION,
                                    section->kind,
                                    0u,
                                    0u);
        }
        seenKinds |= (TZrUInt32)1u << section->kind;
        byteLength = (TZrUInt64)section->elementCount * elementSize;
        totalSize += byteLength;
        if (byteLength > ZR_ARTIFACT_MAX_BYTE_LENGTH || totalSize > ZR_ARTIFACT_MAX_BYTE_LENGTH) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                                    section->kind,
                                    0u,
                                    0u);
        }
    }

    signatureHeap = artifact_find_input(document, ZR_ARTIFACT_SECTION_SIGNATURE_HEAP);
    for (index = 0u; index < document->sectionCount; ++index) {
        const SZrArtifactSectionInput *section = &document->sections[index];
        switch (section->kind) {
            case ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE:
                status = artifact_validate_type_def_input(section, diagnostic);
                break;
            case ZR_ARTIFACT_SECTION_TYPE_REF_TABLE:
            case ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE:
                status = artifact_validate_type_identity_input(section, signatureHeap, diagnostic);
                break;
            case ZR_ARTIFACT_SECTION_CONTRACT_TABLE:
                status = artifact_validate_contract_input(section, diagnostic);
                break;
            case ZR_ARTIFACT_SECTION_LAYOUT_TABLE:
                status = artifact_validate_layout_input(section, diagnostic);
                break;
            case ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE:
                status = artifact_validate_domain_transfer_input(
                        section, signatureHeap, diagnostic);
                break;
            case ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE:
            case ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE:
            case ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE:
                status = artifact_validate_link_input(
                        section,
                        artifact_find_input(document, ZR_ARTIFACT_SECTION_CODE_TABLE),
                        diagnostic);
                break;
            default:
                status = ZR_ARTIFACT_STATUS_OK;
                break;
        }
        if (status != ZR_ARTIFACT_STATUS_OK) {
            return status;
        }
    }

    if (document->kind != ZR_ARTIFACT_KIND_ZRS) {
        status = artifact_validate_input_identity_rows(document, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }

    *outSize = (TZrSize)totalSize;
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_GetEncodedSize(const SZrArtifactDocument *document,
                                                 TZrSize *outSize,
                                                 SZrArtifactDiagnostic *diagnostic) {
    return artifact_validate_document(document, outSize, diagnostic);
}

static void artifact_write_identity(TZrByte *bytes, const SZrArtifactPublicIdentity *identity) {
    zr_artifact_write_u32(bytes + 0u, identity->canonicalTypeId);
    zr_artifact_write_u32(bytes + 4u, identity->typeRefToken);
    zr_artifact_write_u32(bytes + 8u, identity->typeSpecToken);
    zr_artifact_write_u32(bytes + 12u, identity->signatureToken);
    zr_artifact_write_u64(bytes + 16u, identity->typeRefHash);
    zr_artifact_write_u64(bytes + 24u, identity->typeSpecHash);
    zr_artifact_write_u64(bytes + 32u, identity->signatureHash);
    zr_artifact_write_u32(bytes + 40u, identity->layoutVersion);
    zr_artifact_write_u32(bytes + 44u, 0u);
    zr_artifact_write_u64(bytes + 48u, identity->layoutHash);
    zr_artifact_write_u64(bytes + 56u, identity->callableContractHash);
    zr_artifact_write_u64(bytes + 64u, identity->moduleHash);
}

static void artifact_read_identity(const TZrByte *bytes, SZrArtifactPublicIdentity *identity) {
    memset(identity, 0, sizeof(*identity));
    identity->canonicalTypeId = zr_artifact_read_u32(bytes + 0u);
    identity->typeRefToken = zr_artifact_read_u32(bytes + 4u);
    identity->typeSpecToken = zr_artifact_read_u32(bytes + 8u);
    identity->signatureToken = zr_artifact_read_u32(bytes + 12u);
    identity->typeRefHash = zr_artifact_read_u64(bytes + 16u);
    identity->typeSpecHash = zr_artifact_read_u64(bytes + 24u);
    identity->signatureHash = zr_artifact_read_u64(bytes + 32u);
    identity->layoutVersion = zr_artifact_read_u32(bytes + 40u);
    identity->layoutHash = zr_artifact_read_u64(bytes + 48u);
    identity->callableContractHash = zr_artifact_read_u64(bytes + 56u);
    identity->moduleHash = zr_artifact_read_u64(bytes + 64u);
}

EZrArtifactStatus ZrCore_Artifact_Write(const SZrArtifactDocument *document,
                                        TZrByte *buffer,
                                        TZrSize bufferCapacity,
                                        TZrSize *outWrittenSize,
                                        SZrArtifactDiagnostic *diagnostic) {
    TZrSize requiredSize;
    TZrUInt32 payloadOffset;
    TZrUInt32 index;
    EZrArtifactStatus status;

    if (outWrittenSize != ZR_NULL) {
        *outWrittenSize = 0u;
    }
    status = artifact_validate_document(document, &requiredSize, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (buffer == ZR_NULL || outWrittenSize == ZR_NULL) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    }
    if (bufferCapacity < requiredSize) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u, 0u, 0u);
    }

    memset(buffer, 0, requiredSize);
    buffer[0] = ZR_ARTIFACT_MAGIC_0;
    buffer[1] = ZR_ARTIFACT_MAGIC_1;
    buffer[2] = ZR_ARTIFACT_MAGIC_2;
    buffer[3] = ZR_ARTIFACT_MAGIC_3;
    zr_artifact_write_u16(buffer + ZR_ARTIFACT_HEADER_VERSION_OFFSET, ZR_ARTIFACT_SCHEMA_VERSION);
    zr_artifact_write_u16(buffer + ZR_ARTIFACT_HEADER_SIZE_OFFSET, ZR_ARTIFACT_HEADER_ENCODED_SIZE);
    zr_artifact_write_u32(buffer + ZR_ARTIFACT_HEADER_KIND_OFFSET, (TZrUInt32)document->kind);
    zr_artifact_write_u32(buffer + ZR_ARTIFACT_HEADER_FLAGS_OFFSET, document->flags);
    zr_artifact_write_u32(buffer + ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET, document->sectionCount);
    zr_artifact_write_u32(buffer + ZR_ARTIFACT_HEADER_DIRECTORY_SIZE_OFFSET,
                          ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE);
    zr_artifact_write_u32(buffer + ZR_ARTIFACT_HEADER_TOTAL_SIZE_OFFSET, (TZrUInt32)requiredSize);
    artifact_write_identity(buffer + ZR_ARTIFACT_HEADER_IDENTITY_OFFSET, &document->identity);

    payloadOffset = ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                    document->sectionCount * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE;
    for (index = 0u; index < document->sectionCount; ++index) {
        const SZrArtifactSectionInput *section = &document->sections[index];
        TZrByte *directory = buffer + ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                             (TZrSize)index * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE;
        TZrUInt32 elementSize = zr_artifact_section_element_size(section->kind);
        TZrUInt32 byteLength = section->elementCount * elementSize;
        zr_artifact_write_u32(directory + 0u, section->kind);
        zr_artifact_write_u32(directory + 4u, section->flags);
        zr_artifact_write_u32(directory + 8u, payloadOffset);
        zr_artifact_write_u32(directory + 12u, byteLength);
        zr_artifact_write_u32(directory + 16u, section->elementCount);
        zr_artifact_write_u32(directory + 20u, elementSize);
        zr_artifact_write_section_payload(buffer + payloadOffset, section);
        payloadOffset += byteLength;
    }

    *outWrittenSize = requiredSize;
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus zr_artifact_decode_directory_entry(const SZrArtifactView *view,
                                                      TZrUInt32 index,
                                                      SZrArtifactSectionView *outSection,
                                                      SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *directory;
    if (view == ZR_NULL || outSection == ZR_NULL || index >= view->sectionCount) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, index, 0u);
    }
    directory = view->buffer + ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                (TZrSize)index * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE;
    outSection->kind = zr_artifact_read_u32(directory + 0u);
    outSection->flags = zr_artifact_read_u32(directory + 4u);
    outSection->byteOffset = zr_artifact_read_u32(directory + 8u);
    outSection->byteLength = zr_artifact_read_u32(directory + 12u);
    outSection->elementCount = zr_artifact_read_u32(directory + 16u);
    outSection->elementSize = zr_artifact_read_u32(directory + 20u);
    outSection->data = outSection->byteOffset <= view->bufferLength
                               ? view->buffer + outSection->byteOffset
                               : ZR_NULL;
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_decoded_sections(const SZrArtifactView *view,
                                                            SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 directoryEnd = ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                             view->sectionCount * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE;
    TZrUInt32 seenKinds = 0u;
    TZrUInt32 leftIndex;

    for (leftIndex = 0u; leftIndex < view->sectionCount; ++leftIndex) {
        SZrArtifactSectionView left;
        TZrUInt32 expectedElementSize;
        TZrUInt64 end;
        TZrUInt32 rightIndex;

        zr_artifact_decode_directory_entry(view, leftIndex, &left, diagnostic);
        if ((left.flags & ~ZR_ARTIFACT_SECTION_FLAG_KNOWN_MASK) != 0u ||
            left.elementCount > (zr_artifact_section_is_known(left.kind) &&
                                         zr_artifact_section_element_size(left.kind) == 1u
                                         ? ZR_ARTIFACT_MAX_BYTE_LENGTH
                                         : ZR_ARTIFACT_MAX_ROW_COUNT)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                    left.kind,
                                    leftIndex,
                                    left.byteOffset);
        }
        if (!zr_artifact_section_is_known(left.kind)) {
            if ((left.flags & ZR_ARTIFACT_SECTION_FLAG_OPTIONAL) == 0u) {
                return zr_artifact_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_UNKNOWN_MANDATORY_SECTION,
                                        left.kind,
                                        leftIndex,
                                        left.byteOffset);
            }
        } else {
            if (!zr_artifact_section_is_allowed(view->kind, left.kind)) {
                return zr_artifact_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
                                        left.kind,
                                        leftIndex,
                                        left.byteOffset);
            }
            if ((seenKinds & ((TZrUInt32)1u << left.kind)) != 0u) {
                return zr_artifact_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_DUPLICATE_SECTION,
                                        left.kind,
                                        leftIndex,
                                        left.byteOffset);
            }
            seenKinds |= (TZrUInt32)1u << left.kind;
            expectedElementSize = zr_artifact_section_element_size(left.kind);
            if (left.elementSize != expectedElementSize ||
                (TZrUInt64)left.elementCount * left.elementSize != left.byteLength) {
                return zr_artifact_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                        left.kind,
                                        leftIndex,
                                        left.byteOffset);
            }
        }
        end = (TZrUInt64)left.byteOffset + left.byteLength;
        if (left.byteOffset < directoryEnd || end > view->bufferLength) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_TRUNCATED,
                                    left.kind,
                                    leftIndex,
                                    left.byteOffset);
        }
        for (rightIndex = 0u; rightIndex < leftIndex; ++rightIndex) {
            SZrArtifactSectionView right;
            TZrUInt64 leftEnd = (TZrUInt64)left.byteOffset + left.byteLength;
            TZrUInt64 rightEnd;
            zr_artifact_decode_directory_entry(view, rightIndex, &right, diagnostic);
            rightEnd = (TZrUInt64)right.byteOffset + right.byteLength;
            if (left.byteLength > 0u && right.byteLength > 0u &&
                left.byteOffset < rightEnd && right.byteOffset < leftEnd) {
                return zr_artifact_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_SECTION_OVERLAP,
                                        left.kind,
                                        leftIndex,
                                        left.byteOffset);
            }
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_validate_decoded_rows(const SZrArtifactView *view,
                                                        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSectionView signatureHeap;
    SZrArtifactSectionView code;
    TZrBool hasSignatureHeap = (TZrBool)(ZrCore_Artifact_FindSection(
            view, ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, &signatureHeap, ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
    TZrBool hasCode = (TZrBool)(ZrCore_Artifact_FindSection(
            view, ZR_ARTIFACT_SECTION_CODE_TABLE, &code, ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
    TZrUInt32 sectionIndex;

    for (sectionIndex = 0u; sectionIndex < view->sectionCount; ++sectionIndex) {
        SZrArtifactSectionView section;
        TZrUInt32 rowIndex;
        zr_artifact_decode_directory_entry(view, sectionIndex, &section, diagnostic);
        if (!zr_artifact_section_is_known(section.kind)) {
            continue;
        }
        for (rowIndex = 0u; rowIndex < section.elementCount; ++rowIndex) {
            if (section.kind == ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE) {
                SZrArtifactTypeDefRow row;
                ZrCore_Artifact_ReadTypeDefRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_token_is(row.token, ZR_METADATA_TABLE_TYPE_DEF) ||
                    row.canonicalTypeId == 0u || row.typeSignatureHash == 0u ||
                    (row.flags & ~ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK) != 0u ||
                    ((row.flags & ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE) != 0u &&
                     (!artifact_token_is(row.constructorToken, ZR_METADATA_TABLE_MEMBER_DEF) ||
                      !artifact_token_is(row.constructorSignatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
                      row.constructorContractHash == 0u))) {
                    return zr_artifact_fail(diagnostic,
                                            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind,
                                            rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
                }
            } else if (section.kind == ZR_ARTIFACT_SECTION_TYPE_REF_TABLE ||
                       section.kind == ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE) {
                SZrArtifactTypeIdentityRow row;
                TZrUInt32 table = section.kind == ZR_ARTIFACT_SECTION_TYPE_REF_TABLE
                                          ? ZR_METADATA_TABLE_TYPE_REF
                                          : ZR_METADATA_TABLE_TYPE_SPEC;
                ZrCore_Artifact_ReadTypeIdentityRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_token_is(row.token, table) ||
                    !artifact_token_is(row.signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
                    row.canonicalTypeId == 0u || row.signatureHash == 0u ||
                    row.layoutVersion == 0u || row.layoutHash == 0u) {
                    return zr_artifact_fail(diagnostic,
                                            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind,
                                            rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
                }
                if (!hasSignatureHeap || row.signatureLength == 0u ||
                    (TZrUInt64)row.signatureOffset + row.signatureLength > signatureHeap.byteLength) {
                    return zr_artifact_fail(diagnostic,
                                            ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                            section.kind,
                                            rowIndex,
                                            row.signatureOffset);
                }
                {
                    EZrArtifactStatus status = ZrCore_Artifact_ValidateSignature(
                            signatureHeap.data + row.signatureOffset,
                            row.signatureLength,
                            diagnostic);
                    if (status != ZR_ARTIFACT_STATUS_OK) {
                        if (diagnostic != ZR_NULL) {
                            diagnostic->sectionKind = section.kind;
                            diagnostic->rowIndex = rowIndex;
                            diagnostic->byteOffset += row.signatureOffset;
                        }
                        return status;
                    }
                }
            } else if (section.kind == ZR_ARTIFACT_SECTION_CONTRACT_TABLE) {
                SZrArtifactContractRow row;
                ZrCore_Artifact_ReadContractRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_token_is(row.memberToken, ZR_METADATA_TABLE_MEMBER_DEF) ||
                    !artifact_token_is(row.signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
                    row.parameterCount > ZR_ARTIFACT_MAX_ROW_COUNT ||
                    (row.flags & ~ZR_ARTIFACT_CONTRACT_FLAG_KNOWN_MASK) != 0u ||
                    row.receiverEffect > ZR_ARTIFACT_RECEIVER_MUTABLE ||
                    row.refExportEffect > ZR_ARTIFACT_REF_EXPORT_WRITABLE ||
                    row.contractHash == 0u) {
                    return zr_artifact_fail(diagnostic,
                                            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind,
                                            rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
                }
            } else if (section.kind == ZR_ARTIFACT_SECTION_LAYOUT_TABLE) {
                SZrArtifactLayoutRow row;
                ZrCore_Artifact_ReadLayoutRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_type_token_is_valid(row.typeToken) || row.version == 0u ||
                    row.byteAlignment == 0u || row.gcScanKind > ZR_ARTIFACT_GC_SCAN_BARRIERED ||
                    (row.capabilityFlags & ~ZR_ARTIFACT_LAYOUT_CAPABILITY_KNOWN_MASK) != 0u ||
                    row.layoutHash == 0u ||
                    ((row.capabilityFlags & ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE) != 0u &&
                     row.stableSlotContractHash == 0u)) {
                    return zr_artifact_fail(diagnostic,
                                            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind,
                                            rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
                }
            } else if (section.kind ==
                       ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE) {
                SZrArtifactDomainTransferRow row;
                SZrArtifactDomainTransferRow previousRow;
                ZrCore_Artifact_ReadDomainTransferRow(
                        &section, rowIndex, &row, diagnostic);
                if (rowIndex > 0u) {
                    ZrCore_Artifact_ReadDomainTransferRow(
                            &section,
                            rowIndex - 1u,
                            &previousRow,
                            diagnostic);
                    if (previousRow.typeToken >= row.typeToken) {
                        return zr_artifact_fail(
                                diagnostic,
                                ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                section.kind,
                                rowIndex,
                                section.byteOffset +
                                        rowIndex * section.elementSize);
                    }
                }
                if (!artifact_domain_transfer_row_is_valid(
                            &row,
                            hasSignatureHeap,
                            hasSignatureHeap
                                    ? signatureHeap.byteLength
                                    : 0u)) {
                    return zr_artifact_fail(
                            diagnostic,
                            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                            section.kind,
                            rowIndex,
                            row.schemaOffset);
                }
            } else if (section.kind == ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE) {
                SZrArtifactMemberDefRow row;
                ZrCore_Artifact_ReadMemberDefRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_member_row_is_valid(&row))
                    return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind, rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
            } else if (section.kind == ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE) {
                SZrArtifactPropertyDefRow row;
                ZrCore_Artifact_ReadPropertyDefRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_property_row_is_valid(&row))
                    return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind, rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
            } else if (section.kind == ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE) {
                SZrArtifactRelocationRow row;
                ZrCore_Artifact_ReadRelocationRow(&section, rowIndex, &row, diagnostic);
                if (!artifact_relocation_row_is_valid(&row) || !hasCode || row.codeOffset >= code.byteLength)
                    return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                            section.kind, rowIndex,
                                            section.byteOffset + rowIndex * section.elementSize);
            }
        }
    }
    if (view->kind != ZR_ARTIFACT_KIND_ZRS) {
        SZrArtifactSectionView refSection, specSection, contractSection, layoutSection;
        SZrArtifactTypeIdentityRow refRow = {0}, specRow = {0};
        SZrArtifactContractRow contractRow = {0};
        SZrArtifactLayoutRow layoutRow = {0};
        SZrArtifactLayoutRow layoutCandidate = {0};
        TZrBool foundRef = ZR_FALSE, foundSpec = ZR_FALSE, foundContract = ZR_FALSE, foundLayout = ZR_FALSE;
        TZrBool foundLayoutCandidate = ZR_FALSE;
        TZrUInt32 index;
        if (ZrCore_Artifact_FindSection(view, ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, &refSection, ZR_NULL) != ZR_ARTIFACT_STATUS_OK ||
            ZrCore_Artifact_FindSection(view, ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, &specSection, ZR_NULL) != ZR_ARTIFACT_STATUS_OK ||
            ZrCore_Artifact_FindSection(view, ZR_ARTIFACT_SECTION_CONTRACT_TABLE, &contractSection, ZR_NULL) != ZR_ARTIFACT_STATUS_OK ||
            ZrCore_Artifact_FindSection(view, ZR_ARTIFACT_SECTION_LAYOUT_TABLE, &layoutSection, ZR_NULL) != ZR_ARTIFACT_STATUS_OK)
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u, 0u);
        for (index = 0u; index < refSection.elementCount; ++index) {
            ZrCore_Artifact_ReadTypeIdentityRow(&refSection, index, &refRow, ZR_NULL);
            if (refRow.token == view->identity.typeRefToken) { foundRef = ZR_TRUE; break; }
        }
        for (index = 0u; index < specSection.elementCount; ++index) {
            ZrCore_Artifact_ReadTypeIdentityRow(&specSection, index, &specRow, ZR_NULL);
            if (specRow.token == view->identity.typeSpecToken) { foundSpec = ZR_TRUE; break; }
        }
        for (index = 0u; index < contractSection.elementCount; ++index) {
            ZrCore_Artifact_ReadContractRow(&contractSection, index, &contractRow, ZR_NULL);
            if (contractRow.contractHash == view->identity.callableContractHash) { foundContract = ZR_TRUE; break; }
        }
        for (index = 0u; index < layoutSection.elementCount; ++index) {
            SZrArtifactLayoutRow current;
            ZrCore_Artifact_ReadLayoutRow(&layoutSection, index, &current, ZR_NULL);
            if (current.version == view->identity.layoutVersion &&
                current.layoutHash == view->identity.layoutHash) {
                layoutRow = current;
                foundLayout = ZR_TRUE;
                break;
            }
            if (!foundLayoutCandidate && current.version == view->identity.layoutVersion) {
                layoutCandidate = current;
                foundLayoutCandidate = ZR_TRUE;
            }
            layoutRow = current;
        }
        if (!foundLayout && foundLayoutCandidate) layoutRow = layoutCandidate;
#define ZR_ARTIFACT_DECODED_HASH_CHECK(COND, STATUS, EXPECTED, ACTUAL, SECTION) \
        if (!(COND)) { zr_artifact_fail(diagnostic, STATUS, SECTION, 0u, 0u); \
            if (diagnostic != ZR_NULL) { diagnostic->expectedHash = (EXPECTED); diagnostic->actualHash = (ACTUAL); } \
            return STATUS; }
        ZR_ARTIFACT_DECODED_HASH_CHECK(foundRef && refRow.canonicalTypeId == view->identity.canonicalTypeId &&
                                       refRow.signatureToken == view->identity.signatureToken &&
                                       refRow.signatureHash == view->identity.typeRefHash,
                                       ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH, view->identity.typeRefHash,
                                       refRow.signatureHash, ZR_ARTIFACT_SECTION_TYPE_REF_TABLE);
        ZR_ARTIFACT_DECODED_HASH_CHECK(foundSpec && specRow.canonicalTypeId == view->identity.canonicalTypeId &&
                                       specRow.signatureToken == view->identity.signatureToken &&
                                       specRow.signatureHash == view->identity.typeSpecHash,
                                       ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH, view->identity.typeSpecHash,
                                       specRow.signatureHash, ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE);
        ZR_ARTIFACT_DECODED_HASH_CHECK(foundContract && contractRow.contractHash == view->identity.callableContractHash,
                                       ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH, view->identity.callableContractHash,
                                       contractRow.contractHash, ZR_ARTIFACT_SECTION_CONTRACT_TABLE);
        if (layoutRow.version != view->identity.layoutVersion) {
            zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
                             ZR_ARTIFACT_SECTION_LAYOUT_TABLE, 0u, 0u);
            if (diagnostic != ZR_NULL) { diagnostic->expectedVersion = view->identity.layoutVersion;
                diagnostic->actualVersion = layoutRow.version; }
            return ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH;
        }
        ZR_ARTIFACT_DECODED_HASH_CHECK(foundLayout && layoutRow.layoutHash == view->identity.layoutHash,
                                       ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH, view->identity.layoutHash,
                                       layoutRow.layoutHash, ZR_ARTIFACT_SECTION_LAYOUT_TABLE);
#undef ZR_ARTIFACT_DECODED_HASH_CHECK
    }
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_Read(const TZrByte *buffer,
                                       TZrSize bufferLength,
                                       SZrArtifactView *outView,
                                       SZrArtifactDiagnostic *diagnostic) {
    TZrUInt16 version;
    TZrUInt16 headerSize;
    TZrUInt32 sectionCount;
    TZrUInt32 directoryEntrySize;
    TZrUInt32 totalSize;
    EZrArtifactStatus status;

    zr_artifact_diagnostic_clear(diagnostic);
    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (buffer == ZR_NULL || outView == ZR_NULL) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    }
    if (bufferLength < ZR_ARTIFACT_HEADER_ENCODED_SIZE) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_TRUNCATED, 0u, 0u, 0u);
    }
    if (buffer[0] != ZR_ARTIFACT_MAGIC_0 || buffer[1] != ZR_ARTIFACT_MAGIC_1 ||
        buffer[2] != ZR_ARTIFACT_MAGIC_2 || buffer[3] != ZR_ARTIFACT_MAGIC_3) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BAD_MAGIC, 0u, 0u, 0u);
    }
    version = zr_artifact_read_u16(buffer + ZR_ARTIFACT_HEADER_VERSION_OFFSET);
    headerSize = zr_artifact_read_u16(buffer + ZR_ARTIFACT_HEADER_SIZE_OFFSET);
    if (version != ZR_ARTIFACT_SCHEMA_VERSION || headerSize != ZR_ARTIFACT_HEADER_ENCODED_SIZE) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION, 0u, 0u, 4u);
    }
    outView->kind = (EZrArtifactKind)zr_artifact_read_u32(buffer + ZR_ARTIFACT_HEADER_KIND_OFFSET);
    if (!zr_artifact_kind_is_valid(outView->kind)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_KIND, 0u, 0u, 8u);
    }
    outView->flags = zr_artifact_read_u32(buffer + ZR_ARTIFACT_HEADER_FLAGS_OFFSET);
    sectionCount = zr_artifact_read_u32(buffer + ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET);
    directoryEntrySize = zr_artifact_read_u32(buffer + ZR_ARTIFACT_HEADER_DIRECTORY_SIZE_OFFSET);
    totalSize = zr_artifact_read_u32(buffer + ZR_ARTIFACT_HEADER_TOTAL_SIZE_OFFSET);
    if (sectionCount > ZR_ARTIFACT_MAX_SECTION_COUNT) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_COUNT_LIMIT, 0u, 0u, 16u);
    }
    if (directoryEntrySize != ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u, 20u);
    }
    if (totalSize > bufferLength || totalSize < ZR_ARTIFACT_HEADER_ENCODED_SIZE +
                                                  sectionCount * directoryEntrySize) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_TRUNCATED, 0u, 0u, 24u);
    }
    outView->sectionCount = sectionCount;
    outView->buffer = buffer;
    outView->bufferLength = totalSize;
    artifact_read_identity(buffer + ZR_ARTIFACT_HEADER_IDENTITY_OFFSET, &outView->identity);
    status = artifact_validate_identity(outView->kind, &outView->identity, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    status = artifact_validate_decoded_sections(outView, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    return artifact_validate_decoded_rows(outView, diagnostic);
}

EZrArtifactStatus ZrCore_Artifact_FindSection(const SZrArtifactView *view,
                                              EZrArtifactSectionKind kind,
                                              SZrArtifactSectionView *outSection,
                                              SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 index;
    zr_artifact_diagnostic_clear(diagnostic);
    if (view == ZR_NULL || outSection == ZR_NULL) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, kind, 0u, 0u);
    }
    memset(outSection, 0, sizeof(*outSection));
    for (index = 0u; index < view->sectionCount; ++index) {
        SZrArtifactSectionView candidate;
        zr_artifact_decode_directory_entry(view, index, &candidate, diagnostic);
        if (candidate.kind == (TZrUInt32)kind) {
            *outSection = candidate;
            return ZR_ARTIFACT_STATUS_OK;
        }
    }
    return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, kind, 0u, 0u);
}
