#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrBinaryFixtureReader {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)fileSize;
    }
    return buffer;
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *compile_debug_metadata_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "class Box {\n"
            "    pub var raw: int;\n"
            "    pub fn read(): int {\n"
            "        return this.raw;\n"
            "    }\n"
            "}\n"
            "return 0;";
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_debug_metadata_locals_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "var first = 1;\n"
            "var second = first + 2;\n"
            "return second;\n";
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const SZrFunction *debug_metadata_function_constant_at(SZrState *state,
                                                               const SZrFunction *function,
                                                               TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }
    constant = &function->constantValueList[constantIndex];
    if (constant->type != ZR_VALUE_TYPE_FUNCTION || constant->value.object == ZR_NULL || constant->isNative) {
        return ZR_NULL;
    }
    return ZR_CAST_FUNCTION(state, constant->value.object);
}

static const SZrFunctionTypedLocalBinding *debug_metadata_find_receiver_binding(
        const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if ((binding->roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u) {
            return binding;
        }
    }
    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *debug_metadata_find_receiver_binding_in_prototype_data(
        SZrState *state,
        const SZrFunction *function) {
    const TZrByte *current;
    TZrSize remaining;
    TZrUInt32 prototypeIndex;

    if (state == ZR_NULL || function == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32)) {
        return ZR_NULL;
    }

    current = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    for (prototypeIndex = 0u; prototypeIndex < function->prototypeCount; prototypeIndex++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrCompiledMemberInfo *members;
        TZrSize prototypeSize;
        TZrUInt32 memberIndex;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_NULL;
        }
        prototype = (const SZrCompiledPrototypeInfo *)current;
        prototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                        prototype->inheritsCount * sizeof(TZrUInt32) +
                        prototype->decoratorsCount * sizeof(TZrUInt32) +
                        prototype->membersCount * sizeof(SZrCompiledMemberInfo);
        if (remaining < prototypeSize) {
            return ZR_NULL;
        }

        members = (const SZrCompiledMemberInfo *)(current + sizeof(SZrCompiledPrototypeInfo) +
                                                  prototype->inheritsCount * sizeof(TZrUInt32) +
                                                  prototype->decoratorsCount * sizeof(TZrUInt32));
        for (memberIndex = 0u; memberIndex < prototype->membersCount; memberIndex++) {
            const SZrFunctionTypedLocalBinding *binding = debug_metadata_find_receiver_binding(
                    debug_metadata_function_constant_at(
                            state,
                            function,
                            members[memberIndex].functionConstantIndex));
            if (binding != ZR_NULL) {
                return binding;
            }
        }

        current += prototypeSize;
        remaining -= prototypeSize;
    }
    return ZR_NULL;
}

