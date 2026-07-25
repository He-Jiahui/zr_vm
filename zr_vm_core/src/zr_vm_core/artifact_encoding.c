#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

void zr_artifact_diagnostic_clear(SZrArtifactDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_ARTIFACT_STATUS_OK;
    }
}

EZrArtifactStatus zr_artifact_fail(SZrArtifactDiagnostic *diagnostic,
                                   EZrArtifactStatus status,
                                   TZrUInt32 sectionKind,
                                   TZrUInt32 rowIndex,
                                   TZrUInt32 byteOffset) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
        diagnostic->rowIndex = rowIndex;
        diagnostic->byteOffset = byteOffset;
    }
    return status;
}

TZrUInt16 zr_artifact_read_u16(const TZrByte *bytes) {
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

TZrUInt32 zr_artifact_read_u32(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] | ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) | ((TZrUInt32)bytes[3] << 24u);
}

TZrUInt64 zr_artifact_read_u64(const TZrByte *bytes) {
    return (TZrUInt64)zr_artifact_read_u32(bytes) |
           ((TZrUInt64)zr_artifact_read_u32(bytes + 4u) << 32u);
}

void zr_artifact_write_u16(TZrByte *bytes, TZrUInt16 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
}

void zr_artifact_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

void zr_artifact_write_u64(TZrByte *bytes, TZrUInt64 value) {
    zr_artifact_write_u32(bytes, (TZrUInt32)(value & 0xffffffffULL));
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)(value >> 32u));
}

TZrBool zr_artifact_kind_is_valid(EZrArtifactKind kind) {
    return (TZrBool)(kind == ZR_ARTIFACT_KIND_ZRS ||
                     kind == ZR_ARTIFACT_KIND_ZRI ||
                     kind == ZR_ARTIFACT_KIND_ZRO);
}

TZrBool zr_artifact_section_is_known(TZrUInt32 kind) {
    return (TZrBool)(kind >= ZR_ARTIFACT_SECTION_STRING_HEAP &&
                     kind <= ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE);
}

TZrBool zr_artifact_section_is_allowed(EZrArtifactKind artifactKind, TZrUInt32 sectionKind) {
    if (artifactKind == ZR_ARTIFACT_KIND_ZRS)
        return (TZrBool)(sectionKind == ZR_ARTIFACT_SECTION_STRING_HEAP ||
                         sectionKind == ZR_ARTIFACT_SECTION_SYNTAX_TREE ||
                         sectionKind == ZR_ARTIFACT_SECTION_DEBUG_MAP);
    if (artifactKind == ZR_ARTIFACT_KIND_ZRI)
        return (TZrBool)(sectionKind != ZR_ARTIFACT_SECTION_SYNTAX_TREE &&
                         sectionKind != ZR_ARTIFACT_SECTION_CODE_TABLE &&
                         sectionKind != ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE);
    if (artifactKind == ZR_ARTIFACT_KIND_ZRO)
        return (TZrBool)(sectionKind != ZR_ARTIFACT_SECTION_SYNTAX_TREE &&
                         sectionKind != ZR_ARTIFACT_SECTION_SEMANTIC_IR);
    return ZR_FALSE;
}

TZrUInt32 zr_artifact_section_element_size(TZrUInt32 kind) {
    switch (kind) {
        case ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE: return ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_TYPE_REF_TABLE:
        case ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE: return ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE: return ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE: return ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_CONTRACT_TABLE: return ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_LAYOUT_TABLE: return ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE:
            return ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE:
            return ZR_ARTIFACT_SCHEDULER_CONTRACT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE: return ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_STRING_HEAP:
        case ZR_ARTIFACT_SECTION_SIGNATURE_HEAP:
        case ZR_ARTIFACT_SECTION_CODE_TABLE:
        case ZR_ARTIFACT_SECTION_DEBUG_MAP:
        case ZR_ARTIFACT_SECTION_SYNTAX_TREE:
        case ZR_ARTIFACT_SECTION_SEMANTIC_IR: return 1u;
        default: return 0u;
    }
}
