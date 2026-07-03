#include "backend_aot_c_zrp_metadata_signature.h"

#include "backend_aot_c_zrp_metadata_module_ref.h"
#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_type_def.h"
#include "backend_aot_c_zrp_metadata_type_spec.h"
#include "backend_aot_c_zrp_metadata_string_pool.h"

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

static TZrBool backend_aot_c_zrp_token_record_is_retained_for_signature_pruning(
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
        TZrUInt32 retainedMethodDefCount) {
    return record != ZR_NULL &&
           backend_aot_c_zrp_remap_retained_token_record(record,
                                                         methodRows,
                                                         methodCount,
                                                         fieldRows,
                                                         fieldCount,
                                                         typeRows,
                                                         typeCount,
                                                         tokenRecords,
                                                         tokenRecordCount,
                                                         genericParamRows,
                                                         genericParamCount,
                                                         genericParamConstraintRows,
                                                         genericParamConstraintCount,
                                                         functionTable,
                                                         retainedMethodDefCount) &&
           backend_aot_c_zrp_remap_type_def_tokens_in_record(record,
                                                             typeRows,
                                                             typeCount,
                                                             tokenRecords,
                                                             tokenRecordCount,
                                                             methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              genericParamRows,
                                                             genericParamCount,
                                                             genericParamConstraintRows,
                                                             genericParamConstraintCount,
                                                             functionTable,
                                                             retainedMethodDefCount) &&
           backend_aot_c_zrp_remap_type_spec_tokens_in_record(record,
                                                              typeSpecRows,
                                                              typeSpecCount,
                                                              tokenRecords,
                                                              tokenRecordCount,
                                                               methodRows,
                                                               methodCount,
                                                               fieldRows,
                                                               fieldCount,
                                                               typeRows,
                                                               typeCount,
                                                               genericParamRows,
                                                               genericParamCount,
                                                               genericParamConstraintRows,
                                                               genericParamConstraintCount,
                                                               functionTable,
                                                               retainedMethodDefCount) &&
           backend_aot_c_zrp_remap_module_ref_tokens_in_record(record,
                                                               moduleRefRows,
                                                               moduleRefCount,
                                                               tokenRecords,
                                                               tokenRecordCount,
                                                               typeSpecRows,
                                                               typeSpecCount,
                                                               typeRows,
                                                               typeCount,
                                                               methodRows,
                                                               methodCount,
                                                               fieldRows,
                                                               fieldCount,
                                                               genericParamRows,
                                                               genericParamCount,
                                                               genericParamConstraintRows,
                                                               genericParamConstraintCount,
                                                               functionTable,
                                                               retainedMethodDefCount,
                                                               ZR_NULL,
                                                               0u,
                                                               ZR_NULL);
}

