#include "zr_vm_core/function.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

static TZrBool metadata_record_blob_is_valid(const SZrFunction *function,
                                             const SZrMetadataTokenRecord *record) {
    return function != ZR_NULL &&
           record != ZR_NULL &&
           function->signatureBlobHeap != ZR_NULL &&
           record->signatureBlobLength > 0u &&
           record->signatureBlobOffset < function->signatureBlobHeapLength &&
           record->signatureBlobLength <= function->signatureBlobHeapLength - record->signatureBlobOffset;
}

static TZrBool metadata_record_signature_blobs_equal(const SZrFunction *leftFunction,
                                                     const SZrMetadataTokenRecord *leftRecord,
                                                     const SZrFunction *rightFunction,
                                                     const SZrMetadataTokenRecord *rightRecord) {
    if (!metadata_record_blob_is_valid(leftFunction, leftRecord) ||
        !metadata_record_blob_is_valid(rightFunction, rightRecord) ||
        leftRecord->signatureBlobLength != rightRecord->signatureBlobLength) {
        return ZR_FALSE;
    }

    return ZrCore_Memory_RawCompare(leftFunction->signatureBlobHeap + leftRecord->signatureBlobOffset,
                                    rightFunction->signatureBlobHeap + rightRecord->signatureBlobOffset,
                                    leftRecord->signatureBlobLength) == 0
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static const SZrMetadataTokenRecord *metadata_find_token_record(
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

static const SZrMetadataTokenRecord *metadata_find_signature_record(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordLength,
        TZrMetadataToken entityToken) {
    const SZrMetadataTokenRecord *entityRecord;

    if (records == ZR_NULL || entityToken == 0u) {
        return ZR_NULL;
    }

    entityRecord = metadata_find_token_record(records, recordLength, entityToken);
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

static const SZrMetadataTokenRecord *function_find_type_spec_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeSpecRecord) {
    return typeSpecRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeSpecRecord->token);
}

static const SZrMetadataTokenRecord *function_find_matching_type_spec_record(
        const SZrFunction *providerFunction,
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeSpecRecord) {
    if (providerFunction == ZR_NULL ||
        callerFunction == ZR_NULL ||
        callerTypeSpecRecord == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < providerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *providerRecord = &providerFunction->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(providerRecord->token) != ZR_METADATA_TABLE_TYPE_SPEC ||
            providerRecord->signatureHash != callerTypeSpecRecord->signatureHash) {
            continue;
        }
        if (metadata_record_signature_blobs_equal(callerFunction,
                                                  callerTypeSpecRecord,
                                                  providerFunction,
                                                  providerRecord)) {
            return providerRecord;
        }
    }

    return ZR_NULL;
}

static SZrString *function_metadata_string_heap_lookup(const SZrFunction *function,
                                                       TZrUInt32 stringIndex) {
    if (function == ZR_NULL ||
        function->metadataStringHeap == ZR_NULL ||
        stringIndex == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == stringIndex) {
            return function->metadataStringHeap[index].value;
        }
    }

    return ZR_NULL;
}

static TZrBool function_metadata_read_string_ref(const SZrFunction *function,
                                                 const TZrByte *blob,
                                                 TZrSize blobLength,
                                                 TZrSize *cursor,
                                                 SZrString **outString) {
    TZrUInt32 stringIndex;

    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (blob == ZR_NULL || cursor == ZR_NULL || outString == ZR_NULL ||
        *cursor + sizeof(TZrUInt32) > blobLength) {
        return ZR_FALSE;
    }

    stringIndex = ((TZrUInt32)blob[*cursor]) |
                  ((TZrUInt32)blob[*cursor + 1u] << 8) |
                  ((TZrUInt32)blob[*cursor + 2u] << 16) |
                  ((TZrUInt32)blob[*cursor + 3u] << 24);
    *cursor += sizeof(TZrUInt32);
    *outString = function_metadata_string_heap_lookup(function, stringIndex);
    return *outString != ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

static TZrBool function_type_spec_union_base_name(const SZrFunction *function,
                                                  const SZrMetadataTokenRecord *typeSpecRecord,
                                                  SZrString **outBaseName) {
    const TZrByte *blob;
    TZrSize cursor = 0;

    if (outBaseName != ZR_NULL) {
        *outBaseName = ZR_NULL;
    }
    if (!metadata_record_blob_is_valid(function, typeSpecRecord) || outBaseName == ZR_NULL) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + typeSpecRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_UNION) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (cursor + sizeof(TZrUInt32) > typeSpecRecord->signatureBlobLength) {
        return ZR_FALSE;
    }
    cursor += sizeof(TZrUInt32);
    return function_metadata_read_string_ref(function,
                                             blob,
                                             typeSpecRecord->signatureBlobLength,
                                             &cursor,
                                             outBaseName);
}

