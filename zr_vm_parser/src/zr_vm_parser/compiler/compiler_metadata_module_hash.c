#include "compiler_metadata_module_hash.h"
#include "compiler_metadata_signature.h"

#include "zr_vm_core/hash.h"

#include <string.h>

static const TZrByte CZrMetadataModuleSignatureHashV2Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        'm',
        'o',
        'd',
        '.',
        'v',
        '2',
        '\0',
};

static void metadata_module_hash_write_u64(TZrByte *buffer, TZrSize *offset, TZrUInt64 value) {
    buffer[*offset + 0] = (TZrByte)(value & 0xFFu);
    buffer[*offset + 1] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[*offset + 2] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[*offset + 3] = (TZrByte)((value >> 24) & 0xFFu);
    buffer[*offset + 4] = (TZrByte)((value >> 32) & 0xFFu);
    buffer[*offset + 5] = (TZrByte)((value >> 40) & 0xFFu);
    buffer[*offset + 6] = (TZrByte)((value >> 48) & 0xFFu);
    buffer[*offset + 7] = (TZrByte)((value >> 56) & 0xFFu);
    *offset += 8;
}

static TZrBool metadata_module_hash_record_participates(const SZrMetadataTokenRecord *record) {
    TZrUInt32 table;

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    table = ZR_METADATA_TOKEN_TABLE(record->token);
    return table == ZR_METADATA_TABLE_TYPE_DEF ||
           table == ZR_METADATA_TABLE_TYPE_SPEC;
}

static int metadata_module_hash_compare_u64(TZrUInt64 left, TZrUInt64 right) {
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

static int metadata_module_hash_compare_export_symbol_names(const SZrFunction *function,
                                                            TZrUInt32 leftIndex,
                                                            TZrUInt32 rightIndex) {
    const SZrFunctionTypedExportSymbol *left;
    const SZrFunctionTypedExportSymbol *right;
    TZrNativeString leftName;
    TZrNativeString rightName;

    if (function == ZR_NULL || function->typedExportedSymbols == ZR_NULL ||
        leftIndex >= function->typedExportedSymbolLength || rightIndex >= function->typedExportedSymbolLength) {
        return 0;
    }

    left = &function->typedExportedSymbols[leftIndex];
    right = &function->typedExportedSymbols[rightIndex];
    leftName = left->name != ZR_NULL ? ZrCore_String_GetNativeString(left->name) : "";
    rightName = right->name != ZR_NULL ? ZrCore_String_GetNativeString(right->name) : "";
    if (leftName == ZR_NULL) {
        leftName = "";
    }
    if (rightName == ZR_NULL) {
        rightName = "";
    }

    return strcmp(leftName, rightName);
}

static void metadata_module_hash_sort_export_indices(const SZrFunction *function, TZrUInt32 *indices) {
    if (function == ZR_NULL || indices == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        indices[index] = index;
    }

    for (TZrUInt32 index = 1; index < function->typedExportedSymbolLength; index++) {
        TZrUInt32 current = indices[index];
        TZrUInt32 insert = index;

        while (insert > 0 &&
               metadata_module_hash_compare_export_symbol_names(function, indices[insert - 1u], current) > 0) {
            indices[insert] = indices[insert - 1u];
            insert--;
        }
        indices[insert] = current;
    }
}

static TZrUInt32 metadata_module_hash_count_identity_records(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        if (metadata_module_hash_record_participates(&function->metadataTokenRecords[index])) {
            count++;
        }
    }
    return count;
}

