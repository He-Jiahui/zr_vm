#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIGNATURE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIGNATURE_H

#include "backend_aot_function_table.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

typedef struct SZrAotCZrpSignatureBlobRemapEntry {
    TZrUInt32 oldOffset;
    TZrUInt32 byteLength;
    TZrUInt32 newOffset;
} SZrAotCZrpSignatureBlobRemapEntry;

typedef struct SZrAotCZrpSignatureBlobRemap {
    SZrAotCZrpSignatureBlobRemapEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 byteLength;
    TZrUInt32 sourceByteLength;
} SZrAotCZrpSignatureBlobRemap;

TZrBool backend_aot_c_zrp_signature_blob_remap_init(SZrAotCZrpSignatureBlobRemap *remap,
                                                    TZrUInt32 capacity,
                                                    TZrUInt32 sourceByteLength);
void backend_aot_c_zrp_signature_blob_remap_destroy(SZrAotCZrpSignatureBlobRemap *remap);

TZrBool backend_aot_c_zrp_build_signature_blob_remap(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraints,
        TZrUInt32 genericParamConstraintCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrZrpMetadataMethodSpecRow *methodSpecRows,
        TZrUInt32 methodSpecCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);

void backend_aot_c_zrp_copy_signature_blob_pool(TZrByte *targetBlob,
                                                const TZrByte *sourceBlob,
                                                const SZrZrpMetadataHeader *sourceHeader,
                                                const SZrZrpMetadataHeader *targetHeader,
                                                const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrBool backend_aot_c_zrp_rewrite_retained_method_spec_signature_blobs(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataMethodSpecRow *methodSpecRows,
        TZrUInt32 methodSpecCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrBool backend_aot_c_zrp_remap_signature_blob_offset(TZrUInt32 *signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength,
                                                      const SZrAotCZrpSignatureBlobRemap *remap);

TZrUInt64 backend_aot_c_zrp_recomputed_signature_hash(const TZrByte *targetBlob,
                                                      const SZrZrpMetadataHeader *targetHeader,
                                                      TZrUInt32 signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength);

#endif