static TZrBool function_type_def_base_name_matches(const SZrFunction *function,
                                                   const SZrMetadataTokenRecord *typeDefRecord,
                                                   const SZrString *baseName) {
    const TZrByte *blob;
    TZrSize cursor = 0;
    SZrString *typeDefName = ZR_NULL;

    if (!metadata_record_blob_is_valid(function, typeDefRecord) || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + typeDefRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (!function_metadata_read_string_ref(function,
                                           blob,
                                           typeDefRecord->signatureBlobLength,
                                           &cursor,
                                           &typeDefName)) {
        return ZR_FALSE;
    }

    return typeDefName == baseName || ZrCore_String_Equal(typeDefName, (SZrString *)baseName)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static const SZrMetadataTokenRecord *function_find_type_def_for_union_type_spec(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeSpecRecord) {
    SZrString *baseName = ZR_NULL;

    if (function == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        !function_type_spec_union_base_name(function, typeSpecRecord, &baseName)) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            function_type_def_base_name_matches(function, record, baseName)) {
            return record;
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *function_find_type_def_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeDefRecord) {
    return typeDefRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeDefRecord->token);
}

static const SZrMetadataTokenRecord *function_find_type_ref_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeRefRecord) {
    return typeRefRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeRefRecord->token);
}

static TZrBool function_type_ref_base_name_matches_type_def(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord) {
    const TZrByte *blob;
    TZrSize cursor = 0;
    SZrString *typeRefName = ZR_NULL;

    if (!metadata_record_blob_is_valid(callerFunction, callerTypeRefRecord)) {
        return ZR_FALSE;
    }

    blob = callerFunction->signatureBlobHeap + callerTypeRefRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_TYPE_REF) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (cursor + sizeof(TZrUInt32) > callerTypeRefRecord->signatureBlobLength) {
        return ZR_FALSE;
    }
    cursor += sizeof(TZrUInt32);
    if (!function_metadata_read_string_ref(callerFunction,
                                           blob,
                                           callerTypeRefRecord->signatureBlobLength,
                                           &cursor,
                                           &typeRefName)) {
        return ZR_TRUE;
    }

    return function_type_def_base_name_matches(providerFunction, providerTypeDefRecord, typeRefName);
}

typedef enum EZrMetadataTypeRefMatchResult {
    ZR_METADATA_TYPE_REF_MATCH_NONE = 0,
    ZR_METADATA_TYPE_REF_MATCH_OK,
    ZR_METADATA_TYPE_REF_MATCH_UNMATCHED,
    ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH,
    ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH
} EZrMetadataTypeRefMatchResult;

