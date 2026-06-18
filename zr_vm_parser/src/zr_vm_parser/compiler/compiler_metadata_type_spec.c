#include "compiler_metadata_type_spec.h"
#include "compiler_metadata_signature.h"

#include <string.h>

typedef struct SZrMetadataTypeSpecScanContext {
    SZrCompilerState *compiler;
    TZrUInt32 count;
    TZrSize signatureHeapLength;
    TZrByte *emitHeap;
    TZrSize emitHeapLength;
    TZrSize *emitHeapOffset;
    SZrMetadataTokenRecord *records;
    TZrUInt32 recordCount;
    TZrUInt32 *recordIndex;
    TZrUInt32 *signatureRidCursor;
    TZrUInt32 typeSpecRecordStartIndex;
    TZrUInt32 emittedCount;
    struct SZrMetadataTypeSpecUniqueEntry *uniqueEntries;
    TZrUInt32 uniqueEntryCapacity;
    const SZrMetadataStringHeapEntry *stringHeapEntries;
    TZrUInt32 stringHeapEntryCount;
} SZrMetadataTypeSpecScanContext;

typedef struct SZrMetadataTypeSpecUniqueEntry {
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrMetadataTypeSpecUniqueEntry;

static TZrBool metadata_type_spec_requires_record(const SZrFunctionTypedTypeRef *typeRef) {
    TZrNativeString typeNameText;

    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeRef->isNullable ||
        typeRef->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeRef->isArray) {
        return ZR_TRUE;
    }

    typeNameText = typeRef->typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeRef->typeName) : ZR_NULL;
    return typeNameText != ZR_NULL && strchr(typeNameText, '<') != ZR_NULL;
}

static TZrBool metadata_type_spec_write_record_pair(SZrMetadataTypeSpecScanContext *context,
                                                    TZrUInt32 ownerIndex,
                                                    TZrUInt32 signatureStart,
                                                    TZrUInt32 signatureLength,
                                                    TZrUInt64 signatureHash) {
    TZrUInt32 recordIndex;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;

    if (context == ZR_NULL ||
        context->records == ZR_NULL ||
        context->recordIndex == ZR_NULL ||
        context->signatureRidCursor == ZR_NULL ||
        *context->recordIndex + 1u >= context->recordCount) {
        return ZR_FALSE;
    }

    typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, context->emittedCount + 1u);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*context->signatureRidCursor)++);

    recordIndex = *context->recordIndex;
    context->records[recordIndex].token = typeSpecToken;
    context->records[recordIndex].relatedToken = signatureToken;
    context->records[recordIndex].ownerToken = 0;
    context->records[recordIndex].ownerIndex = ownerIndex;
    context->records[recordIndex].signatureBlobOffset = signatureStart;
    context->records[recordIndex].signatureBlobLength = signatureLength;
    context->records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    context->records[recordIndex].token = signatureToken;
    context->records[recordIndex].relatedToken = typeSpecToken;
    context->records[recordIndex].ownerToken = typeSpecToken;
    context->records[recordIndex].ownerIndex = ownerIndex;
    context->records[recordIndex].signatureBlobOffset = signatureStart;
    context->records[recordIndex].signatureBlobLength = signatureLength;
    context->records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    *context->recordIndex = recordIndex;
    context->emittedCount++;
    return ZR_TRUE;
}

