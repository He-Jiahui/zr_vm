#include "backend_aot_c_zrp_metadata_module_ref.h"

#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_signature.h"
#include "backend_aot_c_zrp_metadata_type_spec.h"

#define CZrAotCZrpModuleRefSignatureScanMaxDepth 64u

static TZrBool backend_aot_c_zrp_token_is_assembly_ref(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_ASSEMBLY_REF);
}

static TZrBool backend_aot_c_zrp_token_record_is_import_ref_root(const SZrMetadataTokenRecord *record) {
    TZrUInt32 table;

    if (record == ZR_NULL || record->token == 0u) {
        return ZR_FALSE;
    }

    table = ZR_METADATA_TOKEN_TABLE(record->token);
    return (TZrBool)(table == ZR_METADATA_TABLE_TYPE_REF || table == ZR_METADATA_TABLE_MEMBER_REF);
}

static TZrBool backend_aot_c_zrp_record_references_module_ref(const SZrMetadataTokenRecord *record,
                                                              TZrMetadataToken moduleRefToken) {
    if (record == ZR_NULL || !backend_aot_c_zrp_token_is_assembly_ref(moduleRefToken)) {
        return ZR_FALSE;
    }

    return (TZrBool)(record->relatedToken == moduleRefToken ||
                     record->ownerToken == moduleRefToken ||
                     record->targetMetadataToken == moduleRefToken ||
                     record->targetSignatureToken == moduleRefToken);
}

static TZrBool backend_aot_c_zrp_token_record_is_retained_before_module_ref_pruning(
        SZrMetadataTokenRecord *record,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
    return (TZrBool)(record != ZR_NULL &&
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
                                                                        retainedMethodDefCount));
}

