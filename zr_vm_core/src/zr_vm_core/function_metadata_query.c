#include "zr_vm_core/function.h"

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

const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataTokenRecord(const SZrFunction *function,
                                                                      TZrMetadataToken token) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_token_record(function->metadataTokenRecords,
                                            function->metadataTokenRecordLength,
                                            token);
}

const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataSignatureRecord(const SZrFunction *function,
                                                                          TZrMetadataToken entityToken) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_signature_record(function->metadataTokenRecords,
                                                function->metadataTokenRecordLength,
                                                entityToken);
}

const SZrMetadataTokenRecord *ZrCore_Function_FindModuleMetadataTokenRecord(const SZrFunction *function,
                                                                            TZrMetadataToken token) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return metadata_query_find_token_record(function->moduleMetadataTokenRecords,
                                            function->moduleMetadataTokenRecordLength,
                                            token);
}

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
