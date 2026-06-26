#include "backend_aot_c_zrp_metadata_remap.h"

#include "backend_aot_internal.h"

static TZrBool backend_aot_c_zrp_token_is_member_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF);
}

static TZrBool backend_aot_c_zrp_token_is_type_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF);
}

static TZrBool backend_aot_c_function_table_contains_flat_index(const SZrAotFunctionTable *functionTable,
                                                                TZrUInt32 flatIndex) {
    if (flatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_TRUE;
    }
    if (functionTable == ZR_NULL || functionTable->entries == ZR_NULL || functionTable->count > functionTable->capacity) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < functionTable->count; index++) {
        if (functionTable->entries[index].flatIndex == flatIndex) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_method_def_row_is_retained(const SZrZrpMetadataMethodDefRow *row,
                                                     const SZrAotFunctionTable *functionTable) {
    return row != ZR_NULL && backend_aot_c_function_table_contains_flat_index(functionTable, row->functionIndex);
}

TZrUInt32 backend_aot_c_zrp_count_retained_method_defs(const SZrZrpMetadataMethodDefRow *rows,
                                                       TZrUInt32 count,
                                                       const SZrAotFunctionTable *functionTable) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_method_def_row_is_retained(&rows[index], functionTable)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_method_defs_before(const SZrZrpMetadataMethodDefRow *rows,
                                                                     TZrUInt32 count,
                                                                     TZrUInt32 exclusiveEnd,
                                                                     const SZrAotFunctionTable *functionTable) {
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_method_defs(rows, end, functionTable);
}

static TZrBool backend_aot_c_zrp_find_method_def_index_for_token(const SZrZrpMetadataMethodDefRow *methodRows,
                                                                 TZrUInt32 methodCount,
                                                                 TZrMetadataToken token,
                                                                 TZrUInt32 *outIndex) {
    if (methodRows == ZR_NULL || !backend_aot_c_zrp_token_is_member_def(token)) {
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

static TZrBool backend_aot_c_zrp_find_field_def_index_for_token(const SZrZrpMetadataFieldDefRow *fieldRows,
                                                                TZrUInt32 fieldCount,
                                                                TZrMetadataToken token,
                                                                TZrUInt32 *outIndex) {
    if (fieldRows == ZR_NULL || !backend_aot_c_zrp_token_is_member_def(token)) {
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

TZrMetadataToken backend_aot_c_zrp_compacted_method_def_token(const SZrZrpMetadataMethodDefRow *methodRows,
                                                             TZrUInt32 methodCount,
                                                             TZrUInt32 methodIndex,
                                                             const SZrAotFunctionTable *functionTable) {
    TZrUInt32 newRid =
            backend_aot_c_zrp_count_retained_method_defs_before(methodRows, methodCount, methodIndex, functionTable) +
            1u;
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, newRid);
}

TZrMetadataToken backend_aot_c_zrp_compacted_field_def_token(TZrUInt32 retainedMethodDefCount,
                                                            TZrUInt32 fieldIndex) {
    return ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, retainedMethodDefCount + fieldIndex + 1u);
}

static TZrBool backend_aot_c_zrp_remap_member_def_token(TZrMetadataToken *token,
                                                        const SZrZrpMetadataMethodDefRow *methodRows,
                                                        TZrUInt32 methodCount,
                                                        const SZrZrpMetadataFieldDefRow *fieldRows,
                                                        TZrUInt32 fieldCount,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 methodIndex;
    TZrUInt32 fieldIndex;

    if (token == ZR_NULL || *token == 0u || !backend_aot_c_zrp_token_is_member_def(*token)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_zrp_find_method_def_index_for_token(methodRows, methodCount, *token, &methodIndex)) {
        if (!backend_aot_c_zrp_method_def_row_is_retained(&methodRows[methodIndex], functionTable)) {
            return ZR_FALSE;
        }

        *token = backend_aot_c_zrp_compacted_method_def_token(methodRows, methodCount, methodIndex, functionTable);
        return ZR_TRUE;
    }
    if (backend_aot_c_zrp_find_field_def_index_for_token(fieldRows, fieldCount, *token, &fieldIndex)) {
        *token = backend_aot_c_zrp_compacted_field_def_token(retainedMethodDefCount, fieldIndex);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_remap_token_record(SZrMetadataTokenRecord *record,
                                             const SZrZrpMetadataMethodDefRow *methodRows,
                                             TZrUInt32 methodCount,
                                             const SZrZrpMetadataFieldDefRow *fieldRows,
                                             TZrUInt32 fieldCount,
                                             const SZrAotFunctionTable *functionTable,
                                             TZrUInt32 retainedMethodDefCount) {
    return (TZrBool)(record != ZR_NULL &&
                     backend_aot_c_zrp_remap_member_def_token(&record->token,
                                                              methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              functionTable,
                                                              retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_member_def_token(&record->relatedToken,
                                                              methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              functionTable,
                                                              retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_member_def_token(&record->ownerToken,
                                                              methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              functionTable,
                                                              retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_member_def_token(&record->targetMetadataToken,
                                                              methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              functionTable,
                                                              retainedMethodDefCount) &&
                     backend_aot_c_zrp_remap_member_def_token(&record->targetSignatureToken,
                                                              methodRows,
                                                              methodCount,
                                                              fieldRows,
                                                              fieldCount,
                                                              functionTable,
                                                              retainedMethodDefCount));
}

TZrUInt32 backend_aot_c_zrp_count_retained_token_records(const SZrMetadataTokenRecord *records,
                                                         TZrUInt32 count,
                                                         const SZrZrpMetadataMethodDefRow *methodRows,
                                                         TZrUInt32 methodCount,
                                                         const SZrZrpMetadataFieldDefRow *fieldRows,
                                                         TZrUInt32 fieldCount,
                                                         const SZrAotFunctionTable *functionTable,
                                                         TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (records == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrMetadataTokenRecord record = records[index];
        if (backend_aot_c_zrp_remap_token_record(&record,
                                                 methodRows,
                                                 methodCount,
                                                 fieldRows,
                                                 fieldCount,
                                                 functionTable,
                                                 retainedMethodDefCount)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

TZrBool backend_aot_c_zrp_remap_method_spec_row(SZrZrpMetadataMethodSpecRow *row,
                                                const SZrZrpMetadataMethodDefRow *methodRows,
                                                TZrUInt32 methodCount,
                                                const SZrZrpMetadataFieldDefRow *fieldRows,
                                                TZrUInt32 fieldCount,
                                                const SZrAotFunctionTable *functionTable,
                                                TZrUInt32 retainedMethodDefCount) {
    if (row == ZR_NULL) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_remap_member_def_token(&row->methodToken,
                                                    methodRows,
                                                    methodCount,
                                                    fieldRows,
                                                    fieldCount,
                                                    functionTable,
                                                    retainedMethodDefCount);
}

TZrUInt32 backend_aot_c_zrp_count_retained_method_specs(const SZrZrpMetadataMethodSpecRow *rows,
                                                        TZrUInt32 count,
                                                        const SZrZrpMetadataMethodDefRow *methodRows,
                                                        TZrUInt32 methodCount,
                                                        const SZrZrpMetadataFieldDefRow *fieldRows,
                                                        TZrUInt32 fieldCount,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrZrpMetadataMethodSpecRow row = rows[index];
        if (backend_aot_c_zrp_remap_method_spec_row(&row,
                                                    methodRows,
                                                    methodCount,
                                                    fieldRows,
                                                    fieldCount,
                                                    functionTable,
                                                    retainedMethodDefCount)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

TZrBool backend_aot_c_zrp_remap_generic_param_owner_token(TZrMetadataToken *token,
                                                          const SZrZrpMetadataMethodDefRow *methodRows,
                                                          TZrUInt32 methodCount,
                                                          const SZrZrpMetadataFieldDefRow *fieldRows,
                                                          TZrUInt32 fieldCount,
                                                          const SZrAotFunctionTable *functionTable,
                                                          TZrUInt32 retainedMethodDefCount) {
    if (token == ZR_NULL || *token == 0u) {
        return ZR_FALSE;
    }
    if (backend_aot_c_zrp_token_is_type_def(*token)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_zrp_token_is_member_def(*token)) {
        return backend_aot_c_zrp_remap_member_def_token(token,
                                                       methodRows,
                                                       methodCount,
                                                       fieldRows,
                                                       fieldCount,
                                                       functionTable,
                                                       retainedMethodDefCount);
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_generic_param_row_is_retained(const SZrZrpMetadataGenericParamRow *row,
                                                        const SZrZrpMetadataMethodDefRow *methodRows,
                                                        TZrUInt32 methodCount,
                                                        const SZrZrpMetadataFieldDefRow *fieldRows,
                                                        TZrUInt32 fieldCount,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrUInt32 retainedMethodDefCount) {
    TZrMetadataToken ownerToken;

    if (row == ZR_NULL) {
        return ZR_FALSE;
    }

    ownerToken = row->ownerToken;
    return backend_aot_c_zrp_remap_generic_param_owner_token(&ownerToken,
                                                             methodRows,
                                                             methodCount,
                                                             fieldRows,
                                                             fieldCount,
                                                             functionTable,
                                                             retainedMethodDefCount);
}

TZrUInt32 backend_aot_c_zrp_count_retained_generic_params(const SZrZrpMetadataGenericParamRow *rows,
                                                          TZrUInt32 count,
                                                          const SZrZrpMetadataMethodDefRow *methodRows,
                                                          TZrUInt32 methodCount,
                                                          const SZrZrpMetadataFieldDefRow *fieldRows,
                                                          TZrUInt32 fieldCount,
                                                          const SZrAotFunctionTable *functionTable,
                                                          TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        if (backend_aot_c_zrp_generic_param_row_is_retained(&rows[index],
                                                            methodRows,
                                                            methodCount,
                                                            fieldRows,
                                                            fieldCount,
                                                            functionTable,
                                                            retainedMethodDefCount)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_generic_params_before(
        const SZrZrpMetadataGenericParamRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_generic_params(rows,
                                                           end,
                                                           methodRows,
                                                           methodCount,
                                                           fieldRows,
                                                           fieldCount,
                                                           functionTable,
                                                           retainedMethodDefCount);
}

static TZrBool backend_aot_c_zrp_compacted_generic_param_index(
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        TZrUInt32 genericParamIndex,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        TZrUInt32 *outIndex) {
    if (genericParamRows == ZR_NULL || outIndex == ZR_NULL || genericParamIndex >= genericParamCount) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_generic_param_row_is_retained(&genericParamRows[genericParamIndex],
                                                         methodRows,
                                                         methodCount,
                                                         fieldRows,
                                                         fieldCount,
                                                         functionTable,
                                                         retainedMethodDefCount)) {
        return ZR_FALSE;
    }

    *outIndex = backend_aot_c_zrp_count_retained_generic_params_before(genericParamRows,
                                                                       genericParamCount,
                                                                       genericParamIndex,
                                                                       methodRows,
                                                                       methodCount,
                                                                       fieldRows,
                                                                       fieldCount,
                                                                       functionTable,
                                                                       retainedMethodDefCount);
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_remap_generic_param_constraint_row(
        SZrZrpMetadataGenericParamConstraintRow *row,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 compactedGenericParamIndex;

    if (row == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_compacted_generic_param_index(genericParamRows,
                                                         genericParamCount,
                                                         row->genericParamIndex,
                                                         methodRows,
                                                         methodCount,
                                                         fieldRows,
                                                         fieldCount,
                                                         functionTable,
                                                         retainedMethodDefCount,
                                                         &compactedGenericParamIndex)) {
        return ZR_FALSE;
    }

    row->genericParamIndex = compactedGenericParamIndex;
    return ZR_TRUE;
}

TZrUInt32 backend_aot_c_zrp_count_retained_generic_param_constraints(
        const SZrZrpMetadataGenericParamConstraintRow *rows,
        TZrUInt32 count,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (rows == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrZrpMetadataGenericParamConstraintRow row = rows[index];
        if (backend_aot_c_zrp_remap_generic_param_constraint_row(&row,
                                                                 genericParamRows,
                                                                 genericParamCount,
                                                                 methodRows,
                                                                 methodCount,
                                                                 fieldRows,
                                                                 fieldCount,
                                                                 functionTable,
                                                                 retainedMethodDefCount)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_generic_param_constraints_before(
        const SZrZrpMetadataGenericParamConstraintRow *rows,
        TZrUInt32 count,
        TZrUInt32 exclusiveEnd,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 end = exclusiveEnd < count ? exclusiveEnd : count;
    return backend_aot_c_zrp_count_retained_generic_param_constraints(rows,
                                                                      end,
                                                                      genericParamRows,
                                                                      genericParamCount,
                                                                      methodRows,
                                                                      methodCount,
                                                                      fieldRows,
                                                                      fieldCount,
                                                                      functionTable,
                                                                      retainedMethodDefCount);
}

void backend_aot_c_zrp_adjust_generic_param_range(TZrUInt32 *firstGenericParamIndex,
                                                  TZrUInt32 *genericParamCount,
                                                  const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                  TZrUInt32 genericParamRowCount,
                                                  const SZrZrpMetadataMethodDefRow *methodRows,
                                                  TZrUInt32 methodCount,
                                                  const SZrZrpMetadataFieldDefRow *fieldRows,
                                                  TZrUInt32 fieldCount,
                                                  const SZrAotFunctionTable *functionTable,
                                                  TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    TZrUInt32 firstRetainedIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrUInt32 retainedCount = 0u;

    if (firstGenericParamIndex == ZR_NULL ||
        genericParamCount == ZR_NULL ||
        genericParamRows == ZR_NULL ||
        *genericParamCount == 0u) {
        return;
    }

    firstIndex = *firstGenericParamIndex;
    if (firstIndex >= genericParamRowCount) {
        *firstGenericParamIndex = 0u;
        *genericParamCount = 0u;
        return;
    }

    endIndex = firstIndex + *genericParamCount;
    if (endIndex < firstIndex || endIndex > genericParamRowCount) {
        endIndex = genericParamRowCount;
    }

    for (TZrUInt32 index = firstIndex; index < endIndex; index++) {
        if (!backend_aot_c_zrp_generic_param_row_is_retained(&genericParamRows[index],
                                                             methodRows,
                                                             methodCount,
                                                             fieldRows,
                                                             fieldCount,
                                                             functionTable,
                                                             retainedMethodDefCount)) {
            continue;
        }
        if (firstRetainedIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            firstRetainedIndex =
                    backend_aot_c_zrp_count_retained_generic_params_before(genericParamRows,
                                                                           genericParamRowCount,
                                                                           index,
                                                                           methodRows,
                                                                           methodCount,
                                                                           fieldRows,
                                                                           fieldCount,
                                                                           functionTable,
                                                                           retainedMethodDefCount);
        }
        retainedCount++;
    }

    if (retainedCount == 0u) {
        *firstGenericParamIndex = 0u;
        *genericParamCount = 0u;
    } else {
        *firstGenericParamIndex = firstRetainedIndex;
        *genericParamCount = retainedCount;
    }
}

void backend_aot_c_zrp_adjust_generic_param_constraint_range(
        TZrUInt32 *firstConstraintIndex,
        TZrUInt32 *constraintCount,
        const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
        TZrUInt32 constraintRowCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    TZrUInt32 firstRetainedIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrUInt32 retainedCount = 0u;

    if (firstConstraintIndex == ZR_NULL ||
        constraintCount == ZR_NULL ||
        constraintRows == ZR_NULL ||
        *constraintCount == 0u) {
        return;
    }

    firstIndex = *firstConstraintIndex;
    if (firstIndex >= constraintRowCount) {
        *firstConstraintIndex = 0u;
        *constraintCount = 0u;
        return;
    }

    endIndex = firstIndex + *constraintCount;
    if (endIndex < firstIndex || endIndex > constraintRowCount) {
        endIndex = constraintRowCount;
    }

    for (TZrUInt32 index = firstIndex; index < endIndex; index++) {
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
        if (firstRetainedIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            firstRetainedIndex =
                    backend_aot_c_zrp_count_retained_generic_param_constraints_before(constraintRows,
                                                                                      constraintRowCount,
                                                                                      index,
                                                                                      genericParamRows,
                                                                                      genericParamCount,
                                                                                      methodRows,
                                                                                      methodCount,
                                                                                      fieldRows,
                                                                                      fieldCount,
                                                                                      functionTable,
                                                                                      retainedMethodDefCount);
        }
        retainedCount++;
    }

    if (retainedCount == 0u) {
        *firstConstraintIndex = 0u;
        *constraintCount = 0u;
    } else {
        *firstConstraintIndex = firstRetainedIndex;
        *constraintCount = retainedCount;
    }
}

void backend_aot_c_zrp_adjust_type_def_method_range(SZrZrpMetadataTypeDefRow *row,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrAotFunctionTable *functionTable) {
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    TZrUInt32 firstRetainedIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrUInt32 retainedCount = 0u;

    if (row == ZR_NULL || methodRows == ZR_NULL || row->methodDefCount == 0u) {
        return;
    }

    firstIndex = row->firstMethodDefIndex;
    if (firstIndex >= methodCount) {
        row->firstMethodDefIndex = 0u;
        row->methodDefCount = 0u;
        return;
    }

    endIndex = firstIndex + row->methodDefCount;
    if (endIndex < firstIndex || endIndex > methodCount) {
        endIndex = methodCount;
    }

    for (TZrUInt32 index = firstIndex; index < endIndex; index++) {
        if (!backend_aot_c_zrp_method_def_row_is_retained(&methodRows[index], functionTable)) {
            continue;
        }
        if (firstRetainedIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            firstRetainedIndex =
                    backend_aot_c_zrp_count_retained_method_defs_before(methodRows,
                                                                        methodCount,
                                                                        index,
                                                                        functionTable);
        }
        retainedCount++;
    }

    if (retainedCount == 0u) {
        row->firstMethodDefIndex = 0u;
        row->methodDefCount = 0u;
    } else {
        row->firstMethodDefIndex = firstRetainedIndex;
        row->methodDefCount = retainedCount;
    }
}
