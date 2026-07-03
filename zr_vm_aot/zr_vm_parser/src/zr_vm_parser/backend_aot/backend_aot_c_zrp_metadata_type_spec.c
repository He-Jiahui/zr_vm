#include "backend_aot_c_zrp_metadata_type_spec.h"

#include "backend_aot_c_zrp_metadata_remap.h"

static TZrBool backend_aot_c_zrp_token_is_type_spec(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_SPEC);
}

TZrBool backend_aot_c_zrp_type_spec_row_is_retained(const SZrZrpMetadataTypeSpecRow *row,
                                                    const SZrMetadataTokenRecord *tokenRecords,
                                                    TZrUInt32 tokenRecordCount,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrZrpMetadataFieldDefRow *fieldRows,
                                                    TZrUInt32 fieldCount,
                                                    const SZrZrpMetadataTypeDefRow *typeRows,
                                                    TZrUInt32 typeCount,
                                                    const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                    TZrUInt32 genericParamCount,
                                                    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                    TZrUInt32 genericParamConstraintCount,
                                                    const SZrAotFunctionTable *functionTable,
                                                    TZrUInt32 retainedMethodDefCount) {
    if (row == ZR_NULL ||
        tokenRecords == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(row->token) != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
        SZrMetadataTokenRecord record = tokenRecords[index];
        if (record.token != row->token) {
            continue;
        }
        if (backend_aot_c_zrp_remap_retained_token_record(&record,
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
            return ZR_TRUE;
        }
    }

    if (genericParamConstraintRows != ZR_NULL) {
        for (TZrUInt32 index = 0u; index < genericParamConstraintCount; index++) {
            SZrZrpMetadataGenericParamConstraintRow constraintRow = genericParamConstraintRows[index];
            if (constraintRow.constraintTypeToken != row->token) {
                continue;
            }
            if (backend_aot_c_zrp_remap_generic_param_constraint_row(&constraintRow,
                                                                     genericParamRows,
                                                                     genericParamCount,
                                                                     typeRows,
                                                                     typeCount,
                                                                     tokenRecords,
                                                                     tokenRecordCount,
                                                                     genericParamConstraintRows,
                                                                     genericParamConstraintCount,
                                                                     methodRows,
                                                                     methodCount,
                                                                     fieldRows,
                                                                     fieldCount,
                                                                     functionTable,
                                                                     retainedMethodDefCount)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_find_type_spec_index_for_token(const SZrZrpMetadataTypeSpecRow *rows,
                                                                TZrUInt32 count,
                                                                TZrMetadataToken token,
                                                                TZrUInt32 *outIndex) {
    if (rows == ZR_NULL || !backend_aot_c_zrp_token_is_type_spec(token)) {
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

TZrUInt32 backend_aot_c_zrp_count_retained_type_specs(const SZrZrpMetadataTypeSpecRow *rows,
                                                      TZrUInt32 count,
                                                      const SZrMetadataTokenRecord *tokenRecords,
                                                      TZrUInt32 tokenRecordCount,
                                                      const SZrZrpMetadataMethodDefRow *methodRows,
                                                      TZrUInt32 methodCount,
                                                      const SZrZrpMetadataFieldDefRow *fieldRows,
                                                      TZrUInt32 fieldCount,
                                                      const SZrZrpMetadataTypeDefRow *typeRows,
                                                      TZrUInt32 typeCount,
                                                      const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                      TZrUInt32 genericParamCount,
                                                      const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                      TZrUInt32 genericParamConstraintCount,
                                                      const SZrAotFunctionTable *functionTable,
                                                      TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
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
                                                        retainedMethodDefCount)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_type_specs_before(
        const SZrZrpMetadataTypeSpecRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_type_specs(rows,
                                                       end,
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
                                                       retainedMethodDefCount);
}

TZrMetadataToken backend_aot_c_zrp_compacted_type_spec_token(const SZrZrpMetadataTypeSpecRow *rows,
                                                            TZrUInt32 count,
                                                            TZrUInt32 typeSpecIndex,
                                                            const SZrMetadataTokenRecord *tokenRecords,
                                                            TZrUInt32 tokenRecordCount,
                                                            const SZrZrpMetadataMethodDefRow *methodRows,
                                                            TZrUInt32 methodCount,
                                                            const SZrZrpMetadataFieldDefRow *fieldRows,
                                                            TZrUInt32 fieldCount,
                                                            const SZrZrpMetadataTypeDefRow *typeRows,
                                                            TZrUInt32 typeCount,
                                                            const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                            TZrUInt32 genericParamCount,
                                                            const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                            TZrUInt32 genericParamConstraintCount,
                                                            const SZrAotFunctionTable *functionTable,
                                                            TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 newRid =
            backend_aot_c_zrp_count_retained_type_specs_before(rows,
                                                               count,
                                                               typeSpecIndex,
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
                                                               retainedMethodDefCount) +
            1u;
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, newRid);
}

TZrBool backend_aot_c_zrp_remap_type_spec_token(TZrMetadataToken *token,
                                                const SZrZrpMetadataTypeSpecRow *rows,
                                                TZrUInt32 count,
                                                const SZrMetadataTokenRecord *tokenRecords,
                                                TZrUInt32 tokenRecordCount,
                                                const SZrZrpMetadataMethodDefRow *methodRows,
                                                TZrUInt32 methodCount,
                                                const SZrZrpMetadataFieldDefRow *fieldRows,
                                                TZrUInt32 fieldCount,
                                                const SZrZrpMetadataTypeDefRow *typeRows,
                                                TZrUInt32 typeCount,
                                                const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                TZrUInt32 genericParamCount,
                                                const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                TZrUInt32 genericParamConstraintCount,
                                                const SZrAotFunctionTable *functionTable,
                                                TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 typeSpecIndex;

    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*token == 0u || !backend_aot_c_zrp_token_is_type_spec(*token)) {
        return ZR_TRUE;
    }
    if (!backend_aot_c_zrp_find_type_spec_index_for_token(rows, count, *token, &typeSpecIndex) ||
        !backend_aot_c_zrp_type_spec_row_is_retained(&rows[typeSpecIndex],
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
                                                     retainedMethodDefCount)) {
        return ZR_FALSE;
    }

    *token = backend_aot_c_zrp_compacted_type_spec_token(rows,
                                                        count,
                                                        typeSpecIndex,
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
                                                        retainedMethodDefCount);
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_remap_type_spec_tokens_in_record(SZrMetadataTokenRecord *record,
                                                           const SZrZrpMetadataTypeSpecRow *rows,
                                                           TZrUInt32 count,
                                                           const SZrMetadataTokenRecord *tokenRecords,
                                                           TZrUInt32 tokenRecordCount,
                                                           const SZrZrpMetadataMethodDefRow *methodRows,
                                                           TZrUInt32 methodCount,
                                                           const SZrZrpMetadataFieldDefRow *fieldRows,
                                                           TZrUInt32 fieldCount,
                                                           const SZrZrpMetadataTypeDefRow *typeRows,
                                                           TZrUInt32 typeCount,
                                                           const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                           TZrUInt32 genericParamCount,
                                                           const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                           TZrUInt32 genericParamConstraintCount,
                                                           const SZrAotFunctionTable *functionTable,
                                                           TZrUInt32 retainedMethodDefCount) {
    return (TZrBool)(record != ZR_NULL &&
                     backend_aot_c_zrp_remap_type_spec_token(&record->token,
                                                             rows,
                                                             count,
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
                     backend_aot_c_zrp_remap_type_spec_token(&record->relatedToken,
                                                             rows,
                                                             count,
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
                     backend_aot_c_zrp_remap_type_spec_token(&record->ownerToken,
                                                             rows,
                                                             count,
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
                     backend_aot_c_zrp_remap_type_spec_token(&record->targetMetadataToken,
                                                             rows,
                                                             count,
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
                     backend_aot_c_zrp_remap_type_spec_token(&record->targetSignatureToken,
                                                             rows,
                                                             count,
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

TZrBool backend_aot_c_zrp_copy_type_specs(TZrByte *targetBlob,
                                          const SZrZrpMetadataHeader *targetHeader,
                                          const SZrZrpMetadataTypeSpecRow *typeSpecRows,
                                          TZrUInt32 typeSpecCount,
                                          const SZrMetadataTokenRecord *tokenRecords,
                                          TZrUInt32 tokenRecordCount,
                                          const SZrZrpMetadataMethodDefRow *methodRows,
                                          TZrUInt32 methodCount,
                                          const SZrZrpMetadataFieldDefRow *fieldRows,
                                          TZrUInt32 fieldCount,
                                          const SZrZrpMetadataTypeDefRow *typeRows,
                                          TZrUInt32 typeCount,
                                          const SZrZrpMetadataGenericParamRow *genericParamRows,
                                          TZrUInt32 genericParamCount,
                                          const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                          TZrUInt32 genericParamConstraintCount,
                                          const SZrAotFunctionTable *functionTable,
                                          TZrUInt32 retainedMethodDefCount,
                                          const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrZrpMetadataTypeSpecRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetBlob == ZR_NULL || targetHeader == ZR_NULL) {
        return ZR_FALSE;
    }
    if (targetHeader->typeSpecs.byteLength == 0u) {
        return ZR_TRUE;
    }
    if (typeSpecRows == ZR_NULL) {
        return ZR_FALSE;
    }

    targetRows = (SZrZrpMetadataTypeSpecRow *)(void *)(targetBlob + targetHeader->typeSpecs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < typeSpecCount; readIndex++) {
        if (!backend_aot_c_zrp_type_spec_row_is_retained(&typeSpecRows[readIndex],
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
                                                         retainedMethodDefCount)) {
            continue;
        }
        targetRows[writeIndex] = typeSpecRows[readIndex];
        targetRows[writeIndex].token =
                backend_aot_c_zrp_compacted_type_spec_token(typeSpecRows,
                                                            typeSpecCount,
                                                            readIndex,
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
                                                            retainedMethodDefCount);
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[writeIndex].signatureBlobOffset,
                                                           targetRows[writeIndex].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        targetRows[writeIndex].signatureHash =
                backend_aot_c_zrp_recomputed_signature_hash(targetBlob,
                                                            targetHeader,
                                                            targetRows[writeIndex].signatureBlobOffset,
                                                            targetRows[writeIndex].signatureBlobLength);
        writeIndex++;
    }

    return (TZrBool)(writeIndex == targetHeader->typeSpecs.count);
}
