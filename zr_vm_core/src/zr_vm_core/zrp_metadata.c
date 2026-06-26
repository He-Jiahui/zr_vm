#include "zr_vm_core/zrp_metadata.h"

#include "zr_vm_core/memory.h"

#define ZR_ZRP_METADATA_SECTION_ENCODED_SIZE 16u
#define ZR_ZRP_METADATA_SIGNATURE_MAX_RECURSION_DEPTH 64u

static void zrp_metadata_write_u16(TZrByte *buffer, TZrSize offset, TZrUInt16 value) {
    buffer[offset] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1u] = (TZrByte)((value >> 8u) & 0xFFu);
}

static void zrp_metadata_write_u32(TZrByte *buffer, TZrSize offset, TZrUInt32 value) {
    buffer[offset] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1u] = (TZrByte)((value >> 8u) & 0xFFu);
    buffer[offset + 2u] = (TZrByte)((value >> 16u) & 0xFFu);
    buffer[offset + 3u] = (TZrByte)((value >> 24u) & 0xFFu);
}

static TZrUInt16 zrp_metadata_read_u16(const TZrByte *buffer, TZrSize offset) {
    return (TZrUInt16)((TZrUInt16)buffer[offset] | ((TZrUInt16)buffer[offset + 1u] << 8u));
}

static TZrUInt32 zrp_metadata_read_u32(const TZrByte *buffer, TZrSize offset) {
    return ((TZrUInt32)buffer[offset]) |
           ((TZrUInt32)buffer[offset + 1u] << 8u) |
           ((TZrUInt32)buffer[offset + 2u] << 16u) |
           ((TZrUInt32)buffer[offset + 3u] << 24u);
}

static TZrBool zrp_metadata_signature_read_u8(const TZrByte *buffer,
                                              TZrSize bufferLength,
                                              TZrSize *offset,
                                              TZrUInt8 *outValue) {
    if (buffer == ZR_NULL || offset == ZR_NULL || outValue == ZR_NULL || *offset >= bufferLength) {
        return ZR_FALSE;
    }

    *outValue = buffer[*offset];
    *offset += 1u;
    return ZR_TRUE;
}

static TZrBool zrp_metadata_signature_skip_u32(const TZrByte *buffer,
                                               TZrSize bufferLength,
                                               TZrSize *offset) {
    if (buffer == ZR_NULL || offset == ZR_NULL || *offset > bufferLength || bufferLength - *offset < 4u) {
        return ZR_FALSE;
    }

    *offset += 4u;
    return ZR_TRUE;
}

static TZrBool zrp_metadata_signature_read_u32(const TZrByte *buffer,
                                               TZrSize bufferLength,
                                               TZrSize *offset,
                                               TZrUInt32 *outValue) {
    if (!zrp_metadata_signature_skip_u32(buffer, bufferLength, offset)) {
        return ZR_FALSE;
    }

    *outValue = zrp_metadata_read_u32(buffer, *offset - 4u);
    return ZR_TRUE;
}

static void zrp_metadata_write_section(TZrByte *buffer, TZrSize offset, const SZrZrpMetadataSection *section) {
    zrp_metadata_write_u32(buffer, offset, section->offset);
    zrp_metadata_write_u32(buffer, offset + 4u, section->byteLength);
    zrp_metadata_write_u32(buffer, offset + 8u, section->count);
    zrp_metadata_write_u32(buffer, offset + 12u, section->elementSize);
}

static void zrp_metadata_read_section(const TZrByte *buffer, TZrSize offset, SZrZrpMetadataSection *section) {
    section->offset = zrp_metadata_read_u32(buffer, offset);
    section->byteLength = zrp_metadata_read_u32(buffer, offset + 4u);
    section->count = zrp_metadata_read_u32(buffer, offset + 8u);
    section->elementSize = zrp_metadata_read_u32(buffer, offset + 12u);
}

