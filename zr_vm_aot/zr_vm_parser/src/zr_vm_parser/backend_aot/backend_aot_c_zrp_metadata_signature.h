#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIGNATURE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIGNATURE_H

#include "backend_aot_function_table.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

/** 保留签名的源片段与压缩签名池位置；相同源片段由构建阶段去重。 */
typedef struct SZrAotCZrpSignatureBlobRemapEntry {
    TZrUInt32 oldOffset;
    TZrUInt32 byteLength;
    TZrUInt32 newOffset;
} SZrAotCZrpSignatureBlobRemapEntry;

/** 裁剪阶段临时持有的签名池布局，供模块引用扫描、原位重写及行哈希更新共用。 */
typedef struct SZrAotCZrpSignatureBlobRemap {
    SZrAotCZrpSignatureBlobRemapEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 byteLength;
    TZrUInt32 sourceByteLength;
} SZrAotCZrpSignatureBlobRemap;

typedef struct SZrAotCZrpStringPoolRemap SZrAotCZrpStringPoolRemap;

TZrBool backend_aot_c_zrp_signature_blob_remap_init(SZrAotCZrpSignatureBlobRemap *remap,
                                                    TZrUInt32 capacity,
                                                    TZrUInt32 sourceByteLength);
void backend_aot_c_zrp_signature_blob_remap_destroy(SZrAotCZrpSignatureBlobRemap *remap);
TZrBool backend_aot_c_zrp_signature_blob_remap_is_identity(const SZrAotCZrpSignatureBlobRemap *remap);

/**
 * @brief 从所有保留记录和定义收集签名，再给后续模块引用保留及签名重写提供共同视图。
 * @pre remap 已按源签名池长度初始化，且保留判定应与写表阶段一致。
 */
TZrBool backend_aot_c_zrp_build_signature_blob_remap(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        TZrUInt32 retainedTypeDefCount,
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
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataMethodSpecRow *methodSpecRows,
        TZrUInt32 methodSpecCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);

TZrBool backend_aot_c_zrp_remap_retained_signature_token(
        TZrMetadataToken *token,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);

TZrBool backend_aot_c_zrp_remap_retained_signature_tokens_in_record(
        SZrMetadataTokenRecord *record,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
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
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

/** 在复制签名池后原位改写保留签名的类型、模块及字符串引用；写表前完成以便重算哈希。 */
TZrBool backend_aot_c_zrp_rewrite_retained_signature_type_def_tokens(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *sourceSignatureBlobPool,
        TZrUInt32 sourceSignatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrAotCZrpStringPoolRemap *stringRemap);

TZrBool backend_aot_c_zrp_remap_signature_blob_offset(TZrUInt32 *signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength,
                                                      const SZrAotCZrpSignatureBlobRemap *remap);

TZrUInt64 backend_aot_c_zrp_recomputed_signature_hash(const TZrByte *targetBlob,
                                                      const SZrZrpMetadataHeader *targetHeader,
                                                      TZrUInt32 signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength);

#endif