static TZrBool backend_aot_c_zrp_signature_scan_skip_bytes(TZrUInt32 signatureBlobLength,
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

static TZrBool backend_aot_c_zrp_signature_scan_skip_u32(TZrUInt32 signatureBlobLength, TZrSize *offset) {
    return backend_aot_c_zrp_signature_scan_skip_bytes(signatureBlobLength, offset, 4u);
}

static TZrBool backend_aot_c_zrp_signature_scan_read_u8(const TZrByte *signatureBlob,
                                                        TZrUInt32 signatureBlobLength,
                                                        TZrSize *offset,
                                                        TZrUInt8 *value) {
    if (signatureBlob == ZR_NULL ||
        offset == ZR_NULL ||
        value == ZR_NULL ||
        !backend_aot_c_zrp_signature_scan_skip_bytes(signatureBlobLength, offset, 1u)) {
        return ZR_FALSE;
    }

    *value = signatureBlob[*offset - 1u];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_scan_read_u32(const TZrByte *signatureBlob,
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

static TZrBool backend_aot_c_zrp_signature_type_list_references_module_ref(
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 count,
        TZrUInt32 depth,
        TZrMetadataToken moduleRefToken,
        TZrBool *outFound);

static TZrBool backend_aot_c_zrp_signature_type_node_references_module_ref(
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 depth,
        TZrMetadataToken moduleRefToken,
        TZrBool *outFound) {
    TZrUInt8 node;
    TZrUInt32 count;
    TZrUInt32 tokenValue;

    if (outFound == ZR_NULL ||
        depth > CZrAotCZrpModuleRefSignatureScanMaxDepth ||
        !backend_aot_c_zrp_signature_scan_read_u8(signatureBlob, signatureBlobLength, offset, &node)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               depth + 1u,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            return backend_aot_c_zrp_signature_scan_read_u32(signatureBlob, signatureBlobLength, offset, &count) &&
                   backend_aot_c_zrp_signature_type_list_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               count,
                                                                               depth,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
            return ZR_FALSE;
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            if (!backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                             signatureBlobLength,
                                                                             offset,
                                                                             depth + 1u,
                                                                             moduleRefToken,
                                                                             outFound) ||
                !backend_aot_c_zrp_signature_scan_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_signature_type_list_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               count,
                                                                               depth,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               depth + 1u,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) ||
                !backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) ||
                !backend_aot_c_zrp_signature_scan_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_signature_type_list_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               count,
                                                                               depth,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               offset,
                                                                               depth + 1u,
                                                                               moduleRefToken,
                                                                               outFound);
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            if (!backend_aot_c_zrp_signature_scan_read_u32(signatureBlob,
                                                           signatureBlobLength,
                                                           offset,
                                                           &tokenValue)) {
                return ZR_FALSE;
            }
            if ((TZrMetadataToken)tokenValue == moduleRefToken) {
                *outFound = ZR_TRUE;
            }
            return ZR_TRUE;
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            return backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_zrp_signature_type_list_references_module_ref(
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 count,
        TZrUInt32 depth,
        TZrMetadataToken moduleRefToken,
        TZrBool *outFound) {
    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                         signatureBlobLength,
                                                                         offset,
                                                                         depth + 1u,
                                                                         moduleRefToken,
                                                                         outFound)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_blob_references_module_ref(const TZrByte *signatureBlob,
                                                                      TZrUInt32 signatureBlobLength,
                                                                      TZrMetadataToken moduleRefToken,
                                                                      TZrBool *outFound) {
    TZrSize offset;
    TZrUInt8 rootNode;
    TZrUInt32 parameterCount;

    if (outFound == ZR_NULL) {
        return ZR_FALSE;
    }
    if (signatureBlob == ZR_NULL || signatureBlobLength == 0u) {
        return ZR_TRUE;
    }

    offset = 0u;
    if (!backend_aot_c_zrp_signature_scan_read_u8(signatureBlob, signatureBlobLength, &offset, &rootNode)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)rootNode) {
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
            if (!backend_aot_c_zrp_signature_scan_skip_bytes(signatureBlobLength, &offset, 2u) ||
                !backend_aot_c_zrp_signature_scan_skip_u32(signatureBlobLength, &offset) ||
                !backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                             signatureBlobLength,
                                                                             &offset,
                                                                             0u,
                                                                             moduleRefToken,
                                                                             outFound) ||
                !backend_aot_c_zrp_signature_scan_read_u32(signatureBlob,
                                                           signatureBlobLength,
                                                           &offset,
                                                           &parameterCount)) {
                return ZR_FALSE;
            }
            for (TZrUInt32 index = 0u; index < parameterCount; index++) {
                if (!backend_aot_c_zrp_signature_scan_skip_bytes(signatureBlobLength, &offset, 1u) ||
                    !backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                                 signatureBlobLength,
                                                                                 &offset,
                                                                                 0u,
                                                                                 moduleRefToken,
                                                                                 outFound)) {
                    return ZR_FALSE;
                }
            }
            return offset == (TZrSize)signatureBlobLength;
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
            return backend_aot_c_zrp_signature_scan_skip_bytes(signatureBlobLength, &offset, 1u) &&
                   backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               &offset,
                                                                               0u,
                                                                               moduleRefToken,
                                                                               outFound) &&
                   offset == (TZrSize)signatureBlobLength;
        default:
            offset = 0u;
            return backend_aot_c_zrp_signature_type_node_references_module_ref(signatureBlob,
                                                                               signatureBlobLength,
                                                                               &offset,
                                                                               0u,
                                                                               moduleRefToken,
                                                                               outFound) &&
                   offset == (TZrSize)signatureBlobLength;
    }
}