static const SZrZrpMetadataSection *zrp_metadata_get_section(const SZrZrpMetadataHeader *header,
                                                             EZrZrpMetadataSectionKind sectionKind) {
    if (header == ZR_NULL) {
        return ZR_NULL;
    }

    switch (sectionKind) {
        case ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS:
            return &header->tokenRecords;
        case ZR_ZRP_METADATA_SECTION_TYPE_DEFS:
            return &header->typeDefs;
        case ZR_ZRP_METADATA_SECTION_METHOD_DEFS:
            return &header->methodDefs;
        case ZR_ZRP_METADATA_SECTION_FIELD_DEFS:
            return &header->fieldDefs;
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS:
            return &header->genericParams;
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS:
            return &header->genericParamConstraints;
        case ZR_ZRP_METADATA_SECTION_TYPE_SPECS:
            return &header->typeSpecs;
        case ZR_ZRP_METADATA_SECTION_METHOD_SPECS:
            return &header->methodSpecs;
        case ZR_ZRP_METADATA_SECTION_MODULE_REFS:
            return &header->moduleRefs;
        case ZR_ZRP_METADATA_SECTION_STRING_POOL:
            return &header->stringPool;
        case ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL:
            return &header->signatureBlobPool;
        case ZR_ZRP_METADATA_SECTION_CONSTANT_POOL:
            return &header->constantPool;
        default:
            return ZR_NULL;
    }
}

static TZrBool zrp_metadata_section_kind_is_pool(EZrZrpMetadataSectionKind sectionKind) {
    return sectionKind == ZR_ZRP_METADATA_SECTION_STRING_POOL ||
           sectionKind == ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL ||
           sectionKind == ZR_ZRP_METADATA_SECTION_CONSTANT_POOL;
}

