#include "backend_aot_c_zrp_metadata_signature.h"

#include "backend_aot_c_zrp_metadata_remap.h"

#include "zr_vm_core/hash.h"

#include <stdlib.h>
#include <string.h>

static const TZrByte CZrAotCZrpSignatureHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        's',
        'i',
        'g',
        '.',
        'v',
        '1',
        '\0',
};

static TZrUInt64 backend_aot_c_zrp_signature_hash_v1(const TZrByte *signatureBlob, TZrUInt32 signatureBlobLength) {
    if (signatureBlob == ZR_NULL || signatureBlobLength == 0u) {
        return 0u;
    }

    return ZrCore_Hash_CreateStable64WithPrefix(CZrAotCZrpSignatureHashV1Prefix,
                                                sizeof(CZrAotCZrpSignatureHashV1Prefix),
                                                signatureBlob,
                                                signatureBlobLength);
}

TZrBool backend_aot_c_zrp_signature_blob_remap_init(SZrAotCZrpSignatureBlobRemap *remap,
                                                    TZrUInt32 capacity,
                                                    TZrUInt32 sourceByteLength) {
    if (remap == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(remap, 0, sizeof(*remap));
    remap->capacity = capacity;
    remap->sourceByteLength = sourceByteLength;
    if (capacity == 0u) {
        return ZR_TRUE;
    }

    remap->entries = (SZrAotCZrpSignatureBlobRemapEntry *)malloc(
            (TZrSize)capacity * sizeof(SZrAotCZrpSignatureBlobRemapEntry));
    if (remap->entries == ZR_NULL) {
        memset(remap, 0, sizeof(*remap));
        return ZR_FALSE;
    }
    memset(remap->entries, 0, (TZrSize)capacity * sizeof(SZrAotCZrpSignatureBlobRemapEntry));
    return ZR_TRUE;
}

void backend_aot_c_zrp_signature_blob_remap_destroy(SZrAotCZrpSignatureBlobRemap *remap) {
    if (remap == ZR_NULL) {
        return;
    }

    if (remap->entries != ZR_NULL) {
        free(remap->entries);
    }
    memset(remap, 0, sizeof(*remap));
}

TZrBool backend_aot_c_zrp_signature_blob_remap_is_identity(const SZrAotCZrpSignatureBlobRemap *remap) {
    if (remap == ZR_NULL || remap->byteLength != remap->sourceByteLength) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset != remap->entries[index].newOffset) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_blob_range_is_valid(const SZrAotCZrpSignatureBlobRemap *remap,
                                                               TZrUInt32 oldOffset,
                                                               TZrUInt32 byteLength) {
    if (byteLength == 0u) {
        return ZR_TRUE;
    }
    if (remap == ZR_NULL || oldOffset > remap->sourceByteLength) {
        return ZR_FALSE;
    }
    return byteLength <= remap->sourceByteLength - oldOffset;
}

static TZrBool backend_aot_c_zrp_signature_blob_remap_add(SZrAotCZrpSignatureBlobRemap *remap,
                                                          TZrUInt32 oldOffset,
                                                          TZrUInt32 byteLength) {
    if (remap == ZR_NULL || !backend_aot_c_zrp_signature_blob_range_is_valid(remap, oldOffset, byteLength)) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset && remap->entries[index].byteLength == byteLength) {
            return ZR_TRUE;
        }
    }
    if (remap->entries == ZR_NULL ||
        remap->count >= remap->capacity ||
        byteLength > (TZrUInt32)(0xFFFFFFFFu - remap->byteLength)) {
        return ZR_FALSE;
    }

    remap->entries[remap->count].oldOffset = oldOffset;
    remap->entries[remap->count].byteLength = byteLength;
    remap->entries[remap->count].newOffset = remap->byteLength;
    remap->count++;
    remap->byteLength += byteLength;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_blob_remap_lookup(const SZrAotCZrpSignatureBlobRemap *remap,
                                                             TZrUInt32 oldOffset,
                                                             TZrUInt32 byteLength,
                                                             TZrUInt32 *outNewOffset) {
    if (outNewOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        *outNewOffset = 0u;
        return ZR_TRUE;
    }
    if (remap == ZR_NULL || remap->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset && remap->entries[index].byteLength == byteLength) {
            *outNewOffset = remap->entries[index].newOffset;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_remap_signature_blob_offset(TZrUInt32 *signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength,
                                                      const SZrAotCZrpSignatureBlobRemap *remap) {
    TZrUInt32 newOffset;

    if (signatureBlobOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_signature_blob_remap_lookup(remap,
                                                       *signatureBlobOffset,
                                                       signatureBlobLength,
                                                       &newOffset)) {
        return ZR_FALSE;
    }

    *signatureBlobOffset = newOffset;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_retained_token_record_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (tokenRecords == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
        SZrMetadataTokenRecord record = tokenRecords[index];
        if (!backend_aot_c_zrp_remap_token_record(&record,
                                                  methodRows,
                                                  methodCount,
                                                  fieldRows,
                                                  fieldCount,
                                                  functionTable,
                                                  retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        record.signatureBlobOffset,
                                                        record.signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_type_def_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataTypeDefRow *rows,
        TZrUInt32 count) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        rows[index].signatureBlobOffset,
                                                        rows[index].signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_retained_method_def_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataMethodDefRow *rows,
        TZrUInt32 count,
        const SZrAotFunctionTable *functionTable) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_method_def_row_is_retained(&rows[index], functionTable)) {
            continue;
        }
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        rows[index].signatureBlobOffset,
                                                        rows[index].signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_field_def_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataFieldDefRow *rows,
        TZrUInt32 count) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        rows[index].signatureBlobOffset,
                                                        rows[index].signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_retained_generic_param_constraint_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
        TZrUInt32 constraintCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (constraintRows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < constraintCount; index++) {
        SZrZrpMetadataGenericParamConstraintRow row = constraintRows[index];
        if (!backend_aot_c_zrp_remap_generic_param_constraint_row(&row,
                                                                  genericParamRows,
                                                                  genericParamCount,
                                                                  methodRows,
                                                                  methodCount,
                                                                  fieldRows,
                                                                  fieldCount,
                                                                  functionTable,
                                                                  retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        row.signatureBlobOffset,
                                                        row.signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_type_spec_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataTypeSpecRow *rows,
        TZrUInt32 count) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        rows[index].signatureBlobOffset,
                                                        rows[index].signatureBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_retained_method_spec_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataMethodSpecRow *rows,
        TZrUInt32 count,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrZrpMetadataMethodSpecRow row = rows[index];
        if (!backend_aot_c_zrp_remap_method_spec_row(&row,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     functionTable,
                                                     retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
                                                        row.instantiationBlobOffset,
                                                        row.instantiationBlobLength)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

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
        TZrUInt32 retainedMethodDefCount) {
    return backend_aot_c_zrp_add_retained_token_record_signature_blobs(signatureRemap,
                                                                       tokenRecords,
                                                                       tokenRecordCount,
                                                                       methodRows,
                                                                       methodCount,
                                                                       fieldRows,
                                                                       fieldCount,
                                                                       functionTable,
                                                                       retainedMethodDefCount) &&
           backend_aot_c_zrp_add_type_def_signature_blobs(signatureRemap, typeRows, typeCount) &&
           backend_aot_c_zrp_add_retained_method_def_signature_blobs(signatureRemap,
                                                                     methodRows,
                                                                     methodCount,
                                                                     functionTable) &&
           backend_aot_c_zrp_add_field_def_signature_blobs(signatureRemap, fieldRows, fieldCount) &&
           backend_aot_c_zrp_add_retained_generic_param_constraint_signature_blobs(signatureRemap,
                                                                                  genericParamConstraints,
                                                                                  genericParamConstraintCount,
                                                                                  genericParamRows,
                                                                                  genericParamCount,
                                                                                  methodRows,
                                                                                  methodCount,
                                                                                  fieldRows,
                                                                                  fieldCount,
                                                                                  functionTable,
                                                                                  retainedMethodDefCount) &&
           backend_aot_c_zrp_add_type_spec_signature_blobs(signatureRemap, typeSpecRows, typeSpecCount) &&
           backend_aot_c_zrp_add_retained_method_spec_signature_blobs(signatureRemap,
                                                                      methodSpecRows,
                                                                      methodSpecCount,
                                                                      methodRows,
                                                                      methodCount,
                                                                      fieldRows,
                                                                      fieldCount,
                                                                      functionTable,
                                                                      retainedMethodDefCount);
}

void backend_aot_c_zrp_copy_signature_blob_pool(TZrByte *targetBlob,
                                                const TZrByte *sourceBlob,
                                                const SZrZrpMetadataHeader *sourceHeader,
                                                const SZrZrpMetadataHeader *targetHeader,
                                                const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrByte *targetPool;
    const TZrByte *sourcePool;

    if (targetHeader->signatureBlobPool.byteLength == 0u || signatureRemap == ZR_NULL) {
        return;
    }

    targetPool = targetBlob + targetHeader->signatureBlobPool.offset;
    sourcePool = sourceBlob + sourceHeader->signatureBlobPool.offset;
    for (TZrUInt32 index = 0u; index < signatureRemap->count; index++) {
        const SZrAotCZrpSignatureBlobRemapEntry *entry = &signatureRemap->entries[index];
        memcpy(targetPool + entry->newOffset, sourcePool + entry->oldOffset, entry->byteLength);
    }
}

static TZrBool backend_aot_c_zrp_write_u32_to_signature(TZrByte *signatureBlob,
                                                        TZrUInt32 signatureBlobLength,
                                                        TZrUInt32 offset,
                                                        TZrUInt32 value) {
    if (signatureBlob == ZR_NULL || offset > signatureBlobLength || signatureBlobLength - offset < 4u) {
        return ZR_FALSE;
    }

    signatureBlob[offset] = (TZrByte)(value & 0xFFu);
    signatureBlob[offset + 1u] = (TZrByte)((value >> 8u) & 0xFFu);
    signatureBlob[offset + 2u] = (TZrByte)((value >> 16u) & 0xFFu);
    signatureBlob[offset + 3u] = (TZrByte)((value >> 24u) & 0xFFu);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_rewrite_method_spec_signature_blob(TZrByte *signatureBlob,
                                                                    TZrUInt32 signatureBlobLength,
                                                                    TZrMetadataToken methodToken) {
    if (signatureBlobLength < 6u ||
        signatureBlob == ZR_NULL ||
        signatureBlob[0] != (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
        signatureBlob[1] != (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_write_u32_to_signature(signatureBlob, signatureBlobLength, 2u, methodToken) &&
           ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlob, signatureBlobLength);
}

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
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrByte *targetPool;

    if (targetHeader->signatureBlobPool.byteLength == 0u || methodSpecRows == ZR_NULL) {
        return ZR_TRUE;
    }

    targetPool = targetBlob + targetHeader->signatureBlobPool.offset;
    for (TZrUInt32 readIndex = 0u; readIndex < methodSpecCount; readIndex++) {
        SZrZrpMetadataMethodSpecRow row = methodSpecRows[readIndex];
        if (!backend_aot_c_zrp_remap_method_spec_row(&row,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     functionTable,
                                                     retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&row.instantiationBlobOffset,
                                                           row.instantiationBlobLength,
                                                           signatureRemap) ||
            row.instantiationBlobOffset > targetHeader->signatureBlobPool.byteLength ||
            row.instantiationBlobLength >
                    targetHeader->signatureBlobPool.byteLength - row.instantiationBlobOffset ||
            !backend_aot_c_zrp_rewrite_method_spec_signature_blob(targetPool + row.instantiationBlobOffset,
                                                                  row.instantiationBlobLength,
                                                                  row.methodToken)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrUInt64 backend_aot_c_zrp_recomputed_signature_hash(const TZrByte *targetBlob,
                                                      const SZrZrpMetadataHeader *targetHeader,
                                                      TZrUInt32 signatureBlobOffset,
                                                      TZrUInt32 signatureBlobLength) {
    if (signatureBlobLength == 0u ||
        targetHeader->signatureBlobPool.byteLength == 0u ||
        signatureBlobOffset > targetHeader->signatureBlobPool.byteLength ||
        signatureBlobLength > targetHeader->signatureBlobPool.byteLength - signatureBlobOffset) {
        return 0u;
    }

    return backend_aot_c_zrp_signature_hash_v1(targetBlob +
                                                      targetHeader->signatureBlobPool.offset +
                                                      signatureBlobOffset,
                                              signatureBlobLength);
}
