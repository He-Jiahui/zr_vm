#include "compiler_metadata_module_record.h"
#include "compiler_metadata_signature.h"
#include "compiler_metadata_type_def.h"

static TZrSize metadata_module_record_signature_size(void) {
    return 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

static void metadata_module_record_write_signature(TZrByte *buffer,
                                                   TZrSize *offset,
                                                   const SZrFunction *function,
                                                   const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                   TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_MODULE);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    function != ZR_NULL ? function->functionName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    function != ZR_NULL ? function->moduleVersion : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
}

TZrBool compiler_metadata_module_record_collect_strings(SZrCompilerState *cs,
                                                        const SZrFunction *function,
                                                        TZrMetadataTypeDefStringCollector collector,
                                                        void *userData) {
    if (collector == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!collector(cs, function->functionName, userData)) {
        return ZR_FALSE;
    }
    return collector(cs, function->moduleVersion, userData);
}

TZrBool compiler_metadata_module_record_plan(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataModuleRecordPlan *outPlan) {
    ZR_UNUSED_PARAMETER(cs);

    if (outPlan == ZR_NULL) {
        return ZR_FALSE;
    }
    outPlan->moduleRecordCount = 0u;
    outPlan->signatureHeapLength = 0u;
    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    outPlan->moduleRecordCount = 1u;
    outPlan->signatureHeapLength = metadata_module_record_signature_size();
    return ZR_TRUE;
}

TZrBool compiler_metadata_module_record_emit(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataTokenRecord *records,
                                             TZrUInt32 recordCount,
                                             TZrUInt32 *ioRecordIndex,
                                             TZrByte *heap,
                                             TZrSize heapLength,
                                             TZrSize *ioHeapOffset,
                                             TZrUInt32 *ioSignatureRidCursor,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount) {
    TZrSize signatureStart;
    TZrSize expectedLength;
    TZrUInt64 signatureHash;
    TZrUInt32 recordIndex;
    TZrMetadataToken moduleToken;
    TZrMetadataToken signatureToken;

    ZR_UNUSED_PARAMETER(cs);

    if (function == ZR_NULL || records == ZR_NULL || ioRecordIndex == ZR_NULL || heap == ZR_NULL ||
        ioHeapOffset == ZR_NULL || ioSignatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedLength = metadata_module_record_signature_size();
    if (*ioHeapOffset > heapLength || expectedLength > heapLength - *ioHeapOffset ||
        *ioRecordIndex + 1u >= recordCount) {
        return ZR_FALSE;
    }

    signatureStart = *ioHeapOffset;
    metadata_module_record_write_signature(heap,
                                           ioHeapOffset,
                                           function,
                                           stringHeapEntries,
                                           stringHeapEntryCount);
    if (*ioHeapOffset - signatureStart != expectedLength) {
        return ZR_FALSE;
    }

    signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
    if (signatureHash == 0u) {
        return ZR_FALSE;
    }

    moduleToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*ioSignatureRidCursor)++);
    recordIndex = *ioRecordIndex;
    records[recordIndex].token = moduleToken;
    records[recordIndex].relatedToken = signatureToken;
    records[recordIndex].ownerToken = 0u;
    records[recordIndex].ownerIndex = 0u;
    records[recordIndex].signatureBlobOffset = (TZrUInt32)signatureStart;
    records[recordIndex].signatureBlobLength = (TZrUInt32)expectedLength;
    records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    records[recordIndex].token = signatureToken;
    records[recordIndex].relatedToken = moduleToken;
    records[recordIndex].ownerToken = moduleToken;
    records[recordIndex].ownerIndex = 0u;
    records[recordIndex].signatureBlobOffset = (TZrUInt32)signatureStart;
    records[recordIndex].signatureBlobLength = (TZrUInt32)expectedLength;
    records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    *ioRecordIndex = recordIndex;
    return ZR_TRUE;
}