static TZrUInt32 zrp_metadata_definition_table_element_size(EZrZrpMetadataSectionKind sectionKind) {
    switch (sectionKind) {
        case ZR_ZRP_METADATA_SECTION_TYPE_DEFS:
            return (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
        case ZR_ZRP_METADATA_SECTION_METHOD_DEFS:
            return (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
        case ZR_ZRP_METADATA_SECTION_FIELD_DEFS:
            return (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS:
            return (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS:
            return (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
        case ZR_ZRP_METADATA_SECTION_TYPE_SPECS:
            return (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
        case ZR_ZRP_METADATA_SECTION_METHOD_SPECS:
            return (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
        case ZR_ZRP_METADATA_SECTION_MODULE_REFS:
            return (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
        default:
            return 0u;
    }
}

static TZrBool zrp_metadata_token_has_table(TZrMetadataToken token, TZrUInt32 table) {
    return token != 0u && ZR_METADATA_TOKEN_TABLE(token) == table;
}

static TZrBool zrp_metadata_token_is_type_reference(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);

    return token != 0u &&
           (table == ZR_METADATA_TABLE_TYPE_DEF ||
            table == ZR_METADATA_TABLE_TYPE_REF ||
            table == ZR_METADATA_TABLE_TYPE_SPEC);
}

static TZrBool zrp_metadata_token_is_method_reference(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);

    return token != 0u &&
           (table == ZR_METADATA_TABLE_MEMBER_DEF ||
            table == ZR_METADATA_TABLE_MEMBER_REF);
}

static TZrBool zrp_metadata_rid_is_in_count(TZrMetadataToken token, TZrUInt32 count) {
    TZrUInt32 rid = ZR_METADATA_TOKEN_RID(token);

    return rid != 0u && rid <= count;
}

static TZrBool zrp_metadata_range_is_in_count(TZrUInt32 firstIndex, TZrUInt32 count, TZrUInt32 totalCount) {
    if (count == 0u) {
        return firstIndex <= totalCount;
    }
    if (firstIndex >= totalCount) {
        return ZR_FALSE;
    }
    return count <= (totalCount - firstIndex);
}

static TZrBool zrp_metadata_section_is_valid(const SZrZrpMetadataSection *section,
                                             TZrSize bufferLength,
                                             TZrUInt32 expectedElementSize) {
    TZrSize sectionEnd;

    if (section == ZR_NULL) {
        return ZR_FALSE;
    }
    if (section->byteLength == 0u && section->count == 0u && section->elementSize == 0u && section->offset == 0u) {
        return ZR_TRUE;
    }
    if (section->offset < ZR_ZRP_METADATA_HEADER_SIZE || section->offset > bufferLength) {
        return ZR_FALSE;
    }
    if (section->byteLength > (TZrUInt32)(bufferLength - section->offset)) {
        return ZR_FALSE;
    }
    sectionEnd = (TZrSize)section->offset + (TZrSize)section->byteLength;
    if (sectionEnd > bufferLength) {
        return ZR_FALSE;
    }
    if (section->count == 0u) {
        return section->byteLength == 0u && section->elementSize == 0u;
    }
    if (section->elementSize == 0u) {
        return ZR_FALSE;
    }
    if (expectedElementSize != 0u && section->elementSize != expectedElementSize) {
        return ZR_FALSE;
    }
    return ((TZrSize)section->count * (TZrSize)section->elementSize) == (TZrSize)section->byteLength;
}

static TZrBool zrp_metadata_validate_signature_type_node(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 depth);

static TZrBool zrp_metadata_validate_signature_type_list(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 count,
                                                         TZrUInt32 depth) {
    TZrUInt32 index;

    for (index = 0u; index < count; ++index) {
        if (!zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, offset, depth + 1u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zrp_metadata_validate_signature_type_node(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 depth) {
    TZrUInt8 node;
    TZrUInt32 count;

    if (depth > ZR_ZRP_METADATA_SIGNATURE_MAX_RECURSION_DEPTH ||
        !zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, offset, &node)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   zrp_metadata_validate_signature_type_node(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            return zrp_metadata_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count) &&
                   zrp_metadata_validate_signature_type_list(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            count,
                                                            depth);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
            return ZR_FALSE;
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            if (!zrp_metadata_validate_signature_type_node(signatureBlob,
                                                           signatureBlobLength,
                                                           offset,
                                                           depth + 1u) ||
                !zrp_metadata_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return zrp_metadata_validate_signature_type_list(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            count,
                                                            depth);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   zrp_metadata_validate_signature_type_node(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) ||
                !zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) ||
                !zrp_metadata_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return zrp_metadata_validate_signature_type_list(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            count,
                                                            depth);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, offset, depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            return zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            return ZR_FALSE;
    }
}

static TZrBool zrp_metadata_validate_method_signature_blob(const TZrByte *signatureBlob,
                                                           TZrSize signatureBlobLength,
                                                           TZrSize *offset) {
    TZrUInt8 ignoredByte;
    TZrUInt32 parameterCount;
    TZrUInt32 index;

    if (!zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, offset, &ignoredByte) ||
        !zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, offset, &ignoredByte) ||
        !zrp_metadata_signature_skip_u32(signatureBlob, signatureBlobLength, offset) ||
        !zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, offset, 0u) ||
        !zrp_metadata_signature_read_u32(signatureBlob, signatureBlobLength, offset, &parameterCount)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < parameterCount; ++index) {
        if (!zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, offset, &ignoredByte) ||
            !zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, offset, 0u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zrp_metadata_validate_field_signature_blob(const TZrByte *signatureBlob,
                                                          TZrSize signatureBlobLength,
                                                          TZrSize *offset) {
    TZrUInt8 ignoredByte;

    return zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, offset, &ignoredByte) &&
           zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, offset, 0u);
}

ZR_CORE_API void ZrCore_ZrpMetadata_InitHeader(SZrZrpMetadataHeader *header) {
    if (header == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(header, 0, sizeof(*header));
    header->magic = ZR_ZRP_METADATA_MAGIC;
    header->version = ZR_ZRP_METADATA_VERSION;
    header->headerSize = (TZrUInt16)ZR_ZRP_METADATA_HEADER_SIZE;
    header->sectionCount = ZR_ZRP_METADATA_SECTION_COUNT;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateHeader(const SZrZrpMetadataHeader *header, TZrSize bufferLength) {
    if (header == ZR_NULL) {
        return ZR_FALSE;
    }
    if (bufferLength < ZR_ZRP_METADATA_HEADER_SIZE ||
        header->magic != ZR_ZRP_METADATA_MAGIC ||
        header->version != ZR_ZRP_METADATA_VERSION ||
        header->headerSize != ZR_ZRP_METADATA_HEADER_SIZE ||
        header->sectionCount != ZR_ZRP_METADATA_SECTION_COUNT) {
        return ZR_FALSE;
    }

    return zrp_metadata_section_is_valid(&header->tokenRecords,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrMetadataTokenRecord)) &&
           zrp_metadata_section_is_valid(&header->typeDefs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) &&
           zrp_metadata_section_is_valid(&header->methodDefs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)) &&
           zrp_metadata_section_is_valid(&header->fieldDefs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow)) &&
           zrp_metadata_section_is_valid(&header->genericParams,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow)) &&
           zrp_metadata_section_is_valid(&header->genericParamConstraints,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow)) &&
           zrp_metadata_section_is_valid(&header->typeSpecs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow)) &&
           zrp_metadata_section_is_valid(&header->methodSpecs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow)) &&
           zrp_metadata_section_is_valid(&header->moduleRefs,
                                         bufferLength,
                                         (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow)) &&
           zrp_metadata_section_is_valid(&header->stringPool, bufferLength, 1u) &&
           zrp_metadata_section_is_valid(&header->signatureBlobPool, bufferLength, 1u) &&
           zrp_metadata_section_is_valid(&header->constantPool, bufferLength, 1u);
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteHeader(TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   const SZrZrpMetadataHeader *header) {
    TZrSize sectionOffset;

    if (buffer == ZR_NULL ||
        !ZrCore_ZrpMetadata_ValidateHeader(header, bufferLength)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(buffer, 0, ZR_ZRP_METADATA_HEADER_SIZE);
    zrp_metadata_write_u32(buffer, 0u, header->magic);
    zrp_metadata_write_u16(buffer, 4u, header->version);
    zrp_metadata_write_u16(buffer, 6u, header->headerSize);
    zrp_metadata_write_u32(buffer, 8u, header->flags);
    zrp_metadata_write_u32(buffer, 12u, header->sectionCount);

    sectionOffset = 16u;
    zrp_metadata_write_section(buffer, sectionOffset, &header->tokenRecords);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->typeDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->methodDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->fieldDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->genericParams);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->genericParamConstraints);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->typeSpecs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->methodSpecs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->moduleRefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->stringPool);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->signatureBlobPool);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_write_section(buffer, sectionOffset, &header->constantPool);
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ReadHeader(const TZrByte *buffer,
                                                  TZrSize bufferLength,
                                                  SZrZrpMetadataHeader *outHeader) {
    TZrSize sectionOffset;

    if (buffer == ZR_NULL || outHeader == ZR_NULL || bufferLength < ZR_ZRP_METADATA_HEADER_SIZE) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outHeader, 0, sizeof(*outHeader));
    outHeader->magic = zrp_metadata_read_u32(buffer, 0u);
    outHeader->version = zrp_metadata_read_u16(buffer, 4u);
    outHeader->headerSize = zrp_metadata_read_u16(buffer, 6u);
    outHeader->flags = zrp_metadata_read_u32(buffer, 8u);
    outHeader->sectionCount = zrp_metadata_read_u32(buffer, 12u);

    sectionOffset = 16u;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->tokenRecords);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->typeDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->methodDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->fieldDefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->genericParams);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->genericParamConstraints);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->typeSpecs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->methodSpecs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->moduleRefs);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->stringPool);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->signatureBlobPool);
    sectionOffset += ZR_ZRP_METADATA_SECTION_ENCODED_SIZE;
    zrp_metadata_read_section(buffer, sectionOffset, &outHeader->constantPool);
    return ZrCore_ZrpMetadata_ValidateHeader(outHeader, bufferLength);
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetSectionView(const TZrByte *buffer,
                                                      TZrSize bufferLength,
                                                      const SZrZrpMetadataHeader *header,
                                                      EZrZrpMetadataSectionKind sectionKind,
                                                      SZrZrpMetadataSectionView *outView) {
    const SZrZrpMetadataSection *section;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (buffer == ZR_NULL ||
        outView == ZR_NULL ||
        !ZrCore_ZrpMetadata_ValidateHeader(header, bufferLength)) {
        return ZR_FALSE;
    }

    section = zrp_metadata_get_section(header, sectionKind);
    if (section == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->section = section;
    outView->byteLength = section->byteLength;
    outView->count = section->count;
    outView->elementSize = section->elementSize;
    if (section->byteLength == 0u) {
        return ZR_TRUE;
    }

    if (section->offset > bufferLength || section->byteLength > (TZrUInt32)(bufferLength - section->offset)) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
        return ZR_FALSE;
    }
    outView->data = buffer + section->offset;
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetPoolSlice(const TZrByte *buffer,
                                                    TZrSize bufferLength,
                                                    const SZrZrpMetadataHeader *header,
                                                    EZrZrpMetadataSectionKind poolKind,
                                                    TZrUInt32 offset,
                                                    TZrUInt32 byteLength,
                                                    SZrZrpMetadataPoolSliceView *outSlice) {
    SZrZrpMetadataSectionView poolView;

    if (outSlice != ZR_NULL) {
        ZrCore_Memory_RawSet(outSlice, 0, sizeof(*outSlice));
    }
    if (outSlice == ZR_NULL || !zrp_metadata_section_kind_is_pool(poolKind)) {
        return ZR_FALSE;
    }
    if (!ZrCore_ZrpMetadata_GetSectionView(buffer, bufferLength, header, poolKind, &poolView)) {
        return ZR_FALSE;
    }
    if (offset > poolView.byteLength || byteLength > (TZrUInt32)(poolView.byteLength - offset)) {
        return ZR_FALSE;
    }

    outSlice->data = buffer + poolView.section->offset + offset;
    outSlice->byteLength = byteLength;
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetString(const TZrByte *buffer,
                                                 TZrSize bufferLength,
                                                 const SZrZrpMetadataHeader *header,
                                                 TZrUInt32 stringOffset,
                                                 SZrZrpMetadataStringView *outString) {
    SZrZrpMetadataSectionView poolView;
    TZrUInt32 index;

    if (outString != ZR_NULL) {
        ZrCore_Memory_RawSet(outString, 0, sizeof(*outString));
    }
    if (outString == ZR_NULL ||
        !ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                           &poolView) ||
        stringOffset >= poolView.byteLength) {
        return ZR_FALSE;
    }

    for (index = stringOffset; index < poolView.byteLength; ++index) {
        if (poolView.data[index] == 0u) {
            outString->data = (const char *)(const void *)(poolView.data + stringOffset);
            outString->byteLength = (TZrSize)(index - stringOffset);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateSignatureBlob(const TZrByte *signatureBlob,
                                                             TZrSize signatureBlobLength) {
    TZrSize offset;
    TZrUInt8 rootNode;
    TZrBool valid;

    if (signatureBlob == ZR_NULL || signatureBlobLength == 0u) {
        return ZR_FALSE;
    }

    offset = 0u;
    if (!zrp_metadata_signature_read_u8(signatureBlob, signatureBlobLength, &offset, &rootNode)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)rootNode) {
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
            valid = zrp_metadata_validate_method_signature_blob(signatureBlob, signatureBlobLength, &offset);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
            valid = zrp_metadata_validate_field_signature_blob(signatureBlob, signatureBlobLength, &offset);
            break;
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
        case ZR_METADATA_SIGNATURE_NODE_UNION:
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            offset = 0u;
            valid = zrp_metadata_validate_signature_type_node(signatureBlob, signatureBlobLength, &offset, 0u);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            valid = ZR_FALSE;
            break;
    }

    return valid && offset == signatureBlobLength;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WritePoolPayload(TZrByte *buffer,
                                                        TZrSize bufferLength,
                                                        const SZrZrpMetadataHeader *header,
                                                        EZrZrpMetadataSectionKind poolKind,
                                                        const TZrByte *payload,
                                                        TZrUInt32 payloadLength) {
    const SZrZrpMetadataSection *section;

    if (buffer == ZR_NULL ||
        !zrp_metadata_section_kind_is_pool(poolKind) ||
        (payloadLength != 0u && payload == ZR_NULL) ||
        !ZrCore_ZrpMetadata_ValidateHeader(header, bufferLength)) {
        return ZR_FALSE;
    }

    section = zrp_metadata_get_section(header, poolKind);
    if (section == ZR_NULL ||
        section->count != payloadLength ||
        section->byteLength != payloadLength ||
        (payloadLength == 0u && section->elementSize != 0u) ||
        (payloadLength != 0u && section->elementSize != 1u) ||
        section->offset > bufferLength ||
        section->byteLength > (TZrUInt32)(bufferLength - section->offset)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(buffer + section->offset, (TZrPtr)payload, payloadLength);
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteDefinitionTablePayload(TZrByte *buffer,
                                                                   TZrSize bufferLength,
                                                                   const SZrZrpMetadataHeader *header,
                                                                   EZrZrpMetadataSectionKind tableKind,
                                                                   const void *rows,
                                                                   TZrUInt32 rowCount,
                                                                   TZrUInt32 elementSize) {
    const SZrZrpMetadataSection *section;
    TZrUInt32 expectedElementSize;
    TZrSize byteLength;

    expectedElementSize = zrp_metadata_definition_table_element_size(tableKind);
    if (buffer == ZR_NULL ||
        expectedElementSize == 0u ||
        elementSize != expectedElementSize ||
        (rowCount != 0u && rows == ZR_NULL) ||
        !ZrCore_ZrpMetadata_ValidateHeader(header, bufferLength)) {
        return ZR_FALSE;
    }

    byteLength = (TZrSize)rowCount * (TZrSize)elementSize;
    if (byteLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    section = zrp_metadata_get_section(header, tableKind);
    if (section == ZR_NULL ||
        section->count != rowCount ||
        section->byteLength != (TZrUInt32)byteLength ||
        (rowCount == 0u && section->elementSize != 0u) ||
        (rowCount != 0u && section->elementSize != elementSize) ||
        section->offset > bufferLength ||
        section->byteLength > (TZrUInt32)(bufferLength - section->offset)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(buffer + section->offset, (TZrPtr)rows, byteLength);
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateDefinitionTables(const TZrByte *buffer,
                                                                TZrSize bufferLength,
                                                                const SZrZrpMetadataHeader *header) {
    SZrZrpMetadataSectionView view;
    TZrUInt32 typeDefCount;
    TZrUInt32 methodDefCount;
    TZrUInt32 fieldDefCount;
    TZrUInt32 genericParamCount;
    TZrUInt32 index;

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataTypeDefRow *row = &((const SZrZrpMetadataTypeDefRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_TYPE_DEF)) {
            return ZR_FALSE;
        }
    }
    typeDefCount = view.count;

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataMethodDefRow *row =
                &((const SZrZrpMetadataMethodDefRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_MEMBER_DEF) ||
            !zrp_metadata_token_has_table(row->ownerTypeToken, ZR_METADATA_TABLE_TYPE_DEF) ||
            !zrp_metadata_rid_is_in_count(row->ownerTypeToken, typeDefCount)) {
            return ZR_FALSE;
        }
    }
    methodDefCount = view.count;

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataFieldDefRow *row = &((const SZrZrpMetadataFieldDefRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_MEMBER_DEF) ||
            !zrp_metadata_token_has_table(row->ownerTypeToken, ZR_METADATA_TABLE_TYPE_DEF) ||
            !zrp_metadata_rid_is_in_count(row->ownerTypeToken, typeDefCount)) {
            return ZR_FALSE;
        }
    }
    fieldDefCount = view.count;

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataGenericParamRow *row =
                &((const SZrZrpMetadataGenericParamRow *)(const void *)view.data)[index];
        if (zrp_metadata_token_has_table(row->ownerToken, ZR_METADATA_TABLE_TYPE_DEF)) {
            if (!zrp_metadata_rid_is_in_count(row->ownerToken, typeDefCount)) {
                return ZR_FALSE;
            }
        } else if (zrp_metadata_token_has_table(row->ownerToken, ZR_METADATA_TABLE_MEMBER_DEF)) {
            if (!zrp_metadata_rid_is_in_count(row->ownerToken, methodDefCount + fieldDefCount)) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    }
    genericParamCount = view.count;

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataGenericParamConstraintRow *row =
                &((const SZrZrpMetadataGenericParamConstraintRow *)(const void *)view.data)[index];
        if (row->genericParamIndex >= genericParamCount ||
            !zrp_metadata_token_is_type_reference(row->constraintTypeToken)) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataTypeSpecRow *row = &((const SZrZrpMetadataTypeSpecRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_TYPE_SPEC)) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_METHOD_SPECS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataMethodSpecRow *row =
                &((const SZrZrpMetadataMethodSpecRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_SIGNATURE) ||
            !zrp_metadata_token_is_method_reference(row->methodToken)) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_MODULE_REFS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataModuleRefRow *row =
                &((const SZrZrpMetadataModuleRefRow *)(const void *)view.data)[index];
        if (!zrp_metadata_token_has_table(row->token, ZR_METADATA_TABLE_ASSEMBLY_REF)) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_ZrpMetadata_GetSectionView(buffer,
                                           bufferLength,
                                           header,
                                           ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                           &view)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < view.count; ++index) {
        const SZrZrpMetadataTypeDefRow *row = &((const SZrZrpMetadataTypeDefRow *)(const void *)view.data)[index];
        if (!zrp_metadata_range_is_in_count(row->firstMethodDefIndex, row->methodDefCount, methodDefCount) ||
            !zrp_metadata_range_is_in_count(row->firstFieldDefIndex, row->fieldDefCount, fieldDefCount) ||
            !zrp_metadata_range_is_in_count(row->firstGenericParamIndex,
                                            row->genericParamCount,
                                            genericParamCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}
