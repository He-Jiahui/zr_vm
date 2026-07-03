#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_METADATA_ROOTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_METADATA_ROOTS_H

#include "zr_vm_core/metadata_token.h"

#define ZR_AOT_C_TYPE_LAYOUT_METADATA_ROOT_CAPACITY 2u

typedef struct SZrAotCTypeLayoutMetadataRoots {
    TZrUInt32 typeLayoutIds[ZR_AOT_C_TYPE_LAYOUT_METADATA_ROOT_CAPACITY];
    TZrUInt32 count;
} SZrAotCTypeLayoutMetadataRoots;

TZrBool backend_aot_c_type_layout_metadata_type_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots);

TZrBool backend_aot_c_type_layout_metadata_field_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken fieldToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots);

#endif
