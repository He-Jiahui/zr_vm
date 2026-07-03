#include "backend_aot_c_zrp_metadata_type_def.h"

#include "backend_aot_c_zrp_metadata_prune.h"
#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_signature.h"
#include "backend_aot_c_zrp_metadata_string_pool.h"

#include <stdlib.h>

static TZrBool backend_aot_c_zrp_token_is_type_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF);
}

static TZrBool backend_aot_c_zrp_token_is_bound_type_def(TZrMetadataToken token) {
    return (TZrBool)(backend_aot_c_zrp_token_is_type_def(token) && ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool backend_aot_c_zrp_type_def_token_is_member_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF);
}

static TZrBool backend_aot_c_zrp_type_def_find_method_def_index_for_token(
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        TZrMetadataToken token,
        TZrUInt32 *outIndex) {
    if (methodRows == ZR_NULL || !backend_aot_c_zrp_type_def_token_is_member_def(token)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < methodCount; index++) {
        if (methodRows[index].token == token) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_type_def_find_field_def_index_for_token(
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        TZrMetadataToken token,
        TZrUInt32 *outIndex) {
    if (fieldRows == ZR_NULL || !backend_aot_c_zrp_type_def_token_is_member_def(token)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < fieldCount; index++) {
        if (fieldRows[index].token == token) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_token_record_references_type_def(const SZrMetadataTokenRecord *record,
                                                                  TZrMetadataToken typeDefToken) {
    if (record == ZR_NULL || !backend_aot_c_zrp_token_is_type_def(typeDefToken)) {
        return ZR_FALSE;
    }

    return (TZrBool)(record->token == typeDefToken ||
                     record->relatedToken == typeDefToken ||
                     record->ownerToken == typeDefToken ||
                     record->targetMetadataToken == typeDefToken ||
                     record->targetSignatureToken == typeDefToken);
}

static TZrBool backend_aot_c_zrp_type_def_root_member_token_is_retained(
        TZrMetadataToken token,
        TZrMetadataToken typeDefToken,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
    TZrUInt32 methodIndex;
    TZrUInt32 fieldIndex;

    if (!backend_aot_c_zrp_type_def_token_is_member_def(token)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_zrp_type_def_find_method_def_index_for_token(methodRows,
                                                                   methodCount,
                                                                   token,
                                                                   &methodIndex)) {
        return backend_aot_c_zrp_method_def_row_is_retained(&methodRows[methodIndex], functionTable);
    }
    if (!backend_aot_c_zrp_type_def_find_field_def_index_for_token(fieldRows,
                                                                   fieldCount,
                                                                   token,
                                                                   &fieldIndex)) {
        return ZR_FALSE;
    }

    if (fieldRows[fieldIndex].ownerTypeToken == typeDefToken) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[fieldIndex],
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
                                                       retainedMethodDefCount);
}

static TZrBool backend_aot_c_zrp_type_def_root_token_record_member_tokens_are_retained(
        const SZrMetadataTokenRecord *record,
        TZrMetadataToken typeDefToken,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
    return (TZrBool)(record != ZR_NULL &&
                     backend_aot_c_zrp_type_def_root_member_token_is_retained(record->token,
                                                                              typeDefToken,
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
                     backend_aot_c_zrp_type_def_root_member_token_is_retained(record->relatedToken,
                                                                              typeDefToken,
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
                     backend_aot_c_zrp_type_def_root_member_token_is_retained(record->ownerToken,
                                                                              typeDefToken,
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
                     backend_aot_c_zrp_type_def_root_member_token_is_retained(record->targetMetadataToken,
                                                                              typeDefToken,
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
                     backend_aot_c_zrp_type_def_root_member_token_is_retained(record->targetSignatureToken,
                                                                              typeDefToken,
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
                                                                              retainedMethodDefCount));
}

static TZrBool backend_aot_c_zrp_retained_token_record_references_type_def(
        TZrMetadataToken typeDefToken,
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
    if (tokenRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < tokenRecordCount; index++) {
        SZrMetadataTokenRecord record = tokenRecords[index];
        if (!backend_aot_c_zrp_token_record_references_type_def(&record, typeDefToken)) {
            continue;
        }
        if (backend_aot_c_zrp_type_def_root_token_record_member_tokens_are_retained(
                    &record,
                    typeDefToken,
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
                    retainedMethodDefCount)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_retained_method_def_references_type_def(
        TZrMetadataToken typeDefToken,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrAotFunctionTable *functionTable) {
    if (methodRows == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < methodCount; index++) {
        if (methodRows[index].ownerTypeToken == typeDefToken &&
            backend_aot_c_zrp_method_def_row_is_retained(&methodRows[index], functionTable)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_constraint_references_type_def(
        TZrMetadataToken typeDefToken,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
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
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < constraintCount; index++) {
        SZrZrpMetadataGenericParamConstraintRow row = constraintRows[index];
        if (row.constraintTypeToken != typeDefToken) {
            continue;
        }
        if (backend_aot_c_zrp_remap_generic_param_constraint_row(&row,
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
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_type_def_row_has_retained_root(
        const SZrZrpMetadataTypeDefRow *row,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
    if (row == ZR_NULL || !backend_aot_c_zrp_token_is_type_def(row->token)) {
        return ZR_FALSE;
    }

    return (TZrBool)(backend_aot_c_zrp_retained_token_record_references_type_def(row->token,
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
                                                                                 genericParamConstraintRows,
                                                                                 genericParamConstraintCount,
                                                                                 functionTable,
                                                                                 retainedMethodDefCount) ||
                     backend_aot_c_zrp_retained_method_def_references_type_def(row->token,
                                                                               methodRows,
                                                                               methodCount,
                                                                               functionTable) ||
                     backend_aot_c_zrp_constraint_references_type_def(row->token,
                                                                      typeRows,
                                                                      typeCount,
                                                                      tokenRecords,
                                                                      tokenRecordCount,
                                                                      genericParamConstraintRows,
                                                                      genericParamConstraintCount,
                                                                      genericParamRows,
                                                                      genericParamCount,
                                                                      methodRows,
                                                                      methodCount,
                                                                      fieldRows,
                                                                      fieldCount,
                                                                      functionTable,
                                                                      retainedMethodDefCount));
}

TZrBool backend_aot_c_zrp_type_def_row_is_retained(
        const SZrZrpMetadataTypeDefRow *row,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
    return backend_aot_c_zrp_type_def_row_has_retained_root(row,
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
                                                            retainedMethodDefCount);
}

TZrUInt32 backend_aot_c_zrp_count_retained_type_defs(
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
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_type_def_row_is_retained(&rows[index],
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
            retainedCount++;
        }
    }

    return retainedCount;
}

TZrBool backend_aot_c_zrp_field_def_row_is_retained(
        const SZrZrpMetadataFieldDefRow *row,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
    TZrMetadataToken ownerTypeToken;

    if (row == ZR_NULL || !backend_aot_c_zrp_token_is_type_def(row->ownerTypeToken)) {
        return ZR_FALSE;
    }

    ownerTypeToken = row->ownerTypeToken;
    return backend_aot_c_zrp_remap_type_def_token(&ownerTypeToken,
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
                                                  retainedMethodDefCount);
}

TZrUInt32 backend_aot_c_zrp_count_retained_field_defs(
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
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_field_def_row_is_retained(&rows[index],
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
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_field_defs_before(
        const SZrZrpMetadataFieldDefRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
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
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_field_defs(rows,
                                                       end,
                                                       typeRows,
                                                       typeCount,
                                                       tokenRecords,
                                                       tokenRecordCount,
                                                       methodRows,
                                                       methodCount,
                                                       genericParamRows,
                                                       genericParamCount,
                                                       genericParamConstraintRows,
                                                       genericParamConstraintCount,
                                                       functionTable,
                                                       retainedMethodDefCount);
}

TZrMetadataToken backend_aot_c_zrp_compacted_retained_field_def_token(
        TZrUInt32 retainedMethodDefCount,
        const SZrZrpMetadataFieldDefRow *rows,
        TZrUInt32 count,
        TZrUInt32 fieldDefIndex,
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
        const SZrAotFunctionTable *functionTable) {
    TZrUInt32 newRid = retainedMethodDefCount +
                       backend_aot_c_zrp_count_retained_field_defs_before(rows,
                                                                          count,
                                                                          fieldDefIndex,
                                                                          typeRows,
                                                                          typeCount,
                                                                          tokenRecords,
                                                                          tokenRecordCount,
                                                                          methodRows,
                                                                          methodCount,
                                                                          genericParamRows,
                                                                          genericParamCount,
                                                                          genericParamConstraintRows,
                                                                          genericParamConstraintCount,
                                                                          functionTable,
                                                                          retainedMethodDefCount) +
                       1u;
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, newRid);
}

void backend_aot_c_zrp_adjust_type_def_field_range(
        SZrZrpMetadataTypeDefRow *row,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
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
    const TZrUInt32 invalidIndex = 0xFFFFFFFFu;
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    TZrUInt32 firstRetainedIndex = invalidIndex;
    TZrUInt32 retainedCount = 0u;

    if (row == ZR_NULL || fieldRows == ZR_NULL || row->fieldDefCount == 0u) {
        return;
    }

    firstIndex = row->firstFieldDefIndex;
    if (firstIndex >= fieldCount) {
        row->firstFieldDefIndex = 0u;
        row->fieldDefCount = 0u;
        return;
    }

    endIndex = firstIndex + row->fieldDefCount;
    if (endIndex < firstIndex || endIndex > fieldCount) {
        endIndex = fieldCount;
    }

    for (TZrUInt32 index = firstIndex; index < endIndex; index++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[index],
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
                                                         retainedMethodDefCount)) {
            continue;
        }
        if (firstRetainedIndex == invalidIndex) {
            firstRetainedIndex =
                    backend_aot_c_zrp_count_retained_field_defs_before(fieldRows,
                                                                       fieldCount,
                                                                       index,
                                                                       typeRows,
                                                                       typeCount,
                                                                       tokenRecords,
                                                                       tokenRecordCount,
                                                                       methodRows,
                                                                       methodCount,
                                                                       genericParamRows,
                                                                       genericParamCount,
                                                                       genericParamConstraintRows,
                                                                       genericParamConstraintCount,
                                                                       functionTable,
                                                                       retainedMethodDefCount);
        }
        retainedCount++;
    }

    row->firstFieldDefIndex = firstRetainedIndex == invalidIndex ? 0u : firstRetainedIndex;
    row->fieldDefCount = retainedCount;
}

void backend_aot_c_zrp_type_def_token_remap_destroy(SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL) {
        return;
    }

    if (metadata->ownedTypeDefTokenRemapEntries != ZR_NULL) {
        free(metadata->ownedTypeDefTokenRemapEntries);
    }

    metadata->hasTypeDefTokenRemap = ZR_FALSE;
    metadata->typeDefTokenRemapEntries = ZR_NULL;
    metadata->typeDefTokenRemapCount = 0u;
    metadata->ownedTypeDefTokenRemapEntries = ZR_NULL;
}

static TZrBool backend_aot_c_zrp_type_def_token_remap_append(
        SZrAotCZrpTypeDefTokenRemapEntry *entries,
        TZrUInt32 capacity,
        TZrUInt32 *writeIndex,
        TZrMetadataToken sourceToken,
        TZrMetadataToken targetToken) {
    if (entries == ZR_NULL ||
        writeIndex == ZR_NULL ||
        *writeIndex >= capacity ||
        !backend_aot_c_zrp_token_is_bound_type_def(sourceToken) ||
        !backend_aot_c_zrp_token_is_bound_type_def(targetToken)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < *writeIndex; index++) {
        if (entries[index].sourceToken == sourceToken) {
            return ZR_FALSE;
        }
    }

    entries[*writeIndex].sourceToken = sourceToken;
    entries[*writeIndex].targetToken = targetToken;
    (*writeIndex)++;
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_type_def_token_remap_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
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
        TZrUInt32 retainedMethodDefCount,
        TZrUInt32 retainedTypeDefCount) {
    SZrAotCZrpTypeDefTokenRemapEntry *entries;
    TZrUInt32 actualRetainedTypeDefCount;
    TZrUInt32 writeIndex = 0u;

    if (metadata == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_c_zrp_type_def_token_remap_destroy(metadata);

    if ((count > 0u && rows == ZR_NULL) ||
        (tokenRecordCount > 0u && tokenRecords == ZR_NULL) ||
        (methodCount > 0u && methodRows == ZR_NULL) ||
        (fieldCount > 0u && fieldRows == ZR_NULL) ||
        (genericParamCount > 0u && genericParamRows == ZR_NULL) ||
        (genericParamConstraintCount > 0u && genericParamConstraintRows == ZR_NULL)) {
        return ZR_FALSE;
    }

    actualRetainedTypeDefCount =
            backend_aot_c_zrp_count_retained_type_defs(rows,
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
                                                       retainedMethodDefCount);
    if (actualRetainedTypeDefCount != retainedTypeDefCount) {
        return ZR_FALSE;
    }

    metadata->hasTypeDefTokenRemap = ZR_TRUE;
    if (retainedTypeDefCount == 0u) {
        return ZR_TRUE;
    }

    entries = (SZrAotCZrpTypeDefTokenRemapEntry *)malloc(
            sizeof(SZrAotCZrpTypeDefTokenRemapEntry) * (size_t)retainedTypeDefCount);
    if (entries == ZR_NULL) {
        backend_aot_c_zrp_type_def_token_remap_destroy(metadata);
        return ZR_FALSE;
    }

    for (TZrUInt32 typeDefIndex = 0u; typeDefIndex < count; typeDefIndex++) {
        if (!backend_aot_c_zrp_type_def_row_is_retained(&rows[typeDefIndex],
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

        if (!backend_aot_c_zrp_type_def_token_remap_append(
                    entries,
                    retainedTypeDefCount,
                    &writeIndex,
                    rows[typeDefIndex].token,
                    backend_aot_c_zrp_compacted_type_def_token(rows,
                                                               count,
                                                               typeDefIndex,
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
                                                               retainedMethodDefCount))) {
            free(entries);
            backend_aot_c_zrp_type_def_token_remap_destroy(metadata);
            return ZR_FALSE;
        }
    }

    metadata->typeDefTokenRemapEntries = entries;
    metadata->typeDefTokenRemapCount = writeIndex;
    metadata->ownedTypeDefTokenRemapEntries = entries;
    if (writeIndex != retainedTypeDefCount) {
        backend_aot_c_zrp_type_def_token_remap_destroy(metadata);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_type_defs_before(
        const SZrZrpMetadataTypeDefRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
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
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_type_defs(rows,
                                                      end,
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
                                                      retainedMethodDefCount);
}

static TZrBool backend_aot_c_zrp_find_type_def_index_for_token(const SZrZrpMetadataTypeDefRow *rows,
                                                               TZrUInt32 count,
                                                               TZrMetadataToken token,
                                                               TZrUInt32 *outIndex) {
    if (rows == ZR_NULL || !backend_aot_c_zrp_token_is_type_def(token)) {
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

TZrMetadataToken backend_aot_c_zrp_compacted_type_def_token(
        const SZrZrpMetadataTypeDefRow *rows,
        TZrUInt32 count,
        TZrUInt32 typeDefIndex,
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
    TZrUInt32 newRid =
            backend_aot_c_zrp_count_retained_type_defs_before(rows,
                                                              count,
                                                              typeDefIndex,
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
                                                              retainedMethodDefCount) +
            1u;
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, newRid);
}

TZrBool backend_aot_c_zrp_remap_type_def_token(
        TZrMetadataToken *token,
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
    TZrUInt32 typeDefIndex;

    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*token == 0u || !backend_aot_c_zrp_token_is_type_def(*token)) {
        return ZR_TRUE;
    }
    if (!backend_aot_c_zrp_find_type_def_index_for_token(rows, count, *token, &typeDefIndex) ||
        !backend_aot_c_zrp_type_def_row_is_retained(&rows[typeDefIndex],
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
        return ZR_FALSE;
    }

    *token = backend_aot_c_zrp_compacted_type_def_token(rows,
                                                       count,
                                                       typeDefIndex,
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
                                                       retainedMethodDefCount);
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_remap_type_def_tokens_in_record(
        SZrMetadataTokenRecord *record,
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
    return (TZrBool)(record != ZR_NULL &&
                     backend_aot_c_zrp_remap_type_def_token(&record->token,
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
                                                            retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_type_def_token(&record->relatedToken,
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
                                                            retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_type_def_token(&record->ownerToken,
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
                                                            retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_type_def_token(&record->targetMetadataToken,
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
                                                            retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_type_def_token(&record->targetSignatureToken,
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
                                                            retainedMethodDefCount));
}

TZrBool backend_aot_c_zrp_copy_type_defs(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
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
        const SZrAotCZrpSignatureBlobRemap *signatureRemap,
        const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataTypeDefRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetBlob == ZR_NULL || targetHeader == ZR_NULL) {
        return ZR_FALSE;
    }
    if (targetHeader->typeDefs.byteLength == 0u) {
        return ZR_TRUE;
    }
    if (typeRows == ZR_NULL) {
        return ZR_FALSE;
    }

    targetRows = (SZrZrpMetadataTypeDefRow *)(void *)(targetBlob + targetHeader->typeDefs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < typeCount; readIndex++) {
        if (!backend_aot_c_zrp_type_def_row_is_retained(&typeRows[readIndex],
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
                                                        retainedMethodDefCount)) {
            continue;
        }
        targetRows[writeIndex] = typeRows[readIndex];
        targetRows[writeIndex].token =
                backend_aot_c_zrp_compacted_type_def_token(typeRows,
                                                           typeCount,
                                                           readIndex,
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
                                                           retainedMethodDefCount);
        backend_aot_c_zrp_adjust_type_def_method_range(&targetRows[writeIndex],
                                                       methodRows,
                                                       methodCount,
                                                       functionTable);
        backend_aot_c_zrp_adjust_type_def_field_range(&targetRows[writeIndex],
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
                                                      genericParamConstraintRows,
                                                      genericParamConstraintCount,
                                                      functionTable,
                                                      retainedMethodDefCount);
        backend_aot_c_zrp_adjust_generic_param_range(&targetRows[writeIndex].firstGenericParamIndex,
                                                     &targetRows[writeIndex].genericParamCount,
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
                                                     genericParamConstraintRows,
                                                     genericParamConstraintCount,
                                                     functionTable,
                                                     retainedMethodDefCount);
        if (!backend_aot_c_zrp_remap_type_def_string_offsets(&targetRows[writeIndex], stringRemap) ||
            !backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[writeIndex].signatureBlobOffset,
                                                           targetRows[writeIndex].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        writeIndex++;
    }

    return (TZrBool)(writeIndex == targetHeader->typeDefs.count);
}
