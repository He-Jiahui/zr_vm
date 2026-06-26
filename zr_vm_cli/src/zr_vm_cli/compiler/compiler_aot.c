#include "compiler/compiler_aot.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/compiler.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/generic_instantiation.h"
#include "zr_vm_parser/writer.h"

static TZrBool zr_cli_read_binary_file(const TZrChar *path, TZrByte **outBytes, TZrSize *outLength) {
    FILE *file;
    long fileLength;
    TZrByte *bytes;
    size_t readLength;

    if (outBytes != ZR_NULL) {
        *outBytes = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL || outBytes == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_FALSE;
    }
    fileLength = ftell(file);
    if (fileLength <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    bytes = (TZrByte *)malloc((size_t)fileLength);
    if (bytes == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    readLength = fread(bytes, 1u, (size_t)fileLength, file);
    fclose(file);
    if (readLength != (size_t)fileLength) {
        free(bytes);
        return ZR_FALSE;
    }

    *outBytes = bytes;
    *outLength = (TZrSize)readLength;
    return ZR_TRUE;
}

void ZrCli_Compiler_AotPreserveRoots_Init(SZrCliAotPreserveRoots *roots) {
    if (roots == ZR_NULL) {
        return;
    }

    roots->indices = ZR_NULL;
    roots->count = 0u;
    roots->capacity = 0u;
    roots->genericRoots = ZR_NULL;
    roots->genericRootCount = 0u;
    roots->genericRootCapacity = 0u;
}

void ZrCli_Compiler_AotPreserveRoots_Free(SZrCliAotPreserveRoots *roots) {
    if (roots == ZR_NULL) {
        return;
    }

    if (roots->genericRoots != ZR_NULL) {
        for (TZrUInt32 index = 0u; index < roots->genericRootCount; index++) {
            free((void *)roots->genericRoots[index].arguments);
        }
    }
    free(roots->genericRoots);
    free(roots->indices);
    ZrCli_Compiler_AotPreserveRoots_Init(roots);
}

static void zr_cli_aot_preserve_roots_clear_generic_roots(SZrCliAotPreserveRoots *roots) {
    if (roots == ZR_NULL || roots->genericRoots == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0u; index < roots->genericRootCount; index++) {
        free((void *)roots->genericRoots[index].arguments);
        roots->genericRoots[index].arguments = ZR_NULL;
        roots->genericRoots[index].argumentCount = 0u;
        roots->genericRoots[index].target = ZR_NULL;
        roots->genericRoots[index].hasTypeSpecBinding = ZR_FALSE;
        roots->genericRoots[index].typeSpecToken = 0u;
        roots->genericRoots[index].signatureToken = 0u;
        roots->genericRoots[index].signatureHash = 0u;
        roots->genericRoots[index].hasMethodSpecBinding = ZR_FALSE;
        roots->genericRoots[index].methodSpecToken = 0u;
        roots->genericRoots[index].methodSpecMethodToken = 0u;
        roots->genericRoots[index].methodSpecSignatureHash = 0u;
        roots->genericRoots[index].hasGenericInstantiationBinding = ZR_FALSE;
        roots->genericRoots[index].genericInstantiationBaseToken = 0u;
        roots->genericRoots[index].genericInstantiationInstanceId = 0u;
        roots->genericRoots[index].genericInstantiationShareKind = 0u;
    }
    roots->genericRootCount = 0u;
}

static TZrBool zr_cli_aot_preserve_roots_append_unique(SZrCliAotPreserveRoots *roots, TZrUInt32 flatIndex) {
    TZrUInt32 *newIndices;
    TZrUInt32 newCapacity;

    if (roots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < roots->count; index++) {
        if (roots->indices[index] == flatIndex) {
            return ZR_TRUE;
        }
    }

    if (roots->count >= roots->capacity) {
        newCapacity = roots->capacity == 0u ? 4u : roots->capacity * 2u;
        newIndices = (TZrUInt32 *)realloc(roots->indices, sizeof(TZrUInt32) * newCapacity);
        if (newIndices == ZR_NULL) {
            return ZR_FALSE;
        }
        roots->indices = newIndices;
        roots->capacity = newCapacity;
    }

    roots->indices[roots->count++] = flatIndex;
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_generic_root_matches(const SZrAotManifestGenericRoot *root,
                                                        const TZrChar *target,
                                                        const TZrChar **arguments,
                                                        TZrUInt32 argumentCount) {
    if (root == ZR_NULL ||
        target == ZR_NULL ||
        arguments == ZR_NULL ||
        root->target == ZR_NULL ||
        root->arguments == ZR_NULL ||
        root->argumentCount != argumentCount ||
        strcmp(root->target, target) != 0) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < argumentCount; index++) {
        if (root->arguments[index] == ZR_NULL ||
            arguments[index] == ZR_NULL ||
            strcmp(root->arguments[index], arguments[index]) != 0) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_aot_read_u32_le(const TZrByte *blob,
                                      TZrSize blobLength,
                                      TZrSize *cursor,
                                      TZrUInt32 *outValue) {
    TZrSize offset;

    if (blob == ZR_NULL || cursor == ZR_NULL || outValue == ZR_NULL ||
        *cursor > blobLength || blobLength - *cursor < sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    offset = *cursor;
    *outValue = ((TZrUInt32)blob[offset]) |
                ((TZrUInt32)blob[offset + 1u] << 8u) |
                ((TZrUInt32)blob[offset + 2u] << 16u) |
                ((TZrUInt32)blob[offset + 3u] << 24u);
    *cursor = offset + sizeof(TZrUInt32);
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_metadata_string_matches(const SZrFunction *function,
                                                  TZrUInt32 stringIndex,
                                                  const TZrChar *expectedText) {
    if (function == ZR_NULL || expectedText == ZR_NULL ||
        function->metadataStringHeap == ZR_NULL || stringIndex == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->metadataStringHeapLength; index++) {
        const SZrMetadataStringHeapEntry *entry = &function->metadataStringHeap[index];
        const TZrChar *actualText = entry->value != ZR_NULL
                                            ? ZrCore_String_GetNativeString(entry->value)
                                            : ZR_NULL;

        if (entry->stringIndex == stringIndex &&
            actualText != ZR_NULL &&
            strcmp(actualText, expectedText) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 zr_cli_aot_metadata_string_stable_index(const TZrChar *text) {
    TZrSize length;
    TZrUInt64 hash;

    if (text == ZR_NULL || text[0] == '\0') {
        return 0u;
    }

    length = (TZrSize)strlen(text);
    hash = ZrCore_Hash_CreateStable64((const TZrByte *)text, length);
    return (TZrUInt32)(hash & 0x7FFFFFFFu) + 1u;
}

static TZrBool zr_cli_aot_metadata_string_find_index(const SZrFunction *function,
                                                     const TZrChar *text,
                                                     TZrUInt32 *outStringIndex) {
    if (outStringIndex != ZR_NULL) {
        *outStringIndex = 0u;
    }
    if (function == ZR_NULL || text == ZR_NULL || text[0] == '\0' ||
        outStringIndex == ZR_NULL || function->metadataStringHeap == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->metadataStringHeapLength; index++) {
        const SZrMetadataStringHeapEntry *entry = &function->metadataStringHeap[index];
        const TZrChar *actualText = entry->value != ZR_NULL
                                            ? ZrCore_String_GetNativeString(entry->value)
                                            : ZR_NULL;

        if (actualText != ZR_NULL && strcmp(actualText, text) == 0) {
            *outStringIndex = entry->stringIndex;
            return (TZrBool)(entry->stringIndex != 0u);
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_aot_metadata_string_heap_ensure(SZrState *state,
                                                      SZrFunction *function,
                                                      const TZrChar *text,
                                                      TZrUInt32 *outStringIndex) {
    SZrMetadataStringHeapEntry *newHeap;
    SZrString *newString;
    TZrUInt32 stableIndex;
    TZrUInt32 newLength;

    if (outStringIndex != ZR_NULL) {
        *outStringIndex = 0u;
    }
    if (state == ZR_NULL || state->global == ZR_NULL ||
        function == ZR_NULL || text == ZR_NULL || text[0] == '\0' ||
        outStringIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_cli_aot_metadata_string_find_index(function, text, outStringIndex)) {
        return ZR_TRUE;
    }

    stableIndex = zr_cli_aot_metadata_string_stable_index(text);
    if (stableIndex == 0u || function->metadataStringHeapLength == UINT32_MAX) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < function->metadataStringHeapLength; index++) {
        const SZrMetadataStringHeapEntry *entry = &function->metadataStringHeap[index];
        if (entry->stringIndex == stableIndex) {
            return ZR_FALSE;
        }
    }

    newLength = function->metadataStringHeapLength + 1u;
    newHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry) * newLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newHeap == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newHeap, 0, sizeof(SZrMetadataStringHeapEntry) * newLength);
    if (function->metadataStringHeap != ZR_NULL && function->metadataStringHeapLength > 0u) {
        ZrCore_Memory_RawCopy(newHeap,
                              function->metadataStringHeap,
                              sizeof(SZrMetadataStringHeapEntry) * function->metadataStringHeapLength);
    }

    newString = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);
    if (newString == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      newHeap,
                                      sizeof(SZrMetadataStringHeapEntry) * newLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }
    newHeap[function->metadataStringHeapLength].stringIndex = stableIndex;
    newHeap[function->metadataStringHeapLength].value = newString;

    if (function->metadataStringHeap != ZR_NULL && function->metadataStringHeapLength > 0u) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->metadataStringHeap,
                                      sizeof(SZrMetadataStringHeapEntry) * function->metadataStringHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    function->metadataStringHeap = newHeap;
    function->metadataStringHeapLength = newLength;
    *outStringIndex = stableIndex;
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_primitive_name_to_value_type(const TZrChar *text, EZrValueType *outType) {
    TZrSize length;

    if (outType != ZR_NULL) {
        *outType = ZR_VALUE_TYPE_UNKNOWN;
    }
    if (text == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    length = (TZrSize)strlen(text);
    if (length == 3u && memcmp(text, "int", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (length == 4u && memcmp(text, "uint", 4u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (length == 5u && memcmp(text, "float", 5u) == 0) {
        *outType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }
    if (length == 4u && memcmp(text, "bool", 4u) == 0) {
        *outType = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (length == 6u && memcmp(text, "string", 6u) == 0) {
        *outType = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }
    if ((length == 4u && memcmp(text, "null", 4u) == 0) ||
        (length == 4u && memcmp(text, "void", 4u) == 0)) {
        *outType = ZR_VALUE_TYPE_NULL;
        return ZR_TRUE;
    }
    if (length == 2u && memcmp(text, "i8", 2u) == 0) {
        *outType = ZR_VALUE_TYPE_INT8;
        return ZR_TRUE;
    }
    if (length == 2u && memcmp(text, "u8", 2u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT8;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "i16", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT16;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "u16", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT16;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "i32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT32;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "u32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT32;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "i64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "u64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "f32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_FLOAT;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(text, "f64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static const TZrByte CZrCliAotMetadataSignatureHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        's',
        'i',
        'g',
        '.',
        'v',
        '1',
        '\0',
};

static TZrUInt64 zr_cli_aot_metadata_signature_hash_v1(const TZrByte *signatureBlob,
                                                       TZrSize signatureBlobLength) {
    if (signatureBlob == ZR_NULL || signatureBlobLength == 0u) {
        return 0u;
    }

    return ZrCore_Hash_CreateStable64WithPrefix(CZrCliAotMetadataSignatureHashV1Prefix,
                                                sizeof(CZrCliAotMetadataSignatureHashV1Prefix),
                                                signatureBlob,
                                                signatureBlobLength);
}

static TZrBool zr_cli_aot_type_signature_matches_name(const SZrFunction *function,
                                                      const TZrByte *blob,
                                                      TZrSize blobLength,
                                                      TZrSize *cursor,
                                                      const TZrChar *expectedText) {
    TZrUInt8 node;
    TZrUInt32 encodedValue;
    EZrValueType primitiveType;

    if (blob == ZR_NULL || cursor == ZR_NULL || expectedText == ZR_NULL || *cursor >= blobLength) {
        return ZR_FALSE;
    }

    node = blob[*cursor];
    (*cursor)++;
    if (node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (!zr_cli_aot_read_u32_le(blob, blobLength, cursor, &encodedValue)) {
            return ZR_FALSE;
        }
        return zr_cli_aot_primitive_name_to_value_type(expectedText, &primitiveType) &&
               encodedValue == (TZrUInt32)primitiveType;
    }
    if (node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF ||
        node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        if (!zr_cli_aot_read_u32_le(blob, blobLength, cursor, &encodedValue) ||
            !zr_cli_aot_read_u32_le(blob, blobLength, cursor, &encodedValue)) {
            return ZR_FALSE;
        }
        return zr_cli_aot_metadata_string_matches(function, encodedValue, expectedText);
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_aot_generic_root_matches_type_spec_signature(const SZrFunction *function,
                                                                   const SZrAotManifestGenericRoot *root,
                                                                   const SZrMetadataTokenRecord *record) {
    const TZrByte *blob;
    TZrSize cursor = 0u;
    TZrUInt32 argumentCount;
    TZrUInt32 baseTypeOrStringIndex;
    TZrUInt8 baseNode;

    if (function == ZR_NULL ||
        root == ZR_NULL ||
        record == ZR_NULL ||
        root->target == ZR_NULL ||
        root->arguments == ZR_NULL ||
        root->argumentCount == 0u ||
        function->signatureBlobHeap == ZR_NULL ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + record->signatureBlobOffset;
    if (blob[cursor++] != ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
        cursor >= record->signatureBlobLength) {
        return ZR_FALSE;
    }

    baseNode = blob[cursor++];
    if ((baseNode != ZR_METADATA_SIGNATURE_NODE_TYPE_REF &&
         baseNode != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) ||
        !zr_cli_aot_read_u32_le(blob,
                                record->signatureBlobLength,
                                &cursor,
                                &baseTypeOrStringIndex) ||
        !zr_cli_aot_read_u32_le(blob,
                                record->signatureBlobLength,
                                &cursor,
                                &baseTypeOrStringIndex) ||
        !zr_cli_aot_metadata_string_matches(function, baseTypeOrStringIndex, root->target) ||
        !zr_cli_aot_read_u32_le(blob,
                                record->signatureBlobLength,
                                &cursor,
                                &argumentCount) ||
        argumentCount != root->argumentCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < root->argumentCount; index++) {
        if (!zr_cli_aot_type_signature_matches_name(function,
                                                    blob,
                                                    record->signatureBlobLength,
                                                    &cursor,
                                                    root->arguments[index])) {
            return ZR_FALSE;
        }
    }

    return cursor == record->signatureBlobLength;
}

static TZrBool zr_cli_aot_method_token_matches_target(const SZrFunction *function,
                                                      TZrMetadataToken methodToken,
                                                      const TZrChar *target) {
    if (function == ZR_NULL ||
        methodToken == 0u ||
        target == ZR_NULL ||
        function->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        const TZrChar *symbolName;

        if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION ||
            symbol->metadataToken != methodToken ||
            symbol->name == ZR_NULL) {
            continue;
        }

        symbolName = ZrCore_String_GetNativeString(symbol->name);
        if (symbolName != ZR_NULL && strcmp(symbolName, target) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_aot_generic_root_matches_method_spec_signature(const SZrFunction *function,
                                                                     const SZrAotManifestGenericRoot *root,
                                                                     const SZrMetadataTokenRecord *record,
                                                                     TZrMetadataToken *outMethodToken) {
    const TZrByte *blob;
    TZrSize cursor = 0u;
    TZrUInt32 argumentCount;
    TZrUInt32 methodToken;

    if (outMethodToken != ZR_NULL) {
        *outMethodToken = 0u;
    }
    if (function == ZR_NULL ||
        root == ZR_NULL ||
        record == ZR_NULL ||
        root->target == ZR_NULL ||
        root->arguments == ZR_NULL ||
        root->argumentCount == 0u ||
        function->signatureBlobHeap == ZR_NULL ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + record->signatureBlobOffset;
    if (blob[cursor++] != ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
        cursor >= record->signatureBlobLength ||
        blob[cursor++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &methodToken) ||
        !zr_cli_aot_method_token_matches_target(function, (TZrMetadataToken)methodToken, root->target) ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &argumentCount) ||
        argumentCount != root->argumentCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < root->argumentCount; index++) {
        if (!zr_cli_aot_type_signature_matches_name(function,
                                                    blob,
                                                    record->signatureBlobLength,
                                                    &cursor,
                                                    root->arguments[index])) {
            return ZR_FALSE;
        }
    }

    if (cursor != record->signatureBlobLength) {
        return ZR_FALSE;
    }
    if (outMethodToken != ZR_NULL) {
        *outMethodToken = (TZrMetadataToken)methodToken;
    }
    return ZR_TRUE;
}

static const SZrMetadataTokenRecord *zr_cli_aot_find_open_generic_base_record(
        const SZrFunction *function,
        const TZrChar *target,
        TZrUInt8 *outBaseNode,
        TZrUInt32 *outBaseValueType,
        TZrUInt32 *outBaseStringIndex);

static void zr_cli_aot_write_u8(TZrByte *buffer, TZrSize *cursor, TZrUInt8 value) {
    buffer[*cursor] = value;
    *cursor += 1u;
}

static void zr_cli_aot_write_u32(TZrByte *buffer, TZrSize *cursor, TZrUInt32 value) {
    TZrSize offset = *cursor;

    buffer[offset + 0u] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1u] = (TZrByte)((value >> 8u) & 0xFFu);
    buffer[offset + 2u] = (TZrByte)((value >> 16u) & 0xFFu);
    buffer[offset + 3u] = (TZrByte)((value >> 24u) & 0xFFu);
    *cursor = offset + sizeof(TZrUInt32);
}

static TZrSize zr_cli_aot_type_signature_length_for_text(const TZrChar *text) {
    EZrValueType primitiveType;

    if (zr_cli_aot_primitive_name_to_value_type(text, &primitiveType)) {
        return 1u + sizeof(TZrUInt32);
    }
    return 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

static TZrBool zr_cli_aot_write_type_signature_for_text(const SZrFunction *function,
                                                        TZrByte *buffer,
                                                        TZrSize *cursor,
                                                        const TZrChar *text) {
    EZrValueType primitiveType;
    TZrUInt32 stringIndex;

    if (function == ZR_NULL || buffer == ZR_NULL || cursor == ZR_NULL ||
        text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    if (zr_cli_aot_primitive_name_to_value_type(text, &primitiveType)) {
        zr_cli_aot_write_u8(buffer, cursor, ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
        zr_cli_aot_write_u32(buffer, cursor, (TZrUInt32)primitiveType);
        return ZR_TRUE;
    }

    if (!zr_cli_aot_metadata_string_find_index(function, text, &stringIndex)) {
        return ZR_FALSE;
    }
    zr_cli_aot_write_u8(buffer, cursor, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
    zr_cli_aot_write_u32(buffer, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    zr_cli_aot_write_u32(buffer, cursor, stringIndex);
    return ZR_TRUE;
}

static TZrMetadataToken zr_cli_aot_next_metadata_token(const SZrFunction *function,
                                                       EZrMetadataTableTag table) {
    TZrUInt32 maxRid = 0u;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return ZR_METADATA_TOKEN_MAKE(table, 1u);
    }

    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; index++) {
        TZrMetadataToken token = function->metadataTokenRecords[index].token;
        if (ZR_METADATA_TOKEN_TABLE(token) == (TZrUInt32)table &&
            ZR_METADATA_TOKEN_RID(token) > maxRid) {
            maxRid = ZR_METADATA_TOKEN_RID(token);
        }
    }
    if (maxRid >= ZR_METADATA_TOKEN_RID_MASK) {
        return 0u;
    }
    return ZR_METADATA_TOKEN_MAKE(table, maxRid + 1u);
}

static TZrBool zr_cli_aot_append_type_spec_record(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrMetadataToken typeSpecToken,
                                                  TZrMetadataToken signatureToken,
                                                  TZrUInt32 signatureBlobLength,
                                                  TZrUInt32 baseValueType,
                                                  TZrUInt32 baseStringIndex,
                                                  TZrUInt8 baseNode,
                                                  const SZrAotManifestGenericRoot *root,
                                                  TZrUInt64 *outSignatureHash) {
    SZrMetadataTokenRecord *newRecords;
    TZrByte *newSignatureHeap;
    TZrSize oldHeapLength;
    TZrSize newHeapLength;
    TZrSize cursor;
    TZrUInt64 signatureHash;
    TZrUInt32 newRecordLength;

    if (outSignatureHash != ZR_NULL) {
        *outSignatureHash = 0u;
    }
    if (state == ZR_NULL ||
        state->global == ZR_NULL ||
        function == ZR_NULL ||
        root == ZR_NULL ||
        signatureBlobLength == 0u ||
        root->arguments == ZR_NULL ||
        root->argumentCount == 0u ||
        function->signatureBlobHeapLength > UINT32_MAX - signatureBlobLength ||
        function->metadataTokenRecordLength > UINT32_MAX - 2u) {
        return ZR_FALSE;
    }

    oldHeapLength = function->signatureBlobHeapLength;
    newHeapLength = oldHeapLength + signatureBlobLength;
    newRecordLength = function->metadataTokenRecordLength + 2u;

    newSignatureHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                 newHeapLength,
                                                                 ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newSignatureHeap == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newSignatureHeap, 0, newHeapLength);
    if (function->signatureBlobHeap != ZR_NULL && oldHeapLength > 0u) {
        ZrCore_Memory_RawCopy(newSignatureHeap, function->signatureBlobHeap, oldHeapLength);
    }

    cursor = oldHeapLength;
    zr_cli_aot_write_u8(newSignatureHeap, &cursor, ZR_METADATA_SIGNATURE_NODE_GENERIC_INST);
    zr_cli_aot_write_u8(newSignatureHeap, &cursor, baseNode);
    zr_cli_aot_write_u32(newSignatureHeap, &cursor, baseValueType);
    zr_cli_aot_write_u32(newSignatureHeap, &cursor, baseStringIndex);
    zr_cli_aot_write_u32(newSignatureHeap, &cursor, root->argumentCount);
    for (TZrUInt32 index = 0u; index < root->argumentCount; index++) {
        if (!zr_cli_aot_write_type_signature_for_text(function,
                                                      newSignatureHeap,
                                                      &cursor,
                                                      root->arguments[index])) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          newSignatureHeap,
                                          newHeapLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
    }
    if (cursor != newHeapLength) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      newSignatureHeap,
                                      newHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    signatureHash = zr_cli_aot_metadata_signature_hash_v1(newSignatureHeap + oldHeapLength,
                                                          signatureBlobLength);
    if (signatureHash == 0u) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      newSignatureHeap,
                                      newHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    newRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * newRecordLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newRecords == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      newSignatureHeap,
                                      newHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newRecords, 0, sizeof(SZrMetadataTokenRecord) * newRecordLength);
    if (function->metadataTokenRecords != ZR_NULL && function->metadataTokenRecordLength > 0u) {
        ZrCore_Memory_RawCopy(newRecords,
                              function->metadataTokenRecords,
                              sizeof(SZrMetadataTokenRecord) * function->metadataTokenRecordLength);
    }

    newRecords[function->metadataTokenRecordLength].token = typeSpecToken;
    newRecords[function->metadataTokenRecordLength].relatedToken = signatureToken;
    newRecords[function->metadataTokenRecordLength].signatureBlobOffset = (TZrUInt32)oldHeapLength;
    newRecords[function->metadataTokenRecordLength].signatureBlobLength = signatureBlobLength;
    newRecords[function->metadataTokenRecordLength].signatureHash = signatureHash;
    newRecords[function->metadataTokenRecordLength + 1u].token = signatureToken;
    newRecords[function->metadataTokenRecordLength + 1u].relatedToken = typeSpecToken;
    newRecords[function->metadataTokenRecordLength + 1u].ownerToken = typeSpecToken;
    newRecords[function->metadataTokenRecordLength + 1u].signatureBlobOffset = (TZrUInt32)oldHeapLength;
    newRecords[function->metadataTokenRecordLength + 1u].signatureBlobLength = signatureBlobLength;
    newRecords[function->metadataTokenRecordLength + 1u].signatureHash = signatureHash;

    if (function->signatureBlobHeap != ZR_NULL && function->signatureBlobHeapLength > 0u) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->signatureBlobHeap,
                                      function->signatureBlobHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->metadataTokenRecords != ZR_NULL && function->metadataTokenRecordLength > 0u) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->metadataTokenRecords,
                                      sizeof(SZrMetadataTokenRecord) * function->metadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    function->signatureBlobHeap = newSignatureHeap;
    function->signatureBlobHeapLength = (TZrUInt32)newHeapLength;
    function->metadataTokenRecords = newRecords;
    function->metadataTokenRecordLength = newRecordLength;
    if (outSignatureHash != ZR_NULL) {
        *outSignatureHash = signatureHash;
    }
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_synthesize_generic_root_type_spec(SZrState *state,
                                                                     SZrFunction *function,
                                                                     SZrAotManifestGenericRoot *root) {
    const SZrMetadataTokenRecord *baseRecord;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 baseValueType;
    TZrUInt32 baseStringIndex;
    TZrUInt32 signatureBlobLength;
    TZrUInt64 signatureHash;
    TZrUInt8 baseNode;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        root == ZR_NULL ||
        root->hasTypeSpecBinding ||
        root->target == ZR_NULL ||
        root->arguments == ZR_NULL ||
        root->argumentCount == 0u) {
        return ZR_FALSE;
    }

    baseRecord = zr_cli_aot_find_open_generic_base_record(function,
                                                          root->target,
                                                          &baseNode,
                                                          &baseValueType,
                                                          &baseStringIndex);
    if (baseRecord == ZR_NULL || baseNode == 0u || baseStringIndex == 0u) {
        return ZR_FALSE;
    }

    signatureBlobLength = 1u + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    for (TZrUInt32 index = 0u; index < root->argumentCount; index++) {
        const TZrChar *argument = root->arguments[index];
        EZrValueType primitiveType;
        TZrUInt32 ignoredStringIndex;

        if (argument == ZR_NULL || argument[0] == '\0' ||
            signatureBlobLength > UINT32_MAX - zr_cli_aot_type_signature_length_for_text(argument)) {
            return ZR_FALSE;
        }
        if (!zr_cli_aot_primitive_name_to_value_type(argument, &primitiveType) &&
            !zr_cli_aot_metadata_string_heap_ensure(state, function, argument, &ignoredStringIndex)) {
            return ZR_FALSE;
        }
        signatureBlobLength += (TZrUInt32)zr_cli_aot_type_signature_length_for_text(argument);
    }

    typeSpecToken = zr_cli_aot_next_metadata_token(function, ZR_METADATA_TABLE_TYPE_SPEC);
    signatureToken = zr_cli_aot_next_metadata_token(function, ZR_METADATA_TABLE_SIGNATURE);
    if (typeSpecToken == 0u || signatureToken == 0u ||
        !zr_cli_aot_append_type_spec_record(state,
                                            function,
                                            typeSpecToken,
                                            signatureToken,
                                            signatureBlobLength,
                                            baseValueType,
                                            baseStringIndex,
                                            baseNode,
                                            root,
                                            &signatureHash)) {
        return ZR_FALSE;
    }

    root->hasTypeSpecBinding = ZR_TRUE;
    root->typeSpecToken = typeSpecToken;
    root->signatureToken = signatureToken;
    root->signatureHash = signatureHash;
    return ZR_TRUE;
}

static void zr_cli_aot_preserve_bind_generic_root_type_spec(SZrState *state,
                                                            SZrFunction *function,
                                                            SZrAotManifestGenericRoot *root) {
    if (function == ZR_NULL || root == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_TYPE_SPEC ||
            !zr_cli_aot_generic_root_matches_type_spec_signature(function, root, record)) {
            continue;
        }

        root->hasTypeSpecBinding = ZR_TRUE;
        root->typeSpecToken = record->token;
        root->signatureToken = record->relatedToken;
        root->signatureHash = record->signatureHash;
        return;
    }

    (void)zr_cli_aot_preserve_synthesize_generic_root_type_spec(state, function, root);
}

static void zr_cli_aot_preserve_bind_generic_root_method_spec(SZrFunction *function,
                                                              SZrAotManifestGenericRoot *root) {
    if (function == ZR_NULL || root == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        TZrMetadataToken methodToken;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_SIGNATURE ||
            !zr_cli_aot_generic_root_matches_method_spec_signature(function, root, record, &methodToken)) {
            continue;
        }

        root->hasMethodSpecBinding = ZR_TRUE;
        root->methodSpecToken = record->token;
        root->methodSpecMethodToken = methodToken;
        root->methodSpecSignatureHash = record->signatureHash;
        return;
    }
}

static TZrBool zr_cli_aot_named_type_record_matches_name(const SZrFunction *function,
                                                         const SZrMetadataTokenRecord *record,
                                                         TZrUInt8 expectedNode,
                                                         const TZrChar *expectedText) {
    const TZrByte *blob;
    TZrSize cursor = 0u;
    TZrUInt32 encodedValue;

    if (function == ZR_NULL ||
        record == ZR_NULL ||
        expectedText == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + record->signatureBlobOffset;
    if (blob[cursor++] != expectedNode ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedValue) ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedValue) ||
        !zr_cli_aot_metadata_string_matches(function, encodedValue, expectedText)) {
        return ZR_FALSE;
    }

    return cursor == record->signatureBlobLength;
}

static TZrBool zr_cli_aot_named_type_record_signature(const SZrFunction *function,
                                                      const SZrMetadataTokenRecord *record,
                                                      TZrUInt8 expectedNode,
                                                      const TZrChar *expectedText,
                                                      TZrUInt32 *outValueType,
                                                      TZrUInt32 *outStringIndex) {
    const TZrByte *blob;
    TZrSize cursor = 0u;
    TZrUInt32 encodedValueType;
    TZrUInt32 encodedStringIndex;

    if (outValueType != ZR_NULL) {
        *outValueType = 0u;
    }
    if (outStringIndex != ZR_NULL) {
        *outStringIndex = 0u;
    }
    if (function == ZR_NULL ||
        record == ZR_NULL ||
        expectedText == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + record->signatureBlobOffset;
    if (blob[cursor++] != expectedNode ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedValueType) ||
        !zr_cli_aot_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedStringIndex) ||
        !zr_cli_aot_metadata_string_matches(function, encodedStringIndex, expectedText) ||
        cursor != record->signatureBlobLength) {
        return ZR_FALSE;
    }

    if (outValueType != ZR_NULL) {
        *outValueType = encodedValueType;
    }
    if (outStringIndex != ZR_NULL) {
        *outStringIndex = encodedStringIndex;
    }
    return ZR_TRUE;
}

static const SZrMetadataTokenRecord *zr_cli_aot_find_open_generic_base_record(
        const SZrFunction *function,
        const TZrChar *target,
        TZrUInt8 *outBaseNode,
        TZrUInt32 *outBaseValueType,
        TZrUInt32 *outBaseStringIndex) {
    static const TZrUInt8 baseNodes[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
    };

    if (outBaseNode != ZR_NULL) {
        *outBaseNode = 0u;
    }
    if (outBaseValueType != ZR_NULL) {
        *outBaseValueType = 0u;
    }
    if (outBaseStringIndex != ZR_NULL) {
        *outBaseStringIndex = 0u;
    }
    if (function == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        target == ZR_NULL ||
        target[0] == '\0') {
        return ZR_NULL;
    }

    for (TZrSize baseNodeIndex = 0u; baseNodeIndex < sizeof(baseNodes) / sizeof(baseNodes[0]); baseNodeIndex++) {
        TZrUInt8 baseNode = baseNodes[baseNodeIndex];
        TZrUInt32 baseTable = baseNode == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF
                                      ? (TZrUInt32)ZR_METADATA_TABLE_TYPE_DEF
                                      : (TZrUInt32)ZR_METADATA_TABLE_TYPE_REF;

        for (TZrUInt32 recordIndex = 0u; recordIndex < function->metadataTokenRecordLength; recordIndex++) {
            const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[recordIndex];
            TZrUInt32 baseValueType;
            TZrUInt32 baseStringIndex;

            if (ZR_METADATA_TOKEN_TABLE(record->token) != baseTable ||
                !zr_cli_aot_named_type_record_signature(function,
                                                        record,
                                                        baseNode,
                                                        target,
                                                        &baseValueType,
                                                        &baseStringIndex)) {
                continue;
            }

            if (outBaseNode != ZR_NULL) {
                *outBaseNode = baseNode;
            }
            if (outBaseValueType != ZR_NULL) {
                *outBaseValueType = baseValueType;
            }
            if (outBaseStringIndex != ZR_NULL) {
                *outBaseStringIndex = baseStringIndex;
            }
            return record;
        }
    }

    return ZR_NULL;
}

static TZrBool zr_cli_aot_generic_root_type_spec_base_node(const SZrFunction *function,
                                                           const SZrAotManifestGenericRoot *root,
                                                           TZrUInt8 *outBaseNode) {
    if (outBaseNode != ZR_NULL) {
        *outBaseNode = 0u;
    }
    if (function == ZR_NULL ||
        root == ZR_NULL ||
        outBaseNode == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        !root->hasTypeSpecBinding) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        const TZrByte *blob;

        if (record->token != root->typeSpecToken ||
            record->signatureBlobLength < 2u ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[0] != ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
            (blob[1] != ZR_METADATA_SIGNATURE_NODE_TYPE_REF &&
             blob[1] != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF)) {
            return ZR_FALSE;
        }

        *outBaseNode = blob[1];
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrMetadataToken zr_cli_aot_preserve_resolve_generic_root_base_token(
        const SZrFunction *function,
        const SZrAotManifestGenericRoot *root) {
    TZrUInt32 baseTable;
    TZrUInt8 baseNode;

    if (root == ZR_NULL) {
        return 0u;
    }
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL || root->target == ZR_NULL) {
        return root->typeSpecToken;
    }
    if (!zr_cli_aot_generic_root_type_spec_base_node(function, root, &baseNode)) {
        return root->typeSpecToken;
    }

    baseTable = baseNode == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF
                        ? (TZrUInt32)ZR_METADATA_TABLE_TYPE_DEF
                        : (TZrUInt32)ZR_METADATA_TABLE_TYPE_REF;

    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == baseTable &&
            zr_cli_aot_named_type_record_matches_name(function, record, baseNode, root->target)) {
            return record->token;
        }
    }

    return root->typeSpecToken;
}

static void zr_cli_aot_generic_instantiation_arguments_free(
        SZrState *state,
        SZrGenericInstantiationTypeArgument *arguments,
        TZrUInt32 argumentCount) {
    if (arguments == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0u; index < argumentCount; index++) {
        ZrParser_InferredType_Free(state, &arguments[index].type);
        arguments[index].shape = ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE;
    }
}

static TZrBool zr_cli_aot_generic_instantiation_argument_init(SZrState *state,
                                                              const TZrChar *text,
                                                              SZrGenericInstantiationTypeArgument *argument) {
    EZrValueType primitiveType;
    SZrString *typeName;

    if (state == ZR_NULL || text == ZR_NULL || text[0] == '\0' || argument == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_cli_aot_primitive_name_to_value_type(text, &primitiveType)) {
        ZrParser_InferredType_Init(state, &argument->type, primitiveType);
    } else {
        typeName = ZrCore_String_CreateFromNative(state, (TZrChar *)text);
        if (typeName == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_InitFull(state,
                                       &argument->type,
                                       ZR_VALUE_TYPE_OBJECT,
                                       ZR_FALSE,
                                       typeName);
    }
    argument->shape = ZrParser_GenericInstantiation_InferTypeShape(&argument->type);
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_bind_generic_root_instantiation(
        SZrState *state,
        SZrGenericInstantiationTable *genericInstantiationTable,
        const SZrFunction *function,
        SZrAotManifestGenericRoot *root) {
    SZrGenericInstantiationTypeArgument *arguments;
    const SZrGenericInstantiationRecord *record = ZR_NULL;
    TZrMetadataToken baseToken;
    TZrUInt32 initializedCount = 0u;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL ||
        genericInstantiationTable == ZR_NULL ||
        root == ZR_NULL ||
        !root->hasTypeSpecBinding ||
        root->arguments == ZR_NULL ||
        root->argumentCount == 0u) {
        return ZR_FALSE;
    }

    arguments = (SZrGenericInstantiationTypeArgument *)malloc(
            sizeof(SZrGenericInstantiationTypeArgument) * root->argumentCount);
    if (arguments == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(arguments, 0, sizeof(SZrGenericInstantiationTypeArgument) * root->argumentCount);

    for (TZrUInt32 index = 0u; index < root->argumentCount; index++) {
        if (!zr_cli_aot_generic_instantiation_argument_init(state, root->arguments[index], &arguments[index])) {
            goto cleanup;
        }
        initializedCount++;
    }

    baseToken = zr_cli_aot_preserve_resolve_generic_root_base_token(function, root);
    if (!ZrParser_GenericInstantiationTable_GetOrAddResolved(state,
                                                             genericInstantiationTable,
                                                             baseToken,
                                                             arguments,
                                                             root->argumentCount,
                                                             &record) ||
        record == ZR_NULL) {
        goto cleanup;
    }

    root->hasGenericInstantiationBinding = ZR_TRUE;
    root->genericInstantiationBaseToken = record->baseToken;
    root->genericInstantiationInstanceId = record->cInstanceId;
    root->genericInstantiationShareKind = (TZrUInt32)record->shareKind;
    success = ZR_TRUE;

cleanup:
    zr_cli_aot_generic_instantiation_arguments_free(state, arguments, initializedCount);
    free(arguments);
    return success;
}

static TZrBool zr_cli_aot_preserve_roots_append_generic(SZrCliAotPreserveRoots *roots,
                                                        SZrState *state,
                                                        SZrGenericInstantiationTable *genericInstantiationTable,
                                                        SZrFunction *function,
                                                        const TZrChar *target,
                                                        SZrString **genericArguments,
                                                        TZrSize genericArgumentCount) {
    const TZrChar **argumentTexts;
    SZrAotManifestGenericRoot *newGenericRoots;
    TZrUInt32 argumentCount;
    TZrUInt32 newCapacity;

    if (roots == ZR_NULL ||
        target == ZR_NULL ||
        target[0] == '\0' ||
        genericArguments == ZR_NULL ||
        genericArgumentCount == 0u ||
        genericArgumentCount > UINT32_MAX) {
        return ZR_FALSE;
    }

    argumentCount = (TZrUInt32)genericArgumentCount;
    argumentTexts = (const TZrChar **)malloc(sizeof(TZrChar *) * argumentCount);
    if (argumentTexts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < argumentCount; index++) {
        argumentTexts[index] = genericArguments[index] != ZR_NULL
                                       ? ZrCore_String_GetNativeString(genericArguments[index])
                                       : ZR_NULL;
        if (argumentTexts[index] == ZR_NULL || argumentTexts[index][0] == '\0') {
            free(argumentTexts);
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < roots->genericRootCount; index++) {
        if (zr_cli_aot_preserve_generic_root_matches(&roots->genericRoots[index],
                                                     target,
                                                     argumentTexts,
                                                     argumentCount)) {
            free(argumentTexts);
            return ZR_TRUE;
        }
    }

    if (roots->genericRootCount >= roots->genericRootCapacity) {
        newCapacity = roots->genericRootCapacity == 0u ? 4u : roots->genericRootCapacity * 2u;
        newGenericRoots = (SZrAotManifestGenericRoot *)realloc(
                roots->genericRoots,
                sizeof(SZrAotManifestGenericRoot) * newCapacity);
        if (newGenericRoots == ZR_NULL) {
            free(argumentTexts);
            return ZR_FALSE;
        }
        roots->genericRoots = newGenericRoots;
        roots->genericRootCapacity = newCapacity;
    }

    roots->genericRoots[roots->genericRootCount].target = target;
    roots->genericRoots[roots->genericRootCount].arguments = argumentTexts;
    roots->genericRoots[roots->genericRootCount].argumentCount = argumentCount;
    roots->genericRoots[roots->genericRootCount].hasTypeSpecBinding = ZR_FALSE;
    roots->genericRoots[roots->genericRootCount].typeSpecToken = 0u;
    roots->genericRoots[roots->genericRootCount].signatureToken = 0u;
    roots->genericRoots[roots->genericRootCount].signatureHash = 0u;
    roots->genericRoots[roots->genericRootCount].hasMethodSpecBinding = ZR_FALSE;
    roots->genericRoots[roots->genericRootCount].methodSpecToken = 0u;
    roots->genericRoots[roots->genericRootCount].methodSpecMethodToken = 0u;
    roots->genericRoots[roots->genericRootCount].methodSpecSignatureHash = 0u;
    roots->genericRoots[roots->genericRootCount].hasGenericInstantiationBinding = ZR_FALSE;
    roots->genericRoots[roots->genericRootCount].genericInstantiationBaseToken = 0u;
    roots->genericRoots[roots->genericRootCount].genericInstantiationInstanceId = 0u;
    roots->genericRoots[roots->genericRootCount].genericInstantiationShareKind = 0u;
    zr_cli_aot_preserve_bind_generic_root_method_spec(function,
                                                      &roots->genericRoots[roots->genericRootCount]);
    if (!roots->genericRoots[roots->genericRootCount].hasMethodSpecBinding) {
        zr_cli_aot_preserve_bind_generic_root_type_spec(state,
                                                        function,
                                                        &roots->genericRoots[roots->genericRootCount]);
    }
    if (roots->genericRoots[roots->genericRootCount].hasTypeSpecBinding &&
        !zr_cli_aot_preserve_bind_generic_root_instantiation(
                state,
                genericInstantiationTable,
                function,
                &roots->genericRoots[roots->genericRootCount])) {
        free(argumentTexts);
        roots->genericRoots[roots->genericRootCount].target = ZR_NULL;
        roots->genericRoots[roots->genericRootCount].arguments = ZR_NULL;
        roots->genericRoots[roots->genericRootCount].argumentCount = 0u;
        roots->genericRoots[roots->genericRootCount].hasTypeSpecBinding = ZR_FALSE;
        roots->genericRoots[roots->genericRootCount].typeSpecToken = 0u;
        roots->genericRoots[roots->genericRootCount].signatureToken = 0u;
        roots->genericRoots[roots->genericRootCount].signatureHash = 0u;
        roots->genericRoots[roots->genericRootCount].hasMethodSpecBinding = ZR_FALSE;
        roots->genericRoots[roots->genericRootCount].methodSpecToken = 0u;
        roots->genericRoots[roots->genericRootCount].methodSpecMethodToken = 0u;
        roots->genericRoots[roots->genericRootCount].methodSpecSignatureHash = 0u;
        roots->genericRoots[roots->genericRootCount].hasGenericInstantiationBinding = ZR_FALSE;
        roots->genericRoots[roots->genericRootCount].genericInstantiationBaseToken = 0u;
        roots->genericRoots[roots->genericRootCount].genericInstantiationInstanceId = 0u;
        roots->genericRoots[roots->genericRootCount].genericInstantiationShareKind = 0u;
        return ZR_FALSE;
    }
    roots->genericRootCount++;
    return ZR_TRUE;
}

static const TZrChar *zr_cli_aot_preserve_callable_target_for_module(const TZrChar *target,
                                                                     const TZrChar *moduleName) {
    TZrSize moduleNameLength;

    if (target == ZR_NULL || target[0] == '\0' || moduleName == ZR_NULL || moduleName[0] == '\0') {
        return ZR_NULL;
    }

    moduleNameLength = (TZrSize)strlen(moduleName);
    if (strncmp(target, moduleName, (size_t)moduleNameLength) == 0 &&
        target[moduleNameLength] == '.' &&
        target[moduleNameLength + 1u] != '\0') {
        return target + moduleNameLength + 1u;
    }
    if (strchr(target, '.') == ZR_NULL) {
        return target;
    }

    return ZR_NULL;
}

static TZrBool zr_cli_aot_preserve_target_has_module_prefix(const TZrChar *target, const TZrChar *moduleName) {
    TZrSize moduleNameLength;

    if (target == ZR_NULL || moduleName == ZR_NULL || moduleName[0] == '\0') {
        return ZR_FALSE;
    }

    moduleNameLength = (TZrSize)strlen(moduleName);
    return (TZrBool)(strncmp(target, moduleName, (size_t)moduleNameLength) == 0 &&
                     target[moduleNameLength] == '.' &&
                     target[moduleNameLength + 1u] != '\0');
}

static TZrBool zr_cli_aot_preserve_resolve_append_method(SZrState *state,
                                                         SZrFunction *function,
                                                         const TZrChar *callableName,
                                                         SZrCliAotPreserveRoots *roots,
                                                         TZrBool *outResolved) {
    TZrUInt32 flatIndex = 0u;

    if (outResolved != ZR_NULL) {
        *outResolved = ZR_FALSE;
    }
    if (callableName == ZR_NULL || callableName[0] == '\0') {
        return ZR_TRUE;
    }
    if (!ZrParser_Writer_ResolveTopLevelCallableFlatIndex(state, function, callableName, &flatIndex)) {
        return ZR_TRUE;
    }
    if (!zr_cli_aot_preserve_roots_append_unique(roots, flatIndex)) {
        return ZR_FALSE;
    }
    if (outResolved != ZR_NULL) {
        *outResolved = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_apply_method_target(SZrState *state,
                                                       SZrFunction *function,
                                                       const TZrChar *target,
                                                       const TZrChar *moduleName,
                                                       SZrCliAotPreserveRoots *roots) {
    const TZrChar *moduleLocalTarget;
    TZrBool resolved = ZR_FALSE;
    TZrBool resolvedModuleLocal = ZR_FALSE;

    if (!zr_cli_aot_preserve_resolve_append_method(state, function, target, roots, &resolved)) {
        return ZR_FALSE;
    }
    if (resolved) {
        return ZR_TRUE;
    }

    moduleLocalTarget = zr_cli_aot_preserve_callable_target_for_module(target, moduleName);
    if (moduleLocalTarget != ZR_NULL && moduleLocalTarget != target) {
        if (!zr_cli_aot_preserve_resolve_append_method(state,
                                                       function,
                                                       moduleLocalTarget,
                                                       roots,
                                                       &resolvedModuleLocal)) {
            return ZR_FALSE;
        }
        return resolvedModuleLocal;
    }

    if (strchr(target, '.') == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_binding_has_type_prefix(const TZrChar *bindingName, const TZrChar *typeTarget) {
    TZrSize typeTargetLength;

    if (bindingName == ZR_NULL || typeTarget == ZR_NULL || typeTarget[0] == '\0') {
        return ZR_FALSE;
    }

    typeTargetLength = (TZrSize)strlen(typeTarget);
    return (TZrBool)(strncmp(bindingName, typeTarget, (size_t)typeTargetLength) == 0 &&
                     bindingName[typeTargetLength] == '.' &&
                     bindingName[typeTargetLength + 1u] != '\0');
}

static TZrBool zr_cli_aot_preserve_apply_type_method_prefix(SZrState *state,
                                                            SZrFunction *function,
                                                            const TZrChar *typeTarget,
                                                            SZrCliAotPreserveRoots *roots,
                                                            TZrBool *ioResolvedAny) {
    if (function == ZR_NULL || typeTarget == ZR_NULL || roots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 bindingIndex = 0u; bindingIndex < function->topLevelCallableBindingLength; bindingIndex++) {
        const SZrFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[bindingIndex];
        const TZrChar *bindingName = binding->name != ZR_NULL
                                             ? ZrCore_String_GetNativeString(binding->name)
                                             : ZR_NULL;
        TZrBool resolved = ZR_FALSE;

        if (!zr_cli_aot_preserve_binding_has_type_prefix(bindingName, typeTarget)) {
            continue;
        }
        if (!zr_cli_aot_preserve_resolve_append_method(state, function, bindingName, roots, &resolved)) {
            return ZR_FALSE;
        }
        if (!resolved) {
            return ZR_FALSE;
        }
        if (ioResolvedAny != ZR_NULL) {
            *ioResolvedAny = ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_apply_type_target(SZrState *state,
                                                     SZrFunction *function,
                                                     const TZrChar *target,
                                                     const TZrChar *moduleName,
                                                     EZrLibrary_ProjectPreserveMembers members,
                                                     SZrCliAotPreserveRoots *roots) {
    const TZrChar *moduleLocalTarget;
    TZrBool resolvedAny = ZR_FALSE;

    if (members != ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS &&
        members != ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_ALL) {
        return ZR_TRUE;
    }

    if (!zr_cli_aot_preserve_apply_type_method_prefix(state, function, target, roots, &resolvedAny)) {
        return ZR_FALSE;
    }

    moduleLocalTarget = zr_cli_aot_preserve_callable_target_for_module(target, moduleName);
    if (moduleLocalTarget != ZR_NULL && moduleLocalTarget != target) {
        if (!zr_cli_aot_preserve_apply_type_method_prefix(state,
                                                          function,
                                                          moduleLocalTarget,
                                                          roots,
                                                          &resolvedAny)) {
            return ZR_FALSE;
        }
    }

    if (resolvedAny) {
        return ZR_TRUE;
    }
    if (strchr(target, '.') == ZR_NULL || zr_cli_aot_preserve_target_has_module_prefix(target, moduleName)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_cli_aot_preserve_rule_feature_matches(const SZrLibrary_Project *project,
                                                        const SZrLibrary_ProjectPreserveRule *rule) {
    const TZrChar *featureName;

    if (rule == ZR_NULL || !rule->hasFeatureValue) {
        return ZR_TRUE;
    }
    if (project == ZR_NULL || rule->feature == ZR_NULL) {
        return ZR_FALSE;
    }

    featureName = ZrCore_String_GetNativeString(rule->feature);
    if (featureName == ZR_NULL || featureName[0] == '\0') {
        return ZR_FALSE;
    }

    for (TZrSize featureIndex = 0; featureIndex < project->featureSwitchCount; featureIndex++) {
        const SZrLibrary_ProjectFeatureSwitch *feature = &project->featureSwitches[featureIndex];
        const TZrChar *currentName = feature->name != ZR_NULL
                                             ? ZrCore_String_GetNativeString(feature->name)
                                             : ZR_NULL;
        if (currentName != ZR_NULL && strcmp(currentName, featureName) == 0) {
            return (TZrBool)(feature->value == rule->featureValue);
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_aot_preserve_apply_generic_target(const SZrLibrary_ProjectPreserveRule *rule,
                                                        SZrState *state,
                                                        SZrGenericInstantiationTable *genericInstantiationTable,
                                                        SZrFunction *function,
                                                        const TZrChar *target,
                                                        SZrCliAotPreserveRoots *roots) {
    if (rule == ZR_NULL || roots == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_cli_aot_preserve_roots_append_generic(roots,
                                                    state,
                                                    genericInstantiationTable,
                                                    function,
                                                    target,
                                                    rule->genericArguments,
                                                    rule->genericArgumentCount);
}

TZrBool ZrCli_Compiler_ApplyProjectAotPreserveRules(const SZrCliProjectContext *project,
                                                    SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *moduleName,
                                                    SZrAotWriterOptions *options,
                                                    SZrCliAotPreserveRoots *roots) {
    const SZrLibrary_Project *libraryProject;
    SZrGenericInstantiationTable genericInstantiationTable;
    TZrBool success = ZR_FALSE;

    if (project == ZR_NULL || project->libraryProject == ZR_NULL || state == ZR_NULL || function == ZR_NULL ||
        moduleName == ZR_NULL || options == ZR_NULL || roots == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_GenericInstantiationTable_Init(state, &genericInstantiationTable);
    roots->count = 0u;
    zr_cli_aot_preserve_roots_clear_generic_roots(roots);
    options->manifestPreserveFunctionFlatIndices = ZR_NULL;
    options->manifestPreserveFunctionFlatIndexCount = 0u;
    options->manifestPreserveGenericRoots = ZR_NULL;
    options->manifestPreserveGenericRootCount = 0u;
    libraryProject = project->libraryProject;
    for (TZrSize ruleIndex = 0; ruleIndex < libraryProject->preserveRuleCount; ruleIndex++) {
        const SZrLibrary_ProjectPreserveRule *rule = &libraryProject->preserveRules[ruleIndex];
        const TZrChar *target;

        if (rule->target == ZR_NULL) {
            continue;
        }
        if (!zr_cli_aot_preserve_rule_feature_matches(libraryProject, rule)) {
            continue;
        }

        target = ZrCore_String_GetNativeString(rule->target);
        if (rule->kind == ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD) {
            if (!zr_cli_aot_preserve_apply_method_target(state, function, target, moduleName, roots)) {
                goto cleanup;
            }
            continue;
        }
        if (rule->kind == ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE) {
            if (!zr_cli_aot_preserve_apply_type_target(state,
                                                       function,
                                                       target,
                                                       moduleName,
                                                       rule->members,
                                                       roots)) {
                goto cleanup;
            }
            continue;
        }
        if (rule->kind == ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC) {
            if (!zr_cli_aot_preserve_apply_generic_target(rule,
                                                          state,
                                                          &genericInstantiationTable,
                                                          function,
                                                          target,
                                                          roots)) {
                goto cleanup;
            }
            continue;
        }
        goto cleanup;
    }

    options->manifestPreserveFunctionFlatIndices = roots->count > 0u ? roots->indices : ZR_NULL;
    options->manifestPreserveFunctionFlatIndexCount = roots->count;
    options->manifestPreserveGenericRoots = roots->genericRootCount > 0u ? roots->genericRoots : ZR_NULL;
    options->manifestPreserveGenericRootCount = roots->genericRootCount;
    success = ZR_TRUE;

cleanup:
    ZrParser_GenericInstantiationTable_Free(state, &genericInstantiationTable);
    return success;
}

TZrBool ZrCli_Compiler_WriteAotCFileForModule(const SZrCliProjectContext *project,
                                              SZrState *state,
                                              SZrFunction *function,
                                              const TZrChar *moduleName,
                                              const TZrChar *sourceHash,
                                              const TZrChar *zroHash,
                                              const TZrChar *zroPath,
                                              const TZrChar *aotCPath) {
    SZrAotWriterOptions options;
    TZrByte *embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    SZrCliAotPreserveRoots preserveRoots;
    TZrBool success = ZR_FALSE;

    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);
    if (project == ZR_NULL || state == ZR_NULL || function == ZR_NULL ||
        moduleName == ZR_NULL || zroPath == ZR_NULL || aotCPath == ZR_NULL || aotCPath[0] == '\0') {
        return ZR_FALSE;
    }

    if (!zr_cli_read_binary_file(zroPath, &embeddedBlob, &embeddedBlobLength)) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = sourceHash != ZR_NULL && sourceHash[0] != '\0' ? sourceHash : ZR_NULL;
    options.zroHash = zroHash != ZR_NULL && zroHash[0] != '\0' ? zroHash : ZR_NULL;
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = options.zroHash;
    options.embeddedModuleBlob = embeddedBlob;
    options.embeddedModuleBlobLength = embeddedBlobLength;
    options.requireExecutableLowering = ZR_TRUE;

    if (ZrCli_Compiler_ApplyProjectAotWriterOptions(project, &options) &&
        ZrCli_Compiler_ApplyProjectAotPreserveRules(project, state, function, moduleName, &options, &preserveRoots)) {
        success = ZrCli_Project_EnsureParentDirectory(aotCPath) &&
                  ZrParser_Writer_WriteAotCFileWithOptions(state, function, aotCPath, &options);
    }

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    free(embeddedBlob);
    return success;
}
