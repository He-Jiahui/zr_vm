#include "module/module_import_signature.h"
#include "module/module_import_signature_binding.h"

#include <string.h>

#include "zr_vm_core/function.h"

#define ZR_MODULE_RUNTIME_ENTRY_FUNCTION_FIELD "__zr_reflection_entry_function"

static const SZrFunctionTypedExportSymbol *module_import_signature_find_typed_export_symbol(
        const SZrFunction *function,
        SZrString *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        if (symbol->name == name || (symbol->name != ZR_NULL && ZrCore_String_Equal(symbol->name, name))) {
            return symbol;
        }
    }

    return ZR_NULL;
}

static TZrBool module_import_signature_symbol_name_matches(const SZrFunctionTypedExportSymbol *symbol,
                                                           SZrString *name) {
    if (symbol == ZR_NULL || name == ZR_NULL || symbol->name == ZR_NULL) {
        return ZR_FALSE;
    }

    return symbol->name == name || ZrCore_String_Equal(symbol->name, name) ? ZR_TRUE : ZR_FALSE;
}

static SZrFunction *module_import_signature_get_module_entry_function(SZrState *state, SZrObjectModule *module) {
    SZrString *fieldName;
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_NULL;
    }

    fieldName = ZrCore_String_CreateFromNative(state, ZR_MODULE_RUNTIME_ENTRY_FUNCTION_FIELD);
    if (fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, fieldName);
    entryValue = ZrCore_Object_GetValue(state, &module->super, &key);
    return ZrCore_Closure_GetMetadataFunctionFromValue(state, entryValue);
}

