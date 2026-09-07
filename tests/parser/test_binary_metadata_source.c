#include "unity.h"
#include "runtime_support.h"
#include "path_support.h"
#include "compiler/module_init_analysis.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_library/file.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#include <string.h>

static SZrState *g_state;
static SZrFunction *g_function;
static SZrIoSource *g_source;
static char g_binaryPath[ZR_TESTS_PATH_MAX];
static char g_intermediatePath[ZR_TESTS_PATH_MAX];

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_function = ZR_NULL;
    g_source = ZR_NULL;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("binary_metadata_source", "", "provider", ".zro",
            g_binaryPath, sizeof(g_binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("binary_metadata_source", "", "provider", ".zri",
            g_intermediatePath, sizeof(g_intermediatePath)));
    remove(g_binaryPath);
    remove(g_intermediatePath);
}

void tearDown(void) {
    if (g_source != ZR_NULL) {
        ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(g_state->global, g_source);
    }
    if (g_function != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_function);
    }
    ZrTests_Runtime_State_Destroy(g_state);
    remove(g_binaryPath);
    remove(g_intermediatePath);
}

static SZrFunction *compile_provider(const char *source) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "binary_metadata_provider.zr");
    return ZrParser_Source_Compile(g_state, source, strlen(source), name);
}

static void assert_binary_identity(const char *intermediateSource) {
    SZrBinaryWriterOptions options = {0};
    SZrLibrary_File_Reader *reader;
    SZrIo io;
    TZrBool loaded;
    const SZrIoFunction *actualFunction;
    const SZrIoFunctionTypedExportSymbol *actual;
    const SZrFunctionTypedExportSymbol *expected;

    g_function = compile_provider("pub var measure = fn(value: int = 7): int => value;\n");
    TEST_ASSERT_NOT_NULL(g_function);
    options.moduleName = "provider";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(g_state, g_function, g_binaryPath, &options));
    if (intermediateSource != ZR_NULL) {
        SZrFunction *intermediate = compile_provider(intermediateSource);
        TZrBool written;
        TEST_ASSERT_NOT_NULL(intermediate);
        written = ZrParser_Writer_WriteIntermediateFile(g_state, intermediate, g_intermediatePath);
        ZrCore_Function_Free(g_state, intermediate);
        TEST_ASSERT_TRUE(written);
    }

    reader = ZrLibrary_File_OpenRead(g_state->global, g_binaryPath, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(reader);
    ZrCore_Io_Init(g_state, &io, ZrLibrary_File_SourceReadImplementation,
            ZrLibrary_File_SourceCloseImplementation, reader);
    io.isBinary = ZR_TRUE;
    loaded = ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(g_state, &io, &g_source);
    io.close(g_state, io.customData);
    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_NOT_NULL(g_source);
    TEST_ASSERT_EQUAL_UINT(1, g_source->modulesLength);
    actualFunction = g_source->modules[0].entryFunction;
    TEST_ASSERT_NOT_NULL(actualFunction);
    TEST_ASSERT_EQUAL_UINT(1, actualFunction->typedExportedSymbolsLength);
    actual = &actualFunction->typedExportedSymbols[0];
    expected = &g_function->typedExportedSymbols[0];
    TEST_ASSERT_TRUE(ZrCore_String_Equal(expected->name, actual->name));
    TEST_ASSERT_NOT_EQUAL(0, expected->metadataToken);
    TEST_ASSERT_NOT_EQUAL(0, expected->signatureToken);
    TEST_ASSERT_NOT_EQUAL(0, expected->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(expected->metadataToken, actual->metadataToken);
    TEST_ASSERT_EQUAL_UINT32(expected->signatureToken, actual->signatureToken);
    TEST_ASSERT_EQUAL_UINT64(expected->signatureHash, actual->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(expected->signatureBlobOffset, actual->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(expected->signatureBlobLength, actual->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32(expected->lineInSourceStart, actual->lineInSourceStart);
    TEST_ASSERT_EQUAL_UINT32(expected->columnInSourceStart, actual->columnInSourceStart);
    TEST_ASSERT_EQUAL_UINT32(expected->lineInSourceEnd, actual->lineInSourceEnd);
    TEST_ASSERT_EQUAL_UINT32(expected->columnInSourceEnd, actual->columnInSourceEnd);
    TEST_ASSERT_EQUAL_UINT32(expected->callableChildIndex, actual->callableChildIndex);
    TEST_ASSERT_EQUAL_UINT64(g_function->moduleSignatureHash, actualFunction->moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT(g_function->metadataTokenRecordLength, actualFunction->metadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT(g_function->signatureBlobHeapLength, actualFunction->signatureBlobHeapLength);
    TEST_ASSERT_GREATER_THAN(0, actualFunction->closuresLength);
    TEST_ASSERT_EQUAL_UINT(1, actualFunction->closures[0].subFunction->parameterMetadataLength);
    TEST_ASSERT_TRUE(actualFunction->closures[0].subFunction->parameterMetadata[0].hasDefaultValue);
}

static void test_binary_identity_without_intermediate(void) {
    assert_binary_identity(ZR_NULL);
}

static void test_binary_identity_with_current_intermediate(void) {
    assert_binary_identity("pub var measure = fn(value: int = 7): int => value;\n");
}

static void test_binary_identity_with_stale_intermediate(void) {
    assert_binary_identity("pub var staleExport = fn(): float => 3.5;\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_identity_without_intermediate);
    RUN_TEST(test_binary_identity_with_current_intermediate);
    RUN_TEST(test_binary_identity_with_stale_intermediate);
    return UNITY_END();
}