static int metadata_module_hash_compare_identity_records(const SZrFunction *function,
                                                         TZrUInt32 leftIndex,
                                                         TZrUInt32 rightIndex) {
    const SZrMetadataTokenRecord *left;
    const SZrMetadataTokenRecord *right;
    const TZrByte *leftBlob;
    const TZrByte *rightBlob;
    TZrUInt32 leftTable;
    TZrUInt32 rightTable;
    TZrUInt32 minLength;
    int comparison;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        leftIndex >= function->metadataTokenRecordLength ||
        rightIndex >= function->metadataTokenRecordLength) {
        return 0;
    }

    left = &function->metadataTokenRecords[leftIndex];
    right = &function->metadataTokenRecords[rightIndex];
    leftTable = ZR_METADATA_TOKEN_TABLE(left->token);
    rightTable = ZR_METADATA_TOKEN_TABLE(right->token);
    if (leftTable != rightTable) {
        return leftTable < rightTable ? -1 : 1;
    }

    comparison = metadata_module_hash_compare_u64(left->signatureHash, right->signatureHash);
    if (comparison != 0) {
        return comparison;
    }
    if (left->signatureBlobLength != right->signatureBlobLength) {
        return left->signatureBlobLength < right->signatureBlobLength ? -1 : 1;
    }
    if (left->signatureBlobOffset <= function->signatureBlobHeapLength &&
        left->signatureBlobLength <= function->signatureBlobHeapLength - left->signatureBlobOffset &&
        right->signatureBlobOffset <= function->signatureBlobHeapLength &&
        right->signatureBlobLength <= function->signatureBlobHeapLength - right->signatureBlobOffset) {
        leftBlob = function->signatureBlobHeap + left->signatureBlobOffset;
        rightBlob = function->signatureBlobHeap + right->signatureBlobOffset;
        minLength = left->signatureBlobLength < right->signatureBlobLength
                            ? left->signatureBlobLength
                            : right->signatureBlobLength;
        if (minLength > 0u) {
            comparison = memcmp(leftBlob, rightBlob, minLength);
            if (comparison != 0) {
                return comparison;
            }
        }
    }
    if (left->layoutVersion != right->layoutVersion) {
        return left->layoutVersion < right->layoutVersion ? -1 : 1;
    }
    comparison = metadata_module_hash_compare_u64(left->layoutHash, right->layoutHash);
    if (comparison != 0) {
        return comparison;
    }
    return left->token < right->token ? -1 : (left->token > right->token ? 1 : 0);
}

static void metadata_module_hash_sort_identity_indices(const SZrFunction *function,
                                                       TZrUInt32 *indices,
                                                       TZrUInt32 identityRecordCount) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL || indices == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength && count < identityRecordCount; index++) {
        if (metadata_module_hash_record_participates(&function->metadataTokenRecords[index])) {
            indices[count++] = index;
        }
    }

    for (TZrUInt32 index = 1; index < count; index++) {
        TZrUInt32 current = indices[index];
        TZrUInt32 insert = index;

        while (insert > 0 &&
               metadata_module_hash_compare_identity_records(function, indices[insert - 1u], current) > 0) {
            indices[insert] = indices[insert - 1u];
            insert--;
        }
        indices[insert] = current;
    }
}

static TZrSize metadata_module_hash_buffer_size(const SZrFunction *function) {
    TZrSize size = sizeof(TZrUInt32) +
                   sizeof(TZrUInt32) +
                   sizeof(TZrUInt32);

    if (function == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        return size;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        size += sizeof(TZrUInt32);
        size += sizeof(TZrUInt8) * 4u;
        size += sizeof(TZrUInt64);
        size += sizeof(TZrUInt32);
        size += symbol->signatureBlobLength;
    }

    if (function->metadataTokenRecords != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
            const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

            if (!metadata_module_hash_record_participates(record)) {
                continue;
            }
            size += sizeof(TZrUInt32);
            size += sizeof(TZrUInt64);
            size += sizeof(TZrUInt32);
            size += sizeof(TZrUInt64);
            size += sizeof(TZrUInt32);
            size += record->signatureBlobLength;
        }
    }

    return size;
}

