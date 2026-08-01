#include "unity.h"

#include "module_fixture_support.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/test_contract.h"
#include "zr_vm_parser/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void manifest_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static void test_test_manifest_roundtrips_through_binary_and_runtime_loader(void) {
    static const TZrChar *sourceText =
            "#zr.testing.test#\n"
            "#zr.testing.case(value: 7)#\n"
            "#zr.testing.skip(reason: \"documented\")#\n"
            "fn roundtrip(value: int): void { }\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrFunction *function;
    SZrFunction *loadedFunction;
    SZrParserTestManifest manifest;
    TZrChar binaryPath[512];
    TZrByte *binaryBytes;
    TZrSize binaryLength = 0U;
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *ioSource;
    TZrByte originalMagicByte;
    TZrByte *corruptManifest;
    TZrUInt32 invalidSchema = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION + 1U;
    TZrUInt32 invalidEntryCount = ZR_PARSER_TEST_MANIFEST_MAX_ENTRIES + 1U;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "test_manifest_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_CompileTest(
            state, sourceText, strlen(sourceText), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Validate(
            state,
            function->testManifestData,
            function->testManifestDataLength));
    TEST_ASSERT_FALSE(ZrParser_TestManifest_Validate(
            state,
            function->testManifestData,
            function->testManifestDataLength - 1U));
    corruptManifest = (TZrByte *)malloc(function->testManifestDataLength + 1U);
    TEST_ASSERT_NOT_NULL(corruptManifest);
    memcpy(
            corruptManifest,
            function->testManifestData,
            function->testManifestDataLength);
    memcpy(
            corruptManifest + sizeof(TZrUInt32),
            &invalidSchema,
            sizeof(invalidSchema));
    TEST_ASSERT_FALSE(ZrParser_TestManifest_Validate(
            state, corruptManifest, function->testManifestDataLength));
    memcpy(
            corruptManifest,
            function->testManifestData,
            function->testManifestDataLength);
    memcpy(
            corruptManifest + sizeof(TZrUInt32) * 2U,
            &invalidEntryCount,
            sizeof(invalidEntryCount));
    TEST_ASSERT_FALSE(ZrParser_TestManifest_Validate(
            state, corruptManifest, function->testManifestDataLength));
    memcpy(
            corruptManifest,
            function->testManifestData,
            function->testManifestDataLength);
    corruptManifest[function->testManifestDataLength] = 0U;
    TEST_ASSERT_FALSE(ZrParser_TestManifest_Validate(
            state, corruptManifest, function->testManifestDataLength + 1U));
    free(corruptManifest);

    snprintf(binaryPath,
             sizeof(binaryPath),
             "%s/test_manifest_roundtrip.zro",
             ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(
            ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, binaryLength);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(
            state,
            io,
            ZrTests_Fixture_ReaderRead,
            manifest_reader_close_noop,
            &reader);
    io->isBinary = ZR_TRUE;
    ioSource = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(ioSource);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST,
            ioSource->versionPatch);
    TEST_ASSERT_NOT_NULL(ioSource->modules[0].entryFunction);
    TEST_ASSERT_EQUAL_UINT32(
            function->testManifestDataLength,
            ioSource->modules[0].entryFunction->testManifestDataLength);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Validate(
            state,
            ioSource->modules[0].entryFunction->testManifestData,
            (TZrUInt32)ioSource->modules[0].entryFunction->testManifestDataLength));

    loadedFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    TEST_ASSERT_NOT_NULL(loadedFunction);
    TEST_ASSERT_EQUAL_UINT32(
            function->testManifestDataLength,
            loadedFunction->testManifestDataLength);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            state,
            loadedFunction->testManifestData,
            loadedFunction->testManifestDataLength,
            &manifest));
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entryCount);
    TEST_ASSERT_EQUAL_STRING("roundtrip", manifest.entries[0].qualifiedName);
    TEST_ASSERT_EQUAL_STRING("documented", manifest.entries[0].skipReason);
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entries[0].caseCount);
    TEST_ASSERT_EQUAL_INT64(
            7, manifest.entries[0].cases[0].arguments[0].value.intValue);
    ZrParser_TestManifest_Free(state, &manifest);

    originalMagicByte = ioSource->modules[0].entryFunction->testManifestData[0];
    ioSource->modules[0].entryFunction->testManifestData[0] ^= 1U;
    TEST_ASSERT_NULL(ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource));
    ioSource->modules[0].entryFunction->testManifestData[0] = originalMagicByte;

    ZrCore_Function_Free(state, loadedFunction);
    ZrCore_Function_Free(state, function);
    ZrCore_Io_ReadSourceFree(state->global, ioSource);
    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    remove(binaryPath);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_test_manifest_roundtrips_through_binary_and_runtime_loader);
    return UNITY_END();
}
