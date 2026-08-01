#include "zr_vm_parser/test_contract.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <string.h>

#define ZR_TEST_MANIFEST_MAGIC ((TZrUInt32)0x4d54525aU)
#define ZR_TEST_MANIFEST_NULL_STRING ((TZrUInt32)UINT32_MAX)

typedef struct SZrTestManifestWriter {
    TZrByte *cursor;
    TZrSize remaining;
} SZrTestManifestWriter;

typedef struct SZrTestManifestReader {
    const TZrByte *cursor;
    TZrSize remaining;
} SZrTestManifestReader;

static TZrSize test_manifest_string_size(const TZrChar *value) {
    if (value == ZR_NULL) {
        return sizeof(TZrUInt32);
    }
    return sizeof(TZrUInt32) + strlen(value);
}

static TZrBool test_manifest_add_size(TZrSize *size, TZrSize amount) {
    if (size == ZR_NULL || amount > SIZE_MAX - *size) {
        return ZR_FALSE;
    }
    *size += amount;
    return ZR_TRUE;
}

static TZrBool test_manifest_constant_size(
        const SZrParserTestConstant *constant,
        TZrSize *size) {
    TZrSize payloadSize;

    if (constant == ZR_NULL || size == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (constant->kind) {
    case ZR_PARSER_TEST_CONSTANT_NULL:
        payloadSize = 0U;
        break;
    case ZR_PARSER_TEST_CONSTANT_BOOL:
        payloadSize = sizeof(TZrUInt8);
        break;
    case ZR_PARSER_TEST_CONSTANT_INT:
        payloadSize = sizeof(TZrInt64);
        break;
    case ZR_PARSER_TEST_CONSTANT_UINT:
        payloadSize = sizeof(TZrUInt64);
        break;
    case ZR_PARSER_TEST_CONSTANT_FLOAT:
        payloadSize = sizeof(TZrDouble);
        break;
    case ZR_PARSER_TEST_CONSTANT_STRING:
        payloadSize = test_manifest_string_size(constant->value.stringValue);
        break;
    default:
        return ZR_FALSE;
    }
    return test_manifest_add_size(size, sizeof(TZrUInt32)) &&
           test_manifest_add_size(size, payloadSize);
}

static TZrBool test_manifest_compute_size(
        const SZrParserTestManifest *manifest,
        TZrSize *outSize) {
    TZrSize size = sizeof(TZrUInt32) * 4U + sizeof(TZrUInt64);

    if (manifest == ZR_NULL || outSize == ZR_NULL ||
        manifest->schemaVersion != ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION ||
        manifest->entryCount > ZR_PARSER_TEST_MANIFEST_MAX_ENTRIES ||
        (manifest->entryCount > 0U && manifest->entries == ZR_NULL) ||
        !test_manifest_add_size(&size, test_manifest_string_size(manifest->targetTriple))) {
        return ZR_FALSE;
    }
    for (TZrUInt32 entryIndex = 0U; entryIndex < manifest->entryCount; entryIndex++) {
        const SZrParserTestEntry *entry = &manifest->entries[entryIndex];
        if (entry->qualifiedName == ZR_NULL ||
            entry->caseCount > ZR_PARSER_TEST_MANIFEST_MAX_CASES_PER_ENTRY ||
            (entry->caseCount > 0U && entry->cases == ZR_NULL) ||
            !test_manifest_add_size(&size, sizeof(TZrUInt32) * 8U + sizeof(TZrUInt8)) ||
            !test_manifest_add_size(&size, test_manifest_string_size(entry->moduleId)) ||
            !test_manifest_add_size(&size, test_manifest_string_size(entry->qualifiedName)) ||
            !test_manifest_add_size(&size, test_manifest_string_size(entry->skipReason))) {
            return ZR_FALSE;
        }
        for (TZrUInt32 caseIndex = 0U; caseIndex < entry->caseCount; caseIndex++) {
            const SZrParserTestCaseDescriptor *testCase = &entry->cases[caseIndex];
            if (testCase->argumentCount > ZR_PARSER_TEST_MANIFEST_MAX_ARGUMENTS_PER_CASE ||
                (testCase->argumentCount > 0U && testCase->arguments == ZR_NULL) ||
                !test_manifest_add_size(&size, sizeof(TZrUInt32) * 2U)) {
                return ZR_FALSE;
            }
            for (TZrUInt32 argumentIndex = 0U;
                 argumentIndex < testCase->argumentCount;
                 argumentIndex++) {
                if (!test_manifest_constant_size(&testCase->arguments[argumentIndex], &size)) {
                    return ZR_FALSE;
                }
            }
        }
    }
    if (size > UINT32_MAX) {
        return ZR_FALSE;
    }
    *outSize = size;
    return ZR_TRUE;
}

static TZrBool test_manifest_write(
        SZrTestManifestWriter *writer,
        const void *value,
        TZrSize size) {
    if (writer == ZR_NULL || value == ZR_NULL || size > writer->remaining) {
        return ZR_FALSE;
    }
    memcpy(writer->cursor, value, size);
    writer->cursor += size;
    writer->remaining -= size;
    return ZR_TRUE;
}

static TZrBool test_manifest_write_string(
        SZrTestManifestWriter *writer,
        const TZrChar *value) {
    TZrUInt32 length;

    if (value == ZR_NULL) {
        length = ZR_TEST_MANIFEST_NULL_STRING;
        return test_manifest_write(writer, &length, sizeof(length));
    }
    length = (TZrUInt32)strlen(value);
    if (length > ZR_PARSER_TEST_MANIFEST_MAX_STRING_BYTES ||
        !test_manifest_write(writer, &length, sizeof(length))) {
        return ZR_FALSE;
    }
    return length == 0U || test_manifest_write(writer, value, length);
}

static TZrBool test_manifest_write_constant(
        SZrTestManifestWriter *writer,
        const SZrParserTestConstant *constant) {
    TZrUInt32 kind;

    if (constant == ZR_NULL) {
        return ZR_FALSE;
    }
    kind = (TZrUInt32)constant->kind;
    if (!test_manifest_write(writer, &kind, sizeof(kind))) {
        return ZR_FALSE;
    }
    switch (constant->kind) {
    case ZR_PARSER_TEST_CONSTANT_NULL:
        return ZR_TRUE;
    case ZR_PARSER_TEST_CONSTANT_BOOL: {
        TZrUInt8 value = constant->value.boolValue ? 1U : 0U;
        return test_manifest_write(writer, &value, sizeof(value));
    }
    case ZR_PARSER_TEST_CONSTANT_INT:
        return test_manifest_write(writer, &constant->value.intValue, sizeof(TZrInt64));
    case ZR_PARSER_TEST_CONSTANT_UINT:
        return test_manifest_write(writer, &constant->value.uintValue, sizeof(TZrUInt64));
    case ZR_PARSER_TEST_CONSTANT_FLOAT:
        return test_manifest_write(writer, &constant->value.floatValue, sizeof(TZrDouble));
    case ZR_PARSER_TEST_CONSTANT_STRING:
        return test_manifest_write_string(writer, constant->value.stringValue);
    default:
        return ZR_FALSE;
    }
}

TZrBool ZrParser_TestManifest_Encode(
        SZrState *state,
        const SZrParserTestManifest *manifest,
        TZrByte **outData,
        TZrUInt32 *outLength) {
    TZrSize size;
    TZrByte *data;
    SZrTestManifestWriter writer;
    TZrUInt32 magic = ZR_TEST_MANIFEST_MAGIC;
    TZrUInt32 reserved = 0U;

    if (outData != ZR_NULL) *outData = ZR_NULL;
    if (outLength != ZR_NULL) *outLength = 0U;
    if (state == ZR_NULL || outData == ZR_NULL || outLength == ZR_NULL ||
        !test_manifest_compute_size(manifest, &size)) {
        return ZR_FALSE;
    }
    data = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global, size, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (data == ZR_NULL) {
        return ZR_FALSE;
    }
    writer.cursor = data;
    writer.remaining = size;
    if (!test_manifest_write(&writer, &magic, sizeof(magic)) ||
        !test_manifest_write(&writer, &manifest->schemaVersion, sizeof(TZrUInt32)) ||
        !test_manifest_write(&writer, &manifest->entryCount, sizeof(TZrUInt32)) ||
        !test_manifest_write(&writer, &reserved, sizeof(reserved)) ||
        !test_manifest_write(&writer, &manifest->moduleGraphHash, sizeof(TZrUInt64)) ||
        !test_manifest_write_string(&writer, manifest->targetTriple)) {
        goto encode_failure;
    }
    for (TZrUInt32 entryIndex = 0U; entryIndex < manifest->entryCount; entryIndex++) {
        const SZrParserTestEntry *entry = &manifest->entries[entryIndex];
        TZrUInt8 isAsync = entry->isAsync ? 1U : 0U;
        TZrUInt32 source[] = {
                entry->sourceRange.start.line,
                entry->sourceRange.start.column,
                entry->sourceRange.end.line,
                entry->sourceRange.end.column,
        };
        if (!test_manifest_write(&writer, &entry->functionSymbolId, sizeof(TZrUInt32)) ||
            !test_manifest_write(&writer, &entry->functionTypeId, sizeof(TZrUInt32)) ||
            !test_manifest_write(&writer, &entry->callableChildIndex, sizeof(TZrUInt32)) ||
            !test_manifest_write(&writer, &isAsync, sizeof(isAsync)) ||
            !test_manifest_write(&writer, source, sizeof(source)) ||
            !test_manifest_write_string(&writer, entry->moduleId) ||
            !test_manifest_write_string(&writer, entry->qualifiedName) ||
            !test_manifest_write_string(&writer, entry->skipReason) ||
            !test_manifest_write(&writer, &entry->caseCount, sizeof(TZrUInt32))) {
            goto encode_failure;
        }
        for (TZrUInt32 caseIndex = 0U; caseIndex < entry->caseCount; caseIndex++) {
            const SZrParserTestCaseDescriptor *testCase = &entry->cases[caseIndex];
            if (!test_manifest_write(&writer, &testCase->ordinal, sizeof(TZrUInt32)) ||
                !test_manifest_write(&writer, &testCase->argumentCount, sizeof(TZrUInt32))) {
                goto encode_failure;
            }
            for (TZrUInt32 argumentIndex = 0U;
                 argumentIndex < testCase->argumentCount;
                 argumentIndex++) {
                if (!test_manifest_write_constant(&writer, &testCase->arguments[argumentIndex])) {
                    goto encode_failure;
                }
            }
        }
    }
    if (writer.remaining != 0U) {
        goto encode_failure;
    }
    *outData = data;
    *outLength = (TZrUInt32)size;
    return ZR_TRUE;

encode_failure:
    ZrCore_Memory_RawFreeWithType(
            state->global, data, size, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_FALSE;
}

static TZrBool test_manifest_read(
        SZrTestManifestReader *reader,
        TZrPtr destination,
        TZrSize size) {
    if (reader == ZR_NULL || destination == ZR_NULL || size > reader->remaining) {
        return ZR_FALSE;
    }
    memcpy(destination, reader->cursor, size);
    reader->cursor += size;
    reader->remaining -= size;
    return ZR_TRUE;
}

static TZrBool test_manifest_read_string(
        SZrState *state,
        SZrTestManifestReader *reader,
        TZrChar **outValue) {
    TZrUInt32 length;
    TZrChar *value;

    if (outValue != ZR_NULL) *outValue = ZR_NULL;
    if (state == ZR_NULL || reader == ZR_NULL || outValue == ZR_NULL ||
        !test_manifest_read(reader, &length, sizeof(length))) {
        return ZR_FALSE;
    }
    if (length == ZR_TEST_MANIFEST_NULL_STRING) {
        return ZR_TRUE;
    }
    if (length > ZR_PARSER_TEST_MANIFEST_MAX_STRING_BYTES || length > reader->remaining) {
        return ZR_FALSE;
    }
    value = (TZrChar *)ZrCore_Memory_RawMallocWithType(
            state->global, (TZrSize)length + 1U, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    if (length > 0U) {
        memcpy(value, reader->cursor, length);
    }
    value[length] = '\0';
    *outValue = value;
    reader->cursor += length;
    reader->remaining -= length;
    return ZR_TRUE;
}

static TZrBool test_manifest_read_constant(
        SZrState *state,
        SZrTestManifestReader *reader,
        SZrParserTestConstant *constant) {
    TZrUInt32 kind;

    if (constant == ZR_NULL || !test_manifest_read(reader, &kind, sizeof(kind))) {
        return ZR_FALSE;
    }
    constant->kind = (EZrParserTestConstantKind)kind;
    switch (constant->kind) {
    case ZR_PARSER_TEST_CONSTANT_NULL:
        return ZR_TRUE;
    case ZR_PARSER_TEST_CONSTANT_BOOL: {
        TZrUInt8 value;
        if (!test_manifest_read(reader, &value, sizeof(value)) || value > 1U) return ZR_FALSE;
        constant->value.boolValue = value != 0U ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    case ZR_PARSER_TEST_CONSTANT_INT:
        return test_manifest_read(reader, &constant->value.intValue, sizeof(TZrInt64));
    case ZR_PARSER_TEST_CONSTANT_UINT:
        return test_manifest_read(reader, &constant->value.uintValue, sizeof(TZrUInt64));
    case ZR_PARSER_TEST_CONSTANT_FLOAT:
        return test_manifest_read(reader, &constant->value.floatValue, sizeof(TZrDouble));
    case ZR_PARSER_TEST_CONSTANT_STRING:
        return test_manifest_read_string(state, reader, &constant->value.stringValue);
    default:
        return ZR_FALSE;
    }
}

void ZrParser_TestManifest_Free(SZrState *state, SZrParserTestManifest *manifest) {
    if (state == ZR_NULL || manifest == ZR_NULL) {
        return;
    }
    for (TZrUInt32 entryIndex = 0U;
         manifest->entries != ZR_NULL && entryIndex < manifest->entryCount;
         entryIndex++) {
        SZrParserTestEntry *entry = &manifest->entries[entryIndex];
        for (TZrUInt32 caseIndex = 0U;
             entry->cases != ZR_NULL && caseIndex < entry->caseCount;
             caseIndex++) {
            SZrParserTestCaseDescriptor *testCase = &entry->cases[caseIndex];
            for (TZrUInt32 argumentIndex = 0U;
                 testCase->arguments != ZR_NULL &&
                 argumentIndex < testCase->argumentCount;
                 argumentIndex++) {
                SZrParserTestConstant *argument = &testCase->arguments[argumentIndex];
                if (argument->kind == ZR_PARSER_TEST_CONSTANT_STRING &&
                    argument->value.stringValue != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(
                            state->global,
                            argument->value.stringValue,
                            strlen(argument->value.stringValue) + 1U,
                            ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
            }
            if (testCase->arguments != ZR_NULL && testCase->argumentCount > 0U) {
                ZrCore_Memory_RawFreeWithType(
                        state->global, testCase->arguments,
                        sizeof(SZrParserTestConstant) * testCase->argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        if (entry->cases != ZR_NULL && entry->caseCount > 0U) {
            ZrCore_Memory_RawFreeWithType(
                    state->global, entry->cases,
                    sizeof(SZrParserTestCaseDescriptor) * entry->caseCount,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (entry->moduleId != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    state->global, entry->moduleId, strlen(entry->moduleId) + 1U,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (entry->qualifiedName != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    state->global, entry->qualifiedName,
                    strlen(entry->qualifiedName) + 1U,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (entry->skipReason != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    state->global, entry->skipReason,
                    strlen(entry->skipReason) + 1U,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
    }
    if (manifest->entries != ZR_NULL && manifest->entryCount > 0U) {
        ZrCore_Memory_RawFreeWithType(
                state->global, manifest->entries,
                sizeof(SZrParserTestEntry) * manifest->entryCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (manifest->targetTriple != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global, manifest->targetTriple,
                strlen(manifest->targetTriple) + 1U,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    memset(manifest, 0, sizeof(*manifest));
}

TZrBool ZrParser_TestManifest_Decode(
        SZrState *state,
        const TZrByte *data,
        TZrUInt32 length,
        SZrParserTestManifest *outManifest) {
    SZrTestManifestReader reader;
    TZrUInt32 magic;
    TZrUInt32 reserved;

    if (outManifest != ZR_NULL) memset(outManifest, 0, sizeof(*outManifest));
    if (state == ZR_NULL || data == ZR_NULL || length == 0U || outManifest == ZR_NULL) {
        return ZR_FALSE;
    }
    reader.cursor = data;
    reader.remaining = length;
    if (!test_manifest_read(&reader, &magic, sizeof(magic)) || magic != ZR_TEST_MANIFEST_MAGIC ||
        !test_manifest_read(&reader, &outManifest->schemaVersion, sizeof(TZrUInt32)) ||
        outManifest->schemaVersion != ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION ||
        !test_manifest_read(&reader, &outManifest->entryCount, sizeof(TZrUInt32)) ||
        outManifest->entryCount > ZR_PARSER_TEST_MANIFEST_MAX_ENTRIES ||
        !test_manifest_read(&reader, &reserved, sizeof(reserved)) || reserved != 0U ||
        !test_manifest_read(&reader, &outManifest->moduleGraphHash, sizeof(TZrUInt64)) ||
        !test_manifest_read_string(state, &reader, &outManifest->targetTriple)) {
        goto decode_failure;
    }
    if (outManifest->entryCount > 0U) {
        outManifest->entries = (SZrParserTestEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrParserTestEntry) * outManifest->entryCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (outManifest->entries == ZR_NULL) goto decode_failure;
        memset(outManifest->entries, 0, sizeof(SZrParserTestEntry) * outManifest->entryCount);
    }
    for (TZrUInt32 entryIndex = 0U; entryIndex < outManifest->entryCount; entryIndex++) {
        SZrParserTestEntry *entry = &outManifest->entries[entryIndex];
        TZrUInt8 isAsync;
        TZrUInt32 source[4];
        if (!test_manifest_read(&reader, &entry->functionSymbolId, sizeof(TZrUInt32)) ||
            !test_manifest_read(&reader, &entry->functionTypeId, sizeof(TZrUInt32)) ||
            !test_manifest_read(&reader, &entry->callableChildIndex, sizeof(TZrUInt32)) ||
            !test_manifest_read(&reader, &isAsync, sizeof(isAsync)) || isAsync > 1U ||
            !test_manifest_read(&reader, source, sizeof(source)) ||
            !test_manifest_read_string(state, &reader, &entry->moduleId) ||
            !test_manifest_read_string(state, &reader, &entry->qualifiedName) ||
            entry->qualifiedName == ZR_NULL ||
            !test_manifest_read_string(state, &reader, &entry->skipReason) ||
            !test_manifest_read(&reader, &entry->caseCount, sizeof(TZrUInt32)) ||
            entry->caseCount > ZR_PARSER_TEST_MANIFEST_MAX_CASES_PER_ENTRY) {
            goto decode_failure;
        }
        entry->isAsync = isAsync != 0U ? ZR_TRUE : ZR_FALSE;
        entry->sourceRange.start.line = source[0];
        entry->sourceRange.start.column = source[1];
        entry->sourceRange.end.line = source[2];
        entry->sourceRange.end.column = source[3];
        if (entry->caseCount > 0U) {
            entry->cases = (SZrParserTestCaseDescriptor *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrParserTestCaseDescriptor) * entry->caseCount,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
            if (entry->cases == ZR_NULL) goto decode_failure;
            memset(entry->cases, 0, sizeof(SZrParserTestCaseDescriptor) * entry->caseCount);
        }
        for (TZrUInt32 caseIndex = 0U; caseIndex < entry->caseCount; caseIndex++) {
            SZrParserTestCaseDescriptor *testCase = &entry->cases[caseIndex];
            if (!test_manifest_read(&reader, &testCase->ordinal, sizeof(TZrUInt32)) ||
                !test_manifest_read(&reader, &testCase->argumentCount, sizeof(TZrUInt32)) ||
                testCase->argumentCount > ZR_PARSER_TEST_MANIFEST_MAX_ARGUMENTS_PER_CASE) {
                goto decode_failure;
            }
            if (testCase->argumentCount > 0U) {
                testCase->arguments = (SZrParserTestConstant *)ZrCore_Memory_RawMallocWithType(
                        state->global,
                        sizeof(SZrParserTestConstant) * testCase->argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (testCase->arguments == ZR_NULL) goto decode_failure;
                memset(testCase->arguments, 0,
                       sizeof(SZrParserTestConstant) * testCase->argumentCount);
            }
            for (TZrUInt32 argumentIndex = 0U;
                 argumentIndex < testCase->argumentCount;
                 argumentIndex++) {
                if (!test_manifest_read_constant(
                            state, &reader, &testCase->arguments[argumentIndex])) {
                    goto decode_failure;
                }
            }
        }
    }
    if (reader.remaining != 0U) goto decode_failure;
    return ZR_TRUE;

decode_failure:
    ZrParser_TestManifest_Free(state, outManifest);
    return ZR_FALSE;
}

TZrBool ZrParser_TestManifest_Validate(
        SZrState *state,
        const TZrByte *data,
        TZrUInt32 length) {
    SZrParserTestManifest manifest;
    TZrBool valid = ZrParser_TestManifest_Decode(state, data, length, &manifest);
    if (valid) {
        ZrParser_TestManifest_Free(state, &manifest);
    }
    return valid;
}
