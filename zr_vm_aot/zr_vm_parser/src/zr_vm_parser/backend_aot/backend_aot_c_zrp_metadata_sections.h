#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SECTIONS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SECTIONS_H

#include "zr_vm_core/zrp_metadata.h"

const SZrZrpMetadataSection *backend_aot_c_zrp_metadata_section(
        const SZrZrpMetadataHeader *header,
        EZrZrpMetadataSectionKind sectionKind);
SZrZrpMetadataSection *backend_aot_c_zrp_metadata_mutable_section(
        SZrZrpMetadataHeader *header,
        EZrZrpMetadataSectionKind sectionKind);

void backend_aot_c_zrp_set_section_layout(SZrZrpMetadataSection *section,
                                          TZrUInt32 *offset,
                                          TZrUInt32 byteLength,
                                          TZrUInt32 count,
                                          TZrUInt32 elementSize);

void backend_aot_c_zrp_copy_section_if_needed(TZrByte *targetBlob,
                                              const TZrByte *sourceBlob,
                                              const SZrZrpMetadataHeader *sourceHeader,
                                              const SZrZrpMetadataHeader *targetHeader,
                                              EZrZrpMetadataSectionKind sectionKind);

#endif