static TZrBool metadata_module_hash_write_export_symbols(const SZrFunction *function,
                                                         TZrByte *buffer,
                                                         TZrSize bufferSize,
                                                         TZrSize *offset,
                                                         const TZrUInt32 *sortedExportIndices,
                                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                         TZrUInt32 stringHeapEntryCount) {
    if (function == ZR_NULL || buffer == ZR_NULL || offset == ZR_NULL || sortedExportIndices == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_token_write_u32(buffer, offset, function->typedExportedSymbolLength);
    for (TZrUInt32 order = 0; order < function->typedExportedSymbolLength; order++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[sortedExportIndices[order]];
        if (*offset + sizeof(TZrUInt32) + sizeof(TZrUInt8) * 4u +
                      sizeof(TZrUInt64) + sizeof(TZrUInt32) + symbol->signatureBlobLength > bufferSize ||
            symbol->signatureBlobOffset > function->signatureBlobHeapLength ||
            symbol->signatureBlobLength > function->signatureBlobHeapLength - symbol->signatureBlobOffset) {
            return ZR_FALSE;
        }

        metadata_token_write_string_ref(buffer,
                                        offset,
                                        symbol->name,
                                        stringHeapEntries,
                                        stringHeapEntryCount);
        metadata_token_write_u8(buffer, offset, symbol->accessModifier);
        metadata_token_write_u8(buffer, offset, symbol->symbolKind);
        metadata_token_write_u8(buffer, offset, symbol->exportKind);
        metadata_token_write_u8(buffer, offset, symbol->readiness);
        metadata_module_hash_write_u64(buffer, offset, symbol->signatureHash);
        metadata_token_write_u32(buffer, offset, symbol->signatureBlobLength);
        if (symbol->signatureBlobLength > 0) {
            memcpy(buffer + *offset,
                   function->signatureBlobHeap + symbol->signatureBlobOffset,
                   symbol->signatureBlobLength);
            *offset += symbol->signatureBlobLength;
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_module_hash_write_identity_records(const SZrFunction *function,
                                                           TZrByte *buffer,
                                                           TZrSize bufferSize,
                                                           TZrSize *offset,
                                                           const TZrUInt32 *sortedIdentityIndices,
                                                           TZrUInt32 identityRecordCount) {
    if (function == ZR_NULL || buffer == ZR_NULL || offset == ZR_NULL ||
        (identityRecordCount > 0u && sortedIdentityIndices == ZR_NULL)) {
        return ZR_FALSE;
    }

    metadata_token_write_u32(buffer, offset, identityRecordCount);
    for (TZrUInt32 order = 0; order < identityRecordCount; order++) {
        const SZrMetadataTokenRecord *record;

        if (sortedIdentityIndices[order] >= function->metadataTokenRecordLength) {
            return ZR_FALSE;
        }
        record = &function->metadataTokenRecords[sortedIdentityIndices[order]];
        if (*offset + sizeof(TZrUInt32) + sizeof(TZrUInt64) + sizeof(TZrUInt32) +
                      sizeof(TZrUInt64) + sizeof(TZrUInt32) + record->signatureBlobLength > bufferSize ||
            record->signatureBlobOffset > function->signatureBlobHeapLength ||
            record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
            return ZR_FALSE;
        }

        metadata_token_write_u32(buffer, offset, ZR_METADATA_TOKEN_TABLE(record->token));
        metadata_module_hash_write_u64(buffer, offset, record->signatureHash);
        metadata_token_write_u32(buffer, offset, record->layoutVersion);
        metadata_module_hash_write_u64(buffer, offset, record->layoutHash);
        metadata_token_write_u32(buffer, offset, record->signatureBlobLength);
        if (record->signatureBlobLength > 0) {
            memcpy(buffer + *offset,
                   function->signatureBlobHeap + record->signatureBlobOffset,
                   record->signatureBlobLength);
            *offset += record->signatureBlobLength;
        }
    }

    return ZR_TRUE;
}

static TZrBool metadata_module_hash_write_buffer(const SZrFunction *function,
                                                 TZrByte *buffer,
                                                 TZrSize bufferSize,
                                                 const TZrUInt32 *sortedExportIndices,
                                                 const TZrUInt32 *sortedIdentityIndices,
                                                 TZrUInt32 identityRecordCount,
                                                 const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                 TZrUInt32 stringHeapEntryCount) {
    TZrSize offset = 0;

    if (function == ZR_NULL || buffer == ZR_NULL || sortedExportIndices == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_token_write_string_ref(buffer,
                                    &offset,
                                    function->moduleVersion,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    if (!metadata_module_hash_write_export_symbols(function,
                                                   buffer,
                                                   bufferSize,
                                                   &offset,
                                                   sortedExportIndices,
                                                   stringHeapEntries,
                                                   stringHeapEntryCount)) {
        return ZR_FALSE;
    }
    if (!metadata_module_hash_write_identity_records(function,
                                                     buffer,
                                                     bufferSize,
                                                     &offset,
                                                     sortedIdentityIndices,
                                                     identityRecordCount)) {
        return ZR_FALSE;
    }

    return offset == bufferSize ? ZR_TRUE : ZR_FALSE;
}

TZrUInt64 metadata_token_compute_module_signature_hash(
        SZrCompilerState *cs,
        const SZrFunction *function,
        const SZrMetadataStringHeapEntry *stringHeapEntries,
        TZrUInt32 stringHeapEntryCount) {
    SZrGlobalState *global;
    TZrUInt32 *sortedExportIndices;
    TZrUInt32 *sortedIdentityIndices = ZR_NULL;
    TZrByte *buffer;
    TZrSize exportIndexBytes;
    TZrSize identityIndexBytes = 0;
    TZrSize bufferSize;
    TZrUInt32 identityRecordCount;
    TZrUInt64 hash;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        function == ZR_NULL || function->typedExportedSymbolLength == 0u ||
        function->typedExportedSymbols == ZR_NULL || function->signatureBlobHeap == ZR_NULL) {
        return 0;
    }

    global = cs->state->global;
    exportIndexBytes = sizeof(TZrUInt32) * function->typedExportedSymbolLength;
    sortedExportIndices = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                       exportIndexBytes,
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (sortedExportIndices == ZR_NULL) {
        return 0;
    }
    metadata_module_hash_sort_export_indices(function, sortedExportIndices);

    identityRecordCount = metadata_module_hash_count_identity_records(function);
    if (identityRecordCount > 0u) {
        identityIndexBytes = sizeof(TZrUInt32) * identityRecordCount;
        sortedIdentityIndices = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                             identityIndexBytes,
                                                                             ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (sortedIdentityIndices == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          sortedExportIndices,
                                          exportIndexBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return 0;
        }
        metadata_module_hash_sort_identity_indices(function, sortedIdentityIndices, identityRecordCount);
    }

    bufferSize = metadata_module_hash_buffer_size(function);
    buffer = (TZrByte *)ZrCore_Memory_RawMallocWithType(global,
                                                        bufferSize,
                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (buffer == ZR_NULL) {
        if (sortedIdentityIndices != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          sortedIdentityIndices,
                                          identityIndexBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      sortedExportIndices,
                                      exportIndexBytes,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return 0;
    }

    if (!metadata_module_hash_write_buffer(function,
                                           buffer,
                                           bufferSize,
                                           sortedExportIndices,
                                           sortedIdentityIndices,
                                           identityRecordCount,
                                           stringHeapEntries,
                                           stringHeapEntryCount)) {
        ZrCore_Memory_RawFreeWithType(global, buffer, bufferSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (sortedIdentityIndices != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          sortedIdentityIndices,
                                          identityIndexBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      sortedExportIndices,
                                      exportIndexBytes,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return 0;
    }

    hash = ZrCore_Hash_CreateStable64WithPrefix(CZrMetadataModuleSignatureHashV2Prefix,
                                                sizeof(CZrMetadataModuleSignatureHashV2Prefix),
                                                buffer,
                                                bufferSize);
    ZrCore_Memory_RawFreeWithType(global, buffer, bufferSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (sortedIdentityIndices != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      sortedIdentityIndices,
                                      identityIndexBytes,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    ZrCore_Memory_RawFreeWithType(global,
                                  sortedExportIndices,
                                  exportIndexBytes,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return hash;
}
