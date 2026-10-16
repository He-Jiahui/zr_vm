#include "zr_vm_core/function.h"

/* 在指定记录数组按完整 token 找首项；返回地址依附函数持有的数组。 */
static const SZrMetadataTokenRecord *metadata_query_find_token_record(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordLength,
        TZrMetadataToken token) {
    if (records == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordLength; index++) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

/* 实体记录指向签名后，还要求签名记录的 related/owner 双向指回实体。 */
static const SZrMetadataTokenRecord *metadata_query_find_signature_record(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordLength,
        TZrMetadataToken entityToken) {
    const SZrMetadataTokenRecord *entityRecord;

    if (records == ZR_NULL || entityToken == 0u) {
        return ZR_NULL;
    }

    entityRecord = metadata_query_find_token_record(records, recordLength, entityToken);
    if (entityRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(entityRecord->relatedToken) != ZR_METADATA_TABLE_SIGNATURE) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordLength; index++) {
        const SZrMetadataTokenRecord *record = &records[index];

        if (record->token == entityRecord->relatedToken &&
            ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
            record->relatedToken == entityToken &&
            record->ownerToken == entityToken) {
            return record;
        }
    }

    return ZR_NULL;
}

/* 仅扫描 metadataTokenRecords 主数组，不额外扫描导入记录副本；主数组本身也含导入实体。 */
const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataTokenRecord(const SZrFunction *function,
                                                                      TZrMetadataToken token) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_token_record(function->metadataTokenRecords,
                                            function->metadataTokenRecordLength,
                                            token);
}

/* 在 metadataTokenRecords 主数组查实体关联的双向签名，含该数组内的导入实体。 */
const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataSignatureRecord(const SZrFunction *function,
                                                                          TZrMetadataToken entityToken) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_signature_record(function->metadataTokenRecords,
                                                function->metadataTokenRecordLength,
                                                entityToken);
}

/* 仅查由主数组筛出的 moduleMetadataTokenRecords 导入副本，供跨模块解析。 */
const SZrMetadataTokenRecord *ZrCore_Function_FindModuleMetadataTokenRecord(const SZrFunction *function,
                                                                            TZrMetadataToken token) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_token_record(function->moduleMetadataTokenRecords,
                                            function->moduleMetadataTokenRecordLength,
                                            token);
}

/* 在导入模块记录内验证实体与签名的双向关联，返回借用指针。 */
const SZrMetadataTokenRecord *ZrCore_Function_FindModuleMetadataSignatureRecord(
        const SZrFunction *function,
        TZrMetadataToken entityToken) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_signature_record(function->moduleMetadataTokenRecords,
                                                function->moduleMetadataTokenRecordLength,
                                                entityToken);
}