static const SZrMetadataTokenRecord *function_find_targeted_type_ref_provider_type_def(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrFunction *providerFunction,
        EZrMetadataTypeRefMatchResult *outResult) {
    const SZrMetadataTokenRecord *targetRecord = ZR_NULL;

    if (outResult != ZR_NULL) {
        *outResult = ZR_METADATA_TYPE_REF_MATCH_NONE;
    }
    if (callerFunction == ZR_NULL ||
        callerTypeRefRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(callerTypeRefRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < providerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *providerRecord = &providerFunction->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(providerRecord->token) != ZR_METADATA_TABLE_TYPE_DEF ||
            providerRecord->token != callerTypeRefRecord->targetMetadataToken) {
            continue;
        }
        targetRecord = providerRecord;
        if (callerTypeRefRecord->targetSignatureHash == 0u ||
            providerRecord->signatureHash != callerTypeRefRecord->targetSignatureHash) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if (callerTypeRefRecord->targetSignatureToken != 0u &&
            providerRecord->relatedToken != callerTypeRefRecord->targetSignatureToken) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if (callerTypeRefRecord->targetModuleSignatureHash != 0u &&
            providerFunction->moduleSignatureHash != callerTypeRefRecord->targetModuleSignatureHash) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if ((callerTypeRefRecord->layoutVersion != 0u || callerTypeRefRecord->layoutHash != 0u ||
             providerRecord->layoutVersion != 0u || providerRecord->layoutHash != 0u) &&
            (callerTypeRefRecord->layoutVersion != providerRecord->layoutVersion ||
             callerTypeRefRecord->layoutHash != providerRecord->layoutHash)) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH;
            }
            return providerRecord;
        }
        if (!function_type_ref_base_name_matches_type_def(callerFunction,
                                                          callerTypeRefRecord,
                                                          providerFunction,
                                                          providerRecord)) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }

        if (outResult != ZR_NULL) {
            *outResult = ZR_METADATA_TYPE_REF_MATCH_OK;
        }
        return providerRecord;
    }

    if (outResult != ZR_NULL) {
        *outResult = targetRecord == ZR_NULL
                             ? ZR_METADATA_TYPE_REF_MATCH_UNMATCHED
                             : ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
    }
    return ZR_NULL;
}

static TZrBool function_record_type_spec_binding(SZrState *state,
                                                 SZrFunction *callerFunction,
                                                 const SZrMetadataTokenRecord *callerTypeSpecRecord,
                                                 const SZrMetadataTokenRecord *callerSignatureRecord,
                                                 const SZrFunction *providerFunction,
                                                 const SZrMetadataTokenRecord *providerTypeSpecRecord,
                                                 const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeSpecRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeSpecRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeSpecRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeSpecRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeSpecRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeSpecRecord->token;
    binding->expectedSignatureToken = callerSignatureRecord->token;
    binding->expectedSignatureHash = callerTypeSpecRecord->signatureHash;
    binding->expectedModuleSignatureHash = 0u;
    binding->expectedLayoutVersion = 0u;
    binding->expectedLayoutHash = 0u;
    binding->resolvedMetadataToken = providerTypeSpecRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeSpecRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = 0u;
    binding->resolvedLayoutHash = 0u;
    return ZR_TRUE;
}

static TZrBool function_record_type_def_layout_binding(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeDefRecord,
        const SZrMetadataTokenRecord *callerSignatureRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord,
        const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeDefRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeDefRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeDefRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeDefRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeDefRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeDefRecord->token;
    binding->expectedSignatureToken = callerSignatureRecord->token;
    binding->expectedSignatureHash = callerTypeDefRecord->signatureHash;
    binding->expectedModuleSignatureHash = 0u;
    binding->expectedLayoutVersion = callerTypeDefRecord->layoutVersion;
    binding->expectedLayoutHash = callerTypeDefRecord->layoutHash;
    binding->resolvedMetadataToken = providerTypeDefRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeDefRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = providerTypeDefRecord->layoutVersion;
    binding->resolvedLayoutHash = providerTypeDefRecord->layoutHash;
    return ZR_TRUE;
}

static TZrBool function_record_type_ref_binding(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrMetadataTokenRecord *callerSignatureRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord,
        const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeRefRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeDefRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeRefRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeRefRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeRefRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeRefRecord->targetMetadataToken;
    binding->expectedSignatureToken = callerTypeRefRecord->targetSignatureToken;
    binding->expectedSignatureHash = callerTypeRefRecord->targetSignatureHash;
    binding->expectedModuleSignatureHash = callerTypeRefRecord->targetModuleSignatureHash;
    binding->expectedLayoutVersion = callerTypeRefRecord->layoutVersion;
    binding->expectedLayoutHash = callerTypeRefRecord->layoutHash;
    binding->resolvedMetadataToken = providerTypeDefRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeDefRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = providerTypeDefRecord->layoutVersion;
    binding->resolvedLayoutHash = providerTypeDefRecord->layoutHash;
    return ZR_TRUE;
}

const SZrMetadataTokenBinding *ZrCore_Function_FindModuleMetadataBinding(const SZrFunction *function,
                                                                         TZrMetadataToken refToken) {
    if (function == ZR_NULL || refToken == 0u || function->moduleMetadataBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataBindingLength; index++) {
        const SZrMetadataTokenBinding *binding = &function->moduleMetadataBindings[index];

        if (binding->refToken == refToken) {
            return binding;
        }
    }

    return ZR_NULL;
}