static TZrBool backend_aot_c_zrp_retained_signature_blobs_reference_module_ref(
        TZrMetadataToken moduleRefToken,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    if (signatureBlobPool == ZR_NULL ||
        signatureBlobPoolBytes == 0u ||
        signatureRemap == ZR_NULL ||
        signatureRemap->count == 0u ||
        signatureRemap->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < signatureRemap->count; index++) {
        const SZrAotCZrpSignatureBlobRemapEntry *entry = &signatureRemap->entries[index];
        TZrBool found = ZR_FALSE;
        if (entry->byteLength == 0u) {
            continue;
        }
        if (entry->oldOffset > signatureBlobPoolBytes ||
            entry->byteLength > signatureBlobPoolBytes - entry->oldOffset) {
            return ZR_FALSE;
        }
        if (!backend_aot_c_zrp_signature_blob_references_module_ref(signatureBlobPool + entry->oldOffset,
                                                                    entry->byteLength,
                                                                    moduleRefToken,
                                                                    &found)) {
            return ZR_FALSE;
        }
        if (found) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_module_ref_row_is_retained(
        const SZrZrpMetadataModuleRefRow *row,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    if (row == ZR_NULL ||
        !backend_aot_c_zrp_token_is_assembly_ref(row->token)) {
        return ZR_FALSE;
    }

    if (tokenRecords != ZR_NULL) {
        for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
            SZrMetadataTokenRecord record = tokenRecords[index];
            if (!backend_aot_c_zrp_token_record_is_import_ref_root(&record) ||
                !backend_aot_c_zrp_token_record_is_retained_before_module_ref_pruning(&record,
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
                                                                                     retainedMethodDefCount)) {
                continue;
            }
            if (backend_aot_c_zrp_record_references_module_ref(&record, row->token)) {
                return ZR_TRUE;
            }
        }
    }

    return backend_aot_c_zrp_retained_signature_blobs_reference_module_ref(row->token,
                                                                           signatureBlobPool,
                                                                           signatureBlobPoolBytes,
                                                                           signatureRemap);
}

TZrUInt32 backend_aot_c_zrp_count_retained_module_refs(
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_module_ref_row_is_retained(&rows[index],
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
                                                         signatureBlobPool,
                                                         signatureBlobPoolBytes,
                                                         signatureRemap)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrBool backend_aot_c_zrp_find_module_ref_index_for_token(const SZrZrpMetadataModuleRefRow *rows,
                                                                 TZrUInt32 count,
                                                                 TZrMetadataToken token,
                                                                 TZrUInt32 *outIndex) {
    if (rows == ZR_NULL || !backend_aot_c_zrp_token_is_assembly_ref(token)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (rows[index].token == token) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_module_refs_before(
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_module_refs(rows,
                                                        end,
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
                                                        signatureBlobPool,
                                                        signatureBlobPoolBytes,
                                                        signatureRemap);
}

TZrMetadataToken backend_aot_c_zrp_compacted_module_ref_token(
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        TZrUInt32 moduleRefIndex,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrUInt32 newRid = backend_aot_c_zrp_count_retained_module_refs_before(rows,
                                                                            count,
                                                                            moduleRefIndex,
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
                                                                            signatureBlobPool,
                                                                            signatureBlobPoolBytes,
                                                                            signatureRemap) +
                       1u;
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, newRid);
}

TZrBool backend_aot_c_zrp_remap_module_ref_token(
        TZrMetadataToken *token,
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    TZrUInt32 moduleRefIndex;

    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*token == 0u || !backend_aot_c_zrp_token_is_assembly_ref(*token)) {
        return ZR_TRUE;
    }
    if (!backend_aot_c_zrp_find_module_ref_index_for_token(rows, count, *token, &moduleRefIndex) ||
        !backend_aot_c_zrp_module_ref_row_is_retained(&rows[moduleRefIndex],
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
                                                      signatureBlobPool,
                                                      signatureBlobPoolBytes,
                                                      signatureRemap)) {
        return ZR_FALSE;
    }

    *token = backend_aot_c_zrp_compacted_module_ref_token(rows,
                                                         count,
                                                         moduleRefIndex,
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
                                                         signatureBlobPool,
                                                         signatureBlobPoolBytes,
                                                         signatureRemap);
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_remap_module_ref_tokens_in_record(
        SZrMetadataTokenRecord *record,
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    return (TZrBool)(record != ZR_NULL &&
                     backend_aot_c_zrp_remap_module_ref_token(&record->token,
                                                              rows,
                                                              count,
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
                                                              signatureBlobPool,
                                                              signatureBlobPoolBytes,
                                                              signatureRemap) &&
                     backend_aot_c_zrp_remap_module_ref_token(&record->relatedToken,
                                                              rows,
                                                              count,
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
                                                              signatureBlobPool,
                                                              signatureBlobPoolBytes,
                                                              signatureRemap) &&
                     backend_aot_c_zrp_remap_module_ref_token(&record->ownerToken,
                                                              rows,
                                                              count,
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
                                                              signatureBlobPool,
                                                              signatureBlobPoolBytes,
                                                              signatureRemap) &&
                     backend_aot_c_zrp_remap_module_ref_token(&record->targetMetadataToken,
                                                              rows,
                                                              count,
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
                                                              signatureBlobPool,
                                                              signatureBlobPoolBytes,
                                                              signatureRemap) &&
                     backend_aot_c_zrp_remap_module_ref_token(&record->targetSignatureToken,
                                                              rows,
                                                              count,
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
                                                              signatureBlobPool,
                                                              signatureBlobPoolBytes,
                                                              signatureRemap));
}