static TZrBool backend_aot_c_zrp_token_is_signature(TZrMetadataToken token) {
    return ZR_METADATA_TOKEN_TABLE(token) == (TZrUInt32)ZR_METADATA_TABLE_SIGNATURE &&
           ZR_METADATA_TOKEN_RID(token) != 0u;
}

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
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedSignatureRid = 0u;

    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_token_is_signature(*token)) {
        return ZR_TRUE;
    }
    if (tokenRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
        SZrMetadataTokenRecord record = tokenRecords[index];
        if (!backend_aot_c_zrp_token_record_is_retained_for_signature_pruning(&record,
                                                                              tokenRecords,
                                                                              tokenRecordCount,
                                                                              typeRows,
                                                                              typeCount,
                                                                              typeSpecRows,
                                                                              typeSpecCount,
                                                                              moduleRefRows,
                                                                              moduleRefCount,
                                                                              methodRows,
                                                                              methodCount,
                                                                              fieldRows,
                                                                              fieldCount,
                                                                              genericParamRows,
                                                                              genericParamCount,
                                                                              genericParamConstraintRows,
                                                                              genericParamConstraintCount,
                                                                              functionTable,
                                                                              retainedMethodDefCount) ||
            !backend_aot_c_zrp_token_is_signature(record.token)) {
            continue;
        }

        retainedSignatureRid++;
        if (record.token == *token) {
            *token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, retainedSignatureRid);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

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
        TZrUInt32 retainedMethodDefCount) {
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

#define ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(FIELD)                                                                 \
    do {                                                                                                             \
        if (!backend_aot_c_zrp_remap_retained_signature_token(&(record->FIELD),                                      \
                                                              tokenRecords,                                          \
                                                              tokenRecordCount,                                      \
                                                              typeRows,                                              \
                                                              typeCount,                                             \
                                                              typeSpecRows,                                          \
                                                              typeSpecCount,                                         \
                                                              moduleRefRows,                                         \
                                                              moduleRefCount,                                        \
                                                              methodRows,                                            \
                                                              methodCount,                                           \
                                                              fieldRows,                                             \
                                                              fieldCount,                                            \
                                                              genericParamRows,                                      \
                                                              genericParamCount,                                     \
                                                              genericParamConstraintRows,                           \
                                                              genericParamConstraintCount,                          \
                                                              functionTable,                                         \
                                                              retainedMethodDefCount)) {                            \
            return ZR_FALSE;                                                                                         \
        }                                                                                                            \
    } while (0)

    ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(token);
    ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(relatedToken);
    ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(ownerToken);
    ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(targetMetadataToken);
    ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD(targetSignatureToken);

#undef ZR_AOT_REMAP_RETAINED_SIGNATURE_FIELD

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_retained_token_record_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
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
        TZrUInt32 retainedMethodDefCount) {
    if (tokenRecords == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
        SZrMetadataTokenRecord record = tokenRecords[index];
        if (!backend_aot_c_zrp_token_record_is_retained_for_signature_pruning(&record,
                                                                              tokenRecords,
                                                                              tokenRecordCount,
                                                                              typeRows,
                                                                              typeCount,
                                                                              typeSpecRows,
                                                                              typeSpecCount,
                                                                              moduleRefRows,
                                                                              moduleRefCount,
                                                                              methodRows,
                                                                              methodCount,
                                                                              fieldRows,
                                                                              fieldCount,
                                                                              genericParamRows,
                                                                              genericParamCount,
                                                                              genericParamConstraintRows,
                                                                              genericParamConstraintCount,
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
        TZrUInt32 count,
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
        TZrUInt32 retainedMethodDefCount) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_type_def_row_is_retained(&rows[index],
                                                        rows,
                                                        count,
                                                        tokenRecords,
                                                        tokenRecordCount,
                                                        methodRows,
                                                        methodCount,
                                                        fieldRows,
                                                        fieldCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        genericParamConstraintRows,
                                                        genericParamConstraintCount,
                                                        functionTable,
                                                        retainedMethodDefCount)) {
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
        TZrUInt32 count,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&rows[index],
                                                         typeRows,
                                                         typeCount,
                                                         tokenRecords,
                                                         tokenRecordCount,
                                                         methodRows,
                                                         methodCount,
                                                         rows,
                                                         count,
                                                         genericParamRows,
                                                         genericParamCount,
                                                         genericParamConstraintRows,
                                                         genericParamConstraintCount,
                                                         functionTable,
                                                         retainedMethodDefCount)) {
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

static TZrBool backend_aot_c_zrp_add_retained_generic_param_constraint_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
        TZrUInt32 constraintCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
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
                                                                  typeRows,
                                                                  typeCount,
                                                                  tokenRecords,
                                                                  tokenRecordCount,
                                                                  constraintRows,
                                                                  constraintCount,
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

static TZrBool backend_aot_c_zrp_add_retained_type_spec_signature_blobs(
        SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrZrpMetadataTypeSpecRow *rows,
        TZrUInt32 count,
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
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (rows == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_type_spec_row_is_retained(&rows[index],
                                                        tokenRecords,
                                                        tokenRecordCount,
                                                        methodRows,
                                                        methodCount,
                                                        fieldRows,
                                                        fieldCount,
                                                        typeRows,
                                                        typeCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        genericParamConstraintRows,
                                                        genericParamConstraintCount,
                                                        functionTable,
                                                        retainedMethodDefCount) &&
            !backend_aot_c_zrp_signature_blob_remap_add(signatureRemap,
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
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
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
                                                     typeRows,
                                                     typeCount,
                                                     tokenRecords,
                                                     tokenRecordCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     genericParamConstraintRows,
                                                     genericParamConstraintCount,
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
        TZrUInt32 retainedMethodDefCount) {
    (void)retainedTypeDefCount;

    return backend_aot_c_zrp_add_retained_token_record_signature_blobs(signatureRemap,
                                                                       tokenRecords,
                                                                       tokenRecordCount,
                                                                       typeRows,
                                                                       typeCount,
                                                                       typeSpecRows,
                                                                       typeSpecCount,
                                                                       moduleRefRows,
                                                                       moduleRefCount,
                                                                       methodRows,
                                                                       methodCount,
                                                                       fieldRows,
                                                                       fieldCount,
                                                                       genericParamRows,
                                                                       genericParamCount,
                                                                       genericParamConstraints,
                                                                       genericParamConstraintCount,
                                                                       functionTable,
                                                                       retainedMethodDefCount) &&
           backend_aot_c_zrp_add_type_def_signature_blobs(signatureRemap,
                                                          typeRows,
                                                          typeCount,
                                                          tokenRecords,
                                                          tokenRecordCount,
                                                          methodRows,
                                                          methodCount,
                                                          fieldRows,
                                                          fieldCount,
                                                          genericParamRows,
                                                          genericParamCount,
                                                          genericParamConstraints,
                                                          genericParamConstraintCount,
                                                          functionTable,
                                                          retainedMethodDefCount) &&
           backend_aot_c_zrp_add_retained_method_def_signature_blobs(signatureRemap,
                                                                      methodRows,
                                                                     methodCount,
                                                                     functionTable) &&
           backend_aot_c_zrp_add_field_def_signature_blobs(signatureRemap,
                                                           fieldRows,
                                                           fieldCount,
                                                           typeRows,
                                                           typeCount,
                                                           tokenRecords,
                                                           tokenRecordCount,
                                                           methodRows,
                                                           methodCount,
                                                           genericParamRows,
                                                           genericParamCount,
                                                           genericParamConstraints,
                                                           genericParamConstraintCount,
                                                           functionTable,
                                                           retainedMethodDefCount) &&
           backend_aot_c_zrp_add_retained_generic_param_constraint_signature_blobs(signatureRemap,
                                                                   genericParamConstraints,
                                                                   genericParamConstraintCount,
                                                                   genericParamRows,
                                                                   genericParamCount,
                                                                   typeRows,
                                                                   typeCount,
                                                                   tokenRecords,
                                                                   tokenRecordCount,
                                                                   methodRows,
                                                                    methodCount,
                                                                    fieldRows,
                                                                      fieldCount,
                                                                      functionTable,
                                                                      retainedMethodDefCount) &&
           backend_aot_c_zrp_add_retained_type_spec_signature_blobs(signatureRemap,
                                                                    typeSpecRows,
                                                              typeSpecCount,
                                                              tokenRecords,
                                                              tokenRecordCount,
                                                              typeRows,
                                                              typeCount,
                                                              methodRows,
                                                                     methodCount,
                                                                     fieldRows,
                                                                     fieldCount,
                                                                     genericParamRows,
                                                                     genericParamCount,
                                                                     genericParamConstraints,
                                                                     genericParamConstraintCount,
                                                                     functionTable,
                                                                     retainedMethodDefCount) &&
           backend_aot_c_zrp_add_retained_method_spec_signature_blobs(signatureRemap,
                                                                      methodSpecRows,
                                                                      methodSpecCount,
                                                                      methodRows,
                                                                      methodCount,
                                                                      fieldRows,
                                                                      fieldCount,
                                                                      typeRows,
                                                                      typeCount,
                                                                      tokenRecords,
                                                                      tokenRecordCount,
                                                                      genericParamRows,
                                                                      genericParamCount,
                                                                      genericParamConstraints,
                                                                      genericParamConstraintCount,
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

#define CZrAotCZrpSignatureRewriteMaxRecursionDepth 64u

typedef struct SZrAotCZrpSignatureRewriteContext {
    const SZrZrpMetadataTypeDefRow *typeRows;
    TZrUInt32 typeCount;
    const SZrZrpMetadataTypeSpecRow *typeSpecRows;
    TZrUInt32 typeSpecCount;
    const SZrZrpMetadataModuleRefRow *moduleRefRows;
    TZrUInt32 moduleRefCount;
    const SZrMetadataTokenRecord *tokenRecords;
    TZrUInt32 tokenRecordCount;
    const SZrZrpMetadataMethodDefRow *methodRows;
    TZrUInt32 methodCount;
    const SZrZrpMetadataFieldDefRow *fieldRows;
    TZrUInt32 fieldCount;
    const SZrZrpMetadataGenericParamRow *genericParamRows;
    TZrUInt32 genericParamCount;
    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows;
    TZrUInt32 genericParamConstraintCount;
    const SZrAotFunctionTable *functionTable;
    TZrUInt32 retainedMethodDefCount;
    const TZrByte *sourceSignatureBlobPool;
    TZrUInt32 sourceSignatureBlobPoolBytes;
    const SZrAotCZrpSignatureBlobRemap *signatureRemap;
    const SZrAotCZrpStringPoolRemap *stringRemap;
} SZrAotCZrpSignatureRewriteContext;

static TZrBool backend_aot_c_zrp_signature_skip_bytes(TZrUInt32 signatureBlobLength,
                                                      TZrSize *offset,
                                                      TZrUInt32 byteLength) {
    if (offset == ZR_NULL ||
        *offset > (TZrSize)signatureBlobLength ||
        (TZrSize)byteLength > (TZrSize)signatureBlobLength - *offset) {
        return ZR_FALSE;
    }

    *offset += (TZrSize)byteLength;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_skip_u32(TZrUInt32 signatureBlobLength, TZrSize *offset) {
    return backend_aot_c_zrp_signature_skip_bytes(signatureBlobLength, offset, 4u);
}

static TZrBool backend_aot_c_zrp_read_u8_from_signature(const TZrByte *signatureBlob,
                                                        TZrUInt32 signatureBlobLength,
                                                        TZrSize *offset,
                                                        TZrUInt8 *value) {
    if (signatureBlob == ZR_NULL ||
        offset == ZR_NULL ||
        value == ZR_NULL ||
        !backend_aot_c_zrp_signature_skip_bytes(signatureBlobLength, offset, 1u)) {
        return ZR_FALSE;
    }

    *value = signatureBlob[*offset - 1u];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_read_u32_from_signature(const TZrByte *signatureBlob,
                                                         TZrUInt32 signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 *value) {
    TZrSize readOffset;

    if (signatureBlob == ZR_NULL ||
        offset == ZR_NULL ||
        value == ZR_NULL ||
        *offset > (TZrSize)signatureBlobLength ||
        (TZrSize)signatureBlobLength - *offset < 4u) {
        return ZR_FALSE;
    }

    readOffset = *offset;
    *value = ((TZrUInt32)signatureBlob[readOffset]) |
             ((TZrUInt32)signatureBlob[readOffset + 1u] << 8u) |
             ((TZrUInt32)signatureBlob[readOffset + 2u] << 16u) |
             ((TZrUInt32)signatureBlob[readOffset + 3u] << 24u);
    *offset += 4u;
    return ZR_TRUE;
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

static TZrBool backend_aot_c_zrp_rewrite_signature_type_node(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 depth,
        const SZrAotCZrpSignatureRewriteContext *context);

static TZrBool backend_aot_c_zrp_rewrite_signature_type_list(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 count,
        TZrUInt32 depth,
        const SZrAotCZrpSignatureRewriteContext *context) {
    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                           signatureBlobLength,
                                                           offset,
                                                           depth + 1u,
                                                           context)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_rewrite_signature_type_def_token(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize tokenOffset,
        TZrUInt32 tokenValue,
        const SZrAotCZrpSignatureRewriteContext *context) {
    TZrMetadataToken token;

    token = (TZrMetadataToken)tokenValue;
    if (!backend_aot_c_zrp_remap_type_def_token(&token,
                                                context->typeRows,
                                                context->typeCount,
                                                context->tokenRecords,
                                                context->tokenRecordCount,
                                                context->methodRows,
                                                context->methodCount,
                                                context->fieldRows,
                                                context->fieldCount,
                                                context->genericParamRows,
                                                context->genericParamCount,
                                                context->genericParamConstraintRows,
                                                context->genericParamConstraintCount,
                                                context->functionTable,
                                                context->retainedMethodDefCount)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_write_u32_to_signature(signatureBlob,
                                                   signatureBlobLength,
                                                   (TZrUInt32)tokenOffset,
                                                    (TZrUInt32)token);
}

static TZrBool backend_aot_c_zrp_rewrite_signature_module_ref_token(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize tokenOffset,
        TZrUInt32 tokenValue,
        const SZrAotCZrpSignatureRewriteContext *context) {
    TZrMetadataToken token;

    token = (TZrMetadataToken)tokenValue;
    if (!backend_aot_c_zrp_remap_module_ref_token(&token,
                                                  context->moduleRefRows,
                                                  context->moduleRefCount,
                                                  context->tokenRecords,
                                                   context->tokenRecordCount,
                                                   context->typeSpecRows,
                                                   context->typeSpecCount,
                                                   context->typeRows,
                                                   context->typeCount,
                                                   context->methodRows,
                                                  context->methodCount,
                                                  context->fieldRows,
                                                  context->fieldCount,
                                                  context->genericParamRows,
                                                  context->genericParamCount,
                                                  context->genericParamConstraintRows,
                                                  context->genericParamConstraintCount,
                                                  context->functionTable,
                                                  context->retainedMethodDefCount,
                                                  context->sourceSignatureBlobPool,
                                                  context->sourceSignatureBlobPoolBytes,
                                                  context->signatureRemap)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_write_u32_to_signature(signatureBlob,
                                                   signatureBlobLength,
                                                   (TZrUInt32)tokenOffset,
                                                   (TZrUInt32)token);
}

static TZrBool backend_aot_c_zrp_rewrite_signature_member_ref_token(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize tokenOffset,
        TZrUInt32 tokenValue,
        const SZrAotCZrpSignatureRewriteContext *context) {
    SZrMetadataTokenRecord record;

    memset(&record, 0, sizeof(record));
    record.token = (TZrMetadataToken)tokenValue;
    if (!backend_aot_c_zrp_remap_retained_token_record(&record,
                                                       context->methodRows,
                                                       context->methodCount,
                                                       context->fieldRows,
                                                       context->fieldCount,
                                                       context->typeRows,
                                                       context->typeCount,
                                                       context->tokenRecords,
                                                       context->tokenRecordCount,
                                                       context->genericParamRows,
                                                       context->genericParamCount,
                                                       context->genericParamConstraintRows,
                                                       context->genericParamConstraintCount,
                                                       context->functionTable,
                                                       context->retainedMethodDefCount)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_write_u32_to_signature(signatureBlob,
                                                   signatureBlobLength,
                                                   (TZrUInt32)tokenOffset,
                                                   (TZrUInt32)record.token);
}

static TZrBool backend_aot_c_zrp_rewrite_signature_string_offset(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize stringOffsetOffset,
        TZrUInt32 stringOffset,
        const SZrAotCZrpSignatureRewriteContext *context) {
    if (context == ZR_NULL ||
        !backend_aot_c_zrp_remap_string_offset(&stringOffset, context->stringRemap)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_write_u32_to_signature(signatureBlob,
                                                   signatureBlobLength,
                                                   (TZrUInt32)stringOffsetOffset,
                                                   stringOffset);
}

static TZrBool backend_aot_c_zrp_rewrite_signature_type_node(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 depth,
        const SZrAotCZrpSignatureRewriteContext *context) {
    TZrUInt8 node;
    TZrUInt32 count;
    TZrUInt32 tokenValue;
    TZrSize tokenOffset;

    if (context == ZR_NULL ||
        depth > CZrAotCZrpSignatureRewriteMaxRecursionDepth ||
        !backend_aot_c_zrp_read_u8_from_signature(signatureBlob, signatureBlobLength, offset, &node)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
            if (!backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset)) {
                return ZR_FALSE;
            }
            tokenOffset = *offset;
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                             signatureBlobLength,
                                                             offset,
                                                             &tokenValue) &&
                   backend_aot_c_zrp_rewrite_signature_string_offset(signatureBlob,
                                                                     signatureBlobLength,
                                                                     tokenOffset,
                                                                     tokenValue,
                                                                     context);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            if (!backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset)) {
                return ZR_FALSE;
            }
            tokenOffset = *offset;
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                             signatureBlobLength,
                                                             offset,
                                                             &tokenValue) &&
                   backend_aot_c_zrp_rewrite_signature_type_def_token(signatureBlob,
                                                                       signatureBlobLength,
                                                                       tokenOffset,
                                                                       tokenValue,
                                                                       context);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                                 signatureBlobLength,
                                                                 offset,
                                                                 depth + 1u,
                                                                 context);
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob, signatureBlobLength, offset, &count) &&
                   backend_aot_c_zrp_rewrite_signature_type_list(signatureBlob,
                                                                 signatureBlobLength,
                                                                 offset,
                                                                 count,
                                                                 depth,
                                                                 context);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
            return ZR_FALSE;
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            if (!backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                              signatureBlobLength,
                                                              offset,
                                                              depth + 1u,
                                                              context) ||
                !backend_aot_c_zrp_read_u32_from_signature(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_rewrite_signature_type_list(signatureBlob,
                                                                signatureBlobLength,
                                                                offset,
                                                                count,
                                                                depth,
                                                                context);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                                 signatureBlobLength,
                                                                 offset,
                                                                 depth + 1u,
                                                                 context);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset)) {
                return ZR_FALSE;
            }
            tokenOffset = *offset;
            if (!backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                           signatureBlobLength,
                                                           offset,
                                                           &tokenValue) ||
                !backend_aot_c_zrp_rewrite_signature_string_offset(signatureBlob,
                                                                   signatureBlobLength,
                                                                   tokenOffset,
                                                                   tokenValue,
                                                                   context) ||
                !backend_aot_c_zrp_read_u32_from_signature(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_rewrite_signature_type_list(signatureBlob,
                                                                signatureBlobLength,
                                                                offset,
                                                                count,
                                                                depth,
                                                                context);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                                signatureBlobLength,
                                                                offset,
                                                                depth + 1u,
                                                                context);
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
            tokenOffset = *offset;
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                             signatureBlobLength,
                                                             offset,
                                                             &tokenValue) &&
                   backend_aot_c_zrp_rewrite_signature_member_ref_token(signatureBlob,
                                                                        signatureBlobLength,
                                                                        tokenOffset,
                                                                        tokenValue,
                                                                        context);
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            tokenOffset = *offset;
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                             signatureBlobLength,
                                                             offset,
                                                             &tokenValue) &&
                   backend_aot_c_zrp_rewrite_signature_module_ref_token(signatureBlob,
                                                                        signatureBlobLength,
                                                                        tokenOffset,
                                                                       tokenValue,
                                                                       context);
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            tokenOffset = *offset;
            if (!backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                           signatureBlobLength,
                                                           offset,
                                                           &tokenValue) ||
                !backend_aot_c_zrp_rewrite_signature_string_offset(signatureBlob,
                                                                   signatureBlobLength,
                                                                   tokenOffset,
                                                                   tokenValue,
                                                                   context)) {
                return ZR_FALSE;
            }
            tokenOffset = *offset;
            return backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                             signatureBlobLength,
                                                             offset,
                                                             &tokenValue) &&
                   backend_aot_c_zrp_rewrite_signature_string_offset(signatureBlob,
                                                                     signatureBlobLength,
                                                                     tokenOffset,
                                                                     tokenValue,
                                                                     context);
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_zrp_rewrite_method_signature_type_def_tokens(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        const SZrAotCZrpSignatureRewriteContext *context) {
    TZrUInt32 parameterCount;

    if (!backend_aot_c_zrp_signature_skip_bytes(signatureBlobLength, offset, 2u) ||
        !backend_aot_c_zrp_signature_skip_u32(signatureBlobLength, offset) ||
        !backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob, signatureBlobLength, offset, 0u, context) ||
        !backend_aot_c_zrp_read_u32_from_signature(signatureBlob,
                                                   signatureBlobLength,
                                                   offset,
                                                   &parameterCount)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < parameterCount; index++) {
        if (!backend_aot_c_zrp_signature_skip_bytes(signatureBlobLength, offset, 1u) ||
            !backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                          signatureBlobLength,
                                                          offset,
                                                          0u,
                                                          context)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_rewrite_field_signature_type_def_tokens(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        const SZrAotCZrpSignatureRewriteContext *context) {
    return backend_aot_c_zrp_signature_skip_bytes(signatureBlobLength, offset, 1u) &&
           backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob, signatureBlobLength, offset, 0u, context);
}

static TZrBool backend_aot_c_zrp_rewrite_signature_blob_type_def_tokens(
        TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        const SZrAotCZrpSignatureRewriteContext *context) {
    TZrSize offset;
    TZrUInt8 rootNode;
    TZrBool valid;

    if (signatureBlob == ZR_NULL || signatureBlobLength == 0u || context == ZR_NULL) {
        return ZR_FALSE;
    }

    offset = 0u;
    if (!backend_aot_c_zrp_read_u8_from_signature(signatureBlob, signatureBlobLength, &offset, &rootNode)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)rootNode) {
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
            valid = backend_aot_c_zrp_rewrite_method_signature_type_def_tokens(signatureBlob,
                                                                               signatureBlobLength,
                                                                               &offset,
                                                                               context);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
            valid = backend_aot_c_zrp_rewrite_field_signature_type_def_tokens(signatureBlob,
                                                                              signatureBlobLength,
                                                                              &offset,
                                                                              context);
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
            valid = backend_aot_c_zrp_rewrite_signature_type_node(signatureBlob,
                                                                  signatureBlobLength,
                                                                  &offset,
                                                                  0u,
                                                                  context);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            valid = ZR_FALSE;
            break;
    }

    return valid &&
           offset == signatureBlobLength &&
           ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlob, signatureBlobLength);
}

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
        const SZrAotCZrpStringPoolRemap *stringRemap) {
    TZrByte *targetPool;
    SZrAotCZrpSignatureRewriteContext context;

    if (targetBlob == ZR_NULL || targetHeader == ZR_NULL) {
        return ZR_FALSE;
    }
    if (targetHeader->signatureBlobPool.byteLength == 0u) {
        return ZR_TRUE;
    }
    if (signatureRemap == ZR_NULL || (signatureRemap->count != 0u && signatureRemap->entries == ZR_NULL)) {
        return ZR_FALSE;
    }

    context.typeRows = typeRows;
    context.typeCount = typeCount;
    context.typeSpecRows = typeSpecRows;
    context.typeSpecCount = typeSpecCount;
    context.moduleRefRows = moduleRefRows;
    context.moduleRefCount = moduleRefCount;
    context.tokenRecords = tokenRecords;
    context.tokenRecordCount = tokenRecordCount;
    context.methodRows = methodRows;
    context.methodCount = methodCount;
    context.fieldRows = fieldRows;
    context.fieldCount = fieldCount;
    context.genericParamRows = genericParamRows;
    context.genericParamCount = genericParamCount;
    context.genericParamConstraintRows = genericParamConstraintRows;
    context.genericParamConstraintCount = genericParamConstraintCount;
    context.functionTable = functionTable;
    context.retainedMethodDefCount = retainedMethodDefCount;
    context.sourceSignatureBlobPool = sourceSignatureBlobPool;
    context.sourceSignatureBlobPoolBytes = sourceSignatureBlobPoolBytes;
    context.signatureRemap = signatureRemap;
    context.stringRemap = stringRemap;

    targetPool = targetBlob + targetHeader->signatureBlobPool.offset;
    for (TZrUInt32 index = 0u; index < signatureRemap->count; index++) {
        const SZrAotCZrpSignatureBlobRemapEntry *entry = &signatureRemap->entries[index];
        if (entry->byteLength == 0u) {
            continue;
        }
        if (entry->newOffset > targetHeader->signatureBlobPool.byteLength ||
            entry->byteLength > targetHeader->signatureBlobPool.byteLength - entry->newOffset ||
            !backend_aot_c_zrp_rewrite_signature_blob_type_def_tokens(targetPool + entry->newOffset,
                                                                      entry->byteLength,
                                                                      &context)) {
            return ZR_FALSE;
        }
    }

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
                                                     typeRows,
                                                     typeCount,
                                                     tokenRecords,
                                                     tokenRecordCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     genericParamConstraintRows,
                                                     genericParamConstraintCount,
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