SZrMetadataTokenBinding *ZrCore_Function_UpsertModuleMetadataBinding(SZrState *state,
                                                                     SZrFunction *function,
                                                                     TZrMetadataToken refToken) {
    SZrMetadataTokenBinding *newBindings;
    TZrUInt32 newCapacity;
    TZrSize newBytes;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || refToken == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataBindingLength; index++) {
        if (function->moduleMetadataBindings[index].refToken == refToken) {
            return &function->moduleMetadataBindings[index];
        }
    }

    if (function->moduleMetadataBindingLength >= function->moduleMetadataBindingCapacity) {
        newCapacity = function->moduleMetadataBindingCapacity == 0u
                              ? 4u
                              : function->moduleMetadataBindingCapacity * 2u;
        if (newCapacity <= function->moduleMetadataBindingCapacity) {
            return ZR_NULL;
        }

        newBytes = sizeof(SZrMetadataTokenBinding) * (TZrSize)newCapacity;
        newBindings = (SZrMetadataTokenBinding *)ZrCore_Memory_RawMallocWithType(
                state->global,
                newBytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newBindings == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Memory_RawSet(newBindings, 0, newBytes);
        if (function->moduleMetadataBindings != ZR_NULL && function->moduleMetadataBindingCapacity > 0u) {
            ZrCore_Memory_RawCopy(newBindings,
                                  function->moduleMetadataBindings,
                                  sizeof(SZrMetadataTokenBinding) *
                                          (TZrSize)function->moduleMetadataBindingLength);
            ZrCore_Memory_RawFreeWithType(state->global,
                                          function->moduleMetadataBindings,
                                          sizeof(SZrMetadataTokenBinding) *
                                                  (TZrSize)function->moduleMetadataBindingCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }

        function->moduleMetadataBindings = newBindings;
        function->moduleMetadataBindingCapacity = newCapacity;
    }

    return &function->moduleMetadataBindings[function->moduleMetadataBindingLength++];
}

TZrBool ZrCore_Function_BindMatchingTypeSpecMetadata(SZrState *state,
                                                     SZrFunction *callerFunction,
                                                     const SZrFunction *providerFunction) {
    SZrMetadataTypeSpecBindStatus status;

    if (!ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                                callerFunction,
                                                                providerFunction,
                                                                &status)) {
        return status.unmatchedTypeSpecCount > 0u &&
                       status.definitionMismatchCount == 0u &&
                       status.layoutMismatchCount == 0u
                       ? ZR_TRUE
                       : ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeSpecBindStatus *status) {
    if (state == ZR_NULL ||
        callerFunction == ZR_NULL ||
        providerFunction == ZR_NULL ||
        callerFunction->metadataTokenRecords == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        if (status != ZR_NULL) {
            ZrCore_Memory_RawSet(status, 0, sizeof(*status));
        }
        return ZR_FALSE;
    }
    if (status != ZR_NULL) {
        ZrCore_Memory_RawSet(status, 0, sizeof(*status));
    }

    for (TZrUInt32 index = 0; index < callerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *callerRecord = &callerFunction->metadataTokenRecords[index];
        const SZrMetadataTokenRecord *providerRecord;
        const SZrMetadataTokenRecord *callerSignatureRecord;
        const SZrMetadataTokenRecord *providerSignatureRecord;
        const SZrMetadataTokenRecord *callerTypeDefRecord;
        const SZrMetadataTokenRecord *providerTypeDefRecord;
        const SZrMetadataTokenRecord *callerTypeDefSignatureRecord;
        const SZrMetadataTokenRecord *providerTypeDefSignatureRecord;

        if (ZR_METADATA_TOKEN_TABLE(callerRecord->token) != ZR_METADATA_TABLE_TYPE_SPEC) {
            continue;
        }
        if (status != ZR_NULL) {
            status->callerTypeSpecCount++;
        }

        providerRecord = function_find_matching_type_spec_record(providerFunction,
                                                                 callerFunction,
                                                                 callerRecord);
        if (providerRecord == ZR_NULL) {
            if (status != ZR_NULL) {
                status->unmatchedTypeSpecCount++;
                if (status->firstUnmatchedTypeSpecToken == 0u) {
                    status->firstUnmatchedTypeSpecToken = callerRecord->token;
                    status->firstUnmatchedSignatureHash = callerRecord->signatureHash;
                }
            }
            continue;
        }

        callerSignatureRecord = function_find_type_spec_signature_record(callerFunction, callerRecord);
        providerSignatureRecord = function_find_type_spec_signature_record(providerFunction, providerRecord);
        if (callerSignatureRecord == ZR_NULL || providerSignatureRecord == ZR_NULL) {
            return ZR_FALSE;
        }
        callerTypeDefRecord = function_find_type_def_for_union_type_spec(callerFunction, callerRecord);
        providerTypeDefRecord = function_find_type_def_for_union_type_spec(providerFunction, providerRecord);
        if (callerTypeDefRecord != ZR_NULL || providerTypeDefRecord != ZR_NULL) {
            if (callerTypeDefRecord == ZR_NULL || providerTypeDefRecord == ZR_NULL) {
                if (status != ZR_NULL) {
                    status->definitionMismatchCount++;
                    if (status->firstDefinitionMismatchTypeSpecToken == 0u) {
                        status->firstDefinitionMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedDefinitionSignatureHash =
                                callerTypeDefRecord != ZR_NULL ? callerTypeDefRecord->signatureHash : 0u;
                        status->firstActualDefinitionSignatureHash =
                                providerTypeDefRecord != ZR_NULL ? providerTypeDefRecord->signatureHash : 0u;
                    }
                }
                continue;
            }
            if (callerTypeDefRecord->signatureHash != providerTypeDefRecord->signatureHash ||
                !metadata_record_signature_blobs_equal(callerFunction,
                                                       callerTypeDefRecord,
                                                       providerFunction,
                                                       providerTypeDefRecord)) {
                if (status != ZR_NULL) {
                    status->definitionMismatchCount++;
                    if (status->firstDefinitionMismatchTypeSpecToken == 0u) {
                        status->firstDefinitionMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedDefinitionSignatureHash = callerTypeDefRecord->signatureHash;
                        status->firstActualDefinitionSignatureHash = providerTypeDefRecord->signatureHash;
                    }
                }
                continue;
            }
            if (callerTypeDefRecord->layoutVersion != providerTypeDefRecord->layoutVersion ||
                callerTypeDefRecord->layoutHash != providerTypeDefRecord->layoutHash) {
                if (status != ZR_NULL) {
                    status->layoutMismatchCount++;
                    if (status->firstLayoutMismatchTypeSpecToken == 0u) {
                        status->firstLayoutMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedLayoutVersion = callerTypeDefRecord->layoutVersion;
                        status->firstExpectedLayoutHash = callerTypeDefRecord->layoutHash;
                        status->firstActualLayoutVersion = providerTypeDefRecord->layoutVersion;
                        status->firstActualLayoutHash = providerTypeDefRecord->layoutHash;
                    }
                }
                continue;
            }
        }

        if (!function_record_type_spec_binding(state,
                                               callerFunction,
                                               callerRecord,
                                               callerSignatureRecord,
                                               providerFunction,
                                               providerRecord,
                                               providerSignatureRecord)) {
            return ZR_FALSE;
        }
        if (callerTypeDefRecord != ZR_NULL || providerTypeDefRecord != ZR_NULL) {
            callerTypeDefSignatureRecord = function_find_type_def_signature_record(callerFunction,
                                                                                   callerTypeDefRecord);
            providerTypeDefSignatureRecord = function_find_type_def_signature_record(providerFunction,
                                                                                     providerTypeDefRecord);
            if (callerTypeDefSignatureRecord == ZR_NULL ||
                providerTypeDefSignatureRecord == ZR_NULL ||
                !function_record_type_def_layout_binding(state,
                                                         callerFunction,
                                                         callerTypeDefRecord,
                                                         callerTypeDefSignatureRecord,
                                                         providerFunction,
                                                         providerTypeDefRecord,
                                                         providerTypeDefSignatureRecord)) {
                return ZR_FALSE;
            }
        }
        if (status != ZR_NULL) {
            status->matchedTypeSpecCount++;
        }
    }

    return status == ZR_NULL ||
                   (status->unmatchedTypeSpecCount == 0u &&
                    status->definitionMismatchCount == 0u &&
                    status->layoutMismatchCount == 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

TZrBool ZrCore_Function_BindMatchingTypeRefMetadata(SZrState *state,
                                                    SZrFunction *callerFunction,
                                                    const SZrFunction *providerFunction) {
    SZrMetadataTypeRefBindStatus status;

    if (!ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(state,
                                                               callerFunction,
                                                               providerFunction,
                                                               &status)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeRefBindStatus *status) {
    if (state == ZR_NULL ||
        callerFunction == ZR_NULL ||
        providerFunction == ZR_NULL ||
        callerFunction->metadataTokenRecords == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        if (status != ZR_NULL) {
            ZrCore_Memory_RawSet(status, 0, sizeof(*status));
        }
        return ZR_FALSE;
    }
    if (status != ZR_NULL) {
        ZrCore_Memory_RawSet(status, 0, sizeof(*status));
    }

    for (TZrUInt32 index = 0; index < callerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *callerRecord = &callerFunction->metadataTokenRecords[index];
        const SZrMetadataTokenRecord *callerSignatureRecord;
        const SZrMetadataTokenRecord *providerRecord;
        const SZrMetadataTokenRecord *providerSignatureRecord;
        EZrMetadataTypeRefMatchResult matchResult;

        if (ZR_METADATA_TOKEN_TABLE(callerRecord->token) != ZR_METADATA_TABLE_TYPE_REF ||
            ZR_METADATA_TOKEN_TABLE(callerRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF) {
            continue;
        }
        if (status != ZR_NULL) {
            status->callerTypeRefCount++;
        }

        callerSignatureRecord = function_find_type_ref_signature_record(callerFunction, callerRecord);
        providerRecord = function_find_targeted_type_ref_provider_type_def(callerFunction,
                                                                           callerRecord,
                                                                           providerFunction,
                                                                           &matchResult);
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_UNMATCHED) {
            if (status != ZR_NULL) {
                status->unmatchedTypeRefCount++;
                if (status->firstUnmatchedTypeRefToken == 0u) {
                    status->firstUnmatchedTypeRefToken = callerRecord->token;
                    status->firstUnmatchedSignatureHash = callerRecord->targetSignatureHash;
                }
            }
            continue;
        }
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH) {
            if (status != ZR_NULL) {
                status->definitionMismatchCount++;
                if (status->firstDefinitionMismatchTypeRefToken == 0u) {
                    status->firstDefinitionMismatchTypeRefToken = callerRecord->token;
                    status->firstExpectedDefinitionSignatureHash = callerRecord->targetSignatureHash;
                    status->firstActualDefinitionSignatureHash =
                            providerRecord != ZR_NULL ? providerRecord->signatureHash : 0u;
                }
            }
            continue;
        }
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH) {
            if (status != ZR_NULL) {
                status->layoutMismatchCount++;
                if (status->firstLayoutMismatchTypeRefToken == 0u) {
                    status->firstLayoutMismatchTypeRefToken = callerRecord->token;
                    status->firstExpectedLayoutVersion = callerRecord->layoutVersion;
                    status->firstExpectedLayoutHash = callerRecord->layoutHash;
                    status->firstActualLayoutVersion = providerRecord != ZR_NULL ? providerRecord->layoutVersion : 0u;
                    status->firstActualLayoutHash = providerRecord != ZR_NULL ? providerRecord->layoutHash : 0u;
                }
            }
            continue;
        }
        providerSignatureRecord = function_find_type_def_signature_record(providerFunction, providerRecord);
        if (callerSignatureRecord == ZR_NULL ||
            providerRecord == ZR_NULL ||
            providerSignatureRecord == ZR_NULL ||
            !function_record_type_ref_binding(state,
                                              callerFunction,
                                              callerRecord,
                                              callerSignatureRecord,
                                              providerFunction,
                                              providerRecord,
                                              providerSignatureRecord)) {
            return ZR_FALSE;
        }
        if (status != ZR_NULL) {
            status->matchedTypeRefCount++;
        }
    }

    return status == ZR_NULL ||
                   (status->unmatchedTypeRefCount == 0u &&
                    status->definitionMismatchCount == 0u &&
                    status->layoutMismatchCount == 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}