static TZrBool module_import_signature_effect_kind_requires_guard(const SZrFunctionModuleEffect *effect) {
    if (effect == ZR_NULL) {
        return ZR_FALSE;
    }

    return (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_REF ||
            effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_READ ||
            effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool module_import_signature_module_name_matches(SZrString *candidate,
                                                           SZrString *path,
                                                           SZrObjectModule *module) {
    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }

    if (path != ZR_NULL && (candidate == path || ZrCore_String_Equal(candidate, path))) {
        return ZR_TRUE;
    }

    if (module != ZR_NULL && module->moduleName != ZR_NULL &&
        (candidate == module->moduleName || ZrCore_String_Equal(candidate, module->moduleName))) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool module_import_signature_read_u32(const TZrByte *blob,
                                                TZrUInt32 blobLength,
                                                TZrUInt32 *ioOffset,
                                                TZrUInt32 *outValue) {
    TZrUInt32 offset;

    if (blob == ZR_NULL || ioOffset == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    offset = *ioOffset;
    if (offset > blobLength || blobLength - offset < sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    *outValue = ((TZrUInt32)blob[offset]) |
                ((TZrUInt32)blob[offset + 1u] << 8u) |
                ((TZrUInt32)blob[offset + 2u] << 16u) |
                ((TZrUInt32)blob[offset + 3u] << 24u);
    *ioOffset = offset + (TZrUInt32)sizeof(TZrUInt32);
    return ZR_TRUE;
}

static TZrBool module_import_signature_string_matches(const TZrByte *blob,
                                                      TZrUInt32 blobLength,
                                                      TZrUInt32 *ioOffset,
                                                      SZrString *expected) {
    TZrNativeString expectedText;
    TZrSize expectedLength;
    TZrUInt32 encodedLength;
    TZrUInt32 offset;

    if (blob == ZR_NULL || ioOffset == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedText = ZrCore_String_GetNativeString(expected);
    expectedLength = expectedText != ZR_NULL ? strlen(expectedText) : 0u;
    if (expectedLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    if (!module_import_signature_read_u32(blob, blobLength, ioOffset, &encodedLength) ||
        encodedLength != (TZrUInt32)expectedLength) {
        return ZR_FALSE;
    }

    offset = *ioOffset;
    if (offset > blobLength || blobLength - offset < encodedLength) {
        return ZR_FALSE;
    }

    if (encodedLength > 0u &&
        ZrCore_Memory_RawCompare((TZrPtr)(blob + offset), (TZrPtr)expectedText, encodedLength) != 0) {
        return ZR_FALSE;
    }

    *ioOffset = offset + encodedLength;
    return ZR_TRUE;
}

static const SZrString *module_import_signature_find_string_heap_entry(const SZrFunction *function,
                                                                       TZrUInt32 stringIndex) {
    if (function == ZR_NULL || stringIndex == 0u ||
        function->metadataStringHeap == ZR_NULL || function->metadataStringHeapLength == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == stringIndex) {
            return function->metadataStringHeap[index].value;
        }
    }

    return ZR_NULL;
}

static TZrBool module_import_signature_heap_string_matches(const SZrFunction *function,
                                                           const TZrByte *blob,
                                                           TZrUInt32 blobLength,
                                                           TZrUInt32 *ioOffset,
                                                           SZrString *expected) {
    TZrUInt32 stringIndex;
    const SZrString *candidate;

    if (function == ZR_NULL || expected == ZR_NULL ||
        blob == ZR_NULL || ioOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->metadataStringHeap == ZR_NULL || function->metadataStringHeapLength == 0u) {
        return module_import_signature_string_matches(blob, blobLength, ioOffset, expected);
    }
    if (!module_import_signature_read_u32(blob, blobLength, ioOffset, &stringIndex)) {
        return ZR_FALSE;
    }

    candidate = module_import_signature_find_string_heap_entry(function, stringIndex);
    return candidate != ZR_NULL &&
           (candidate == expected || ZrCore_String_Equal((SZrString *)candidate, expected))
           ? ZR_TRUE
           : ZR_FALSE;
}

static SZrString *module_import_signature_read_string(SZrState *state,
                                                      const TZrByte *blob,
                                                      TZrUInt32 blobLength,
                                                      TZrUInt32 *ioOffset) {
    TZrUInt32 encodedLength;
    TZrUInt32 offset;

    if (state == ZR_NULL || blob == ZR_NULL || ioOffset == ZR_NULL) {
        return ZR_NULL;
    }
    if (!module_import_signature_read_u32(blob, blobLength, ioOffset, &encodedLength)) {
        return ZR_NULL;
    }

    offset = *ioOffset;
    if (offset > blobLength || blobLength - offset < encodedLength) {
        return ZR_NULL;
    }

    *ioOffset = offset + encodedLength;
    return ZrCore_String_Create(state, (TZrNativeString)(blob + offset), encodedLength);
}

static SZrString *module_import_signature_read_heap_string(SZrState *state,
                                                           const SZrFunction *function,
                                                           const TZrByte *blob,
                                                           TZrUInt32 blobLength,
                                                           TZrUInt32 *ioOffset) {
    TZrUInt32 stringIndex;
    const SZrString *candidate;

    ZR_UNUSED_PARAMETER(state);

    if (function == ZR_NULL || blob == ZR_NULL || ioOffset == ZR_NULL) {
        return ZR_NULL;
    }
    if (function->metadataStringHeap == ZR_NULL || function->metadataStringHeapLength == 0u) {
        return module_import_signature_read_string(state, blob, blobLength, ioOffset);
    }
    if (!module_import_signature_read_u32(blob, blobLength, ioOffset, &stringIndex)) {
        return ZR_NULL;
    }

    candidate = module_import_signature_find_string_heap_entry(function, stringIndex);
    return (SZrString *)candidate;
}

static TZrBool module_import_signature_find_blob_in_records(const SZrMetadataTokenRecord *records,
                                                            TZrUInt32 recordCount,
                                                            const SZrFunction *callerFunction,
                                                            const SZrFunctionModuleEffect *effect,
                                                            const TZrByte **outBlob,
                                                            TZrUInt32 *outLength) {
    TZrUInt32 index;

    if (records == ZR_NULL || recordCount == 0u || callerFunction == ZR_NULL || effect == ZR_NULL ||
        callerFunction->signatureBlobHeap == ZR_NULL || effect->moduleName == ZR_NULL ||
        effect->symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < recordCount; index++) {
        const SZrMetadataTokenRecord *record = &records[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0u;
        TZrUInt32 effectKind = 0u;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0u ||
            record->signatureBlobOffset >= callerFunction->signatureBlobHeapLength ||
            record->signatureBlobLength > callerFunction->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }
        if (effect->targetMetadataToken != 0u && record->targetMetadataToken != effect->targetMetadataToken) {
            continue;
        }
        if (effect->targetSignatureToken != 0u && record->targetSignatureToken != effect->targetSignatureToken) {
            continue;
        }
        if (effect->targetSignatureHash != 0u && record->targetSignatureHash != effect->targetSignatureHash) {
            continue;
        }

        blob = callerFunction->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !module_import_signature_heap_string_matches(callerFunction,
                                                         blob,
                                                         record->signatureBlobLength,
                                                         &offset,
                                                         effect->moduleName) ||
            !module_import_signature_heap_string_matches(callerFunction,
                                                         blob,
                                                         record->signatureBlobLength,
                                                         &offset,
                                                         effect->symbolName) ||
            !module_import_signature_read_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            effectKind != (TZrUInt32)effect->kind ||
            offset >= record->signatureBlobLength) {
            continue;
        }

        *outBlob = blob + offset;
        *outLength = record->signatureBlobLength - offset;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static const SZrMetadataTokenRecord *module_import_signature_find_member_ref_record_in_records(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        const SZrFunction *callerFunction,
        const SZrFunctionModuleEffect *effect) {
    TZrUInt32 index;

    if (records == ZR_NULL || recordCount == 0u || callerFunction == ZR_NULL || effect == ZR_NULL ||
        callerFunction->signatureBlobHeap == ZR_NULL || effect->moduleName == ZR_NULL ||
        effect->symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < recordCount; index++) {
        const SZrMetadataTokenRecord *record = &records[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0u;
        TZrUInt32 effectKind = 0u;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0u ||
            record->signatureBlobOffset >= callerFunction->signatureBlobHeapLength ||
            record->signatureBlobLength > callerFunction->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = callerFunction->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !module_import_signature_heap_string_matches(callerFunction,
                                                         blob,
                                                         record->signatureBlobLength,
                                                         &offset,
                                                         effect->moduleName) ||
            !module_import_signature_heap_string_matches(callerFunction,
                                                         blob,
                                                         record->signatureBlobLength,
                                                         &offset,
                                                         effect->symbolName) ||
            !module_import_signature_read_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            effectKind != (TZrUInt32)effect->kind ||
            offset >= record->signatureBlobLength) {
            continue;
        }

        return record;
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *module_import_signature_find_effect_member_ref_record(
        const SZrFunction *callerFunction,
        const SZrFunctionModuleEffect *effect) {
    const SZrMetadataTokenRecord *record;

    if (callerFunction == ZR_NULL || effect == ZR_NULL) {
        return ZR_NULL;
    }

    record = module_import_signature_find_member_ref_record_in_records(callerFunction->moduleMetadataTokenRecords,
                                                                       callerFunction->moduleMetadataTokenRecordLength,
                                                                       callerFunction,
                                                                       effect);
    if (record != ZR_NULL) {
        return record;
    }

    return module_import_signature_find_member_ref_record_in_records(callerFunction->metadataTokenRecords,
                                                                     callerFunction->metadataTokenRecordLength,
                                                                     callerFunction,
                                                                     effect);
}

static const SZrMetadataTokenRecord *module_import_signature_find_record_by_token(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        TZrMetadataToken token);

static const SZrMetadataTokenRecord *module_import_signature_find_assembly_ref_owner(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        const SZrMetadataTokenRecord *memberRefRecord) {
    const SZrMetadataTokenRecord *typeRefRecord;

    if (memberRefRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(memberRefRecord->token) != ZR_METADATA_TABLE_MEMBER_REF ||
        memberRefRecord->ownerToken == 0u) {
        return ZR_NULL;
    }

    typeRefRecord = module_import_signature_find_record_by_token(records,
                                                                 recordCount,
                                                                 memberRefRecord->ownerToken);
    if (typeRefRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeRefRecord->token) != ZR_METADATA_TABLE_TYPE_REF ||
        typeRefRecord->ownerToken == 0u) {
        return ZR_NULL;
    }

    return module_import_signature_find_record_by_token(records, recordCount, typeRefRecord->ownerToken);
}

static const SZrMetadataTokenRecord *module_import_signature_find_record_by_token(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        TZrMetadataToken token) {
    TZrUInt32 index;

    if (records == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    for (index = 0; index < recordCount; ++index) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

static TZrBool module_import_signature_parse_semver(const TZrChar *text,
                                                    TZrUInt32 *outMajor,
                                                    TZrUInt32 *outMinor,
                                                    TZrUInt32 *outPatch) {
    TZrUInt32 parts[3] = {0u, 0u, 0u};
    TZrUInt32 partIndex = 0u;
    TZrSize offset = 0u;

    if (outMajor != ZR_NULL) {
        *outMajor = 0u;
    }
    if (outMinor != ZR_NULL) {
        *outMinor = 0u;
    }
    if (outPatch != ZR_NULL) {
        *outPatch = 0u;
    }
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    while (partIndex < 3u) {
        TZrUInt32 value = 0u;
        TZrBool hasDigit = ZR_FALSE;

        while (text[offset] >= '0' && text[offset] <= '9') {
            TZrUInt32 digit = (TZrUInt32)(text[offset] - '0');
            if (value > (((TZrUInt32)0xFFFFFFFFu) - digit) / 10u) {
                return ZR_FALSE;
            }
            value = value * 10u + digit;
            hasDigit = ZR_TRUE;
            offset++;
        }
        if (!hasDigit) {
            return ZR_FALSE;
        }
        parts[partIndex++] = value;
        if (partIndex == 3u) {
            break;
        }
        if (text[offset] != '.') {
            return ZR_FALSE;
        }
        offset++;
    }

    if (text[offset] != '\0') {
        return ZR_FALSE;
    }

    if (outMajor != ZR_NULL) {
        *outMajor = parts[0];
    }
    if (outMinor != ZR_NULL) {
        *outMinor = parts[1];
    }
    if (outPatch != ZR_NULL) {
        *outPatch = parts[2];
    }
    return ZR_TRUE;
}

static int module_import_signature_compare_semver(SZrString *left, SZrString *right) {
    TZrUInt32 leftMajor;
    TZrUInt32 leftMinor;
    TZrUInt32 leftPatch;
    TZrUInt32 rightMajor;
    TZrUInt32 rightMinor;
    TZrUInt32 rightPatch;

    if (!module_import_signature_parse_semver(left != ZR_NULL ? ZrCore_String_GetNativeString(left) : ZR_NULL,
                                              &leftMajor,
                                              &leftMinor,
                                              &leftPatch) ||
        !module_import_signature_parse_semver(right != ZR_NULL ? ZrCore_String_GetNativeString(right) : ZR_NULL,
                                              &rightMajor,
                                              &rightMinor,
                                              &rightPatch)) {
        return 0;
    }

    if (leftMajor != rightMajor) {
        return leftMajor < rightMajor ? -1 : 1;
    }
    if (leftMinor != rightMinor) {
        return leftMinor < rightMinor ? -1 : 1;
    }
    if (leftPatch != rightPatch) {
        return leftPatch < rightPatch ? -1 : 1;
    }
    return 0;
}

static TZrBool module_import_signature_string_is_semver(SZrString *value) {
    TZrUInt32 major;
    TZrUInt32 minor;
    TZrUInt32 patch;

    return module_import_signature_parse_semver(value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL,
                                                &major,
                                                &minor,
                                                &patch);
}

static TZrBool module_import_signature_version_range_matches(const SZrFunctionModuleEffect *effect,
                                                             const SZrFunction *entryFunction) {
    SZrString *actualVersion;

    if (effect == ZR_NULL || entryFunction == ZR_NULL ||
        effect->minModuleVersionInclusive == ZR_NULL ||
        effect->maxModuleVersionExclusive == ZR_NULL) {
        return ZR_TRUE;
    }

    actualVersion = entryFunction->moduleVersion;
    if (!module_import_signature_string_is_semver(actualVersion) ||
        !module_import_signature_string_is_semver(effect->minModuleVersionInclusive) ||
        !module_import_signature_string_is_semver(effect->maxModuleVersionExclusive)) {
        return ZR_TRUE;
    }

    return module_import_signature_compare_semver(actualVersion, effect->minModuleVersionInclusive) >= 0 &&
                   module_import_signature_compare_semver(actualVersion, effect->maxModuleVersionExclusive) < 0
           ? ZR_TRUE
           : ZR_FALSE;
}

static void module_import_signature_record_version_mismatch(SZrModuleImportSignatureMismatch *outMismatch,
                                                            const SZrFunctionModuleEffect *effect,
                                                            const SZrFunction *entryFunction) {
    if (outMismatch == ZR_NULL || outMismatch->effect != ZR_NULL) {
        return;
    }

    if (effect != ZR_NULL) {
        outMismatch->effectSnapshot = *effect;
        outMismatch->effect = &outMismatch->effectSnapshot;
        outMismatch->expectedMinVersionInclusive = effect->minModuleVersionInclusive;
        outMismatch->expectedMaxVersionExclusive = effect->maxModuleVersionExclusive;
    } else {
        outMismatch->effect = ZR_NULL;
    }
    outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_VERSION;
    outMismatch->actualModuleVersion = entryFunction != ZR_NULL ? entryFunction->moduleVersion : ZR_NULL;
}

static const SZrMetadataTokenRecord *module_import_signature_find_export_signature_record(
        const SZrFunction *entryFunction,
        const SZrFunctionTypedExportSymbol *symbol) {
    const SZrMetadataTokenRecord *record;

    if (entryFunction == ZR_NULL || symbol == ZR_NULL || entryFunction->metadataTokenRecords == ZR_NULL ||
        entryFunction->metadataTokenRecordLength == 0u) {
        return ZR_NULL;
    }

    if (symbol->signatureToken != 0u) {
        record = module_import_signature_find_record_by_token(entryFunction->metadataTokenRecords,
                                                              entryFunction->metadataTokenRecordLength,
                                                              symbol->signatureToken);
        if (record != ZR_NULL &&
            ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
            (symbol->signatureHash == 0u || record->signatureHash == symbol->signatureHash)) {
            return record;
        }
    }

    for (TZrUInt32 index = 0; index < entryFunction->metadataTokenRecordLength; ++index) {
        record = &entryFunction->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
            record->relatedToken == symbol->metadataToken &&
            symbol->signatureHash != 0u &&
            record->signatureHash == symbol->signatureHash) {
            return record;
        }
    }

    return ZR_NULL;
}

static TZrBool module_import_signature_member_ref_owner_chain_is_valid(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        const SZrMetadataTokenRecord *memberRefRecord) {
    const SZrMetadataTokenRecord *typeRefRecord;
    const SZrMetadataTokenRecord *assemblyRefRecord;

    if (memberRefRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(memberRefRecord->token) != ZR_METADATA_TABLE_MEMBER_REF ||
        memberRefRecord->ownerToken == 0u) {
        return ZR_FALSE;
    }

    typeRefRecord = module_import_signature_find_record_by_token(records,
                                                                 recordCount,
                                                                 memberRefRecord->ownerToken);
    if (typeRefRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeRefRecord->token) != ZR_METADATA_TABLE_TYPE_REF ||
        typeRefRecord->ownerToken == 0u) {
        return ZR_FALSE;
    }

    assemblyRefRecord = module_import_signature_find_record_by_token(records,
                                                                     recordCount,
                                                                     typeRefRecord->ownerToken);
    return assemblyRefRecord != ZR_NULL &&
                   ZR_METADATA_TOKEN_TABLE(assemblyRefRecord->token) == ZR_METADATA_TABLE_ASSEMBLY_REF
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrBool module_import_signature_decode_member_ref_effect(SZrState *state,
                                                                const SZrFunction *callerFunction,
                                                                const SZrMetadataTokenRecord *record,
                                                                SZrFunctionModuleEffect *outEffect) {
    const TZrByte *blob;
    TZrUInt32 offset;
    TZrUInt32 effectKind;
    SZrString *moduleName;
    SZrString *symbolName;

    if (state == ZR_NULL || callerFunction == ZR_NULL || record == ZR_NULL || outEffect == ZR_NULL ||
        callerFunction->signatureBlobHeap == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= callerFunction->signatureBlobHeapLength ||
        record->signatureBlobLength > callerFunction->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = callerFunction->signatureBlobHeap + record->signatureBlobOffset;
    offset = 0u;
    if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF) {
        return ZR_FALSE;
    }

    moduleName = module_import_signature_read_heap_string(state,
                                                          callerFunction,
                                                          blob,
                                                          record->signatureBlobLength,
                                                          &offset);
    symbolName = module_import_signature_read_heap_string(state,
                                                          callerFunction,
                                                          blob,
                                                          record->signatureBlobLength,
                                                          &offset);
    if (moduleName == ZR_NULL || symbolName == ZR_NULL ||
        !module_import_signature_read_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
        offset >= record->signatureBlobLength) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outEffect, 0, sizeof(*outEffect));
    outEffect->kind = (TZrUInt8)effectKind;
    outEffect->moduleName = moduleName;
    outEffect->symbolName = symbolName;
    outEffect->targetMetadataToken = record->targetMetadataToken;
    outEffect->targetSignatureToken = record->targetSignatureToken;
    outEffect->targetSignatureHash = record->targetSignatureHash;
    outEffect->targetModuleSignatureHash = record->targetModuleSignatureHash;
    return ZR_TRUE;
}

static SZrFunctionModuleEffect module_import_signature_effect_with_ref_identity(
        const SZrFunctionModuleEffect *effect,
        const SZrMetadataTokenRecord *record) {
    SZrFunctionModuleEffect resolvedEffect;

    if (effect == ZR_NULL) {
        memset(&resolvedEffect, 0, sizeof(resolvedEffect));
        return resolvedEffect;
    }

    resolvedEffect = *effect;
    if (record == ZR_NULL) {
        return resolvedEffect;
    }

    if (resolvedEffect.targetMetadataToken == 0u) {
        resolvedEffect.targetMetadataToken = record->targetMetadataToken;
    }
    if (resolvedEffect.targetSignatureToken == 0u) {
        resolvedEffect.targetSignatureToken = record->targetSignatureToken;
    }
    if (resolvedEffect.targetSignatureHash == 0u) {
        resolvedEffect.targetSignatureHash = record->targetSignatureHash;
    }
    if (resolvedEffect.targetModuleSignatureHash == 0u) {
        resolvedEffect.targetModuleSignatureHash = record->targetModuleSignatureHash;
    }

    return resolvedEffect;
}

static const SZrMetadataTokenRecord *module_import_signature_find_assembly_ref_for_member(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *memberRefRecord) {
    const SZrMetadataTokenRecord *assemblyRefRecord;

    if (callerFunction == ZR_NULL || memberRefRecord == ZR_NULL) {
        return ZR_NULL;
    }

    assemblyRefRecord = module_import_signature_find_assembly_ref_owner(callerFunction->moduleMetadataTokenRecords,
                                                                        callerFunction->moduleMetadataTokenRecordLength,
                                                                        memberRefRecord);
    if (assemblyRefRecord != ZR_NULL &&
        ZR_METADATA_TOKEN_TABLE(assemblyRefRecord->token) == ZR_METADATA_TABLE_ASSEMBLY_REF) {
        return assemblyRefRecord;
    }

    assemblyRefRecord = module_import_signature_find_assembly_ref_owner(callerFunction->metadataTokenRecords,
                                                                        callerFunction->metadataTokenRecordLength,
                                                                        memberRefRecord);
    if (assemblyRefRecord != ZR_NULL &&
        ZR_METADATA_TOKEN_TABLE(assemblyRefRecord->token) == ZR_METADATA_TABLE_ASSEMBLY_REF) {
        return assemblyRefRecord;
    }

    return ZR_NULL;
}

static SZrFunctionModuleEffect module_import_signature_effect_with_assembly_version(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *memberRefRecord,
        const SZrFunctionModuleEffect *effect) {
    SZrFunctionModuleEffect resolvedEffect;
    const SZrMetadataTokenRecord *assemblyRefRecord;

    if (effect == ZR_NULL) {
        ZrCore_Memory_RawSet(&resolvedEffect, 0, sizeof(resolvedEffect));
        return resolvedEffect;
    }

    resolvedEffect = *effect;
    assemblyRefRecord = module_import_signature_find_assembly_ref_for_member(callerFunction, memberRefRecord);
    if (assemblyRefRecord == ZR_NULL) {
        return resolvedEffect;
    }

    if (resolvedEffect.requestedModuleVersion == ZR_NULL) {
        resolvedEffect.requestedModuleVersion = assemblyRefRecord->requestedModuleVersion;
    }
    if (resolvedEffect.minModuleVersionInclusive == ZR_NULL) {
        resolvedEffect.minModuleVersionInclusive = assemblyRefRecord->minModuleVersionInclusive;
    }
    if (resolvedEffect.maxModuleVersionExclusive == ZR_NULL) {
        resolvedEffect.maxModuleVersionExclusive = assemblyRefRecord->maxModuleVersionExclusive;
    }

    return resolvedEffect;
}

static TZrBool module_import_signature_find_effect_target_signature_blob(const SZrFunction *callerFunction,
                                                                         const SZrFunctionModuleEffect *effect,
                                                                         const TZrByte **outBlob,
                                                                         TZrUInt32 *outLength) {
    if (outBlob != ZR_NULL) {
        *outBlob = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0u;
    }

    if (callerFunction == ZR_NULL || effect == ZR_NULL || outBlob == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    if (module_import_signature_find_blob_in_records(callerFunction->moduleMetadataTokenRecords,
                                                     callerFunction->moduleMetadataTokenRecordLength,
                                                     callerFunction,
                                                     effect,
                                                     outBlob,
                                                     outLength)) {
        return ZR_TRUE;
    }

    return module_import_signature_find_blob_in_records(callerFunction->metadataTokenRecords,
                                                        callerFunction->metadataTokenRecordLength,
                                                        callerFunction,
                                                        effect,
                                                        outBlob,
                                                        outLength);
}

static TZrBool module_import_signature_get_export_symbol_blob(const SZrFunction *entryFunction,
                                                              const SZrFunctionTypedExportSymbol *symbol,
                                                              const TZrByte **outBlob,
                                                              TZrUInt32 *outLength) {
    const SZrMetadataTokenRecord *signatureRecord;

    if (outBlob != ZR_NULL) {
        *outBlob = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0u;
    }

    if (entryFunction == ZR_NULL || symbol == ZR_NULL || outBlob == ZR_NULL || outLength == ZR_NULL ||
        entryFunction->signatureBlobHeap == ZR_NULL) {
        return ZR_FALSE;
    }

    signatureRecord = module_import_signature_find_export_signature_record(entryFunction, symbol);
    if (signatureRecord != ZR_NULL &&
        signatureRecord->signatureBlobLength > 0u &&
        signatureRecord->signatureBlobOffset < entryFunction->signatureBlobHeapLength &&
        signatureRecord->signatureBlobLength <=
                entryFunction->signatureBlobHeapLength - signatureRecord->signatureBlobOffset) {
        *outBlob = entryFunction->signatureBlobHeap + signatureRecord->signatureBlobOffset;
        *outLength = signatureRecord->signatureBlobLength;
        return ZR_TRUE;
    }

    if (symbol->signatureBlobLength == 0u ||
        symbol->signatureBlobOffset >= entryFunction->signatureBlobHeapLength ||
        symbol->signatureBlobLength > entryFunction->signatureBlobHeapLength - symbol->signatureBlobOffset) {
        return ZR_FALSE;
    }

    *outBlob = entryFunction->signatureBlobHeap + symbol->signatureBlobOffset;
    *outLength = symbol->signatureBlobLength;
    return ZR_TRUE;
}

static void module_import_signature_record_mismatch(SZrModuleImportSignatureMismatch *outMismatch,
                                                    const SZrFunctionModuleEffect *effect,
                                                    TZrUInt64 expectedHash,
                                                    TZrUInt64 actualHash,
                                                    TZrBool hasActualHash) {
    if (outMismatch == ZR_NULL || outMismatch->effect != ZR_NULL) {
        return;
    }

    if (effect != ZR_NULL) {
        outMismatch->effectSnapshot = *effect;
        outMismatch->effect = &outMismatch->effectSnapshot;
    } else {
        outMismatch->effect = ZR_NULL;
    }
    outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_MEMBER_SIGNATURE;
    outMismatch->expectedHash = expectedHash;
    outMismatch->actualHash = actualHash;
    outMismatch->hasActualHash = hasActualHash;
}

static void module_import_signature_record_module_hash_mismatch(SZrModuleImportSignatureMismatch *outMismatch,
                                                                const SZrFunctionModuleEffect *effect,
                                                                TZrUInt64 expectedHash,
                                                                TZrUInt64 actualHash,
                                                                TZrBool hasActualHash) {
    if (outMismatch == ZR_NULL || outMismatch->effect != ZR_NULL) {
        return;
    }

    if (effect != ZR_NULL) {
        outMismatch->effectSnapshot = *effect;
        outMismatch->effect = &outMismatch->effectSnapshot;
    } else {
        outMismatch->effect = ZR_NULL;
    }
    outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE;
    outMismatch->expectedHash = expectedHash;
    outMismatch->actualHash = actualHash;
    outMismatch->hasActualHash = hasActualHash;
}

static void module_import_signature_record_token_mismatch(SZrModuleImportSignatureMismatch *outMismatch,
                                                          const SZrFunctionModuleEffect *effect,
                                                          const SZrFunctionTypedExportSymbol *symbol) {
    if (outMismatch == ZR_NULL || outMismatch->effect != ZR_NULL || effect == ZR_NULL || symbol == ZR_NULL) {
        return;
    }

    module_import_signature_record_mismatch(outMismatch,
                                            effect,
                                            effect->targetSignatureHash,
                                            symbol->signatureHash,
                                            symbol->signatureHash != 0u ? ZR_TRUE : ZR_FALSE);
    if (outMismatch->effect == ZR_NULL) {
        return;
    }

    if (effect->targetMetadataToken != 0u && symbol->metadataToken != 0u &&
        effect->targetMetadataToken != symbol->metadataToken) {
        outMismatch->expectedMetadataToken = effect->targetMetadataToken;
        outMismatch->actualMetadataToken = symbol->metadataToken;
        outMismatch->hasMetadataTokenMismatch = ZR_TRUE;
    }
    if (effect->targetSignatureToken != 0u && symbol->signatureToken != 0u &&
        effect->targetSignatureToken != symbol->signatureToken) {
        outMismatch->expectedSignatureToken = effect->targetSignatureToken;
        outMismatch->actualSignatureToken = symbol->signatureToken;
        outMismatch->hasSignatureTokenMismatch = ZR_TRUE;
    }
}

static TZrBool module_import_signature_token_matches(const SZrFunctionModuleEffect *effect,
                                                     const SZrFunctionTypedExportSymbol *symbol) {
    if (effect == ZR_NULL || symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    if (effect->targetMetadataToken != 0u && symbol->metadataToken != 0u &&
        effect->targetMetadataToken != symbol->metadataToken) {
        return ZR_FALSE;
    }
    if (effect->targetSignatureToken != 0u && symbol->signatureToken != 0u &&
        effect->targetSignatureToken != symbol->signatureToken) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool module_import_signature_has_complete_target_tokens(const SZrFunctionModuleEffect *effect,
                                                                  const SZrFunctionTypedExportSymbol *symbol) {
    if (effect == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    return effect->targetMetadataToken != 0u &&
                   effect->targetSignatureToken != 0u &&
                   symbol->metadataToken != 0u &&
                   symbol->signatureToken != 0u
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrBool module_import_signature_blob_matches(const SZrFunction *callerFunction,
                                                    const SZrFunctionModuleEffect *effect,
                                                    const SZrFunction *entryFunction,
                                                    const SZrFunctionTypedExportSymbol *symbol) {
    const TZrByte *expectedBlob;
    const TZrByte *actualBlob;
    TZrUInt32 expectedLength;
    TZrUInt32 actualLength;

    if (!module_import_signature_find_effect_target_signature_blob(callerFunction,
                                                                   effect,
                                                                   &expectedBlob,
                                                                   &expectedLength)) {
        return ZR_TRUE;
    }
    if (!module_import_signature_get_export_symbol_blob(entryFunction, symbol, &actualBlob, &actualLength)) {
        return ZR_FALSE;
    }

    return expectedLength == actualLength &&
           (expectedLength == 0u ||
            ZrCore_Memory_RawCompare((TZrPtr)expectedBlob, (TZrPtr)actualBlob, expectedLength) == 0)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool module_import_signature_token_matches_or_allows_legacy_fallback(
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol) {
    if (module_import_signature_token_matches(effect, symbol)) {
        return ZR_TRUE;
    }

    return !module_import_signature_has_complete_target_tokens(effect, symbol);
}

static const SZrFunctionTypedExportSymbol *module_import_signature_find_matching_export_symbol(
        const SZrFunction *callerFunction,
        const SZrFunctionModuleEffect *effect,
        const SZrFunction *entryFunction) {
    const SZrFunctionTypedExportSymbol *fallback;
    TZrUInt32 index;

    if (entryFunction == ZR_NULL || effect == ZR_NULL || effect->symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    fallback = module_import_signature_find_typed_export_symbol(entryFunction, effect->symbolName);
    if (fallback == ZR_NULL || entryFunction->typedExportedSymbols == ZR_NULL) {
        return fallback;
    }

    for (index = 0; index < entryFunction->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];

        if (!module_import_signature_symbol_name_matches(symbol, effect->symbolName)) {
            continue;
        }
        if (effect->targetSignatureHash != 0u && symbol->signatureHash != effect->targetSignatureHash) {
            continue;
        }
        if (!module_import_signature_blob_matches(callerFunction, effect, entryFunction, symbol)) {
            continue;
        }
        if (!module_import_signature_token_matches(effect, symbol)) {
            continue;
        }

        return symbol;
    }

    for (index = 0; index < entryFunction->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];

        if (!module_import_signature_symbol_name_matches(symbol, effect->symbolName)) {
            continue;
        }
        if (effect->targetSignatureHash != 0u && symbol->signatureHash != effect->targetSignatureHash) {
            continue;
        }
        if (!module_import_signature_blob_matches(callerFunction, effect, entryFunction, symbol)) {
            continue;
        }

        return symbol;
    }

    return fallback;
}

static TZrBool module_import_signature_module_hash_matches(const SZrFunctionModuleEffect *effect,
                                                           const SZrFunction *entryFunction) {
    if (effect == ZR_NULL || entryFunction == ZR_NULL || effect->targetModuleSignatureHash == 0u) {
        return ZR_TRUE;
    }

    return entryFunction->moduleSignatureHash == effect->targetModuleSignatureHash ? ZR_TRUE : ZR_FALSE;
}

static TZrBool module_import_signature_verify_resolved_effect(SZrState *state,
                                                              SZrFunction *callerFunction,
                                                              const SZrMetadataTokenRecord *memberRefRecord,
                                                              const SZrFunctionModuleEffect *effect,
                                                              SZrObjectModule *module,
                                                              SZrFunction **ioEntryFunction,
                                                              SZrModuleImportSignatureMismatch *outMismatch) {
    const SZrFunctionTypedExportSymbol *symbol;
    SZrFunctionModuleEffect versionResolvedEffect;
    const SZrFunctionModuleEffect *effectiveEffect;

    if (state == ZR_NULL || callerFunction == ZR_NULL || effect == ZR_NULL || module == ZR_NULL ||
        ioEntryFunction == ZR_NULL || effect->targetSignatureHash == 0u) {
        return ZR_TRUE;
    }

    if (*ioEntryFunction == ZR_NULL) {
        *ioEntryFunction = module_import_signature_get_module_entry_function(state, module);
        if (*ioEntryFunction == ZR_NULL) {
            module_import_signature_record_mismatch(outMismatch,
                                                    effect,
                                                    effect->targetSignatureHash,
                                                    0u,
                                                    ZR_FALSE);
            return ZR_FALSE;
        }
    }
    versionResolvedEffect = module_import_signature_effect_with_assembly_version(callerFunction,
                                                                                memberRefRecord,
                                                                                effect);
    effectiveEffect = &versionResolvedEffect;
    if (!module_import_signature_version_range_matches(effectiveEffect, *ioEntryFunction)) {
        module_import_signature_record_version_mismatch(outMismatch, effectiveEffect, *ioEntryFunction);
        return ZR_FALSE;
    }
    if (!module_import_signature_module_hash_matches(effectiveEffect, *ioEntryFunction)) {
        module_import_signature_record_module_hash_mismatch(
                outMismatch,
                effectiveEffect,
                effectiveEffect->targetModuleSignatureHash,
                (*ioEntryFunction)->moduleSignatureHash,
                (*ioEntryFunction)->moduleSignatureHash != 0u ? ZR_TRUE : ZR_FALSE);
        return ZR_FALSE;
    }

    symbol = module_import_signature_find_matching_export_symbol(callerFunction, effectiveEffect, *ioEntryFunction);
    if (symbol == ZR_NULL) {
        module_import_signature_record_mismatch(outMismatch,
                                                effectiveEffect,
                                                effectiveEffect->targetSignatureHash,
                                                0u,
                                                ZR_FALSE);
        return ZR_FALSE;
    }
    if (!module_import_signature_token_matches_or_allows_legacy_fallback(effectiveEffect, symbol)) {
        module_import_signature_record_token_mismatch(outMismatch, effectiveEffect, symbol);
        return ZR_FALSE;
    }
    if (symbol->signatureHash != effectiveEffect->targetSignatureHash) {
        module_import_signature_record_mismatch(outMismatch,
                                                effectiveEffect,
                                                effectiveEffect->targetSignatureHash,
                                                symbol->signatureHash,
                                                ZR_TRUE);
        return ZR_FALSE;
    }
    if (!module_import_signature_blob_matches(callerFunction, effectiveEffect, *ioEntryFunction, symbol)) {
        module_import_signature_record_mismatch(outMismatch,
                                                effectiveEffect,
                                                effectiveEffect->targetSignatureHash,
                                                symbol->signatureHash,
                                                ZR_TRUE);
        return ZR_FALSE;
    }

    zr_module_import_signature_record_binding(
            state,
            callerFunction,
            memberRefRecord,
            module_import_signature_find_assembly_ref_for_member(callerFunction, memberRefRecord),
            effectiveEffect,
            symbol,
            *ioEntryFunction);
    zr_module_import_signature_bind_type_metadata_with_diagnostic(state,
                                                                  callerFunction,
                                                                  effectiveEffect,
                                                                  *ioEntryFunction);
    return ZR_TRUE;
}

static TZrBool module_import_signature_verify_effects(SZrState *state,
                                                      SZrFunction *callerFunction,
                                                      const SZrFunctionModuleEffect *effects,
                                                      TZrUInt32 effectLength,
                                                      SZrString *path,
                                                      SZrObjectModule *module,
                                                      SZrModuleImportSignatureMismatch *outMismatch) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || effects == ZR_NULL || effectLength == 0) {
        return ZR_TRUE;
    }

    entryFunction = ZR_NULL;
    for (index = 0; index < effectLength; index++) {
        const SZrFunctionModuleEffect *effect = &effects[index];
        const SZrMetadataTokenRecord *memberRefRecord;
        SZrFunctionModuleEffect resolvedEffect;

        if (effect->symbolName == ZR_NULL ||
            !module_import_signature_effect_kind_requires_guard(effect) ||
            !module_import_signature_module_name_matches(effect->moduleName, path, module)) {
            continue;
        }
        memberRefRecord = module_import_signature_find_effect_member_ref_record(callerFunction, effect);
        resolvedEffect = module_import_signature_effect_with_ref_identity(effect, memberRefRecord);
        if (!module_import_signature_verify_resolved_effect(state,
                                                            callerFunction,
                                                            memberRefRecord,
                                                            &resolvedEffect,
                                                            module,
                                                            &entryFunction,
                                                            outMismatch)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool module_import_signature_verify_module_ref_records(SZrState *state,
                                                                 SZrFunction *callerFunction,
                                                                 SZrString *path,
                                                                 SZrObjectModule *module,
                                                                 SZrModuleImportSignatureMismatch *outMismatch) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (state == ZR_NULL || callerFunction == ZR_NULL || module == ZR_NULL ||
        callerFunction->moduleMetadataTokenRecords == ZR_NULL ||
        callerFunction->moduleMetadataTokenRecordLength == 0u) {
        return ZR_TRUE;
    }

    entryFunction = ZR_NULL;
    for (index = 0; index < callerFunction->moduleMetadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &callerFunction->moduleMetadataTokenRecords[index];
        SZrFunctionModuleEffect recordEffect;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF) {
            continue;
        }
        if (!module_import_signature_decode_member_ref_effect(state, callerFunction, record, &recordEffect) ||
            !module_import_signature_effect_kind_requires_guard(&recordEffect) ||
            !module_import_signature_module_name_matches(recordEffect.moduleName, path, module)) {
            continue;
        }
        if (!module_import_signature_member_ref_owner_chain_is_valid(callerFunction->moduleMetadataTokenRecords,
                                                                     callerFunction->moduleMetadataTokenRecordLength,
                                                                     record)) {
            module_import_signature_record_mismatch(outMismatch,
                                                    &recordEffect,
                                                    recordEffect.targetSignatureHash,
                                                    0u,
                                                    ZR_FALSE);
            return ZR_FALSE;
        }
        if (!module_import_signature_verify_resolved_effect(state,
                                                            callerFunction,
                                                            record,
                                                            &recordEffect,
                                                            module,
                                                            &entryFunction,
                                                            outMismatch)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

ZR_CORE_API TZrBool zr_module_import_signature_verify(SZrState *state,
                                                      SZrFunction *callerFunction,
                                                      SZrString *path,
                                                      SZrObjectModule *module,
                                                      SZrModuleImportSignatureMismatch *outMismatch) {
    if (state == ZR_NULL || callerFunction == ZR_NULL || module == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!module_import_signature_verify_effects(state,
                                                callerFunction,
                                                callerFunction->moduleEntryEffects,
                                                callerFunction->moduleEntryEffectLength,
                                                path,
                                                module,
                                                outMismatch)) {
        return ZR_FALSE;
    }

    return module_import_signature_verify_module_ref_records(state, callerFunction, path, module, outMismatch);
}
