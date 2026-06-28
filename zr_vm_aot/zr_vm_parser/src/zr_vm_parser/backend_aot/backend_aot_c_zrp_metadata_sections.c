#include "backend_aot_c_zrp_metadata_sections.h"

#include <string.h>

const SZrZrpMetadataSection *backend_aot_c_zrp_metadata_section(
        const SZrZrpMetadataHeader *header,
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

SZrZrpMetadataSection *backend_aot_c_zrp_metadata_mutable_section(
        SZrZrpMetadataHeader *header,
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

void backend_aot_c_zrp_set_section_layout(SZrZrpMetadataSection *section,
                                          TZrUInt32 *offset,
                                          TZrUInt32 byteLength,
                                          TZrUInt32 count,
                                          TZrUInt32 elementSize) {
    if (section == ZR_NULL || offset == ZR_NULL) {
        return;
    }

    if (byteLength == 0u) {
        memset(section, 0, sizeof(*section));
        return;
    }

    section->offset = *offset;
    section->byteLength = byteLength;
    section->count = count;
    section->elementSize = elementSize;
    *offset += byteLength;
}

void backend_aot_c_zrp_copy_section_if_needed(TZrByte *targetBlob,
                                              const TZrByte *sourceBlob,
                                              const SZrZrpMetadataHeader *sourceHeader,
                                              const SZrZrpMetadataHeader *targetHeader,
                                              EZrZrpMetadataSectionKind sectionKind) {
    const SZrZrpMetadataSection *sourceSection = backend_aot_c_zrp_metadata_section(sourceHeader, sectionKind);
    const SZrZrpMetadataSection *targetSection = backend_aot_c_zrp_metadata_section(targetHeader, sectionKind);

    if (targetBlob == ZR_NULL ||
        sourceBlob == ZR_NULL ||
        sourceSection == ZR_NULL ||
        targetSection == ZR_NULL ||
        targetSection->byteLength == 0u) {
        return;
    }

    memcpy(targetBlob + targetSection->offset, sourceBlob + sourceSection->offset, targetSection->byteLength);
}