static TZrBool metadata_type_spec_signature_seen_before(const SZrMetadataTypeSpecScanContext *context,
                                                        const TZrByte *signature,
                                                        TZrSize signatureLength) {
    if (context == ZR_NULL ||
        context->emitHeap == ZR_NULL ||
        signature == ZR_NULL ||
        signatureLength == 0) {
        return ZR_FALSE;
    }

    if (context->uniqueEntries != ZR_NULL) {
        for (TZrUInt32 index = 0; index < context->emittedCount; index++) {
            const SZrMetadataTypeSpecUniqueEntry *entry = &context->uniqueEntries[index];

            if (entry->signatureBlobLength == signatureLength &&
                entry->signatureBlobOffset + entry->signatureBlobLength <= context->emitHeapLength &&
                ZrCore_Memory_RawCompare(context->emitHeap + entry->signatureBlobOffset,
                                         (TZrPtr)signature,
                                         signatureLength) == 0) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    if (context->records != ZR_NULL) {
        for (TZrUInt32 index = 0; index < context->emittedCount; index++) {
            TZrUInt32 recordIndex = context->typeSpecRecordStartIndex + index * 2u;
            const SZrMetadataTokenRecord *record;

            if (recordIndex >= context->recordCount) {
                return ZR_FALSE;
            }

            record = &context->records[recordIndex];
            if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_SPEC &&
                record->signatureBlobLength == signatureLength &&
                record->signatureBlobOffset + record->signatureBlobLength <= context->emitHeapLength &&
                ZrCore_Memory_RawCompare(context->emitHeap + record->signatureBlobOffset,
                                         (TZrPtr)signature,
                                         signatureLength) == 0) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_type_spec_remember_signature(SZrMetadataTypeSpecScanContext *context,
                                                     TZrSize signatureStart,
                                                     TZrSize signatureLength) {
    if (context == ZR_NULL || context->uniqueEntries == ZR_NULL) {
        return ZR_TRUE;
    }
    if (context->emittedCount >= context->uniqueEntryCapacity ||
        signatureStart > (TZrSize)0xFFFFFFFFu ||
        signatureLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    context->uniqueEntries[context->emittedCount].signatureBlobOffset = (TZrUInt32)signatureStart;
    context->uniqueEntries[context->emittedCount].signatureBlobLength = (TZrUInt32)signatureLength;
    context->emittedCount++;
    return ZR_TRUE;
}

static TZrBool metadata_type_spec_visit_type(SZrMetadataTypeSpecScanContext *context,
                                             const SZrFunctionTypedTypeRef *typeRef,
                                             TZrUInt32 ownerIndex) {
    TZrSize signatureLength;

    if (context == ZR_NULL || context->compiler == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_type_spec_requires_record(typeRef)) {
        return ZR_TRUE;
    }

    signatureLength = metadata_token_type_ref_signature_size(context->compiler, typeRef);
    if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    if (context->emitHeap == ZR_NULL) {
        if (context->signatureHeapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
            return ZR_FALSE;
        }
        context->signatureHeapLength += signatureLength;
        context->count++;
        return ZR_TRUE;
    }

    {
        TZrSize signatureStart = *context->emitHeapOffset;
        TZrSize writeOffset = 0;
        TZrUInt64 signatureHash;
        TZrByte *candidate;
        SZrGlobalState *global;

        if (context->compiler->state == ZR_NULL || context->compiler->state->global == ZR_NULL) {
            return ZR_FALSE;
        }

        global = context->compiler->state->global;
        candidate = (TZrByte *)ZrCore_Memory_RawMallocWithType(global,
                                                               signatureLength,
                                                               ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (candidate == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(candidate, 0, signatureLength);
        metadata_token_write_type_ref_signature(candidate,
                                                &writeOffset,
                                                context->compiler,
                                                typeRef,
                                                context->stringHeapEntries,
                                                context->stringHeapEntryCount);
        if (writeOffset != signatureLength) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        if (metadata_type_spec_signature_seen_before(context,
                                                     candidate,
                                                     signatureLength)) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_TRUE;
        }

        if (signatureStart > context->emitHeapLength ||
            signatureLength > context->emitHeapLength - signatureStart) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawCopy(context->emitHeap + signatureStart, candidate, signatureLength);
        signatureHash = metadata_signature_hash_v1(candidate, signatureLength);
        if (signatureHash == 0) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
        if (context->records != ZR_NULL &&
            !metadata_type_spec_write_record_pair(context,
                                                  ownerIndex,
                                                  (TZrUInt32)signatureStart,
                                                  (TZrUInt32)signatureLength,
                                                  signatureHash)) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
        if (!metadata_type_spec_remember_signature(context, signatureStart, signatureLength)) {
            ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        *context->emitHeapOffset = signatureStart + signatureLength;
        ZrCore_Memory_RawFreeWithType(global, candidate, signatureLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_spec_visit_export(SZrMetadataTypeSpecScanContext *context,
                                               const SZrFunctionTypedExportSymbol *symbol,
                                               TZrUInt32 ownerIndex) {
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_type_spec_visit_type(context, &symbol->valueType, ownerIndex)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < symbol->parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[index] : ZR_NULL;

        if (!metadata_type_spec_visit_type(context, parameterType, ownerIndex)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool metadata_type_spec_scan(SZrMetadataTypeSpecScanContext *context,
                                       const SZrFunction *function) {
    if (context == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->typedExportedSymbolLength > 0 && function->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        if (!metadata_type_spec_visit_export(context, &function->typedExportedSymbols[index], index)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_metadata_type_spec_plan(SZrCompilerState *cs,
                                         const SZrFunction *function,
                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                         TZrUInt32 stringHeapEntryCount,
                                         SZrMetadataTypeSpecPlan *outPlan) {
    SZrMetadataTypeSpecScanContext context;
    SZrMetadataTypeSpecUniqueEntry *uniqueEntries = ZR_NULL;
    TZrByte *scratchHeap = ZR_NULL;
    TZrSize scratchHeapOffset = 0;
    TZrUInt32 maxTypeSpecCount;
    TZrSize maxSignatureHeapLength;
    SZrGlobalState *global;

    if (outPlan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outPlan, 0, sizeof(*outPlan));
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    context.stringHeapEntries = stringHeapEntries;
    context.stringHeapEntryCount = stringHeapEntryCount;
    if (!metadata_type_spec_scan(&context, function)) {
        return ZR_FALSE;
    }
    if (context.count == 0 || context.signatureHeapLength == 0) {
        return ZR_TRUE;
    }
    maxTypeSpecCount = context.count;
    maxSignatureHeapLength = context.signatureHeapLength;

    global = cs->state->global;
    uniqueEntries = (SZrMetadataTypeSpecUniqueEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrMetadataTypeSpecUniqueEntry) * maxTypeSpecCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    scratchHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            global,
            maxSignatureHeapLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (uniqueEntries == ZR_NULL || scratchHeap == ZR_NULL) {
        if (uniqueEntries != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          uniqueEntries,
                                          sizeof(SZrMetadataTypeSpecUniqueEntry) * maxTypeSpecCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (scratchHeap != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          scratchHeap,
                                          maxSignatureHeapLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(uniqueEntries, 0, sizeof(SZrMetadataTypeSpecUniqueEntry) * maxTypeSpecCount);
    ZrCore_Memory_RawSet(scratchHeap, 0, maxSignatureHeapLength);
    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    context.emitHeap = scratchHeap;
    context.emitHeapLength = maxSignatureHeapLength;
    context.emitHeapOffset = &scratchHeapOffset;
    context.uniqueEntries = uniqueEntries;
    context.uniqueEntryCapacity = maxTypeSpecCount;
    context.stringHeapEntries = stringHeapEntries;
    context.stringHeapEntryCount = stringHeapEntryCount;
    if (!metadata_type_spec_scan(&context, function)) {
        ZrCore_Memory_RawFreeWithType(global,
                                      uniqueEntries,
                                      sizeof(SZrMetadataTypeSpecUniqueEntry) * maxTypeSpecCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(global,
                                      scratchHeap,
                                      maxSignatureHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    outPlan->typeSpecCount = context.emittedCount;
    outPlan->signatureHeapLength = scratchHeapOffset;
    ZrCore_Memory_RawFreeWithType(global,
                                  uniqueEntries,
                                  sizeof(SZrMetadataTypeSpecUniqueEntry) * maxTypeSpecCount,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    ZrCore_Memory_RawFreeWithType(global,
                                  scratchHeap,
                                  maxSignatureHeapLength,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_spec_emit(SZrCompilerState *cs,
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
    SZrMetadataTypeSpecScanContext context;

    if (cs == ZR_NULL ||
        function == ZR_NULL ||
        records == ZR_NULL ||
        ioRecordIndex == ZR_NULL ||
        heap == ZR_NULL ||
        ioHeapOffset == ZR_NULL ||
        ioSignatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.compiler = cs;
    context.emitHeap = heap;
    context.emitHeapLength = heapLength;
    context.emitHeapOffset = ioHeapOffset;
    context.records = records;
    context.recordCount = recordCount;
    context.recordIndex = ioRecordIndex;
    context.signatureRidCursor = ioSignatureRidCursor;
    context.typeSpecRecordStartIndex = *ioRecordIndex;
    context.stringHeapEntries = stringHeapEntries;
    context.stringHeapEntryCount = stringHeapEntryCount;

    return metadata_type_spec_scan(&context, function);
}