static void test_binary_roundtrip_preserves_debug_source_identity_and_module_name(void) {
    const char *binaryPath = "debug_metadata_roundtrip_test.zro";
    const char *sourcePath = "fixtures/debug/debug_metadata_roundtrip_test.zr";
    const char *moduleName = "fixtures.debug.debug_metadata_roundtrip_test";
    const char *sourceHash = "fixture-source-hash";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrBinaryWriterOptions options;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_metadata_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->sourceCodeList);
    TEST_ASSERT_EQUAL_STRING(sourcePath, ZrCore_String_GetNativeString(function->sourceCodeList));

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.moduleHash = sourceHash;

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &options));

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;

    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].name);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].md5);
    TEST_ASSERT_EQUAL_STRING(moduleName, ZrCore_String_GetNativeString(sourceObject->modules[0].name));
    TEST_ASSERT_EQUAL_STRING(sourceHash, ZrCore_String_GetNativeString(sourceObject->modules[0].md5));
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    TEST_ASSERT_TRUE(sourceObject->modules[0].entryFunction->debugInfosLength > 0);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction->debugInfos[0].sourceFile);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction->debugInfos[0].sourceHash);
    TEST_ASSERT_EQUAL_STRING(sourcePath,
                             ZrCore_String_GetNativeString(sourceObject->modules[0].entryFunction->debugInfos[0].sourceFile));
    TEST_ASSERT_EQUAL_STRING(sourceHash,
                             ZrCore_String_GetNativeString(sourceObject->modules[0].entryFunction->debugInfos[0].sourceHash));

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_NOT_NULL(runtimeFunction->sourceCodeList);
    TEST_ASSERT_NOT_NULL(runtimeFunction->sourceHash);
    TEST_ASSERT_EQUAL_STRING(sourcePath, ZrCore_String_GetNativeString(runtimeFunction->sourceCodeList));
    TEST_ASSERT_EQUAL_STRING(sourceHash, ZrCore_String_GetNativeString(runtimeFunction->sourceHash));

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_binary_roundtrip_preserves_full_debug_source_path_without_writer_options(void) {
    const char *binaryPath = "debug_metadata_roundtrip_plain_test.zro";
    const char *sourcePath = "fixtures/debug/nested/debug_metadata_roundtrip_plain_test.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_metadata_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->sourceCodeList);
    TEST_ASSERT_EQUAL_STRING(sourcePath, ZrCore_String_GetNativeString(function->sourceCodeList));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;

    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    TEST_ASSERT_TRUE(sourceObject->modules[0].entryFunction->debugInfosLength > 0);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction->debugInfos[0].sourceFile);
    TEST_ASSERT_EQUAL_STRING(sourcePath,
                             ZrCore_String_GetNativeString(sourceObject->modules[0].entryFunction->debugInfos[0].sourceFile));

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_NOT_NULL(runtimeFunction->sourceCodeList);
    TEST_ASSERT_EQUAL_STRING(sourcePath, ZrCore_String_GetNativeString(runtimeFunction->sourceCodeList));

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_binary_roundtrip_preserves_instruction_debug_ranges(void) {
    const char *binaryPath = "debug_metadata_instruction_ranges_test.zro";
    const char *sourcePath = "fixtures/debug/debug_metadata_instruction_ranges_test.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;
    TZrBool foundSerializedRange = ZR_FALSE;
    TZrBool foundRuntimeRange = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_metadata_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;

    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    TEST_ASSERT_TRUE(sourceObject->modules[0].entryFunction->debugInfosLength > 0);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction->debugInfos[0].instructionRanges);
    TEST_ASSERT_TRUE(sourceObject->modules[0].entryFunction->debugInfos[0].instructionsLength > 0);

    for (TZrSize index = 0; index < sourceObject->modules[0].entryFunction->debugInfos[0].instructionsLength; index++) {
        const SZrIoInstructionSourceRange *range =
                &sourceObject->modules[0].entryFunction->debugInfos[0].instructionRanges[index];
        if (range->startLine == 0 || range->startColumn == 0 || range->endLine == 0 || range->endColumn == 0) {
            continue;
        }

        foundSerializedRange = ZR_TRUE;
        break;
    }
    TEST_ASSERT_TRUE(foundSerializedRange);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_NOT_NULL(runtimeFunction->executionLocationInfoList);
    TEST_ASSERT_TRUE(runtimeFunction->executionLocationInfoLength > 0);

    for (TZrUInt32 index = 0; index < runtimeFunction->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *location = &runtimeFunction->executionLocationInfoList[index];
        if (location->lineInSource == 0 || location->columnInSourceStart == 0 || location->lineInSourceEnd == 0 ||
            location->columnInSourceEnd == 0) {
            continue;
        }

        foundRuntimeRange = ZR_TRUE;
        break;
    }
    TEST_ASSERT_TRUE(foundRuntimeRange);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_binary_roundtrip_preserves_local_variable_names_and_slots(void) {
    const char *binaryPath = "debug_metadata_locals_roundtrip_test.zro";
    const char *sourcePath = "fixtures/debug/debug_metadata_locals_roundtrip_test.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_metadata_locals_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->localVariableLength >= 2);
    TEST_ASSERT_NOT_NULL(function->localVariableList[0].name);
    TEST_ASSERT_NOT_NULL(function->localVariableList[1].name);
    TEST_ASSERT_EQUAL_STRING("first", ZrCore_String_GetNativeString(function->localVariableList[0].name));
    TEST_ASSERT_EQUAL_STRING("second", ZrCore_String_GetNativeString(function->localVariableList[1].name));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;

    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableLength, runtimeFunction->localVariableLength);
    TEST_ASSERT_NOT_NULL(runtimeFunction->localVariableList[0].name);
    TEST_ASSERT_NOT_NULL(runtimeFunction->localVariableList[1].name);
    TEST_ASSERT_EQUAL_STRING("first", ZrCore_String_GetNativeString(runtimeFunction->localVariableList[0].name));
    TEST_ASSERT_EQUAL_STRING("second", ZrCore_String_GetNativeString(runtimeFunction->localVariableList[1].name));
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[0].stackSlot, runtimeFunction->localVariableList[0].stackSlot);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[1].stackSlot, runtimeFunction->localVariableList[1].stackSlot);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[0].offsetActivate, runtimeFunction->localVariableList[0].offsetActivate);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[1].offsetActivate, runtimeFunction->localVariableList[1].offsetActivate);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[0].offsetDead, runtimeFunction->localVariableList[0].offsetDead);
    TEST_ASSERT_EQUAL_UINT32(function->localVariableList[1].offsetDead, runtimeFunction->localVariableList[1].offsetDead);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_binary_roundtrip_preserves_canonical_local_binding_identity(void) {
    const char *binaryPath = "debug_metadata_local_identity_roundtrip_test.zro";
    const char *sourcePath = "fixtures/debug/debug_metadata_local_identity_roundtrip_test.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;
    const SZrFunctionTypedLocalBinding *sourceBinding;
    const SZrFunctionTypedLocalBinding *runtimeBinding;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_metadata_locals_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength >= 2);
    sourceBinding = &function->typedLocalBindings[1];
    TEST_ASSERT_EQUAL_STRING("second", ZrCore_String_GetNativeString(sourceBinding->name));
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->symbolId);
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->typeId);
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->placeId);
    TEST_ASSERT_TRUE(sourceBinding->declarationEndLine >= sourceBinding->declarationStartLine);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_EQUAL_UINT32(function->typedLocalBindingLength, runtimeFunction->typedLocalBindingLength);
    runtimeBinding = &runtimeFunction->typedLocalBindings[1];
    TEST_ASSERT_EQUAL_STRING("second", ZrCore_String_GetNativeString(runtimeBinding->name));
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->symbolId, runtimeBinding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->typeId, runtimeBinding->typeId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->placeId, runtimeBinding->placeId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationStartLine, runtimeBinding->declarationStartLine);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationStartColumn, runtimeBinding->declarationStartColumn);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationEndLine, runtimeBinding->declarationEndLine);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationEndColumn, runtimeBinding->declarationEndColumn);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_binary_roundtrip_preserves_canonical_receiver_binding_role(void) {
    const char *binaryPath = "debug_metadata_receiver_role_roundtrip_test.zro";
    const char *sourcePath = "fixtures/debug/debug_metadata_receiver_role_roundtrip_test.zr";
    SZrState *sourceState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;
    const SZrFunctionTypedLocalBinding *sourceBinding;
    const SZrFunctionTypedLocalBinding *runtimeBinding;

    TEST_ASSERT_NOT_NULL(sourceState);

    function = compile_debug_metadata_fixture(sourceState, sourcePath);
    TEST_ASSERT_NOT_NULL(function);
    sourceBinding = debug_metadata_find_receiver_binding_in_prototype_data(sourceState, function);
    TEST_ASSERT_NOT_NULL(sourceBinding);
    TEST_ASSERT_TRUE((sourceBinding->roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u);
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->symbolId);
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->typeId);
    TEST_ASSERT_NOT_EQUAL(0u, sourceBinding->placeId);
    TEST_ASSERT_TRUE(sourceBinding->declarationEndLine >= sourceBinding->declarationStartLine);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(sourceState, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    ZrCore_Io_Init(sourceState, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(sourceState, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    runtimeBinding = debug_metadata_find_receiver_binding_in_prototype_data(sourceState, runtimeFunction);
    TEST_ASSERT_NOT_NULL(runtimeBinding);
    TEST_ASSERT_TRUE((runtimeBinding->roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->symbolId, runtimeBinding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->typeId, runtimeBinding->typeId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->placeId, runtimeBinding->placeId);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationStartLine, runtimeBinding->declarationStartLine);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationStartColumn, runtimeBinding->declarationStartColumn);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationEndLine, runtimeBinding->declarationEndLine);
    TEST_ASSERT_EQUAL_UINT32(sourceBinding->declarationEndColumn, runtimeBinding->declarationEndColumn);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(sourceState, function);
    ZrCore_Function_Free(sourceState, runtimeFunction);
    ZrTests_Runtime_State_Destroy(sourceState);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_roundtrip_preserves_debug_source_identity_and_module_name);
    RUN_TEST(test_binary_roundtrip_preserves_full_debug_source_path_without_writer_options);
    RUN_TEST(test_binary_roundtrip_preserves_instruction_debug_ranges);
    RUN_TEST(test_binary_roundtrip_preserves_local_variable_names_and_slots);
    RUN_TEST(test_binary_roundtrip_preserves_canonical_local_binding_identity);
    RUN_TEST(test_binary_roundtrip_preserves_canonical_receiver_binding_role);
    return UNITY_END();
}
